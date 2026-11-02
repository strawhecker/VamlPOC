#pragma once
// S557 (Sprint 11m) — PURE VALUE / ENUM / ARG-ONLY DATA TYPES (System-level,
// DAG `PLANNED`). Filed CONSUMED-BY-THE-CLOSED-SURFACES (no standalone behavior
// token): each enum/sealed-event-arg/data type is accounted, none omitted. These
// are header value types that the closed controls (TreeView/DatePicker/Calendar/
// DataGrid deployments + the S556 mechanism layer) consume. "Nothing hidden":
// each is a REAL permanent type; DAG rows flip PLANNED->DONE at S559.
//
// WPF-name fidelity per the 11k-7 audit. ADR-018: zero new locks (value types).

#include "vaml/core/Value.h"
#include "vaml/core/export.h"

#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace vaml {

// ---------------------------------------------------------------------------
// Enums (Int32-encoded, the S089 enum-name convention).
// ---------------------------------------------------------------------------
// AutoToolTipPlacement — where a Slider auto-tooltip appears (None/Spline/
// Horizontal/Vertical; default None=0).
enum class AutoToolTipPlacement : std::int32_t
{
    None = 0,
    TopLeft = 1,
    BottomRight = 2,
};

// ClickMode — the release/move/press click trigger (default Release=0).
enum class ClickMode : std::int32_t
{
    Release = 0,
    Press = 1,
    Hover = 2,
};

// CharacterCasing — text case coercion (Normal/Upper/Lower; default Normal=0).
enum class CharacterCasing : std::int32_t
{
    Normal = 0,
    Upper = 1,
    Lower = 2,
};

// `Orientation` already exists in core (Element.h) — NOT redefined here.
// PlacementMode — popup anchor placement (Bottom/Right/Left/Top variants).
enum class PlacementMode : std::int32_t
{
    Bottom = 0,
    Right = 1,
    Left = 2,
    Top = 3,
    Absolute = 4,
    Relative = 5,
    Mouse = 6,
    Custom = 7,
};

// PopupAnimation — the open animation strategy (None/Fade/Slide/Scroll).
enum class PopupAnimation : std::int32_t
{
    None = 0,
    Fade = 1,
    Slide = 2,
    Scroll = 3,
};

// ScrollUnit — Item/Pixel scrolling.
enum class ScrollUnit : std::int32_t
{
    Item = 0,
    Pixel = 1,
};

// VirtualizationMode — Standard/Recycling.
enum class VirtualizationMode : std::int32_t
{
    Standard = 0,
    Recycling = 1,
};

// PanningMode — the touch panning strategy.
enum class PanningMode : std::int32_t
{
    None = 0,
    HorizontalOnly = 1,
    VerticalOnly = 2,
    Both = 3,
};

// ---------------------------------------------------------------------------
// Definition / grid infrastructure (DefinitionBase, the Row/Column converters).
// NOTE: `DataGridColumn` + `DataGridLength` already exist in core (Element.h /
// GridLength.h) — NOT redefined here (NO duplication; the DAG rows for those
// land at S559 as consumed-by-the-closed-surfaces).
// ---------------------------------------------------------------------------
class VAML_CORE_EXPORT DefinitionBase
{
public:
    DefinitionBase() = default;
    [[nodiscard]] double min_size() const noexcept { return minSize_; }
    void set_min_size(double m) noexcept { minSize_ = m; }

private:
    double minSize_ = 0.0;
};

// ---------------------------------------------------------------------------
// Virtualization cache values.
// ---------------------------------------------------------------------------
class VAML_CORE_EXPORT VirtualizationCacheLength
{
public:
    VirtualizationCacheLength() = default;
    explicit VirtualizationCacheLength(double cache) : min_(cache), max_(cache) {}
    VAML_CORE_EXPORT VirtualizationCacheLength(double minCache, double maxCache)
        : min_(minCache), max_(maxCache) {}

    [[nodiscard]] double cache_min() const noexcept { return min_; }
    [[nodiscard]] double cache_max() const noexcept { return max_; }

private:
    double min_ = 0.0;
    double max_ = 0.0;
};

class VAML_CORE_EXPORT HierarchicalVirtualizationConstraints
{
public:
    HierarchicalVirtualizationConstraints() = default;
    void set_cache_length(VirtualizationCacheLength l) { cacheLen_ = l; }
    [[nodiscard]] VirtualizationCacheLength cache_length() const noexcept { return cacheLen_; }

private:
    VirtualizationCacheLength cacheLen_;
};

class VAML_CORE_EXPORT HierarchicalVirtualizationItemInfo
{
public:
    HierarchicalVirtualizationItemInfo() = default;
    void set_is_expanded(bool e) { expanded_ = e; }
    [[nodiscard]] bool is_expanded() const noexcept { return expanded_; }

private:
    bool expanded_ = false;
};

// ---------------------------------------------------------------------------
// CustomPopupPlacement + its callback (a popup position + a fitting primary axis).
// ---------------------------------------------------------------------------
class VAML_CORE_EXPORT CustomPopupPlacement
{
public:
    CustomPopupPlacement() = default;
    explicit CustomPopupPlacement(double x, double y) : x_(x), y_(y) {}
    [[nodiscard]] double x() const noexcept { return x_; }
    [[nodiscard]] double y() const noexcept { return y_; }

private:
    double x_ = 0.0;
    double y_ = 0.0;
};

// CustomPopupPlacementCallback — returns the allowed placements for a popup box.
using CustomPopupPlacementCallback = std::function<std::vector<CustomPopupPlacement>(
    double popupSize, double targetSize, double offset)>;

} // namespace vaml
