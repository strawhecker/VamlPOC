// S043 — Attached properties tests (dep:S042).
//
// Delivers the S043 headline against live DependencyObjects:
//   "attached properties: key owned by A settable on B; registry supports
//    owner != class."
//
// Before S043, an attached DP (key owned by A) was NOT settable on B unless B
// individually register_property()'d the same key — GetEffectiveValue threw
// PropertyNotFound and Set had no global-resolved path. S043 makes
// DependencyObject attached-aware by GLOBAL RESOLUTION:
//   - Set()/Get()/GetEffectiveValue() resolve a globally-registered
//     DependencyProperty even when the object has not locally registered it;
//   - the move-only PropertyMetadata is NEVER copied into an object's entry —
//     an attached (borrowed) entry holds a non-owning pointer to the global
//     descriptor (which lives for the process lifetime);
//   - the declaring owner (A) is surfaced via DependencyProperty::owner_type_id().
//
// Proves:
//   1. attached Set/Get on B WITHOUT B calling register_property (key owned by
//      CanvasOwner_A) — headline.
//   2. effective value = local ?? inherited ?? global-default: an unattached
//      object resolves an attached inherited property to the ancestor's value,
//      and an orphan reads the global default.
//   3. owner != class: a key registered under owner A is settable/gettable on a
//      plain object B, and owner_type_id() reads back A.
//   4. wrong-thread attached Set -> QUEUED; the attached drain applies it
//      (marshalling parity with S037).
//   5. a LOCAL registration still shadows the global path (backward compat),
//      and an unregistered (non-global) name still throws PropertyNotFound.
//
// Uses the Shouldly-for-doctest vocabulary (S036_5 standing convention).

// doctest's main comes from smoke_test.cpp (DO doctest IMPLEMENT_WITH_MAIN).
#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/DependencyObject.h"
#include "vaml/core/DependencyProperty.h"
#include "vaml/core/PropertyId.h"
#include "vaml/core/PropertyKey.h"
#include "vaml/core/PropertyRegistry.h" // PropertyNotFound

#include <string>
#include <thread>
#include <unordered_set>

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

// The GLOBAL DependencyProperty table is name-unique (S039 Register throws on a
// duplicate NAME across the whole process). Each S043 name carries a unique
// suffix, so no cross-TEST_CASE / cross-TU collision is possible; the set guards
// against accidental reuse within this TU.
std::unordered_set<std::string>& global_seen()
{
    static std::unordered_set<std::string> seen;
    return seen;
}

// Registers an ATTACHED property GLOBALLY (once per name): the key is owned by
// `owner` but the object `host` does NOT locally register it — this is the
// S043 premise (owner != storage class).
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

// A non-affine Set() executed on a fresh thread; the result captured back by a
// join barrier (mirrors S037's marshalling harness).
void setFromOtherThread(DependencyObject& obj, const PropertyId& id, Value v, SetResult* out)
{
    std::thread t([&] { *out = obj.Set(id, v); });
    t.join();
}

} // namespace

TEST_CASE("S043: attached Set/Get on B without B calling register_property (key owned by A)")
{
    // Headline. A globally-registered attached property, owner = CanvasOwner_A.
    const std::string name = "Left_S043_1";
    (void)registerAttachedGlobal(name, "CanvasOwner_A", PropertyFlags::None, Value::Int32(0));

    // B is a plain object that NEVER calls register_property for this property.
    DependencyObject b;
    SHOULD(ShouldlyString(b.Get(PropertyId::of(name))).ShouldBe(Value::Int32(0))); // global default resolves

    // Set on B without any local registration -> APPLIED (attached write lands).
    SHOULD(ShouldlyString(b.Set(PropertyId::of(name), Value::Int32(5))).ShouldBe(SetResult::APPLIED));
    SHOULD(ShouldlyString(b.Get(PropertyId::of(name))).ShouldBe(Value::Int32(5)));

    // A second, independent B object does NOT see the first B's value (value is
    // stored PER-OBJECT, the definition is owned once by A).
    DependencyObject b2;
    SHOULD(ShouldlyString(b2.Get(PropertyId::of(name))).ShouldBe(Value::Int32(0)));
}

TEST_CASE("S043: effective value = local ?? inherited ?? global-default (orphan -> global default)")
{
    const std::string name = "Theme_S043_2";
    (void)registerAttachedGlobal(name, "AppOwner", PropertyFlags::Inherits, Value::Int32(0));

    // An orphan (no parent) with NO local registration resolves the global default.
    DependencyObject orphan;
    SHOULD(ShouldlyString(orphan.GetEffectiveValue(PropertyId::of(name))).ShouldBe(Value::Int32(0)));

    // A parent->child chain: neither registers the property locally. The parent
    // holds an attached value; the child (no local entry) INHERITS it through the
    // global-resolved read, never having called register_property.
    DependencyObject parent, child;
    child.set_parent(&parent);
    SHOULD(ShouldlyString(parent.Set(PropertyId::of(name), Value::Int32(7))).ShouldBe(SetResult::APPLIED));
    SHOULD(ShouldlyString(parent.GetEffectiveValue(PropertyId::of(name))).ShouldBe(Value::Int32(7)));
    // Child, unattached, no local override -> inherits the parent's 7.
    SHOULD(ShouldlyString(child.GetEffectiveValue(PropertyId::of(name))).ShouldBe(Value::Int32(7)));

    // A child local override wins (local ?? inherited ?? default).
    SHOULD(ShouldlyString(child.Set(PropertyId::of(name), Value::Int32(9))).ShouldBe(SetResult::APPLIED));
    SHOULD(ShouldlyString(child.GetEffectiveValue(PropertyId::of(name))).ShouldBe(Value::Int32(9)));
}

TEST_CASE("S043: owner != class — key owned by A settable on B; owner_type_id reads back A")
{
    const std::string name = "Dock_S043_3";
    const DependencyProperty& dp =
        registerAttachedGlobal(name, "DockPanelOwner_A", PropertyFlags::None, Value::Int32(0));

    // The OWNER is A — read back directly from the global descriptor.
    SHOULD(ShouldlyString(dp.owner_type_id()).ShouldBe(std::string("DockPanelOwner_A")));

    // B is a DIFFERENT class: it stores and reads A's attached key fine.
    DependencyObject b;
    (void)b.Set(PropertyId::of(name), Value::Int32(4));
    SHOULD(ShouldlyString(b.Get(PropertyId::of(name))).ShouldBe(Value::Int32(4)));
    (void)b.Set(PropertyId::of(name), Value::Int32(0)); // clear back toward default
    SHOULD(ShouldlyString(b.Get(PropertyId::of(name))).ShouldBe(Value::Int32(0)));

    // The global descriptor still reports owner A even after B stored a value.
    SHOULD(ShouldlyString(dp.owner_type_id()).ShouldBe(std::string("DockPanelOwner_A")));
}

TEST_CASE("S043: wrong-thread attached Set -> QUEUED; the attached drain applies it (marshalling parity)")
{
    const std::string name = "CanvasTop_S043_4";
    (void)registerAttachedGlobal(name, "CanvasOwner_A", PropertyFlags::None, Value::Int32(0));

    DependencyObject b; // never locally registers the property

    // Affine attached Set applies normally, queued count stays 0.
    SHOULD(ShouldlyString(b.Set(PropertyId::of(name), Value::Int32(1))).ShouldBe(SetResult::APPLIED));
    SHOULD(ShouldlyString(b.pending_change_count()).ShouldBe(0u));

    // Wrong-thread attached Set -> QUEUED, NOT applied, store untouched.
    SetResult r = SetResult::NO_CHANGE;
    setFromOtherThread(b, PropertyId::of(name), Value::Int32(8), &r);
    SHOULD(ShouldlyString(r).ShouldBe(SetResult::QUEUED));
    SHOULD(ShouldlyString(b.pending_change_count()).ShouldBe(1u));
    SHOULD(ShouldlyString(b.Get(PropertyId::of(name))).ShouldBe(Value::Int32(1))); // not applied

    // Affinity-thread drain applies the queued attached write.
    b.ProcessPendingChanges();
    SHOULD(ShouldlyString(b.pending_change_count()).ShouldBe(0u));
    SHOULD(ShouldlyString(b.Get(PropertyId::of(name))).ShouldBe(Value::Int32(8)));
}

TEST_CASE("S043: local registration still shadows the global path; unregistered name throws")
{
    // A property that is BOTH globally registered (attached, owner A) AND then
    // locally registered on B with a DIFFERENT default. B's LOCAL registration
    // must govern (backward compat) — the owned entry, not the global fallback.
    const std::string name = "Margin_S043_5";
    (void)registerAttachedGlobal(name, "MarginOwner_A", PropertyFlags::None, Value::Int32(0));

    DependencyObject b;
    // Local registration shadows the global descriptor -> B's own default (99).
    PropertyKey localKey(name, ValueType::Int32, "LocalOwner_B", PropertyFlags::None);
    (void)b.register_property(std::move(localKey),
                              PropertyMetadata::WithDefault(Value::Int32(99), PropertyFlags::None));
    SHOULD(ShouldlyString(b.Get(PropertyId::of(name))).ShouldBe(Value::Int32(99))); // local default, not global 0
    (void)b.Set(PropertyId::of(name), Value::Int32(3));
    SHOULD(ShouldlyString(b.Get(PropertyId::of(name))).ShouldBe(Value::Int32(3))); // local path

    // An UNREGISTERED (not local, not global) name still throws PropertyNotFound.
    DependencyObject c;
    SHOULD(ShouldThrow<PropertyNotFound>([&] {
        (void)c.Get(PropertyId::of("NoSuchAttached_S043_5"));
    }));
    SHOULD(ShouldThrow<PropertyNotFound>([&] {
        (void)c.Set(PropertyId::of("NoSuchAttached_S043_5"), Value::Int32(1));
    }));
}
