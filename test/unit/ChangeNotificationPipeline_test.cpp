// S041 — Change notification pipeline tests (dep:S040, dep:S038, dep:S037).
// The S041 headline end-to-end: ChangeQueue (per-thread) + processor drains in
// REGISTRATION ORDER, plus the DependencyObject subscriber registry (S041 §2.3)
// and the S040-recorded DESCENDANT PUSH (S041 §2.4, delivered here NOT silent).
//
// Proves:
//   1. ChangeProcessor: two enqueues of the same property before a drain
//      collapse to ONE notification (S038 coalescing, first old kept + latest
//      new adopted — S042 outcome realized end-to-end).
//   2. ChangeProcessor: distinct properties emit once each in FIRST-RECORD order.
//   3. DependencyObject registration-ordered subscribers: handlers fire in the
//      exact order added; remove token detaches; a re-add appends at the end.
//   4. DependencyObject descendant push (S041 §2.4) driven by the per-object
//      notification queue + ProcessNotificationQueue:
//        - parent Inherits change -> inheriting child fires pushed notification
//          (oldEffective -> newEffective);
//        - great-grandparent change -> great-grandchild notified (multi-level);
//        - local-wins child (own override) does NOT get swept, but its
//          grandchild beneath still does;
//        - InheritOnly child always inherits.
//   5. An enqueue that never fires stays pending (pending_notification_count).
//
// Uses the Shouldly-for-doctest vocabulary (SHOULD / ShouldThrow), mirroring
// InheritanceCascade_test.cpp's hostInherited() global-descriptor harness.

// doctest's main comes from smoke_test.cpp.
#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/ChangeProcessor.h"
#include "vaml/core/DependencyObject.h"
#include "vaml/core/DependencyProperty.h"
#include "vaml/core/PropertyId.h"
#include "vaml/core/PropertyKey.h"
#include "vaml/core/Value.h"

#include <string>
#include <unordered_set>
#include <vector>

using vaml::DependencyObject;
using vaml::DependencyProperty;
using vaml::PropertyFlags;
using vaml::PropertyId;
using vaml::PropertyKey;
using vaml::PropertyMetadata;
using vaml::Value;
using vaml::ValueType;
using vaml::ChangeProcessor;
using vaml::test::ShouldThrow;

namespace {

// The GLOBAL DependencyProperty table is name-unique (S039). Track names so each
// inherited property's global descriptor is written exactly once per suite.
std::unordered_set<std::string>& global_seen()
{
    static std::unordered_set<std::string> seen;
    return seen;
}

// Registers the GLOBAL DependencyProperty descriptor (once per name) AND mirrors
// the property onto `obj`'s OWN store (object-scoped; dedupe is per-object).
void hostInherited(DependencyObject& obj, const std::string& name, PropertyFlags flags, Value def)
{
    if (global_seen().insert(name).second)
    {
        (void)DependencyProperty::Register(name, ValueType::Int32, "PropOwner", PropertyMetadata::WithDefault(def, flags));
    }
    PropertyKey key(name, ValueType::Int32, "PropOwner", flags);
    (void)obj.register_property(std::move(key), PropertyMetadata::WithDefault(def, flags));
}

constexpr PropertyFlags kInh = PropertyFlags::Inherits;

// Collects fired subscriber notifications into (name, old, new) Int32 triples.
struct SubTrace
{
    std::vector<std::string> names;
    std::vector<std::int32_t> olds;
    std::vector<std::int32_t> news;

    // Returns a PropertyChangedHandler that appends. `ownerName` must be a static
    // literal (the handler copies it into `names` via id.name() on fire).
    DependencyObject::PropertyChangedHandler handler()
    {
        return [this](PropertyId id, const Value& o, const Value& n) {
            names.emplace_back(id.name());
            olds.push_back(o.get<std::int32_t>());
            news.push_back(n.get<std::int32_t>());
        };
    }
};

} // namespace

TEST_CASE("S041: ChangeProcessor coalesces — two enqueues of the same prop equal one notification")
{
    ChangeProcessor p;
    p.enqueue(PropertyId::of("W"), Value::Int32(0), Value::Int32(5));
    p.enqueue(PropertyId::of("W"), Value::Int32(5), Value::Int32(8));

    SHOULD(ShouldlyString(p.pending_count()).ShouldBe(2u)); // both queued, raw

    std::vector<Value> olds, news;
    auto fn = [&](PropertyId id, const Value& o, const Value& n) {
        (void)id;
        olds.push_back(o);
        news.push_back(n);
    };

    // Coalesced: ONE notification, FIRST old (0) + LATEST new (8).
    SHOULD(ShouldlyString(p.process(fn)).ShouldBe(1u));
    SHOULD(ShouldlyString(olds.size()).ShouldBe(1u));
    SHOULD(ShouldlyString(olds[0]).ShouldBe(Value::Int32(0)));
    SHOULD(ShouldlyString(news[0]).ShouldBe(Value::Int32(8)));
    SHOULD(ShouldlyString(p.pending_count()).ShouldBe(0u));
}

TEST_CASE("S041: ChangeProcessor emits distinct properties once each in first-record order")
{
    ChangeProcessor p;
    p.enqueue(PropertyId::of("Width"), Value::Int32(0), Value::Int32(10));
    p.enqueue(PropertyId::of("Height"), Value::Int32(0), Value::Int32(20));
    p.enqueue(PropertyId::of("Width"), Value::Int32(10), Value::Int32(15)); // coalesce Width

    std::vector<std::string> names;
    std::vector<std::int32_t> news;
    auto fn = [&](PropertyId id, const Value&, const Value& n) {
        names.emplace_back(id.name());
        news.push_back(n.get<std::int32_t>());
    };

    SHOULD(ShouldlyString(p.process(fn)).ShouldBe(2u));
    // First-record order: Width (first-record) before Height (second).
    SHOULD(ShouldlyString(names[0]).ShouldBe(std::string("Width")));
    SHOULD(ShouldlyString(news[0]).ShouldBe(15)); // latest new after coalesce
    SHOULD(ShouldlyString(names[1]).ShouldBe(std::string("Height")));
    SHOULD(ShouldlyString(news[1]).ShouldBe(20));
}

TEST_CASE("S041: ChangeProcessor empty drain returns 0")
{
    ChangeProcessor p;
    int called = 0;
    auto fn = [&](PropertyId, const Value&, const Value&) { ++called; };
    SHOULD(ShouldlyString(p.process(fn)).ShouldBe(0u));
    SHOULD(ShouldlyString(called).ShouldBe(0));
    SHOULD(ShouldlyString(p.pending_count()).ShouldBe(0u));
}

TEST_CASE("S041: DependencyObject subscribers fire in REGISTRATION ORDER (via descendant push)")
{
    // The registration-order proof drives the REAL pipeline: a parent Inherits
    // change PUSHES into the child's queue, and ProcessNotificationQueue fires
    // the child's registration-ordered subscribers.
    DependencyObject parent, child;
    const std::string name = "SubOrder_Push_S041_2";
    hostInherited(parent, name, kInh, Value::Int32(0));
    hostInherited(child, name, kInh, Value::Int32(0));
    child.set_parent(&parent);

    std::vector<int> order;
    auto make = [&](int tag) {
        return [&, tag](PropertyId, const Value&, const Value&) { order.push_back(tag); };
    };
    (void)child.add_property_changed(PropertyId::of(name), make(1));
    (void)child.add_property_changed(PropertyId::of(name), make(2));
    (void)child.add_property_changed(PropertyId::of(name), make(3));

    (void)parent.Set(PropertyId::of(name), Value::Int32(11)); // push -> child queue
    SHOULD(ShouldlyString(child.pending_notification_count()).ShouldBe(1u));

    child.ProcessNotificationQueue(); // drain -> fire child subscribers in order
    SHOULD(ShouldlyString(order.size()).ShouldBe(3u));
    SHOULD(ShouldlyString(order[0]).ShouldBe(1));
    SHOULD(ShouldlyString(order[1]).ShouldBe(2));
    SHOULD(ShouldlyString(order[2]).ShouldBe(3));
    SHOULD(ShouldlyString(child.pending_notification_count()).ShouldBe(0u));
}

TEST_CASE("S041: remove token detaches; re-add appends at the end")
{
    DependencyObject parent, child;
    const std::string name = "SubRemove_S041_3";
    hostInherited(parent, name, kInh, Value::Int32(0));
    hostInherited(child, name, kInh, Value::Int32(0));
    child.set_parent(&parent);

    std::vector<int> order;
    auto make = [&](int tag) {
        return [&, tag](PropertyId, const Value&, const Value&) { order.push_back(tag); };
    };
    auto t1 = child.add_property_changed(PropertyId::of(name), make(1));
    auto t2 = child.add_property_changed(PropertyId::of(name), make(2));
    auto t3 = child.add_property_changed(PropertyId::of(name), make(3));

    child.remove_property_changed(t2); // detach the middle handler
    (void)t3;

    // A re-add appends at the END (after t3).
    auto t4 = child.add_property_changed(PropertyId::of(name), make(4));
    (void)t4;

    (void)parent.Set(PropertyId::of(name), Value::Int32(7));
    child.ProcessNotificationQueue();

    // Fired: t1, t3, t4 — in registration order, with t2 removed.
    SHOULD(ShouldlyString(order.size()).ShouldBe(3u));
    SHOULD(ShouldlyString(order[0]).ShouldBe(1));
    SHOULD(ShouldlyString(order[1]).ShouldBe(3));
    SHOULD(ShouldlyString(order[2]).ShouldBe(4));

    // Removed handler does not fire even on a second push.
    (void)t1;
}

TEST_CASE("S041: descendant push carries oldEffective->newEffective delta")
{
    DependencyObject parent, child;
    const std::string name = "PushDelta_S041_4";
    hostInherited(parent, name, kInh, Value::Int32(0));
    hostInherited(child, name, kInh, Value::Int32(0));
    child.set_parent(&parent);

    SubTrace trace;
    (void)child.add_property_changed(PropertyId::of(name), trace.handler());

    (void)parent.Set(PropertyId::of(name), Value::Int32(5));
    child.ProcessNotificationQueue();

    SHOULD(ShouldlyString(trace.names.size()).ShouldBe(1u));
    SHOULD(ShouldlyString(trace.names[0]).ShouldBe(std::string(name)));
    // oldEffective: parent's previous effective (default 0) -> newEffective 5.
    SHOULD(ShouldlyString(trace.olds[0]).ShouldBe(0));
    SHOULD(ShouldlyString(trace.news[0]).ShouldBe(5));

    // A second parent change pushes the next delta (5 -> 9).
    (void)parent.Set(PropertyId::of(name), Value::Int32(9));
    child.ProcessNotificationQueue();
    SHOULD(ShouldlyString(trace.names.size()).ShouldBe(2u));
    SHOULD(ShouldlyString(trace.olds[1]).ShouldBe(5));
    SHOULD(ShouldlyString(trace.news[1]).ShouldBe(9));
}

TEST_CASE("S041: multi-level descendant push — great-grandparent change notifies great-grandchild")
{
    DependencyObject g1, g2, g3, leaf;
    const std::string name = "PushMulti_S041_5";
    for (DependencyObject* o : { &g1, &g2, &g3, &leaf })
    {
        hostInherited(*o, name, kInh, Value::Int32(0));
    }
    g2.set_parent(&g1);
    g3.set_parent(&g2);
    leaf.set_parent(&g3);

    SubTrace trace;
    (void)leaf.add_property_changed(PropertyId::of(name), trace.handler());

    // Change the great-grandparent; every intermediate (g2, g3) holds no local,
    // so the leaf inherits and must be notified.
    (void)g1.Set(PropertyId::of(name), Value::Int32(5));
    leaf.ProcessNotificationQueue();

    SHOULD(ShouldlyString(trace.names.size()).ShouldBe(1u));
    SHOULD(ShouldlyString(trace.names[0]).ShouldBe(std::string(name)));
    SHOULD(ShouldlyString(trace.news[0]).ShouldBe(5));

    // Intermediate nodes also hold a pending notification (they inherit too).
    SHOULD(ShouldlyString(g2.pending_notification_count()).ShouldBe(1u));
    SHOULD(ShouldlyString(g3.pending_notification_count()).ShouldBe(1u));
}

TEST_CASE("S041: local-wins intermediate shields BOTH itself and descendants beneath it")
{
    DependencyObject parent, child, grandchild;
    const std::string name = "PushLocalWins_S041_6";
    hostInherited(parent, name, kInh, Value::Int32(0));
    hostInherited(child, name, kInh, Value::Int32(0));
    hostInherited(grandchild, name, kInh, Value::Int32(0));
    child.set_parent(&parent);
    grandchild.set_parent(&child);

    // Child declares its OWN local override -> it becomes the inherited SOURCE
    // for everything beneath it (the local-wins node shields its subtree from
    // the parent). This Set itself pushed 0->7 to the grandchild — drain and
    // clear those queues so the assertions measure only the parent move.
    (void)child.Set(PropertyId::of(name), Value::Int32(7));
    SHOULD(ShouldlyString(grandchild.pending_notification_count()).ShouldBe(1u));
    grandchild.ProcessNotificationQueue();
    SHOULD(ShouldlyString(child.pending_notification_count()).ShouldBe(0u));
    SHOULD(ShouldlyString(grandchild.pending_notification_count()).ShouldBe(0u));

    SubTrace childTrace, gcTrace;
    (void)child.add_property_changed(PropertyId::of(name), childTrace.handler());
    (void)grandchild.add_property_changed(PropertyId::of(name), gcTrace.handler());

    // Parent changes: the child's local-wins node is NOT swept (effective stays
    // its own 7), and the grandchild's effective is the CHILD's local (7), not
    // the parent's — so the grandchild is unaffected too. Neither is notified:
    // the parent move changes no descendant's effective value.
    (void)parent.Set(PropertyId::of(name), Value::Int32(99));

    SHOULD(ShouldlyString(child.GetEffectiveValue(PropertyId::of(name))).ShouldBe(Value::Int32(7)));
    SHOULD(ShouldlyString(grandchild.GetEffectiveValue(PropertyId::of(name))).ShouldBe(Value::Int32(7)));

    // No effective value changed anywhere -> no pushed notification anywhere.
    child.ProcessNotificationQueue();
    grandchild.ProcessNotificationQueue();
    SHOULD(ShouldlyString(childTrace.names.size()).ShouldBe(0u));
    SHOULD(ShouldlyString(gcTrace.names.size()).ShouldBe(0u));
    SHOULD(ShouldlyString(child.pending_notification_count()).ShouldBe(0u));
    SHOULD(ShouldlyString(grandchild.pending_notification_count()).ShouldBe(0u));

    // Sanity: the same subtree IS notified when the ORIGIN of the inherited
    // value changes — change the CHILD's local and the grandchild (which
    // inherits through it) must be notified.
    (void)child.Set(PropertyId::of(name), Value::Int32(8));
    grandchild.ProcessNotificationQueue();
    SHOULD(ShouldlyString(gcTrace.names.size()).ShouldBe(1u));
    SHOULD(ShouldlyString(gcTrace.news[0]).ShouldBe(8)); // 7 -> 8, own delta
}

TEST_CASE("S041: InheritOnly descendant always inherits")
{
    DependencyObject parent, child;
    const std::string name = "PushInheritOnly_S041_7";
    const PropertyFlags inhInheritOnly = PropertyFlags::Inherits | PropertyFlags::InheritOnly;
    hostInherited(parent, name, inhInheritOnly, Value::Int32(0));
    hostInherited(child, name, inhInheritOnly, Value::Int32(0));
    child.set_parent(&parent);

    // Child SETs its own local override — but InheritOnly means it is IGNORED,
    // so it must still inherit and be notified of the parent's change.
    (void)child.Set(PropertyId::of(name), Value::Int32(100));

    SubTrace trace;
    (void)child.add_property_changed(PropertyId::of(name), trace.handler());

    (void)parent.Set(PropertyId::of(name), Value::Int32(3));
    child.ProcessNotificationQueue();

    SHOULD(ShouldlyString(trace.names.size()).ShouldBe(1u));
    SHOULD(ShouldlyString(child.GetEffectiveValue(PropertyId::of(name))).ShouldBe(Value::Int32(3)));
    SHOULD(ShouldlyString(trace.news[0]).ShouldBe(3));
}

TEST_CASE("S041: no registered property -> pushed nothing; pending stays 0")
{
    DependencyObject parent, child;
    const std::string name = "PushNoProp_S041_8";
    hostInherited(parent, name, kInh, Value::Int32(0));
    // Child does NOT register the property -> cannot inherit; no push.
    child.set_parent(&parent);

    (void)parent.Set(PropertyId::of(name), Value::Int32(5));
    SHOULD(ShouldlyString(child.pending_notification_count()).ShouldBe(0u));
}

TEST_CASE("S041: single subscriber handler fires once per push; object without subscribers no-op")
{
    DependencyObject parent, child;
    const std::string name = "PushSingle_S041_9";
    hostInherited(parent, name, kInh, Value::Int32(0));
    hostInherited(child, name, kInh, Value::Int32(0));
    child.set_parent(&parent);

    SubTrace trace;
    (void)child.add_property_changed(PropertyId::of(name), trace.handler());

    (void)parent.Set(PropertyId::of(name), Value::Int32(2));
    child.ProcessNotificationQueue();
    SHOULD(ShouldlyString(trace.names.size()).ShouldBe(1u));
    SHOULD(ShouldlyString(trace.news[0]).ShouldBe(2));
    SHOULD(ShouldlyString(child.pending_notification_count()).ShouldBe(0u));
}
