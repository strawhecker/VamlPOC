// S039 — Effective-value resolution tests (dep:S038).
//
// Verifies DependencyObject::GetEffectiveValue — the read-side pull model:
//   local ?? inherited-from-parent ?? default, with the InheritOnly flag
//   (own local override does NOT contribute).
//
// Scope (S039 is READ-side only; Set / write/cascade is S040):
//   1. non-inheriting:           local ?? default (unchanged S036 semantics).
//   2. plain Inherits, 1 level:  child inherits the parent's local.
//   3. plain Inherits, multi-level: child inherits a great-grandparent's value.
//   4. nearest-ancestor-with-value: an intermediate ancestor with no local
//      defers to ITS ancestor (child still resolves the grandparent's value).
//   5. InheritOnly: own local override is IGNORED; value comes from the
//      ancestor chain (else default).
//   6. empty parent chain -> default (nothing to inherit from).
//   7. Get() delegates to GetEffectiveValue (synonym).
//   8. unregistered property -> PropertyNotFound.
//
// Uses the Shouldly-for-doctest vocabulary. The inheritance descriptor
// (flags + default) is sourced from the GLOBAL DependencyProperty where the
// property is globally registered (S039's real consumer); object registration
// mirror-hosts the same flags so the store lookup is satisfiable.

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
using vaml::PropertyNotFound;
using vaml::Value;
using vaml::ValueType;
using vaml::test::ShouldThrow;

namespace {

// The GLOBAL DependencyProperty table is name-unique (S039 Register throws on
// a duplicate). Track names already registered globally so each inherited
// property's GLOBAL descriptor is written exactly once across the test suite.
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

PropertyKey intKey(const std::string& name, PropertyFlags flags)
{
    return PropertyKey(name, ValueType::Int32, "TestOwner", flags);
}

} // namespace

TEST_CASE("S039: non-inheriting property — local ?? default (S036 semantics unchanged)")
{
    DependencyObject obj;
    (void)obj.register_property(intKey("Plain", PropertyFlags::None),
                                PropertyMetadata::WithDefault(Value::Int32(7), PropertyFlags::None));

    // No parent, default applied.
    SHOULD(ShouldlyString(obj.GetEffectiveValue(PropertyId::of("Plain"))).ShouldBe(Value::Int32(7)));
    // Local override wins.
    (void)obj.Set(PropertyId::of("Plain"), Value::Int32(50));
    SHOULD(ShouldlyString(obj.GetEffectiveValue(PropertyId::of("Plain"))).ShouldBe(Value::Int32(50)));
    // Get() delegates -> same value.
    SHOULD(ShouldlyString(obj.Get(PropertyId::of("Plain"))).ShouldBe(Value::Int32(50)));
}

TEST_CASE("S039: plain Inherits, one level — child inherits parent's local override")
{
    DependencyObject parent;
    DependencyObject child;
    const std::string name = "InhColor_S039_1";
    hostInherited(parent, name, PropertyFlags::Inherits, Value::Int32(0));
    hostInherited(child, name, PropertyFlags::Inherits, Value::Int32(0));

    (void)parent.Set(PropertyId::of(name), Value::Int32(5));
    child.set_parent(&parent);

    // Child has no local -> inherits parent's local 5.
    SHOULD(ShouldlyString(child.GetEffectiveValue(PropertyId::of(name))).ShouldBe(Value::Int32(5)));
}

TEST_CASE("S039: plain Inherits, multi-level — child inherits from a great-grandparent")
{
    DependencyObject g1, g2, g3, leaf;
    const std::string name = "InhColor_S039_2";
    for (DependencyObject* o : { &g1, &g2, &g3, &leaf })
    {
        hostInherited(*o, name, PropertyFlags::Inherits, Value::Int32(0));
    }

    (void)g1.Set(PropertyId::of(name), Value::Int32(42));
    g2.set_parent(&g1);
    g3.set_parent(&g2);
    leaf.set_parent(&g3);

    SHOULD(ShouldlyString(leaf.GetEffectiveValue(PropertyId::of(name))).ShouldBe(Value::Int32(42)));
}

TEST_CASE("S039: nearest-ancestor-with-value — intermediate empty local defers to ITS ancestor")
{
    DependencyObject grandparent, parent, child;
    const std::string name = "InhColor_S039_3";
    for (DependencyObject* o : { &grandparent, &parent, &child })
    {
        hostInherited(*o, name, PropertyFlags::Inherits, Value::Int32(0));
    }

    // grandparent holds a local; parent has NO local; child inherits.
    (void)grandparent.Set(PropertyId::of(name), Value::Int32(9));
    parent.set_parent(&grandparent);
    child.set_parent(&parent);

    // parent's effective = grandparent's 9; child's effective = parent's 9
    // (which itself is inherited from grandparent).
    SHOULD(ShouldlyString(parent.GetEffectiveValue(PropertyId::of(name))).ShouldBe(Value::Int32(9)));
    SHOULD(ShouldlyString(child.GetEffectiveValue(PropertyId::of(name))).ShouldBe(Value::Int32(9)));
}

TEST_CASE("S039: InheritOnly ignores own local override and still inherits from the chain")
{
    DependencyObject parent, child;
    const std::string name = "InhOnly_S039_4";
    hostInherited(parent, name, PropertyFlags::Inherits | PropertyFlags::InheritOnly, Value::Int32(0));
    hostInherited(child, name, PropertyFlags::Inherits | PropertyFlags::InheritOnly, Value::Int32(0));

    (void)parent.Set(PropertyId::of(name), Value::Int32(7));
    child.set_parent(&parent);

    // Child SETS its own local override ... but InheritOnly means it must be
    // IGNORED — the value comes from the parent's chain instead.
    (void)child.Set(PropertyId::of(name), Value::Int32(100));
    SHOULD(ShouldlyString(child.GetEffectiveValue(PropertyId::of(name))).ShouldBe(Value::Int32(7)));
}

TEST_CASE("S039: InheritOnly with no ancestor value falls back to default")
{
    DependencyObject child;
    const std::string name = "InhOnly_S039_5";
    hostInherited(child, name, PropertyFlags::Inherits | PropertyFlags::InheritOnly, Value::Int32(3));
    // Set a local override — ignored (InheritOnly) — and there is no parent.
    (void)child.Set(PropertyId::of(name), Value::Int32(77));
    SHOULD(ShouldlyString(child.GetEffectiveValue(PropertyId::of(name))).ShouldBe(Value::Int32(3))); // default
}

TEST_CASE("S039: empty parent chain — inheriting property resolves to default")
{
    DependencyObject orphan;
    const std::string name = "InhColor_S039_6";
    hostInherited(orphan, name, PropertyFlags::Inherits, Value::Int32(4));
    SHOULD(ShouldlyString(orphan.GetEffectiveValue(PropertyId::of(name))).ShouldBe(Value::Int32(4)));
}

TEST_CASE("S039: Get() delegates to GetEffectiveValue for an inheriting property")
{
    DependencyObject parent, child;
    const std::string name = "InhColor_S039_7";
    hostInherited(parent, name, PropertyFlags::Inherits, Value::Int32(0));
    hostInherited(child, name, PropertyFlags::Inherits, Value::Int32(0));

    (void)parent.Set(PropertyId::of(name), Value::Int32(13));
    child.set_parent(&parent);
    SHOULD(ShouldlyString(child.Get(PropertyId::of(name))).ShouldBe(Value::Int32(13)));
}

TEST_CASE("S039: GetEffectiveValue throws PropertyNotFound for an unregistered property")
{
    DependencyObject obj;
    SHOULD(ShouldThrow<PropertyNotFound>([&] {
        (void)obj.GetEffectiveValue(PropertyId::of("NoSuch_S039"));
    }));
}
