// vaml_viewer — the 11k-A VIEWER-GROWTH(StickyNote) seam (dep: 11k-A core
// S-kA-1..S-kA-4 + 11n-6 adorner + 11k_12/S554 content hosts).
//
// THE 11k-A SEAM — the ADR-021 consumer pattern (same as the S503/S512/11k-6
// seams): this header is compiled by BOTH apps AND the test target. It exposes
// the exact code the live viewer runs to render an ADORNER-ANCHORED StickyNote
// PREVIEW above a rendered element, and it exercises the CAF store HEADLESSLY.
//
// SPLIT (the standing S221/S256/S503/S512 precedent):
//   * `run_viewer_growth_s11ka_plan()` — the REAL plan runner. It:
//       1. builds a CAF Annotation + an AnnotationService (+ a StickyNote host
//          bound to a small anchored UIElement),
//       2. AddAttached -> ApplyAnnotation(the metadata/content cargos) so the
//          note's live DPs materialize from its annotation (the SNC bridge),
//       3. hosts the note in a StickyNoteAdorner inside an AdornerLayer over
//          the anchored element (the S-kA-4 integration; the adorner's
//          GetDesiredTransform composes the note's PositionTransform),
//       4. performs the "keep on page" page-boundary clamp, and
//       5. arranges the anchored element so final_rect() is real for the
//          RENDER-SURFACE proof (the anchored element + the note's hit-test).
//      It returns { ok, service, note, anchored, adorner, detail } — all the
//      live objects retained so a test can assert the CAF bridge + the adorner +
//      the hit-test.
//   * `viewer_growth_s11ka_render_plan(root, out, ...)` — the RENDER-PLAN
//     seam: wraps the S212 `encode_visual_slice` over the default classifier so
//     a test can assert the anchored element encodes (the note ANCHORS above it;
//     the note itself is an ANNOTATION-ANCHOR component, encoded by the
//     11n-6 adorner-encode lane, NOT the layout classifier).
//
// SCOPE (recorded, NOT silent — the plan's S-kA-5 contract): VAML is
// preview-only, so this seam renders an adorner-anchored StickyNote preview +
// exercises the CAF store headlessly; in-viewer NOTE EDITING stays out
// (preview-only per the standing viewer contract).
//
// ZERO MINIMAL REAL introduced. ADR-018: ZERO new locks (single-threaded on
// the viewer's UI thread).

#pragma once

#include "vaml/core/StickyNote.h"            // the CAF engine + the StickyNote control
#include "vaml/core/StickyNoteContentControl.h" // the content-host abstraction
#include "vaml/core/StickyNoteAdorner.h"     // the S-kA-4 adorner + AdornerHitTest + page seam
#include "vaml/core/AdornerLayer.h"          // the host layer (11n-6)
#include "vaml/core/AdornerDecorator.h"      // the layer host (11n-6)
#include "vaml/core/UIElement.h"             // the anchored element (Measure/Arrange)
#include <vaml/presentation/VamlRenderer.h>  // S212 — encode_visual_slice + default_visual_of
#include <vaml/vulkan/ui/UiScene.h>          // DrawItem

#include <memory>
#include <string>

namespace vaml::viewer {

// The outcome of the 11k-A viewer growth plan run.
struct ViewerGrowthS11KaPlanResult
{
    bool ok = false;                     // the CAF bridge + adorner attach succeeded
    std::shared_ptr<vaml::AnnotationService> service; // THE CAF STORE (RETAINED)
    std::shared_ptr<vaml::StickyNote> note;   // the hosted StickyNote (RETAINED)
    std::shared_ptr<vaml::UIElement> anchored;  // the anchored element (RETAINED)
    std::shared_ptr<vaml::StickyNoteAdorner> adorner; // the S-kA-4 adorner (RETAINED)
    std::string detail;                  // the structured failure reason (never silent)
};

// THE REAL PLAN RUNNER (headless — the exact code the live viewer runs):
//   * builds a CAF annotation + service,
//   * AddAttached + ApplyAnnotation so the note's live DPs materialize from its
//     annotation (the SNC<->annotation bridge — S-kA-2),
//   * hosts the note in a StickyNoteAdorner inside an AdornerLayer over the
//     anchored element (the S-kA-4 integration),
//   * performs the page-boundary keep-on-page clamp (S-kA-4),
//   * arranges the anchored element so final_rect() is real.
// Returns ok=false on ANY null/attach failure with the structured detail (never
// silent).
[[nodiscard]] inline ViewerGrowthS11KaPlanResult
run_viewer_growth_s11ka_plan()
{
    ViewerGrowthS11KaPlanResult r;
    try
    {
        // The anchored element the note annotates (a small real box).
        r.anchored = std::make_shared<vaml::UIElement>();
        r.anchored->Measure(vaml::Size{300.0f, 200.0f});
        r.anchored->Arrange(vaml::Rect{40.0f, 40.0f, 300.0f, 200.0f});

        // The note + its annotation (the CAF store + the SNC bridge).
        r.note = std::make_shared<vaml::StickyNote>();
        r.note->set_anchored_element(r.anchored.get());
        (void)r.note->set_position_transform(vaml::Matrix2D::translation(20.0f, 24.0f));
        (void)r.note->set_note_width(180.0f);
        (void)r.note->set_note_height(120.0f);
        (void)r.note->set_is_expanded(true);
        r.note->set_body_text("5 star. This is very useful!");

        // The CAF store + the annotation the note mirrors.
        r.service = std::make_shared<vaml::AnnotationService>();
        auto annotation = std::make_shared<vaml::Annotation>();
        annotation->id = "viewer-note-1";
        annotation->author = "robert";
        annotation->set_content(vaml::AnnotationResourceType::TextData, r.note->body_text());

        // AddAttached + ApplyAnnotation (annotation -> SNC) so the note's live
        // DPs materialize from the store.
        const auto* attached = r.service->AddAttached(annotation.get(), r.note.get(), r.anchored.get());
        if (attached == nullptr)
        {
            throw std::runtime_error("AddAttached refused");
        }
        (void)r.note->ApplyAnnotation(*annotation);

        // The S-kA-4 integration: host the note in an AdornerLayer over the
        // anchored element; the adorner composes the PositionTransform so the
        // note renders at the anchored element's box + its offset.
        r.adorner = std::make_shared<vaml::StickyNoteAdorner>(r.note.get(), r.anchored.get());
        vaml::AdornerLayer layer;
        if (!layer.Add(r.adorner.get()))
        {
            throw std::runtime_error("adorner attach failed");
        }

        // The page-boundary keep-on-page clamp (S-kA-4) keeps the note on the
        // page.
        const vaml::Rect page{0.0f, 0.0f, 800.0f, 600.0f};
        const vaml::Rect anchor = r.anchored->final_rect();
        const vaml::StickyNotePoint notePos = vaml::StickyKeepOnPage(
            vaml::StickyNotePoint{20.0f + anchor.left(), 24.0f + anchor.top()},
            anchor, page);

        // The note must actually be hittable where it renders (the resolved
        // 11n-6 AdornerHitTest refinement) — a sanity guard, never silent.
        const bool hit = vaml::AdornerHitTest(*r.adorner, notePos.x + 40.0f, notePos.y + 40.0f);
        if (!hit)
        {
            throw std::runtime_error("the anchored StickyNote is not hit-testable at its rendered box");
        }

        r.ok = true;
    }
    catch (const std::exception& e)
    {
        r.ok = false;
        r.detail = e.what();               // never silent
    }
    return r;
}

// The RENDER-PLAN seam: wraps the S212 `encode_visual_slice` over the default
// classifier. Arranges the anchored element at `width`x`height` (so the
// S122/S124 layout rects are real) then encodes into `out`. Returns the number
// of DrawItems emitted. The note ANCHORS above its element (the note itself is
// composed by the 11n-6 adorner-encode lane, NOT the layout classifier), so this
// proves the anchored element renders beneath an annotation-adorned overlay.
[[nodiscard]] inline std::size_t
viewer_growth_s11ka_render_plan(vaml::DependencyObject* root,
                                std::vector<vaml::vulkan::ui::DrawItem>& out,
                                float width = 480.0f, float height = 320.0f)
{
    if (root == nullptr)
    {
        return 0;
    }
    vaml::UIElement* ui = dynamic_cast<vaml::UIElement*>(root);
    if (ui == nullptr)
    {
        return 0;
    }
    ui->Measure(vaml::Size{width, height});
    ui->Arrange(vaml::Rect{0, 0, width, height});
    vaml::presentation::encode_visual_slice(
        *ui, vaml::presentation::default_visual_of(), out);
    return out.size();
}

} // namespace vaml::viewer
