// S041 — ChangeQueue tests: per-thread pending notification queue (dep:S040,
// dep:S038, dep:S037). CONSUMES the PropertyId name-token (S033), Value (S030),
// and the affinity-thread contract (WrongThreadException, S037).
//
// Proves the S041 headline ("ChangeQueue (per-thread) + processor drains in
// registration order") at the QUEUE primitive level:
//   - enqueue from any thread (cross-thread postbox): a worker-thread enqueue
//     grows pending_count; the owner-thread drain processes it.
//   - FIFO enqueue order preserved (a deque, never unordered_map order).
//   - enqueue is cross-thread (mutex); process()/clear() are affinity-thread-only
//     (WrongThreadException from a non-affine thread — mirrors S037 drain).
//   - swap-out: a record during process() lands in a FRESH queue (not
//     double-emitted this pass), mirroring S037/S038.
//   - empty drain returns 0.
//   - owned-name gotcha: the queued item outlives the registering key (an item
//     enqueued from a short-lived PropertyId source is replayed correctly).
//
// ChangeQueue carries notifications with NO type semantics; the cross-thread
// contract mirrors S037 (only the mutex-protected queue is cross-thread).

// doctest's main comes from smoke_test.cpp (DO doctest IMPLEMENT_WITH_MAIN).
// This TU only declares tests — must NOT re-define implementation.
#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"       // S036_2 StringMaker + S036_4 operator<<
#include <vaml/assert/Shouldly.h>         // ShouldlyString / ShouldThrow (SHOULD seam, S036_1)

#include "vaml/core/ChangeQueue.h"

#include <string>
#include <thread>
#include <vector>

using vaml::ChangeQueue;
using vaml::PropertyId;
using vaml::Value;
using vaml::WrongThreadException;
using vaml::test::ShouldThrow;

namespace {

// An Int32 (old,new) observation.
struct Int32Pair
{
    std::int32_t oldV = -1;
    std::int32_t newV = -1;
};

// Collects drained notifications into a sequence of (name, old, new).
struct Trace
{
    std::vector<std::string> names;
    std::vector<std::int32_t> olds;
    std::vector<std::int32_t> news;

    ChangeQueue::NotifyFn fn()
    {
        return [this](PropertyId id, const Value& o, const Value& n) {
            names.emplace_back(id.name());
            olds.push_back(o.get<std::int32_t>());
            news.push_back(n.get<std::int32_t>());
        };
    }
};

// Enqueues `value` from a fresh thread (definitely NOT the queue's affinity
// thread) and returns via a join barrier — the cross-thread enqueue demon.
void enqueueFromOtherThread(ChangeQueue& q, const PropertyId& id, Value oldV, Value newV)
{
    std::thread t([&] { q.enqueue(id, oldV, newV); });
    t.join();
}

} // namespace

TEST_CASE("S041: enqueue is cross-thread — worker enqueue, owner-thread drain")
{
    ChangeQueue q;
    SHOULD(ShouldlyString(q.pending_count()).ShouldBe(0u));

    // Worker-thread enqueue (cross-thread postbox; the queue lives on `this`
    // thread = its affinity thread).
    enqueueFromOtherThread(q, PropertyId::of("Width"), Value::Int32(0), Value::Int32(10));
    SHOULD(ShouldlyString(q.pending_count()).ShouldBe(1u));

    Trace trace;
    SHOULD(ShouldlyString(q.process(trace.fn())).ShouldBe(1u));
    SHOULD(ShouldlyString(trace.names[0]).ShouldBe(std::string("Width")));
    SHOULD(ShouldlyString(trace.olds[0]).ShouldBe(0));
    SHOULD(ShouldlyString(trace.news[0]).ShouldBe(10));
    SHOULD(ShouldlyString(q.pending_count()).ShouldBe(0u));
}

TEST_CASE("S041: FIFO enqueue order preserved through the drain")
{
    ChangeQueue q;
    q.enqueue(PropertyId::of("A"), Value::Int32(0), Value::Int32(1));
    q.enqueue(PropertyId::of("B"), Value::Int32(0), Value::Int32(2));
    q.enqueue(PropertyId::of("C"), Value::Int32(0), Value::Int32(3));

    Trace trace;
    SHOULD(ShouldlyString(q.process(trace.fn())).ShouldBe(3u));
    SHOULD(ShouldlyString(trace.names.size()).ShouldBe(3u));
    SHOULD(ShouldlyString(trace.names[0]).ShouldBe(std::string("A")));
    SHOULD(ShouldlyString(trace.names[1]).ShouldBe(std::string("B")));
    SHOULD(ShouldlyString(trace.names[2]).ShouldBe(std::string("C")));
    SHOULD(ShouldlyString(q.pending_count()).ShouldBe(0u));
}

TEST_CASE("S041: empty drain returns 0 and invokes nothing")
{
    ChangeQueue q;
    int called = 0;
    auto fn = [&](PropertyId, const Value&, const Value&) { ++called; };
    SHOULD(ShouldlyString(q.process(fn)).ShouldBe(0u));
    SHOULD(ShouldlyString(called).ShouldBe(0));
}

TEST_CASE("S041: process from a non-affine thread throws WrongThreadException")
{
    ChangeQueue q;
    q.enqueue(PropertyId::of("W"), Value::Int32(0), Value::Int32(9));
    SHOULD(ShouldlyString(q.pending_count()).ShouldBe(1u));

    bool caught = false;
    std::thread t([&] {
        try
        {
            q.process([](PropertyId, const Value&, const Value&) {});
        }
        catch (WrongThreadException const&)
        {
            caught = true;
        }
    });
    t.join();

    SHOULD(ShouldlyString(caught).ShouldBe(true));
    SHOULD(ShouldlyString(q.pending_count()).ShouldBe(1u)); // still pending, not drained
}

TEST_CASE("S041: clear from a non-affine thread throws WrongThreadException")
{
    ChangeQueue q;
    q.enqueue(PropertyId::of("W"), Value::Int32(0), Value::Int32(9));

    bool caught = false;
    std::thread t([&] {
        try
        {
            q.clear();
        }
        catch (WrongThreadException const&)
        {
            caught = true;
        }
    });
    t.join();

    SHOULD(ShouldlyString(caught).ShouldBe(true));
    SHOULD(ShouldlyString(q.pending_count()).ShouldBe(1u)); // still pending
}

TEST_CASE("S041: record during process lands in a fresh queue — not double-emitted this pass")
{
    ChangeQueue q;
    q.enqueue(PropertyId::of("A"), Value::Int32(0), Value::Int32(1));

    int emitted = 0;
    auto fn = [&](PropertyId id, const Value&, const Value&) {
        ++emitted;
        if (emitted == 1)
        {
            q.enqueue(id, Value::Int32(1), Value::Int32(2)); // re-enqueue during drain
        }
    };

    // Snapshot-then-run (swap-out): the re-enqueue does NOT double-emit A THIS pass.
    SHOULD(ShouldlyString(q.process(fn)).ShouldBe(1u));
    SHOULD(ShouldlyString(emitted).ShouldBe(1));

    // ...but it IS pending for a subsequent drain.
    SHOULD(ShouldlyString(q.pending_count()).ShouldBe(1u));
    Trace trace;
    SHOULD(ShouldlyString(q.process(trace.fn())).ShouldBe(1u));
    SHOULD(ShouldlyString(trace.olds[0]).ShouldBe(1));
    SHOULD(ShouldlyString(trace.news[0]).ShouldBe(2));
}

TEST_CASE("S041: clear drops all pending; drain after clear returns 0")
{
    ChangeQueue q;
    q.enqueue(PropertyId::of("W"), Value::Int32(0), Value::Int32(1));
    q.enqueue(PropertyId::of("H"), Value::Int32(0), Value::Int32(2));
    SHOULD(ShouldlyString(q.pending_count()).ShouldBe(2u));

    q.clear();
    SHOULD(ShouldlyString(q.pending_count()).ShouldBe(0u));

    Trace trace;
    SHOULD(ShouldlyString(q.process(trace.fn())).ShouldBe(0u));
    SHOULD(ShouldlyString(trace.names.size()).ShouldBe(0u));
}

TEST_CASE("S041: owned-name gotcha — replayed item survives a short-lived key source")
{
    ChangeQueue q;
    static const char W[] = "Width";
    {
        // A short-lived key source whose storage dies after this block.
        std::string temp(W);
        q.enqueue(PropertyId::of(temp), Value::Int32(0), Value::Int32(5));
    }
    // The drain must replay the CORRECT name and values even though the source
    // key storage is long gone.
    Trace trace;
    SHOULD(ShouldlyString(q.process(trace.fn())).ShouldBe(1u));
    SHOULD(ShouldlyString(trace.names[0]).ShouldBe(std::string("Width")));
    SHOULD(ShouldlyString(trace.olds[0]).ShouldBe(0));
    SHOULD(ShouldlyString(trace.news[0]).ShouldBe(5));
}
