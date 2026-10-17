// S087 — built-in Element MINIMAL REAL seeds + startup registration.
//
// Element subclasses that are still seeds are deliberately a THIN identity
// bearer (empty ctor) deriving DependencyObject. NO members / logic here — the
// real content of each control is authored at its OWN resolution sprint (see
// Element.h sentinels). The controls that have reached their sprints are
// StackPanel (S129) and Grid (S131), which RESOLVE their seeds here: re-based
// onto UIElement and delivered as REAL panels. The remaining seeds stay thin
// until their own sprints.
//
// register_builtin_elements() is the single startup registration call that
// populates ElementFactoryRegistry::global() with every roadmap element name the
// S091 runtime reader and S093 sample need. It is idempotent-guarded: a second
// call (e.g. from a test that also calls the user-facing register) is a no-op,
// so startup population can be re-invoked safely.

#include "vaml/core/Element.h"

#include "vaml/core/DependencyProperty.h"
#include "vaml/core/ElementFactoryRegistry.h"
#include "vaml/core/Geometry.h"
#include "vaml/core/GridLength.h"
#include "vaml/core/LayoutSlot.h"
#include "vaml/core/PropertyFlags.h"
#include "vaml/core/PropertyMetadata.h"

#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

namespace vaml {

namespace {

// Registers the StackPanel Orientation DP into the global table exactly once
// per process (Meyers-static guard; a second call is a no-op — mirrors
// UIElement's self-registration). Orientation is GLOBALLY registered so it
// resolves on any DependencyObject via S043 attached-aware resolution, and
// defaults to Vertical (0), NOT inherited (a panel's orientation is its own).
void register_stackpanel_properties()
{
    (void)DependencyProperty::Register(
        "Orientation", ValueType::Int32, "StackPanel",
        PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None));
}

// Runs register_stackpanel_properties() at most once (first StackPanel ctor).
// Function-local static guard => no static-init-order hazard.
void ensure_stackpanel_properties_registered()
{
    static bool registered = false;
    if (!registered)
    {
        registered = true;
        register_stackpanel_properties();
    }
}

// Registers the two Grid ATTACHED DPs (Row + Column) into the global table
// exactly once per process (Meyers-static guard; a second call is a no-op).
// They are GLOBALLY registered under owner "Grid" so they resolve on ANY
// DependencyObject via the S043 attached-aware path — a child's cell position
// is stored on the child (the object holding the value), not on the Grid.
void register_grid_properties()
{
    (void)DependencyProperty::Register(
        "Grid.Row", ValueType::Int32, "Grid",
        PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None));
    (void)DependencyProperty::Register(
        "Grid.Column", ValueType::Int32, "Grid",
        PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None));
    // S132 span DPs: a child's CELL SPAN (rows × columns). Same attached-aware
    // model + Int32 path; default 1 (no explicit span = single cell).
    (void)DependencyProperty::Register(
        "Grid.RowSpan", ValueType::Int32, "Grid",
        PropertyMetadata::WithDefault(Value::Int32(1), PropertyFlags::None));
    (void)DependencyProperty::Register(
        "Grid.ColumnSpan", ValueType::Int32, "Grid",
        PropertyMetadata::WithDefault(Value::Int32(1), PropertyFlags::None));
}

// Runs register_grid_properties() at most once (first Grid ctor).
void ensure_grid_properties_registered()
{
    static bool registered = false;
    if (!registered)
    {
        registered = true;
        register_grid_properties();
    }
}

// --- S132 span-aware track resolution (shared by measure + arrange) ---------
//
// Realizes the S132 D1/D2 decisions:
//   * D2 (Auto equal-division): a spanning child's content contributes
//     desired / span-count to EACH spanned Auto track (not the full extent to
//     one track) — the WPF-faithful rule. For a single cell (span 1) this is
//     just the child's desired extent, so the S131 no-span behavior is a
//     special case (NO-GAP preserved).
//   * D1 (star min/re-award): every child's content also sets a CONTENT MINIMUM
//     (desired / span-count) on each spanned track. A resolved Star track is
//     re-awarded to at least that minimum (so a spanning child whose content
//     exceeds its proportional share forces the spanned star track to grow to
//     fit the content). Non-spanning star tracks obey the same rule with
//     span-count 1 (a single cell's content grows its star track) — WPF star
//     behavior. The content minima are kept separately so the panel DesiredSize
//     reports the content-natural extent (star minimums, not proportional
//     shares that depend on the available offer).
//
// The resolved track extents (+ content-star-minima) are what both Measure
// (merged-cell second pass + DesiredSize) and Arrange (merged-span rect) need.
struct GridTrackResolution
{
    std::vector<float> col;               // resolved column extents
    std::vector<float> row;               // resolved row extents
    std::vector<float> starMinCol;        // content minima for star columns
    std::vector<float> starMinRow;        // content minima for star rows
};

GridTrackResolution resolve_grid_with_spans(
    const Grid& grid,
    std::size_t nRows,
    std::size_t nCols,
    float availW,
    float availH)
{
    GridTrackResolution out;
    out.col.assign(nCols, 0.0f);
    out.row.assign(nRows, 0.0f);
    out.starMinCol.assign(nCols, 0.0f);
    out.starMinRow.assign(nRows, 0.0f);

    std::vector<float> autoCol(nCols, 0.0f);
    std::vector<float> autoRow(nRows, 0.0f);

    // Accumulate Auto extents + Star content minima from every child's STORED
    // desired extent, distributed equally across its span (D2 / D1).
    for (std::size_t i = 0; i < grid.child_count(); ++i)
    {
        UIElement* c = grid.child_at(i);
        const std::int32_t colRaw = std::max<std::int32_t>(Grid::column(*c), 0);
        const std::int32_t rowRaw = std::max<std::int32_t>(Grid::row(*c), 0);
        const std::size_t col = static_cast<std::size_t>(
            std::min<std::int32_t>(colRaw, static_cast<std::int32_t>(nCols) - 1));
        const std::size_t r = static_cast<std::size_t>(
            std::min<std::int32_t>(rowRaw, static_cast<std::int32_t>(nRows) - 1));

        const std::int32_t cspanRaw = std::max<std::int32_t>(Grid::column_span(*c), 1);
        const std::int32_t rspanRaw = std::max<std::int32_t>(Grid::row_span(*c), 1);
        const std::size_t cspan = static_cast<std::size_t>(
            std::min<std::int32_t>(cspanRaw, static_cast<std::int32_t>(nCols) - static_cast<std::int32_t>(col)));
        const std::size_t rspan = static_cast<std::size_t>(
            std::min<std::int32_t>(rspanRaw, static_cast<std::int32_t>(nRows) - static_cast<std::int32_t>(r)));

        // Equal-division across the span (D2). Span-count clamped >= 1.
        const float perCol = c->desired_size().width / static_cast<float>(cspan);
        const float perRow = c->desired_size().height / static_cast<float>(rspan);

        for (std::size_t k = col; k < col + cspan; ++k)
        {
            autoCol[k] = std::max(autoCol[k], perCol);
            out.starMinCol[k] = std::max(out.starMinCol[k], perCol);
        }
        for (std::size_t k = r; k < r + rspan; ++k)
        {
            autoRow[k] = std::max(autoRow[k], perRow);
            out.starMinRow[k] = std::max(out.starMinRow[k], perRow);
        }
    }

    std::vector<GridLength> colDefs;
    colDefs.reserve(nCols);
    for (const auto& d : grid.column_definitions())
    {
        colDefs.push_back(d.width);
    }
    std::vector<GridLength> rowDefs;
    rowDefs.reserve(nRows);
    for (const auto& d : grid.row_definitions())
    {
        rowDefs.push_back(d.height);
    }

    out.col = resolve_grid_lengths(colDefs, autoCol, availW);
    out.row = resolve_grid_lengths(rowDefs, autoRow, availH);

    // D1 star min/re-award: a Star track that received a POSITIVE proportional
    // share grows to at least its content minimum (so a spanning/single-cell
    // child whose content exceeds the share re-awards the track to fit it).
    // Gated on `resolved > 0`: a star with NO share (totalStar==0, or a zero-
    // weight track) stays 0 regardless of content — WPF-faithful, keeps the
    // A4/A5/A6 "star collapses to 0 when there is no proportional share"
    // contract. A star with a positive share never shrinks below its content
    // and is never negative.
    for (std::size_t k = 0; k < nCols; ++k)
    {
        if (colDefs[k].unitType == GridUnitType::Star && out.col[k] > 0.0f)
        {
            out.col[k] = std::max(out.col[k], out.starMinCol[k]);
        }
    }
    for (std::size_t k = 0; k < nRows; ++k)
    {
        if (rowDefs[k].unitType == GridUnitType::Star && out.row[k] > 0.0f)
        {
            out.row[k] = std::max(out.row[k], out.starMinRow[k]);
        }
    }

    return out;
}

} // namespace

// Element constructors — MINIMAL REAL: an empty ctor is all a seed needs to be
// a linkable concrete type the factory registry can produce. Real construction
// (layout slots, named parts, templates) is each control's own sprint. The
// StackPanel ctor is NOT a seed anymore (S129): it derives UIElement and self-
// registers its Orientation DP.
VAML_CORE_EXPORT
Button::Button() = default;

VAML_CORE_EXPORT
StackPanel::StackPanel()
{
    ensure_stackpanel_properties_registered();
}

VAML_CORE_EXPORT
Grid::Grid()
    : impl_(std::make_unique<Impl>())
{
    ensure_grid_properties_registered();
}

VAML_CORE_EXPORT
Grid::~Grid() = default;

// S131 Grid pimpl: the owned row/column definition vectors. A fresh Grid has
// ONE implicit Auto row + ONE implicit Auto column (a Grid with no explicit
// definitions still lays out a single Auto track on each axis — WPF-faithful).
struct Grid::Impl
{
    std::vector<RowDefinition> rows;
    std::vector<ColumnDefinition> cols;

    Impl()
    {
        rows.emplace_back(); // one Auto row
        cols.emplace_back(); // one Auto column
    }
};

VAML_CORE_EXPORT
void Grid::set_row_definitions(std::vector<RowDefinition> rows_in)
{
    impl_->rows = std::move(rows_in);
    // Empty definition list collapses to the single implicit Auto row.
    if (impl_->rows.empty())
    {
        impl_->rows.emplace_back();
    }
    InvalidateMeasure();
    InvalidateArrange();
}

VAML_CORE_EXPORT
void Grid::set_column_definitions(std::vector<ColumnDefinition> cols_in)
{
    impl_->cols = std::move(cols_in);
    if (impl_->cols.empty())
    {
        impl_->cols.emplace_back();
    }
    InvalidateMeasure();
    InvalidateArrange();
}

VAML_CORE_EXPORT
const std::vector<RowDefinition>& Grid::row_definitions() const noexcept
{
    return impl_->rows;
}

VAML_CORE_EXPORT
const std::vector<ColumnDefinition>& Grid::column_definitions() const noexcept
{
    return impl_->cols;
}

VAML_CORE_EXPORT
SetResult Grid::set_row(DependencyObject& obj, std::int32_t value)
{
    // Value rides the Int32 path on the attached name (S043 attached-aware:
    // the key is owned by Grid, stored on `obj`).
    return obj.Set(PropertyId::of("Grid.Row"), Value::Int32(value));
}

VAML_CORE_EXPORT
std::int32_t Grid::row(const DependencyObject& obj)
{
    return obj.Get(PropertyId::of("Grid.Row")).get<std::int32_t>();
}

VAML_CORE_EXPORT
SetResult Grid::set_column(DependencyObject& obj, std::int32_t value)
{
    return obj.Set(PropertyId::of("Grid.Column"), Value::Int32(value));
}

VAML_CORE_EXPORT
std::int32_t Grid::column(const DependencyObject& obj)
{
    return obj.Get(PropertyId::of("Grid.Column")).get<std::int32_t>();
}

VAML_CORE_EXPORT
SetResult Grid::set_row_span(DependencyObject& obj, std::int32_t value)
{
    return obj.Set(PropertyId::of("Grid.RowSpan"), Value::Int32(value));
}

VAML_CORE_EXPORT
std::int32_t Grid::row_span(const DependencyObject& obj)
{
    return obj.Get(PropertyId::of("Grid.RowSpan")).get<std::int32_t>();
}

VAML_CORE_EXPORT
SetResult Grid::set_column_span(DependencyObject& obj, std::int32_t value)
{
    return obj.Set(PropertyId::of("Grid.ColumnSpan"), Value::Int32(value));
}

VAML_CORE_EXPORT
std::int32_t Grid::column_span(const DependencyObject& obj)
{
    return obj.Get(PropertyId::of("Grid.ColumnSpan")).get<std::int32_t>();
}

VAML_CORE_EXPORT
Text::Text() = default;

VAML_CORE_EXPORT
TextBox::TextBox() = default;

VAML_CORE_EXPORT
CheckBox::CheckBox() = default;

VAML_CORE_EXPORT
Slider::Slider() = default;

VAML_CORE_EXPORT
ScrollViewer::ScrollViewer() = default;

VAML_CORE_EXPORT
Window::Window() = default;

// --- S129 StackPanel: Orientation DP + real panel Measure/Arrange ----------

VAML_CORE_EXPORT
SetResult StackPanel::set_orientation(Orientation o)
{
    // Rides the Int32 value path (the enum stores as Int32, same convention as
    // Visibility / alignment enums).
    return Set(PropertyId::of("Orientation"),
               Value::Int32(static_cast<std::int32_t>(o)));
}

VAML_CORE_EXPORT
Orientation StackPanel::orientation() const
{
    return static_cast<Orientation>(Get(PropertyId::of("Orientation")).get<std::int32_t>());
}

// The panel's own content is its children. Orientation-aware measure:
//   * Vertical   — offer each child {available.width, ∞height} (a vertical
//     stack lets each child wrap to content vertically), then the panel's
//     natural extent = {max child width, Σ child heights}.
//   * Horizontal — offer each child {∞width, available.height}, natural =
//     {Σ child widths, max child height}.
// Dirty-respect (S128): re-measure ONLY is_measure_dirty() children; a clean
// child reuses its stored desired_size() (the S128 dirty-only-subpass pattern
// realized inside the real panel). Measure() (base) then finalizes this
// natural extent via the S123 infinity-aware stretch-vs-wrap rule.
VAML_CORE_EXPORT
Size StackPanel::MeasureOverride(const Size& available) const
{
    float width  = 0.0f;
    float height = 0.0f;

    if (orientation() == Orientation::Horizontal)
    {
        const Size rowAvail{ Size::Infinite().width, available.height };
        for (std::size_t i = 0; i < child_count(); ++i)
        {
            UIElement* child = child_at(i);
            if (child->is_measure_dirty())
            {
                child->Measure(rowAvail);
            }
            width += child->desired_size().width;
            height = (child->desired_size().height > height)
                         ? child->desired_size().height
                         : height;
        }
        return { width, height };
    }

    // Vertical (default).
    const Size columnAvail{ available.width, Size::Infinite().height };
    for (std::size_t i = 0; i < child_count(); ++i)
    {
        UIElement* child = child_at(i);
        if (child->is_measure_dirty())
        {
            child->Measure(columnAvail);
        }
        width = (child->desired_size().width > width)
                    ? child->desired_size().width
                    : width;
        height += child->desired_size().height;
    }
    return { width, height };
}

// The arrange-side virtual (S127). Receives the panel's FULL final render slot
// (final_rect()). Orientation-aware placement:
//   * Vertical   — children top-to-bottom, each into {slot.x, y, slot.width,
//     childHeight}, advancing y by the child's desired height; the LAST child
//     FILLS the remaining vertical space (last-child-stretch:
//     remaining = finalSlot.bottom() - y).
//   * Horizontal — left-to-right mirror; the LAST child fills the remaining
//     width.
// Each child's own alignment (Horizontal/Vertical + Margin) is honored inside
// its slot via child->Arrange (compute_final_rect), so a default-Stretch last
// child takes the leftover track while a non-Stretch last child is pinned
// within it (never over-fills). Dirty-respect (S128): re-arrange ONLY
// is_arrange_dirty() children; clean children keep their final_rect untouched.
VAML_CORE_EXPORT
void StackPanel::ArrangeOverride(const Rect& finalSlot)
{
    if (orientation() == Orientation::Horizontal)
    {
        float x = finalSlot.x;
        for (std::size_t i = 0; i < child_count(); ++i)
        {
            UIElement* child = child_at(i);
            const bool isLast = (i + 1 == child_count());
            const float remaining = finalSlot.right() - x;
            const float slotWidth = isLast ? remaining : child->desired_size().width;
            if (child->is_arrange_dirty())
            {
                child->Arrange(Rect{ x, finalSlot.y, slotWidth, finalSlot.height });
            }
            x += (isLast ? remaining : child->desired_size().width);
        }
        return;
    }

    // Vertical (default).
    float y = finalSlot.y;
    for (std::size_t i = 0; i < child_count(); ++i)
    {
        UIElement* child = child_at(i);
        const bool isLast = (i + 1 == child_count());
        const float remaining = finalSlot.bottom() - y;
        const float slotHeight = isLast ? remaining : child->desired_size().height;
        if (child->is_arrange_dirty())
        {
            child->Arrange(Rect{ finalSlot.x, y, finalSlot.width, slotHeight });
        }
        y += (isLast ? remaining : child->desired_size().height);
    }
}

// --- S131/S132 Grid: row/col resolution + span-aware measure/arrange ---------
//
// The measure pass resolves both track axes against `available` and measures
// each child into its (single or MERGED) cell:
//   1. Measure every child at an unbounded-width / finite-height offer so its
//      desired extent is content-based (S123 infinity-aware).
//   2. Auto extents + star content-minima accumulated via the span-aware equal-
//      division rule (D2): a spanning child contributes desired/span to each
//      spanned track (S132).
//   3. Resolve all column then all row lengths via resolve_grid_lengths, then
//      D1 star min/re-award: a Star track grows to at least its content
//      minimum (S132).
//   4. Re-measure ONLY dirty children at their resolved MERGED cell slot
//      (S128 dirty-respect; a Stretch child now fills its whole spanned cell).
// The panel's own DesiredSize = Σ(non-star resolved) + Σ(star content-minima)
// — the content-natural extent a parent should offer the panel.
VAML_CORE_EXPORT
Size Grid::MeasureOverride(const Size& available) const
{
    const std::size_t nRows = impl_->rows.size();
    const std::size_t nCols = impl_->cols.size();

    // First measure pass: content-based offer (unbounded width, finite height)
    // so desired extents reflect content (S123 infinity-aware).
    for (std::size_t i = 0; i < child_count(); ++i)
    {
        UIElement* c = child_at(i);
        if (c->is_measure_dirty())
        {
            c->Measure(Size{ Size::Infinite().width, available.height });
        }
    }

    const GridTrackResolution res =
        resolve_grid_with_spans(*this, nRows, nCols, available.width, available.height);

    // Second pass: re-measure ONLY dirty children at their resolved MERGED cell
    // slot (S128 dirty-respect; a Stretch child fills its whole spanned cell).
    for (std::size_t i = 0; i < child_count(); ++i)
    {
        UIElement* c = child_at(i);
        if (!c->is_measure_dirty())
        {
            continue;
        }
        const std::size_t col = static_cast<std::size_t>(
            std::min<std::int32_t>(std::max(Grid::column(*c), 0), static_cast<std::int32_t>(nCols) - 1));
        const std::size_t r = static_cast<std::size_t>(
            std::min<std::int32_t>(std::max(Grid::row(*c), 0), static_cast<std::int32_t>(nRows) - 1));
        // Merged (spanned) cell extent: sum of the resolved tracks the child's
        // span covers.
        const std::int32_t cspanRaw = std::max<std::int32_t>(Grid::column_span(*c), 1);
        const std::int32_t rspanRaw = std::max<std::int32_t>(Grid::row_span(*c), 1);
        const std::size_t cspan = static_cast<std::size_t>(
            std::min<std::int32_t>(cspanRaw, static_cast<std::int32_t>(nCols) - static_cast<std::int32_t>(col)));
        const std::size_t rspan = static_cast<std::size_t>(
            std::min<std::int32_t>(rspanRaw, static_cast<std::int32_t>(nRows) - static_cast<std::int32_t>(r)));

        float mergedW = 0.0f;
        float mergedH = 0.0f;
        for (std::size_t k = col; k < col + cspan; ++k)
        {
            mergedW += res.col[k];
        }
        for (std::size_t k = r; k < r + rspan; ++k)
        {
            mergedH += res.row[k];
        }
        c->Measure(Size{ mergedW, mergedH });
    }

    // Panel DesiredSize = Σ(non-star resolved) + Σ(star content-minima) on each
    // axis — the content-natural extent (S131 basic scope delivered star=0
    // minimum; S132 upgrades to the star content-minimum, NO-GAP).
    float desiredW = 0.0f;
    float desiredH = 0.0f;
    for (std::size_t c = 0; c < nCols; ++c)
    {
        if (impl_->cols[c].width.unitType != GridUnitType::Star)
        {
            desiredW += res.col[c];
        }
        else
        {
            desiredW += res.starMinCol[c];
        }
    }
    for (std::size_t r = 0; r < nRows; ++r)
    {
        if (impl_->rows[r].height.unitType != GridUnitType::Star)
        {
            desiredH += res.row[r];
        }
        else
        {
            desiredH += res.starMinRow[r];
        }
    }
    desiredW = std::max(0.0f, desiredW);
    desiredH = std::max(0.0f, desiredH);

    // The panel's own desired extent (the natural). Compute_measured_desired
    // (parent Measure finalizes with alignment/margin) turns this into the
    // returned DesiredSize, so return the natural content extent here.
    return Size{ desiredW, desiredH };
}

// The arrange-side virtual (S127). Receives the panel's FULL final render slot.
// Resolves every track length against finalSlot's extent (span-aware, S132),
// computes the cell ORIGINS as cumulative track sums, and places each child
// into its MERGED span rect {anchor origin, Σ spanned extents} via child->Arrange
// (honoring the child's own alignment + margin inside the merged cell).
// Dirty-respect (S128): re-arrange ONLY is_arrange_dirty() children; clean
// children keep their final_rect untouched.
VAML_CORE_EXPORT
void Grid::ArrangeOverride(const Rect& finalSlot)
{
    const std::size_t nRows = impl_->rows.size();
    const std::size_t nCols = impl_->cols.size();

    const GridTrackResolution res =
        resolve_grid_with_spans(*this, nRows, nCols, finalSlot.width, finalSlot.height);

    // Cell origins: cumulative track sums from the slot origin.
    std::vector<float> colX(nCols + 1, finalSlot.x);
    std::vector<float> rowY(nRows + 1, finalSlot.y);
    for (std::size_t c = 0; c < nCols; ++c)
    {
        colX[c + 1] = colX[c] + res.col[c];
    }
    for (std::size_t r = 0; r < nRows; ++r)
    {
        rowY[r + 1] = rowY[r] + res.row[r];
    }

    for (std::size_t i = 0; i < child_count(); ++i)
    {
        UIElement* c = child_at(i);
        const std::size_t col = static_cast<std::size_t>(
            std::min<std::int32_t>(std::max(Grid::column(*c), 0), static_cast<std::int32_t>(nCols) - 1));
        const std::size_t r = static_cast<std::size_t>(
            std::min<std::int32_t>(std::max(Grid::row(*c), 0), static_cast<std::int32_t>(nRows) - 1));
        const std::int32_t cspanRaw = std::max<std::int32_t>(Grid::column_span(*c), 1);
        const std::int32_t rspanRaw = std::max<std::int32_t>(Grid::row_span(*c), 1);
        const std::size_t cspan = static_cast<std::size_t>(
            std::min<std::int32_t>(cspanRaw, static_cast<std::int32_t>(nCols) - static_cast<std::int32_t>(col)));
        const std::size_t rspan = static_cast<std::size_t>(
            std::min<std::int32_t>(rspanRaw, static_cast<std::int32_t>(nRows) - static_cast<std::int32_t>(r)));

        // Merged span rect: anchor origin + Σ spanned track extents.
        const Rect cell{
            colX[col],
            rowY[r],
            colX[col + cspan] - colX[col],
            rowY[r + rspan] - rowY[r],
        };
        if (c->is_arrange_dirty())
        {
            c->Arrange(cell);
        }
    }
}

VAML_CORE_EXPORT
void register_builtin_elements()
{
    ElementFactoryRegistry& registry = ElementFactoryRegistry::global();

    // register_builtin_elements() is documented single-threaded startup; the
    // bool guards against a stray second call (which would otherwise throw
    // ElementAlreadyRegistered). NO-LAZY: this is a startup-ordering guard, not
    // dead code — a duplicated population is a real double-registration hazard
    // for a process-wide singleton.
    static bool registered = false;
    if (registered)
    {
        return;
    }
    registered = true;

    // Register the roadmap element vocabulary. Factories return a fresh,
    // owned instance of the concrete seed class.
    registry.register_element("Button",       [] { return std::make_unique<Button>(); });
    registry.register_element("StackPanel",   [] { return std::make_unique<StackPanel>(); });
    registry.register_element("Grid",         [] { return std::make_unique<Grid>(); });
    registry.register_element("Text",         [] { return std::make_unique<Text>(); });
    registry.register_element("TextBox",      [] { return std::make_unique<TextBox>(); });
    registry.register_element("CheckBox",     [] { return std::make_unique<CheckBox>(); });
    registry.register_element("Slider",       [] { return std::make_unique<Slider>(); });
    registry.register_element("ScrollViewer", [] { return std::make_unique<ScrollViewer>(); });
    registry.register_element("Window",       [] { return std::make_unique<Window>(); });
}

} // namespace vaml
