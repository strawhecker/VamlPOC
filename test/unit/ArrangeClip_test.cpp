// S834 (dep:S833) — the CLIPTOBOUNDS DP: the explicit WPF layout-clip opt-in.
//
// WPF ground truth (Rule-2 source audit, E:\GitRepos\wpf):
//   * UIElement.cs:3338-3345 registers `"ClipToBounds"` as `typeof(bool)` on
//     owner `typeof(UIElement)` with default `BooleanBoxes.FalseBox` and a plain
//     `PropertyMetadata` (NO FrameworkPropertyMetadata.Inherits, NO ValidateFunc).
//   * UIElement.cs:3347-3356 `ClipToBounds_Changed` calls `InvalidateArrange()`
//     — reproduced here by the DP's `PropertyFlags::AffectsArrange` (S818).
//   * FrameworkElement.GetLayoutClip (FE.cs:4874) gates on
//     `NeedsClipBounds || ClipToBounds`, and `needToClipSlot = ClipToBounds ||
//     clippingSize < inkSize` (FE.cs:4917-4923) — so a `ClipToBounds="true"`
//     element whose content FITS is STILL clipped to its layout slot (the
//     forced-mode / closure-audit case), distinct from the S832 conditional
//     overflow mode. The S833 render gate becomes
//     `needs_clip() || clip_to_bounds()`.
//
// Under test:
//   1. DP registration metadata: owner "UIElement", boundType Bool, default
//      false, AffectsArrange set, Inherits NOT set.
//   2. Typed accessors round-trip (set_clip_to_bounds / clip_to_bounds).
//   3. NOT inherited (a child under a ClipToBounds=true parent reads false).
//   4. S043 attached-aware global resolution — the pair-keyed DP resolves on a
//      plain DependencyObject via the global registry (Set/Get agreement).
//   5. Wrong-tag boundary -> ValueTypeMismatch (the DP is Bool, not Int32).
//   6. AffectsArrange runtime effect — a Set marks the element arrange-dirty.
//   7. The forced-mode closure audit — a FITTING element with ClipToBounds=true
//      keeps needs_clip()==false yet `needs_clip() || clip_to_bounds()`==true
//      and its clip_rect() is the slot-client window (so the renderer clips it
//      to its slot even though the content fits).
//
// Uses the Shouldly-for-doctest vocabulary (S036_5 standing convention).
// doctest's main comes from smoke_test.cpp.

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/DependencyObject.h"
#include "vaml/core/DependencyProperty.h"
#include "vaml/core/Geometry.h"
#include "vaml/core/LayoutSlot.h"
#include "vaml/core/PropertyFlags.h"
#include "vaml/core/Thickness.h"
#include "vaml/core/UIElement.h"
#include "vaml/core/Value.h"

using vaml::DependencyObject;
using vaml::DependencyProperty;
using vaml::PropertyFlags;
using vaml::PropertyId;
using vaml::Rect;
using vaml::SetResult;
using vaml::Size;
using vaml::UIElement;
using vaml::ValueType;
using vaml::ValueTypeMismatch;

namespace {

Rect rect_of(float x, float y, float w, float h)
{
    return { x, y, w, h };
}

// A leaf carrying a real content extent through the S123/S124 seam so the
// Arrange pairing (desired vs slot) is concrete — the S124 Arrange_test pattern.
class ContentLeaf : public UIElement
{
public:
    explicit ContentLeaf(Size natural) : natural_(natural) {}

protected:
    Size MeasureOverride(const Size& available) const override
    {
        (void)available;
        return natural_;
    }

private:
    Size natural_;
};

} // namespace

// --- 1. DP registration metadata ------------------------------------------------

TEST_CASE("S834: ClipToBounds DP — UIElement owner, Bool, default false, AffectsArrange, not inherited")
{
    const DependencyProperty& dp = DependencyProperty::Get("ClipToBounds", "UIElement");
    CHECK(static_cast<int>(dp.metadata().boundType()) == static_cast<int>(ValueType::Bool));
    CHECK(dp.metadata().defaultValue().get<bool>() == false);            // default FALSE
    // UIElement.cs:3347-3356 — InvalidateArrange on change = AffectsArrange.
    CHECK(vaml::has_flag(dp.flags(), PropertyFlags::AffectsArrange));
    // UIElement.cs:3343 — plain PropertyMetadata, NO Inherits flag.
    CHECK(!vaml::has_flag(dp.flags(), PropertyFlags::Inherits));
}

// --- 2. typed accessors round-trip ----------------------------------------------

TEST_CASE("S834: ClipToBounds typed accessors round-trip (default false -> true -> false)")
{
    UIElement el;
    SHOULD(ShouldlyString(el.clip_to_bounds()).ShouldBe(false));
    (void)el.set_clip_to_bounds(true);
    SHOULD(ShouldlyString(el.clip_to_bounds()).ShouldBe(true));
    (void)el.set_clip_to_bounds(false);
    SHOULD(ShouldlyString(el.clip_to_bounds()).ShouldBe(false));
}

// --- 3. NOT inherited -----------------------------------------------------------

TEST_CASE("S834: ClipToBounds is NOT inherited — a child under a true parent reads false")
{
    UIElement parent;
    (void)parent.set_clip_to_bounds(true);
    UIElement* child = parent.create_child<UIElement>();
    // The parent opted in; the child's OWN local stays the default false (WPF
    // ClipToBounds has no Inherits — it does not cascade to visual children).
    SHOULD(ShouldlyString(parent.clip_to_bounds()).ShouldBe(true));
    SHOULD(ShouldlyString(child->clip_to_bounds()).ShouldBe(false));
}

// --- 4. S043 attached-aware global resolution -----------------------------------

TEST_CASE("S834: ClipToBounds resolves attached-aware on a plain DependencyObject (S043)")
{
    // The pair-keyed DP resolves through the global registry on ANY
    // DependencyObject — the S043 attached-aware contract — and the direct
    // Set/Get path agrees with the typed UIElement accessor.
    DependencyObject obj;
    const PropertyId id = PropertyId::of("ClipToBounds", "UIElement");
    SHOULD(ShouldlyString(obj.Get(id).get<bool>()).ShouldBe(false));
    (void)obj.Set(id, vaml::Value::Bool(true));
    SHOULD(ShouldlyString(obj.Get(id).get<bool>()).ShouldBe(true));

    UIElement el;
    (void)el.Set(id, vaml::Value::Bool(true));
    SHOULD(ShouldlyString(el.clip_to_bounds()).ShouldBe(true));
}

// --- 5. wrong-tag boundary ------------------------------------------------------

TEST_CASE("S834: ClipToBounds rejects a non-Bool tag at the Set boundary (ValueTypeMismatch)")
{
    UIElement el;
    // The DP is Bool — an Int32 payload is a type mismatch (the NO-LAZY tag
    // guard), and the store is left at the default false.
    CHECK_THROWS_AS((void)el.Set(PropertyId::of("ClipToBounds", "UIElement"), vaml::Value::Int32(1)),
                    ValueTypeMismatch);
    SHOULD(ShouldlyString(el.clip_to_bounds()).ShouldBe(false));
}

// --- 6. AffectsArrange runtime effect -------------------------------------------

TEST_CASE("S834: a ClipToBounds Set marks the element arrange-dirty (AffectsArrange)")
{
    UIElement el;
    el.Arrange(rect_of(0.0f, 0.0f, 50.0f, 50.0f)); // a completed pass clears the bit
    SHOULD(ShouldlyString(el.is_arrange_dirty()).ShouldBe(false));
    (void)el.set_clip_to_bounds(true);              // a real change -> AffectsArrange
    SHOULD(ShouldlyString(el.is_arrange_dirty()).ShouldBe(true));
}

// --- 7. the forced-mode closure audit -------------------------------------------

TEST_CASE("S834: a fitting ClipToBounds element is STILL clipped to its slot (forced mode)")
{
    // A leaf that FITS its 100x100 slot (natural 10x10, default Stretch fills).
    ContentLeaf leaf{ Size{ 10.0f, 10.0f } };
    leaf.Measure(Size{ 100.0f, 100.0f });
    leaf.Arrange(rect_of(0.0f, 0.0f, 100.0f, 100.0f));

    // Fitting -> the S832 CONDITIONAL mode is off: no overflow, needs_clip false.
    SHOULD(ShouldlyString(leaf.needs_clip()).ShouldBe(false));
    SHOULD(ShouldlyString(leaf.final_rect().width).ShouldBe(100.0f));
    SHOULD(ShouldlyString(leaf.final_rect().height).ShouldBe(100.0f));

    // With ClipToBounds default false, the S833 gate is inactive (no clip).
    SHOULD(ShouldlyString(leaf.clip_to_bounds()).ShouldBe(false));
    SHOULD(ShouldlyString(leaf.needs_clip() || leaf.clip_to_bounds()).ShouldBe(false));

    // Opt into the forced mode. The content STILL fits (needs_clip stays false),
    // but the S834 gate now FIRES — WPF's `NeedsClipBounds || ClipToBounds`
    // (FE.cs:4874) forces the layout-slot clip even for a fitting element.
    (void)leaf.set_clip_to_bounds(true);
    SHOULD(ShouldlyString(leaf.needs_clip()).ShouldBe(false));      // still fits
    SHOULD(ShouldlyString(leaf.clip_to_bounds()).ShouldBe(true));   // opted in
    SHOULD(ShouldlyString(leaf.needs_clip() || leaf.clip_to_bounds()).ShouldBe(true));

    // The scissor target is the SLOT-CLIENT window (clip_rect). For a fitting
    // Stretch element with margin 0 this equals the slot (0,0,100,100) — so the
    // renderer clips the element to its slot even though nothing overflows.
    SHOULD(ShouldlyString(leaf.clip_rect().x).ShouldBe(0.0f));
    SHOULD(ShouldlyString(leaf.clip_rect().y).ShouldBe(0.0f));
    SHOULD(ShouldlyString(leaf.clip_rect().width).ShouldBe(100.0f));
    SHOULD(ShouldlyString(leaf.clip_rect().height).ShouldBe(100.0f));
}

TEST_CASE("S834: ClipToBounds forces the slot-client clip even with a margin (forced mode)")
{
    // A fitting leaf with a margin: the slot-client window is the inset slot
    // (slot shrunk by the margin, origin advanced) — the S833 clip_rect — and
    // ClipToBounds forces the renderer to scissor to exactly that window even
    // though the content fits. This proves the forced clip uses the slot client,
    // not the (Stretch-filled) arranged rect.
    ContentLeaf leaf{ Size{ 10.0f, 10.0f } };
    (void)leaf.set_margin(vaml::Thickness{ 10.0f, 20.0f, 5.0f, 5.0f });
    leaf.Measure(Size{ 200.0f, 200.0f });
    leaf.Arrange(rect_of(0.0f, 0.0f, 100.0f, 50.0f));

    (void)leaf.set_clip_to_bounds(true);
    SHOULD(ShouldlyString(leaf.needs_clip()).ShouldBe(false));    // content fits
    SHOULD(ShouldlyString(leaf.clip_to_bounds()).ShouldBe(true)); // forced mode

    // Slot client = {slot.x+ml, slot.y+mt, insetW, insetH} = {10,20,85,25}.
    SHOULD(ShouldlyString(leaf.clip_rect().x).ShouldBe(10.0f));
    SHOULD(ShouldlyString(leaf.clip_rect().y).ShouldBe(20.0f));
    SHOULD(ShouldlyString(leaf.clip_rect().width).ShouldBe(85.0f));
    SHOULD(ShouldlyString(leaf.clip_rect().height).ShouldBe(25.0f));
}
