// S044 (dep:S043) — Attached set/get + inheritance + notification tests.
//
// Delivers the S044 headline end-to-end against live DependencyObjects:
//   "attached set/get + inheritance + notification."
//
// S043 made DependencyObject attached-aware by GLOBAL RESOLUTION: Set/Get/
// GetEffectiveValue resolve a globally-registered DependencyProperty even when
// the object has not locally registered it (a borrowed non-owning metadata
// pointer; owner != class). S044 (THIS file) proves the full story on the
// attached axis AND closes a real gap found while tracing the headline:
//
//   S041's descendant push captured a descendant ONLY when it already had a
//   local store entry (store.find(name) != end()). For an attached Inherits
//   property, a PURELY-PASSIVE inheriting child (never registers, never Set,
//   never materializes a borrowed entry) reads the inherited value correctly
//   via GetEffectiveValue (S043) but was NEVER captured -> NEVER notified when
//   the parent's attached Inherits value changed.
//
//   S044 broadens the push walk (DependencyObject.cpp, visit_scope /
//   collect_descendant_effectives): a descendant is captured when it registers
//   the property OR when the property is a globally-registered attached
//   Inherits/InheritOnly property (contextual captureAttachedInherits flag).
//   notify_descendants still filters to real change (old != new), so
//   over-capturing a passive descendant that ends up unchanged is harmless —
//   shielded / local-wins descendants still produce zero notifications.
//
// Proves:
//   1. Attached-inherits cascade (headline): an attached Inherits property
//      (owner A) never locally registered on parent/child; parent.Set(5) then
//      parent.Set(9) — the child's effective value follows each move (Get /
//      unattached read).
//   2. Attached inherited + local-wins: child Set its own attached value first
//      (local-wins); a later parent.Set(44) does NOT clobber the child; the
//      child's effective stays its own value.
//   3. Passive descendant IS notified (the S044 gap-fix under test): child
//      subscribes; parent.Set(8) -> child.ProcessNotificationQueue() fires
//      exactly once (old=0, new=8) on a child that NEVER materialized a store
//      entry. (Fails pre-S044, passes post-S044.)
//   4. Attached coalescing: parent.Set(5) then parent.Set(8) before one child
//      drain -> raw queue depth 2, ONE notification (0->8) first-old-kept /
//      latest-new-adopted (S042 on the attached axis).
//   5. Wrong-thread attached Inherits: a non-affine parent.Set(9) -> QUEUED,
//      pending 1, child not yet notified; affinity ProcessPendingChanges ->
//      applied, ProcessNotificationQueue -> child notified (old, new).
//   6. Owner != class + shadowing preserved: attached owner A read back via
//      owner_type_id() while plain objects store; a locally-registered same-name
//      shadows the global path; an unregistered name still throws
//      PropertyNotFound.
//
// Uses the Shouldly-for-doctest vocabulary (S036_5 standing convention).

// doctest's main comes from smoke_test.cpp.
#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/DependencyObject.h"
#include "vaml/core/DependencyProperty.h"
#include "vaml/core/PropertyId.h"
#include "vaml/core/PropertyKey.h"
#include "vaml/core/PropertyRegistry.h" // PropertyNotFound
#include "vaml/core/Value.h"

#include <cstdint>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

using vaml::DependencyObject;
using vaml::DependencyProperty;
using vaml::PropertyFlags;
using vaml::PropertyId;
using vaml::PropertyKey;
using vaml::PropertyMetadata;
using vaml::PropertyNotFound;
using vaml::SetResult;
using vaml::Value;
using vaml::ValueType;
using vaml::test::ShouldThrow;

namespace {

// The GLOBAL DependencyProperty table is name-unique (S039). Each S044 name
// carries a unique suffix so no cross-TEST_CASE / cross-TU collision is possible;
// the set guards against accidental reuse within this TU.
std::unordered_set<std::string>& global_seen()
{
    static std::unordered_set<std::string> seen;
    return seen;
}

// Registers an ATTACHED property GLOBALLY (once per name): the key is owned by
// `owner` but NO object locally registers it — the true S043/S044 attached
// premise (owner != storage class). A local override does NOT exist until a
// Set() materializes a borrowed store entry.
const DependencyProperty& registerAttachedGlobal(const std::string& name, const std::string& owner,
                                                 PropertyFlags flags, Value def)
{
    if (global_seen().insert(name).second)
    {
        return DependencyProperty::Register(name, ValueType::Int32, owner,
                                            PropertyMetadata::WithDefault(def, flags));
    }
    return DependencyProperty::Get(name);
}

constexpr PropertyFlags kInh = PropertyFlags::Inherits;

// A non-affine Set() executed on a fresh thread; the result captured back by a
// join barrier (mirrors S037 / S043's marshalling harness).
void setFromOtherThread(DependencyObject& obj, const PropertyId& id, Value v, SetResult* out)
{
    std::thread t([&] { *out = obj.Set(id, v); });
    t.join();
}

// Collects fired subscriber notifications into (name, old, new) Int32 triples —
// the same trace harness as S042's Coalescing proof.
struct SubTrace
{
    std::vector<std::string> names;
    std::vector<std::int32_t> olds;
    std::vector<std::int32_t> news;

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

TEST_CASE("S044: attached-inherits cascade — child effective follows each parent move")
{
    // THE S044 headline on the attached axis. An attached Inherits property
    // (owner A) is NEVER locally registered on the parent or the child — both
    // are pure consumers of the global descriptor.
    const std::string name = "AttachedTheme_S044_1";
    (void)registerAttachedGlobal(name, "AppOwner_A", kInh, Value::Int32(0));

    DependencyObject parent, child;
    child.set_parent(&parent);

    // Orphan read -> global default, and child inherits that default (0).
    SHOULD(ShouldlyString(child.Get(PropertyId::of(name))).ShouldBe(Value::Int32(0)));

    // parent.Set(5) — attached write materializes a borrowed entry on the parent.
    SHOULD(ShouldlyString(parent.Set(PropertyId::of(name), Value::Int32(5))).ShouldBe(SetResult::APPLIED));
    // Child, unattached (no store entry), INHERITS the parent's 5.
    SHOULD(ShouldlyString(child.Get(PropertyId::of(name))).ShouldBe(Value::Int32(5)));

    // parent.Set(9) — the child's effective follows the new move (9).
    SHOULD(ShouldlyString(parent.Set(PropertyId::of(name), Value::Int32(9))).ShouldBe(SetResult::APPLIED));
    SHOULD(ShouldlyString(child.Get(PropertyId::of(name))).ShouldBe(Value::Int32(9)));
}

TEST_CASE("S044: attached inherited + local-wins — child override not clobbered")
{
    // A child that Sets its OWN attached value first becomes a local-wins node:
    // a later parent change must NOT clobber it (S040 local-wins on the attached
    // axis).
    const std::string name = "AttachedOpacity_S044_2";
    (void)registerAttachedGlobal(name, "AppOwner_A", kInh, Value::Int32(0));

    DependencyObject parent, child;
    child.set_parent(&parent);

    // Child sets its own attached value FIRST -> local override materialized.
    SHOULD(ShouldlyString(child.Set(PropertyId::of(name), Value::Int32(7))).ShouldBe(SetResult::APPLIED));
    SHOULD(ShouldlyString(child.Get(PropertyId::of(name))).ShouldBe(Value::Int32(7)));

    // A later parent.Set(44) must NOT clobber the child (local-wins).
    (void)parent.Set(PropertyId::of(name), Value::Int32(44));
    SHOULD(ShouldlyString(child.Get(PropertyId::of(name))).ShouldBe(Value::Int32(7)));
}

TEST_CASE("S044: passive attached-Inherits descendant IS notified (gap-fix under test)")
{
    // S044's product gap-fix in action: a PURELY-PASSIVE child (never registers,
    // never Set, never materializes a store entry) that INHERITS an attached
    // Inherits property must be NOTIFIED when the parent's value changes. This
    // failed before S044's push-walk broadening and passes after.
    const std::string name = "AttachedBrush_S044_3";
    (void)registerAttachedGlobal(name, "AppOwner_A", kInh, Value::Int32(0));

    DependencyObject parent, child;
    child.set_parent(&parent); // child is passive — no Set, no register ever

    SubTrace trace;
    (void)child.add_property_changed(PropertyId::of(name), trace.handler());

    // Parent's attached Inherits write pushes to the passive child.
    SHOULD(ShouldlyString(parent.Set(PropertyId::of(name), Value::Int32(8))).ShouldBe(SetResult::APPLIED));
    SHOULD(ShouldlyString(child.pending_notification_count()).ShouldBe(1u));

    // Child drain fires EXACTLY once, old=0 (inherited default) -> new=8.
    child.ProcessNotificationQueue();
    SHOULD(ShouldlyString(trace.names.size()).ShouldBe(1u));
    SHOULD(ShouldlyString(trace.names[0]).ShouldBe(std::string(name)));
    SHOULD(ShouldlyString(trace.olds[0]).ShouldBe(0));
    SHOULD(ShouldlyString(trace.news[0]).ShouldBe(8));
    SHOULD(ShouldlyString(child.pending_notification_count()).ShouldBe(0u));
}

TEST_CASE("S044: attached coalescing — two Sets before one drain == one notification")
{
    // S042's coalescing contract on the attached axis: two attached Inherits
    // Sets BEFORE the passive child's drain collapse to ONE notification, with
    // first-old kept and latest-new adopted.
    const std::string name = "AttachedCoalesce_S044_4";
    (void)registerAttachedGlobal(name, "AppOwner_A", kInh, Value::Int32(0));

    DependencyObject parent, child;
    child.set_parent(&parent); // passive child

    SubTrace trace;
    (void)child.add_property_changed(PropertyId::of(name), trace.handler());

    (void)parent.Set(PropertyId::of(name), Value::Int32(5)); // push (0 -> 5)
    (void)parent.Set(PropertyId::of(name), Value::Int32(8)); // push (5 -> 8)

    // RAW depth 2: coalescing happens AT DRAIN, not enqueue.
    SHOULD(ShouldlyString(child.pending_notification_count()).ShouldBe(2u));

    // ONE drain collapses to ONE notification, first old (0) kept / latest new (8).
    child.ProcessNotificationQueue();
    SHOULD(ShouldlyString(trace.names.size()).ShouldBe(1u));
    SHOULD(ShouldlyString(trace.names[0]).ShouldBe(std::string(name)));
    SHOULD(ShouldlyString(trace.olds[0]).ShouldBe(0));
    SHOULD(ShouldlyString(trace.news[0]).ShouldBe(8));
    SHOULD(ShouldlyString(child.pending_notification_count()).ShouldBe(0u));
}

TEST_CASE("S044: wrong-thread attached Inherits -> QUEUED; drain applies -> descendant notified")
{
    // S037 marshalling on the attached-Inherits axis: a NON-affine parent Set is
    // QUEUED (not applied, child not yet notified); the affinity-thread drain
    // applies it (ProcessPendingChanges) which then pushes to the passive child;
    // the child's drain (ProcessNotificationQueue) fires (old, new).
    const std::string name = "AttachedMarshalled_S044_5";
    (void)registerAttachedGlobal(name, "AppOwner_A", kInh, Value::Int32(0));

    DependencyObject parent, child;
    child.set_parent(&parent); // passive child

    SubTrace trace;
    (void)child.add_property_changed(PropertyId::of(name), trace.handler());

    // Non-affine attached Inherits Set -> QUEUED, NOT applied.
    SetResult r = SetResult::NO_CHANGE;
    setFromOtherThread(parent, PropertyId::of(name), Value::Int32(9), &r);
    SHOULD(ShouldlyString(r).ShouldBe(SetResult::QUEUED));
    SHOULD(ShouldlyString(parent.pending_change_count()).ShouldBe(1u));
    SHOULD(ShouldlyString(parent.Get(PropertyId::of(name))).ShouldBe(Value::Int32(0)));
    // Child not yet notified.
    SHOULD(ShouldlyString(child.pending_notification_count()).ShouldBe(0u));

    // Affinity-thread drain applies the queued attached write -> pushes to child.
    parent.ProcessPendingChanges();
    SHOULD(ShouldlyString(parent.pending_change_count()).ShouldBe(0u));
    SHOULD(ShouldlyString(parent.Get(PropertyId::of(name))).ShouldBe(Value::Int32(9)));
    SHOULD(ShouldlyString(child.pending_notification_count()).ShouldBe(1u));

    // Child drain fires (0 -> 9).
    child.ProcessNotificationQueue();
    SHOULD(ShouldlyString(trace.names.size()).ShouldBe(1u));
    SHOULD(ShouldlyString(trace.olds[0]).ShouldBe(0));
    SHOULD(ShouldlyString(trace.news[0]).ShouldBe(9));
    SHOULD(ShouldlyString(child.pending_notification_count()).ShouldBe(0u));
}

TEST_CASE("S044: owner != class preserved; local registration shadows global; unregistered throws")
{
    // The attached authority (owner A) is preserved; a LOCAL registration of the
    // same name still shadows the global path (backward compat, S043); an
    // unregistered name still throws PropertyNotFound.
    const std::string name = "AttachedAccent_S044_6";
    const DependencyProperty& dp =
        registerAttachedGlobal(name, "AccentOwner_A", kInh, Value::Int32(0));

    // Attached owner read back via owner_type_id() — A, while a DIFFERENT host
    // stores the value (owner != class preserved on the inheriting axis too).
    SHOULD(ShouldlyString(dp.owner_type_id()).ShouldBe(std::string("AccentOwner_A")));

    DependencyObject parent, child;
    child.set_parent(&parent);
    (void)parent.Set(PropertyId::of(name), Value::Int32(3));
    SHOULD(ShouldlyString(child.Get(PropertyId::of(name))).ShouldBe(Value::Int32(3)));
    SHOULD(ShouldlyString(dp.owner_type_id()).ShouldBe(std::string("AccentOwner_A")));

    // A LOCAL registration of the same NAME shadows the global path -> the local
    // object's own default (99) governs, not the global descriptor's.
    DependencyObject b;
    PropertyKey localKey(name, ValueType::Int32, "LocalOwner_B", PropertyFlags::None);
    (void)b.register_property(std::move(localKey),
                              PropertyMetadata::WithDefault(Value::Int32(99), PropertyFlags::None));
    SHOULD(ShouldlyString(b.Get(PropertyId::of(name))).ShouldBe(Value::Int32(99)));

    // An UNREGISTERED (not local, not global) name still throws PropertyNotFound.
    DependencyObject c;
    SHOULD(ShouldThrow<PropertyNotFound>([&] {
        (void)c.Get(PropertyId::of("NoSuchAttached_S044_6"));
    }));
    SHOULD(ShouldThrow<PropertyNotFound>([&] {
        (void)c.Set(PropertyId::of("NoSuchAttached_S044_6"), Value::Int32(1));
    }));
}
