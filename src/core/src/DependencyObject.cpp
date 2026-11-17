// S036 + S037 (corrected by S811) — DependencyObject out-of-line definitions.
// S036 (dep:S035): implements the S035 apply pipeline verbatim in Set():
//   coerce -> validate(on the coerced value) -> fire changed cb ONLY on a
//   real change -> store.
// S037 (dep:S036_5, corrected by S811): adds wrong-thread access THROWING — a
//   Set()/Get() from a NON-affine thread THROWS WrongThreadException (mirror
//   WPF DependencyObject.VerifyAccess, DO.cs:161/412/...). S811 RETRACTS S037's
//   original write-QUEUING (queued-not-applied + ProcessPendingChanges drain +
//   SetResult::QUEUED) and S041's fabricated change-notification pipeline
//   (per-object subscriber registry, ProcessNotificationQueue, descendant-push
//   Inherits postbox, ChangeQueue/ChangeProcessor/ChangeRecorder). There is NO
//   per-object multi-subscriber list in WPF (DO.cs:1704/1732/2039).
//
// S811 introduces the expression-as-value write-back seam (WPF Expression,
// DO.cs:705-708): a bound target DP holds one live expression; a Set() on the
// affinity thread is routed through `expr->SetValue(...)` for the two-way
// write-back BEFORE the plain store.
//
// S142 (dep:S141): the pimpl is COLLAPSED. The per-object store + all inline
// state live directly in the object's allocation as ONE block-resident unit
// (pimpl removed, not preserved; ADR-003 revised + ADR-019). The store is
// inline-started (PD slots_[kInline]) and heap-grown on demand (append-only
// overflow once count_ exceeds kInline) — no per-object heap hop, no ~100%-
// of-access `impl_->` trampoline. Single PD type (no common/root). Behavior is
// unchanged (the whole S030-S141 suite must stay green).

#include "vaml/core/DependencyObject.h"

#include "ObjectPool.h"                  // S145: the thread-local object block pool

#include "vaml/core/DependencyProperty.h" // global descriptor for S039 inheritance
#include "vaml/core/DependencyPropertyExpression.h" // S811: the expression-as-value interface
#include "vaml/core/Intern.h"            // S140: affinity-thread interned name keys (string_view)

#include <algorithm>
#include <cstdlib>
#include <mutex>
#include <new>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace vaml {

// S810_1 helper: render a refused Value to a readable one-line diagnostic for
// InvalidPropertyValueException::value(). Renders the primitive payloads that
// the validate funcs actually inspect; anything else falls back to the tag name
// (never a crash, never a half-formed string).
namespace {
std::string render_value_for_diagnostic(const Value& v)
{
    switch (v.type())
    {
        case ValueType::Bool:    return v.get<bool>() ? "true" : "false";
        case ValueType::Int32:   return std::to_string(v.get<std::int32_t>());
        case ValueType::Int64:   return std::to_string(v.get<std::int64_t>());
        case ValueType::Float:   { std::ostringstream os; os << v.get<float>(); return os.str(); }
        case ValueType::Double:  { std::ostringstream os; os << v.get<double>(); return os.str(); }
        case ValueType::String:  return "\"" + v.get<std::string>() + "\"";
        default:                 return "<" + std::to_string(static_cast<int>(v.type())) + ">";
    }
}
} // namespace

// ---------------------------------------------------------------------------
// S145 — class-scoped allocation: the object-pool funnel.
//
// Every DependencyObject / derived element is doled from (and recycled back to)
// the thread-local ObjectPool. A size that fits a slot carries the object + its
// inline S142 store with no per-object heap hop; an oversized request falls back
// to the process heap and operator delete forwards it back there via the pool's
// contains() check. Never delete during the thread's life (opt-in
// object_pool_master_destroy_all). ADR-018: zero locks — thread-local, lockless
// by construction.
// ---------------------------------------------------------------------------
VAML_CORE_EXPORT
void* DependencyObject::operator new(std::size_t size)
{
    return vaml::detail::object_pool_allocate(size);
}

VAML_CORE_EXPORT
void DependencyObject::operator delete(void* p) noexcept
{
    vaml::detail::object_pool_deallocate(p);
}

VAML_CORE_EXPORT
void DependencyObject::operator delete(void* p, std::size_t) noexcept
{
    vaml::detail::object_pool_deallocate(p);
}

// -- S142 Store: block-resident, inline-started, heap-grown on demand. --------
// The PD array is the single block-resident unit; only the overflow array is a
// separate heap allocation, and only once count_ exceeds kInline (append-only,
// never shrunk). Lookup is a linear scan (inlined in the header) by interned
// name — cache-friendly, deterministic (insertion) order.
//
// OWNERSHIP (S142 single PD): each live PD owns at most a `owned`
// PropertyMetadata* (object-scoped) and a `stableKey` PropertyKey* (stable heap
// key node); `borrowed` is a NON-OWNING pointer to a process-lifetime global. So
// destruction / overwrite must release exactly the two OWNED heap nodes, never
// the borrowed pointer. PDs are trivially copyable, so array growth/relocation
// copies raw pointers (ownership transfers only via the destructor/release).

void DependencyObject::Store::release_slot(PD& pd) noexcept
{
    delete pd.owned;
    pd.owned = nullptr;
    delete pd.stableKey;
    pd.stableKey = nullptr;
}

DependencyObject::Store::~Store()
{
    // Release every live PD's OWNED heap nodes, then the overflow POD array.
    for (std::size_t i = 0; i < count_; ++i)
    {
        PD& pd = (i < kInline) ? slots_[i] : overflow_[i - kInline];
        release_slot(pd);
    }
    std::free(overflow_);
    overflow_ = nullptr;
}

DependencyObject::Store::Store(Store&& other) noexcept
    : count_(other.count_),
      overflow_(other.overflow_),
      overflowCap_(other.overflowCap_)
{
    // Transfer the inline slots by copy (PD is trivially copyable: the pointers
    // to the OWNED heap nodes + the borrowed pointer + the interned view + the
    // Value move cleanly). Only the INLINE portion (min(count_,kInline)) lives
    // in other.slots_; the overflow entries already come along via the moved
    // overflow_ pointer — copying past kInline would over-read the inline array.
    // Ownership of the owned nodes transfers to THIS store; the source is
    // cleared so it releases nothing on destruction.
    const std::size_t inlineN = (count_ < kInline) ? count_ : kInline;
    for (std::size_t i = 0; i < inlineN; ++i)
    {
        slots_[i] = other.slots_[i];
        other.slots_[i] = PD{};
    }
    other.count_ = 0;
    other.overflow_ = nullptr;
    other.overflowCap_ = 0;
}

DependencyObject::Store& DependencyObject::Store::operator=(Store&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }
    // Release THIS store's existing OWNED nodes (destination is being replaced),
    // including any inline slots it already held, then the overflow array.
    for (std::size_t i = 0; i < count_; ++i)
    {
        PD& pd = (i < kInline) ? slots_[i] : overflow_[i - kInline];
        release_slot(pd);
    }
    std::free(overflow_);

    count_ = other.count_;
    overflow_ = other.overflow_;
    overflowCap_ = other.overflowCap_;
    const std::size_t inlineN = (count_ < kInline) ? count_ : kInline;
    for (std::size_t i = 0; i < inlineN; ++i)
    {
        slots_[i] = other.slots_[i];
        other.slots_[i] = PD{};
    }
    other.count_ = 0;
    other.overflow_ = nullptr;
    other.overflowCap_ = 0;
    return *this;
}

DependencyObject::PD* DependencyObject::Store::emplace(PD pd)
{
    if (count_ < kInline)
    {
        PD& slot = slots_[count_];
        slot = std::move(pd);
        ++count_;
        return &slot;
    }

    // Overflow path: grow the append-only array (never shrink). PDs are
    // trivially movable, so we reallocate + copy the existing overflow, then
    // append the new entry.
    //
    // BUGFIX (S500; pre-existing S142 defect surfaced by the DataTemplate
    // S500 clone crossing the kInline=8 threshold): `fresh` is allocated with
    // std::malloc, so its PD slots are RAW, UNCONSTRUCTED memory. Assigning
    // into them (fresh[i] = overflow_[i], and slot = std::move(pd)) invokes
    // PD::operator= -> Value::operator= on an UNINITIALIZED destination Value —
    // which, for a String, reads the garbage type_/length and calls
    // ~basic_string on a corrupt length field (the observed
    // `free(): invalid pointer` on "prop_6" in the full clang suite).
    // The fix: PLACEMENT-NEW construct each destination PD (and its Value) in
    // the raw buffer BEFORE assignment, so the move/copy-assignment targets a
    // fully-initialized object that releases nothing.
    const std::size_t newCap = (overflowCap_ == 0) ? 8 : overflowCap_ * 2;
    PD* fresh = static_cast<PD*>(std::malloc(newCap * sizeof(PD)));
    for (std::size_t i = 0; i < count_ - kInline; ++i)
    {
        // Construct the destination PD (raw buffer) in place, THEN copy-assign
        // the source into it (Value::operator= now has a valid, empty target).
        new (fresh + i) PD(overflow_[i]);
    }
    std::free(overflow_);
    overflow_ = fresh;
    overflowCap_ = newCap;
    // Construct the new appended slot (raw buffer), then move-assign the new
    // entry into the now-valid object.
    new (overflow_ + (count_ - kInline)) PD();
    PD& slot = overflow_[count_ - kInline];
    slot = std::move(pd);
    ++count_;
    return &slot;
}

// S037: a wrong-thread write from a non-affine thread — the OWNED property name
// (survives the registering PropertyKey) + the already COERCED+VALUE-validated
// value. Coerce/validate run at queue time; the drain replays accepted values.

VAML_CORE_EXPORT
DependencyObject::DependencyObject() = default;

VAML_CORE_EXPORT
DependencyObject::~DependencyObject()
{
    // S245: fire the destruction-lifecycle notification FIRST — before any
    // member teardown disposes the store/subs — so an observer (e.g. a binding
    // expression's "detach on element destroy" wire) can still read the
    // object's own value store during the notification. Copy-before-invoke
    // (a handler that (un)subscribes mid-fire is safe). The handler vector is
    // intentionally copied OUT of the moving-to-destruction object; the members
    // are torn down after.
    fire_destroyed();
    // Members (store_, destroyed_, expressions_, ...) are disposed here by the
    // implicit member destructors. The expressions_ map holds NON-OWNING
    // pointers to live expressions (the BindingExpression owns itself; it
    // detaches via add_destroyed/Detach) — no deletion here.
}

// S142 + S811 move: the pimpl is collapsed and the fabricated notification
// surface (subs_, inheritance_scope_, queue_) is removed by S811, so the custom
// move moves only the real members (store, affinity, parent, destroyed list,
// and the S811 expression store). The moved-from object is left in a
// valid-but-unspecified state (empty store, no parent, empty expressions),
// matching the pre-S142 moved-from semantics.
VAML_CORE_EXPORT
DependencyObject::DependencyObject(DependencyObject&& other) noexcept
    : store_(std::move(other.store_)),
      affinity_(other.affinity_),
      parent_(other.parent_),
      destroyed_(std::move(other.destroyed_)),
      next_destroyed_token_(other.next_destroyed_token_),
      expressions_(std::move(other.expressions_))
{
    other.parent_ = nullptr;
    other.destroyed_.clear();
    other.next_destroyed_token_ = 1;
    other.expressions_.clear();
}

VAML_CORE_EXPORT
DependencyObject& DependencyObject::operator=(DependencyObject&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }
    store_ = std::move(other.store_);
    affinity_ = other.affinity_;
    parent_ = other.parent_;
    destroyed_ = std::move(other.destroyed_);
    next_destroyed_token_ = other.next_destroyed_token_;
    expressions_ = std::move(other.expressions_);
    other.parent_ = nullptr;
    other.destroyed_.clear();
    other.next_destroyed_token_ = 1;
    other.expressions_.clear();
    return *this;
}

VAML_CORE_EXPORT
const PropertyKey& DependencyObject::register_property(PropertyKey key, PropertyMetadata metadata)
{
    // Type coherence: never register a key/metadata pair that disagree on the
    // bound value type (NO-LAZY guard). This is the WPF
    // `DefaultValuePropertyTypeMismatch` analogue (DependencyProperty.cs:376-434) —
    // a key type != metadata bound type (== defaultValue().type()) refuses.
    if (key.type() != metadata.boundType())
    {
        throw ValueTypeMismatch("DependencyObject::register_property: key.type() does not match metadata's bound value type");
    }

    // The canonical interned name + owner (composite store key, S802) NEXT —
    // before the duplicate-pair check. The thread-local intern views are stable
    // for the life of this object's affinity thread (S140, ADR-018) — a safe
    // store key. The owner comes from the key's OWNER-TYPE identity (the
    // declaring type), exactly WPF's (name, ownerType) pair.
    const std::string_view name  = vaml::intern(key.name());
    const std::string_view owner = vaml::intern(key.ownerTypeId());

    // S802: duplicate-detection is by the composite PAIR. A same-name/
    // different-owner key is a DISTINCT property (coexistence) and registers a
    // new entry; only a same-pair re-register throws (never overwrite).
    if (store_.find(name, owner) != nullptr)
    {
        throw PropertyAlreadyRegistered(
            "DependencyObject::register_property: a property with this (name, owner) pair is already registered");
    }

    // S810_2 — REGISTRATION-TIME DEFAULT VALIDATION (the WPF
    // `ValidateDefaultValueCommon` / `DefaultValueInvalid` analogue). The
    // metadata `ValidateFunc` is run against the DEFAULT here, AFTER the
    // duplicate-pair check above (WPF `RegisterCommon` throws on an existing
    // name/owner before validating defaults) and BEFORE the owned-metadata/
    // stableKey heap nodes + `store_.emplace(...)` — so a false result throws
    // ATOMICALLY (no entry installed, nothing allocated, no partial state).
    // Mirrors WPF throwing ArgumentException(DefaultValueInvalid) from
    // ValidateDefaultValueCommon (DependencyProperty.cs:376-434) when
    // validateValueCallback(defaultValue) returns false. Reuses the S810_1
    // InvalidPropertyValueException (std::invalid_argument -> WPF
    // ArgumentException lineage), naming the property in canonical
    // `owner.name` form + the offending default.
    if (!metadata.validate(metadata.defaultValue()))
    {
        throw InvalidPropertyValueException(
            "DependencyObject::register_property: default value for property " +
                std::string(key.ownerTypeId()) + "." + std::string(key.name()) +
                " failed its ValidateFunc at registration (DefaultValueInvalid)",
            std::string(key.ownerTypeId()) + "." + std::string(key.name()),
            render_value_for_diagnostic(metadata.defaultValue()));
    }

    // The OWNED metadata bundle lives in its own heap node (move-only,
    // never copied — S043), addressed by pointer so store growth/relocation
    // never invalidates it.
    PropertyMetadata* owned = new PropertyMetadata(std::move(metadata));

    // The stable KEY node holds the owned PropertyKey copy. register_property
    // returns `const PropertyKey&` valid for the object's lifetime; keeping the
    // key in a SEPARATE stable heap node (not a relocating PD slot) guarantees
    // the returned reference survives store overflow-growth (S142 §3).
    PropertyKey* stableKey = new PropertyKey(std::move(key));

    PD* entry = store_.emplace(PD{
        name,
        owner,
        Value::Null(),
        owned,
        nullptr, // borrowed
        stableKey,
    });

    return *entry->stableKey;
}

VAML_CORE_EXPORT
Value DependencyObject::Get(PropertyId id) const
{
    return GetEffectiveValue(id);
}

VAML_CORE_EXPORT
bool DependencyObject::has_local_value(PropertyId id) const noexcept
{
    // S802/S803: composite (name, owner) resolution — the id always carries its
    // owner (WPF: no name-only DP lookup); a same-name/different-owner pair is
    // a distinct property. resolve_entry is strict-pair.
    const PD* entry = resolve_entry(id);
    if (entry == nullptr)
    {
        return false; // absent / no local override for an unattached read
    }
    // A local override is stored as a non-Null local in the object's OWN store.
    // (An inherited value never lands here — it resolves through the parent
    // chain, not the object's store; a Null-valued local is genuinely a local,
    // but DependencyObject stores absent == Null, so a present non-Null entry
    // flags a real authored/Set local override.)
    return !entry->local.isNull();
}

VAML_CORE_EXPORT
Value DependencyObject::GetEffectiveValue(PropertyId id) const
{
    // S811: a non-affine Get THROWS (WPF DependencyObject.GetValue calls
    // this.VerifyAccess(), DO.cs:161). There is no cross-thread read queue; a
    // wrong-thread read is refused with a clear error (VERIFY the affinity
    // marker against the current thread before touching the store).
    if (affinity_ != std::this_thread::get_id())
    {
        throw WrongThreadException("DependencyObject::GetEffectiveValue: cannot read from a thread other than the object's affinity thread");
    }

    // S802/S803: the per-object store resolves on the composite (name, owner)
    // pair — the id always carries its owner (WPF: no name-only DP lookup).
    const PD* entry = resolve_entry(id);
    const bool inStore = (entry != nullptr);

    // The effective owner used for the ancestor walk + subscriber keying: the
    // store entry's canonical owner when present.
    const std::string_view effectiveOwner =
        (entry != nullptr) ? entry->owner : id.owner();

    // The property's metadata provider + this object's local override.
    //
    // Three cases (S043 — attached-aware global resolution):
    //   1. in the local store: use the stored entry's metadata (owned ODR or
    //      a previously materialized borrowed attached entry) + its local.
    //   2. NOT in the store but globally registered: an ATTACHED property the
    //      object has not locally registered OR a purely-global descriptor.
    //      Resolve with the GLOBAL metadata + an empty local (local NULL) —
    //      so the value is `inherited ?? global-default`. The object needs NO
    //      local store entry to read an attached property.
    //   3. in NEITHER (no local entry, no global descriptor): PropertyNotFound
    //      (unchanged — an unregistered/unresolvable name still throws).
    //
    // S801/S802: the global metadata read resolves by the composite (name,
    // owner) PAIR (the id always carries its owner — WPF-faithful). The global
    // descriptor supplies the canonical flags + default for an
    // attached/purely-global property.
    const DependencyProperty* globalDp = resolve_global(id);

    const PropertyMetadata* mdPtr = nullptr;
    Value local = Value::Null();
    if (inStore)
    {
        mdPtr = &entry->md();
        local = entry->local.isNull() ? Value::Null() : entry->local;
    }
    else if (globalDp != nullptr)
    {
        mdPtr = &globalDp->metadata();
        local = Value::Null(); // no local override for an unattached read
    }
    else
    {
        throw PropertyNotFound("DependencyObject::GetEffectiveValue: no property registered with this id");
    }
    const PropertyMetadata& md = *mdPtr;

    // The inheritance descriptor (flags + default): the GLOBAL DependencyProperty's
    // when the property is globally registered (S039's real consumer); falling
    // back to the provider metadata keeps object-scoped S036 registration working
    // identically (no global entry -> object metadata governs).
    const PropertyFlags localFlags = md.flags();
    const bool inherits =
        has_flag(localFlags, PropertyFlags::Inherits)
        || (globalDp != nullptr && has_flag(globalDp->flags(), PropertyFlags::Inherits));

    const Value defaultValue = md.defaultValue();

    // NOT an inheriting property: local ?? default (unchanged S036 semantics).
    if (!inherits)
    {
        return local.isNull() ? defaultValue : local;
    }

    // Inheriting property: resolve the inherited value from the ancestor chain
    // (tolerant — never throws for an ancestor lacking the property) keyed on
    // the composite pair (S802).
    const Value inherited = nearest_ancestor_local(*this, id.name(), effectiveOwner);

    // S814: InheritOnly is retired. Every inheriting property resolves
    // local ?? inherited ?? default — a local override on the object WINS,
    // exactly as EvaluateEffectiveValue / GetValueEntry do in WPF (a child's
    // authored/Set local overrides the inherited ancestor value).
    return local.isNull() ? (inherited.isNull() ? defaultValue : inherited) : local;
}

VAML_CORE_EXPORT
void DependencyObject::set_parent(DependencyObject* parent) noexcept
{
    // S811: the S041 inverse-edge (registering `this` into the parent's
    // inheritance-scope for a descendant-push postbox) is RETRACTED. Only the
    // single non-owning parent link is installed. The INHERITANCE VALUE still
    // propagates through the S039 ancestor walk in Get()/GetEffectiveValue()
    // (pull model) — there is no per-object notification queue in WPF
    // (DO.cs:1704/1732/2039).
    parent_ = parent;
}

VAML_CORE_EXPORT
DependencyObject* DependencyObject::parent() const noexcept
{
    return parent_;
}

// S802 resolution helper: the per-object store entry for `id` on the composite
// (name, owner) pair. S803: owner-less ids are GONE (total removal) — the owner
// is always carried, so the per-object store is reached strictly by the pair.
const DependencyObject::PD* DependencyObject::resolve_entry(PropertyId id) const noexcept
{
    return store_.find(vaml::intern(id.name()), vaml::intern(id.owner()));
}

DependencyObject::PD* DependencyObject::resolve_entry(PropertyId id) noexcept
{
    return store_.find(vaml::intern(id.name()), vaml::intern(id.owner()));
}

const DependencyProperty* DependencyObject::resolve_global(PropertyId id) const noexcept
{
    return DependencyProperty::TryGet(id.name(), id.owner());
}

VAML_CORE_EXPORT
Value DependencyObject::nearest_ancestor_local(
    const DependencyObject& self, std::string_view name, std::string_view owner)
{
    // Straight upward scan for the nearest ancestor with a LOCAL override for
    // the (name, owner) PAIR (S802 composite — a same-name/different-owner
    // ancestor local does not leak in). A plain-Inherits property's net value
    // is "nearest ancestor with a local override else default", because an
    // intermediate ancestor with no local defers to ITS own ancestor anyway —
    // so this is exactly equivalent to recursing into each ancestor's effective
    // value, without the throw hazard and re-deriving a single descriptor
    // (flags/default come from the caller).
    //
    // The INTERNED canonical view is used for lookup: ancestors are affinity-
    // thread objects, so their stores hold THIS thread's interned views
    // (content-equal; string_view equality is by content — S140).
    const DependencyObject* cur = self.parent();
    while (cur != nullptr)
    {
        const PD* entry = cur->store_.find(intern(name), intern(owner));
        if (entry != nullptr && !entry->local.isNull())
        {
            return entry->local;
        }
        cur = cur->parent();
    }
    return Value::Null();
}

VAML_CORE_EXPORT
SetResult DependencyObject::Set(PropertyId id, const Value& input)
{
    // S811: a NON-affine Set THROWS WrongThreadException (WPF
    // DependencyObject.SetValue calls this.VerifyAccess(), DO.cs:412) BEFORE
    // resolving the property / running any pipeline — there is NO cross-thread
    // value queue, no deferral, no queued-not-applied. A wrong-thread writer is
    // refused with a clear error (the exact S1724 "blocked with clear error"
    // instinct, WPF-confirmed).
    if (affinity_ != std::this_thread::get_id())
    {
        throw WrongThreadException("DependencyObject::Set: cannot Set from a thread other than the object's affinity thread");
    }

    // The canonical interned name + owner views (thread-local, content-equal —
    // S140). Used against this object's affinity-thread store and the interned-
    // global DependencyProperty read path. NO transient std::string is
    // materialized. S802: the pair is the per-object store identity.
    const std::string_view name  = id.name();
    const std::string_view owner = id.owner();

    // Resolve the metadata PROVIDER (non-owning) WITHOUT mutating the store
    // (S043 attached-aware global resolution):
    //   - in the local store: the stored entry's metadata (owned ODR or an
    //     already-materialized borrowed attached entry);
    //   - NOT in the store but globally registered: the GLOBAL metadata — an
    //     ATTACHED property key owned by A, settable on this object B. The
    //     move-only PropertyMetadata is NEVER copied; the global descriptor is
    //     the single owner;
    //   - NEITHER: PropertyNotFound (unchanged — an unregistered id throws).
    //
    // S802/S803: a composite (name, owner) id resolves by its pair (the id
    // always carries its owner — WPF has no name-only lookup).
    const PropertyMetadata* mdPtr = nullptr;
    PD* entry = resolve_entry(id);
    if (entry != nullptr)
    {
        mdPtr = &entry->md();
    }
    else
    {
        const DependencyProperty* globalDp = resolve_global(id);
        if (globalDp == nullptr)
        {
            throw PropertyNotFound("DependencyObject::Set: no property registered with this id");
        }
        mdPtr = &globalDp->metadata();
    }
    const PropertyMetadata& md = *mdPtr;

    // S811 expression-as-value interception (WPF SetValueCommon, DO.cs:705-708):
    // if the target DP currently holds a live expression (a bound TwoWay
    // BindingExpression is "current value = expression"), a Set() arriving here
    // is first offered to the expression via `SetValue`. If the expression
    // HANDLES the value (a reflective binding stores + writes it back to the
    // source), the plain local-store write is skipped — the expression owns the
    // target value. If it declines (non-reflective), we fall through to the
    // plain store and DETACH the expression (WPF's `return false` + OnDetach,
    // BindingExpressionBase.cs:412, DO.cs:753-761).
    // S811 expression-as-value interception (WPF SetValueCommon, DO.cs:705-708):
    // if the target DP currently holds a live expression (a TwoWay
    // BindingExpression is "current value = expression"), a Set() arriving here
    // is first offered to the expression via `SetValue`. If the expression
    // HANDLES the value (a reflective TwoWay binding stores it + writes it back
    // to the source), the expression stays attached; either way we FALL THROUGH
    // to the plain store so the target's local value reflects the incoming edit
    // (WPF: the target's effective value holds what was set). If the expression
    // DECLINES (a non-reflective OneWay/OneTime binding), it is detached (WPF's
    // `return false` + OnDetach, BindingExpressionBase.cs:412, DO.cs:753-761).
    DependencyPropertyExpression* expr = current_expression(id);
    if (expr != nullptr && !expr->SetValue(input))
    {
        // Non-reflective expression declined: detach it.
        set_current_expression(id, nullptr);
    }

    // S045 read-only guard: structural + value-independent. Placed BEFORE
    // coerce/validate (the pipeline never runs). Throws even when
    // `input` equals the current/default value, and never materializes a
    // borrowed store entry for a refused read-only write.
    if (has_flag(md.flags(), PropertyFlags::ReadOnly))
    {
        throw PropertyReadOnlyException(
            "DependencyObject::Set: cannot Set a read-only property");
    }

    // D1 (S810) pipeline step 1: validate ON THE RAW input - WPF
    // DependencyObject.cs:653 validates the incoming value BEFORE coercion.
    // S810_1: a ValidateFunc refusal THROWS InvalidPropertyValueException (WPF
    // ArgumentException(InvalidPropertyValue)) — SetResult::REJECTED is retired
    // as the out-of-range disposition. Store unchanged, no cb.
    if (!md.validate(input))
    {
        throw InvalidPropertyValueException(
            "DependencyObject::Set: invalid value for property " +
                std::string(id.owner()) + "." + std::string(id.name()),
            std::string(id.owner()) + "." + std::string(id.name()),
            render_value_for_diagnostic(input));
    }

    // D1 (S810) pipeline step 2: coerce (WPF DO.cs:1692 coerces AFTER the raw validation).
    const Value coerced = md.coerce(input);

    // D1 (S810) pipeline step 3: re-validate ON THE COERCED value (WPF DO.cs:1692).
    // S810_1: same throwing disposition as the raw gate (WPF DO.cs:1692's
    // `if (!dp.IsValidValue(coercedValue)) throw new ArgumentException(...)`).
    if (!md.validate(coerced))
    {
        throw InvalidPropertyValueException(
            "DependencyObject::Set: coerced value for property " +
                std::string(id.owner()) + "." + std::string(id.name()) +
                " failed validation",
            std::string(id.owner()) + "." + std::string(id.name()),
            render_value_for_diagnostic(coerced));
    }

    // On the AFFINE thread, lazily materialize a BORROWED store entry for an
    // attached property so the local override has a home (S043). The global
    // descriptor is guaranteed present (we resolved `md` from it above).
    entry = resolve_entry(id);
    if (entry == nullptr)
    {
        // Resolve the global descriptor by the composite pair (owner always
        // carried — S803).
        const DependencyProperty& globalDp = DependencyProperty::Get(name, owner);
        const std::string_view canonicalOwner = vaml::intern(globalDp.owner_type_id());
        // PropertyKey owns its name (std::string); the store KEY is the
        // INTERNED string_view (canonical, stable for the affinity thread —
        // S140). The caller's id.name() may point into transient parser
        // storage, so we MUST intern before using it as a store key.
        PropertyKey* attachedKey = new PropertyKey(
            std::string(name), globalDp.metadata().boundType(),
            globalDp.owner_type_id(), globalDp.flags());
        entry = store_.emplace(PD{
            vaml::intern(name),
            canonicalOwner,
            Value::Null(),
            nullptr, // owned
            &globalDp.metadata(), // borrowed
            attachedKey,
        });
    }

    // Compute the current EFFECTIVE value (local override else default) for
    // both the change comparison and the changed cb's `old`.
    const Value currentEffective = entry->local.isNull() ? md.defaultValue() : entry->local;

    // D1 (S810) pipeline step 4: fire changed cb ONLY on a real change.
    if (coerced == currentEffective)
    {
        return SetResult::NO_CHANGE;
    }

    // S811: the changed notification is the SINGLE synchronous metadata
    // PropertyChangedCallback (WPF DO.cs:2037) — `md.notifyChanged(old, new)`.
    // There is NO subscriber registry, NO descendant-push postbox, NO deferred
    // notification.
    md.notifyChanged(currentEffective, coerced);

    // S818: the WPF `DependencyObject.OnPropertyChanged` change call-out
    // (DO.cs:2039) — fired ONLY on this real change, synchronous, on the
    // affinity thread, AFTER the changed-cb (matching WPF's PostProcessGetValue
    // ordering). The default base no-op does nothing; UIElement's override
    // consumes the Affects* arms (FE.cs:2198-2251). Fires with the flags of
    // the RESOLVED property (the carrier of the Affects* arms).
    OnPropertyChanged(id, currentEffective, coerced, md.flags());

    // D1 (S810) pipeline step 5: store.
    entry->local = coerced;

    return SetResult::APPLIED;
}

VAML_CORE_EXPORT
void DependencyObject::OnPropertyChanged(
    PropertyId /*id*/,
    const Value& /*oldValue*/,
    const Value& /*newValue*/,
    PropertyFlags /*flags*/)
{
    // Base no-op (WPF DependencyObject.OnPropertyChanged default). UIElement
    // overrides this to run the Affects* invalidation (FE.cs:2198-2251).
}

VAML_CORE_EXPORT
std::thread::id DependencyObject::thread_affinity_id() const noexcept
{
    return affinity_;
}

// S811 expression-as-value accessors. `expressions_` is a per-DP single-slot
// holder of the live expression that holds a bound target DP's value (WPF's
// "the expression IS the value", DO.cs:705-708). Lookup is on the composite
// PropertyId; a null Result means "not bound / plain local value".
VAML_CORE_EXPORT
DependencyPropertyExpression* DependencyObject::current_expression(PropertyId id) const noexcept
{
    const auto it = expressions_.find(id);
    return (it != expressions_.end()) ? it->second : nullptr;
}

VAML_CORE_EXPORT
void DependencyObject::set_current_expression(PropertyId id, DependencyPropertyExpression* expr) noexcept
{
    if (expr == nullptr)
    {
        expressions_.erase(id);
    }
    else
    {
        expressions_[id] = expr;
    }
}

VAML_CORE_EXPORT
DependencyObject::DestroyedToken DependencyObject::add_destroyed(DestroyedHandler handler)
{
    // Append in REGISTRATION ORDER; the token is a monotonically-increasing
    // counter never reused, so an expired token can never alias a live one
    // (mirrors add_property_changed/sub_ tokens). Single-threaded affinity,
    // no lock (ADR-018).
    const DestroyedToken token = next_destroyed_token_++;
    destroyed_.emplace_back(token, std::move(handler));
    return token;
}

VAML_CORE_EXPORT
void DependencyObject::remove_destroyed(DestroyedToken token)
{
    // Removal is O(n) but preserves the registration order of what remains.
    // No-op for an unknown/expired token (mirrors remove_property_changed).
    for (auto it = destroyed_.begin(); it != destroyed_.end(); ++it)
    {
        if (it->first == token)
        {
            destroyed_.erase(it);
            return;
        }
    }
}

void DependencyObject::fire_destroyed()
{
    // S245: fire the destruction handlers copy-before-invoke in registration
    // order. A handler that (un)subscribes mid-fire is safe (the snapshot is
    // already taken); an empty list is the no-cost default. Runs at the START
    // of the destructor, BEFORE member teardown.
    const std::vector<std::pair<DestroyedToken, DestroyedHandler>> snapshot = destroyed_;
    for (const auto& [token, handler] : snapshot)
    {
        (void)token;
        if (handler)
        {
            handler();
        }
    }
}

} // namespace vaml
