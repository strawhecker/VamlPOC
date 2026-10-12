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

#include <deque>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>

namespace vaml {

struct DependencyObject::Impl
{
    // One entry per REGISTERED property. `local` is empty (Value::Null) until
    // a Set() stores an override; an empty `local` means "default applied".
    // The key is stored by value (ref returned to the caller) and the
    // property is addressed by its NAME.
    struct PropEntry
    {
        PropertyKey    key;
        PropertyMetadata metadata;
        Value          local = Value::Null(); // Value::Null == no local override

        PropEntry(PropertyKey k, PropertyMetadata m)
            : key(std::move(k)), metadata(std::move(m)) {}
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
    const auto it = impl_->store.find(std::string(id.name()));
    if (it == impl_->store.end())
    {
        throw PropertyNotFound("DependencyObject::Get: no property registered with this id");
    }

    const Impl::PropEntry& entry = it->second;
    // "Default applied": empty local == no override -> return defaultValue().
    return entry.local.isNull() ? entry.metadata.defaultValue() : entry.local;
}

VAML_CORE_EXPORT
SetResult DependencyObject::Set(PropertyId id, const Value& input)
{
    const auto it = impl_->store.find(std::string(id.name()));
    if (it == impl_->store.end())
    {
        throw PropertyNotFound("DependencyObject::Set: no property registered with this id");
    }

    Impl::PropEntry& entry = it->second;
    PropertyMetadata& md   = entry.metadata;

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
    if (impl_->affinity != std::this_thread::get_id())
    {
        std::lock_guard<std::mutex> lock(impl_->queue_mutex);
        impl_->pending.emplace_back(entry.key.name(), coerced);
        return SetResult::QUEUED;
    }

    // Compute the current EFFECTIVE value (local override else default) for
    // both the change comparison and the changed cb's `old`.
    const Value currentEffective = entry.local.isNull() ? md.defaultValue() : entry.local;

    // S035 pipeline step 3: fire changed cb ONLY on a real change.
    if (coerced == currentEffective)
    {
        return SetResult::NO_CHANGE;
    }
    md.notifyChanged(currentEffective, coerced);

    // S035 pipeline step 4: store.
    entry.local = coerced;
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
        const auto it = impl_->store.find(write.name);
        if (it == impl_->store.end())
        {
            continue; // property was unregistered after the write was queued; skip
        }
        Impl::PropEntry& entry = it->second;
        PropertyMetadata& md   = entry.metadata;

        const Value currentEffective = entry.local.isNull() ? md.defaultValue() : entry.local;
        if (write.coerced == currentEffective)
        {
            continue; // no real change at apply time -> no cb
        }
        md.notifyChanged(currentEffective, write.coerced);
        entry.local = write.coerced;
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

} // namespace vaml
