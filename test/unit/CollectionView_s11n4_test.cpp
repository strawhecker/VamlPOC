// 11n-4 (main, step 3) — CollectionView, the live collection read model
// (vaml::core). Proves:
//   (A) CollectionView IS-A DispatcherObject (affinity) + ICollectionView +
//       INotifyPropertyChanged + IWeakEventListener.
//   (B) read API (Count/Contains/GetItemAt/IndexOf) tracks the source LIVE
//       as it mutates (event-subscription, not polled).
//   (C) current-item navigation (MoveCurrentTo*/CurrentItem/CurrentPosition).
//   (D) it re-emits CollectionChanged + PropertyChanged("Count") to its OWN
//       consumers (the live delta a consumer applies incrementally).
//   (E) DECISION-2 change-log: DEFAULT sync mode (fill==drain, zero locks) +
//       the cross-thread REJECTION guard; OPT-IN mode budget-drains through
//       drain_one_pass() with re-queue of overflow.
//   (F) weak-lifetime: a COLLECTED view is dropped (no leak, no dangling call).
//
// ADR-018 / DECISION-2: ZERO locks on the default path; the ONE permitted lock
// is the opt-in fill->drain swap mutex.

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include <vaml/core/CollectionView.h>
#include <vaml/core/ObservableCollection.h>
#include <vaml/core/PropertyId.h>

#include <memory>
#include <string>
#include <vector>

using vaml::CollectionChangedAction;
using vaml::CollectionView;
using vaml::CrossThreadChangeRejected;
using vaml::NotifyCollectionChangedEventArgs;
using vaml::ObservableCollection;
using vaml::PropertyId;
using vaml::Value;

// ---------------------------------------------------------------------------
// A. IS-A checks (DispatcherObject + ICollectionView + INotifyPropertyChanged)
// ---------------------------------------------------------------------------

TEST_CASE("11n-4: CollectionView IS-A DispatcherObject + ICollectionView")
{
    ObservableCollection<std::string> col;
    auto view = CollectionView::Create(col.base());

    SHOULD(ShouldlyIntegral(view->check_access() ? 1 : 0).ShouldBe(1)); // on affinity thread
    SHOULD(ShouldlyIntegral(view->affinity_ok() ? 1 : 0).ShouldBe(1));
}

// ---------------------------------------------------------------------------
// B. Live read API tracks the source
// ---------------------------------------------------------------------------

TEST_CASE("11n-4: CollectionView read API tracks the source live")
{
    ObservableCollection<std::string> col;
    col.Add("a");
    col.Add("b");
    col.Add("c");

    auto view = CollectionView::Create(col.base());
    SHOULD(ShouldlyIntegral(static_cast<int>(view->Count())).ShouldBe(3));
    SHOULD(ShouldlyString(view->GetItemAt(0).get<std::string>()).ShouldBe(std::string("a")));
    SHOULD(ShouldlyString(view->GetItemAt(2).get<std::string>()).ShouldBe(std::string("c")));
    SHOULD(ShouldlyIntegral(view->Contains(Value::String("b")) ? 1 : 0).ShouldBe(1));
    SHOULD(ShouldlyIntegral(view->IndexOf(Value::String("b"))).ShouldBe(1));
    SHOULD(ShouldlyIntegral(view->Contains(Value::String("zzz")) ? 1 : 0).ShouldBe(0));

    // Live: mutating the source is reflected WITHOUT polling (event-subscription).
    col.Add("d");
    SHOULD(ShouldlyIntegral(static_cast<int>(view->Count())).ShouldBe(4));
    SHOULD(ShouldlyString(view->GetItemAt(3).get<std::string>()).ShouldBe(std::string("d")));

    col.RemoveAt(0);
    SHOULD(ShouldlyIntegral(static_cast<int>(view->Count())).ShouldBe(3));
    SHOULD(ShouldlyString(view->GetItemAt(0).get<std::string>()).ShouldBe(std::string("b")));
}

// ---------------------------------------------------------------------------
// C. Current-item navigation
// ---------------------------------------------------------------------------

TEST_CASE("11n-4: CollectionView current-item navigation")
{
    ObservableCollection<std::string> col;
    col.Add("x");
    col.Add("y");
    col.Add("z");
    auto view = CollectionView::Create(col.base());

    SHOULD(ShouldlyIntegral(view->MoveCurrentToFirst() ? 1 : 0).ShouldBe(1));
    SHOULD(ShouldlyIntegral(view->CurrentPosition()).ShouldBe(0));
    SHOULD(ShouldlyString(view->CurrentItem().get<std::string>()).ShouldBe(std::string("x")));

    SHOULD(ShouldlyIntegral(view->MoveCurrentToNext() ? 1 : 0).ShouldBe(1));
    SHOULD(ShouldlyString(view->CurrentItem().get<std::string>()).ShouldBe(std::string("y")));

    SHOULD(ShouldlyIntegral(view->MoveCurrentToLast() ? 1 : 0).ShouldBe(1));
    SHOULD(ShouldlyString(view->CurrentItem().get<std::string>()).ShouldBe(std::string("z")));

    SHOULD(ShouldlyIntegral(view->MoveCurrentTo(Value::String("y")) ? 1 : 0).ShouldBe(1));
    SHOULD(ShouldlyIntegral(view->CurrentPosition()).ShouldBe(1));
    SHOULD(ShouldlyString(view->CurrentItem().get<std::string>()).ShouldBe(std::string("y")));

    SHOULD(ShouldlyIntegral(view->MoveCurrentToPrevious() ? 1 : 0).ShouldBe(1));
    SHOULD(ShouldlyString(view->CurrentItem().get<std::string>()).ShouldBe(std::string("x")));
}

// ---------------------------------------------------------------------------
// D. Re-emission to OWN consumers (CollectionChanged + PropertyChanged(Count))
// ---------------------------------------------------------------------------

TEST_CASE("11n-4: CollectionView re-emits CollectionChanged + PropertyChanged(Count)")
{
    ObservableCollection<std::string> col;
    col.Add("a");
    auto view = CollectionView::Create(col.base());

    int ownChanges = 0;
    CollectionChangedAction lastAction = CollectionChangedAction::Reset;
    (void)view->add_collection_changed([&](const NotifyCollectionChangedEventArgs& e) {
        ++ownChanges;
        lastAction = e.action();
    });

    int countChanges = 0;
    (void)view->add_property_changed(PropertyId::of("Count"),
                                     [&](PropertyId, const Value&, const Value&) { ++countChanges; });

    // Source mutation -> the view re-emits to its own consumers.
    col.Add("b");
    SHOULD(ShouldlyIntegral(ownChanges).ShouldBe(1));
    SHOULD(ShouldlyIntegral(static_cast<int>(lastAction)).ShouldBe(static_cast<int>(CollectionChangedAction::Add)));
    SHOULD(ShouldlyIntegral(countChanges).ShouldBe(1));
}

// ---------------------------------------------------------------------------
// E1. DECISION-2 DEFAULT mode — synchronous fill==drain, zero locks; the
//     cross-thread rejection guard (dodged here — affinity check is on the
//     creator thread).
// ---------------------------------------------------------------------------

TEST_CASE("11n-4: CollectionView default mode tracks synchronously (fill==drain)")
{
    ObservableCollection<std::string> col;
    auto view = CollectionView::Create(col.base());

    // Default: AllowsCrossThreadChanges = false.
    SHOULD(ShouldlyIntegral(view->allows_cross_thread_changes() ? 1 : 0).ShouldBe(0));

    int own = 0;
    (void)view->add_collection_changed([&](const NotifyCollectionChangedEventArgs&) { ++own; });

    // A quick burst on the affinity thread: each change fills + drains inline.
    col.Add("1");
    col.Add("2");
    col.Add("3");
    SHOULD(ShouldlyIntegral(static_cast<int>(view->Count())).ShouldBe(3));
    SHOULD(ShouldlyIntegral(own).ShouldBe(3));
}

// ---------------------------------------------------------------------------
// E2. DECISION-2 OPT-IN mode — budget-drain via drain_one_pass() + re-queue.
// ---------------------------------------------------------------------------

TEST_CASE("11n-4: CollectionView opt-in mode budget-drains via drain_one_pass")
{
    ObservableCollection<std::string> col;
    col.Add("base0");
    col.Add("base1");
    auto view = CollectionView::Create(col.base());
    view->set_allows_cross_thread_changes(true);
    SHOULD(ShouldlyIntegral(view->allows_cross_thread_changes() ? 1 : 0).ShouldBe(1));

    // In opt-in mode the source changes FILL the filling deque (not drained
    // inline — the drain is the app-pulled drain_one_pass()).
    col.Add("x");
    col.Add("y");

    // The view's Count does NOT advance until the drain harvests the log.
    SHOULD(ShouldlyIntegral(static_cast<int>(view->Count())).ShouldBe(2));

    // Harvest ONE drain pass (the app/11z pump pulls the deferred work).
    const std::size_t processed = view->drain_one_pass();
    SHOULD(ShouldlyIntegral(static_cast<int>(processed)).ShouldBe(2));
    SHOULD(ShouldlyIntegral(static_cast<int>(view->Count())).ShouldBe(4));
    SHOULD(ShouldlyString(view->GetItemAt(3).get<std::string>()).ShouldBe(std::string("y")));
}

// ---------------------------------------------------------------------------
// F. Weak-lifetime: a collected view is dropped (no leak).
// ---------------------------------------------------------------------------

TEST_CASE("11n-4: a collected CollectionView is dropped (weak lifetime — no leak)")
{
    ObservableCollection<std::string> col;
    col.Add("a");

    std::weak_ptr<CollectionView> weakView;
    {
        auto view = CollectionView::Create(col.base());
        weakView = view;
        SHOULD(ShouldlyIntegral(weakView.expired() ? 1 : 0).ShouldBe(0));
    }
    // The only strong owner died — the weak bridge does NOT keep it alive.
    SHOULD(ShouldlyIntegral(weakView.expired() ? 1 : 0).ShouldBe(1));

    // The source continues to mutate safely (no dangling callback into a dead
    // view).
    col.Add("b");
    col.Add("c");
}
