#pragma once
// VAML VisualTreeHelper — the static visual-tree navigation helper
// (WPF System.Windows.Media.VisualTreeHelper). 11n-6.
//
// This is a REAL permanent core surface (NOT a STUB / NOT MINIMAL REAL). It is
// a thin READ-ONLY wrapper over the REAL S121 visual children list that
// UIElement already owns: GetParent/GetChildrenCount/GetChild walk the live
// tree. It NEVER builds a parallel tree and it NEVER mutates — all access is
// const (the non-owning pointer results are the caller's borrowed view).
//
// WPF-faithful surface (read from the WPF source, Media\VisualTreeHelper.cs):
//   * GetChildrenCount(DependencyObject) -> int  — the number of visual
//     children; VAML's UIElement.child_count() (S121).
//   * GetChild(DependencyObject, int) -> DependencyObject — the child at the
//     index (VAML: child_at(i)); out-of-range returns nullptr (the WPF indexer
//     throws, but our non-owning-pointer contract returns nullptr — recorded
//     convention, mirroring UIElement::child_at).
//   * GetParent(DependencyObject) -> DependencyObject — the visual parent
//     (VAML: parent()).
//   * GetDescendantBounds(DependencyObject) -> Rect — the union of bounds
//     (this element's final_rect()) over this node and its descendants.
//
// VisualTreeHelper is CONST + PURE — it cannot be attached to a tree, holds no
// state, routes only through the borrowed S121 structure. All overloads accept
// a UIElement (the VAML visual element; UIElement IS the visual-node type, and
// *Host (FrameworkContentElement) hosts are a later NO-GAP token, recorded not
// silent). ADR-018: ZERO new locks (read-only borrow).
//
// GetDescendantBounds + GetAdjacentElementsFromPoint are the WPF-documented
// members we do NOT surface this step (adjacent-element-from-point is the
// S323 hit-test refinement, recorded NO-GAP not silent). ZERO MINIMAL REAL.

#include "vaml/core/UIElement.h"      // UIElement (the VAML visual-node type)
#include "vaml/core/Geometry.h"       // Rect (GetDescendantBounds)
#include "vaml/core/DependencyObject.h" // DependencyObject (the WPF-typed base)
#include "vaml/core/export.h"

#include <cstddef>

namespace vaml {

// The static visual-tree navigation helper (11n-6). All members are CONST,
// PURE, and borrow the live S121 tree — no mutation, no ownership, no state.
class VAML_CORE_EXPORT VisualTreeHelper
{
public:
    // The number of visual children of `element` (VAML: child_count()).
    // Matches WPF GetChildrenCount(Visual). Passing nullptr -> 0.
    [[nodiscard]] static std::size_t GetChildrenCount(const UIElement* element) noexcept;

    // The NON-OWNING visual child at `index` (VAML: child_at(index));
    // nullptr for an out-of-range index or a null parent. Matches WPF
    // GetChild(Visual, int) (which throws on a bad index; VAML's
    // non-owning-pointer convention returns nullptr — recorded).
    [[nodiscard]] static UIElement* GetChild(const UIElement* element,
                                             std::size_t index) noexcept;

    // The NON-OWNING visual parent of `element` (VAML: parent() as a
    // UIElement). nullptr when the element is a root. Matches WPF
    // GetParent(DependencyObject). Passing nullptr -> nullptr.
    [[nodiscard]] static UIElement* GetParent(const UIElement* element) noexcept;

    // The UNION of the bounds (this node's final_rect()) over `element` and
    // all its descendants, in layout space (floats, top-left origin). A leaf
    // returns its own final_rect(); an element with no arranged extent
    // contributes a zero rect (the zero rect never "grows" the union). The
    // WPF-documented GetDescendantBounds(Visual). Pure + headless-testable.
    [[nodiscard]] static vaml::Rect GetDescendantBounds(const UIElement* element) noexcept;
};

} // namespace vaml
