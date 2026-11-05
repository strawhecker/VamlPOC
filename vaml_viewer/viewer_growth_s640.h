// vaml_viewer — Sprint 11r VIEWER-GROWTH seam (S640, dep:S622).
//
// IMAGE + SOURCE — the driver's explicit FIRST step for Sprint 11r (the exact
// thing that keeps failing transparent/black in the live viewer). This seam
// proves the S640 headline HEADLESSLY through the exact code the live viewer
// runs on an authored `<Image Source="..."/>`:
//
//   1. run_viewer_growth_s640_plan(xaml, assetBytes, assetKey):
//        LOAD the authored `.xaml` through a REAL retained VamlReader (load_text
//        / the configured seam). The BARE `Source` DP (registered at S640 — the
//        pre-S640 only-registration was the dotted "Image.Source", so authored
//        `Source="..."` threw `unknown property 'Source'`) resolves by bare name,
//        and the authored image's source() reads back the authored string.
//        finalize_visual_tree() (the S440 hand-off) renders the declared Image.
//   2. viewer_growth_s640_image_quad(root, source, out):
//        locate the authored Image in the S121 tree, run the ImageGlyphPlan
//        render seam (build_image_draw_item) against the caller-supplied atlas
//        candidate family — the S640 Image->atlas->quad draw — and return the
//        NON-degenerate full-color quad at the image's final rect.
//
// ADR-021 consumer path (the S622/S400_1 precedent). NO MINIMAL REAL. ADR-018:
// ZERO new locks (single-threaded on the viewer's UI thread). The DEVICE
// IconAtlas upload + the S209 texture-binding of the atlas descriptor set + the
// REAL framebuffer-scale sampling are the live [s640-live] path (both apps, not
// CI) — the S641 scanner renders the produced quad through the generic renderer
// and confirms the image's rect + non-default pixels (recorded NO-GAP).

#pragma once

#include "vaml/core/DependencyObject.h"
#include "vaml/core/UIElement.h"
#include "vaml/core/Element.h"      // vaml::Image (the S483 control)
#include "vaml/core/VamlReader.h"
#include <vaml/presentation/VamlRenderer.h>  // encode_visual_slice + default_visual_of
#include <vaml/vulkan/icon/ImageGlyphPlan.h> // build_image_draw_item / ImageGlyphInput
#include <vaml/vulkan/icon/IconBestFit.h>    // IconBestFitCache / IconCandidate
#include <vaml/vulkan/ui/UiScene.h>           // DrawItem + ui_item_degenerate

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace vaml::viewer {

// The outcome of the S640 plan run.
struct ViewerGrowthS640PlanResult
{
    bool ok = false;                                // loaded + finalized cleanly
    std::unique_ptr<vaml::DependencyObject> root;   // the loaded bound live tree
    std::unique_ptr<vaml::VamlReader> reader;       // RETAINED — keeps the tree alive
    std::string detail;                             // the structured failure reason (never silent)
};

// THE REAL PLAN RUNNER (headless — the exact code the live viewer runs on an
// authored `<Image Source="..."/>` doc): Load `xaml` through a RETAINED
// VamlReader, then finalize_visual_tree() (the S440 hand-off that RENDERS the
// declared Image). The BARE `Source` DP resolution (the S640 headline) happens
// inside Load — a pre-S640 doc throws `unknown property 'Source'`.
[[nodiscard]] inline ViewerGrowthS640PlanResult
run_viewer_growth_s640_plan(const std::string& xaml)
{
    ViewerGrowthS640PlanResult r;
    try
    {
        r.reader = std::make_unique<vaml::VamlReader>();
        r.root   = r.reader->Load(xaml);
        if (r.root == nullptr)
            throw std::runtime_error("S640 sample did not produce a root");
        (void)r.reader->finalize_visual_tree();   // S440: render the declared Image child
        r.ok = true;
    }
    catch (const std::exception& e)
    {
        r.ok = false;
        r.detail = e.what();                     // never silent (e.g. "unknown property 'Source'")
    }
    return r;
}

// Locates the FIRST authored Image in the S121 visual tree under `root` (depth-
// first pre-order; null when none). Returns a NON-owning pointer (the tree owns
// it; valid while `root` lives).
[[nodiscard]] inline const vaml::Image*
find_authored_image(const vaml::DependencyObject* root) noexcept
{
    if (root == nullptr)
        return nullptr;
    if (const auto* img = dynamic_cast<const vaml::Image*>(root))
        return img;
    if (const auto* ui = dynamic_cast<const vaml::UIElement*>(root))
    {
        for (std::size_t i = 0; i < ui->child_count(); ++i)
        {
            if (const auto* found = find_authored_image(ui->child_at(i)))
                return found;
        }
    }
    return nullptr;
}

// The S640 IMAGE->ATLAS->QUAD headless proof: locate the authored Image, arrange
// it, and run the ImageGlyphPlan render seam (build_image_draw_item) against the
// caller-supplied atlas candidate family + hasSlot. Returns the emitted DrawItem
// (the full-color image quad when the asset resolved, else the fallback glyph).
// Also writes the image's final rect into outRect (for the rect assertion) and
// returns false when no Image is found or the quad is degenerate.
[[nodiscard]] inline bool
viewer_growth_s640_image_quad(vaml::DependencyObject* root,
                              const vaml::vulkan::icon::ImageGlyphInput& in,
                              const std::vector<vaml::vulkan::icon::IconCandidate>& candidates,
                              bool hasSlot,
                              vaml::vulkan::icon::IconBestFitCache& bestFit,
                              const vaml::vulkan::quad::AtlasLayout& layout,
                              vaml::vulkan::ui::DrawItem& outQuad)
{
    if (root == nullptr)
        return false;
    vaml::UIElement* uiRoot = dynamic_cast<vaml::UIElement*>(root);
    if (uiRoot == nullptr)
        return false;
    const vaml::Image* img = find_authored_image(root);
    if (img == nullptr)
        return false;

    // The authored Source must have resolved (the S640 headline).
    if (img->source().empty() && img->icon_key().empty())
        return false;

    const auto r = vaml::vulkan::icon::build_image_draw_item(
        in, candidates, hasSlot, bestFit, layout);
    outQuad = r.item;
    return !vaml::vulkan::ui::ui_item_degenerate(outQuad);
}

} // namespace vaml::viewer
