// S122 (dep:S121) â€” LayoutSlot + final rect + DesiredSize + Margin + alignment.
//
// Resolves the S120 layout-slot sentinel (the real layout geometry now lives
// on UIElement + LayoutSlot.h). Under test:
//   1. compute_final_rect â€” the REAL alignment algorithm S124's Arrange will
//      consume: Stretch fills the inset slot when the child fits; Left/Top
//      arrange at desired; Center offsets by half; Right/Bottom offset by the
//      full difference; margin shrinks the available; S832 (11ad) child-wins:
//      desired > available keeps the FULL unclipped desired + needs_clip; margin
//      > slot clamps to 0 (never negative); Infinite desired keeps the available
//      bound (finite guard).
//   2. UIElement layout DPs â€” Margin (default 0, set/get roundtrip),
//      HorizontalAlignment/VerticalAlignment (default Stretch, set/get), all
//      attached-aware (resolve on a plain DependencyObject), wrong-tag ->
//      ValueTypeMismatch.
//   3. Slots â€” desired_size()/final_rect() default 0.
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

#include <cstdint>
#include <limits>

using vaml::DependencyObject;
using vaml::DependencyProperty;
using vaml::HorizontalAlignment;
using vaml::InvalidPropertyValueException;
using vaml::LayoutSlot;
using vaml::PropertyFlags;
using vaml::Rect;
using vaml::SetResult;
using vaml::Size;
using vaml::Thickness;
using vaml::UIElement;
using vaml::Value;
using vaml::ValueTypeMismatch;
using vaml::VerticalAlignment;
using vaml::has_flag;
using vaml::test::ShouldThrow;

namespace {

// Constructs a rect from origin + extent for readable slot literals.
Rect slot_of(float x, float y, float w, float h)
{
    return { x, y, w, h };
}

} // namespace

// --- compute_final_rect ------------------------------------------------------
//
// S832 (11ad): compute_final_rect now returns arrange_result{rect, needs_clip,
// clip_rect}. The rect is the (possibly OVERSIZED) child-wins arranged rect (WPF
// ArrangeCore FE.cs:4597-4624); needs_clip mirrors NeedsClipBounds; S833 adds
// clip_rect — the WPF GetLayoutClip SLOT-CLIENT window (FE.cs:4916-4929) the
// renderer scissors to when needs_clip.

TEST_CASE("S122: alignment — Stretch fills the inset slot (default both axes)")
{
    // Slot 100x50 at origin; child Stretch on both axes fills it exactly. The
    // desired (5x5) is smaller than the slot, so there is no overflow.
    LayoutSlot slot{ slot_of(10.0f, 20.0f, 100.0f, 50.0f) };
    const vaml::arrange_result ar = vaml::compute_final_rect(
        slot, Size{ 5.0f, 5.0f }, Thickness{}, HorizontalAlignment::Stretch, VerticalAlignment::Stretch);
    const Rect& r = ar.rect;
    SHOULD(ShouldlyString(r.x).ShouldBe(10.0f));
    SHOULD(ShouldlyString(r.y).ShouldBe(20.0f));
    SHOULD(ShouldlyString(r.width).ShouldBe(100.0f));
    SHOULD(ShouldlyString(r.height).ShouldBe(50.0f));
    SHOULD(ShouldlyString(ar.needs_clip).ShouldBe(false));
}

TEST_CASE("S122: alignment — Left/Top pinch to desired (origin corner)")
{
    // Child smaller than slot: Left+Top place it at the slot's top-left, size
    // = desired (100x30), ignoring the extra available space.
    LayoutSlot slot{ slot_of(0.0f, 0.0f, 400.0f, 200.0f) };
    const vaml::arrange_result ar = vaml::compute_final_rect(
        slot, Size{ 100.0f, 30.0f }, Thickness{}, HorizontalAlignment::Left, VerticalAlignment::Top);
    const Rect& r = ar.rect;
    SHOULD(ShouldlyString(r.x).ShouldBe(0.0f));
    SHOULD(ShouldlyString(r.y).ShouldBe(0.0f));
    SHOULD(ShouldlyString(r.width).ShouldBe(100.0f));
    SHOULD(ShouldlyString(r.height).ShouldBe(30.0f));
    SHOULD(ShouldlyString(ar.needs_clip).ShouldBe(false));
}

TEST_CASE("S122: alignment — Center offsets by half the leftover on both axes")
{
    // Desired 100x30 in a 400x200 slot, centered: x = (400-100)/2 = 150,
    // y = (200-30)/2 = 85.
    LayoutSlot slot{ slot_of(0.0f, 0.0f, 400.0f, 200.0f) };
    const vaml::arrange_result ar = vaml::compute_final_rect(
        slot, Size{ 100.0f, 30.0f }, Thickness{}, HorizontalAlignment::Center, VerticalAlignment::Center);
    const Rect& r = ar.rect;
    SHOULD(ShouldlyString(r.x).ShouldBe(150.0f));
    SHOULD(ShouldlyString(r.y).ShouldBe(85.0f));
    SHOULD(ShouldlyString(r.width).ShouldBe(100.0f));
    SHOULD(ShouldlyString(r.height).ShouldBe(30.0f));
    SHOULD(ShouldlyString(ar.needs_clip).ShouldBe(false));
}

TEST_CASE("S122: alignment — Right/Bottom offset by the full difference")
{
    // Desired 100x30 in a 400x200 slot, right/bottom:
    // x = 400-100 = 300, y = 200-30 = 170.
    LayoutSlot slot{ slot_of(0.0f, 0.0f, 400.0f, 200.0f) };
    const vaml::arrange_result ar = vaml::compute_final_rect(
        slot, Size{ 100.0f, 30.0f }, Thickness{}, HorizontalAlignment::Right, VerticalAlignment::Bottom);
    const Rect& r = ar.rect;
    SHOULD(ShouldlyString(r.x).ShouldBe(300.0f));
    SHOULD(ShouldlyString(r.y).ShouldBe(170.0f));
    SHOULD(ShouldlyString(r.width).ShouldBe(100.0f));
    SHOULD(ShouldlyString(r.height).ShouldBe(30.0f));
    SHOULD(ShouldlyString(ar.needs_clip).ShouldBe(false));
}

TEST_CASE("S832: margin strips both the slot and the desired; a squeezed Stretch child keeps its desired")
{
    // Margins 10 left, 20 top, 5 right, 5 bottom on a 100x50 slot:
    //   inset window = (100-10-5, 50-20-5) = 85x25; origin advances to (10,20).
    //   inner desired = (200-15, 200-25) = 185x175 (margin stripped).
    // S832 (11ad, FE.cs:4597-4624): the inner desired EXCEEDS the window, so a
    // Stretch child keeps its full unclipped desired (185x175) — the child wins
    // and needs_clip is true; the deferred S833 scissor trims it to 85x25.
    LayoutSlot slot{ slot_of(0.0f, 0.0f, 100.0f, 50.0f) };
    const vaml::arrange_result ar = vaml::compute_final_rect(
        slot, Size{ 200.0f, 200.0f }, Thickness{ 10.0f, 20.0f, 5.0f, 5.0f },
        HorizontalAlignment::Stretch, VerticalAlignment::Stretch);
    const Rect& r = ar.rect;
    SHOULD(ShouldlyString(r.x).ShouldBe(10.0f));
    SHOULD(ShouldlyString(r.y).ShouldBe(20.0f));
    SHOULD(ShouldlyString(r.width).ShouldBe(185.0f));
    SHOULD(ShouldlyString(r.height).ShouldBe(175.0f));
    SHOULD(ShouldlyString(ar.needs_clip).ShouldBe(true));
    // S833: the clip_rect is the SLOT-CLIENT window (inset 85x25 at slot origin
    // + margin 10/20), NOT the oversized arranged rect — the value the S208
    // renderer scissors to when needs_clip.
    SHOULD(ShouldlyString(ar.clip_rect.x).ShouldBe(10.0f));
    SHOULD(ShouldlyString(ar.clip_rect.y).ShouldBe(20.0f));
    SHOULD(ShouldlyString(ar.clip_rect.width).ShouldBe(85.0f));
    SHOULD(ShouldlyString(ar.clip_rect.height).ShouldBe(25.0f));
}

TEST_CASE("S832: desired > slot keeps the child at its unclipped desired (child-wins + clip)")
{
    // Desired 500x300, slot only 100x50; Left+Top arranges at the FULL unclipped
    // desired extent (500x300) — the child OVERFLOWS its slot (WPF
    // ArrangeCore FE.cs:4617-4623 non-Stretch arm) and needs_clip is true.
    // (Pre-S832 this clamped to the slot; the clamp was the drift.)
    LayoutSlot slot{ slot_of(0.0f, 0.0f, 100.0f, 50.0f) };
    const vaml::arrange_result ar = vaml::compute_final_rect(
        slot, Size{ 500.0f, 300.0f }, Thickness{}, HorizontalAlignment::Left, VerticalAlignment::Top);
    const Rect& r = ar.rect;
    SHOULD(ShouldlyString(r.width).ShouldBe(500.0f));
    SHOULD(ShouldlyString(r.height).ShouldBe(300.0f));
    SHOULD(ShouldlyString(r.x).ShouldBe(0.0f));
    SHOULD(ShouldlyString(r.y).ShouldBe(0.0f));
    SHOULD(ShouldlyString(ar.needs_clip).ShouldBe(true));
    // S833: the clip_rect is the 100x50 slot client (the slot bounds, margin 0)
    // — clipping the 500x300 arranged rect to THIS trims the overflow, whereas
    // clipping it to its own rect would clip nothing.
    SHOULD(ShouldlyString(ar.clip_rect.x).ShouldBe(0.0f));
    SHOULD(ShouldlyString(ar.clip_rect.y).ShouldBe(0.0f));
    SHOULD(ShouldlyString(ar.clip_rect.width).ShouldBe(100.0f));
    SHOULD(ShouldlyString(ar.clip_rect.height).ShouldBe(50.0f));
}

TEST_CASE("S122: margin larger than the slot clamps to zero — never negative")
{
    // Margin 200 on every edge of a 100x50 slot: inset clamps to 0 and the inner
    // desired also clamps to 0 (50-400 < 0), so size is 0 — no negative geometry
    // escapes the layout contract (NO-LAZY). No overflow => no clip.
    LayoutSlot slot{ slot_of(0.0f, 0.0f, 100.0f, 50.0f) };
    const vaml::arrange_result ar = vaml::compute_final_rect(
        slot, Size{ 50.0f, 50.0f }, Thickness{ 200.0f, 200.0f, 200.0f, 200.0f },
        HorizontalAlignment::Stretch, VerticalAlignment::Stretch);
    const Rect& r = ar.rect;
    SHOULD(ShouldlyString(r.width).ShouldBe(0.0f));
    SHOULD(ShouldlyString(r.height).ShouldBe(0.0f));
    SHOULD(ShouldlyString(r.x >= 0.0f).ShouldBe(true));
    SHOULD(ShouldlyString(r.y >= 0.0f).ShouldBe(true));
    SHOULD(ShouldlyString(ar.needs_clip).ShouldBe(false));
}

TEST_CASE("S122: Infinite desired keeps the available bound (non-Stretch)")
{
    // Infinite is the S123 Measure "unbounded available" sentinel. S832's finite
    // guard folds a non-finite inner desired to the inset window, so the child
    // keeps the finite slot (90x40) — infinity never escapes into geometry and
    // there is no overflow.
    LayoutSlot slot{ slot_of(0.0f, 0.0f, 90.0f, 40.0f) };
    const vaml::arrange_result ar = vaml::compute_final_rect(
        slot, Size::Infinite(), Thickness{}, HorizontalAlignment::Left, VerticalAlignment::Top);
    const Rect& r = ar.rect;
    SHOULD(ShouldlyString(r.width).ShouldBe(90.0f));
    SHOULD(ShouldlyString(r.height).ShouldBe(40.0f));
    SHOULD(ShouldlyString(ar.needs_clip).ShouldBe(false));
}

// --- UIElement layout DPs ----------------------------------------------------

TEST_CASE("S122: Margin â€” default 0; set/get roundtrip")
{
    UIElement el;
    SHOULD(ShouldlyString(el.margin().left).ShouldBe(0.0f));
    SHOULD(ShouldlyString(el.margin().top).ShouldBe(0.0f));
    SHOULD(ShouldlyString(el.margin().right).ShouldBe(0.0f));
    SHOULD(ShouldlyString(el.margin().bottom).ShouldBe(0.0f));

    SHOULD(ShouldlyString(el.set_margin(Thickness{ 2.0f, 3.0f, 4.0f, 5.0f })).ShouldBe(SetResult::APPLIED));
    const Thickness m = el.margin();
    SHOULD(ShouldlyString(m.left).ShouldBe(2.0f));
    SHOULD(ShouldlyString(m.top).ShouldBe(3.0f));
    SHOULD(ShouldlyString(m.right).ShouldBe(4.0f));
    SHOULD(ShouldlyString(m.bottom).ShouldBe(5.0f));
}

TEST_CASE("S122: HorizontalAlignment/VerticalAlignment â€” default Stretch; set/get roundtrip")
{
    UIElement el;
    // S11p (WPF-fidelity re-pointing): the enums are now WPF-true
    // (FrameworkElement.cs â€” HorizontalAlignment { Left=0, Center=1, Right=2,
    // Stretch=3 }; VerticalAlignment { Top=0, Center=1, Bottom=2, Stretch=3 }),
    // and the DP defaults are SYMBOLIC (Stretch, not a bare 0). Default:
    // horizontal_alignment()==Stretch(3), vertical_alignment()==Stretch(3).
    SHOULD(ShouldlyString(static_cast<std::int32_t>(el.horizontal_alignment())).ShouldBe(3));
    SHOULD(ShouldlyString(static_cast<std::int32_t>(el.vertical_alignment())).ShouldBe(3));

    SHOULD(ShouldlyString(el.set_horizontal_alignment(HorizontalAlignment::Center)).ShouldBe(SetResult::APPLIED));
    SHOULD(ShouldlyString(static_cast<std::int32_t>(el.horizontal_alignment())).ShouldBe(1));

    SHOULD(ShouldlyString(el.set_vertical_alignment(VerticalAlignment::Bottom)).ShouldBe(SetResult::APPLIED));
    SHOULD(ShouldlyString(static_cast<std::int32_t>(el.vertical_alignment())).ShouldBe(2));
}

TEST_CASE("S122: layout DPs resolve on a plain DependencyObject too (S043 attached-aware)")
{
    // All three layout DPs are GLOBALLY registered (self-registration during
    // the first UIElement ctor), so a plain object reads/writes them via the
    // S043 attached-aware global resolution.
    DependencyObject plain;

    (void)plain.Set(vaml::PropertyId::of("Margin", "FrameworkElement"), Value::Thickness(Thickness{ 1.0f, 1.0f, 1.0f, 1.0f }));
    SHOULD(ShouldlyString(plain.Get(vaml::PropertyId::of("Margin", "FrameworkElement")).get<Thickness>().left).ShouldBe(1.0f));

    (void)plain.Set(vaml::PropertyId::of("HorizontalAlignment", "FrameworkElement"), Value::Int32(1));
    SHOULD(ShouldlyString(plain.Get(vaml::PropertyId::of("HorizontalAlignment", "FrameworkElement")).get<std::int32_t>()).ShouldBe(1));

    (void)plain.Set(vaml::PropertyId::of("VerticalAlignment", "FrameworkElement"), Value::Int32(2));
    SHOULD(ShouldlyString(plain.Get(vaml::PropertyId::of("VerticalAlignment", "FrameworkElement")).get<std::int32_t>()).ShouldBe(2));
}

TEST_CASE("S122: layout DP wrong-tag write throws ValueTypeMismatch")
{
    UIElement el;
    // Margin is a Thickness; writing an Int32 payload to it is a wrong-tag
    // write -> ValueTypeMismatch, store unchanged (still 0).
    SHOULD(ShouldThrow<ValueTypeMismatch>([&] {
        (void)el.Set(vaml::PropertyId::of("Margin", "FrameworkElement"), Value::Int32(7));
    }));
    SHOULD(ShouldlyString(el.margin().left).ShouldBe(0.0f));

    // HorizontalAlignment is an Int32; a Double payload is wrong-tag.
    SHOULD(ShouldThrow<ValueTypeMismatch>([&] {
        (void)el.Set(vaml::PropertyId::of("HorizontalAlignment", "FrameworkElement"), Value::Double(1.0));
    }));
    // Store unchanged = the SYMBOLIC default Stretch=3 (S11p re-pointing).
    SHOULD(ShouldlyString(static_cast<std::int32_t>(el.horizontal_alignment())).ShouldBe(3));
}

TEST_CASE("S824a: alignment DPs — WPF boundary gate (0/1/2/3 resolve; 4/-1 throw; store unchanged)")
{
    UIElement el;
    const auto hId = vaml::PropertyId::of("HorizontalAlignment", "FrameworkElement");
    const auto vId = vaml::PropertyId::of("VerticalAlignment", "FrameworkElement");

    // Start from a non-default state so every in-domain Set below is a REAL
    // change (Set to the already-effective default Stretch=3 returns NO_CHANGE,
    // the no-op optimization — not a boundary signal).
    SHOULD(ShouldlyString(el.Set(hId, Value::Int32(1))).ShouldBe(SetResult::APPLIED)); // Center
    SHOULD(ShouldlyString(el.Set(vId, Value::Int32(2))).ShouldBe(SetResult::APPLIED)); // Bottom

    // The four in-domain values resolve — Left/Top(0) Center(1) Right/Bottom(2)
    // Stretch(3). WPF-true ordinals: HorizontalAlignment
    // { Left=0, Center=1, Right=2, Stretch=3 } (FE.cs:28), VerticalAlignment
    // { Top=0, Center=1, Bottom=2, Stretch=3 } (FE.cs:57).
    SHOULD(ShouldlyString(el.Set(hId, Value::Int32(0))).ShouldBe(SetResult::APPLIED));
    SHOULD(ShouldlyString(static_cast<std::int32_t>(el.horizontal_alignment())).ShouldBe(0));
    SHOULD(ShouldlyString(el.Set(hId, Value::Int32(2))).ShouldBe(SetResult::APPLIED));
    SHOULD(ShouldlyString(static_cast<std::int32_t>(el.horizontal_alignment())).ShouldBe(2));
    SHOULD(ShouldlyString(el.Set(hId, Value::Int32(3))).ShouldBe(SetResult::APPLIED));
    SHOULD(ShouldlyString(static_cast<std::int32_t>(el.horizontal_alignment())).ShouldBe(3));
    SHOULD(ShouldlyString(el.Set(vId, Value::Int32(0))).ShouldBe(SetResult::APPLIED));
    SHOULD(ShouldlyString(static_cast<std::int32_t>(el.vertical_alignment())).ShouldBe(0));
    SHOULD(ShouldlyString(el.Set(vId, Value::Int32(1))).ShouldBe(SetResult::APPLIED));
    SHOULD(ShouldlyString(static_cast<std::int32_t>(el.vertical_alignment())).ShouldBe(1));
    SHOULD(ShouldlyString(el.Set(vId, Value::Int32(3))).ShouldBe(SetResult::APPLIED));
    SHOULD(ShouldlyString(static_cast<std::int32_t>(el.vertical_alignment())).ShouldBe(3));

    // Out-of-domain values are REJECTED at the boundary (WPF
    // ValidateHorizontalAlignmentValue FE.cs:3751 / ValidateVerticalAlignmentValue
    // FE.cs:3783): the ValidateFunc is false -> InvalidPropertyValueException
    // (S810_1 disposition; WPF ArgumentException(InvalidPropertyValue)), and the
    // store stays UNCHANGED (still Stretch=3 from the last in-domain Set).
    SHOULD(ShouldThrow<InvalidPropertyValueException>([&] {
        (void)el.Set(hId, Value::Int32(4));
    }));
    SHOULD(ShouldThrow<InvalidPropertyValueException>([&] {
        (void)el.Set(hId, Value::Int32(-1));
    }));
    SHOULD(ShouldThrow<InvalidPropertyValueException>([&] {
        (void)el.Set(vId, Value::Int32(4));
    }));
    SHOULD(ShouldThrow<InvalidPropertyValueException>([&] {
        (void)el.Set(vId, Value::Int32(-1));
    }));
    SHOULD(ShouldlyString(static_cast<std::int32_t>(el.horizontal_alignment())).ShouldBe(3));
    SHOULD(ShouldlyString(static_cast<std::int32_t>(el.vertical_alignment())).ShouldBe(3));

    // The typed accessor path flows through the same gate: an enum value always
    // maps into 0/1/2/3, so it keeps resolving; a synthetic out-of-range enum is
    // impossible, but the generic path above already proves the boundary.
    SHOULD(ShouldlyString(el.set_horizontal_alignment(HorizontalAlignment::Left)).ShouldBe(SetResult::APPLIED));
    SHOULD(ShouldlyString(static_cast<std::int32_t>(el.horizontal_alignment())).ShouldBe(0));
    SHOULD(ShouldlyString(el.set_vertical_alignment(VerticalAlignment::Top)).ShouldBe(SetResult::APPLIED));
    SHOULD(ShouldlyString(static_cast<std::int32_t>(el.vertical_alignment())).ShouldBe(0));
}

TEST_CASE("S824a: alignment DPs carry AffectsArrange (WPF FE.cs:3748)")
{
    // Both alignment DPs are registered by owner "FrameworkElement" WITH
    // PropertyFlags::AffectsArrange — the WPF FE.cs:3748 flag — so a change
    // dirties the OWN arrange axis through the S818 Affects* invalidation
    // engine (no new mechanism, 11aa S818).
    const DependencyProperty& h = DependencyProperty::Get("HorizontalAlignment", "FrameworkElement");
    const DependencyProperty& v = DependencyProperty::Get("VerticalAlignment", "FrameworkElement");
    SHOULD(ShouldlyString(has_flag(h.flags(), PropertyFlags::AffectsArrange)).ShouldBe(true));
    SHOULD(ShouldlyString(has_flag(v.flags(), PropertyFlags::AffectsArrange)).ShouldBe(true));
}

// --- Slots -------------------------------------------------------------------

TEST_CASE("S122: desired_size()/final_rect() default to 0 until a layout pass")
{
    UIElement el;
    SHOULD(ShouldlyString(el.desired_size().width).ShouldBe(0.0f));
    SHOULD(ShouldlyString(el.desired_size().height).ShouldBe(0.0f));
    SHOULD(ShouldlyString(el.final_rect().x).ShouldBe(0.0f));
    SHOULD(ShouldlyString(el.final_rect().y).ShouldBe(0.0f));
    SHOULD(ShouldlyString(el.final_rect().width).ShouldBe(0.0f));
    SHOULD(ShouldlyString(el.final_rect().height).ShouldBe(0.0f));
}
