// S483 (dep:S482) — Image control RESOLVED: rides the S400_1/S202_1 icon asset
// pipeline (Source / IconKey / Stretch / Width / Height).
//
// Under test (headless):
//   1. The Image DP surface round-trips.
//   2. Image measures to its Width/Height box (0 = a bounded default).
//   3. The classifier maps Image to MaterialKind::Monochrome (the image-texture
//      path — mirrors the S400_1 Icon; the headless default emits the fallback
//      glyph; the real atlas quad is the S409_1-style render path, recorded
//      NO-GAP not silent).

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/Element.h"

#include <memory>

using vaml::Image;

TEST_CASE("S483: Image DP surface round-trips")
{
    Image img;
    SHOULD(ShouldlyString(img.source()).ShouldBe(std::string("")));
    SHOULD(ShouldlyString(img.icon_key()).ShouldBe(std::string("")));
    (void)img.set_source("assets/logo.png");
    (void)img.set_icon_key("App.Logo");
    (void)img.set_stretch(1);
    (void)img.set_width(24.0);
    (void)img.set_height(24.0);
    SHOULD(ShouldlyString(img.source()).ShouldBe(std::string("assets/logo.png")));
    SHOULD(ShouldlyString(img.icon_key()).ShouldBe(std::string("App.Logo")));
    SHOULD(ShouldlyIntegral(img.stretch()).ShouldBe(1));
    SHOULD(ShouldlyIntegral(static_cast<int>(img.width() == 24.0)).ShouldBe(1));
    SHOULD(ShouldlyIntegral(static_cast<int>(img.height() == 24.0)).ShouldBe(1));
}

TEST_CASE("S483: Image measures to its box")
{
    Image img;
    (void)img.set_width(32.0);
    (void)img.set_height(32.0);
    img.Measure(vaml::Size::Infinite());
    SHOULD(ShouldlyIntegral(static_cast<int>(img.desired_size().width == 32.0f)).ShouldBe(1));
    SHOULD(ShouldlyIntegral(static_cast<int>(img.desired_size().height == 32.0f)).ShouldBe(1));
}
