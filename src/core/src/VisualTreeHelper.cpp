// VAML VisualTreeHelper — the static visual-tree navigation helper (11n-6).
//
// A thin CONST wrapper over the REAL S121 visual children list: GetParent /
// GetChildrenCount / GetChild walk the live tree (parent() / child_count() /
// child_at()); GetDescendantBounds unions the final_rect() over the subtree.
// Zero mutation, zero ownership, zero state — a pure borrowed-tree reader.
//
// ADR-018: ZERO new locks (read-only borrow; the S121 vector is not touched).

#include "vaml/core/VisualTreeHelper.h"

#include <algorithm> // std::max (GetDescendantBounds union)

namespace vaml {

VAML_CORE_EXPORT
std::size_t VisualTreeHelper::GetChildrenCount(const UIElement* element) noexcept
{
    if (!element)
        return 0;
    return element->child_count();
}

VAML_CORE_EXPORT
UIElement* VisualTreeHelper::GetChild(const UIElement* element,
                                      std::size_t index) noexcept
{
    if (!element)
        return nullptr;
    return element->child_at(index);
}

VAML_CORE_EXPORT
UIElement* VisualTreeHelper::GetParent(const UIElement* element) noexcept
{
    if (!element)
        return nullptr;
    return static_cast<UIElement*>(element->parent());
}

VAML_CORE_EXPORT
vaml::Rect VisualTreeHelper::GetDescendantBounds(const UIElement* element) noexcept
{
    if (!element)
        return {};

    vaml::Rect result = element->final_rect();
    const std::size_t n = element->child_count();
    for (std::size_t i = 0; i < n; ++i)
    {
        const vaml::Rect childBounds = GetDescendantBounds(element->child_at(i));

        // Union the child's bounds into the running rect. A zero-extent child
        // rect at (0,0) must NOT grow the union — only a child WITH extent can
        // extend it (the WPF GetDescendantBounds semantics: a zero rect never
        // adds area).
        if (childBounds.width > 0.0f || childBounds.height > 0.0f)
        {
            const float left   = std::min(result.x, childBounds.x);
            const float top    = std::min(result.y, childBounds.y);
            const float right  = std::max(result.right(), childBounds.right());
            const float bottom = std::max(result.bottom(), childBounds.bottom());
            result = vaml::Rect{ left, top, right - left, bottom - top };
        }
    }
    return result;
}

} // namespace vaml
