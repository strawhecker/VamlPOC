// S818 (dep:S817, Sprint 11aa) — WIRE the Affects* runtime invalidation EXACTLY
// as `FrameworkElement.OnPropertyChanged` (FE.cs:2198-2251), consumed through
// the S126 measure/arrange dirty axis + the S208/S210 render-dirty seam.
//
// A Set that PRODUCED A REAL CHANGE on a DP carrying:
//   - AffectsParentMeasure / AffectsParentArrange -> the NEAREST UIElement
//     layout parent's InvalidateMeasure / InvalidateArrange (parent-chain walk,
//     FE.cs:2219/2234 — the nearest only, a grandchild dirties its immediate
//     panel parent, not beyond).
//   - AffectsMeasure -> InvalidateMeasure (FE.cs:2241).
//   - AffectsArrange -> InvalidateArrange (FE.cs:2243).
//   - AffectsRender -> InvalidateVisual (FE.cs:2251), GATED by
//     SubPropertiesDoNotAffectRender (a sub-property change does NOT repaint).
//
// The real-change gate: a Set whose coerced value equals the current effective
// returns NO_CHANGE and does NOT run OnPropertyChanged -> NO invalidation. This
// is exactly WPF's `args.IsAValueChange` gate (FE.cs:2251).
//
// Uses the Shouldly-for-doctest vocabulary (S036_5 standing convention).

// doctest's main comes from smoke_test.cpp.
#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/DependencyProperty.h"
#include "vaml/core/PropertyFlags.h"
#include "vaml/core/PropertyId.h"
#include "vaml/core/PropertyMetadata.h"
#include "vaml/core/UIElement.h"
#include "vaml/core/Value.h"

#include <string>
#include <unordered_set>

using vaml::DependencyProperty;
using vaml::PropertyFlags;
using vaml::PropertyId;
using vaml::PropertyMetadata;
using vaml::SetResult;
using vaml::UIElement;
using vaml::Value;
using vaml::ValueType;
using vaml::test::ShouldThrow;

namespace {

// The GLOBAL DependencyProperty table is (name, owner)-unique (S039/S801). Each
// S818 name carries a unique suffix so no cross-TEST_CASE / cross-TU collision
// is possible; the set guards against accidental reuse within this TU.
std::unordered_set<std::string>& s818_seen()
{
    static std::unordered_set<std::string> seen;
    return seen;
}

// Registers a GLOBAL Int32 attached property (owner "AffectsOwner_A") carrying
// `flags`. Objects (UIElements) do NOT locally register it — the true attached
// premise (S043/S044): Set on any element resolves the global definition's
// flags, so the Affects* invalidation fires from the resolved metadata.
const DependencyProperty& s818_register(const char* name, PropertyFlags flags)
{
    if (s818_seen().insert(name).second)
    {
        return DependencyProperty::Register(
            name, ValueType::Int32, "AffectsOwner_A",
            PropertyMetadata::WithDefault(Value::Int32(0), flags));
    }
    return DependencyProperty::Get(name, "AffectsOwner_A");
}

// Builds a parent -> child UIElement tree and clears BOTH layout axes on every
// node (Measure + Arrange) so the dirty observables start CLEAN (a fresh
// element starts dirty on both axes). This makes a later invalidation visible
// as a bit flipping true.
void clear_layout(UIElement& parent, UIElement& child)
{
    parent.Measure(vaml::Size{ 400.0f, 400.0f });
    parent.Arrange(vaml::Rect{ 0, 0, 400, 400 });
    child.Measure(vaml::Size{ 400.0f, 400.0f });
    child.Arrange(vaml::Rect{ 0, 0, 400, 400 });

    SHOULD(ShouldlyString(parent.is_measure_dirty()).ShouldBe(false));
    SHOULD(ShouldlyString(parent.is_arrange_dirty()).ShouldBe(false));
    SHOULD(ShouldlyString(child.is_measure_dirty()).ShouldBe(false));
    SHOULD(ShouldlyString(child.is_arrange_dirty()).ShouldBe(false));
    SHOULD(ShouldlyString(child.is_render_dirty()).ShouldBe(false));
}

} // namespace

TEST_CASE("S818: AffectsMeasure — a real Set dirties the OWN measure axis")
{
    const char* name = "AffectsMeasure_S818";
    (void)s818_register(name, PropertyFlags::AffectsMeasure);
    const PropertyId id = PropertyId::of(name, "AffectsOwner_A");

    UIElement parent;
    UIElement* child = parent.create_child<UIElement>();
    clear_layout(parent, *child);

    // A REAL change on an AffectsMeasure DP -> the element's own measure dirty.
    SHOULD(ShouldlyString(child->Set(id, Value::Int32(7))).ShouldBe(SetResult::APPLIED));
    SHOULD(ShouldlyString(child->is_measure_dirty()).ShouldBe(true));
    SHOULD(ShouldlyString(child->is_arrange_dirty()).ShouldBe(false)); // measure axis only
    SHOULD(ShouldlyString(child->is_render_dirty()).ShouldBe(false));  // no AffectsRender
}

TEST_CASE("S818: AffectsArrange — a real Set dirties the OWN arrange axis")
{
    const char* name = "AffectsArrange_S818";
    (void)s818_register(name, PropertyFlags::AffectsArrange);
    const PropertyId id = PropertyId::of(name, "AffectsOwner_A");

    UIElement parent;
    UIElement* child = parent.create_child<UIElement>();
    clear_layout(parent, *child);

    SHOULD(ShouldlyString(child->Set(id, Value::Int32(1))).ShouldBe(SetResult::APPLIED));
    SHOULD(ShouldlyString(child->is_arrange_dirty()).ShouldBe(true));
    SHOULD(ShouldlyString(child->is_measure_dirty()).ShouldBe(false)); // arrange axis only
    SHOULD(ShouldlyString(child->is_render_dirty()).ShouldBe(false));
}

TEST_CASE("S818: AffectsParentMeasure — a real child Set invalidates the NEAREST UIElement layout parent")
{
    const char* name = "AffectsParentMeasure_S818";
    (void)s818_register(name, PropertyFlags::AffectsParentMeasure);
    const PropertyId id = PropertyId::of(name, "AffectsOwner_A");

    // root -> panel -> grandchild. Set the AffectsParentMeasure DP on the
    // GRANDCHILD. FE.cs:2219 walks the parent chain to the NEAREST UIElement
    // layout parent (the panel) and calls InvalidateMeasure() on IT. That
    // InvalidateMeasure call then propagates up the parent chain per the S126
    // contract (a parent's DesiredSize derives from its children) — so both
    // the nearest parent (panel) and the ancestors (root) land dirty. The
    // FE.cs "nearest-only" contract names WHICH parent receives the call (the
    // panel, not the root directly); the S126 propagation of that call is
    // unchanged.
    UIElement root;
    UIElement* panel = root.create_child<UIElement>();
    UIElement* grandchild = panel->create_child<UIElement>();
    // Clear the ENTIRE tree (root + panel + grandchild) so all start clean.
    root.Measure(vaml::Size{ 400.0f, 400.0f });
    root.Arrange(vaml::Rect{ 0, 0, 400, 400 });
    panel->Measure(vaml::Size{ 400.0f, 400.0f });
    panel->Arrange(vaml::Rect{ 0, 0, 400, 400 });
    grandchild->Measure(vaml::Size{ 400.0f, 400.0f });
    grandchild->Arrange(vaml::Rect{ 0, 0, 400, 400 });
    SHOULD(ShouldlyString(panel->is_measure_dirty()).ShouldBe(false));
    SHOULD(ShouldlyString(root.is_measure_dirty()).ShouldBe(false));

    SHOULD(ShouldlyString(grandchild->Set(id, Value::Int32(4))).ShouldBe(SetResult::APPLIED));
    // FE.cs:2219 — the nearest UIElement layout parent is the panel:
    SHOULD(ShouldlyString(panel->is_measure_dirty()).ShouldBe(true));
    // S839 (D1): the panel's InvalidateMeasure is LOCAL (local bit + enqueue, NO
    // ancestor walk) — the root is NOT dirtied here. It is dirtied only later,
    // LAZILY, when a real Measure of the panel changes its DesiredSize (the
    // OnChildDesiredSizeChanged waterfall). So with no measure pass run, the root
    // stays clean — matching WPF, whose InvalidateMeasure does not walk up.
    SHOULD(ShouldlyString(root.is_measure_dirty()).ShouldBe(false));
    // The GRANDCHILD's OWN measure axis is NOT dirtied (AffectsParent* only
    // targets the layout parent, not the changing element):
    SHOULD(ShouldlyString(grandchild->is_measure_dirty()).ShouldBe(false));
}

TEST_CASE("S818: AffectsParentArrange — a real child Set dirties the NEAREST UIElement parent's arrange")
{
    const char* name = "AffectsParentArrange_S818";
    (void)s818_register(name, PropertyFlags::AffectsParentArrange);
    const PropertyId id = PropertyId::of(name, "AffectsOwner_A");

    UIElement parent;
    UIElement* child = parent.create_child<UIElement>();
    clear_layout(parent, *child);

    SHOULD(ShouldlyString(child->Set(id, Value::Int32(2))).ShouldBe(SetResult::APPLIED));
    SHOULD(ShouldlyString(parent.is_arrange_dirty()).ShouldBe(true));
    SHOULD(ShouldlyString(child->is_arrange_dirty()).ShouldBe(false)); // parent axis, not own
    SHOULD(ShouldlyString(child->is_measure_dirty()).ShouldBe(false));
}

TEST_CASE("S818: AffectsRender — a real Set dirties the visual (InvalidateVisual); gated by SubPropertiesDoNotAffectRender")
{
    const char* name = "AffectsRender_S818";
    (void)s818_register(name, PropertyFlags::AffectsRender);
    const PropertyId id = PropertyId::of(name, "AffectsOwner_A");

    UIElement parent;
    UIElement* child = parent.create_child<UIElement>();
    clear_layout(parent, *child);

    // A real change on an AffectsRender DP -> InvalidateVisual (render dirty).
    SHOULD(ShouldlyString(child->Set(id, Value::Int32(3))).ShouldBe(SetResult::APPLIED));
    SHOULD(ShouldlyString(child->is_render_dirty()).ShouldBe(true));

    // Gate: AffectsRender | SubPropertiesDoNotAffectRender — a VALUE change is
    // still a real change and DOES invalidate (FE.cs:2251: a real change is NOT
    // gated). The flag only gates NON-value (sub-property) changes, which VAML's
    // Set() never emits (a Set reaching OnPropertyChanged is always a value
    // change). The flag's presence does NOT suppress a value change's repaint.
    const char* name2 = "AffectsRenderGated_S818";
    (void)s818_register(name2,
                        PropertyFlags::AffectsRender | PropertyFlags::SubPropertiesDoNotAffectRender);
    const PropertyId id2 = PropertyId::of(name2, "AffectsOwner_A");

    UIElement p2;
    UIElement* c2 = p2.create_child<UIElement>();
    clear_layout(p2, *c2);
    SHOULD(ShouldlyString(c2->Set(id2, Value::Int32(9))).ShouldBe(SetResult::APPLIED));
    SHOULD(ShouldlyString(c2->is_render_dirty()).ShouldBe(true)); // value change repaints
}

TEST_CASE("S818: a NO_CHANGE Set (equal value) fires NO invalidation")
{
    // The real-change gate: a Set whose coerced value equals the current
    // effective returns NO_CHANGE and does NOT run OnPropertyChanged -> no
    // Affects* invalidation (the WPF IsAValueChange gate, FE.cs:2251).
    const char* name = "AffectsNoChange_S818";
    (void)s818_register(name,
                        PropertyFlags::AffectsMeasure | PropertyFlags::AffectsRender);
    const PropertyId id = PropertyId::of(name, "AffectsOwner_A");

    UIElement parent;
    UIElement* child = parent.create_child<UIElement>();
    clear_layout(parent, *child);

    // First set APPLIES -> both axes+render dirty.
    SHOULD(ShouldlyString(child->Set(id, Value::Int32(5))).ShouldBe(SetResult::APPLIED));
    SHOULD(ShouldlyString(child->is_measure_dirty()).ShouldBe(true));
    SHOULD(ShouldlyString(child->is_render_dirty()).ShouldBe(true));

    // Re-measure/arrange to CLEAR the axes; render stays dirty (no render pass
    // here). Then an EQUAL Set is NO_CHANGE -> MUST NOT re-dirty.
    child->Measure(vaml::Size{ 400.0f, 400.0f });
    child->Arrange(vaml::Rect{ 0, 0, 400, 400 });
    SHOULD(ShouldlyString(child->is_measure_dirty()).ShouldBe(false));
    SHOULD(ShouldlyString(child->is_arrange_dirty()).ShouldBe(false));

    const auto rDirtyBefore = child->is_render_dirty();
    SHOULD(ShouldlyString(child->Set(id, Value::Int32(5))).ShouldBe(SetResult::NO_CHANGE));
    SHOULD(ShouldlyString(child->is_measure_dirty()).ShouldBe(false)); // NOT re-dirtied
    SHOULD(ShouldlyString(child->is_arrange_dirty()).ShouldBe(false));
    SHOULD(ShouldlyString(child->is_render_dirty()).ShouldBe(rDirtyBefore)); // unchanged
}
