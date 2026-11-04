// S-kA-4 — the StickyNote adorner integration (11k-A, core).
//
// WPF ground truth (StickyNoteAnnotations.cs + StickyNoteHelper.cs): the note
// is hosted in an AdornerLayer over its anchored element; GetDesiredTransform
// composes the note's offset onto the offered transform; the page-boundary math
// keeps the note on the page. This step authors the StickyNote adorner + the
// AdornerHitTest refinement (RESOLVING the 11n-6 forward token) + the
// page-boundary seam.
//
// ADR-011 held (core data + transform/hit-test). ZERO MINIMAL REAL.

#include "vaml/core/StickyNoteAdorner.h"

#include <algorithm>
#include <cmath>

namespace vaml {

StickyNoteAdorner::StickyNoteAdorner(StickyNote* note, UIElement* anchored)
    : Adorner(anchored), note_(note)
{
    // The note carries the anchor (kept in sync: the adorner's AdornedElement
    // IS the note's anchored element).
    if (note_ != nullptr)
    {
        note_->set_anchored_element(anchored);
    }
}

vaml::Matrix2D StickyNoteAdorner::GetDesiredTransform(const vaml::Matrix2D& offered) const
{
    // The WPF GeneralTransformGroup order: the OFFERED transform (the anchored
    // element's composed Model), then the NOTE's own PositionTransform
    // (right-most applied first — the note renders at its anchor + its offset).
    if (note_ == nullptr)
    {
        return offered;
    }
    return vaml::multiply(offered, note_->position_transform());
}

bool AdornerHitTest(const StickyNoteAdorner& adorner, float x, float y) noexcept
{
    const StickyNote* note = adorner.Note();
    if (note == nullptr)
    {
        return false;
    }
    const UIElement* anchor = note->AnchoredElement();
    // The note's rendered box = the anchored element's final rect, offset by
    // the note's PositionTransform (the adorner renders above the anchor).
    float ax = 0.0f;
    float ay = 0.0f;
    if (anchor != nullptr)
    {
        ax = anchor->final_rect().left();
        ay = anchor->final_rect().top();
    }
    // The note's own extent (Width/Height DPs; NaN -> the nominal box).
    float w = static_cast<float>(note->note_width());
    float h = static_cast<float>(note->note_height());
    if (std::isnan(w)) w = 200.0f;
    if (std::isnan(h)) h = 160.0f;
    // Apply the PositionTransform translation (the WPF Translate/offset).
    const vaml::Matrix2D t = note->position_transform();
    float px = x - (ax + t.dx);
    float py = y - (ay + t.dy);
    return (px >= 0.0f && px <= w) && (py >= 0.0f && py <= h);
}

StickyNotePoint StickyKeepOnPage(const StickyNotePoint& position,
                                 const vaml::Rect& anchorRect,
                                 const vaml::Rect& pageRect) noexcept
{
    // The WPF GetOffsets contract: keep the note's placement within the page.
    // The note's nominal box rides its anchor's extent (the WPF StickyNote
    // sizes to its anchored content); the note's origin clamps so its box stays
    // on the page rather than drifting off into the margin.
    StickyNotePoint out = position;
    const float pageLeft  = pageRect.left();
    const float pageTop   = pageRect.top();
    const float pageRight = pageRect.right();
    const float pageBottom = pageRect.bottom();
    const float boxW = std::max(1.0f, anchorRect.right() - anchorRect.left());
    const float boxH = std::max(1.0f, anchorRect.bottom() - anchorRect.top());
    // Clamp the note's origin so the note's box (anchor-sized, offset by the
    // position) stays within the page.
    out.x = std::clamp(out.x, pageLeft, std::max(pageLeft, pageRight - boxW));
    out.y = std::clamp(out.y, pageTop, std::max(pageTop, pageBottom - boxH));
    return out;
}

} // namespace vaml
