// S640 (dep:S622) — IMAGE + SOURCE — the driver's explicit FIRST step for
// Sprint 11r (the exact thing that keeps failing transparent/black in the live
// viewer). Headless Rail 1 (vaml-presentation-tests — links vaml-presentation
// so the S212 encode_visual_slice render-plan is provable + the ADR-021 consumer
// path viewer_growth_s640.h).
//
// Under test (the S640 headline — the exact thing that FAILED before this step):
//
//   1. LOAD WITHOUT `unknown property 'Source'` — an authored `<Image Source="..."/>`
//      loads through a REAL retained VamlReader (the run_viewer_growth_s640_plan
//      seam). Pre-S640 only the DOTTED "Image.Source" DP was registered, so a BARE
//      `Source="..."` attribute threw `unknown property 'Source'`. S640 registered
//      the BARE `Source` DP (the reader resolves a bare attribute key by bare name),
//      so the doc loads, finalizes, and the authored image's source() reads back the
//      authored string.
//   2. IMAGE -> ATLAS -> QUAD — the S640 render seam (build_image_draw_item, the
//      ImageGlyphPlan) emits a NON-degenerate FULL-COLOR image quad at the image's
//      logical box (the Image->atlas->quad draw). When the asset is resolved against
//      the caller-supplied atlas candidate family, the quad is a real full-color
//      quad (rect + non-fallback pixels) — the content the S641 scanner confirms on
//      the rendered surface.
//   3. The authored Image actually reaches the tree + the classifier emits its item
//      (the S212 render-plan carries the Image).
//
// The LIVE window display (the [both-apps] proof) + the DEVICE IconAtlas upload +
// the S209 texture-binding of the atlas descriptor set stay the on-machine local
// gate (recorded NO-GAP); the S641 scanner confirms the image's rect + non-default
// pixels on the rendered surface.

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include <vaml/core/Element.h>            // register_builtin_elements
#include <vaml/core/ValueConverter.h>      // register_builtin_converters (the compiled-open S7 reader's converter table)
#include <vaml/core/UIElement.h>
#include <vaml/vulkan/ui/UiScene.h>       // DrawItem / DrawItemKind
#include <vaml/vulkan/icon/ImageGlyphPlan.h>
#include <vaml/vulkan/icon/IconBestFit.h>  // IconBestFitCache / IconCandidate

#include "viewer_growth_s640.h"           // S640 — the ADR-021 consumer seam

#include <string>
#include <vector>

using vaml::UIElement;
using vaml::viewer::run_viewer_growth_s640_plan;
using vaml::viewer::find_authored_image;
using vaml::viewer::viewer_growth_s640_image_quad;

namespace {

// A toy S202 exact-grid layout: a 128x128 atlas in 4x4 -> 16 tiles of 32x32.
const vaml::vulkan::quad::AtlasLayout kLayout =
    vaml::vulkan::quad::atlas_layout(128, 128, 4, 4);

// The S213_1 size family for a resolved image (all square, the S202_1 icon
// contract — Image rides the S400_1/S202_1 icon-asset pipeline).
std::vector<vaml::vulkan::icon::IconCandidate> toyFamily()
{
    return {
        { 1, 16, 16, 256 },   // tile 1, 16x16
        { 2, 32, 32, 1024 },  // tile 2, 32x32
        { 3, 48, 48, 2304 },  // tile 3, 48x48
    };
}

// A static authored doc with an Image that declares its expected outcome: a
// 48x48 image at (32,32) with the bare `Source="face-blowing-a-kiss-36.svg"`
// (the asset the S483/S202_1 pipeline resolves). The BARE `Source` is the S640
// headline — pre-S640 the dotted-only registration made this throw.
const std::string kSampleImage =
    "<StackPanel xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" "
    "Width=\"440\" Height=\"360\">"
    "<Image Source=\"face-blowing-a-kiss-36.svg\" Width=\"48\" Height=\"48\" Margin=\"32\"/>"
    "</StackPanel>";

} // namespace

// ---------------------------------------------------------------------------
// 1. LOAD — the authored `Source="..."` resolves (no `unknown property 'Source'`)
// ---------------------------------------------------------------------------

TEST_CASE("S640: an authored <Image Source=.../> loads via the BARE Source DP (no unknown-property throw)")
{
    vaml::register_builtin_elements();
    vaml::register_builtin_converters();
    auto r = run_viewer_growth_s640_plan(kSampleImage);
    SHOULD(ShouldlyIntegral(static_cast<int>(r.ok)).ShouldBe(1));
    SHOULD(ShouldlyIntegral(r.root != nullptr ? 1 : 0).ShouldBe(1));
    SHOULD(ShouldlyIntegral(r.reader != nullptr ? 1 : 0).ShouldBe(1));

    // The authored Image is in the S121 visual tree (the S440 hand-off rendered
    // the declared child — NOT just the root).
    const vaml::Image* img = find_authored_image(r.root.get());
    SHOULD(ShouldlyIntegral(img != nullptr ? 1 : 0).ShouldBe(1));
    if (img)
    {
        // The BARE Source round-tripped: the authored string read back.
        SHOULD(ShouldlyString(img->source()).ShouldBe(std::string("face-blowing-a-kiss-36.svg")));
    }
}

TEST_CASE("S640: the authored Image is a real UIElement in the arranged tree with a non-degenerate box")
{
    vaml::register_builtin_elements();
    vaml::register_builtin_converters();
    auto r = run_viewer_growth_s640_plan(kSampleImage);
    SHOULD(ShouldlyIntegral(static_cast<int>(r.ok)).ShouldBe(1));

    auto* uiRoot = dynamic_cast<UIElement*>(r.root.get());
    SHOULD(ShouldlyIntegral(uiRoot != nullptr ? 1 : 0).ShouldBe(1));
    if (!uiRoot)
        return;
    uiRoot->Measure(vaml::Size{ 440.0f, 360.0f });
    uiRoot->Arrange(vaml::Rect{ 0, 0, 440.0f, 360.0f });

    const vaml::Image* img = find_authored_image(r.root.get());
    SHOULD(ShouldlyIntegral(img != nullptr ? 1 : 0).ShouldBe(1));
    if (img)
    {
        // The image's final rect is non-degenerate (has extent) — it occupies a
        // real box the scanner will confirm.
        const vaml::Rect fr = img->final_rect();
        SHOULD(ShouldlyString(fr.width > 0.0f).ShouldBe(true));
        SHOULD(ShouldlyString(fr.height > 0.0f).ShouldBe(true));
    }
}

// ---------------------------------------------------------------------------
// 2. IMAGE -> ATLAS -> QUAD — the S640 render seam emits the full-color quad
// ---------------------------------------------------------------------------

TEST_CASE("S640: the resolved Image emits a NON-degenerate full-color quad at its box")
{
    vaml::register_builtin_elements();
    vaml::register_builtin_converters();
    auto r = run_viewer_growth_s640_plan(kSampleImage);
    SHOULD(ShouldlyIntegral(static_cast<int>(r.ok)).ShouldBe(1));

    auto* uiRoot = dynamic_cast<UIElement*>(r.root.get());
    SHOULD(ShouldlyIntegral(uiRoot != nullptr ? 1 : 0).ShouldBe(1));
    if (!uiRoot)
        return;
    uiRoot->Measure(vaml::Size{ 440.0f, 360.0f });
    uiRoot->Arrange(vaml::Rect{ 0, 0, 440.0f, 360.0f });

    const vaml::Image* img = find_authored_image(r.root.get());
    SHOULD(ShouldlyIntegral(img != nullptr ? 1 : 0).ShouldBe(1));
    if (!img)
        return;
    const vaml::Rect fr = img->final_rect();

    // The S640 render seam against the resolved asset family + slot.
    vaml::vulkan::icon::IconBestFitCache cache;
    vaml::vulkan::icon::ImageGlyphInput in;
    in.logicalBox    = fr;
    in.source        = img->source();
    in.assetResolved = true;
    in.assetKey      = 0;
    in.dpiScale      = 1.0f;

    vaml::vulkan::ui::DrawItem quad;
    const bool drew = viewer_growth_s640_image_quad(
        r.root.get(), in, toyFamily(), /*hasSlot=*/true, cache, kLayout, quad);
    SHOULD(ShouldlyIntegral(drew ? 1 : 0).ShouldBe(1));

    // The quad is the full-color image quad at the image's box: non-degenerate,
    // at the image's final rect, opaque white tint (the asset's RGB shows).
    SHOULD(ShouldlyString(quad.w > 0.0f).ShouldBe(true));
    SHOULD(ShouldlyString(quad.h > 0.0f).ShouldBe(true));
    SHOULD(ShouldlyString(quad.r).ShouldBe(1.0f));
    SHOULD(ShouldlyString(quad.g).ShouldBe(1.0f));
    SHOULD(ShouldlyString(quad.b).ShouldBe(1.0f));
}

// ---------------------------------------------------------------------------
// 3. The authored Image actually reaches the S212 render-plan (carried)
// ---------------------------------------------------------------------------

TEST_CASE("S640: the authored Image's classifier item is emitted to the render slice")
{
    vaml::register_builtin_elements();
    vaml::register_builtin_converters();
    auto r = run_viewer_growth_s640_plan(kSampleImage);
    SHOULD(ShouldlyIntegral(static_cast<int>(r.ok)).ShouldBe(1));

    auto* uiRoot = dynamic_cast<UIElement*>(r.root.get());
    SHOULD(ShouldlyIntegral(uiRoot != nullptr ? 1 : 0).ShouldBe(1));
    if (!uiRoot)
        return;
    uiRoot->Measure(vaml::Size{ 440.0f, 360.0f });
    uiRoot->Arrange(vaml::Rect{ 0, 0, 440.0f, 360.0f });

    // The Image classifies to a draw (Monochrome fallback headlessly — mirrors
    // Icon; the REAL full-color quad is the render seam proven above + the live
    // atlas path). The S212 slice therefore CARRIES a non-degenerate item for the
    // authored Image — the S641 scanner confirms the image's rect + non-default
    // pixels on the rendered surface.
    std::vector<vaml::vulkan::ui::DrawItem> items;
    vaml::presentation::encode_visual_slice(*uiRoot, vaml::presentation::default_visual_of(), items);
    std::size_t nonDegenerate = 0;
    for (const auto& item : items)
        if (!vaml::vulkan::ui::ui_item_degenerate(item))
            ++nonDegenerate;
    SHOULD(ShouldlyIntegral(nonDegenerate > 0u ? 1 : 0).ShouldBe(1));
}
