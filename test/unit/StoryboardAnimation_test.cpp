// S510 (dep:S503) — the REAL Storyboard-on-a-live-tree suite (Storyboard.cpp).
//
// Complements Animation_test.cpp's primitives: this file owns the end-to-end
// Storyboard lifecycle tests — start/tick/complete/FillBehavior/repeat/
// autoReverse/name-resolution/animation-of-a-named-child — against a LIVE
// StackPanel+TextBlock tree (the visual-motion surface's change-driven tick).
//
// Headless (Rail 1, ADR-021). doctest main comes from smoke_test.cpp.

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/Animation.h"
#include "vaml/core/Element.h"
#include "vaml/core/Storyboard.h"
#include "vaml/core/Transform.h"
#include "vaml/core/UIElement.h"

#include <cmath>
#include <memory>
#include <string>

using vaml::DoubleAnimation;
using vaml::RepeatBehavior;
using vaml::StackPanel;
using vaml::Storyboard;
using vaml::TextBlock;
using vaml::TransformAnimation;
using vaml::TransformChannel;
using vaml::UIElement;

namespace {

[[nodiscard]] bool feq(double a, double b, double eps = 1e-6)
{
    return std::fabs(a - b) <= eps;
}

} // namespace

TEST_CASE("S510: a Storyboard animates a NAMED child resolved by the resolver")
{
    vaml::register_builtin_elements();
    StackPanel host;
    auto* block = host.create_child<vaml::TextBlock>();
    // x:Name "FadeMe" (simulated by an internal name map the resolver uses).
    (void)block->set_opacity(1.0);
    struct Ctx
    {
        StackPanel* host;
        TextBlock*  block;
    };
    Ctx ctx{ &host, block };

    Storyboard sb;
    auto fade = std::make_unique<DoubleAnimation>();
    fade->set_target_name("FadeMe");
    fade->set_target_property(vaml::PropertyId::of("Opacity"));
    fade->set_from(1.0);
    fade->set_to(0.0);
    fade->set_duration(2.0);
    sb.add_child(std::move(fade));

    sb.start(&host, [&ctx](const std::string& n) -> UIElement* {
        return (n == "FadeMe") ? ctx.block : nullptr;
    });

    sb.tick(1.0);
    CHECK(feq(block->opacity(), 0.5));
    sb.tick(2.0);
    CHECK(feq(block->opacity(), 0.0));
}

TEST_CASE("S510: an unresolvable named target is skipped (recorded, not a crash)")
{
    vaml::register_builtin_elements();
    StackPanel host;
    (void)host.set_opacity(1.0);

    Storyboard sb;
    auto fade = std::make_unique<DoubleAnimation>();
    fade->set_target_name("Ghost");
    fade->set_target_property(vaml::PropertyId::of("Opacity"));
    fade->set_from(0.0);
    fade->set_to(1.0);
    fade->set_duration(1.0);
    sb.add_child(std::move(fade));

    // The resolver returns nullptr for "Ghost" -> animation skipped.
    sb.start(&host, [](const std::string&) -> UIElement* { return nullptr; });
    sb.tick(1.0);
    CHECK(feq(host.opacity(), 1.0)); // root untouched
}

TEST_CASE("S510: storyboard tick drives the S208 RenderTransform channel live")
{
    vaml::register_builtin_elements();
    StackPanel host;
    (void)host.set_render_transform(vaml::Matrix2D::translation(10.0f, 0.0f));

    Storyboard sb;
    auto move = std::make_unique<TransformAnimation>();
    move->set_target_name("");
    move->set_channel(TransformChannel::TranslateX);
    move->set_from(0.0);
    move->set_to(50.0);
    move->set_duration(1.0);
    sb.add_child(std::move(move));
    sb.start(&host, nullptr);

    sb.tick(0.5);
    CHECK(feq(host.render_transform().dx, 10.0f + 25.0f));
    sb.tick(1.0);
    CHECK(feq(host.render_transform().dx, 10.0f + 50.0f));
}

TEST_CASE("S510: RepeatBehavior forever on a transform channel keeps cycling")
{
    vaml::register_builtin_elements();
    StackPanel host;

    Storyboard sb;
    auto move = std::make_unique<TransformAnimation>();
    move->set_target_name("");
    move->set_channel(TransformChannel::TranslateY);
    move->set_from(0.0);
    move->set_to(10.0);
    move->set_duration(1.0);
    move->set_repeat_behavior(RepeatBehavior::repeat_forever());
    sb.add_child(std::move(move));
    sb.start(&host, nullptr);

    sb.tick(0.5);
    CHECK(feq(host.render_transform().dy, 5.0));
    sb.tick(1.5); // second cycle -> back at midpoint
    CHECK(feq(host.render_transform().dy, 5.0));
}

TEST_CASE("S510: stop() restores the captured base transform (FillBehavior Stop)")
{
    vaml::register_builtin_elements();
    StackPanel host;
    (void)host.set_render_transform(vaml::Matrix2D::translation(7.0f, 0.0f));

    Storyboard sb;
    auto move = std::make_unique<TransformAnimation>();
    move->set_target_name("");
    move->set_channel(TransformChannel::TranslateX);
    move->set_from(0.0);
    move->set_to(100.0);
    move->set_duration(1.0);
    move->set_fill_behavior(vaml::FillBehavior::Stop);
    sb.add_child(std::move(move));
    sb.start(&host, nullptr);

    sb.tick(0.5);
    CHECK(feq(host.render_transform().dx, 7.0f + 50.0f));
    sb.stop();
    CHECK(feq(host.render_transform().dx, 7.0f)); // base restored
}
