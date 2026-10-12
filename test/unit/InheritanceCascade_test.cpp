// S040 — Inheritance cascade tests (dep:S039).
//
// Contracts the WRITE/CASCADE side of the S039 pull model: because
// GetEffectiveValue walks the ancestor chain FRESH on every read, a parent's
// Set is automatically reflected by every descendant with NO child write.
// S040 proves that "parent change recomputes child effective value without
// child write" as real behavior, with the genuinely new (untested-at-S039)
// dimensions:
//   1. dynamic ordering — the parent change happens AFTER the child is
//      attached (S039 only set up the chain then read once, static);
//   2. local-vs-inherited interaction — a real child local override vs the
//      inherited value ("child Set vs inherited old").
//
// S040 is a TEST-ONLY step: zero new product symbols. The S039 pull model
// already delivers correct cascade semantics, and S041 owns the explicit
// push-notification/ChangeQueue pipeline — so S040 MUST NOT build descendant
// tracking / notifications (stepping on S041's scope, recorded NOT silent).
//
// Scope of THIS file:
//   1. cascade after attach       — parent.Set AFTER child.set_parent ->
//      child effective updates, no child Set ever called.
//   2. multi-level cascade        — set up a 3-level chain, then change the
//      great-grandparent -> great-grandchild recomputes, no child write.
//   3. local-wins                 — child holds its own local override -> a
//      parent change does NOT sweep it; child effective stays its own value.
//   4. InheritOnly recompute      — child has its own local but InheritOnly ->
//      a parent change DOES recompute it (own local ignored, follows chain).
//   5. child Set vs inherited old — child writes its own local AFTER
//      inheriting -> its effective is the local; a later parent change leaves
//      it untouched (the cascade never clobbers a child write).
//   6. parent reverts             — parent Set to a new value then to another
//      -> child follows both, no child write.
//
// Uses the Shouldly-for-doctest vocabulary (SHOULD / ShouldThrow), mirroring
// EffectiveValue_test.cpp's harness: hostInherited() registers the GLOBAL
// DependencyProperty descriptor once per name (name-unique) and mirrors the
// property onto each object's own store.

// doctest's main comes from smoke_test.cpp.
#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/DependencyObject.h"
#include "vaml/core/DependencyProperty.h"
#include "vaml/core/PropertyId.h"
#include "vaml/core/PropertyKey.h"

#include <string>
#include <unordered_set>

using vaml::DependencyObject;
using vaml::DependencyProperty;
using vaml::PropertyFlags;
using vaml::PropertyId;
using vaml::PropertyKey;
using vaml::PropertyMetadata;
using vaml::Value;
using vaml::ValueType;
using vaml::test::ShouldThrow;

namespace {

// The GLOBAL DependencyProperty table is name-unique (S039 Register throws on
// a duplicate). Track names already registered globally so each inherited
// property's GLOBAL descriptor is written exactly once across the suite.
std::unordered_set<std::string>& global_seen()
{
    static std::unordered_set<std::string> seen;
    return seen;
}

// Registers the GLOBAL DependencyProperty descriptor (once per name) AND
// mirrors the property onto `obj`'s OWN store (object-scoped, dedupe is
// per-object so this is safe to repeat across objects).
void hostInherited(DependencyObject& obj, const std::string& name, PropertyFlags flags, Value def)
{
    if (global_seen().insert(name).second)
    {
        (void)DependencyProperty::Register(name, ValueType::Int32, "PropOwner", PropertyMetadata::WithDefault(def, flags));
    }
    PropertyKey key(name, ValueType::Int32, "PropOwner", flags);
    (void)obj.register_property(std::move(key), PropertyMetadata::WithDefault(def, flags));
}

// Plain inheriting property (local ?? inherited ?? default, no InheritOnly).
constexpr PropertyFlags kInh = PropertyFlags::Inherits;

} // namespace

TEST_CASE("S040: cascade after attach — parent change AFTER child attached recomputes child effective")
{
    DependencyObject parent;
    DependencyObject child;
    const std::string name = "Cascade_S040_1";
    hostInherited(parent, name, kInh, Value::Int32(0));
    hostInherited(child, name, kInh, Value::Int32(0));

    // Attach FIRST, then the parent changes (the dynamic ordering S039 did
    // not exercise — S039 set the parent then attached the child once).
    child.set_parent(&parent);

    // No child write ever: the child's own local store stays empty, so the
    // parent's later Set must be reflected purely through the read-side pull.
    (void)parent.Set(PropertyId::of(name), Value::Int32(11));
    SHOULD(ShouldlyString(child.GetEffectiveValue(PropertyId::of(name))).ShouldBe(Value::Int32(11)));

    // A second parent change recomputes again, still with no child write.
    (void)parent.Set(PropertyId::of(name), Value::Int32(22));
    SHOULD(ShouldlyString(child.GetEffectiveValue(PropertyId::of(name))).ShouldBe(Value::Int32(22)));
}

TEST_CASE("S040: multi-level cascade — changing the great-grandparent recomputes the great-grandchild")
{
    DependencyObject g1, g2, g3, leaf;
    const std::string name = "Cascade_S040_2";
    for (DependencyObject* o : { &g1, &g2, &g3, &leaf })
    {
        hostInherited(*o, name, kInh, Value::Int32(0));
    }

    // Build the chain in place, THEN mutate the top.
    g2.set_parent(&g1);
    g3.set_parent(&g2);
    leaf.set_parent(&g3);

    (void)g1.Set(PropertyId::of(name), Value::Int32(5));
    SHOULD(ShouldlyString(leaf.GetEffectiveValue(PropertyId::of(name))).ShouldBe(Value::Int32(5)));

    // Change the great-grandparent after the chain is fully wired —
    // intermediate nodes g2/g3 hold NO local, so the leaf must follow.
    (void)g1.Set(PropertyId::of(name), Value::Int32(9));
    SHOULD(ShouldlyString(leaf.GetEffectiveValue(PropertyId::of(name))).ShouldBe(Value::Int32(9)));
}

TEST_CASE("S040: local-wins — a child's own local override is NOT swept by a parent change")
{
    DependencyObject parent;
    DependencyObject child;
    const std::string name = "Cascade_S040_3";
    hostInherited(parent, name, kInh, Value::Int32(0));
    hostInherited(child, name, kInh, Value::Int32(0));

    child.set_parent(&parent);

    // Child declares a REAL local override BEFORE the parent changes.
    (void)child.Set(PropertyId::of(name), Value::Int32(7));
    SHOULD(ShouldlyString(child.GetEffectiveValue(PropertyId::of(name))).ShouldBe(Value::Int32(7)));

    // Parent changes AFTER: the cascade must RESPECT the child's real local
    // override — it does NOT sweep it (child effective stays its own 7).
    (void)parent.Set(PropertyId::of(name), Value::Int32(99));
    SHOULD(ShouldlyString(child.GetEffectiveValue(PropertyId::of(name))).ShouldBe(Value::Int32(7)));
}

TEST_CASE("S040: InheritOnly recompute — own local is IGNORED, so a parent change DOES recompute")
{
    DependencyObject parent;
    DependencyObject child;
    const std::string name = "Cascade_S040_4";
    const PropertyFlags inhInheritOnly = PropertyFlags::Inherits | PropertyFlags::InheritOnly;
    hostInherited(parent, name, inhInheritOnly, Value::Int32(0));
    hostInherited(child, name, inhInheritOnly, Value::Int32(0));

    child.set_parent(&parent);

    // Child SETS its own local override — but InheritOnly means it is IGNORED,
    // so the value comes from the parent's chain instead.
    (void)child.Set(PropertyId::of(name), Value::Int32(100));

    (void)parent.Set(PropertyId::of(name), Value::Int32(3));
    SHOULD(ShouldlyString(child.GetEffectiveValue(PropertyId::of(name))).ShouldBe(Value::Int32(3)));

    // A second parent change still recomputes the InheritOnly child (its own
    // local never contributes), again with no child write.
    (void)parent.Set(PropertyId::of(name), Value::Int32(4));
    SHOULD(ShouldlyString(child.GetEffectiveValue(PropertyId::of(name))).ShouldBe(Value::Int32(4)));
}

TEST_CASE("S040: child Set vs inherited old — a child write after inheriting is never clobbered by a later parent change")
{
    DependencyObject parent;
    DependencyObject child;
    const std::string name = "Cascade_S040_5";
    hostInherited(parent, name, kInh, Value::Int32(0));
    hostInherited(child, name, kInh, Value::Int32(0));

    child.set_parent(&parent);

    // Child first INHERITS the parent's value...
    (void)parent.Set(PropertyId::of(name), Value::Int32(20));
    SHOULD(ShouldlyString(child.GetEffectiveValue(PropertyId::of(name))).ShouldBe(Value::Int32(20)));

    // ...then writes its OWN local override (child Set vs inherited old).
    (void)child.Set(PropertyId::of(name), Value::Int32(31));
    SHOULD(ShouldlyString(child.GetEffectiveValue(PropertyId::of(name))).ShouldBe(Value::Int32(31)));

    // A LATER parent change must NOT clobber that child write — the cascade
    // respects the now-local override and leaves the child effective untouched.
    (void)parent.Set(PropertyId::of(name), Value::Int32(44));
    SHOULD(ShouldlyString(child.GetEffectiveValue(PropertyId::of(name))).ShouldBe(Value::Int32(31)));
}

TEST_CASE("S040: parent reverts — the child follows both the new value and the reverting value, no child write")
{
    DependencyObject parent;
    DependencyObject child;
    const std::string name = "Cascade_S040_6";
    hostInherited(parent, name, kInh, Value::Int32(0));
    hostInherited(child, name, kInh, Value::Int32(0));

    child.set_parent(&parent);

    // Parent moves to a new value AFTER attach...
    (void)parent.Set(PropertyId::of(name), Value::Int32(8));
    SHOULD(ShouldlyString(child.GetEffectiveValue(PropertyId::of(name))).ShouldBe(Value::Int32(8)));

    // ...and reverts to a prior/different value. The child tracks BOTH moves
    // purely through the read-side pull — no child Set anywhere in this case.
    (void)parent.Set(PropertyId::of(name), Value::Int32(0));
    SHOULD(ShouldlyString(child.GetEffectiveValue(PropertyId::of(name))).ShouldBe(Value::Int32(0)));

    (void)parent.Set(PropertyId::of(name), Value::Int32(6));
    SHOULD(ShouldlyString(child.GetEffectiveValue(PropertyId::of(name))).ShouldBe(Value::Int32(6)));
}
