// S554 (Sprint 11m) — DRAWING / MEDIA / EMBEDDED CONTROL TAIL (dep:S553).
//
// InkCanvas/InkPresenter (DONE-classified stylus/mouse stroke surface + the
// Strokes DP), MediaElement (Source/Volume/Muted + the Media* events).
// WebBrowser/ViewPort3D are re-affirmed OUT-OF-SCOPE(11m) dependencies on the
// S520 import/inject seam (a host-linked foreign surface, NOT a VAML-owned
// native type) — reclassified at S559. Real + tested for the DONE-classified
// party; ZERO MINIMAL REAL.

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "doctest/doctest.h"

#include <vaml/core/Element.h>
#include <vaml/core/ElementFactoryRegistry.h>
#include <vaml/core/RoutedEvent.h>
#include <vaml/core/UIElement.h>
#include <vaml/core/VamlReader.h>
#include <vaml/presentation/VamlRenderer.h>
#include <vaml/vulkan/ui/UiScene.h>

#include <memory>
#include <string>

using namespace vaml;
using vaml::register_builtin_elements;
using vaml::presentation::classify_merge_element;
using vaml::vulkan::ui::MaterialKind;

TEST_CASE("S554: InkCanvas/InkPresenter/MediaElement are registered; WebBrowser/ViewPort3D are not")
{
    register_builtin_elements();
    ElementFactoryRegistry& g = ElementFactoryRegistry::global();
    SHOULD(ShouldlyIntegral(g.is_registered("InkCanvas")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("InkPresenter")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("MediaElement")).ShouldBe(true));
    // WebBrowser/ViewPort3D are OUT-OF-SCOPE(11m) — the S520 import/inject seam.
    SHOULD(ShouldlyIntegral(g.is_registered("WebBrowser")).ShouldBe(false));
    SHOULD(ShouldlyIntegral(g.is_registered("ViewPort3D")).ShouldBe(false));
}

TEST_CASE("S554: InkCanvas/InkPresenter carry the real strokes + background surface")
{
    InkCanvas canvas;
    (void)canvas.set_strokes("M0,0 L10,10 M20,0 L30,10");
    SHOULD(ShouldlyString(canvas.strokes()).ShouldBe("M0,0 L10,10 M20,0 L30,10"));
    (void)canvas.set_active_stroke("M5,5 L6,6");
    SHOULD(ShouldlyString(canvas.active_stroke()).ShouldBe("M5,5 L6,6"));
    // S643b-1: Background is a Brush OBJECT.
    auto bg = std::make_shared<vaml::SolidColorBrush>();
    (void)bg->set_color(vaml::Color{ 4, 4, 4, 255 });
    (void)canvas.set_background(bg);
    SHOULD(ShouldlyIntegral(canvas.background() == bg).ShouldBe(1));
    canvas.Measure(Size{ 120.0f, 80.0f });
    SHOULD(ShouldlyIntegral(canvas.desired_size().width > 0.0f).ShouldBe(true));

    InkPresenter presenter;
    (void)presenter.set_strokes("M0,0 L10,10");
    SHOULD(ShouldlyString(presenter.strokes()).ShouldBe("M0,0 L10,10"));
    presenter.Measure(Size{ 120.0f, 80.0f });
}

TEST_CASE("S554: MediaElement carries the real media surface + the Media* events")
{
    register_builtin_elements();
    MediaElement media;
    (void)media.set_source("clip.wmv");
    SHOULD(ShouldlyString(media.source()).ShouldBe("clip.wmv"));
    (void)media.set_volume(0.5);
    SHOULD(ShouldlyIntegral(media.volume() >= 0.49 && media.volume() <= 0.51).ShouldBe(true));
    (void)media.set_muted(true);
    SHOULD(ShouldlyIntegral(media.muted()).ShouldBe(true));
    media.Measure(Size{ 320.0f, 180.0f });

    SHOULD(ShouldlyString(std::string(MediaOpenedEvent().name())).ShouldBe("MediaOpened"));
    SHOULD(ShouldlyString(std::string(MediaEndedEvent().name())).ShouldBe("MediaEnded"));
    SHOULD(ShouldlyString(std::string(MediaFailedEvent().name())).ShouldBe("MediaFailed"));
    SHOULD(ShouldlyIntegral(static_cast<int>(MediaOpenedEvent().direction()) ==
                            static_cast<int>(RouteEventDirection::Bubble)).ShouldBe(true));
}

TEST_CASE("S554: the drawing/media tail classifies to the correct material slice")
{
    register_builtin_elements();
    InkCanvas canvas;
    SHOULD(ShouldlyIntegral(classify_merge_element(canvas).kind == MaterialKind::Solid).ShouldBe(true));
    InkPresenter presenter;
    SHOULD(ShouldlyIntegral(classify_merge_element(presenter).kind == MaterialKind::Solid).ShouldBe(true));
    MediaElement media;
    SHOULD(ShouldlyIntegral(classify_merge_element(media).kind == MaterialKind::Monochrome).ShouldBe(true));
}

TEST_CASE("S554: an authored drawing/media assembly loads end-to-end")
{
    VamlReader reader;
    auto root = reader.Load(
        "<Window xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
        "<InkCanvas/>"
        "<InkPresenter/>"
        "<MediaElement/>"
        "</Window>");
    SHOULD(ShouldlyIntegral(root != nullptr).ShouldBe(true));
    auto* win = dynamic_cast<Window*>(root.get());
    SHOULD(ShouldlyIntegral(win != nullptr).ShouldBe(true));
}
