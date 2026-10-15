// S122 (dep:S121) — LayoutSlot + final rect + DesiredSize + Margin + alignment.
//
// Resolves the S120 layout-slot sentinel (the real layout geometry now lives
// on UIElement + LayoutSlot.h). Under test:
//   1. compute_final_rect — the REAL alignment algorithm S124's Arrange will
//      consume: Stretch fills the inset slot; Left/Top pinch to desired;
//      Center offsets by half; Right/Bottom offset by the full difference;
//      margin shrinks the available; desired > available clamps; margin > slot
//      clamps to 0 (never negative); Infinite desired keeps the available
//      bound.
//   2. UIElement layout DPs — Margin (default 0, set/get roundtrip),
//      HorizontalAlignment/VerticalAlignment (default Stretch, set/get), all
//      attached-aware (resolve on a plain DependencyObject), wrong-tag ->
//      ValueTypeMismatch.
//   3. Slots — desired_size()/final_rect() default 0.
//
// Uses the Shouldly-for-doctest vocabulary (S036_5 standing convention).
// doctest's main comes from smoke_test.cpp.

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/DependencyObject.h"
#include "vaml/core/Geometry.h"
#include "vaml/core/LayoutSlot.h"
#include "vaml/core/Thickness.h"
#include "vaml/core/UIElement.h"
#include "vaml/core/Value.h"

#include <cstdint>
#include <limits>

using vaml::DependencyObject;
using vaml::HorizontalAlignment;
using vaml::LayoutSlot;
using vaml::Rect;
using vaml::SetResult;
using vaml::Size;
using vaml::Thickness;
using vaml::UIElement;
using vaml::Value;
using vaml::ValueTypeMismatch;
using vaml::VerticalAlignment;
using vaml::test::ShouldThrow;

namespace {

// Constructs a rect from origin + extent for readable slot literals.
Rect slot_of(float x, float y, float w, float h)
{
    return { x, y, w, h };
}

} // namespace

// --- compute_final_rect ------------------------------------------------------

TEST_CASE("S122: alignment — Stretch fills the inset slot (default both axes)")
{
    // Slot 100x50 at origin; child Stretch on both axes fills it exactly.
    LayoutSlot slot{ slot_of(10.0f, 20.0f, 100.0f, 50.0f) };
    const Rect r = vaml::compute_final_rect(
        slot, Size{ 5.0f, 5.0f }, Thickness{}, HorizontalAlignment::Stretch, VerticalAlignment::Stretch);
    SHOULD(ShouldlyString(r.x).ShouldBe(10.0f));
    SHOULD(ShouldlyString(r.y).ShouldBe(20.0f));
    SHOULD(ShouldlyString(r.width).ShouldBe(100.0f));
    SHOULD(ShouldlyString(r.height).ShouldBe(50.0f));
}

TEST_CASE("S122: alignment — Left/Top pinch to desired (origin corner)")
{
    // Child smaller than slot: Left+Top place it at the slot's top-left, size
    // = desired (100x30), ignoring the extra available space.
    LayoutSlot slot{ slot_of(0.0f, 0.0f, 400.0f, 200.0f) };
    const Rect r = vaml::compute_final_rect(
        slot, Size{ 100.0f, 30.0f }, Thickness{}, HorizontalAlignment::Left, VerticalAlignment::Top);
    SHOULD(ShouldlyString(r.x).ShouldBe(0.0f));
    SHOULD(ShouldlyString(r.y).ShouldBe(0.0f));
    SHOULD(ShouldlyString(r.width).ShouldBe(100.0f));
    SHOULD(ShouldlyString(r.height).ShouldBe(30.0f));
}

TEST_CASE("S122: alignment — Center offsets by half the leftover on both axes")
{
    // Desired 100x30 in a 400x200 slot, centered: x = (400-100)/2 = 150,
    // y = (200-30)/2 = 85.
    LayoutSlot slot{ slot_of(0.0f, 0.0f, 400.0f, 200.0f) };
    const Rect r = vaml::compute_final_rect(
        slot, Size{ 100.0f, 30.0f }, Thickness{}, HorizontalAlignment::Center, VerticalAlignment::Center);
    SHOULD(ShouldlyString(r.x).ShouldBe(150.0f));
    SHOULD(ShouldlyString(r.y).ShouldBe(85.0f));
    SHOULD(ShouldlyString(r.width).ShouldBe(100.0f));
    SHOULD(ShouldlyString(r.height).ShouldBe(30.0f));
}

TEST_CASE("S122: alignment — Right/Bottom offset by the full difference")
{
    // Desired 100x30 in a 400x200 slot, right/bottom:
    // x = 400-100 = 300, y = 200-30 = 170.
    LayoutSlot slot{ slot_of(0.0f, 0.0f, 400.0f, 200.0f) };
    const Rect r = vaml::compute_final_rect(
        slot, Size{ 100.0f, 30.0f }, Thickness{}, HorizontalAlignment::Right, VerticalAlignment::Bottom);
    SHOULD(ShouldlyString(r.x).ShouldBe(300.0f));
    SHOULD(ShouldlyString(r.y).ShouldBe(170.0f));
    SHOULD(ShouldlyString(r.width).ShouldBe(100.0f));
    SHOULD(ShouldlyString(r.height).ShouldBe(30.0f));
}

TEST_CASE("S122: margin shrinks the available slot; origin advances by left/top")
{
    // Margins 10 left, 20 top, 5 right, 5 bottom on a 100x50 slot:
    // available = (100-10-5, 50-20-5) = 85x25; origin advances to (10,20).
    // Stretch fills the remaining.
    LayoutSlot slot{ slot_of(0.0f, 0.0f, 100.0f, 50.0f) };
    const Rect r = vaml::compute_final_rect(
        slot, Size{ 200.0f, 200.0f }, Thickness{ 10.0f, 20.0f, 5.0f, 5.0f },
        HorizontalAlignment::Stretch, VerticalAlignment::Stretch);
    SHOULD(ShouldlyString(r.x).ShouldBe(10.0f));
    SHOULD(ShouldlyString(r.y).ShouldBe(20.0f));
    SHOULD(ShouldlyString(r.width).ShouldBe(85.0f));
    SHOULD(ShouldlyString(r.height).ShouldBe(25.0f));
}

TEST_CASE("S122: desired > available clamps the child to the available (non-Stretch)")
{
    // Desired 500x300 but slot is only 100x50; Left+Top clamps to the slot.
    LayoutSlot slot{ slot_of(0.0f, 0.0f, 100.0f, 50.0f) };
    const Rect r = vaml::compute_final_rect(
        slot, Size{ 500.0f, 300.0f }, Thickness{}, HorizontalAlignment::Left, VerticalAlignment::Top);
    SHOULD(ShouldlyString(r.width).ShouldBe(100.0f));
    SHOULD(ShouldlyString(r.height).ShouldBe(50.0f));
    SHOULD(ShouldlyString(r.x).ShouldBe(0.0f));
    SHOULD(ShouldlyString(r.y).ShouldBe(0.0f));
}

TEST_CASE("S122: margin larger than the slot clamps to zero — never negative")
{
    // Margin 200 on every edge of a 100x50 slot: available clamps to 0, so the
    // child size is 0 and the origin is the (inset) slot origin — no negative
    // geometry escapes the layout contract (NO-LAZY).
    LayoutSlot slot{ slot_of(0.0f, 0.0f, 100.0f, 50.0f) };
    const Rect r = vaml::compute_final_rect(
        slot, Size{ 50.0f, 50.0f }, Thickness{ 200.0f, 200.0f, 200.0f, 200.0f },
        HorizontalAlignment::Stretch, VerticalAlignment::Stretch);
    SHOULD(ShouldlyString(r.width).ShouldBe(0.0f));
    SHOULD(ShouldlyString(r.height).ShouldBe(0.0f));
    SHOULD(ShouldlyString(r.x >= 0.0f).ShouldBe(true));
    SHOULD(ShouldlyString(r.y >= 0.0f).ShouldBe(true));
}

TEST_CASE("S122: Infinite desired keeps the available bound (non-Stretch)")
{
    // Infinite is the S123 Measure "unbounded available" sentinel; a non-
    // Stretch alignment must still clamp to the slot's finite available.
    LayoutSlot slot{ slot_of(0.0f, 0.0f, 90.0f, 40.0f) };
    const Rect r = vaml::compute_final_rect(
        slot, Size::Infinite(), Thickness{}, HorizontalAlignment::Left, VerticalAlignment::Top);
    SHOULD(ShouldlyString(r.width).ShouldBe(90.0f));
    SHOULD(ShouldlyString(r.height).ShouldBe(40.0f));
}

// --- UIElement layout DPs ----------------------------------------------------

TEST_CASE("S122: Margin — default 0; set/get roundtrip")
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

TEST_CASE("S122: HorizontalAlignment/VerticalAlignment — default Stretch; set/get roundtrip")
{
    UIElement el;
    SHOULD(ShouldlyString(static_cast<std::int32_t>(el.horizontal_alignment())).ShouldBe(0));
    SHOULD(ShouldlyString(static_cast<std::int32_t>(el.vertical_alignment())).ShouldBe(0));

    SHOULD(ShouldlyString(el.set_horizontal_alignment(HorizontalAlignment::Center)).ShouldBe(SetResult::APPLIED));
    SHOULD(ShouldlyString(static_cast<std::int32_t>(el.horizontal_alignment())).ShouldBe(2));

    SHOULD(ShouldlyString(el.set_vertical_alignment(VerticalAlignment::Bottom)).ShouldBe(SetResult::APPLIED));
    SHOULD(ShouldlyString(static_cast<std::int32_t>(el.vertical_alignment())).ShouldBe(3));
}

TEST_CASE("S122: layout DPs resolve on a plain DependencyObject too (S043 attached-aware)")
{
    // All three layout DPs are GLOBALLY registered (self-registration during
    // the first UIElement ctor), so a plain object reads/writes them via the
    // S043 attached-aware global resolution.
    DependencyObject plain;

    (void)plain.Set(vaml::PropertyId::of("Margin"), Value::Thickness(Thickness{ 1.0f, 1.0f, 1.0f, 1.0f }));
    SHOULD(ShouldlyString(plain.Get(vaml::PropertyId::of("Margin")).get<Thickness>().left).ShouldBe(1.0f));

    (void)plain.Set(vaml::PropertyId::of("HorizontalAlignment"), Value::Int32(1));
    SHOULD(ShouldlyString(plain.Get(vaml::PropertyId::of("HorizontalAlignment")).get<std::int32_t>()).ShouldBe(1));

    (void)plain.Set(vaml::PropertyId::of("VerticalAlignment"), Value::Int32(2));
    SHOULD(ShouldlyString(plain.Get(vaml::PropertyId::of("VerticalAlignment")).get<std::int32_t>()).ShouldBe(2));
}

TEST_CASE("S122: layout DP wrong-tag write throws ValueTypeMismatch")
{
    UIElement el;
    // Margin is a Thickness; writing an Int32 payload to it is a wrong-tag
    // write -> ValueTypeMismatch, store unchanged (still 0).
    SHOULD(ShouldThrow<ValueTypeMismatch>([&] {
        (void)el.Set(vaml::PropertyId::of("Margin"), Value::Int32(7));
    }));
    SHOULD(ShouldlyString(el.margin().left).ShouldBe(0.0f));

    // HorizontalAlignment is an Int32; a Double payload is wrong-tag.
    SHOULD(ShouldThrow<ValueTypeMismatch>([&] {
        (void)el.Set(vaml::PropertyId::of("HorizontalAlignment"), Value::Double(1.0));
    }));
    SHOULD(ShouldlyString(static_cast<std::int32_t>(el.horizontal_alignment())).ShouldBe(0));
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
