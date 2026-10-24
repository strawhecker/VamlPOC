// S247 — RelayCommand tests. Sprint 6 (dep:S246).
//
// Proves the S246 headline against the REAL vaml::RelayCommand (through the
// REAL vaml::ICommand interface where it matters):
//   1. an Execute-only command is always executable (CanExecute true);
//   2. a predicate governs CanExecute per parameter;
//   3. Execute invokes the action with the exact parameter Value;
//   4. subscribe -> NotifyCanExecuteChanged fires once;
//   5. multiple subscribers fire in registration order;
//   6. remove_can_execute_changed removes one; the remaining fire; re-add
//      appends;
//   7. tokens are distinct / monotonic;
//   8. NotifyCanExecuteChanged with zero subscribers = safe no-op;
//   9. CanExecute is side-effect-free (fires nothing, changes nothing);
//  10. Execute with a Value::Null() parameter;
//  11. move ctor/assign transfer the action + predicate + subscriptions
//      (post-move fires; moved-from inert);
//  12. re-query contract: a captured flag flips CanExecute, and a subscribed
//      handler observes via NotifyCanExecuteChanged.
//
// S247 is a pure value-domain suite (no property names registered).

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"       // StringMaker + operator<< for Value
#include <vaml/assert/Shouldly.h>         // ShouldlyString / SHOULD seam

#include "vaml/core/ICommand.h"
#include "vaml/core/RelayCommand.h"
#include "vaml/core/Value.h"

#include <cstdint>
#include <string>
#include <vector>

using vaml::ICommand;
using vaml::RelayCommand;
using vaml::Value;

namespace {

// A recording handler that counts + records invocations.
struct HandlerRec
{
    int count = 0;
    void operator()() { ++count; }
};

} // namespace

TEST_CASE("S247 Execute-only: CanExecute is true by default")
{
    RelayCommand cmd([](const Value&) {});
    SHOULD(ShouldlyString(cmd.CanExecute(Value::Null())).ShouldBe(true));
    SHOULD(ShouldlyString(cmd.CanExecute(Value::Int32(5))).ShouldBe(true));
    // Through the interface too.
    ICommand& base = cmd;
    SHOULD(ShouldlyString(base.CanExecute(Value::Null())).ShouldBe(true));
}

TEST_CASE("S247 predicate: CanExecute reflects the predicate per parameter")
{
    // Only Int32 parameters >= 0 are executable.
    RelayCommand cmd([](const Value&) {},
                     [](const Value& p) { return p.type() == vaml::ValueType::Int32 && p.get<std::int32_t>() >= 0; });
    SHOULD(ShouldlyString(cmd.CanExecute(Value::Int32(3))).ShouldBe(true));
    SHOULD(ShouldlyString(cmd.CanExecute(Value::Int32(-4))).ShouldBe(false));
    SHOULD(ShouldlyString(cmd.CanExecute(Value::Null())).ShouldBe(false));
}

TEST_CASE("S247 Execute: invokes the action with the exact parameter Value")
{
    std::vector<Value> captured;
    RelayCommand cmd([&captured](const Value& p) { captured.push_back(p); });

    cmd.Execute(Value::Int32(42));
    cmd.Execute(Value::String("hi"));

    SHOULD(ShouldlyIntegral(captured.size()).ShouldBe(std::size_t{ 2 }));
    SHOULD(ShouldlyString(captured[0]).ShouldBe(Value::Int32(42)));
    SHOULD(ShouldlyString(captured[1]).ShouldBe(Value::String("hi")));
}

TEST_CASE("S247 subscribe: NotifyCanExecuteChanged fires each handler once")
{
    RelayCommand cmd([](const Value&) {});
    HandlerRec a, b;
    SHOULD(ShouldlyIntegral(cmd.add_can_execute_changed(std::ref(a))).ShouldBe(std::size_t{ 1 }));
    SHOULD(ShouldlyIntegral(cmd.add_can_execute_changed(std::ref(b))).ShouldBe(std::size_t{ 2 }));

    cmd.NotifyCanExecuteChanged();
    cmd.NotifyCanExecuteChanged();

    SHOULD(ShouldlyIntegral(a.count).ShouldBe(2));
    SHOULD(ShouldlyIntegral(b.count).ShouldBe(2));
}

TEST_CASE("S247 multiple subscribers fire in registration order")
{
    RelayCommand cmd([](const Value&) {});
    std::vector<int> order;
    (void)cmd.add_can_execute_changed([&order] { order.push_back(1); });
    (void)cmd.add_can_execute_changed([&order] { order.push_back(2); });
    (void)cmd.add_can_execute_changed([&order] { order.push_back(3); });

    cmd.NotifyCanExecuteChanged();

    SHOULD(ShouldlyIntegral(order.size()).ShouldBe(std::size_t{ 3 }));
    SHOULD(ShouldlyIntegral(order[0]).ShouldBe(1));
    SHOULD(ShouldlyIntegral(order[1]).ShouldBe(2));
    SHOULD(ShouldlyIntegral(order[2]).ShouldBe(3));
}

TEST_CASE("S247 remove: removes one; remaining fire; re-add appends")
{
    RelayCommand cmd([](const Value&) {});
    std::vector<int> order;
    auto t1 = cmd.add_can_execute_changed([&order] { order.push_back(1); });
    auto t2 = cmd.add_can_execute_changed([&order] { order.push_back(2); });
    auto t3 = cmd.add_can_execute_changed([&order] { order.push_back(3); });

    // Remove the middle subscriber; returns true (it was present).
    const bool removedMiddle = cmd.remove_can_execute_changed(t2);
    SHOULD(ShouldlyIntegral(removedMiddle).ShouldBe(true));
    // Removing an unknown/expired token is a false no-op.
    const bool removedUnknown = cmd.remove_can_execute_changed(std::size_t{ 999 });
    SHOULD(ShouldlyIntegral(removedUnknown).ShouldBe(false));
    const bool removedAgain = cmd.remove_can_execute_changed(t2);
    SHOULD(ShouldlyIntegral(removedAgain).ShouldBe(false));

    cmd.NotifyCanExecuteChanged();
    SHOULD(ShouldlyIntegral(order.size()).ShouldBe(std::size_t{ 2 }));
    SHOULD(ShouldlyIntegral(order[0]).ShouldBe(1));
    SHOULD(ShouldlyIntegral(order[1]).ShouldBe(3));

    // Re-add appends at the end.
    (void)cmd.add_can_execute_changed([&order] { order.push_back(4); });
    cmd.NotifyCanExecuteChanged();
    SHOULD(ShouldlyIntegral(order.size()).ShouldBe(std::size_t{ 5 }));
    SHOULD(ShouldlyIntegral(order[2]).ShouldBe(1));
    SHOULD(ShouldlyIntegral(order[3]).ShouldBe(3));
    SHOULD(ShouldlyIntegral(order[4]).ShouldBe(4));

    (void)t1; (void)t3;
}

TEST_CASE("S247 tokens are distinct + monotonic")
{
    RelayCommand cmd([](const Value&) {});
    auto t1 = cmd.add_can_execute_changed([] {});
    auto t2 = cmd.add_can_execute_changed([] {});
    auto t3 = cmd.add_can_execute_changed([] {});
    const bool strictIncreasing = (t1 < t2) && (t2 < t3);
    SHOULD(ShouldlyIntegral(strictIncreasing).ShouldBe(true));
    const bool allDistinct = (t1 != t2) && (t2 != t3) && (t1 != t3);
    SHOULD(ShouldlyIntegral(allDistinct).ShouldBe(true));
}

TEST_CASE("S247 zero subscribers: NotifyCanExecuteChanged is a safe no-op")
{
    RelayCommand cmd([](const Value&) {});
    // No subscribers registered; must not throw.
    cmd.NotifyCanExecuteChanged();
    SHOULD(ShouldlyIntegral(cmd.remove_can_execute_changed(std::size_t{ 1 })).ShouldBe(false));
}

TEST_CASE("S247 CanExecute is side-effect-free (fires nothing, changes nothing)")
{
    std::vector<Value> captured;
    HandlerRec rec;
    RelayCommand cmd([&captured](const Value& p) { captured.push_back(p); },
                     [](const Value& p) { return !p.isNull(); });

    (void)cmd.add_can_execute_changed(std::ref(rec));

    // Querying must not invoke the action nor raise the event.
    (void)cmd.CanExecute(Value::Int32(1));
    (void)cmd.CanExecute(Value::Int32(2));

    SHOULD(ShouldlyIntegral(captured.size()).ShouldBe(std::size_t{ 0 }));
    SHOULD(ShouldlyIntegral(rec.count).ShouldBe(0));
}

TEST_CASE("S247 Execute with a Value::Null() parameter")
{
    std::vector<Value> captured;
    RelayCommand cmd([&captured](const Value& p) { captured.push_back(p); });
    cmd.Execute(Value::Null());
    SHOULD(ShouldlyIntegral(captured.size()).ShouldBe(std::size_t{ 1 }));
    SHOULD(ShouldlyString(captured[0].isNull()).ShouldBe(true));
}

TEST_CASE("S247 move: transfers action + predicate + subscriptions; moved-from inert")
{
    std::vector<Value> captured;
    HandlerRec rec;
    RelayCommand src([&captured](const Value& p) { captured.push_back(p); },
                     [](const Value& p) { return !p.isNull(); });
    (void)src.add_can_execute_changed(std::ref(rec));

    RelayCommand dst(std::move(src));

    // dst is fully live: predicate + action + subscription survived the move.
    SHOULD(ShouldlyString(dst.CanExecute(Value::Int32(7))).ShouldBe(true));
    SHOULD(ShouldlyString(dst.CanExecute(Value::Null())).ShouldBe(false));
    dst.Execute(Value::Int32(9));
    dst.NotifyCanExecuteChanged();
    SHOULD(ShouldlyIntegral(captured.size()).ShouldBe(std::size_t{ 1 }));
    SHOULD(ShouldlyString(captured[0]).ShouldBe(Value::Int32(9)));
    SHOULD(ShouldlyIntegral(rec.count).ShouldBe(1));

    // Moved-from `src` is inert: CanExecute always true, Execute is a no-op,
    // and no handlers remain.
    SHOULD(ShouldlyString(src.CanExecute(Value::Null())).ShouldBe(true));
    src.Execute(Value::Int32(100));
    src.NotifyCanExecuteChanged();
    SHOULD(ShouldlyIntegral(captured.size()).ShouldBe(std::size_t{ 1 }));
    SHOULD(ShouldlyIntegral(rec.count).ShouldBe(1));
}

TEST_CASE("S247 move-assign: re-arms onto the destination; moved-from inert")
{
    std::vector<Value> aCap, bCap;
    HandlerRec aRec;
    RelayCommand a([&aCap](const Value& p) { aCap.push_back(p); });
    (void)a.add_can_execute_changed(std::ref(aRec));
    RelayCommand b([&bCap](const Value& p) { bCap.push_back(p); });

    b = std::move(a);

    b.Execute(Value::Int32(3));
    b.NotifyCanExecuteChanged();
    SHOULD(ShouldlyIntegral(aCap.size()).ShouldBe(std::size_t{ 1 }));
    SHOULD(ShouldlyIntegral(bCap.size()).ShouldBe(std::size_t{ 0 }));
    SHOULD(ShouldlyIntegral(aRec.count).ShouldBe(1));

    // Moved-from `a` inert.
    a.Execute(Value::Int32(4));
    SHOULD(ShouldlyIntegral(aCap.size()).ShouldBe(std::size_t{ 1 }));
}

TEST_CASE("S247 re-query contract: a captured flag flips CanExecute + a subscriber observes")
{
    bool enabled = true;
    RelayCommand cmd([](const Value&) {});
    // The command's executability is governed by the external `enabled` flag.
    // (S247 wires this through a lambda that reads the flag each CanExecute;
    // the WPF RelayCommand analogue would update CanExecute on a property
    // change + raise the event.)
    RelayCommand predCmd([](const Value&) {},
                         [&enabled](const Value&) { return enabled; });

    std::vector<int> observed;
    (void)predCmd.add_can_execute_changed([&] { observed.push_back(enabled ? 1 : 0); });

    SHOULD(ShouldlyString(predCmd.CanExecute(Value::Null())).ShouldBe(true));
    enabled = false;
    SHOULD(ShouldlyString(predCmd.CanExecute(Value::Null())).ShouldBe(false));
    predCmd.NotifyCanExecuteChanged(); // subscriber observes the CURRENT flag
    enabled = true;
    SHOULD(ShouldlyString(predCmd.CanExecute(Value::Null())).ShouldBe(true));
    predCmd.NotifyCanExecuteChanged();

    SHOULD(ShouldlyIntegral(observed.size()).ShouldBe(std::size_t{ 2 }));
    SHOULD(ShouldlyIntegral(observed[0]).ShouldBe(0)); // observed while false
    SHOULD(ShouldlyIntegral(observed[1]).ShouldBe(1)); // observed while true

    (void)cmd;
}
