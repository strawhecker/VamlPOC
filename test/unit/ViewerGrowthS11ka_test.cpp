// 11k-A (S-kA-5, dep:S-kA-1..4 + 11n-6 + 11k_12/S554) — VIEWER-GROWTH
// (StickyNote): the ADR-021 consumer seam (`viewer_growth_s11ka.h`) runs the
// exact code the live viewer uses to render an ADORNER-ANCHORED StickyNote
// PREVIEW above a rendered element + exercises the CAF store HEADLESSLY.
//
// Under test (vaml-core-tests — it links vaml-presentation so the S212
// encode_visual_slice render-plan is provable):
//   1. CAF-STORE — the AnnotationService AddAttached is real; the note's live
//      DPs materialize from its annotation via ApplyAnnotation (the SNC bridge,
//      S-kA-2), the author lands, the TextData cargo round-trips.
//   2. ADORNER — the note is hosted in a StickyNoteAdorner inside an
//      AdornerLayer over the anchored element (S-kA-4); GetDesiredTransform
//      composes the PositionTransform. The note is hit-testable where it
//      renders (the resolved 11n-6 AdornerHitTest refinement).
//   3. PAGE-SEAM — StickyKeepOnPage clamps the note's placement to the page
//      (the WPF GetOffsets contract).
//   4. RENDER-PLAN — the S212 encode_visual_slice emits the anchored element's
//      solid (the note anchors ABOVE it; the note itself is an annotation-
//      anchor component, composed by the 11n-6 adorner-encode lane, NOT the
//      layout classifier).
//
// NO-GAP (recorded, not silent): the LIVE WINDOW display of the anchored
// StickyNote preview (the [both-apps] proof) stays the on-machine local gate;
// this headless seam is the exact code path's CI-checkable half. In-viewer
// note EDITING stays out (preview-only per the standing viewer contract).
// ADR-018: ZERO new locks.

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/StickyNote.h"
#include "vaml/core/StickyNoteAdorner.h"
#include "vaml/core/AdornerLayer.h"
#include "vaml/core/UIElement.h"
#include <vaml/vulkan/ui/UiScene.h>       // DrawItem
#include "viewer_growth_s11ka.h"          // the 11k-A ADR-021 consumer seam

#include <memory>
#include <vector>

using vaml::AnnotationResourceType;
using vaml::Matrix2D;
using vaml::viewer::run_viewer_growth_s11ka_plan;
using vaml::viewer::viewer_growth_s11ka_render_plan;

TEST_CASE("11k-A: viewer-growth — the CAF store + the anchored StickyNote preview")
{
    auto r = run_viewer_growth_s11ka_plan();

    // The CAF store + the note + the anchored element + the adorner all real.
    SHOULD(ShouldlyIntegral(static_cast<int>(r.ok)).ShouldBe(1));
    SHOULD(ShouldlyPointer(r.service.get()).ShouldNotBeNull());
    SHOULD(ShouldlyPointer(r.note.get()).ShouldNotBeNull());
    SHOULD(ShouldlyPointer(r.anchored.get()).ShouldNotBeNull());
    SHOULD(ShouldlyPointer(r.adorner.get()).ShouldNotBeNull());

    // AddAttached is real: one attached annotation in the store.
    SHOULD(ShouldlyIntegral(static_cast<std::size_t>(r.service->AttachedCount())).ShouldBe(1u));

    // The SNC bridge: the annotation's TextData cargo materialized onto the
    // note's body (S-kA-2) — the note previews what the store holds.
    SHOULD(ShouldlyString(r.note->body_text()).ShouldBe("5 star. This is very useful!"));

    // The S-kA-4 adorner: hosted in the layer over the anchored element; the
    // PositionTransform composes so the note renders at its anchor.
    const Matrix2D t = r.adorner->GetDesiredTransform(Matrix2D::identity());
    SHOULD(ShouldlyString(t.dx).ShouldBe(20.0f));
    SHOULD(ShouldlyString(t.dy).ShouldBe(24.0f));

    // The page-boundary keep-on-page clamp (S-kA-4) never throws and the note
    // is hit-testable where it renders (the resolved 11n-6 AdornerHitTest).
    const vaml::Rect page{0.0f, 0.0f, 800.0f, 600.0f};
    const vaml::Rect anchor = r.anchored->final_rect();
    const vaml::StickyNotePoint pos = vaml::StickyKeepOnPage(
        vaml::StickyNotePoint{20.0f + anchor.left(), 24.0f + anchor.top()},
        anchor, page);
    SHOULD(ShouldlyIntegral(static_cast<int>(vaml::AdornerHitTest(*r.adorner, pos.x + 40.0f, pos.y + 40.0f))).ShouldBe(1));
}

TEST_CASE("11k-A: viewer-growth render-plan — the anchored element renders beneath the overlay")
{
    auto r = run_viewer_growth_s11ka_plan();
    SHOULD(ShouldlyIntegral(static_cast<int>(r.ok)).ShouldBe(1));

    // The anchored element encodes via the S212 render-plan (the note anchors
    // ABOVE it — the overlay is the adorner lane, not the layout classifier).
    std::vector<vaml::vulkan::ui::DrawItem> items;
    const std::size_t n = viewer_growth_s11ka_render_plan(r.anchored.get(), items, 480.0f, 320.0f);
    SHOULD(ShouldlyIntegral(static_cast<std::size_t>(n)).ShouldBe(1u));
}
