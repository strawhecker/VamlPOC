// S038 — ChangeRecorder out-of-line definitions.
// Pending per-property changes for deferred notification (dep:S037).
//
// Design (recorded in ChangeRecorder.h — NO LAZY LLM): per-property with coalescing
// (first `old` kept, latest `new` adopted); deliberately single-threaded (a
// pipeline-internal deferral device; the cross-thread ChangeQueue/mutex is S041's
// job). Keyed on an OWNED name (S036/S037 gotcha honored) + insertion-ordered index
// (vector + name->index map), so flush emits in first-record order, not hash order.
//
// Storage is pimpl'd so the public header exposes no std::vector / std::unordered_map
// / std::function value member.

#include "vaml/core/ChangeRecorder.h"

#include "vaml/core/Intern.h" // S140: affinity-thread interned name keys

#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace vaml {

struct ChangeRecorder::Impl
{
    // One pending notification per DISTINCT property. `name` is an INTERNED
    // std::string_view (thread-local retain-forever pool, S140/ADR-018): the
    // recorder is single-threaded and its per-thread interned views are stable
    // for the thread's life, content-equal across re-interns. This removes the
    // transient std::string(id.name()) per record/lookup.
    struct Pending
    {
        std::string_view name;
        Value            oldValue;
        Value            newValue;
    };

    // Insertion-ordered index: `order` stores interned names in first-record
    // order; `byName` maps each interned view to its pending record.
    // `pending_count()` is order.size() (== byName.size()).
    std::vector<std::string_view>          order;
    std::unordered_map<std::string_view, Pending> byName;
};

VAML_CORE_EXPORT
ChangeRecorder::ChangeRecorder()
    : impl_(std::make_unique<Impl>())
{
}

VAML_CORE_EXPORT
ChangeRecorder::~ChangeRecorder() = default;

VAML_CORE_EXPORT
ChangeRecorder::ChangeRecorder(ChangeRecorder&&) noexcept = default;

VAML_CORE_EXPORT
ChangeRecorder& ChangeRecorder::operator=(ChangeRecorder&&) noexcept = default;

VAML_CORE_EXPORT
void ChangeRecorder::record(PropertyId id, const Value& oldValue, const Value& newValue)
{
    // Intern the name to the calling thread's canonical view (S140) — stable
    // for the thread's life, content-equal across re-interns.
    const std::string_view name = vaml::intern(id.name());

    auto [it, inserted] = impl_->byName.try_emplace(name, Impl::Pending{name, oldValue, newValue});
    if (inserted)
    {
        // First record for this property: also track first-record order.
        impl_->order.push_back(name);
        return;
    }
    // Coalesce (S042 realized-early): keep the FIRST old, adopt the LATEST new.
    it->second.newValue = newValue;
}

VAML_CORE_EXPORT
bool ChangeRecorder::has_pending(PropertyId id) const
{
    return impl_->byName.count(vaml::intern(id.name())) != 0;
}

VAML_CORE_EXPORT
std::size_t ChangeRecorder::pending_count() const
{
    return impl_->order.size();
}

VAML_CORE_EXPORT
std::size_t ChangeRecorder::flush(const NotifyFn& notify)
{
    // Snapshot-then-emit (mirrors S037's swap-out): take the pending set out under
    // our single-threaded ownership, then replay. A notify handler that re-record()s
    // lands in a fresh recorder state and is NOT double-emitted this pass.
    Impl snapshot;
    snapshot.order.swap(impl_->order);
    snapshot.byName.swap(impl_->byName);

    std::size_t emitted = 0;
    for (const std::string_view& name : snapshot.order)
    {
        auto it = snapshot.byName.find(name);
        if (it == snapshot.byName.end())
        {
            continue; // defensive: order/byName kept in lockstep, should not happen
        }
        const Impl::Pending& p = it->second;
        notify(PropertyId::of(p.name), p.oldValue, p.newValue);
        ++emitted;
    }
    return emitted;
}

VAML_CORE_EXPORT
void ChangeRecorder::clear()
{
    impl_->order.clear();
    impl_->byName.clear();
}

} // namespace vaml
