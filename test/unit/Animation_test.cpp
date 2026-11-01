// S510 (dep:S503) — ANIMATION engine: Storyboard + DoubleAnimation + keyframes
// + the per-element transform animation surface (drives the S208 RenderTransform
// path), change-driven off the viewer's tick cadence.
//
// WPF-FAITHFUL (rev 3, user-accepted): Timeline/Clock fused into the Storyboard
// for this POC (recorded NO-GAP at the DAG `Clock` row); BeginTime negative
// offset; By from base; AutoReverse ping-pong; RepeatBehavior; FillBehavior
// HoldEnd/Stop; keyframe Linear vs Discrete + KeyTime forms.
//
// Headless (Rail 1, ADR-021). doctest main comes from smoke_test.cpp.

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/Animation.h"
#include "vaml/core/Element.h"    // StackPanel + register_builtin_elements
#include "vaml/core/Storyboard.h"
#include "vaml/core/Transform.h"  // Matrix2D
#include "vaml/core/UIElement.h"

#include <cmath>
#include <memory>
#include <string>

using vaml::DiscreteDoubleKeyFrame;
using vaml::DoubleAnimation;
using vaml::DoubleAnimationUsingKeyFrames;
using vaml::KeyTime;
using vaml::LinearDoubleKeyFrame;
using vaml::Matrix2D;
using vaml::RepeatBehavior;
using vaml::StackPanel;
using vaml::Storyboard;
using vaml::TransformAnimation;
using vaml::TransformChannel;
using vaml::UIElement;
using vaml::Value;

namespace {

// Computes the rendered transform of `m` applied to (0,0) — the translated
// origin — for asserting translations simply.
void translate_of(const Matrix2D& m, double& dx, double& dy)
{
    dx = m.dx;
    dy = m.dy;
}

[[nodiscard]] bool feq(double a, double b, double eps = 1e-6)
{
    return std::fabs(a - b) <= eps;
}

} // namespace

// ---------------------------------------------------------------------------
// 1. KeyTime parsing
// ---------------------------------------------------------------------------

TEST_CASE("S510: parse_key_time time-span + fraction forms")
{
    KeyTime kt;
    REQUIRE(parse_key_time("0:0:1", kt));
    CHECK(kt.unit == vaml::KeyTimeUnit::TimeSeconds);
    CHECK(feq(kt.value, 1.0));

    REQUIRE(parse_key_time("0:2", kt));
    CHECK(kt.unit == vaml::KeyTimeUnit::TimeSeconds);
    CHECK(feq(kt.value, 2.0));

    REQUIRE(parse_key_time("5", kt));
    CHECK(feq(kt.value, 5.0));

    REQUIRE(parse_key_time("50%", kt));
    CHECK(kt.unit == vaml::KeyTimeUnit::Fraction);
    CHECK(feq(kt.value, 0.5));

    // Malformed -> false, never throws.
    CHECK(!parse_key_time("", kt));
    CHECK(!parse_key_time("abc", kt));
    CHECK(!parse_key_time("a:b", kt));
    CHECK(!parse_key_time("-1", kt));
}

TEST_CASE("S510: KeyTime::to_fraction converts time-span vs fraction")
{
    CHECK(feq(KeyTime::Time(2.0).to_fraction(4.0), 0.5));
    CHECK(feq(KeyTime::Fraction(0.75).to_fraction(100.0), 0.75));
    // Guard: non-positive total -> 0 (never NaN).
    CHECK(feq(KeyTime::Time(2.0).to_fraction(0.0), 0.0));
}

// ---------------------------------------------------------------------------
// 2. Timeline clock progress (pure authority)
// ---------------------------------------------------------------------------

TEST_CASE("S510: clock progress before begin / active / fill")
{
    double f = -1.0;
    // BeforeBegin (elapsed < 0): inactive.
    bool active = vaml::timeline_clock_progress(-1.0, 2.0, false, RepeatBehavior::repeat_for(1.0), f);
    CHECK(!active);
    CHECK(feq(f, 0.0));

    // Active within the cycle.
    active = vaml::timeline_clock_progress(1.0, 2.0, false, RepeatBehavior::repeat_for(1.0), f);
    CHECK(active);
    CHECK(feq(f, 0.5));

    // Fill reached at the end of one repeat -> inactive, fraction clamped to 1.
    active = vaml::timeline_clock_progress(2.0, 2.0, false, RepeatBehavior::repeat_for(1.0), f);
    CHECK(!active);
    CHECK(feq(f, 1.0));
}

TEST_CASE("S510: clock progress honors AutoReverse (ping-pong)")
{
    double f = 0.0;
    // duration=2.0, autoReverse -> one iteration = 4.0s (forward 0..2, reverse 2..4).
    bool active = vaml::timeline_clock_progress(0.5, 2.0, true, RepeatBehavior::repeat_for(1.0), f);
    CHECK(active);
    CHECK(feq(f, 0.25)); // forward at 25%

    active = vaml::timeline_clock_progress(1.5, 2.0, true, RepeatBehavior::repeat_for(1.0), f);
    CHECK(active);
    CHECK(feq(f, 0.75)); // still forward at 75%

    active = vaml::timeline_clock_progress(3.5, 2.0, true, RepeatBehavior::repeat_for(1.0), f);
    CHECK(active);
    CHECK(feq(f, 0.25)); // reverse, 25% back toward the start
}

TEST_CASE("S510: clock progress honors RepeatBehavior Forever")
{
    double f = 0.0;
    // Forever never fills; cycles wrap.
    bool active = vaml::timeline_clock_progress(5.0, 2.0, false, RepeatBehavior::repeat_forever(), f);
    CHECK(active);
    CHECK(feq(f, 0.5));
}

// ---------------------------------------------------------------------------
// 3. DoubleAnimation From/To/By
// ---------------------------------------------------------------------------

TEST_CASE("S510: DoubleAnimation value_at From/To at a fraction")
{
    DoubleAnimation a;
    a.set_from(0.0);
    a.set_to(10.0);
    CHECK(feq(a.value_at(500.0, 0.5), 5.0));
    CHECK(feq(a.value_at(500.0, 1.0), 10.0));
}

TEST_CASE("S510: DoubleAnimation By is a delta from the BASE (not From)")
{
    DoubleAnimation a;
    a.set_by(5.0); // no From -> effective From = base
    // base=100 -> To = 100 + 5 = 105; at fraction 0.5 -> 102.5.
    CHECK(feq(a.value_at(100.0, 0.5), 102.5));
    CHECK(feq(a.value_at(100.0, 1.0), 105.0));
}

TEST_CASE("S510: DoubleAnimation From + By resolves To = From + By")
{
    DoubleAnimation a;
    a.set_from(10.0);
    a.set_by(5.0);
    CHECK(feq(a.value_at(999.0, 1.0), 15.0));
    CHECK(feq(a.value_at(999.0, 0.0), 10.0));
}

// ---------------------------------------------------------------------------
// 4. Keyframes
// ---------------------------------------------------------------------------

TEST_CASE("S510: keyframe Linear interpolates the enclosing segment")
{
    DoubleAnimationUsingKeyFrames k;
    k.add_key_frame(std::make_unique<LinearDoubleKeyFrame>(0.0, KeyTime::Fraction(0.0)));
    k.add_key_frame(std::make_unique<LinearDoubleKeyFrame>(10.0, KeyTime::Fraction(1.0)));
    CHECK(feq(k.keyframe_value_at(0.5), 5.0));
    CHECK(feq(k.keyframe_value_at(0.0), 0.0));
    CHECK(feq(k.keyframe_value_at(1.0), 10.0));
}

TEST_CASE("S510: keyframe Discrete hops (holds until the next KeyTime)")
{
    DoubleAnimationUsingKeyFrames k;
    k.add_key_frame(std::make_unique<DiscreteDoubleKeyFrame>(1.0, KeyTime::Fraction(0.0)));
    k.add_key_frame(std::make_unique<DiscreteDoubleKeyFrame>(9.0, KeyTime::Fraction(1.0)));
    // Fraction before the next frame -> holds THIS frame's value.
    CHECK(feq(k.keyframe_value_at(0.0), 1.0));
    CHECK(feq(k.keyframe_value_at(0.9), 1.0));
    CHECK(feq(k.keyframe_value_at(1.0), 9.0));
}

TEST_CASE("S510: out-of-order keyframe KeyTime fails loud (never silent)")
{
    DoubleAnimationUsingKeyFrames k;
    k.add_key_frame(std::make_unique<LinearDoubleKeyFrame>(1.0, KeyTime::Fraction(0.8)));
    bool threw = false;
    try
    {
        k.add_key_frame(std::make_unique<LinearDoubleKeyFrame>(2.0, KeyTime::Fraction(0.5)));
    }
    catch (const std::invalid_argument&)
    {
        threw = true;
    }
    CHECK(threw);
}

// ---------------------------------------------------------------------------
// 5. TransformAnimation channel -> Matrix2D math
// ---------------------------------------------------------------------------

TEST_CASE("S510: channel_matrix translation/scale/rotate")
{
    const Matrix2D tx = vaml::channel_matrix(TransformChannel::TranslateX, 30.0);
    CHECK(feq(tx.dx, 30.0));
    CHECK(feq(tx.dy, 0.0));

    const Matrix2D ty = vaml::channel_matrix(TransformChannel::TranslateY, -12.0);
    CHECK(feq(ty.dx, 0.0));
    CHECK(feq(ty.dy, -12.0));

    const Matrix2D sx = vaml::channel_matrix(TransformChannel::ScaleX, 2.0);
    CHECK(feq(sx.m11, 2.0));
    CHECK(feq(sx.m22, 1.0));

    // Rotate 90 degrees: matrix has cos=0/sin=1.
    const Matrix2D rot = vaml::channel_matrix(TransformChannel::Rotate, 90.0);
    CHECK(feq(rot.m11, 0.0));
    CHECK(feq(rot.m12, -1.0));
    CHECK(feq(rot.m21, 1.0));
}

// ---------------------------------------------------------------------------
// 6. REAL Storyboard against a live element tree (change-driven tick)
// ---------------------------------------------------------------------------

TEST_CASE("S510: a REAL Storyboard animates Opacity + RenderTransform via tick")
{
    vaml::register_builtin_elements();
    StackPanel host;
    (void)host.set_opacity(1.0);
    // Give the stack a named TextBlock child to animate.
    auto* block = host.create_child<vaml::TextBlock>();
    (void)block;

    // Compose a storyboard:
    //   - fade the host Opacity from 1.0 -> 0.0 over 2s (From/To);
    //   - translate the HOST by +40 px in X over 2s (TransformAnimation
    //     channel, base captured at start).
    Storyboard sb;
    auto fade = std::make_unique<DoubleAnimation>();
    fade->set_target_name("");
    fade->set_target_property(vaml::PropertyId::of("Opacity"));
    fade->set_from(1.0);
    fade->set_to(0.0);
    fade->set_duration(2.0);

    auto move = std::make_unique<TransformAnimation>();
    move->set_target_name("");
    move->set_channel(TransformChannel::TranslateX);
    move->set_from(0.0);
    move->set_to(40.0);
    move->set_duration(2.0);

    sb.add_child(std::move(fade));
    sb.add_child(std::move(move));

    // Resolve names: empty -> the root (host).
    sb.start(&host, [](const std::string&) -> UIElement* { return nullptr; });

    // At t=0: base Opacity 1.0, base translation 0.
    sb.tick(0.0);
    CHECK(feq(host.opacity(), 1.0));
    {
        double dx = 0, dy = 0;
        translate_of(host.render_transform(), dx, dy);
        CHECK(feq(dx, 0.0));
    }

    // At t=1 (midpoint): Opacity 0.5, translate X 20.
    sb.tick(1.0);
    CHECK(feq(host.opacity(), 0.5));
    {
        double dx = 0, dy = 0;
        translate_of(host.render_transform(), dx, dy);
        CHECK(feq(dx, 20.0));
    }

    // At t=2 (end, HoldEnd default): Opacity 0.0, translate X 40.
    sb.tick(2.0);
    CHECK(feq(host.opacity(), 0.0));
    {
        double dx = 0, dy = 0;
        translate_of(host.render_transform(), dx, dy);
        CHECK(feq(dx, 40.0));
    }
}

TEST_CASE("S510: tick BEFORE start is a no-op")
{
    vaml::register_builtin_elements();
    StackPanel host;
    (void)host.set_opacity(0.7);
    Storyboard sb;
    auto fade = std::make_unique<DoubleAnimation>();
    fade->set_target_name("");
    fade->set_target_property(vaml::PropertyId::of("Opacity"));
    fade->set_from(0.7);
    fade->set_to(0.0);
    fade->set_duration(1.0);
    sb.add_child(std::move(fade));

    sb.tick(1.0); // never started -> no-op, Opacity unchanged
    CHECK(feq(host.opacity(), 0.7));
}

TEST_CASE("S510: FillBehavior HoldEnd holds the end value; Stop restores the base")
{
    vaml::register_builtin_elements();
    StackPanel host;

    // HoldEnd (default): animates base .5 -> to .1, complete(holdEnd) keeps .1.
    (void)host.set_opacity(0.5);
    Storyboard hold;
    auto fadeHold = std::make_unique<DoubleAnimation>();
    fadeHold->set_target_name("");
    fadeHold->set_target_property(vaml::PropertyId::of("Opacity"));
    fadeHold->set_to(0.1);               // no From -> From defaults to base .5
    fadeHold->set_duration(1.0);
    fadeHold->set_fill_behavior(vaml::FillBehavior::HoldEnd);
    hold.add_child(std::move(fadeHold));
    hold.start(&host, nullptr);
    hold.tick(0.5);
    CHECK(feq(host.opacity(), 0.3));     // interpolated .5->.1 at 50%
    hold.tick(1.0);
    CHECK(feq(host.opacity(), 0.1));     // end value
    hold.complete(true);                 // HoldEnd keeps .1
    CHECK(feq(host.opacity(), 0.1));

    // Stop: animates base .5 -> to .1, stop() restores the BASE .5.
    (void)host.set_opacity(0.5);
    Storyboard stop;
    auto fadeStop = std::make_unique<DoubleAnimation>();
    fadeStop->set_target_name("");
    fadeStop->set_target_property(vaml::PropertyId::of("Opacity"));
    fadeStop->set_to(0.1);
    fadeStop->set_duration(1.0);
    fadeStop->set_fill_behavior(vaml::FillBehavior::Stop);
    stop.add_child(std::move(fadeStop));
    stop.start(&host, nullptr);
    stop.tick(0.5);
    CHECK(feq(host.opacity(), 0.3));     // interpolated .5->.1 at 50%
    stop.complete(false);                // Stop restores the base
    CHECK(feq(host.opacity(), 0.5));
}

TEST_CASE("S510: transform animation COMPOSES over the captured base transform")
{
    vaml::register_builtin_elements();
    StackPanel host;
    // A pre-existing base transform: translate (10, 5).
    (void)host.set_render_transform(Matrix2D::translation(10.0f, 5.0f));

    Storyboard sb;
    auto ty = std::make_unique<TransformAnimation>();
    ty->set_target_name("");
    ty->set_channel(TransformChannel::TranslateY);
    ty->set_from(0.0);
    ty->set_to(20.0);
    ty->set_duration(1.0);
    sb.add_child(std::move(ty));
    sb.start(&host, nullptr);

    // At end: dy = 5 (base) + 20 (animated) = 25; dx stays 10 (base).
    sb.tick(1.0);
    {
        double dx = 0, dy = 0;
        translate_of(host.render_transform(), dx, dy);
        CHECK(feq(dx, 10.0));
        CHECK(feq(dy, 25.0));
    }
}

TEST_CASE("S510: AutoReverse DoubleAnimation ping-pongs the value")
{
    vaml::register_builtin_elements();
    StackPanel host;
    (void)host.set_opacity(0.0);

    Storyboard sb;
    auto fade = std::make_unique<DoubleAnimation>();
    fade->set_target_name("");
    fade->set_target_property(vaml::PropertyId::of("Opacity"));
    fade->set_from(0.0);
    fade->set_to(1.0);
    fade->set_duration(2.0);
    fade->set_auto_reverse(true);
    sb.add_child(std::move(fade));
    sb.start(&host, nullptr);

    sb.tick(0.5);  // forward 0.25
    CHECK(feq(host.opacity(), 0.25));
    sb.tick(1.0);  // forward 0.5
    CHECK(feq(host.opacity(), 0.5));
    sb.tick(3.5);  // reverse -> reflected (1 - 0.75 = 0.25)
    CHECK(feq(host.opacity(), 0.25));
}

TEST_CASE("S510: RepeatBehavior Forever cycles the value continuously")
{
    vaml::register_builtin_elements();
    StackPanel host;
    (void)host.set_opacity(0.0);

    Storyboard sb;
    auto fade = std::make_unique<DoubleAnimation>();
    fade->set_target_name("");
    fade->set_target_property(vaml::PropertyId::of("Opacity"));
    fade->set_from(0.0);
    fade->set_to(1.0);
    fade->set_duration(1.0);
    fade->set_repeat_behavior(RepeatBehavior::repeat_forever());
    sb.add_child(std::move(fade));
    sb.start(&host, nullptr);

    sb.tick(0.5);
    CHECK(feq(host.opacity(), 0.5));
    sb.tick(1.5); // second cycle -> 0.5 again
    CHECK(feq(host.opacity(), 0.5));
}
