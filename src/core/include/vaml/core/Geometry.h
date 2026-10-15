#pragma once
// VAML layout geometry — Size + Rect, the pure value types that carry the
// Sprint-3 layout contract (S122: LayoutSlot + final rect + DesiredSize +
// Margin + alignment).
//
// Both are plain WPF-faithful value types (like Thickness/Color): default 0,
// value equality, no owning storage, no export surface beyond trivial
// accessors. They are the units Measure (writes DesiredSize) and Arrange
// (writes final rect) exchange at S123/S124.

#include <limits>

namespace vaml {

// A measured extent in logical layout units (floats, top-left origin).
// `Size::Infinite()` is the WPF-faithful "available is unbounded" sentinel
// that the S123 Measure contract consumes (a panel offers an unbounded axis;
// a child measures to its content on that axis).
struct Size
{
    float width  = 0.0f;
    float height = 0.0f;

    [[nodiscard]] constexpr bool operator==(const Size&) const noexcept = default;

    // The unbounded-available sentinel (+inf on both axes). Consumed by
    // Measure (S123) and, on non-Stretch alignments, by the S122 alignment
    // algorithm (min(infinite, available) == available).
    [[nodiscard]] static constexpr Size Infinite() noexcept
    {
        const float inf = std::numeric_limits<float>::infinity();
        return { inf, inf };
    }
};

// An axis-aligned rectangle in layout space: origin (x, y) + extent
// (width, height). The container for a child's arrange slot and final rect.
struct Rect
{
    float x      = 0.0f;
    float y      = 0.0f;
    float width  = 0.0f;
    float height = 0.0f;

    [[nodiscard]] constexpr bool operator==(const Rect&) const noexcept = default;

    [[nodiscard]] constexpr float left()   const noexcept { return x; }
    [[nodiscard]] constexpr float top()    const noexcept { return y; }
    [[nodiscard]] constexpr float right()  const noexcept { return x + width; }
    [[nodiscard]] constexpr float bottom() const noexcept { return y + height; }
};

} // namespace vaml
