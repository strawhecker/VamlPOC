// S037 — DependencyObject wrong-thread write QUEUING tests (marshalling
// foundation, dep:S036_5). CONSUMES S036's thread-affinity marker
// (thread_affinity_id / is_affine_to_current_thread) as the basis for
// wrong-thread detection; S037 adds actual wrong-thread write QUEUING
// (queued-not-applied).
//
// Proves the S037 title contract:
//   5. Set() is thread-affinity aware: a write from a NON-affine thread runs
//      coerce + validate immediately (instant writer feedback) but does NOT
//      store and does NOT fire the changed cb; on accept it is posted and
//      Set() returns SetResult::QUEUED.
//   6. ProcessPendingChanges() drains the queue (FIFO) on the affinity thread,
//      replaying each queued write through the S036 store half so a real
//      change fires cb(old,new) with the correct effective-value chain.
//      Calling it from a non-affine thread throws WrongThreadException.
//   7. pending_change_count() exposes the queue depth (mutex-protected).
//
// Cross-thread contract (recorded in DependencyObject.h + roadmap): the only
// truly cross-thread datum is the mutex-protected pending queue; the value
// store stays affinity-thread-owned by construction. Each test uses a join
// barrier so no concurrent drain/read occurs in the demonstration (full
// store-race freedom is TSan/S620/S723 territory, documented not silent).
//
// This is a NEW file, distinct from DependencyObject_test.cpp (the S036_5
// capstone proof) — the marshalling foundation gets its own traceable home.

// doctest's main comes from smoke_test.cpp (DO doctest IMPLEMENT_WITH_MAIN).
// This TU only declares tests — must NOT re-define implementation.
#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"       // S036_2 StringMaker + S036_4 operator<<
#include <vaml/assert/Shouldly.h>         // ShouldlyString / ShouldThrow (SHOULD seam, S036_1)

#include "vaml/core/DependencyObject.h"
#include "vaml/core/PropertyKey.h"

#include <atomic>
#include <string>
#include <thread>
#include <vector>

using vaml::DependencyObject;
using vaml::PropertyFlags;
using vaml::PropertyId;
using vaml::PropertyKey;
using vaml::PropertyMetadata;
using vaml::SetResult;
using vaml::Value;
using vaml::ValueType;
using vaml::ValueTypeMismatch;
using vaml::WrongThreadException;
using vaml::test::ShouldThrow;

namespace {

PropertyKey int32Key(const std::string& name)
{
    return PropertyKey(name, ValueType::Int32, "TestOwner", PropertyFlags::None);
}

// A non-affine Set() on an Int32 property, executed on a fresh thread, the
// result captured back into `out` by a join barrier (no concurrency).
void setFromOtherThread(DependencyObject& obj, const PropertyId& id, Value v, SetResult* out)
{
    std::thread t([&] {
        // This thread is definitely NOT obj's affinity thread.
        *out = obj.Set(id, v);
    });
    t.join();
}

} // namespace

TEST_CASE("S037: wrong-thread Set returns QUEUED, store unchanged, queue depth 1")
{
    DependencyObject obj;
    obj.register_property(int32Key("Width"), PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None));

    // Affine (this) thread Set still applies normally.
    SHOULD(ShouldlyString(obj.Set(PropertyId::of("Width"), Value::Int32(0))).ShouldBe(SetResult::NO_CHANGE));
    SHOULD(ShouldlyString(obj.pending_change_count()).ShouldBe(0u));

    // Wrong-thread write -> QUEUED, not applied.
    SetResult result = SetResult::NO_CHANGE;
    setFromOtherThread(obj, PropertyId::of("Width"), Value::Int32(42), &result);
    SHOULD(ShouldlyString(result).ShouldBe(SetResult::QUEUED));

    // Queued-not-applied: Get() still sees the old value AND the queue holds one.
    SHOULD(ShouldlyString(obj.Get(PropertyId::of("Width"))).ShouldBe(Value::Int32(0)));
    SHOULD(ShouldlyString(obj.pending_change_count()).ShouldBe(1u));
}

TEST_CASE("S037: drain applies queued writes in FIFO order with correct (old,new) cbs")
{
    DependencyObject obj;
    int fire = 0;
    std::vector<std::pair<std::int32_t, std::int32_t>> seen;
    obj.register_property(
        int32Key("Counter"),
        PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None)
            .with_changed([&](const Value& o, const Value& n) {
                seen.emplace_back(o.get<std::int32_t>(), n.get<std::int32_t>());
                ++fire;
            }));

    // Post [5, 7] from a non-affine thread (two frames; each joined).
    for (std::int32_t v : {5, 7})
    {
        SetResult r = SetResult::NO_CHANGE;
        setFromOtherThread(obj, PropertyId::of("Counter"), Value::Int32(v), &r);
        SHOULD(ShouldlyString(r).ShouldBe(SetResult::QUEUED));
    }
    SHOULD(ShouldlyString(obj.pending_change_count()).ShouldBe(2u));
    SHOULD(ShouldlyString(fire).ShouldBe(0)); // no cb fired until drain

    // Drain on the affinity (this) thread: plays [5,7] -> cb(0->5) then cb(5->7).
    obj.ProcessPendingChanges();

    SHOULD(ShouldlyString(obj.Get(PropertyId::of("Counter"))).ShouldBe(Value::Int32(7)));
    SHOULD(ShouldlyString(obj.pending_change_count()).ShouldBe(0u));
    SHOULD(ShouldlyString(fire).ShouldBe(2));
    // coerce/validate-at-queue; drain replays with the correct effective chain.
    SHOULD(ShouldlyString(seen[0].first).ShouldBe(0));
    SHOULD(ShouldlyString(seen[0].second).ShouldBe(5));
    SHOULD(ShouldlyString(seen[1].first).ShouldBe(5));
    SHOULD(ShouldlyString(seen[1].second).ShouldBe(7));
}

TEST_CASE("S037: wrong-thread Set on a value equal to the current effective is QUEUED then NO_CHANGE at drain")
{
    DependencyObject obj;
    int fire = 0;
    obj.register_property(
        int32Key("W"),
        PropertyMetadata::WithDefault(Value::Int32(3), PropertyFlags::None)
            .with_changed([&](const Value&, const Value&) { ++fire; }));

    // Queued value equals the current effective (3) -> still QUEUED at write time
    // (a no-change can only be judged at apply time), but produces NO cb at drain.
    SetResult r = SetResult::NO_CHANGE;
    setFromOtherThread(obj, PropertyId::of("W"), Value::Int32(3), &r);
    SHOULD(ShouldlyString(r).ShouldBe(SetResult::QUEUED));

    obj.ProcessPendingChanges();
    SHOULD(ShouldlyString(fire).ShouldBe(0)); // no real change -> no cb
    SHOULD(ShouldlyString(obj.Get(PropertyId::of("W"))).ShouldBe(Value::Int32(3)));
    SHOULD(ShouldlyString(obj.pending_change_count()).ShouldBe(0u));
}

TEST_CASE("S037: wrong-thread reject refused value -> REJECTED, nothing queued")
{
    DependencyObject obj;
    obj.register_property(
        int32Key("Age"),
        PropertyMetadata::WithDefault(Value::Int32(1), PropertyFlags::None)
            .with_validate([](const Value& v) { return v.get<std::int32_t>() > 0; }));

    // Wrong-thread write of a value validate refuses -> REJECTED, NOT queued.
    SetResult r = SetResult::NO_CHANGE;
    setFromOtherThread(obj, PropertyId::of("Age"), Value::Int32(-3), &r);
    SHOULD(ShouldlyString(r).ShouldBe(SetResult::REJECTED));

    SHOULD(ShouldlyString(obj.pending_change_count()).ShouldBe(0u)); // nothing queued
    SHOULD(ShouldlyString(obj.Get(PropertyId::of("Age"))).ShouldBe(Value::Int32(1))); // store unchanged

    // A later drain has nothing to do.
    obj.ProcessPendingChanges();
    SHOULD(ShouldlyString(obj.Get(PropertyId::of("Age"))).ShouldBe(Value::Int32(1)));
}

TEST_CASE("S037: wrong-thread wrong-tag input throws ValueTypeMismatch, nothing queued")
{
    DependencyObject obj;
    obj.register_property(int32Key("W"), PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None));

    // Wrong-tag input from a non-affine thread surfaces ValueTypeMismatch (the
    // call happens on a worker thread; we funnel the exception message out and
    // assert type via RTTI in a safe, single-threaded way after join).
    std::string thrownWhat;
    bool caughtMismatch = false;
    std::thread t([&] {
        try
        {
            (void)obj.Set(PropertyId::of("W"), Value::Double(2.5));
        }
        catch (ValueTypeMismatch const& e)
        {
            caughtMismatch = true;
            thrownWhat = e.what();
        }
    });
    t.join();

    SHOULD(ShouldlyString(caughtMismatch).ShouldBe(true));
    SHOULD(ShouldlyString(thrownWhat.empty()).ShouldBe(false));
    SHOULD(ShouldlyString(obj.pending_change_count()).ShouldBe(0u)); // nothing queued
    SHOULD(ShouldlyString(obj.Get(PropertyId::of("W"))).ShouldBe(Value::Int32(0)));
}

TEST_CASE("S037: drain from a non-affine thread throws WrongThreadException")
{
    DependencyObject obj;
    obj.register_property(int32Key("W"), PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None));

    // Prime the queue from a worker thread so there is something pending.
    SetResult r = SetResult::NO_CHANGE;
    setFromOtherThread(obj, PropertyId::of("W"), Value::Int32(9), &r);
    SHOULD(ShouldlyString(r).ShouldBe(SetResult::QUEUED));
    SHOULD(ShouldlyString(obj.pending_change_count()).ShouldBe(1u));

    // Drain from a non-affine thread -> WrongThreadException, queue untouched.
    bool caughtWrongThread = false;
    std::thread t([&] {
        try
        {
            obj.ProcessPendingChanges();
        }
        catch (WrongThreadException const&)
        {
            caughtWrongThread = true;
        }
    });
    t.join();

    SHOULD(ShouldlyString(caughtWrongThread).ShouldBe(true));
    SHOULD(ShouldlyString(obj.pending_change_count()).ShouldBe(1u)); // still pending, not drained
    SHOULD(ShouldlyString(obj.Get(PropertyId::of("W"))).ShouldBe(Value::Int32(0))); // store unchanged
}

TEST_CASE("S037: drain on the affinity thread throws no exception and from here is legal")
{
    DependencyObject obj;
    obj.register_property(int32Key("W"), PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None));

    // Empty drain on the affinity (this) thread is a legal no-op (no pending).
    // Proved by reaching here without throwing (SHOULD is not used on a call
    // that returns void; we use ShouldlyString on the observable outcome).
    obj.ProcessPendingChanges();
    SHOULD(ShouldlyString(obj.pending_change_count()).ShouldBe(0u));
}

TEST_CASE("S037: coalescing generalizes — a second wrong-thread frame still queues correctly")
{
    DependencyObject obj;
    int fire = 0;
    obj.register_property(
        int32Key("C"),
        PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None)
            .with_changed([&](const Value&, const Value&) { ++fire; }));

    // Three wrong-thread writes [1, 2, 3] -> three queued, nothing applied.
    for (std::int32_t v : {1, 2, 3})
    {
        SetResult r = SetResult::NO_CHANGE;
        setFromOtherThread(obj, PropertyId::of("C"), Value::Int32(v), &r);
        SHOULD(ShouldlyString(r).ShouldBe(SetResult::QUEUED));
    }
    SHOULD(ShouldlyString(obj.pending_change_count()).ShouldBe(3u));
    SHOULD(ShouldlyString(fire).ShouldBe(0));

    // First drain applies all three.
    obj.ProcessPendingChanges();
    SHOULD(ShouldlyString(obj.Get(PropertyId::of("C"))).ShouldBe(Value::Int32(3)));
    SHOULD(ShouldlyString(fire).ShouldBe(3));
    SHOULD(ShouldlyString(obj.pending_change_count()).ShouldBe(0u));

    // A second wrong-thread frame queues fresh writes; a second drain applies them.
    for (std::int32_t v : {4, 5})
    {
        SetResult r = SetResult::NO_CHANGE;
        setFromOtherThread(obj, PropertyId::of("C"), Value::Int32(v), &r);
        SHOULD(ShouldlyString(r).ShouldBe(SetResult::QUEUED));
    }
    SHOULD(ShouldlyString(obj.pending_change_count()).ShouldBe(2u));

    obj.ProcessPendingChanges();
    SHOULD(ShouldlyString(obj.Get(PropertyId::of("C"))).ShouldBe(Value::Int32(5)));
    SHOULD(ShouldlyString(fire).ShouldBe(5));
    SHOULD(ShouldlyString(obj.pending_change_count()).ShouldBe(0u));
}

TEST_CASE("S037: affine Set does not queue; a queued write + later affine Set both hold")
{
    DependencyObject obj;
    obj.register_property(int32Key("W"), PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None));

    // Affine Set is not queued.
    SHOULD(ShouldlyString(obj.Set(PropertyId::of("W"), Value::Int32(1))).ShouldBe(SetResult::APPLIED));
    SHOULD(ShouldlyString(obj.pending_change_count()).ShouldBe(0u));
    SHOULD(ShouldlyString(obj.Get(PropertyId::of("W"))).ShouldBe(Value::Int32(1)));

    // A wrong-thread write queues on top of the applied affine value.
    SetResult r = SetResult::NO_CHANGE;
    setFromOtherThread(obj, PropertyId::of("W"), Value::Int32(7), &r);
    SHOULD(ShouldlyString(r).ShouldBe(SetResult::QUEUED));
    SHOULD(ShouldlyString(obj.Get(PropertyId::of("W"))).ShouldBe(Value::Int32(1))); // not yet applied

    // Drain applies the queued write over the affine value.
    obj.ProcessPendingChanges();
    SHOULD(ShouldlyString(obj.Get(PropertyId::of("W"))).ShouldBe(Value::Int32(7)));
    SHOULD(ShouldlyString(obj.pending_change_count()).ShouldBe(0u));
}
