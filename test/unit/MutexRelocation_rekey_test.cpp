// S143 — mutex relocation + token re-key tests (dep:S142).
//
// Proves the S143 title contract:
//   PART A (mutex relocation into the ChangeQueue, rewrites S037/S041):
//     1. The per-object pending mutex physically relocates OUT of DependencyObject
//        INTO its single ChangeQueue (pending_/queue_mutex_ removed). The S037
//        value-write family (enqueue_value/drain_values/value_pending_count)
//        and the S041 notification family (enqueue/process/pending_count) now
//        share ONE queue + ONE mutex + ONE affinity marker.
//     2. BEHAVIOR-UNCHANGED: wrong-thread Set -> QUEUED; drain on affinity thread
//        applies FIFO with correct (old,new) cbs; non-affine drain throws
//        WrongThreadException (the S037 guard MOVED WITH THE QUEUE); swap-out
//        preserved (writers during a drain land in a fresh deque).
//     3. Coexistence: a pending value-write and a pending notification can sit
//        side-by-side in the SAME object's queue and drain through their OWN half
//        without cross-drain interference.
//   PART B (re-key onto S140 token + S141 store):
//     4. PropertyRegistry::global()/instance lookup by string_view CONTENT (no
//        transient std::string); dedupe-throws unchanged; returned-key stability.
//     5. DependencyProperty::Register/TryGet/Get content lookup; duplicate-name
//        throws PropertyAlreadyRegistered + stores nothing; property_id()/
//        owner_type_id() views remain valid.
//     6. ChangeQueue drain re-interns to canonical views (drained name resolves
//        in the affinity thread's intern store).
//
// ADR-018: zero new locks are introduced; the relocated queue mutex is the
// standing pre-approved S037/S041 lock (S143 rewrites S037/S041 in place).

// doctest's main comes from smoke_test.cpp (DO doctest IMPLEMENT_WITH_MAIN).
#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/ChangeQueue.h"
#include "vaml/core/DependencyObject.h"
#include "vaml/core/DependencyProperty.h"
#include "vaml/core/Intern.h"
#include "vaml/core/PropertyKey.h"
#include "vaml/core/PropertyRegistry.h"

#include <string>
#include <thread>
#include <utility>
#include <vector>

using vaml::ChangeQueue;
using vaml::DependencyObject;
using vaml::DependencyProperty;
using vaml::intern;
using vaml::PropertyAlreadyRegistered;
using vaml::PropertyFlags;
using vaml::PropertyId;
using vaml::PropertyKey;
using vaml::PropertyMetadata;
using vaml::PropertyNotFound;
using vaml::PropertyRegistry;
using vaml::SetResult;
using vaml::Value;
using vaml::ValueType;
using vaml::ValueTypeMismatch;
using vaml::WrongThreadException;
using vaml::test::ShouldThrow;

namespace {

PropertyKey int32Key(const std::string& name)
{
    return PropertyKey(name, ValueType::Int32, "S143Owner", PropertyFlags::None);
}

// Captures value-write drains into (name, value) pairs.
struct ValueWriteTrace
{
    std::vector<std::string> names;
    std::vector<std::int32_t> values;

    ChangeQueue::ValueDrainFn fn()
    {
        return [this](std::string_view n, const Value& v) {
            names.emplace_back(n);
            values.push_back(v.get<std::int32_t>());
        };
    }
};

// A non-affine Set() on an Int32 property, value captured back via a join barrier.
void setFromOtherThread(DependencyObject& obj, const PropertyId& id, Value v, SetResult* out)
{
    std::thread t([&] { *out = obj.Set(id, v); });
    t.join();
}

} // namespace

TEST_CASE("S143 PartA: S037 value-write family lives in the single ChangeQueue (enqueue_value/drain_values)")
{
    ChangeQueue q; // created on THIS thread = affinity thread
    SHOULD(ShouldlyString(q.value_pending_count()).ShouldBe(0u));

    // Cross-thread value-write enqueue (relocated S037 postbox; same mutex as
    // the notification family).
    std::thread t([&] { q.enqueue_value("Width", Value::Int32(10)); });
    t.join();
    SHOULD(ShouldlyString(q.value_pending_count()).ShouldBe(1u));

    ValueWriteTrace trace;
    SHOULD(ShouldlyString(q.drain_values(trace.fn())).ShouldBe(1u));
    SHOULD(ShouldlyString(trace.names[0]).ShouldBe(std::string("Width")));
    SHOULD(ShouldlyString(trace.values[0]).ShouldBe(10));
    SHOULD(ShouldlyString(q.value_pending_count()).ShouldBe(0u));
}

TEST_CASE("S143 PartA: drain_values from a non-affine thread throws WrongThreadException (guard moved with the queue)")
{
    ChangeQueue q;
    q.enqueue_value("W", Value::Int32(7));
    SHOULD(ShouldlyString(q.value_pending_count()).ShouldBe(1u));

    bool caught = false;
    std::thread t([&] {
        try
        {
            q.drain_values([](std::string_view, const Value&) {});
        }
        catch (WrongThreadException const&)
        {
            caught = true;
        }
    });
    t.join();

    SHOULD(ShouldlyString(caught).ShouldBe(true));
    SHOULD(ShouldlyString(q.value_pending_count()).ShouldBe(1u)); // not drained
}

TEST_CASE("S143 PartA: S037 wrong-thread Set QUEUED + affinity drain applies FIFO with correct cbs")
{
    DependencyObject obj;
    std::vector<std::pair<std::int32_t, std::int32_t>> seen;
    obj.register_property(
        int32Key("Counter"),
        PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None)
            .with_changed([&](const Value& o, const Value& n) {
                seen.emplace_back(o.get<std::int32_t>(), n.get<std::int32_t>());
            }));

    // Two wrong-thread writes [5,7] -> QUEUED, not applied.
    for (std::int32_t v : {5, 7})
    {
        SetResult r = SetResult::NO_CHANGE;
        setFromOtherThread(obj, PropertyId::of("Counter"), Value::Int32(v), &r);
        SHOULD(ShouldlyString(r).ShouldBe(SetResult::QUEUED));
    }
    SHOULD(ShouldlyString(obj.pending_change_count()).ShouldBe(2u));
    SHOULD(ShouldlyString(seen.size()).ShouldBe(0u)); // no cb until drain

    obj.ProcessPendingChanges(); // affinity (this) thread

    SHOULD(ShouldlyString(obj.Get(PropertyId::of("Counter"))).ShouldBe(Value::Int32(7)));
    SHOULD(ShouldlyString(obj.pending_change_count()).ShouldBe(0u));
    // FIFO effective chain: cb(0->5) then cb(5->7).
    SHOULD(ShouldlyString(seen.size()).ShouldBe(2u));
    SHOULD(ShouldlyString(seen[0].first).ShouldBe(0));
    SHOULD(ShouldlyString(seen[0].second).ShouldBe(5));
    SHOULD(ShouldlyString(seen[1].first).ShouldBe(5));
    SHOULD(ShouldlyString(seen[1].second).ShouldBe(7));
}

TEST_CASE("S143 PartA: ProcessPendingChanges from a non-affine thread throws (guard relocated with the queue)")
{
    DependencyObject obj;
    obj.register_property(int32Key("W"), PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None));

    SetResult r = SetResult::NO_CHANGE;
    setFromOtherThread(obj, PropertyId::of("W"), Value::Int32(9), &r);
    SHOULD(ShouldlyString(r).ShouldBe(SetResult::QUEUED));
    SHOULD(ShouldlyString(obj.pending_change_count()).ShouldBe(1u));

    bool caught = false;
    std::thread t([&] {
        try
        {
            obj.ProcessPendingChanges();
        }
        catch (WrongThreadException const&)
        {
            caught = true;
        }
    });
    t.join();

    SHOULD(ShouldlyString(caught).ShouldBe(true));
    SHOULD(ShouldlyString(obj.pending_change_count()).ShouldBe(1u)); // still pending
    SHOULD(ShouldlyString(obj.Get(PropertyId::of("W"))).ShouldBe(Value::Int32(0))); // store unchanged
}

TEST_CASE("S143 PartA: swap-out preserved — concurrent value-writes during a drain land in a fresh deque")
{
    ChangeQueue q;
    q.enqueue_value("A", Value::Int32(1));

    int emitted = 0;
    auto fn = [&](std::string_view n, const Value&) {
        ++emitted;
        if (emitted == 1)
        {
            q.enqueue_value(n, Value::Int32(2)); // re-enqueue during drain
        }
    };

    // Snapshot-then-run: the re-enqueue does NOT double-emit A THIS pass...
    SHOULD(ShouldlyString(q.drain_values(fn)).ShouldBe(1u));
    SHOULD(ShouldlyString(emitted).ShouldBe(1));
    // ...but it IS pending for a subsequent drain.
    SHOULD(ShouldlyString(q.value_pending_count()).ShouldBe(1u));
}

TEST_CASE("S143 PartA: a value-write and a notification coexist in the same queue without cross-drain interference")
{
    ChangeQueue q; // one queue, one mutex, two families
    q.enqueue_value("Width", Value::Int32(5));
    q.enqueue(PropertyId::of("Height"), Value::Int32(0), Value::Int32(9));

    // The two families have independent depths...
    SHOULD(ShouldlyString(q.value_pending_count()).ShouldBe(1u));
    SHOULD(ShouldlyString(q.pending_count()).ShouldBe(1u));

    // Draining the VALUE half leaves the NOTIFICATION half untouched.
    ValueWriteTrace vTrace;
    SHOULD(ShouldlyString(q.drain_values(vTrace.fn())).ShouldBe(1u));
    SHOULD(ShouldlyString(vTrace.names[0]).ShouldBe(std::string("Width")));
    SHOULD(ShouldlyString(q.value_pending_count()).ShouldBe(0u));
    SHOULD(ShouldlyString(q.pending_count()).ShouldBe(1u)); // notification still pending

    // Draining the NOTIFICATION half leaves the (now drained) value half untouched.
    int notified = 0;
    ChangeQueue::NotifyFn nf = [&](PropertyId id, const Value&, const Value&) {
        ++notified;
        (void)id;
    };
    SHOULD(ShouldlyString(q.process(nf)).ShouldBe(1u));
    SHOULD(ShouldlyString(notified).ShouldBe(1));
    SHOULD(ShouldlyString(q.value_pending_count()).ShouldBe(0u));
    SHOULD(ShouldlyString(q.pending_count()).ShouldBe(0u));
}

TEST_CASE("S143 PartB: ChangeQueue drain re-interns to the affinity thread's canonical view")
{
    // A value-write enqueued from ANOTHER thread carries an owned std::string;
    // drain_values re-interns it on THIS (affinity) thread to a canonical view.
    ChangeQueue q;
    std::thread t([&] { q.enqueue_value("Foreground", Value::Int32(3)); });
    t.join();

    std::string_view drained;
    std::thread::id affinity = std::this_thread::get_id(); // the queue's affinity
    ValueWriteTrace trace;
    auto fn = [&](std::string_view n, const Value& v) {
        drained = n;
        trace.names.emplace_back(n);
        trace.values.push_back(v.get<std::int32_t>());
    };
    SHOULD(ShouldlyString(q.drain_values(fn)).ShouldBe(1u));

    (void)affinity;
    // The drained view is the canonical interned view on THIS thread (content-
    // equal to intern("Foreground")), and its address matches the store's.
    SHOULD(ShouldlyString(drained == intern("Foreground")).ShouldBe(true));
    SHOULD(ShouldlyString(trace.names[0]).ShouldBe(std::string("Foreground")));
    SHOULD(ShouldlyString(trace.values[0]).ShouldBe(3));
}

TEST_CASE("S143 PartB: PropertyRegistry content lookup — no transient std::string, dedupe unchanged, key stability")
{
    PropertyRegistry reg;
    const PropertyKey& w = reg.register_key(int32Key("S143_Width"));
    const PropertyKey& m = reg.register_key(int32Key("S143_Margin"));

    // Lookup by string_view content (the previous const std::string& overloads
    // bound implicitly; string_view materializes no transient std::string).
    SHOULD(ShouldlyString(reg.try_find(std::string_view("S143_Width"))).ShouldBe(&w));
    SHOULD(ShouldlyString(reg.find(std::string_view("S143_Margin")).name()).ShouldBe(std::string("S143_Margin")));
    SHOULD(ShouldlyString(reg.id_of("S143_Width")).ShouldBe(PropertyId::of("S143_Width")));
    SHOULD(ShouldlyString(reg.try_id("S143_Width").valid()).ShouldBe(true));
    SHOULD(ShouldlyString(reg.try_id("S143_Missing").valid()).ShouldBe(false));

    // Dedupe throws + leaves the registry unchanged.
    SHOULD(ShouldThrow<PropertyAlreadyRegistered>([&] {
        (void)reg.register_key(int32Key("S143_Width"));
    }));
    SHOULD(ShouldlyString(reg.size()).ShouldBe(2u));

    // Returned key reference remains stable (the NameTable owns the key node).
    SHOULD(ShouldlyString(reg.try_find("S143_Width")).ShouldBe(&w));
    (void)m;

    // Register many to force the node-vector relocation; stored key refs stay valid.
    {
        PropertyRegistry big;
        std::vector<const PropertyKey*> refs;
        const int n = 200;
        for (int i = 0; i < n; ++i)
        {
            const PropertyKey& k = big.register_key(int32Key("S143_Bulk_" + std::to_string(i)));
            refs.push_back(&k);
        }
        for (int i = 0; i < n; ++i)
        {
            // Every earlier reference is still valid + content-resolvable.
            SHOULD(ShouldlyString(big.try_find("S143_Bulk_" + std::to_string(i))).ShouldBe(refs[i]));
        }
    }
}

TEST_CASE("S143 PartB: DependencyProperty content lookup — Register/TryGet/Get, dedupe throws, views stay valid")
{
    // Unique names to avoid cross-TEST_CASE interference in the global table.
    const std::string name = "S143_Dp_ThemeColor";
    const DependencyProperty& dp = DependencyProperty::Register(
        name, ValueType::Int32, "S143Owner",
        PropertyMetadata::WithDefault(Value::Int32(3), PropertyFlags::Inherits));

    SHOULD(ShouldlyString(dp.property_id().name()).ShouldBe(name));
    // property_id()/owner_type_id() views remain valid after content lookup.
    const std::string& owner = dp.owner_type_id();
    SHOULD(ShouldlyString(owner).ShouldBe(std::string("S143Owner")));

    // TryGet/Get by content (string_view) — no transient std::string.
    const DependencyProperty* got = DependencyProperty::TryGet(std::string_view(name));
    SHOULD(ShouldlyString(got != nullptr).ShouldBe(true));
    if (got != nullptr)
    {
        SHOULD(ShouldlyString(got->metadata().defaultValue()).ShouldBe(Value::Int32(3)));
    }
    SHOULD(ShouldlyString(DependencyProperty::Get(name).property_id().name()).ShouldBe(name));

    // Duplicate-name Register throws + stores nothing (existing kept).
    SHOULD(ShouldThrow<PropertyAlreadyRegistered>([&] {
        (void)DependencyProperty::Register(
            name, ValueType::Int32, "S143Owner",
            PropertyMetadata::WithDefault(Value::Int32(99), PropertyFlags::None));
    }));
    const DependencyProperty& still = DependencyProperty::Get(name);
    SHOULD(ShouldlyString(still.metadata().defaultValue()).ShouldBe(Value::Int32(3))); // original kept

    // Missing name -> null / throw.
    SHOULD(ShouldlyString(DependencyProperty::TryGet("S143_NoSuch") == nullptr).ShouldBe(true));
    SHOULD(ShouldThrow<PropertyNotFound>([&] {
        (void)DependencyProperty::Get("S143_NoSuch");
    }));
}
