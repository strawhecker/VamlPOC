// S042 — Coalescing integration gate (dep:S041, dep:S038).
// Headline: "two sets of the same prop before drain == one notification (test)".
//
// S038 delivered the primitive: ChangeRecorder coalesces per-property by design
// (FIRST old kept, LATEST new adopted), tested in ChangeRecorder_test.cpp.
// S041 wired it end-to-end into ChangeProcessor + the DependencyObject
// notification queue + ProcessNotificationQueue against live DependencyObjects.
// S042 (THIS file) is the formal integration proof at the LIVE-OBJECT boundary:
// two Set() calls on an Inherits property BEFORE the child's
// ProcessNotificationQueue() drain collapse to ONE subscriber notification, with
// first-old kept and latest-new adopted.
//
// Why a dedicated TU rather than extending the S041 file:
//   - S041 proved coalescing on the STANDALONE ChangeProcessor (enqueue ->
//     process) and proved live descendant-push only with a DRAIN BETWEEN Sets
//     (two *sequential* notifications). It never proved the exact S042 headline:
//     two LIVE Sets before ONE drain coalesce through the descendant-push + per-
//     object notification queue. That is this file's singular theme.
//   - It mirrors the S040 precedent (each proof owns its own file) with a clean
//     diff surface per step and no risk of editing a green S041 proof.
//
// Proves (all against live DependencyObjects, Inherits properties, through the
// real descendant-push -> per-object notification queue -> ProcessNotificationQueue
// path):
//   1. Headline: two Sets of the same prop before one drain == one notification.
//      Raw queue depth is 2 (coalescing happens AT DRAIN, not enqueue); after
//      drain the subscriber fired EXACTLY ONCE with (old=0 first-kept, new=latest).
//   2. Three Sets -> one notification: earliest old + newest new.
//   3. Drain is the coalescing boundary: a drain between Sets yields two
//      sequential notifications, each its own delta (no cross-drain collapse).
//   4. Distinct props do NOT coalesce with each other (two notifications in
//      first-record order, each property's own delta).
//   5. A no-op (NO_CHANGE) Set does not inflate the queue.
//   6. Aggregate capstone: exactly one notification per property across an
//      interleaved sequence.
//
// Uses the Shouldly-for-doctest vocabulary (SHOULD / ShouldThrow), mirroring
// ChangeNotificationPipeline_test.cpp's hostInherited() global-descriptor harness.

// doctest's main comes from smoke_test.cpp.
#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

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

namespace {

// The GLOBAL DependencyProperty table is name-unique (S039). Track names so each
// inherited property's global descriptor is written exactly once per suite. All
// S042 names are S042-UNIQUE (Coalesce_S042_*) so this suite can never collide
// with another TU's global descriptors.
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

TEST_CASE("S042: two live Sets of the same prop before one drain == one notification")
{
    // THE S042 headline, end-to-end against live DependencyObjects: a real
    // parent->child Inherits push, two Sets, ONE drain, ONE notification —
    // first old (0) kept, latest new (8) adopted.
    DependencyObject parent, child;
    const std::string name = "Coalesce_S042_1";
    hostInherited(parent, name, kInh, Value::Int32(0));
    hostInherited(child, name, kInh, Value::Int32(0));
    child.set_parent(&parent);

    SubTrace trace;
    (void)child.add_property_changed(PropertyId::of(name), trace.handler());

    // Two Sets before the drain — each pushes one entry into the child's queue.
    (void)parent.Set(PropertyId::of(name), Value::Int32(5)); // push (0 -> 5)
    (void)parent.Set(PropertyId::of(name), Value::Int32(8)); // push (5 -> 8)

    // RAW depth is 2: coalescing happens AT DRAIN, not at enqueue.
    SHOULD(ShouldlyString(child.pending_notification_count()).ShouldBe(2u));

    // ONE drain collapses the two raw records into ONE notification.
    child.ProcessNotificationQueue();

    SHOULD(ShouldlyString(trace.names.size()).ShouldBe(1u));
    SHOULD(ShouldlyString(trace.names[0]).ShouldBe(std::string(name)));
    // FIRST old kept (0), LATEST new adopted (8).
    SHOULD(ShouldlyString(trace.olds[0]).ShouldBe(0));
    SHOULD(ShouldlyString(trace.news[0]).ShouldBe(8));
    SHOULD(ShouldlyString(child.pending_notification_count()).ShouldBe(0u));
}

TEST_CASE("S042: three Sets -> one notification, earliest old + newest new")
{
    DependencyObject parent, child;
    const std::string name = "Coalesce_S042_2";
    hostInherited(parent, name, kInh, Value::Int32(0));
    hostInherited(child, name, kInh, Value::Int32(0));
    child.set_parent(&parent);

    SubTrace trace;
    (void)child.add_property_changed(PropertyId::of(name), trace.handler());

    (void)parent.Set(PropertyId::of(name), Value::Int32(5));
    (void)parent.Set(PropertyId::of(name), Value::Int32(7));
    (void)parent.Set(PropertyId::of(name), Value::Int32(9));

    // Raw depth 3 -> single drain -> ONE coalesced notification.
    SHOULD(ShouldlyString(child.pending_notification_count()).ShouldBe(3u));
    child.ProcessNotificationQueue();

    SHOULD(ShouldlyString(trace.names.size()).ShouldBe(1u));
    SHOULD(ShouldlyString(trace.olds[0]).ShouldBe(0)); // earliest old (0)
    SHOULD(ShouldlyString(trace.news[0]).ShouldBe(9)); // newest new (9)
    SHOULD(ShouldlyString(child.pending_notification_count()).ShouldBe(0u));
}

TEST_CASE("S042: drain is the coalescing boundary — no cross-drain collapse")
{
    // Set -> drain -> set -> drain yields TWO sequential notifications, each its
    // own delta (0->5 then 5->8). The drain separates coalescing scopes.
    DependencyObject parent, child;
    const std::string name = "Coalesce_S042_3";
    hostInherited(parent, name, kInh, Value::Int32(0));
    hostInherited(child, name, kInh, Value::Int32(0));
    child.set_parent(&parent);

    SubTrace trace;
    (void)child.add_property_changed(PropertyId::of(name), trace.handler());

    (void)parent.Set(PropertyId::of(name), Value::Int32(5));
    child.ProcessNotificationQueue();
    SHOULD(ShouldlyString(trace.names.size()).ShouldBe(1u));
    SHOULD(ShouldlyString(trace.olds[0]).ShouldBe(0));
    SHOULD(ShouldlyString(trace.news[0]).ShouldBe(5));

    (void)parent.Set(PropertyId::of(name), Value::Int32(8));
    child.ProcessNotificationQueue();
    SHOULD(ShouldlyString(trace.names.size()).ShouldBe(2u));
    SHOULD(ShouldlyString(trace.olds[1]).ShouldBe(5));
    SHOULD(ShouldlyString(trace.news[1]).ShouldBe(8));
    SHOULD(ShouldlyString(child.pending_notification_count()).ShouldBe(0u));
}

TEST_CASE("S042: distinct props do NOT coalesce with each other")
{
    // Two Inherits props A,B on one parent; an interleaved sequence then a single
    // drain. Each prop coalesces independently -> exactly TWO notifications, in
    // FIRST-RECORD order (A was first-recorded, so A before B).
    DependencyObject parent, child;
    const std::string aName = "Coalesce_A_S042_4";
    const std::string bName = "Coalesce_B_S042_4";
    hostInherited(parent, aName, kInh, Value::Int32(0));
    hostInherited(parent, bName, kInh, Value::Int32(0));
    hostInherited(child, aName, kInh, Value::Int32(0));
    hostInherited(child, bName, kInh, Value::Int32(0));
    child.set_parent(&parent);

    SubTrace trace;
    (void)child.add_property_changed(PropertyId::of(aName), trace.handler());
    (void)child.add_property_changed(PropertyId::of(bName), trace.handler());

    (void)parent.Set(PropertyId::of(aName), Value::Int32(1)); // A first-record
    (void)parent.Set(PropertyId::of(bName), Value::Int32(2)); // B first-record
    (void)parent.Set(PropertyId::of(aName), Value::Int32(3)); // coalesce A (0->3)
    child.ProcessNotificationQueue();

    // Two distinct-property notifications, first-record order: A then B.
    SHOULD(ShouldlyString(trace.names.size()).ShouldBe(2u));
    SHOULD(ShouldlyString(trace.names[0]).ShouldBe(std::string(aName)));
    SHOULD(ShouldlyString(trace.olds[0]).ShouldBe(0));
    SHOULD(ShouldlyString(trace.news[0]).ShouldBe(3));
    SHOULD(ShouldlyString(trace.names[1]).ShouldBe(std::string(bName)));
    SHOULD(ShouldlyString(trace.olds[1]).ShouldBe(0));
    // B was set only once (0->2), so the single new (2) is adopted.
    SHOULD(ShouldlyString(trace.news[1]).ShouldBe(2));
    SHOULD(ShouldlyString(child.pending_notification_count()).ShouldBe(0u));
}

TEST_CASE("S042: a NO_CHANGE Set does not inflate the queue")
{
    // parent.Set(5) then parent.Set(5): the second is NO_CHANGE (no push), so the
    // child's queue stays at depth 1 and one notification (0->5) is produced.
    DependencyObject parent, child;
    const std::string name = "Coalesce_S042_5";
    hostInherited(parent, name, kInh, Value::Int32(0));
    hostInherited(child, name, kInh, Value::Int32(0));
    child.set_parent(&parent);

    SubTrace trace;
    (void)child.add_property_changed(PropertyId::of(name), trace.handler());

    (void)parent.Set(PropertyId::of(name), Value::Int32(5));
    SHOULD(ShouldlyString(child.pending_notification_count()).ShouldBe(1u));

    // Second same-value Set: NO_CHANGE -> no push, queue depth unchanged.
    (void)parent.Set(PropertyId::of(name), Value::Int32(5));
    SHOULD(ShouldlyString(child.pending_notification_count()).ShouldBe(1u));

    child.ProcessNotificationQueue();
    SHOULD(ShouldlyString(trace.names.size()).ShouldBe(1u));
    SHOULD(ShouldlyString(trace.olds[0]).ShouldBe(0));
    SHOULD(ShouldlyString(trace.news[0]).ShouldBe(5));
    SHOULD(ShouldlyString(child.pending_notification_count()).ShouldBe(0u));
}

TEST_CASE("S042: aggregate — exactly one notification per property")
{
    // Mixed interleaved sequence: A=5,B=2,A=7,B=3 then one drain. Per-property
    // coalescing -> exactly TWO notifications (A 0->7, B 0->3 — each prop adopts
    // its LATEST new), never four.
    DependencyObject parent, child;
    const std::string aName = "Coalesce_A_S042_6";
    const std::string bName = "Coalesce_B_S042_6";
    hostInherited(parent, aName, kInh, Value::Int32(0));
    hostInherited(parent, bName, kInh, Value::Int32(0));
    hostInherited(child, aName, kInh, Value::Int32(0));
    hostInherited(child, bName, kInh, Value::Int32(0));
    child.set_parent(&parent);

    SubTrace trace;
    (void)child.add_property_changed(PropertyId::of(aName), trace.handler());
    (void)child.add_property_changed(PropertyId::of(bName), trace.handler());

    (void)parent.Set(PropertyId::of(aName), Value::Int32(5));
    (void)parent.Set(PropertyId::of(bName), Value::Int32(2));
    (void)parent.Set(PropertyId::of(aName), Value::Int32(7));
    (void)parent.Set(PropertyId::of(bName), Value::Int32(3));

    // 4 raw records -> single drain -> exactly 2 coalesced notifications.
    SHOULD(ShouldlyString(child.pending_notification_count()).ShouldBe(4u));
    child.ProcessNotificationQueue();

    SHOULD(ShouldlyString(trace.names.size()).ShouldBe(2u));
    SHOULD(ShouldlyString(trace.names[0]).ShouldBe(std::string(aName)));
    SHOULD(ShouldlyString(trace.olds[0]).ShouldBe(0));
    SHOULD(ShouldlyString(trace.news[0]).ShouldBe(7));
    SHOULD(ShouldlyString(trace.names[1]).ShouldBe(std::string(bName)));
    SHOULD(ShouldlyString(trace.olds[1]).ShouldBe(0));
    // B's delta chain is 0->2->3, so the LATEST new (3) is adopted.
    SHOULD(ShouldlyString(trace.news[1]).ShouldBe(3));
    SHOULD(ShouldlyString(child.pending_notification_count()).ShouldBe(0u));
}
