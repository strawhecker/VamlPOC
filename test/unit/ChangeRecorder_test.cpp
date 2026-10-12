// S038 — ChangeRecorder tests: pending per-property changes for deferred
// notification (dep:S037). CONSUMES the PropertyId name-token (S033) and Value (S030).
//
// Proves the S038 title contract:
//   - record + pending_count/has_pending: a new property records, the distinct-property
//     count increments, has_pending is true; distinct properties coexist.
//   - per-property COALESCING: two records for the same property before a flush => ONE
//     pending notification, FIRST old kept + LATEST new adopted (S042 realized-early).
//   - flush order: emits once per property in FIRST-RECORD order with the correct
//     (old,new); returns the count; clears the recorder.
//   - record during flush: a notify handler that re-record()s lands in fresh state
//     (not double-emitted this pass) — snapshot-then-emit (mirrors S037's swap).
//   - clear: drops all pending; flush after clear returns 0.
//   - empty flush: returns 0, callback invoked for nothing.
//
// ChangeRecorder is deliberately SINGLE-THREADED (pipeline-internal deferral device);
// the cross-thread ChangeQueue/mutex is S041's job (recorded in ChangeRecorder.h,
// NOT silent). No threads are used here.

// doctest's main comes from smoke_test.cpp (DO doctest IMPLEMENT_WITH_MAIN).
// This TU only declares tests — must NOT re-define implementation.
#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"       // S036_2 StringMaker + S036_4 operator<<
#include <vaml/assert/Shouldly.h>         // ShouldlyString / ShouldThrow (SHOULD seam, S036_1)

#include "vaml/core/ChangeRecorder.h"

#include <string>
#include <utility>
#include <vector>

using vaml::ChangeRecorder;
using vaml::PropertyId;
using vaml::Value;

namespace {

// Collects the flushed notifications into a sequence of (name, old, new).
struct FlushTrace
{
    std::vector<std::string> nameOrder; // first-record flush order
    std::vector<Value>       oldValues;
    std::vector<Value>       newValues;

    ChangeRecorder::NotifyFn fn()
    {
        return [this](PropertyId id, const Value& o, const Value& n) {
            nameOrder.emplace_back(id.name());
            oldValues.push_back(o);
            newValues.push_back(n);
        };
    }
};

// An Int32 (old,new) observation for compact assertions.
struct Int32Pair
{
    std::int32_t oldV = -1;
    std::int32_t newV = -1;
};

} // namespace

TEST_CASE("S038: record — pending_count increments, has_pending true, distinct props coexist")
{
    ChangeRecorder rec;
    SHOULD(ShouldlyString(rec.pending_count()).ShouldBe(0u));
    SHOULD(ShouldlyString(rec.has_pending(PropertyId::of("Width"))).ShouldBe(false));

    rec.record(PropertyId::of("Width"), Value::Int32(1), Value::Int32(5));
    SHOULD(ShouldlyString(rec.pending_count()).ShouldBe(1u));
    SHOULD(ShouldlyString(rec.has_pending(PropertyId::of("Width"))).ShouldBe(true));

    // A DISTINCT property coexists.
    rec.record(PropertyId::of("Height"), Value::Int32(2), Value::Int32(9));
    SHOULD(ShouldlyString(rec.pending_count()).ShouldBe(2u));
    SHOULD(ShouldlyString(rec.has_pending(PropertyId::of("Height"))).ShouldBe(true));
    SHOULD(ShouldlyString(rec.has_pending(PropertyId::of("Width"))).ShouldBe(true));
}

TEST_CASE("S038: per-property coalescing — first old kept, latest new adopted")
{
    ChangeRecorder rec;
    rec.record(PropertyId::of("W"), Value::Int32(1), Value::Int32(5));
    rec.record(PropertyId::of("W"), Value::Int32(5), Value::Int32(8)); // coalesce

    // One pending notification, not two.
    SHOULD(ShouldlyString(rec.pending_count()).ShouldBe(1u));

    FlushTrace trace;
    SHOULD(ShouldlyString(rec.flush(trace.fn())).ShouldBe(1u));
    // FIRST old (1) kept, LATEST new (8) adopted.
    SHOULD(ShouldlyString(trace.oldValues[0]).ShouldBe(Value::Int32(1)));
    SHOULD(ShouldlyString(trace.newValues[0]).ShouldBe(Value::Int32(8)));
}

TEST_CASE("S038: three records same prop — earliest old + newest new")
{
    ChangeRecorder rec;
    rec.record(PropertyId::of("C"), Value::Int32(0), Value::Int32(1));
    rec.record(PropertyId::of("C"), Value::Int32(1), Value::Int32(2));
    rec.record(PropertyId::of("C"), Value::Int32(2), Value::Int32(3));

    SHOULD(ShouldlyString(rec.pending_count()).ShouldBe(1u));

    FlushTrace trace;
    SHOULD(ShouldlyString(rec.flush(trace.fn())).ShouldBe(1u));
    SHOULD(ShouldlyString(trace.oldValues[0]).ShouldBe(Value::Int32(0))); // earliest old
    SHOULD(ShouldlyString(trace.newValues[0]).ShouldBe(Value::Int32(3))); // newest new
}

TEST_CASE("S038: flush emits once per property in FIRST-RECORD order and clears")
{
    ChangeRecorder rec;
    rec.record(PropertyId::of("Width"), Value::Int32(0), Value::Int32(10));
    rec.record(PropertyId::of("Height"), Value::Int32(0), Value::Int32(20));
    rec.record(PropertyId::of("Depth"), Value::Int32(0), Value::Int32(30));

    FlushTrace trace;
    SHOULD(ShouldlyString(rec.flush(trace.fn())).ShouldBe(3u));

    // First-record order preserved (not hash order).
    SHOULD(ShouldlyString(trace.nameOrder.size()).ShouldBe(3u));
    SHOULD(ShouldlyString(trace.nameOrder[0]).ShouldBe(std::string("Width")));
    SHOULD(ShouldlyString(trace.nameOrder[1]).ShouldBe(std::string("Height")));
    SHOULD(ShouldlyString(trace.nameOrder[2]).ShouldBe(std::string("Depth")));
    SHOULD(ShouldlyString(trace.oldValues[0]).ShouldBe(Value::Int32(0)));
    SHOULD(ShouldlyString(trace.newValues[0]).ShouldBe(Value::Int32(10)));
    SHOULD(ShouldlyString(trace.newValues[1]).ShouldBe(Value::Int32(20)));
    SHOULD(ShouldlyString(trace.newValues[2]).ShouldBe(Value::Int32(30)));

    // Recorder cleared after flush.
    SHOULD(ShouldlyString(rec.pending_count()).ShouldBe(0u));
    SHOULD(ShouldlyString(rec.has_pending(PropertyId::of("Width"))).ShouldBe(false));

    // A second flush has nothing left.
    FlushTrace trace2;
    SHOULD(ShouldlyString(rec.flush(trace2.fn())).ShouldBe(0u));
    SHOULD(ShouldlyString(trace2.nameOrder.size()).ShouldBe(0u));
}

TEST_CASE("S038: record during flush lands in fresh state — not double-emitted this pass")
{
    ChangeRecorder rec;
    rec.record(PropertyId::of("A"), Value::Int32(0), Value::Int32(1));

    // A notify handler that re-record()s the same property during the flush.
    int emitted = 0;
    auto fn = [&](PropertyId id, const Value&, const Value&) {
        ++emitted;
        if (emitted == 1)
        {
            rec.record(id, Value::Int32(1), Value::Int32(2)); // re-record during flush
        }
    };

    // Snapshot-then-emit: the re-record does NOT double-emit A in THIS pass.
    SHOULD(ShouldlyString(rec.flush(fn)).ShouldBe(1u));
    SHOULD(ShouldlyString(emitted).ShouldBe(1));

    // ...but it IS pending for a subsequent flush.
    SHOULD(ShouldlyString(rec.pending_count()).ShouldBe(1u));
    FlushTrace trace;
    SHOULD(ShouldlyString(rec.flush(trace.fn())).ShouldBe(1u));
    SHOULD(ShouldlyString(trace.oldValues[0]).ShouldBe(Value::Int32(1))); // the re-record
    SHOULD(ShouldlyString(trace.newValues[0]).ShouldBe(Value::Int32(2)));
}

TEST_CASE("S038: clear drops all pending; flush after clear returns 0")
{
    ChangeRecorder rec;
    rec.record(PropertyId::of("W"), Value::Int32(0), Value::Int32(1));
    rec.record(PropertyId::of("H"), Value::Int32(0), Value::Int32(2));
    SHOULD(ShouldlyString(rec.pending_count()).ShouldBe(2u));

    rec.clear();
    SHOULD(ShouldlyString(rec.pending_count()).ShouldBe(0u));
    SHOULD(ShouldlyString(rec.has_pending(PropertyId::of("W"))).ShouldBe(false));

    FlushTrace trace;
    SHOULD(ShouldlyString(rec.flush(trace.fn())).ShouldBe(0u));
    SHOULD(ShouldlyString(trace.nameOrder.size()).ShouldBe(0u));
}

TEST_CASE("S038: empty flush returns 0 and invokes nothing")
{
    ChangeRecorder rec;
    int called = 0;
    auto fn = [&](PropertyId, const Value&, const Value&) { ++called; };
    SHOULD(ShouldlyString(rec.flush(fn)).ShouldBe(0u));
    SHOULD(ShouldlyString(called).ShouldBe(0));
}

TEST_CASE("S038: owned-name (record survives the registering key) — coalescing by value equality of name")
{
    // A PropertyId is a non-owning std::string_view. The recorder must COPY the name,
    // so a record made from a short-lived string source stays correct (S036/S037 gotcha).
    ChangeRecorder rec;
    static const char W[] = "Width";
    {
        // Simulate a short-lived key source whose storage dies after this block.
        std::string temp(W);
        rec.record(PropertyId::of(temp), Value::Int32(0), Value::Int32(5));
    }

    // The same NAME (via a fresh, unrelated source) must COALESCE, not add a second.
    std::string fresh = "Width";
    rec.record(PropertyId::of(fresh), Value::Int32(5), Value::Int32(7));
    SHOULD(ShouldlyString(rec.pending_count()).ShouldBe(1u));

    FlushTrace trace;
    SHOULD(ShouldlyString(rec.flush(trace.fn())).ShouldBe(1u));
    SHOULD(ShouldlyString(trace.nameOrder[0]).ShouldBe(std::string("Width")));
    SHOULD(ShouldlyString(trace.oldValues[0]).ShouldBe(Value::Int32(0))); // first old kept
    SHOULD(ShouldlyString(trace.newValues[0]).ShouldBe(Value::Int32(7))); // latest new
}
