#pragma once
// VAML CollectionView — the live collection read model (WPF
// System.Windows.Data.CollectionView : DispatcherObject, ICollectionView,
// INotifyPropertyChanged). 11n-4 — DAG anchor `CollectionView`, the live read
// model under ItemsControl.
//
// WPF-faithful (verified against the WPF source, CollectionView.cs):
//   - The view is a live, readable projection of an underlying
//     INotifyCollectionChanged source. It maintains an internal snapshot (the
//     items vector) that tracks the source as changes arrive.
//   - It subscribes to the source WEAKLY via IWeakEventListener (the weak-
//     lifetime contract) so the source can be collected while the view lives;
//     the subscription route is the DECISION-2 weak bridge.
//   - It re-emits the live signals to its OWN consumers: CollectionChanged
//     (the delta an ItemsControl applies incrementally) + PropertyChanged
//     ("Count" / current-item axis) so a consumer re-reads.
//
// DECISION-2 (the human-gated ADR-018 renegotiation) — the change log + the
// two-mode drain, all of which 11n-4 OWNS in-library:
//   - The change log is TWO deques: `filling_` (collects pending changes) and
//     `draining_` (the pass being processed). Filling collects; draining
//     processes. On opt-in, the fill->drain swap happens under the ONE
//     permitted mutex (`swap_mutex_`, WPF `_changeLogLock`); draining itself
//     runs OUTSIDE the lock.
//   - `AllowsCrossThreadChanges` is a per-view OPT-IN, defaulting to FALSE.
//     Default false: cross-thread producers are REJECTED at the source-change
//     boundary (a WPF-style NotSupportedException throw-guard), NOT a queue
//     and NOT a lock.
//   - DRAIN MODE 1 (default, !AllowsCrossThreadChanges): DRAIN WHAT YOU FILL,
//     synchronously. The fill and the drain are the same call — when the
//     source fires on the affinity thread, the view processes the change
//     immediately (the sync branch). No deque, no lock. If a drain exceeds the
//     time budget the overflow is deferred to a later drain pass (harvested by
//     the app's pump via drain_one_pass()); it is still drained-what-it-filled.
//   - DRAIN MODE 2 (opt-in): BUDGET-DRAIN WITH RE-QUEUE. Producers fill the
//     filling deque; the fill->drain swap (under the mutex) isolates a draining
//     deque; drain_one_pass() drains it on the time budget; overflow is
//     re-queued back onto the filling deque for a later harvest. Two deques,
//     one swap mutex, one app-pumped budget-drain.
//   - NO WORK IS DEFERRED TO 11z. 11n-4 OWNS the drain code here
//     (`drain_one_pass`'s budgeted drain + the re-queue). `drain_one_pass()`
//     is the in-library drain an app / 11z pump calls to pull ONE pass and
//     harvest deferred work — the 11z harvest API SURFACE is 11z's step, this
//     in-library drain is 11n-4's.
//
// ADR-018 / DECISION-2 verdict: single-threaded UI paths (default) have ZERO
// locks; the ONE permitted lock is the opt-in fill->drain swap mutex, exactly
// where WPF itself locks. Cross-thread background updates are a STRUCTURED
// HANDOFF — never direct collection mutation.

#include "vaml/core/DispatcherObject.h"   // the affinity-thread base
#include "vaml/core/ICollectionView.h"    // the interface + read/navigation contract
#include "vaml/core/IWeakEventListener.h" // the weak subscription contract
#include "vaml/core/INotifyCollectionChanged.h"
#include "vaml/core/INotifyPropertyChanged.h"
#include "vaml/core/ObservableCollection.h" // the concrete source (for the initial snapshot)
#include "vaml/core/WeakEventListener.h"  // the weak bridge
#include "vaml/core/export.h"

#include <cstddef>
#include <deque>
#include <memory>
#include <mutex>
#include <vector>

namespace vaml {

// The WPF-faithful default-mode cross-thread rejection (WPF throws
// NotSupportedException when a producer mutates a collection on a non-affinity
// thread while AllowsCrossThreadChanges is FALSE). CLASS B / observation never
// control-flow — but here the throw IS the WPF-faithful guard at the boundary
// (a DECISION-2 throw-guard, NOT a queue and NOT a lock). It is deliberately a
// hard signal so a cross-thread producer knows to opt-in and use the structured
// handoff. Carries the offending thread's affinity vs the view's.
class VAML_CORE_EXPORT CrossThreadChangeRejected : public std::logic_error
{
public:
    explicit CrossThreadChangeRejected(const char* const message) : std::logic_error(message) {}
};

// The live collection read model. WPF-faithful + DECISION-2 change-log (the
// two deques + the two-mode budget drain owned here). See the class comment.
class VAML_CORE_EXPORT CollectionView final
    : public DispatcherObject
    , public ICollectionView
    , public INotifyPropertyChanged
    , public IWeakEventListener
    , public std::enable_shared_from_this<CollectionView>
{
public:
    // Creates the view over a source and subscribes to it WEAKLY (the
    // DECISION-2 weak bridge). Snapshots the source's current items into the
    // view's live read model, then tracks it as the source mutates. Takes a
    // NON-OWNING reference (the caller owns the collection; the view must not
    // outlive it). Returns a shared_ptr so the weak subscription has a genuine
    // shared owner: the source's trampoline holds a weak_ptr, so the view can
    // be collected while the source lives. The app owns the returned shared_ptr
    // as long as it wants reactivity.
    [[nodiscard]] static std::shared_ptr<CollectionView> Create(ObservableCollectionBase& source);

    ~CollectionView() override;

    CollectionView(const CollectionView&) = delete;
    CollectionView& operator=(const CollectionView&) = delete;

    // --- DECISION-2 opt-in surface ----------------------------------------
    // Sets whether cross-thread producers may FILL the filling deque. Default
    // false (cross-thread producers rejected at the boundary). Opt-in true
    // allows them to fill; the APP owns its pump + calls drain_one_pass() to
    // harvest (11z seam). Setting it disables/enables the throw-guard.
    void set_allows_cross_thread_changes(bool allowed) noexcept { allowsCrossThread_ = allowed; }
    [[nodiscard]] bool allows_cross_thread_changes() const noexcept { return allowsCrossThread_; }

    // The 11n-4-owned app-pulled harvest: pulls ONE drain pass and processes
    // the pending change log on the time budget, re-queueing any overflow. In
    // the DEFAULT (sync) mode this is a no-op when the log is empty (fill==drain
    // already happened inline). In the OPT-IN mode this is how the app / 11z
    // pump harvests deferred work. Returns the number of changes processed.
    [[nodiscard]] std::size_t drain_one_pass();

    // --- ICollectionView (read API) ---------------------------------------
    [[nodiscard]] std::size_t Count() const override;
    [[nodiscard]] bool Contains(const Value& item) const override;
    [[nodiscard]] Value GetItemAt(std::size_t index) const override;
    [[nodiscard]] std::size_t IndexOf(const Value& item) const override;

    // --- ICollectionView (current-item navigation) ------------------------
    [[nodiscard]] Value CurrentItem() const override;
    [[nodiscard]] std::size_t CurrentPosition() const override;
    bool MoveCurrentTo(const Value& item) override;
    bool MoveCurrentToPosition(std::size_t position) override;
    bool MoveCurrentToFirst() override;
    bool MoveCurrentToLast() override;
    bool MoveCurrentToNext() override;
    bool MoveCurrentToPrevious() override;

    // --- ICollectionView (refresh) ----------------------------------------
    void Refresh() override;

    // --- ICollectionView (live subscription) ------------------------------
    [[nodiscard]] CollectionChangedToken add_collection_changed(CollectionChangedHandler handler) override;
    bool remove_collection_changed(CollectionChangedToken token) override;

    // --- INotifyPropertyChanged (Count/current-item axis) -----------------
    using PropertyChangedHandler = INotifyPropertyChanged::PropertyChangedHandler;
    using PropertyChangedToken   = INotifyPropertyChanged::PropertyChangedToken;
    [[nodiscard]] PropertyChangedToken add_property_changed(PropertyChangedHandler handler) override;
    [[nodiscard]] PropertyChangedToken add_property_changed(PropertyId id, PropertyChangedHandler handler) override;
    void remove_property_changed(PropertyChangedToken token) override;

    // --- IWeakEventListener (the source-change entry point) ---------------
    // The source's CollectionChanged arrives here (through the DECISION-2 weak
    // bridge). `managerTag` is CollectionChanged; `args` is the
    // NotifyCollectionChangedEventArgs delta. Returns true when handled.
    bool ReceiveWeakEvent(const WeakEventManagerTag& managerTag,
                          const void* sender,
                          const Value& args) override;

    // --- Unit-test seam (the "sync branch" — default-mode drain). ---------
    // Returns true when this view was created on the affinity thread (the only
    // thread that may fill+drain synchronously in default mode). The owner/
    // test asserts it before calling drain_one_pass on the default path.
    [[nodiscard]] bool affinity_ok() const noexcept { return check_access(); }

private:
    // Private ctor (Create() factory — enables shared_from_this).
    explicit CollectionView(ObservableCollectionBase& source);

    // Applies ONE source change to the internal snapshot + subscribers. Used
    // by BOTH the sync branch (default mode) and the budgeted drain (opt-in).
    void process_change(CollectionChangedAction action, std::size_t newIndex, Value newItem,
                        std::size_t oldIndex, Value oldItem);

    // The single source-change entry point: in DEFAULT mode throws the
    // CrossThreadChangeRejected guard when not on the affinity thread; on the
    // affinity thread it fills+drains synchronously (fill==drain). In OPT-IN
    // mode it fills the filling deque (from any thread) and returns.
    void on_source_change(const NotifyCollectionChangedEventArgs& e);

    // The fill->drain swap (opt-in mode, under the ONE permitted mutex).
    void swap_change_logs();

    // Processes the draining deque on a time budget (drain_one_pass body).
    std::size_t drain_impl();

    // Raises this view's CollectionChanged + PropertyChanged("Count") to its
    // OWN consumers after items_ mutates (the re-emission a consumer re-reads).
    void notify_own_change(const NotifyCollectionChangedEventArgs& e);

    // Internal snapshot (the live projection). Read through Count/GetItemAt/
    // IndexOf/Contains.
    std::vector<Value> items_;
    std::size_t        currentPos_ = 0;

    // The source (not owned). The subscription trampoline (in Create) captures
    // a std::weak_ptr<CollectionView> — the weak-lifetime bridge — so the view
    // can be collected while the source lives (no strong ref keeps it alive).
    ObservableCollectionBase* source_ = nullptr;
    CollectionChangedToken sourceToken_ = 0;
    bool sourceSubscribed_ = false;

    // DECISION-2 change-log: two deques + the ONE permitted swap mutex.
    std::deque<NotifyCollectionChangedEventArgs> filling_;
    std::deque<NotifyCollectionChangedEventArgs> draining_;
    mutable std::mutex swapMutex_;  // ONLY the fill->drain swap in opt-in mode
    bool allowsCrossThread_ = false;

    // This view's own subscriber lists (the re-emission to consumers).
    std::vector<std::pair<CollectionChangedToken, CollectionChangedHandler>> ownChanged_;
    CollectionChangedToken nextOwn_ = 1;
    // The PropertyChanged axis is a broadcast + per-property list backed by a
    // lightweight sub list (INotifyPropertyChanged contract).
    struct Sub { PropertyId id; PropertyChangedToken token; PropertyChangedHandler handler; };
    std::vector<Sub> propSubs_;
    PropertyChangedToken nextPropToken_ = 1;
};

} // namespace vaml
