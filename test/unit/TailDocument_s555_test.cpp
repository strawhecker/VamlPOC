// S555 (Sprint 11m) — DOCUMENT / FRAME CONTROLS (dep:S554).
//
// Page, Frame (ContentControl source/navigation surface, closing the 11j-6
// StartupUri navigation NO-GAP), DocumentViewer/DocumentViewerBase +
// FlowDocumentReader/FlowDocumentScrollViewer (+ SinglePageViewer). Real +
// tested; the document-flow/text rendering depth is bounded per the
// S649/11k-11 continuation (recorded NO-GAP not silent).

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "doctest/doctest.h"

#include <vaml/core/Element.h>
#include <vaml/core/ElementFactoryRegistry.h>
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

TEST_CASE("S555: the document/frame controls are registered + resolvable")
{
    register_builtin_elements();
    ElementFactoryRegistry& g = ElementFactoryRegistry::global();
    SHOULD(ShouldlyIntegral(g.is_registered("Page")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("Frame")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("DocumentViewer")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("FlowDocumentReader")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("FlowDocumentScrollViewer")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("SinglePageViewer")).ShouldBe(true));
    // DocumentViewerBase is abstract — the 11k-4 precedent keeps it unregistered.
    SHOULD(ShouldlyIntegral(g.is_registered("DocumentViewerBase")).ShouldBe(false));
}

TEST_CASE("S555: Page + Frame carry the real ContentControl navigation surface")
{
    Page page;
    (void)page.set_content("Hello page");
    SHOULD(ShouldlyString(page.content()).ShouldBe("Hello page"));
    page.Measure(Size{ 240.0f, 180.0f });
    SHOULD(ShouldlyIntegral(page.desired_size().width > 0.0f).ShouldBe(true));

    Frame frame;
    (void)frame.set_source("page1.xaml");
    SHOULD(ShouldlyString(frame.source()).ShouldBe("page1.xaml"));
    (void)frame.set_navigate_uri("page2.xaml");
    SHOULD(ShouldlyString(frame.navigate_uri()).ShouldBe("page2.xaml"));
    frame.Measure(Size{ 300.0f, 200.0f });
    SHOULD(ShouldlyIntegral(frame.desired_size().width > 0.0f).ShouldBe(true));
}

TEST_CASE("S555: the document viewers carry the real document surface")
{
    DocumentViewer viewer;
    (void)viewer.set_document("doc-content");
    SHOULD(ShouldlyString(viewer.document()).ShouldBe("doc-content"));
    viewer.Measure(Size{ 260.0f, 220.0f });

    FlowDocumentScrollViewer scroll;
    (void)scroll.set_document("flow");
    SHOULD(ShouldlyString(scroll.document()).ShouldBe("flow"));

    FlowDocumentReader reader;
    (void)reader.set_document("reader");
    SHOULD(ShouldlyString(reader.document()).ShouldBe("reader"));

    SinglePageViewer single;
    (void)single.set_document("single");
    SHOULD(ShouldlyString(single.document()).ShouldBe("single"));
}

TEST_CASE("S555: Application.StartupUri — the 11j-6 navigation NO-GAP resolves (loadable)")
{
    register_builtin_elements();
    // The canonical app entry with a StartupUri parses + resolves (the
    // record-only DP is REAL; the window-open is wired through the S440
    // app-shell / S558 live viewer, recorded NO-GAP not silent).
    VamlReader reader;
    auto root = reader.Load(
        "<Application StartupUri=\"MainWindow.xaml\"/>");
    SHOULD(ShouldlyIntegral(root != nullptr).ShouldBe(true));
    auto* app = dynamic_cast<Application*>(root.get());
    SHOULD(ShouldlyIntegral(app != nullptr).ShouldBe(true));
    if (app)
    {
        SHOULD(ShouldlyString(app->startup_uri()).ShouldBe("MainWindow.xaml"));
    }
}

TEST_CASE("S555: the document/frame controls classify to the material slice")
{
    register_builtin_elements();
    Page page;
    SHOULD(ShouldlyIntegral(classify_merge_element(page).kind == MaterialKind::Solid).ShouldBe(true));
    Frame frame;
    SHOULD(ShouldlyIntegral(classify_merge_element(frame).kind == MaterialKind::Solid).ShouldBe(true));
    DocumentViewer viewer;
    SHOULD(ShouldlyIntegral(classify_merge_element(viewer).kind == MaterialKind::Solid).ShouldBe(true));
    FlowDocumentReader reader;
    SHOULD(ShouldlyIntegral(classify_merge_element(reader).kind == MaterialKind::Solid).ShouldBe(true));
}

TEST_CASE("S555: an authored document/frame assembly loads end-to-end")
{
    VamlReader reader;
    auto root = reader.Load(
        "<Window xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
        "<Page/>"
        "<Frame/>"
        "<DocumentViewer/>"
        "</Window>");
    SHOULD(ShouldlyIntegral(root != nullptr).ShouldBe(true));
    auto* win = dynamic_cast<Window*>(root.get());
    SHOULD(ShouldlyIntegral(win != nullptr).ShouldBe(true));
}
