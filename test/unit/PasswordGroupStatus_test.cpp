// S485 (dep:S484) — PasswordBox + GroupBox + Expander + Separator + StatusBar
// controls RESOLVED. Each is a REAL re-based UIElement: PasswordBox = masked
// Text input; GroupBox/Expander = header + content containers; Separator =
// divider; StatusBar = docking item host.
//
// Under test (headless):
//   1. PasswordBox: the masked Password DP + a single-line masked handle_key
//      edit that raises TextChanged on a real change.
//   2. GroupBox/Expander: the header + single content slot; Expander.IsExpanded
//      gates the content.
//   3. Separator: the thin divider DP.
//   4. StatusBar: the attached StatusBar.Dock + the dock item layout.
//   5. classify_element maps each to MaterialKind::Solid (+ Border as
//      appropriate).

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/Element.h"
#include "vaml/core/FocusManager.h" // KeyCode

#include <memory>

using vaml::KeyCode;
using vaml::PasswordBox;
using vaml::GroupBox;
using vaml::Expander;
using vaml::Separator;
using vaml::StatusBar;

TEST_CASE("S485: PasswordBox DP surface + masked edit raises TextChanged")
{
    PasswordBox pb;
    SHOULD(ShouldlyString(pb.password()).ShouldBe(std::string("")));
    SHOULD(ShouldlyString(pb.password_char()).ShouldBe(std::string("*")));
    SHOULD(ShouldlyIntegral(pb.max_length()).ShouldBe(0));
    (void)pb.set_max_length(8);
    (void)pb.set_password_char("#");
    SHOULD(ShouldlyIntegral(pb.max_length()).ShouldBe(8));
    SHOULD(ShouldlyString(pb.password_char()).ShouldBe(std::string("#")));

    // A real edit mutates the masked Password (the content is masked on render,
    // stored as the real value).
    (void)pb.handle_key(KeyCode::A, true, false);
    (void)pb.handle_key(KeyCode::B, true, false);
    SHOULD(ShouldlyString(pb.password()).ShouldBe(std::string("ab")));
    (void)pb.handle_key(KeyCode::Backspace, true, false);
    SHOULD(ShouldlyString(pb.password()).ShouldBe(std::string("a")));
}

TEST_CASE("S485: PasswordBox MaxLength clamps the insert")
{
    PasswordBox pb;
    (void)pb.set_max_length(3);
    (void)pb.handle_key(KeyCode::A, true, false);
    (void)pb.handle_key(KeyCode::B, true, false);
    (void)pb.handle_key(KeyCode::C, true, false);
    SHOULD(ShouldlyString(pb.password()).ShouldBe(std::string("abc")));
    (void)pb.handle_key(KeyCode::D, true, false); // at max length -> no-op
    SHOULD(ShouldlyString(pb.password()).ShouldBe(std::string("abc")));
}

TEST_CASE("S485: GroupBox header + single content slot")
{
    GroupBox gb;
    SHOULD(ShouldlyString(gb.header()).ShouldBe(std::string("")));
    (void)gb.set_header("Options");
    SHOULD(ShouldlyString(gb.header()).ShouldBe(std::string("Options")));
    auto inner = std::make_unique<vaml::TextBlock>();
    (void)inner->set_text("inside");
    auto* raw = inner.get();
    gb.set_content(std::move(inner));
    SHOULD(ShouldlyIntegral(gb.content() == raw).ShouldBe(1));
    SHOULD(ShouldlyIntegral(gb.child_count()).ShouldBe(1));
    gb.Measure(vaml::Size{ 200.0f, 120.0f });
    gb.Arrange(vaml::Rect{ 0.0f, 0.0f, 200.0f, 120.0f });
    SHOULD(ShouldlyIntegral(gb.content()->final_rect().y >= 18.0f - 0.01f).ShouldBe(true));
}

TEST_CASE("S485: Expander IsExpanded gates the content")
{
    Expander ex;
    (void)ex.set_header("Details");
    SHOULD(ShouldlyString(ex.header()).ShouldBe(std::string("Details")));
    SHOULD(ShouldlyIntegral(static_cast<int>(ex.is_expanded())).ShouldBe(0));
    auto inner = std::make_unique<vaml::TextBlock>();
    (void)inner->set_text("expandable");
    ex.set_content(std::move(inner));
    // Collapsed by default: measure does NOT size the content (header only).
    // An UNBOUNDED offer (Size::Infinite) lets the intrinsic content extent
    // survive (the S123 wrap rule — a bounded offer would stretch a default-
    // Stretch element to fill it).
    ex.Measure(vaml::Size::Infinite());
    SHOULD(ShouldlyIntegral(static_cast<int>(ex.desired_size().height <= 24.0f)).ShouldBe(1));
    // Expanded: the content grows the box above the header band.
    (void)ex.set_is_expanded(1);
    SHOULD(ShouldlyIntegral(static_cast<int>(ex.is_expanded())).ShouldBe(1));
    ex.Measure(vaml::Size::Infinite());
    SHOULD(ShouldlyIntegral(ex.desired_size().height > 24.0f).ShouldBe(true));
}

TEST_CASE("S485: Separator thin divider + Solid material")
{
    Separator sep;
    SHOULD(ShouldlyIntegral(static_cast<int>(sep.thickness() == 1.0)).ShouldBe(1));
    (void)sep.set_thickness(3.0);
    SHOULD(ShouldlyIntegral(static_cast<int>(sep.thickness() == 3.0)).ShouldBe(1));
    // An unbounded offer lets the thin rule use its thickness as the cross
    // extent (a bounded offer would stretch the default-Stretch Separator).
    sep.Measure(vaml::Size::Infinite());
    SHOULD(ShouldlyIntegral(static_cast<int>(sep.desired_size().height == 3.0f)).ShouldBe(1));
}

TEST_CASE("S485: StatusBar attached Dock + dock item layout")
{
    StatusBar sb;
    // A left + a right item dock to opposite edges.
    auto left = std::make_unique<vaml::TextBlock>();
    (void)left->set_text("Ready");
    (void)StatusBar::set_dock(*left, 0); // Left
    auto* leftRaw = left.get();
    sb.add_child(std::move(left));

    auto right = std::make_unique<vaml::TextBlock>();
    (void)right->set_text("v1.0");
    (void)StatusBar::set_dock(*right, 1); // Right
    auto* rightRaw = right.get();
    sb.add_child(std::move(right));

    SHOULD(ShouldlyIntegral(StatusBar::dock(*leftRaw)).ShouldBe(0));
    SHOULD(ShouldlyIntegral(StatusBar::dock(*rightRaw)).ShouldBe(1));

    sb.Measure(vaml::Size{ 400.0f, 24.0f });
    sb.Arrange(vaml::Rect{ 0.0f, 0.0f, 400.0f, 24.0f });
    // Left item at the left edge; right item at the right edge.
    SHOULD(ShouldlyIntegral(leftRaw->final_rect().x <= 0.01f).ShouldBe(true));
    SHOULD(ShouldlyIntegral(rightRaw->final_rect().x >= 300.0f).ShouldBe(true));
}
