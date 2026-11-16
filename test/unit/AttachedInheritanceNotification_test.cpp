// S044 (dep:S043) — Attached set/get + inheritance + notification tests.
//
// Delivers the S044 headline end-to-end against live DependencyObjects:
//   "attached set/get + inheritance + notification."
//
// S043 made DependencyObject attached-aware by GLOBAL RESOLUTION: Set/Get/
// GetEffectiveValue resolve a globally-registered DependencyProperty even when
// the object has not locally registered it (a borrowed non-owning metadata
// pointer; owner != class). S044 (THIS file) proves the full story on the
// attached axis:
//
//   S041's fabricated descendant-push postbox (the captureAttachedInherits /
//   collect_descendant_effectives walk that captured a descendant ONLY when it
//   already had a local store entry) is RETRACTED by S811 — there is NO per-
//   object notification pipeline in WPF (DO.cs:1704/1732/2039). The S041/S044
//   behavior now reduces to the SINGLE `Inherits` flag + the S039 ancestor
//   PULL walk (S814 retired the second InheritOnly disposition axis):
//   a PURELY-PASSIVE inheriting child (never registers, never Set, never
//   materializes a borrowed entry) reads the inherited value correctly via
//   GetEffectiveValue (S043) with no child write; the ORIGIN's synchronous
//   metadata PropertyChangedCallback fires inside its Set on a real change.
//   A child with its OWN local value is never clobbered (WPF local-wins).
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
using vaml::WrongThreadException;
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
    return DependencyProperty::Get(name, owner);
}

constexpr PropertyFlags kInh = PropertyFlags::Inherits;

// A non-affine Set() executed on a fresh thread; the thrown WrongThreadException
// is captured by a join barrier (S811: a wrong-thread Set THROWS, WPF VerifyAccess).
void setFromOtherThreadThrows(DependencyObject& obj, const PropertyId& id, Value v,
                              std::exception_ptr& excOut)
{
    std::thread t([&] {
        try
        {
            (void)obj.Set(id, v);
        }
        catch (...)
        {
            excOut = std::current_exception();
        }
    });
    t.join();
}

// Collects fired SYNCHRONOUS metadata PropertyChangedCallback (with_changed)
// notifications into (name, old, new) Int32 triples. S811: the changed
// notification is the metadata callback fired inside Set (WPF DO.cs:2037/2039),
// NOT the removed per-object subscriber registry / descendant-push postbox.
struct SubTrace
{
    std::vector<std::string> names;
    std::vector<std::int32_t> olds;
    std::vector<std::int32_t> news;

    // A metadata property-changed callback that appends to this trace.
    PropertyMetadata::ChangedFunc changed(const char* name)
    {
        return [this, name](const Value& o, const Value& n) {
            names.emplace_back(name);
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
    SHOULD(ShouldlyString(child.Get(PropertyId::of(name, "AppOwner_A"))).ShouldBe(Value::Int32(0)));

    // parent.Set(5) — attached write materializes a borrowed entry on the parent.
    SHOULD(ShouldlyString(parent.Set(PropertyId::of(name, "AppOwner_A"), Value::Int32(5))).ShouldBe(SetResult::APPLIED));
    // Child, unattached (no store entry), INHERITS the parent's 5.
    SHOULD(ShouldlyString(child.Get(PropertyId::of(name, "AppOwner_A"))).ShouldBe(Value::Int32(5)));

    // parent.Set(9) — the child's effective follows the new move (9).
    SHOULD(ShouldlyString(parent.Set(PropertyId::of(name, "AppOwner_A"), Value::Int32(9))).ShouldBe(SetResult::APPLIED));
    SHOULD(ShouldlyString(child.Get(PropertyId::of(name, "AppOwner_A"))).ShouldBe(Value::Int32(9)));
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
    SHOULD(ShouldlyString(child.Set(PropertyId::of(name, "AppOwner_A"), Value::Int32(7))).ShouldBe(SetResult::APPLIED));
    SHOULD(ShouldlyString(child.Get(PropertyId::of(name, "AppOwner_A"))).ShouldBe(Value::Int32(7)));

    // A later parent.Set(44) must NOT clobber the child (local-wins).
    (void)parent.Set(PropertyId::of(name, "AppOwner_A"), Value::Int32(44));
    SHOULD(ShouldlyString(child.Get(PropertyId::of(name, "AppOwner_A"))).ShouldBe(Value::Int32(7)));
}

TEST_CASE("S044(S811): attached-Inherits VALUE propagates to a passive child; origin fires the synchronous metadata cb")
{
    // S811: there is NO per-object notification pipeline (no child subscriber,
    // no descendant-push postbox — removed). What survives is (A) the inherited
    // VALUE propagating to a purely-passive child (it never registers, never Set,
    // never materializes a store entry — it reads the inherited value through
    // S043 global resolution + the S039 ancestor walk), and (B) the ORIGIN's
    // synchronous PropertyChangedCallback (metadata with_changed, WPF DO.cs:
    // 2037) firing once inside its Set. This is the WPF-faithful notification:
    // the changed callback lives on the metadata of the property being Set, not
    // on a spurious per-object subscriber list.
    const std::string name = "AttachedBrush_S044_3";
    SubTrace trace;
    const DependencyProperty& dp =
        DependencyProperty::Register(name, ValueType::Int32, "AppOwner_A",
                                     PropertyMetadata::WithDefault(Value::Int32(0), kInh)
                                         .with_changed(trace.changed(name.c_str())));
    (void)dp;

    DependencyObject parent, child;
    child.set_parent(&parent); // child is passive — no Set, no register ever

    // Parent's attached Inherits write fires the ORIGIN's metadata cb exactly
    // once (0 -> 8) AND the child's inherited value follows (8).
    SHOULD(ShouldlyString(parent.Set(PropertyId::of(name, "AppOwner_A"), Value::Int32(8))).ShouldBe(SetResult::APPLIED));
    SHOULD(ShouldlyString(trace.names.size()).ShouldBe(1u));
    SHOULD(ShouldlyString(trace.names[0]).ShouldBe(std::string(name)));
    SHOULD(ShouldlyString(trace.olds[0]).ShouldBe(0));
    SHOULD(ShouldlyString(trace.news[0]).ShouldBe(8));
    SHOULD(ShouldlyString(child.Get(PropertyId::of(name, "AppOwner_A"))).ShouldBe(Value::Int32(8)));
}

TEST_CASE("S044(S811): attached inheritance — each parent Set fires the origin cb synchronously; child follows the latest")
{
    // S811: NO coalescing / deferred drain — each Set fires the origin's
    // synchronous metadata cb with its own old->new delta, and the child's
    // effective value follows the latest parent move (the pull-model ancestor
    // walk).
    const std::string name = "AttachedCoalesce_S044_4";
    SubTrace trace;
    const DependencyProperty& dp =
        DependencyProperty::Register(name, ValueType::Int32, "AppOwner_A",
                                     PropertyMetadata::WithDefault(Value::Int32(0), kInh)
                                         .with_changed(trace.changed(name.c_str())));
    (void)dp;

    DependencyObject parent, child;
    child.set_parent(&parent); // passive child

    (void)parent.Set(PropertyId::of(name, "AppOwner_A"), Value::Int32(5)); // cb (0 -> 5)
    (void)parent.Set(PropertyId::of(name, "AppOwner_A"), Value::Int32(8)); // cb (5 -> 8)

    // Two SYNCHRONOUS origin callbacks, in order, each its own delta.
    SHOULD(ShouldlyString(trace.names.size()).ShouldBe(2u));
    SHOULD(ShouldlyString(trace.olds[0]).ShouldBe(0));
    SHOULD(ShouldlyString(trace.news[0]).ShouldBe(5));
    SHOULD(ShouldlyString(trace.olds[1]).ShouldBe(5));
    SHOULD(ShouldlyString(trace.news[1]).ShouldBe(8));

    // The child's inherited value follows the latest (8).
    SHOULD(ShouldlyString(child.Get(PropertyId::of(name, "AppOwner_A"))).ShouldBe(Value::Int32(8)));
}

TEST_CASE("S044(S811): wrong-thread attached Inherits Set THROWS; value and metadata cb untouched")
{
    // S811: a NON-affine parent Set THROWS WrongThreadException (WPF
    // VerifyAccess). Nothing is applied, the origin's metadata cb does NOT fire,
    // and the child's inherited value stays the default.
    const std::string name = "AttachedMarshalled_S044_5";
    SubTrace trace;
    const DependencyProperty& dp =
        DependencyProperty::Register(name, ValueType::Int32, "AppOwner_A",
                                     PropertyMetadata::WithDefault(Value::Int32(0), kInh)
                                         .with_changed(trace.changed(name.c_str())));
    (void)dp;

    DependencyObject parent, child;
    child.set_parent(&parent); // passive child

    // Non-affine attached Inherits Set -> THROWS; nothing applied, no cb.
    std::exception_ptr exc;
    setFromOtherThreadThrows(parent, PropertyId::of(name, "AppOwner_A"), Value::Int32(9), exc);
    SHOULD(ShouldlyString(exc != nullptr).ShouldBe(true));
    SHOULD(ShouldThrow<WrongThreadException>([&] { std::rethrow_exception(exc); }));
    SHOULD(ShouldlyString(parent.Get(PropertyId::of(name, "AppOwner_A"))).ShouldBe(Value::Int32(0)));
    // The origin's metadata cb did NOT fire (nothing was applied).
    SHOULD(ShouldlyString(trace.names.size()).ShouldBe(0u));
    // The child's inherited value is unchanged (default 0).
    SHOULD(ShouldlyString(child.Get(PropertyId::of(name, "AppOwner_A"))).ShouldBe(Value::Int32(0)));
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
    (void)parent.Set(PropertyId::of(name, "AccentOwner_A"), Value::Int32(3));
    SHOULD(ShouldlyString(child.Get(PropertyId::of(name, "AccentOwner_A"))).ShouldBe(Value::Int32(3)));
    SHOULD(ShouldlyString(dp.owner_type_id()).ShouldBe(std::string("AccentOwner_A")));

    // A LOCAL registration of the same NAME shadows the global path -> the local
    // object's own default (99) governs, not the global descriptor's.
    DependencyObject b;
    PropertyKey localKey(name, ValueType::Int32, "LocalOwner_B", PropertyFlags::None);
    (void)b.register_property(std::move(localKey),
                              PropertyMetadata::WithDefault(Value::Int32(99), PropertyFlags::None));
    SHOULD(ShouldlyString(b.Get(PropertyId::of(name, "LocalOwner_B"))).ShouldBe(Value::Int32(99)));

    // An UNREGISTERED (not local, not global) name still throws PropertyNotFound.
    DependencyObject c;
    SHOULD(ShouldThrow<PropertyNotFound>([&] {
        (void)c.Get(PropertyId::of("NoSuchAttached_S044_6", "LocalOwner_B"));
    }));
    SHOULD(ShouldThrow<PropertyNotFound>([&] {
        (void)c.Set(PropertyId::of("NoSuchAttached_S044_6", "LocalOwner_B"), Value::Int32(1));
    }));
}
