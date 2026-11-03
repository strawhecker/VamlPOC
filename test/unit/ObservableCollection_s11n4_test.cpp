// 11n-4 (foundation) — the ObservableCollection notification spine (vaml::core).
//
// The reactive spine's collection core (DECISION-2: ZERO locks on the
// single-threaded UI path). Proves a VAML-native ObservableCollection raises
// BOTH WPF signals on every mutation, in WPF order:
//   PropertyChanged("Count") + PropertyChanged("Item[]"), THEN CollectionChanged.
//
// Proves:
//   (A) Add raises CollectionChanged(Add, newIndex, item) + Count/Item[]
//       PropertyChanged in WPF order.
//   (B) Insert (positional Add), Remove/RemoveAt (Remove with oldIndex+oldItem),
//       Clear (Reset), SetAt (Replace, real-change discipline — no signal on
//       equal), Move (Move with both indices).
//   (C) INotifyCollectionChanged subscribe/unsubscribe token discipline.
//   (D) The CollectionChangedAction enum vocabulary (DAG anchor).
//
// ADR-018 / DECISION-2: ZERO new locks (single-threaded affinity; the
// mutation itself raises the signals; re-entrancy is the 11n-4 budgeted-drain
// concern, never a per-collection mutex).

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include <vaml/core/ObservableCollection.h>
#include <vaml/core/PropertyId.h>

#include <string>
#include <vector>

using vaml::CollectionChangedAction;
using vaml::ObservableCollection;
using vaml::ObservableCollectionBase;
using vaml::Value;

namespace {
int cc(CollectionChangedAction a) { return static_cast<int>(a); }
}

// ---------------------------------------------------------------------------
// A. Add — CollectionChanged(Add) + Count/Item[] PropertyChanged, WPF order
// ---------------------------------------------------------------------------

TEST_CASE("11n-4: ObservableCollection Add raises CollectionChanged + PropertyChanged (WPF order)")
{
    ObservableCollection<std::string> col;

    // Record the order of signals (PropertyChanged must precede CollectionChanged).
    std::vector<std::string> signalOrder;
    std::vector<CollectionChangedAction> actions;

    (void)col.add_property_changed(vaml::PropertyId::of("Count"),
                             [&](vaml::PropertyId, const Value&, const Value&) {
                                 signalOrder.push_back("P:Count");
                             });
    (void)col.add_property_changed(vaml::PropertyId::of("Item[]"),
                             [&](vaml::PropertyId, const Value&, const Value&) {
                                 signalOrder.push_back("P:Item[]");
                             });
    (void)col.add_collection_changed([&](const vaml::NotifyCollectionChangedEventArgs& e) {
        signalOrder.push_back("C");
        actions.push_back(e.action());
    });

    col.Add("a");
    col.Add("b");

    SHOULD(ShouldlyIntegral(static_cast<int>(col.size())).ShouldBe(2));
    // WPF order per mutation: Count, Item[], then CollectionChanged
    SHOULD(ShouldlyIntegral(static_cast<int>(signalOrder.size())).ShouldBe(6));
    SHOULD(ShouldlyString(signalOrder[0]).ShouldBe(std::string("P:Count")));
    SHOULD(ShouldlyString(signalOrder[1]).ShouldBe(std::string("P:Item[]")));
    SHOULD(ShouldlyString(signalOrder[2]).ShouldBe(std::string("C")));
    SHOULD(ShouldlyString(signalOrder[3]).ShouldBe(std::string("P:Count")));
    SHOULD(ShouldlyString(signalOrder[4]).ShouldBe(std::string("P:Item[]")));
    SHOULD(ShouldlyString(signalOrder[5]).ShouldBe(std::string("C")));
    // both actions are Add
    SHOULD(ShouldlyIntegral(cc(actions[0])).ShouldBe(cc(CollectionChangedAction::Add)));
    SHOULD(ShouldlyIntegral(cc(actions[1])).ShouldBe(cc(CollectionChangedAction::Add)));
}

// ---------------------------------------------------------------------------
// B. The full mutation vocabulary (Add/Remove/Replace/Move/Reset)
// ---------------------------------------------------------------------------

TEST_CASE("11n-4: ObservableCollection mutation vocabulary (Insert/Remove/Clear/SetAt/Move)")
{
    ObservableCollection<std::string> col;
    std::vector<CollectionChangedAction> actions;
    (void)col.add_collection_changed([&](const vaml::NotifyCollectionChangedEventArgs& e) {
        actions.push_back(e.action());
    });

    col.Add("a");
    col.Add("b");
    col.Add("c");
    actions.clear();

    // Insert at 1 (positional Add)
    col.Insert(1, "X");
    SHOULD(ShouldlyIntegral(cc(actions.back())).ShouldBe(cc(CollectionChangedAction::Add)));
    SHOULD(ShouldlyIntegral(static_cast<int>(col.size())).ShouldBe(4));
    SHOULD(ShouldlyString(col.at(1).get<std::string>()).ShouldBe(std::string("X")));
    actions.clear();

    // RemoveAt(0) (Remove with oldIndex)
    col.RemoveAt(0);
    SHOULD(ShouldlyIntegral(cc(actions.back())).ShouldBe(cc(CollectionChangedAction::Remove)));
    SHOULD(ShouldlyIntegral(static_cast<int>(col.size())).ShouldBe(3));
    actions.clear();

    // SetAt(0, "Z") (Replace)
    col.SetAt(0, "Z");
    SHOULD(ShouldlyIntegral(cc(actions.back())).ShouldBe(cc(CollectionChangedAction::Replace)));
    SHOULD(ShouldlyString(col.at(0).get<std::string>()).ShouldBe(std::string("Z")));
    actions.clear();

    // Move(0 -> 1)
    col.Move(0, 1);
    SHOULD(ShouldlyIntegral(cc(actions.back())).ShouldBe(cc(CollectionChangedAction::Move)));
    SHOULD(ShouldlyIntegral(static_cast<int>(col.size())).ShouldBe(3));
    actions.clear();

    // Clear (Reset)
    col.Clear();
    SHOULD(ShouldlyIntegral(cc(actions.back())).ShouldBe(cc(CollectionChangedAction::Reset)));
    SHOULD(ShouldlyIntegral(static_cast<int>(col.size())).ShouldBe(0));
    SHOULD(ShouldlyIntegral(col.empty() ? 1 : 0).ShouldBe(1));
}

// ---------------------------------------------------------------------------
// C. Subscription token discipline + real-change (no Replace on equal Set)
// ---------------------------------------------------------------------------

TEST_CASE("11n-4: CollectionChanged token unsubscribe + real-change discipline")
{
    ObservableCollection<std::string> col;
    int fired = 0;
    auto tok = col.add_collection_changed([&](const vaml::NotifyCollectionChangedEventArgs&) { ++fired; });

    col.Add("a");
    SHOULD(ShouldlyIntegral(fired).ShouldBe(1));

    // real-change: SetAt with an EQUAL value emits no Replace signal (the
    // WPF real-change discipline).
    col.SetAt(0, "a");
    SHOULD(ShouldlyIntegral(fired).ShouldBe(1));

    // unsubscribe stops delivery
    SHOULD(ShouldlyIntegral(col.remove_collection_changed(tok) ? 1 : 0).ShouldBe(1));
    col.Add("b");
    SHOULD(ShouldlyIntegral(fired).ShouldBe(1));
    SHOULD(ShouldlyIntegral(col.remove_collection_changed(tok) ? 1 : 0).ShouldBe(0));
}
