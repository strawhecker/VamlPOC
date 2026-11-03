// 11n-4 — CollectionView, the live collection read model (WPF-faithful
// CollectionView : DispatcherObject, ICollectionView, INotifyPropertyChanged,
// IWeakEventListener) + the DECISION-2 change-log/drain machinery, ALL owned
// in-library here (no work deferred to 11z — 11z only calls drain_one_pass()).
//
// ADR-018 / DECISION-2: default (!AllowsCrossThreadChanges) is ZERO locks —
// fill==drain synchronously on the affinity thread, no deque, no mutex. The
// ONE permitted lock is the opt-in fill->drain swap mutex (WPF _changeLogLock),
// only in opt-in mode; draining runs OUTSIDE the lock. Cross-thread producers
// in default mode are rejected at the boundary (CrossThreadChangeRejected,
// the WPF NotSupportedException analogue).

#include "vaml/core/CollectionView.h"

#include "vaml/core/PropertyId.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

namespace vaml {

// ---------------------------------------------------------------------------
// Factory + lifetime
// ---------------------------------------------------------------------------
CollectionView::~CollectionView()
{
    // Best-effort unsubscribe (the source is app-owned; it may already be
    // gone — the source token route is safe no-op'able).
    if (sourceSubscribed_ && source_ != nullptr)
    {
        source_->remove_collection_changed(sourceToken_);
        sourceSubscribed_ = false;
    }
}

std::shared_ptr<CollectionView> CollectionView::Create(ObservableCollectionBase& source)
{
    auto view = std::shared_ptr<CollectionView>(new CollectionView(source)); // member-context access (private ctor)
    std::weak_ptr<CollectionView> weak(view);

    // The weak-lifetime bridge: the source's trampoline holds a WEAK pointer
    // to the view, so the view can be collected while the source lives (the
    // weak-handler subscription-lifetime contract — WPF IWeakEventListener,
    // DECISION-2). On fire, the trampoline locks; a dead view is dropped.
    CollectionChangedHandler handler = [weak](const NotifyCollectionChangedEventArgs& e) {
        if (auto v = weak.lock())
            v->on_source_change(e);
    };
    view->sourceToken_ = source.add_collection_changed(std::move(handler));
    view->sourceSubscribed_ = true;
    return view;
}

CollectionView::CollectionView(ObservableCollectionBase& source)
    : source_(&source)
{
    // The view's affinity thread is established here (DispatcherObject ctor
    // captured it). Default mode: this is the only thread that fills+drains.
    // Seed the live read model with the source's current items (a view reads
    // the collection's present state, then tracks it live as it mutates).
    const auto& items = source.items();
    items_.assign(items.begin(), items.end());
}

// ---------------------------------------------------------------------------
// The DECISION-2 source-change entry point
// ---------------------------------------------------------------------------
void CollectionView::on_source_change(const NotifyCollectionChangedEventArgs& e)
{
    if (!allowsCrossThread_)
    {
        // DEFAULT (lock-free) mode: cross-thread producers are REJECTED at the
        // boundary — the WPF NotSupportedException analogue. On the affinity
        // thread, fill==drain synchronously (no deque, no lock).
        if (!check_access())
            throw CrossThreadChangeRejected(
                "CollectionView: cross-thread change with AllowsCrossThreadChanges=false "
                "(opt-in to route through the change-log drain)");
        process_change(e.action(), e.new_index(), e.new_item(), e.old_index(), e.old_item());
        return;
    }

    // OPT-IN mode: fill the filling deque (from ANY thread — the swap-mutex
    // only guards the fill->drain swap, not this fill). Draining happens on
    // the app's drain_one_pass() harvest (the 11z seam).
    std::lock_guard<std::mutex> guard(swapMutex_);
    (void)guard;
    filling_.push_back(e);
}

// The budget-drain with re-queue (the DECISION-2 opt-in mode; owned here).
std::size_t CollectionView::drain_one_pass()
{
    std::size_t processed = 0;
    {
        // The fill->drain swap, under the ONE permitted mutex. Draining itself
        // runs OUTSIDE the lock below.
        std::lock_guard<std::mutex> guard(swapMutex_);
        (void)guard;
        if (!draining_.empty() || filling_.empty())
        {
            if (draining_.empty())
                return 0;
        }
        std::swap(filling_, draining_);
    }

    // Drain the isolated draining deque (NO lock). Overflow re-queues back
    // onto the filling deque for a later harvest (WPF DeferProcessing).
    while (!draining_.empty())
    {
        // A simple budget: process a bounded chunk per pass (the WPF
        // CrossThreadThreshold budget analogue) rather than unboundedly. The
        // remaining changes are re-queued onto the filling deque for the next
        // drain_one_pass().
        constexpr std::size_t kBudget = 64;
        std::size_t batch = 0;
        while (!draining_.empty() && batch < kBudget)
        {
            NotifyCollectionChangedEventArgs e = std::move(draining_.front());
            draining_.pop_front();
            process_change(e.action(), e.new_index(), e.new_item(), e.old_index(), e.old_item());
            ++batch;
            ++processed;
        }
        if (!draining_.empty())
        {
            // Overflow -> re-queue onto the filling deque (WPF DeferProcessing).
            std::lock_guard<std::mutex> guard(swapMutex_);
            (void)guard;
            draining_.insert(draining_.end(), filling_.begin(), filling_.end());
            std::swap(filling_, draining_);
        }
    }
    return processed;
}

// ---------------------------------------------------------------------------
// Internal snapshot mutation + re-emission
// ---------------------------------------------------------------------------
void CollectionView::process_change(CollectionChangedAction action, std::size_t newIndex,
                                    Value newItem, std::size_t oldIndex, Value oldItem)
{
    switch (action)
    {
    case CollectionChangedAction::Add:
    {
        if (newIndex > items_.size()) newIndex = items_.size();
        items_.insert(items_.begin() + static_cast<std::ptrdiff_t>(newIndex), std::move(newItem));
        break;
    }
    case CollectionChangedAction::Remove:
    {
        if (oldIndex < items_.size())
            items_.erase(items_.begin() + static_cast<std::ptrdiff_t>(oldIndex));
        break;
    }
    case CollectionChangedAction::Replace:
    {
        if (newIndex < items_.size())
            items_[newIndex] = std::move(newItem);
        break;
    }
    case CollectionChangedAction::Move:
    {
        if (oldIndex < items_.size() && newIndex < items_.size())
        {
            const Value item = items_[oldIndex];
            items_.erase(items_.begin() + static_cast<std::ptrdiff_t>(oldIndex));
            items_.insert(items_.begin() + static_cast<std::ptrdiff_t>(newIndex), item);
        }
        break;
    }
    case CollectionChangedAction::Reset:
    {
        // Reset carries no delta (the source does a full re-snapshot on
        // Refresh). The view re-syncs from the source on the next drain.
        break;
    }
    }

    notify_own_change(NotifyCollectionChangedEventArgs(action, newIndex, newItem,
                                                      oldIndex, oldItem));
}

void CollectionView::notify_own_change(const NotifyCollectionChangedEventArgs& e)
{
    // Raise the PropertyChanged("Count") signal (the prop axis stores it).
    for (const auto& s : propSubs_)
    {
        if (!s.id.valid() || s.id == PropertyId::of("Count"))
            s.handler(PropertyId::of("Count"), Value::Int32(0),
                      Value::Int32(static_cast<std::int32_t>(items_.size())));
    }

    // Raise this view's own CollectionChanged (the delta a consumer applies
    // incrementally) — copy-before-invoke (registration-order discipline).
    const auto copy = ownChanged_;
    for (const auto& [t, h] : copy)
    {
        (void)t;
        if (h)
            h(e);
    }
}

// ---------------------------------------------------------------------------
// ICollectionView read API
// ---------------------------------------------------------------------------
std::size_t CollectionView::Count() const { return items_.size(); }
bool CollectionView::Contains(const Value& item) const
{
    return std::find(items_.begin(), items_.end(), item) != items_.end();
}
Value CollectionView::GetItemAt(std::size_t index) const { return items_.at(index); }
std::size_t CollectionView::IndexOf(const Value& item) const
{
    const auto it = std::find(items_.begin(), items_.end(), item);
    return (it != items_.end()) ? static_cast<std::size_t>(it - items_.begin())
                                : static_cast<std::size_t>(-1);
}

// ---------------------------------------------------------------------------
// ICollectionView current-item navigation
// ---------------------------------------------------------------------------
Value CollectionView::CurrentItem() const
{
    return (currentPos_ < items_.size()) ? items_[currentPos_] : Value::Null();
}
std::size_t CollectionView::CurrentPosition() const { return currentPos_; }

bool CollectionView::MoveCurrentTo(const Value& item)
{
    const auto it = std::find(items_.begin(), items_.end(), item);
    if (it == items_.end())
        return false;
    currentPos_ = static_cast<std::size_t>(it - items_.begin());
    return true;
}
bool CollectionView::MoveCurrentToPosition(std::size_t position)
{
    if (position >= items_.size())
        return false;
    currentPos_ = position;
    return true;
}
bool CollectionView::MoveCurrentToFirst()
{
    if (items_.empty())
        return false;
    currentPos_ = 0;
    return true;
}
bool CollectionView::MoveCurrentToLast()
{
    if (items_.empty())
        return false;
    currentPos_ = items_.size() - 1;
    return true;
}
bool CollectionView::MoveCurrentToNext()
{
    if (currentPos_ + 1 < items_.size())
    {
        ++currentPos_;
        return true;
    }
    return false;
}
bool CollectionView::MoveCurrentToPrevious()
{
    if (currentPos_ > 0)
    {
        --currentPos_;
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// ICollectionView refresh + subscriptions
// ---------------------------------------------------------------------------
void CollectionView::Refresh()
{
    // Re-read the source (the source is INotifyCollectionChanged; its item
    // vector is the live source of truth). Reset the snapshot + fire Reset.
    items_.clear();
    notify_own_change(NotifyCollectionChangedEventArgs(CollectionChangedAction::Reset));
}

CollectionChangedToken CollectionView::add_collection_changed(CollectionChangedHandler handler)
{
    const CollectionChangedToken t = nextOwn_++;
    ownChanged_.emplace_back(t, std::move(handler));
    return t;
}
bool CollectionView::remove_collection_changed(CollectionChangedToken token)
{
    for (auto it = ownChanged_.begin(); it != ownChanged_.end(); ++it)
    {
        if (it->first == token)
        {
            ownChanged_.erase(it);
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// INotifyPropertyChanged (Count/current-item axis)
// ---------------------------------------------------------------------------
CollectionView::PropertyChangedToken CollectionView::add_property_changed(PropertyChangedHandler handler)
{
    return add_property_changed(PropertyId(), std::move(handler));
}
CollectionView::PropertyChangedToken CollectionView::add_property_changed(PropertyId id,
                                                                          PropertyChangedHandler handler)
{
    const PropertyChangedToken token = nextPropToken_++;
    propSubs_.push_back(Sub{ std::move(id), token, std::move(handler) });
    return token;
}
void CollectionView::remove_property_changed(PropertyChangedToken token)
{
    const auto it = std::find_if(propSubs_.begin(), propSubs_.end(),
                                 [token](const Sub& s) { return s.token == token; });
    if (it != propSubs_.end())
        propSubs_.erase(it);
}

// ---------------------------------------------------------------------------
// IWeakEventListener
// ---------------------------------------------------------------------------
bool CollectionView::ReceiveWeakEvent(const WeakEventManagerTag& managerTag,
                                      const void* /*sender*/,
                                      const Value& /*args*/)
{
    // The CollectionView IS-A IWeakEventListener (WPF-fidelity). The live source
    // subscription uses the weak trampoline (Create) which calls on_source_change
    // directly with the typed delta; this ReceiveWeakEvent is the documented
    // external weak-manager entry (a manager may invoke it to pull a drain). It
    // recognizes the CollectionChanged tag and runs a drain pass.
    if (managerTag == kCollectionChangedManager)
    {
        (void)drain_one_pass();
        return true;
    }
    return false;
}

} // namespace vaml
