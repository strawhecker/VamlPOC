#pragma once
// VAML StickyNoteAdorner — the adorner integration for the StickyNote (11k-A,
// S-kA-4). RESOLVES the 11n-6 recorded forward refinement: AdornerHitTest +
// adorner input routing at the StickyNote's own step.
//
// WPF GROUND TRUTH (StickyNoteAnnotations.cs + StickyNoteHelper.cs):
//   * A StickyNoteControl is hosted in an AdornerLayer keyed to its ANNOTATED
//     element. The note's POSITION is an ADORNER-LAYER + TRANSFORM concern, not
//     layout: `GetDesiredTransform(transform)` returns a GeneralTransformGroup =
//     optional RTL self-mirror + TranslateTransform(anchor) + the offset
//     transform + the passed-in transform. Page-boundary math (GetOffsets,
//     persisted _offsetX/Y + transient _deltaX/Y) keeps the note on the page.
//   * The layer hosts the note's Adorner; the adorner renders ABOVE the
//     annotated element (never clipped to it — IsClipEnabled default false).
//
// THE VAML-FAITHFUL MAPPING (conformance-pinned):
//   * `StickyNoteAdorner : Adorner` — a REAL concrete adorner that hosts a
//     StickyNote above its anchored element. Its `GetDesiredTransform` composes
//     the note's PositionTransform (the WPF Translate/offset) onto the offered
//     transform (the anchored element's composed Model) — the anchor offset
//     translation, conformance-pinned to the WPF GeneralTransformGroup order.
//   * `AdornerHitTest` — the recorded 11n-6 refinement: hit-testing an adorner
//     whose visual is a StickyNote (the note IS hit-testable where it renders).
//     This step RESOLVES the 11n-6 "AdornerHitTest + adorner input routing"
//     forward token at the StickyNote's own step, NOT a backfill.
//   * Page-boundary math (GetOffsets + the persisted/transient offset split):
//     a headless seam `sticky_keep_on_page(position, anchorRect, pageRect)` —
//     the "keep the note on the page" contract, pure + headless-testable.
//
// The LAYER attach/detach + anchor wiring reuse the 11n-6 AdornerLayer/
// AdornerDecorator surface (already real). This step authors the StickyNote-
// specific adorner + the hit-test refinement + the page-boundary seam.
//
// ZERO MINIMAL REAL. ZERO invented names. ADR-018: ZERO new locks (the hit-
// test is a pure tree read; attachments single-threaded).

#include "vaml/core/Adorner.h"             // Adorner + GetDesiredTransform (11n-6)
#include "vaml/core/AdornerLayer.h"        // the host layer (Add/Remove/GetAdorners)
#include "vaml/core/StickyNote.h"          // the hosted StickyNote (the visible facade)
#include "vaml/core/Geometry.h"            // Rect (the page-bounds + anchor rect)
#include "vaml/core/Transform.h"           // Matrix2D (GetDesiredTransform)
#include "vaml/core/export.h"

namespace vaml {

// A point in layout space (the StickyNote page-placement domain). A small local
// value type (Geometry.h has no Point — VAML uses Rect/Size; this is the
// position outcome of the page-boundary clamp).
struct StickyNotePoint
{
    float x = 0.0f;
    float y = 0.0f;
};

// The REAL concrete StickyNote adorner: hosts a StickyNote above its anchored
// element. The note is a NON-OWNING reference (the caller/layer owns it); the
// adorner is hosted by the AdornerLayer (11n-6).
class VAML_CORE_EXPORT StickyNoteAdorner final : public Adorner
{
public:
    // Constructs the adorner over `note` (the hosted StickyNote) anchored to
    // `anchored` (the S204 element the note annotates — also the adorner's
    // AdornedElement). The note must outlive the adorner (non-owning).
    StickyNoteAdorner(StickyNote* note, UIElement* anchored);

    // The hosted StickyNote (non-owning).
    [[nodiscard]] StickyNote* Note() const noexcept { return note_; }

    // --- GetDesiredTransform (the S-kA-4 headline) ------------------------
    // Composes the note's PositionTransform (the WPF Translate/offset) onto the
    // offered transform (the anchored element's composed Model) — the anchor
    // offset. Conformance-pinned to the WPF GeneralTransformGroup order: the
    // offered transform, then the note's own transform (right-most applied
    // first). The note renders at its anchored element's box + its offset.
    [[nodiscard]] vaml::Matrix2D GetDesiredTransform(const vaml::Matrix2D& offered) const override;

private:
    StickyNote* note_ = nullptr;   // NON-OWNING hosted note
};

// --- S-kA-4 AdornerHitTest (RESOLVES the 11n-6 forward token) -------------
// Hit-tests an adorner whose visual is a StickyNote: the note is hit-testable
// WHERE IT RENDERS (its own final_rect via its PositionTransform over the
// anchored element). Returns true iff the note's rendered box contains (x,y).
// Pure + headless-testable. The full adorner input ROUTING (an input event
// delivering to the note through the routed engine + the S323 flow) is via the
// Adorner input-routing refinement recorded at 11n-6 + completed here at the
// StickyNote's own step.
VAML_CORE_EXPORT bool AdornerHitTest(const StickyNoteAdorner& adorner,
                                     float x, float y) noexcept;

// --- S-kA-4 page-boundary seam (WPF GetOffsets) ---------------------------
// Keeps the note's POSITION within the PAGE (the viewport the note renders in):
// clamps the note's proposed placement so it stays on the page, honoring the
// persisted/transient offset split (WPF _offsetX/Y persisted + _deltaX/Y
// transient). Pure + headless-testable. Returns the clamped position.
VAML_CORE_EXPORT StickyNotePoint
StickyKeepOnPage(const StickyNotePoint& position, const vaml::Rect& anchorRect,
                 const vaml::Rect& pageRect) noexcept;

} // namespace vaml
