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

// --- S131 Grid: row/col resolution + basic (no-span) cell measure/arrange ---
//
// The measure pass resolves both track axes against `available` and measures
// each child into its cell:
//   1. Measure every child at an unbounded-width / finite-height offer so its
//      desired extent is content-based (S123 infinity-aware).
//   2. Auto column extents = max over the children's desired WIDTH in that
//      column; Auto row extents = max over the children's desired HEIGHT in
//      that row (the no-span basic scope — each child occupies one cell).
//   3. Resolve all column then all row lengths via resolve_grid_lengths
//      (Absolute = value; Auto = measured; Star = proportional remaining).
//   4. Re-measure ONLY dirty children at their resolved cell slot so a Stretch
//      child fills its cell (S128 dirty-respect).
// The panel's own DesiredSize = Σ(Absolute + Auto-resolved) on each axis —
// Star tracks contribute their MINIMUM (0) here; min/sun sizing fidelity is
// S132/S133's named scope (recorded NO-GAP, not silent).
VAML_CORE_EXPORT
Size Grid::MeasureOverride(const Size& available) const
{
    const std::size_t nRows = impl_->rows.size();
    const std::size_t nCols = impl_->cols.size();

    std::vector<float> autoCol(nCols, 0.0f);
    std::vector<float> autoRow(nRows, 0.0f);

    // First measure pass: content-based offer (unbounded width, finite height)
    // so Auto extents reflect content, then accumulate per-cell maxima.
    for (std::size_t i = 0; i < child_count(); ++i)
    {
        UIElement* c = child_at(i);
        const std::size_t col = static_cast<std::size_t>(
            std::min<std::int32_t>(std::max(Grid::column(*c), 0), static_cast<std::int32_t>(nCols) - 1));
        const std::size_t r = static_cast<std::size_t>(
            std::min<std::int32_t>(std::max(Grid::row(*c), 0), static_cast<std::int32_t>(nRows) - 1));
        if (c->is_measure_dirty())
        {
            c->Measure(Size{ Size::Infinite().width, available.height });
        }
        autoCol[col] = std::max(autoCol[col], c->desired_size().width);
        autoRow[r] = std::max(autoRow[r], c->desired_size().height);
    }

    // Column definitions + resolution.
    std::vector<GridLength> colDefs;
    colDefs.reserve(nCols);
    for (const auto& d : impl_->cols)
    {
        colDefs.push_back(d.width);
    }
    const std::vector<float> colResolved = resolve_grid_lengths(colDefs, autoCol, available.width);

    // Row definitions + resolution.
    std::vector<GridLength> rowDefs;
    rowDefs.reserve(nRows);
    for (const auto& d : impl_->rows)
    {
        rowDefs.push_back(d.height);
    }
    const std::vector<float> rowResolved = resolve_grid_lengths(rowDefs, autoRow, available.height);

    // Second pass: re-measure ONLY dirty children at their resolved cell slot
    // (a Stretch child now fills its cell; a non-Stretch child wraps within).
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
        c->Measure(Size{ colResolved[col], rowResolved[r] });
    }

    // Panel DesiredSize = Σ(Absolute + Auto) on each axis; Star contributes 0
    // minimum (basic scope — recorded NO-GAP, not silent).
    float desiredW = 0.0f;
    float desiredH = 0.0f;
    for (std::size_t c = 0; c < nCols; ++c)
    {
        if (colDefs[c].unitType != GridUnitType::Star)
        {
            desiredW += colResolved[c];
        }
    }
    for (std::size_t r = 0; r < nRows; ++r)
    {
        if (rowDefs[r].unitType != GridUnitType::Star)
        {
            desiredH += rowResolved[r];
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
// Resolves every track length against finalSlot's extent, computes the cell
// ORIGINS as cumulative track sums, and places each child into its cell
// {origin, cell size} via child->Arrange (honoring the child's own alignment +
// margin inside the cell). Dirty-respect (S128): re-arrange ONLY
// is_arrange_dirty() children; clean children keep their final_rect untouched.
// Basic no-span scope (each child occupies exactly one cell) — RowSpan/
// ColumnSpan is S132's named step (recorded NO-GAP, not silent).
VAML_CORE_EXPORT
void Grid::ArrangeOverride(const Rect& finalSlot)
{
    const std::size_t nRows = impl_->rows.size();
    const std::size_t nCols = impl_->cols.size();

    // Recompute Auto extents from the children's STORED desired_size (no re-
    // measure in arrange — arrange is size/position only).
    std::vector<float> autoCol(nCols, 0.0f);
    std::vector<float> autoRow(nRows, 0.0f);
    for (std::size_t i = 0; i < child_count(); ++i)
    {
        UIElement* c = child_at(i);
        const std::size_t col = static_cast<std::size_t>(
            std::min<std::int32_t>(std::max(Grid::column(*c), 0), static_cast<std::int32_t>(nCols) - 1));
        const std::size_t r = static_cast<std::size_t>(
            std::min<std::int32_t>(std::max(Grid::row(*c), 0), static_cast<std::int32_t>(nRows) - 1));
        autoCol[col] = std::max(autoCol[col], c->desired_size().width);
        autoRow[r] = std::max(autoRow[r], c->desired_size().height);
    }

    std::vector<GridLength> colDefs;
    colDefs.reserve(nCols);
    for (const auto& d : impl_->cols)
    {
        colDefs.push_back(d.width);
    }
    std::vector<GridLength> rowDefs;
    rowDefs.reserve(nRows);
    for (const auto& d : impl_->rows)
    {
        rowDefs.push_back(d.height);
    }

    const std::vector<float> colResolved = resolve_grid_lengths(colDefs, autoCol, finalSlot.width);
    const std::vector<float> rowResolved = resolve_grid_lengths(rowDefs, autoRow, finalSlot.height);

    // Cell origins: cumulative track sums from the slot origin.
    std::vector<float> colX(nCols + 1, finalSlot.x);
    std::vector<float> rowY(nRows + 1, finalSlot.y);
    for (std::size_t c = 0; c < nCols; ++c)
    {
        colX[c + 1] = colX[c] + colResolved[c];
    }
    for (std::size_t r = 0; r < nRows; ++r)
    {
        rowY[r + 1] = rowY[r] + rowResolved[r];
    }

    for (std::size_t i = 0; i < child_count(); ++i)
    {
        UIElement* c = child_at(i);
        const std::size_t col = static_cast<std::size_t>(
            std::min<std::int32_t>(std::max(Grid::column(*c), 0), static_cast<std::int32_t>(nCols) - 1));
        const std::size_t r = static_cast<std::size_t>(
            std::min<std::int32_t>(std::max(Grid::row(*c), 0), static_cast<std::int32_t>(nRows) - 1));
        const Rect cell{ colX[col], rowY[r], colResolved[col], rowResolved[r] };
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
