// 11n-4 (main, step 2) — the IWeakEventListener weak-lifetime contract (vaml::core).
//
// The DAG anchor `IWeakEventListener`: a binding / collection-view subscribes to
// its source WEAKLY so the source can be collected while the binding lives.
// Proves the WeakEventListenerBridge:
//   (A) a LIVE listener receives ReceiveWeakEvent with the manager tag + payload;
//   (B) a listener that is DESTROYED no longer fires (weak handle expires —
//       no dangling call, no strong reference keeps it alive);
//   (C) the bridge reports alive() truthfully (expires on listener death).
//
// ADR-018: ZERO new locks (single-threaded affinity; the weak bridge is
// invoked on the affinity thread).

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include <vaml/core/IWeakEventListener.h>
#include <vaml/core/WeakEventListener.h>

#include <memory>
#include <string>

using vaml::IWeakEventListener;
using vaml::Value;
using vaml::WeakEventListenerBridge;
using vaml::WeakEventManagerTag;

namespace {
// A listener that records the manager tag + payload it received.
struct RecordingListener : public IWeakEventListener
{
    WeakEventManagerTag lastTag;
    Value lastArgs = Value::Null();
    bool fired = false;

    bool ReceiveWeakEvent(const WeakEventManagerTag& tag, const void* /*sender*/, const Value& args) override
    {
        lastTag = tag;
        lastArgs = args;
        fired = true;
        return true;
    }
};
} // namespace

// ---------------------------------------------------------------------------
// A. Live listener receives the weak event
// ---------------------------------------------------------------------------

TEST_CASE("11n-4: a live IWeakEventListener receives ReceiveWeakEvent via the bridge")
{
    auto listener = std::make_shared<RecordingListener>();
    WeakEventListenerBridge bridge(listener);
    SHOULD(ShouldlyIntegral(bridge.alive() ? 1 : 0).ShouldBe(1));

    const bool delivered = bridge.fire(vaml::kPropertyChangedManager, nullptr, Value::String("Name"));
    SHOULD(ShouldlyIntegral(delivered ? 1 : 0).ShouldBe(1));
    SHOULD(ShouldlyIntegral(listener->fired ? 1 : 0).ShouldBe(1));
    SHOULD(ShouldlyString(listener->lastTag).ShouldBe(std::string(vaml::kPropertyChangedManager)));
    SHOULD(ShouldlyString(listener->lastArgs.get<std::string>()).ShouldBe(std::string("Name")));
}

// ---------------------------------------------------------------------------
// B + C. Collected listener no longer fires (weak lifetime)
// ---------------------------------------------------------------------------

TEST_CASE("11n-4: a collected IWeakEventListener is dropped (weak lifetime — no leak)")
{
    auto listener = std::make_shared<RecordingListener>();
    WeakEventListenerBridge bridge(listener);

    // fire while alive
    (void)bridge.fire(vaml::kCollectionChangedManager, nullptr, Value::Int32(1));
    SHOULD(ShouldlyIntegral(listener->fired ? 1 : 0).ShouldBe(1));

    // destroy the ONLY strong owner of the listener (the bridge holds only a weak ref)
    listener.reset();
    SHOULD(ShouldlyIntegral(bridge.alive() ? 1 : 0).ShouldBe(0));

    // firing a dead listener is a safe no-op (returns false — nothing kept alive)
    const bool delivered = bridge.fire(vaml::kCollectionChangedManager, nullptr, Value::Int32(2));
    SHOULD(ShouldlyIntegral(delivered ? 1 : 0).ShouldBe(0));
}
