#pragma once
// VAML ChangeRecorder — pending per-property changes for deferred notification.
// S038 (dep:S037, dep:S036_5).
//
// S037 delivered VALUE-MARSHALLING: a per-object, mutex-protected pending queue where
// wrong-thread writes live until ProcessPendingChanges() drains them on the affinity
// thread. That queue carries *values to apply later*.
//
// S038 delivers a DIFFERENT concept: deferred *notification* of *per-property*
// changes. Today (S036/S037) the `changed` cb fires synchronously on the affinity
// thread at apply time. A real notification pipeline (S041) defers those
// notifications and dispatches them as a batch. S038 provides the standalone
// `ChangeRecorder` device that *holds* those pending per-property notifications so
// they can be flushed later.
//
// SCOPE (recorded, not silent — NO LAZY LLM):
//   - PER-PROPERTY WITH COALESCING BY DESIGN. At most one pending record per
//     property: the FIRST `old` is kept, the LATEST `new` is adopted. Two record()
//     calls for the same property before a flush => ONE notification. This is
//     exactly the S042 outcome ("two sets of same prop before drain == one
//     notification"); S042 is annotated REALIZED-EARLY (matching the existing
//     S036_1..S036_5 -> S047 precedent) so it is not duplicated later.
//   - DELIBERATELY SINGLE-THREADED. ChangeRecorder is a pipeline-internal
//     deferral device; it is NOT mutex-protected. The cross-thread ChangeQueue /
//     mutex is S041's job (a generalization of S037's per-object queue). This keeps
//     S038 out of S041's scope. No DependencyObject callback rewiring happens here
//     (that would break the S037 marshalling tests and is S041's work).
//   - NOT MINIMAL REAL, NOT A STUB. ChangeRecorder is a complete, fully-tested
//     public symbol with an explicitly scheduled consumer (S041's per-thread
//     ChangeQueue + notification processor) — recorded via NO-GAP, never a silent
//     backfill.
//
// NAME-KEY (S140): the pending record keys on an INTERNED std::string_view
// (thread-local retain-forever pool — ADR-018 lockless). ChangeRecorder is
// deliberately single-threaded (a pipeline-internal deferral device), so its
// per-thread interned views are stable for the thread's life and content-equal
// across re-interns (string_view equality is by content). This removes the
// transient std::string(id.name()) materialized per record/lookup. Ordering is
// tracked via an insertion-ordered index (vector + name-map), NOT unordered_map
// iteration order.
//
// Storage is pimpl'd (class Impl + unique_ptr) so the public header exposes no
// std::vector / std::unordered_map and no std::function value — keeping the ABI
// surface stable and move-only members out of the public declaration (mirrors the
// S036/S037 DependencyObject pimpl).

#include "vaml/core/PropertyId.h"
#include "vaml/core/Value.h"

#include <cstddef>
#include <functional>
#include <memory>

namespace vaml {

class VAML_CORE_EXPORT ChangeRecorder
{
public:
    ChangeRecorder();                 // empty recorder, no pending changes
    ~ChangeRecorder();

    // move-only (copy deleted) so it can live in a pimpl/unique_ptr cleanly.
    ChangeRecorder(const ChangeRecorder&) = delete;
    ChangeRecorder& operator=(const ChangeRecorder&) = delete;
    ChangeRecorder(ChangeRecorder&&) noexcept;
    ChangeRecorder& operator=(ChangeRecorder&&) noexcept;

    // Records a pending change for `id`. First time for a property: stores
    // (old,new). A repeat for the SAME property COALESCES: the FIRST `old` is kept
    // and the LATEST `new` is adopted (at most one pending notification per property,
    // mirroring S042). The id's NAME is copied (owned), so the record outlives the
    // registering key.
    void record(PropertyId id, const Value& oldValue, const Value& newValue);

    // True when at least one pending change is recorded for `id`.
    [[nodiscard]] bool has_pending(PropertyId id) const;

    // Number of DISTINCT properties currently holding a pending change.
    [[nodiscard]] std::size_t pending_count() const;

    // The callback shape for a flushed notification: (id, old, new).
    using NotifyFn = std::function<void(PropertyId, const Value&, const Value&)>;

    // Emits one notify(id, old, new) per pending property in FIRST-RECORD order,
    // then clears. Returns the number of notifications emitted. Snapshot-then-emit:
    // a notify handler that re-record()s lands in a FRESH (empty) recorder, so it is
    // NOT double-emitted in the same flush (mirrors S037's swap-out). Flushing an
    // empty recorder returns 0 and invokes the callback for nothing.
    std::size_t flush(const NotifyFn& notify);

    // Drops all pending changes (pending_count() becomes 0); a subsequent flush
    // returns 0.
    void clear();

private:
    struct Impl; // pimpl: hides std::vector + std::unordered_map + std::function
    std::unique_ptr<Impl> impl_;
};

} // namespace vaml
