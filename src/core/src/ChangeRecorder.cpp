// S038 — ChangeRecorder out-of-line definitions.
// Pending per-property changes for deferred notification (dep:S037).
//
// Design (recorded in ChangeRecorder.h — NO LAZY LLM): per-property with coalescing
// (first `old` kept, latest `new` adopted); deliberately single-threaded (a
// pipeline-internal deferral device; the cross-thread ChangeQueue/mutex is S041's
// job). Keyed on an OWNED name (S036/S037 gotcha honored) + insertion-ordered index
// (vector + name->index map), so flush emits in first-record order, not hash order.
//
// S146 (refactor gate) — the pimpl is COLLAPSED: `order` + `byName` are now
// BY-VALUE members of ChangeRecorder (see ChangeRecorder.h); no Impl heap node,
// no impl_-> trampoline. NameTable.h was promoted to the public detail tree
// (vaml/core/detail/NameTable.h) solely to make this collapse legal.

#include "vaml/core/ChangeRecorder.h"

#include "vaml/core/Intern.h" // S140: affinity-thread interned name keys

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace vaml {

using Pending = ChangeRecorder::Pending;

VAML_CORE_EXPORT
ChangeRecorder::ChangeRecorder() = default;

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

    // S143 B1: content-keyed probe index. NameTable::insert returns a pointer
    // to the stored Pending (nullptr => the name is already present = coalesce).
    Pending pending{name, oldValue, newValue};
    Pending* stored = byName.insert(std::string(name), std::move(pending));
    if (stored != nullptr)
    {
        // First record for this property: track first-record order too.
        order.push_back(name);
        return;
    }
    // Coalesce (S042 realized-early): keep the FIRST old, adopt the LATEST new.
    Pending* existing = byName.find(name);
    existing->newValue = newValue;
}

VAML_CORE_EXPORT
bool ChangeRecorder::has_pending(PropertyId id) const
{
    return byName.find(vaml::intern(id.name())) != nullptr;
}

VAML_CORE_EXPORT
std::size_t ChangeRecorder::pending_count() const
{
    return order.size();
}

VAML_CORE_EXPORT
std::size_t ChangeRecorder::flush(const NotifyFn& notify)
{
    // Snapshot-then-emit (mirrors S037's swap-out): take the pending set out under
    // our single-threaded ownership, then replay. A notify handler that re-record()s
    // lands in a fresh recorder state and is NOT double-emitted this pass.
    //
    // S146 (no pimpl): snapshot into LOCAL by-value containers (their names are
    // interned views, stable for the thread's life), so the members are cleared
    // and a re-record() lands in fresh state — identical to the old Impl swap-out
    // semantics, without the Impl heap node.
    std::vector<std::string_view> snapOrder;
    vaml::detail::NameTable<Pending> snapByName;
    snapOrder.swap(order);
    snapByName.swap(byName);

    std::size_t emitted = 0;
    for (const std::string_view& name : snapOrder)
    {
        // The NameTable owns the key bytes (a stable std::string), and its
        // Pending carries the interned view; look the stored Pending up by the
        // owned key (content find) and read the value + owned key for PropertyId.
        const Pending* p = snapByName.find(name);
        if (p == nullptr)
        {
            continue; // defensive: order/byName kept in lockstep, should not happen
        }
        const std::string* key = snapByName.find_key(name);
        notify(PropertyId::of(*key), p->oldValue, p->newValue);
        ++emitted;
    }
    return emitted;
}

VAML_CORE_EXPORT
void ChangeRecorder::clear()
{
    order.clear();
    byName = vaml::detail::NameTable<Pending>();
}

} // namespace vaml
