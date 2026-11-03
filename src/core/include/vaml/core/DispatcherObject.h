#pragma once
// VAML DispatcherObject — the affinity-thread base (WPF
// System.Windows.Threading.DispatcherObject). 11n-4 — DAG anchor
// `DispatcherObject`. The base of CollectionView (and, in WPF, of
// DependencyObject): it captures the affinity thread at construction and
// offers the affinity-thread contract (check_access / verify_affinity).
//
// ADR-018 / DECISION-2: ZERO new locks. The affinity id is captured once at
// construction (the affinity thread monotonic id) and is immutable after; a
// wrong-thread access is detected by comparing the caller's thread id to the
// affinity id — a pure read, no mutex. Cross-thread producers on a
// CollectionView are handled by the DECISION-2 change-log (opt-in
// AllowsCrossThreadChanges), never by a lock here.
//
// The DispatcherObject is not a Dispatcher owner (WPF threads a Dispatcher
// per-thread); VAML keeps the affinity as a monotonic thread-id (std::thread::id
// wrapped) so single-threaded UI paths stay lock-free per ADR-018. The
// framework does NOT impose a Dispatcher/InvokeAsync pump — the app owns the
// pump (S540 IUiExecutor / 11z seam); this type only carries the affinity
// contract a CollectionView verifies against.

#include "vaml/core/export.h"

#include <thread>

namespace vaml {

// A serializable affinity identity (one per OS thread, captured at
// construction). Moveable/copyable; the value is the owning thread's id.
class VAML_CORE_EXPORT DispatcherAffinity
{
public:
    DispatcherAffinity() : tid_(std::this_thread::get_id()) {}
    explicit DispatcherAffinity(std::thread::id tid) noexcept : tid_(tid) {}

    [[nodiscard]] bool same_thread() const noexcept
    {
        return tid_ == std::this_thread::get_id();
    }
    [[nodiscard]] std::thread::id id() const noexcept { return tid_; }

    bool operator==(const DispatcherAffinity& o) const noexcept { return tid_ == o.tid_; }
    bool operator!=(const DispatcherAffinity& o) const noexcept { return tid_ != o.tid_; }

private:
    std::thread::id tid_;
};

// The affinity-thread base. A derived object captures its affinity at
// construction and can verify that a caller is on that thread.
class VAML_CORE_EXPORT DispatcherObject
{
public:
    DispatcherObject() : affinity_(std::this_thread::get_id()) {}
    virtual ~DispatcherObject() = default;

    DispatcherObject(const DispatcherObject&) = delete;
    DispatcherObject& operator=(const DispatcherObject&) = delete;
    DispatcherObject(DispatcherObject&&) noexcept = default;
    DispatcherObject& operator=(DispatcherObject&&) noexcept = default;

    // True iff the caller is on the object's affinity thread. PURE READ — no
    // lock (ADR-018).
    [[nodiscard]] bool check_access() const noexcept { return affinity_.same_thread(); }

    // The affinity identity this object was created on.
    [[nodiscard]] DispatcherAffinity affinity() const noexcept { return affinity_; }

    // Returns true when the caller is on the affinity thread. When false the
    // caller is a cross-thread producer — the DECISION-2 change-log governs
    // that path (a CollectionView opt-in AllowsCrossThreadChanges), never a
    // lock.
    [[nodiscard]] bool verify_affinity() const noexcept { return affinity_.same_thread(); }

private:
    DispatcherAffinity affinity_;
};

} // namespace vaml
