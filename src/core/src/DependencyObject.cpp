// S036 + S037 — DependencyObject out-of-line definitions.
// S036 (dep:S035): implements the S035 apply pipeline verbatim in Set():
//   coerce -> validate(on the coerced value) -> fire changed cb ONLY on a
//   real change -> store.
// S037 (dep:S036_5): adds wrong-thread write QUEUING — a Set() from a non-affine
//   thread runs coerce+validate at queue time but POSTS (queued-not-applied),
//   and ProcessPendingChanges() (affinity-thread-only) later drains FIFO through
//   the same store half.
//
// Storage is pimpl'd so the public header exposes no std::unordered_map and no
// move-only PropertyMetadata value member.

#include "vaml/core/DependencyObject.h"

#include "vaml/core/ChangeQueue.h"       // S041: per-object notification queue
#include "vaml/core/ChangeRecorder.h"    // S041: per-property coalescing at drain
#include "vaml/core/DependencyProperty.h" // global descriptor for S039 inheritance

#include <algorithm>
#include <deque>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace vaml {

struct DependencyObject::Impl
{
    // One entry per stored/local property. `local` is empty (Value::Null) until
    // a Set() stores an override; an empty `local` means "default applied".
    // The key is stored by value (ref returned to the caller) and the property
    // is addressed by its NAME.
    //
    // S043 — metadata provider has TWO modes (exactly one active):
    //   - OWNED: object-scoped registration (register_property) owns its
    //     move-only PropertyMetadata in `owned` (non-null). This is the S036
    //     object-scoped path, unchanged.
    //   - BORROWED: an ATTACHED property materialized onto this object (which
    //     did NOT locally register it) holds a NON-OWNING pointer to the
    //     GLOBAL DependencyProperty's metadata (`borrowed`, non-null). This is
    //     safe because the global DependencyProperty (and its metadata) lives
    //     for the process lifetime, so the pointer never dangles.
    // metadata() returns whichever is active. The move-only PropertyMetadata is
    // NEVER copied into an object's entry (S043 key design decision).
    struct PropEntry
    {
        PropertyKey              key;
        std::unique_ptr<PropertyMetadata> owned;    // non-null if object-scoped (owned)
        const PropertyMetadata*  borrowed = nullptr; // non-null if attached (borrowed)
        Value                    local = Value::Null(); // Value::Null == no local override

        // Owned (object-scoped registration).
        PropEntry(PropertyKey k, PropertyMetadata m)
            : key(std::move(k)), owned(std::make_unique<PropertyMetadata>(std::move(m))) {}

        // Borrowed (attached): references the global metadata, never copies it.
        PropEntry(PropertyKey k, const PropertyMetadata* gmd)
            : key(std::move(k)), owned(nullptr), borrowed(gmd) {}

        const PropertyMetadata& metadata() const noexcept
        {
            return owned != nullptr ? *owned : *borrowed;
        }
    };

    // S037: a queued wrong-thread write — the OWNED property name (so the
    // queue outlives the registering PropertyKey's storage) plus the already
    // COERCED + VALIDATED value. Coerce/validate run at queue time; the drain
    // only replays accepted values through the store half.
    struct PendingWrite
    {
        std::string name;
        Value       coerced;

        PendingWrite(std::string n, Value v)
            : name(std::move(n)), coerced(std::move(v)) {}
    };

    // Keyed by the OWNED property NAME (std::string), NOT by PropertyId:
    // a PropertyId is a non-owning std::string_view that would dangle once the
    // registering PropertyKey (its storage source) dies. Keying on an owned
    // std::string makes lookups stable against both SSO and heap-backed names.
    std::unordered_map<std::string, PropEntry> store;
    std::thread::id                            affinity = std::this_thread::get_id();

    // S039 parent link (loose, non-owning). Used by effective-value resolution
    // to walk the ancestor chain. Defaults to nullptr (no parent); set_parent
    // installs it. Intentionally no cascade on set here (S040 owns that).
    DependencyObject* parent = nullptr;

    // S041 §2.3 registration-ordered property-changed subscribers, keyed by the
    // OWNED property NAME (same gotcha as store). `subs[name]` is a vector of
    // (token, handler) pairs in REGISTRATION ORDER. `nextSubToken` is the
    // monotonically increasing removal-token counter (never reused, so an
    // expired token can never alias a live one).
    std::unordered_map<std::string, std::vector<std::pair<DependencyObject::PropertyChangedToken, DependencyObject::PropertyChangedHandler>>> subs;
    DependencyObject::PropertyChangedToken nextSubToken = 1;

    // S041 descendant push: the object's OWN per-thread notification queue
    // (affinity == this object's affinity). set_parent() registers this object
    // into the parent's inheritance-scope below; a parent's Inherits/InheritOnly
    // change enqueues pushed notifications here (via notify_descendants), and
    // ProcessNotificationQueue() drains the queue into this object's
    // registration-ordered subscribers.
    ChangeQueue notificationQueue;

    // S041 §2.4 non-owning inheritance-scope — the INVERSE edge of `parent`.
    // Each DependencyObject keeps the set of DependencyObject* that currently
    // have THIS object as their parent (registered by set_parent). A parent
    // walks this scope to push Inherits/InheritOnly changes to inheriting
    // descendants. Loose (non-owning) pointers, like `parent` (S039). DISTINCT
    // from the visual children list (S121) — recorded, no overlap, no gap.
    std::vector<DependencyObject*> inheritanceScope;

    // S037: the ONE genuinely cross-thread datum — protected by queue_mutex.
    // Written by any thread (wrong-thread Set), drained by the affinity thread
    // (ProcessPendingChanges). The value store itself is NOT in this mutex:
    // only the affinity thread stores (on-affine Set or drain), so the store
    // stays affinity-thread-owned by construction (see the S037 cross-thread
    // contract in DependencyObject.h).
    std::deque<PendingWrite> pending;
    std::mutex               queue_mutex;
};

VAML_CORE_EXPORT
DependencyObject::DependencyObject()
    : impl_(std::make_unique<Impl>())
{
}

VAML_CORE_EXPORT
DependencyObject::~DependencyObject() = default;

VAML_CORE_EXPORT
DependencyObject::DependencyObject(DependencyObject&&) noexcept = default;

VAML_CORE_EXPORT
DependencyObject& DependencyObject::operator=(DependencyObject&&) noexcept = default;

VAML_CORE_EXPORT
const PropertyKey& DependencyObject::register_property(PropertyKey key, PropertyMetadata metadata)
{
    // Type coherence: never register a key/metadata pair that disagree on the
    // bound value type (NO-LAZY guard).
    if (key.type() != metadata.boundType())
    {
        throw ValueTypeMismatch("DependencyObject::register_property: key.type() does not match metadata's bound value type");
    }

    // Determine the canonical owned name (dedupe + store key) BEFORE moving
    // `key` into the entry.
    const std::string name = key.name();

    const auto [it, inserted] = impl_->store.emplace(
        name, Impl::PropEntry(std::move(key), std::move(metadata)));
    if (!inserted)
    {
        // Name already registered -> throw; emplace with a duplicate key does
        // NOT overwrite the existing entry, so the store is unchanged.
        throw PropertyAlreadyRegistered("DependencyObject::register_property: a property with this name is already registered");
    }

    return it->second.key;
}

VAML_CORE_EXPORT
Value DependencyObject::Get(PropertyId id) const
{
    return GetEffectiveValue(id);
}

VAML_CORE_EXPORT
Value DependencyObject::GetEffectiveValue(PropertyId id) const
{
    const auto it = impl_->store.find(std::string(id.name()));
    const bool inStore = (it != impl_->store.end());

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
        mdPtr = &it->second.metadata();
        local = it->second.local.isNull() ? Value::Null() : it->second.local;
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
    const Value inherited = nearest_ancestor_local(*this, std::string(id.name()));

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
    if (impl_->parent != nullptr)
    {
        auto& scope = impl_->parent->impl_->inheritanceScope;
        const auto it = std::find(scope.begin(), scope.end(), this);
        if (it != scope.end())
        {
            scope.erase(it);
        }
    }

    impl_->parent = parent;

    // S041 §2.4: register this object into the NEW parent's inheritance-scope
    // (the inverse edge, non-owning). A parent walks this scope to push
    // Inherits/InheritOnly changes to inheriting descendants.
    if (impl_->parent != nullptr)
    {
        auto& scope = impl_->parent->impl_->inheritanceScope;
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
    return impl_->parent;
}

VAML_CORE_EXPORT
Value DependencyObject::nearest_ancestor_local(const DependencyObject& self, const std::string& name)
{
    // Straight upward scan for the nearest ancestor with a LOCAL override. A
    // plain-Inherits property's net value is "nearest ancestor with a local
    // override else default", because an intermediate ancestor with no local
    // defers to ITS own ancestor anyway — so this is exactly equivalent to
    // recursing into each ancestor's effective value, without the throw hazard
    // and re-deriving a single descriptor (flags/default come from the caller).
    const DependencyObject* cur = self.parent();
    while (cur != nullptr)
    {
        const auto it = cur->impl_->store.find(name);
        if (it != cur->impl_->store.end() && !it->second.local.isNull())
        {
            return it->second.local;
        }
        cur = cur->parent();
    }
    return Value::Null();
}

VAML_CORE_EXPORT
SetResult DependencyObject::Set(PropertyId id, const Value& input)
{
    const std::string name = std::string(id.name());

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
    const auto sit = impl_->store.find(name);
    if (sit != impl_->store.end())
    {
        mdPtr = &sit->second.metadata();
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
    if (impl_->affinity != std::this_thread::get_id())
    {
        std::lock_guard<std::mutex> lock(impl_->queue_mutex);
        impl_->pending.emplace_back(name, coerced);
        return SetResult::QUEUED;
    }

    // On the AFFINE thread, lazily materialize a BORROWED store entry for an
    // attached property so the local override has a home (S043). The global
    // descriptor is guaranteed present (we resolved `md` from it above).
    auto it = impl_->store.find(name);
    if (it == impl_->store.end())
    {
        const DependencyProperty& globalDp = DependencyProperty::Get(name);
        PropertyKey attachedKey(name, globalDp.metadata().boundType(),
                                globalDp.owner_type_id(), globalDp.flags());
        auto [matIt, matInserted] = impl_->store.emplace(
            name, Impl::PropEntry(std::move(attachedKey), &globalDp.metadata()));
        (void)matInserted;
        it = matIt;
    }
    Impl::PropEntry& entry = it->second;

    // Compute the current EFFECTIVE value (local override else default) for
    // both the change comparison and the changed cb's `old`.
    const Value currentEffective = entry.local.isNull() ? md.defaultValue() : entry.local;

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
    // (entry.owned == nullptr): the origin hosts the value but does NOT own the
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
            && entry.owned == nullptr;
        descendantBefore = collect_descendant_effectives(*this, name, captureAttachedInherits);
    }

    md.notifyChanged(currentEffective, coerced);

    // S035 pipeline step 4: store.
    entry.local = coerced;

    // S041 §2.4: propagate the real change to inheriting descendants (S040's
    // recorded obligation, delivered here, NOT silent).
    if (inherits)
    {
        notify_descendants(std::string(id.name()), descendantBefore);
    }
    return SetResult::APPLIED;
}

VAML_CORE_EXPORT
void DependencyObject::ProcessPendingChanges()
{
    // Drain is affinity-thread-only (S037 D3): a wrong-thread drain would
    // mutate the affinity-owned store from the wrong thread — throw rather than
    // silently no-op (NO-LAZY).
    if (impl_->affinity != std::this_thread::get_id())
    {
        throw WrongThreadException("DependencyObject::ProcessPendingChanges: must be called from the object's affinity thread");
    }

    // Swap the whole queue out under the lock so wrong-thread writers hitting
    // during the drain enqueue into a FRESH queue (their writes drain next
    // call), never into the one we are about to consume. The store half below
    // then runs lock-free on the affinity thread.
    std::deque<Impl::PendingWrite> snapshot;
    {
        std::lock_guard<std::mutex> lock(impl_->queue_mutex);
        snapshot.swap(impl_->pending);
    }

    // FIFO replay through the S036 store half — exactly the on-affine pipeline's
    // steps 3-4 (compare/notify/store) for each queued write. A queued write
    // whose coerced value equals the current effective at APPLY time is a
    // no-change (no cb). Two queued writes [5,7] on a default-0 prop sequentially
    // fire cb(0->5) then cb(5->7), preserving real-change semantics.
    for (auto& write : snapshot)
    {
        auto it = impl_->store.find(write.name);
        if (it == impl_->store.end())
        {
            // Not in the local store. If the property is globally registered, this
            // is an ATTACHED property a wrong-thread writer targeted before the
            // object had a store entry — materialize a BORROWED entry (affinity
            // thread, legal) and apply. If it is NOT globally registered, skip it
            // (property was never registered; nothing to do).
            const DependencyProperty* globalDp = DependencyProperty::TryGet(write.name);
            if (globalDp == nullptr)
            {
                continue; // unregistered after the write was queued; skip
            }
            PropertyKey attachedKey(write.name, globalDp->metadata().boundType(),
                                    globalDp->owner_type_id(), globalDp->flags());
            auto [matIt, matInserted] = impl_->store.emplace(
                write.name, Impl::PropEntry(std::move(attachedKey), &globalDp->metadata()));
            (void)matInserted;
            it = matIt;
        }
        Impl::PropEntry& entry = it->second;
        const PropertyMetadata& md = entry.metadata();

        const Value currentEffective = entry.local.isNull() ? md.defaultValue() : entry.local;
        if (write.coerced == currentEffective)
        {
            continue; // no real change at apply time -> no cb
        }

        // S041 §2.4: capture descendants' current effective values (BEFORE the
        // store) so the push carries each descendant's own old->new delta.
        // S044: for an ATTACHED Inherits/InheritOnly property (origin's entry is
        // BORROWED — owned == nullptr), also capture purely passive descendants
        // with no local store entry (broadened walk). An object-scoped Inherits
        // property keeps the S041 behavior.
        const DependencyProperty* dp = DependencyProperty::TryGet(write.name);
        const bool inherits =
            has_flag(md.flags(), PropertyFlags::Inherits)
            || (dp != nullptr && has_flag(dp->flags(), PropertyFlags::Inherits));
        std::vector<std::pair<DependencyObject*, Value>> descendantBefore;
        if (inherits)
        {
            const bool captureAttachedInherits =
                dp != nullptr && has_flag(dp->flags(), PropertyFlags::Inherits)
                && entry.owned == nullptr;
            descendantBefore = collect_descendant_effectives(*this, write.name, captureAttachedInherits);
        }

        md.notifyChanged(currentEffective, write.coerced);
        entry.local = write.coerced;

        // S041 §2.4: propagate the applied queued change to inheriting descendants
        // exactly like an on-affine one.
        if (inherits)
        {
            notify_descendants(write.name, descendantBefore);
        }
    }
}

VAML_CORE_EXPORT
std::size_t DependencyObject::pending_change_count() const noexcept
{
    std::lock_guard<std::mutex> lock(impl_->queue_mutex);
    return impl_->pending.size();
}

VAML_CORE_EXPORT
std::thread::id DependencyObject::thread_affinity_id() const noexcept
{
    return impl_->affinity;
}

VAML_CORE_EXPORT
DependencyObject::PropertyChangedToken DependencyObject::add_property_changed(
    PropertyId id, PropertyChangedHandler handler)
{
    // Own the name (S036/S037/S038 gotcha) and append in REGISTRATION ORDER.
    // The token is a monotonically-increasing counter that is never reused, so
    // an expired token can never alias a live one.
    const DependencyObject::PropertyChangedToken token = impl_->nextSubToken++;
    impl_->subs[std::string(id.name())].emplace_back(token, std::move(handler));
    return token;
}

VAML_CORE_EXPORT
void DependencyObject::remove_property_changed(PropertyChangedToken token)
{
    // Removal is O(n) per property, but preserves the registration order of
    // what remains. We iterate every subscribed property because the token is
    // global (not per-property); this is correct and simple, and the subscriber
    // list is expected to be small.
    for (auto& [name, handlers] : impl_->subs)
    {
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
    if (impl_->affinity != std::this_thread::get_id())
    {
        throw WrongThreadException("DependencyObject::ProcessNotificationQueue: must be called from the object's affinity thread");
    }

    // Drain the object's own ChangeQueue (swap-out inside ChangeQueue::process)
    // and fire this object's registration-ordered subscribers for each pushed
    // change. Two pushed changes of the same property before this drain collapse
    // to one notification (S038 coalescing, consumed end-to-end — S042 outcome).
    vaml::ChangeRecorder recorder;
    const std::size_t drained = impl_->notificationQueue.process(
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
        const auto it = impl_->subs.find(std::string(id.name()));
        if (it == impl_->subs.end())
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
    return impl_->notificationQueue.pending_count();
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
std::vector<std::pair<DependencyObject*, Value>> DependencyObject::collect_descendant_effectives(
    const DependencyObject& root, const std::string& name, bool captureAttachedInherits)
{
    std::vector<std::pair<DependencyObject*, Value>> result;
    root.visit_scope(name, result, captureAttachedInherits);
    return result;
}

// Recursive scope walker used by collect_descendant_effectives. `self` is the
// object whose inheritance-scope we walk; if `self` itself is the root (the
// origin) we do NOT include it (we only care about descendants). Each visited
// object contributes (object, its CURRENT effective value) when it registers
// `name` OR (S044) when `captureAttachedInherits` is set — a passive attached
// Inherits descendant that has no local store entry is still captured so it can
// be notified. Notify-side filtering (old != new) makes over-capture harmless.
void DependencyObject::visit_scope(
    const std::string& name,
    std::vector<std::pair<DependencyObject*, Value>>& out,
    bool captureAttachedInherits) const
{
    for (DependencyObject* child : impl_->inheritanceScope)
    {
        if (child == nullptr)
        {
            continue;
        }
        // A descendant that registers the property is captured with its current
        // effective value; one that does not is captured ONLY when the property
        // is a globally-registered attached Inherits/InheritOnly property (S044).
        const auto it = child->impl_->store.find(name);
        if (it != child->impl_->store.end() || captureAttachedInherits)
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
    const std::string& name,
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
            desc->impl_->notificationQueue.enqueue(PropertyId::of(name), oldEffective, newEffective);
        }
    }
}

} // namespace vaml
