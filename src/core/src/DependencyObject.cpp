// S036 + S037 — DependencyObject out-of-line definitions.
// S036 (dep:S035): implements the S035 apply pipeline verbatim in Set():
//   coerce -> validate(on the coerced value) -> fire changed cb ONLY on a
//   real change -> store.
// S037 (dep:S036_5): adds wrong-thread write QUEUING — a Set() from a non-affine
//   thread runs coerce+validate at queue time but POSTS (queued-not-applied),
//   and ProcessPendingChanges() (affinity-thread-only) later drains FIFO through
//   the same store half.
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

#include "vaml/core/ChangeRecorder.h"    // S041: per-property coalescing at drain
#include "vaml/core/DependencyProperty.h" // global descriptor for S039 inheritance
#include "vaml/core/Intern.h"            // S140: affinity-thread interned name keys (string_view)

#include <algorithm>
#include <cstdlib>
#include <mutex>
#include <new>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace vaml {

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
    const std::size_t newCap = (overflowCap_ == 0) ? 8 : overflowCap_ * 2;
    PD* fresh = static_cast<PD*>(std::malloc(newCap * sizeof(PD)));
    for (std::size_t i = 0; i < count_ - kInline; ++i)
    {
        fresh[i] = overflow_[i];
    }
    std::free(overflow_);
    overflow_ = fresh;
    overflowCap_ = newCap;
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
DependencyObject::~DependencyObject() = default;

// S142 + S143 move: the pimpl collapsed so the S037 pending mutex was a DIRECT
// std::mutex member (non-movable) — S143 relocates that mutex INTO the ChangeQueue
// (the queue is move-only and movable), so the custom move simplifies to moving
// every member; the single ChangeQueue carries both families. The moved-from
// object is left in a valid-but-unspecified state (empty store, no parent, empty
// scopes/queues), matching the pre-S142 moved-from semantics.
VAML_CORE_EXPORT
DependencyObject::DependencyObject(DependencyObject&& other) noexcept
    : store_(std::move(other.store_)),
      affinity_(other.affinity_),
      parent_(other.parent_),
      subs_(std::move(other.subs_)),
      next_sub_token_(other.next_sub_token_),
      inheritance_scope_(std::move(other.inheritance_scope_)),
      queue_(std::move(other.queue_))
{
    other.parent_ = nullptr;
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
    subs_ = std::move(other.subs_);
    next_sub_token_ = other.next_sub_token_;
    inheritance_scope_ = std::move(other.inheritance_scope_);
    queue_ = std::move(other.queue_);
    other.parent_ = nullptr;
    return *this;
}

VAML_CORE_EXPORT
const PropertyKey& DependencyObject::register_property(PropertyKey key, PropertyMetadata metadata)
{
    // Type coherence: never register a key/metadata pair that disagree on the
    // bound value type (NO-LAZY guard).
    if (key.type() != metadata.boundType())
    {
        throw ValueTypeMismatch("DependencyObject::register_property: key.type() does not match metadata's bound value type");
    }

    // The canonical interned name (store key) BEFORE moving `key` into the
    // stable key node. The thread-local intern view is stable for the life of
    // this object's affinity thread (S140, ADR-018) — a safe store key.
    const std::string_view name = vaml::intern(key.name());

    if (store_.find(name) != nullptr)
    {
        // Name already registered -> throw; do NOT overwrite (store unchanged).
        throw PropertyAlreadyRegistered("DependencyObject::register_property: a property with this name is already registered");
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
Value DependencyObject::GetEffectiveValue(PropertyId id) const
{
    const PD* entry = store_.find(id.name());
    const bool inStore = (entry != nullptr);

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
    const DependencyProperty* globalDp = DependencyProperty::TryGet(id.name());

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
    const bool inheritOnly =
        has_flag(localFlags, PropertyFlags::InheritOnly)
        || (globalDp != nullptr && has_flag(globalDp->flags(), PropertyFlags::InheritOnly));

    const Value defaultValue = md.defaultValue();

    // NOT an inheriting property: local ?? default (unchanged S036 semantics).
    if (!inherits)
    {
        return local.isNull() ? defaultValue : local;
    }

    // Inheriting property: resolve the inherited value from the ancestor chain
    // (tolerant — never throws for an ancestor lacking the property).
    const Value inherited = nearest_ancestor_local(*this, id.name());

    if (inheritOnly)
    {
        // InheritOnly: the object's OWN local override does NOT contribute.
        // Value is the inherited value, else default.
        return inherited.isNull() ? defaultValue : inherited;
    }

    // Plain Inherits: local ?? inherited ?? default.
    return local.isNull() ? (inherited.isNull() ? defaultValue : inherited) : local;
}

VAML_CORE_EXPORT
void DependencyObject::set_parent(DependencyObject* parent) noexcept
{
    // S041: unwind the reverse edge from the CURRENT parent (remove this from
    // its inheritance-scope) before installing a new one.
    if (parent_ != nullptr)
    {
        auto& scope = parent_->inheritance_scope_;
        const auto it = std::find(scope.begin(), scope.end(), this);
        if (it != scope.end())
        {
            scope.erase(it);
        }
    }

    parent_ = parent;

    // S041 §2.4: register this object into the NEW parent's inheritance-scope
    // (the inverse edge, non-owning). A parent walks this scope to push
    // Inherits/InheritOnly changes to inheriting descendants.
    if (parent_ != nullptr)
    {
        auto& scope = parent_->inheritance_scope_;
        // Guard against a duplicate registration (defensive; a well-formed
        // reparent never double-adds, but set_parent may be called with the
        // same parent twice).
        if (std::find(scope.begin(), scope.end(), this) == scope.end())
        {
            scope.push_back(this);
        }
    }
}

VAML_CORE_EXPORT
DependencyObject* DependencyObject::parent() const noexcept
{
    return parent_;
}

VAML_CORE_EXPORT
Value DependencyObject::nearest_ancestor_local(const DependencyObject& self, std::string_view name)
{
    // Straight upward scan for the nearest ancestor with a LOCAL override. A
    // plain-Inherits property's net value is "nearest ancestor with a local
    // override else default", because an intermediate ancestor with no local
    // defers to ITS own ancestor anyway — so this is exactly equivalent to
    // recursing into each ancestor's effective value, without the throw hazard
    // and re-deriving a single descriptor (flags/default come from the caller).
    //
    // The INTERNED canonical view is used for lookup: ancestors are affinity-
    // thread objects, so their stores hold THIS thread's interned views
    // (content-equal; string_view equality is by content — S140).
    const DependencyObject* cur = self.parent();
    while (cur != nullptr)
    {
        const PD* entry = cur->store_.find(intern(name));
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
    // The canonical interned name view (thread-local, content-equal — S140).
    // Used against this object's affinity-thread store and the interned-global
    // DependencyProperty read path. NO transient std::string is materialized.
    const std::string_view name = id.name();

    // Resolve the metadata PROVIDER (non-owning) WITHOUT mutating the store
    // (S043 attached-aware global resolution + S037 cross-thread contract):
    //   - in the local store: the stored entry's metadata (owned ODR or an
    //     already-materialized borrowed attached entry);
    //   - NOT in the store but globally registered: the GLOBAL metadata — an
    //     ATTACHED property key owned by A, settable on this object B. The
    //     move-only PropertyMetadata is NEVER copied; the global descriptor is
    //     the single owner;
    //   - NEITHER: PropertyNotFound (unchanged — an unregistered name throws).
    //
    // coerce + validate MUST run before any store touch (S037 instant writer
    // feedback), and a NON-affine Set must NOT mutate the store (the store is
    // affinity-thread-owned by construction) — so materialization is deferred
    // to the on-affine path / the drain below, never to a wrong-thread writer.
    const PropertyMetadata* mdPtr = nullptr;
    PD* entry = store_.find(name);
    if (entry != nullptr)
    {
        mdPtr = &entry->md();
    }
    else
    {
        const DependencyProperty* globalDp = DependencyProperty::TryGet(name);
        if (globalDp == nullptr)
        {
            throw PropertyNotFound("DependencyObject::Set: no property registered with this id");
        }
        mdPtr = &globalDp->metadata();
    }
    const PropertyMetadata& md = *mdPtr;

    // S045 read-only guard: structural + value-independent. Placed BEFORE
    // coerce/validate (the pipeline never runs) AND before the S037 affinity
    // branch (a wrong-thread read-only Set THROWS, it is never QUEUED; a
    // read-only rejection must not be deferrable to a drain). Throws even when
    // `input` equals the current/default value, and never materializes a
    // borrowed store entry for a refused read-only write.
    if (has_flag(md.flags(), PropertyFlags::ReadOnly))
    {
        throw PropertyReadOnlyException(
            "DependencyObject::Set: cannot Set a read-only property");
    }

    // S035 pipeline step 1: coerce.
    const Value coerced = md.coerce(input);

    // S035 pipeline step 2: validate ON THE COERCED value.
    if (!md.validate(coerced))
    {
        return SetResult::REJECTED; // store unchanged, no cb
    }

    // S037 affinity branch: if this thread is NOT the object's affinity thread,
    // the accepted (coerced+validated) value is POSTED, not applied. coerce and
    // validate ALREADY ran above, so the writer got instant feedback; the drain
    // (ProcessPendingChanges) later replays this value through the store half.
    // NOTE: no store materialization happens here — a wrong-thread writer never
    // mutates the affinity-owned store.
    if (affinity_ != std::this_thread::get_id())
    {
        // S143 relocation: post the S037 value-write into the object's SINGLE
        // ChangeQueue (queue_.enqueue_value keeps an OWNED std::string — the
        // queue traverses threads, so it cannot hold a thread-local intern view;
        // the affinity-thread drain re-interns it to a canonical view).
        queue_.enqueue_value(name, coerced);
        return SetResult::QUEUED;
    }

    // On the AFFINE thread, lazily materialize a BORROWED store entry for an
    // attached property so the local override has a home (S043). The global
    // descriptor is guaranteed present (we resolved `md` from it above).
    entry = store_.find(name);
    if (entry == nullptr)
    {
        const DependencyProperty& globalDp = DependencyProperty::Get(name);
        // PropertyKey owns its name (std::string); the store KEY is the
        // INTERNED string_view (canonical, stable for the affinity thread —
        // S140). The caller's id.name() may point into transient parser
        // storage, so we MUST intern before using it as a store key.
        PropertyKey* attachedKey = new PropertyKey(
            std::string(name), globalDp.metadata().boundType(),
            globalDp.owner_type_id(), globalDp.flags());
        entry = store_.emplace(PD{
            vaml::intern(name),
            Value::Null(),
            nullptr, // owned
            &globalDp.metadata(), // borrowed
            attachedKey,
        });
    }

    // Compute the current EFFECTIVE value (local override else default) for
    // both the change comparison and the changed cb's `old`.
    const Value currentEffective = entry->local.isNull() ? md.defaultValue() : entry->local;

    // S035 pipeline step 3: fire changed cb ONLY on a real change.
    if (coerced == currentEffective)
    {
        return SetResult::NO_CHANGE;
    }

    // S041 §2.4: if this is an Inherits/InheritOnly property, capture each
    // descendant's CURRENT effective value (BEFORE the store) so the push can
    // carry each descendant's own old->new delta after the store (S039 pull
    // model, S040 local-wins respected through an intermediate).
    const DependencyProperty* dp = DependencyProperty::TryGet(id.name());
    const bool inherits =
        has_flag(md.flags(), PropertyFlags::Inherits)
        || (dp != nullptr && has_flag(dp->flags(), PropertyFlags::Inherits));

    // S044: an ATTACHED Inherits/InheritOnly property must also capture purely
    // passive descendants (those with NO local store entry). The precise
    // "attached" signal is that the ORIGIN's own store entry is BORROWED
    // (entry->owned == nullptr): the origin hosts the value but does NOT own the
    // property (S043 borrowed metadata). An object-scoped Inherits property
    // (the origin owns its entry) keeps the S041 behavior — a non-registering
    // child cannot inherit it. Together with the global Inherits flag this
    // triggers the broadened capture so passive ATTACHED descendants are
    // notified on the parent's change (not just materialized ones).
    std::vector<std::pair<DependencyObject*, Value>> descendantBefore;
    if (inherits)
    {
        const bool captureAttachedInherits =
            dp != nullptr && has_flag(dp->flags(), PropertyFlags::Inherits)
            && entry->owned == nullptr;
        collect_descendant_effectives(*this, name, captureAttachedInherits, descendantBefore);
    }

    md.notifyChanged(currentEffective, coerced);

    // S035 pipeline step 4: store.
    entry->local = coerced;

    // S041 §2.4: propagate the real change to inheriting descendants (S040's
    // recorded obligation, delivered here, NOT silent).
    if (inherits)
    {
        notify_descendants(id.name(), descendantBefore);
    }
    return SetResult::APPLIED;
}

VAML_CORE_EXPORT
void DependencyObject::ProcessPendingChanges()
{
    // Drain is affinity-thread-only (S037 D3): a wrong-thread drain would
    // mutate the affinity-owned store from the wrong thread — throw rather than
    // silently no-op (NO-LAZY).
    if (affinity_ != std::this_thread::get_id())
    {
        throw WrongThreadException("DependencyObject::ProcessPendingChanges: must be called from the object's affinity thread");
    }

    // S143 relocation: drain the S037 value-write family from the object's
    // SINGLE ChangeQueue (guard moved with the queue). The queue swaps the deque
    // out under its mutex (snapshot-then-run) so wrong-thread writers hitting
    // during the drain land in a FRESH deque (their writes drain next call),
    // never into the one we consume — then the store half runs lock-free on
    // the affinity thread. drain_values also re-interns each name to the
    // canonical view (S143 B2).
    queue_.drain_values([this](std::string_view name, const Value& coerced) {
        PD* entry = store_.find(name);

        // S045 DEFENSIVE guard at apply time: resolve the metadata provider
        // BEFORE materializing anything, and skip a write to a property that is
        // NOW read-only. The Set() read-only guard already prevents a read-only
        // write from ever being QUEUED, so this only defends the narrow edge case
        // where a property's metadata became read-only between queue-time and the
        // drain (NO-LAZY: never apply a write to a read-only property, and never
        // materialize a borrowed entry for a refused read-only write).
        const PropertyMetadata* drainMd = nullptr;
        const DependencyProperty* globalDpDrain = nullptr;
        if (entry != nullptr)
        {
            drainMd = &entry->md();
        }
        else
        {
            globalDpDrain = DependencyProperty::TryGet(name);
            if (globalDpDrain == nullptr)
            {
                return; // unregistered after the write was queued; skip
            }
            drainMd = &globalDpDrain->metadata();
        }
        if (has_flag(drainMd->flags(), PropertyFlags::ReadOnly))
        {
            return; // property is read-only at apply time; skip the queued write
        }

        if (entry == nullptr)
        {
            // Not in the local store. If the property is globally registered, this
            // is an ATTACHED property a wrong-thread writer targeted before the
            // object had a store entry — materialize a BORROWED entry (affinity
            // thread, legal) and apply. If it is NOT globally registered, skip it
            // (property was never registered; nothing to do).
            PropertyKey* attachedKey = new PropertyKey(
                std::string(name), globalDpDrain->metadata().boundType(),
                globalDpDrain->owner_type_id(), globalDpDrain->flags());
            entry = store_.emplace(PD{
                name,
                Value::Null(),
                nullptr, // owned
                &globalDpDrain->metadata(), // borrowed
                attachedKey,
            });
        }
        const PropertyMetadata& md = entry->md();

        const Value currentEffective = entry->local.isNull() ? md.defaultValue() : entry->local;
        if (coerced == currentEffective)
        {
            return; // no real change at apply time -> no cb
        }

        // S041 §2.4: capture descendants' current effective values (BEFORE the
        // store) so the push carries each descendant's own old->new delta.
        // S044: for an ATTACHED Inherits/InheritOnly property (origin's entry is
        // BORROWED — owned == nullptr), also capture purely passive descendants
        // with no local store entry (broadened walk). An object-scoped Inherits
        // property keeps the S041 behavior.
        const DependencyProperty* dp = DependencyProperty::TryGet(name);
        const bool inherits =
            has_flag(md.flags(), PropertyFlags::Inherits)
            || (dp != nullptr && has_flag(dp->flags(), PropertyFlags::Inherits));
        std::vector<std::pair<DependencyObject*, Value>> descendantBefore;
        if (inherits)
        {
            const bool captureAttachedInherits =
                dp != nullptr && has_flag(dp->flags(), PropertyFlags::Inherits)
                && entry->owned == nullptr;
            collect_descendant_effectives(*this, name, captureAttachedInherits, descendantBefore);
        }

        md.notifyChanged(currentEffective, coerced);
        entry->local = coerced;

        // S041 §2.4: propagate the applied queued change to inheriting descendants
        // exactly like an on-affine one.
        if (inherits)
        {
            notify_descendants(name, descendantBefore);
        }
    });
}

VAML_CORE_EXPORT
std::size_t DependencyObject::pending_change_count() const noexcept
{
    // S143 relocation: the S037 value-write depth is now the queue's own
    // value-write family (drain reads the single mutex-protected deque).
    return queue_.value_pending_count();
}

VAML_CORE_EXPORT
std::thread::id DependencyObject::thread_affinity_id() const noexcept
{
    return affinity_;
}

VAML_CORE_EXPORT
DependencyObject::PropertyChangedToken DependencyObject::add_property_changed(
    PropertyId id, PropertyChangedHandler handler)
{
    // Use the canonical interned name view (thread-local, S140) and append in
    // REGISTRATION ORDER. The token is a monotonically-increasing counter that
    // is never reused, so an expired token can never alias a live one.
    const DependencyObject::PropertyChangedToken token = next_sub_token_++;
    subs_[id.name()].emplace_back(token, std::move(handler));
    return token;
}

VAML_CORE_EXPORT
void DependencyObject::remove_property_changed(PropertyChangedToken token)
{
    // Removal is O(n) per property, but preserves the registration order of
    // what remains. We iterate every subscribed property because the token is
    // global (not per-property); this is correct and simple, and the subscriber
    // list is expected to be small.
    for (auto& [name, handlers] : subs_)
    {
        (void)name;
        for (auto it = handlers.begin(); it != handlers.end(); ++it)
        {
            if (it->first == token)
            {
                handlers.erase(it);
                return;
            }
        }
    }
}

VAML_CORE_EXPORT
void DependencyObject::ProcessNotificationQueue()
{
    // Drain is affinity-thread-only (mirrors S037's ProcessPendingChanges):
    // a wrong-thread drain would fire subscribers on the wrong thread — throw
    // rather than silently no-op (NO-LAZY).
    if (affinity_ != std::this_thread::get_id())
    {
        throw WrongThreadException("DependencyObject::ProcessNotificationQueue: must be called from the object's affinity thread");
    }

    // Drain the object's own ChangeQueue (swap-out inside ChangeQueue::process)
    // and fire this object's registration-ordered subscribers for each pushed
    // change. Two pushed changes of the same property before this drain collapse
    // to one notification (S038 coalescing, consumed end-to-end — S042 outcome).
    vaml::ChangeRecorder recorder;
    const std::size_t drained = queue_.process(
        [&recorder](PropertyId id, const Value& o, const Value& n) {
            recorder.record(id, o, n);
        });
    if (drained == 0)
    {
        return;
    }
    recorder.flush([this](PropertyId id, const Value& o, const Value& n) {
        // Fire THIS object's registration-ordered subscribers for the pushed
        // property. The handle vector is copied before invoking so a handler
        // that (un)subscribes mid-fire does not invalidate iteration (registration
        // ORDER is preserved for the captured snapshot).
        const auto it = subs_.find(id.name());
        if (it == subs_.end())
        {
            return; // nobody subscribed to this property
        }
        const std::vector<std::pair<PropertyChangedToken, PropertyChangedHandler>> snapshot = it->second;
        for (const auto& [token, handler] : snapshot)
        {
            (void)token;
            if (handler)
            {
                handler(id, o, n);
            }
        }
    });
}

VAML_CORE_EXPORT
std::size_t DependencyObject::pending_notification_count() const noexcept
{
    return queue_.pending_count();
}

// S041 descendant push, walk 1 (BEFORE the origin stores its new value):
// capture every descendant in this object's inheritance-scope (recursively) that
// REGISTERS `name`, along with its CURRENT effective value. This snapshot is the
// "before" side of each descendant's own old->new delta. Descendants that do NOT
// register the property are skipped (they cannot inherit it).
//
// S044 broadening: when `captureAttachedInherits` is set (the property is a
// globally registered Inherits/InheritOnly ATTACHED property), a descendant is
// captured even if it has NO local store entry — a purely-passive attached
// descendant inherits the value through the global-resolved read (S043) and must
// be notified on the parent's change, not just materialized/registering ones.
VAML_CORE_EXPORT
void DependencyObject::collect_descendant_effectives(
    const DependencyObject& root, std::string_view name, bool captureAttachedInherits,
    std::vector<std::pair<DependencyObject*, Value>>& out)
{
    // S144: out-param accessor (no return-by-value). `out` is the caller's sink.
    out.clear();
    root.visit_scope(name, out, captureAttachedInherits);
}

// Recursive scope walker used by collect_descendant_effectives. `self` is the
// object whose inheritance-scope we walk; if `self` itself is the root (the
// origin) we do NOT include it (we only care about descendants). Each visited
// object contributes (object, its CURRENT effective value) when it registers
// `name` OR (S044) when `captureAttachedInherits` is set — a passive attached
// Inherits descendant that has no local store entry is still captured so it can
// be notified. Notify-side filtering (old != new) makes over-capture harmless.
void DependencyObject::visit_scope(
    std::string_view name,
    std::vector<std::pair<DependencyObject*, Value>>& out,
    bool captureAttachedInherits) const
{
    for (DependencyObject* child : inheritance_scope_)
    {
        if (child == nullptr)
        {
            continue;
        }
        // A descendant that registers the property is captured with its current
        // effective value; one that does not is captured ONLY when the property
        // is a globally-registered attached Inherits/InheritOnly property (S044).
        // The descendant's store is affinity-keyed on interned views; intern
        // `name` to the canonical (content-equal) view for the lookup.
        const PD* entry = child->store_.find(intern(name));
        if (entry != nullptr || captureAttachedInherits)
        {
            out.emplace_back(child, child->GetEffectiveValue(PropertyId::of(name)));
        }
        // Recurse into this child's own scope so great-grandchildren are captured.
        child->visit_scope(name, out, captureAttachedInherits);
    }
}

// S041 descendant push, walk 2 (AFTER the origin has stored its new value):
// for each captured descendant, recompute the NEW effective value and enqueue a
// pushed notification into that descendant's OWN notification queue ONLY when
// old != new. A descendant whose effective is unchanged (local-wins node, or one
// shielded by a nearer ancestor's still-present local override) produces no
// notification — S040's local-wins is respected through an intermediate, not just
// at the direct child.
VAML_CORE_EXPORT
void DependencyObject::notify_descendants(
    std::string_view name,
    const std::vector<std::pair<DependencyObject*, Value>>& before)
{
    for (const auto& [desc, oldEffective] : before)
    {
        if (desc == nullptr)
        {
            continue;
        }
        // Recompute this descendant's NEW effective value AFTER the origin stored.
        const Value newEffective = desc->GetEffectiveValue(PropertyId::of(name));

        // Only a descendant whose effective value ACTUALLY changed gets pushed
        // (NO-LAZY: never over-notify).
        if (!(oldEffective == newEffective))
        {
            desc->queue_.enqueue(PropertyId::of(name), oldEffective, newEffective);
        }
    }
}

} // namespace vaml
