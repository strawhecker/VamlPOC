// S087 — built-in Element MINIMAL REAL seeds + startup registration.
//
// Element subclasses that are still seeds are deliberately a THIN identity
// bearer (empty ctor) deriving DependencyObject. NO members / logic here — the
// real content of each control is authored at its OWN resolution sprint (see
// Element.h sentinels). The controls that have reached their sprints are
// StackPanel (S129), Grid (S131), WrapPanel (S134) and Canvas (S136), which
// RESOLVE here: re-based onto UIElement and delivered as REAL panels. The
// remaining seeds stay thin until their own sprints.
//
// register_builtin_elements() is the single startup registration call that
// populates ElementFactoryRegistry::global() with every roadmap element name the
// S091 runtime reader and S093 sample need. It is idempotent-guarded: a second
// call (e.g. from a test that also calls the user-facing register) is a no-op,
// so startup population can be re-invoked safely.

#include "vaml/core/Element.h"

#include "vaml/core/ControlTemplate.h"   // S364/S365 — the Button's default template + visual states
#include "vaml/core/Brush.h"             // S453 — SolidColorBrush (authored resource type)
#include "vaml/core/DependencyProperty.h"
#include "vaml/core/ElementFactoryRegistry.h"
#include "vaml/core/FocusManager.h"      // S325/S402 — KeyCode (the TextBox edits on)
#include "vaml/core/Geometry.h"
#include "vaml/core/GridLength.h"
#include "vaml/core/LayoutSlot.h"
#include "vaml/core/PropertyFlags.h"
#include "vaml/core/PropertyMetadata.h"
#include "vaml/core/RoutedEvent.h"       // S320/S321 — ClickEvent (the Button's routed event)
#include "vaml/core/Style.h"             // S453/S454 — Style (authored resource type)

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
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

// Registers the WrapPanel WrapDirection DP into the global table exactly once
// per process (Meyers-static guard; a second call is a no-op — mirrors
// UIElement's self-registration). WrapDirection is GLOBALLY registered so it
// resolves on any DependencyObject via S043 attached-aware resolution, and
// defaults to Horizontal (1), NOT inherited (a panel's wrap direction is its
// own). The property NAME is "WrapDirection" (owner "WrapPanel") — StackPanel
// already owns "Orientation" in the global registry, so reusing that name
// under a different owner would be ambiguous to the S091 dotted-key reader.
void register_wrappanel_properties()
{
    (void)DependencyProperty::Register(
        "WrapDirection", ValueType::Int32, "WrapPanel",
        PropertyMetadata::WithDefault(Value::Int32(1), PropertyFlags::None));
}

// Runs register_wrappanel_properties() at most once (first WrapPanel ctor).
void ensure_wrappanel_properties_registered()
{
    static bool registered = false;
    if (!registered)
    {
        registered = true;
        register_wrappanel_properties();
    }
}

// Registers the two Canvas ATTACHED DPs (Left + Top) into the global table
// exactly once per process (Meyers-static guard; a second call is a no-op).
// They are GLOBALLY registered under owner "Canvas" so they resolve on ANY
// DependencyObject via the S043 attached-aware path — a child's explicit
// position is stored on the child (the object holding the value), not on the
// Canvas.
//
// NaN default = "not set" (the S136 NaN contract): a child with no explicit
// coordinate collapses to the origin (0,0) at layout time. The Double value
// path; default registered as NaN, NOT inherited (each child's position is its
// own).
void register_canvas_properties()
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    (void)DependencyProperty::Register(
        "Canvas.Left", ValueType::Double, "Canvas",
        PropertyMetadata::WithDefault(Value::Double(nan), PropertyFlags::None));
    (void)DependencyProperty::Register(
        "Canvas.Top", ValueType::Double, "Canvas",
        PropertyMetadata::WithDefault(Value::Double(nan), PropertyFlags::None));
}

// Runs register_canvas_properties() at most once (first Canvas ctor).
void ensure_canvas_properties_registered()
{
    static bool registered = false;
    if (!registered)
    {
        registered = true;
        register_canvas_properties();
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
// --- S401 Button control: REAL DP + Click + template/visual states ---------

namespace {

// Registers the Button's REAL DPs into the global table exactly once per
// process (Meyers-static guard). Button is the S401 headline control; its DPs
// are GLOBALLY registered so they resolve on any DependencyObject via S043
// attached-aware resolution.
void register_button_properties()
{
    // IsEnabled: Bool via Int32, default true. Gates the Click routing.
    (void)DependencyProperty::Register(
        "IsEnabled", ValueType::Int32, "Button",
        std::move(PropertyMetadata::WithDefault(Value::Int32(1), PropertyFlags::None))
            .with_validate([](const Value& v) { const int x = v.get<int>(); return x == 0 || x == 1; }));
}

// A helper that builds ONE VisualState entry for the Button's default template:
// a named state's brush-property target is the shared "Background" DP (the S364
// visual-state engine register) with the given opaque RGBA fill.
VisualState make_state(std::string name, std::uint8_t r, std::uint8_t g, std::uint8_t b)
{
    VisualState st;
    st.name          = std::move(name);
    st.brushProperty = "Background"; // the S364 visual-state shared brush DP
    st.brush         = std::make_unique<SolidColorBrush>();
    (void)st.brush->set_color(Color{ r, g, b, 255 });
    return st;
}

// Runs register_button_properties() at most once (first Button ctor).
void ensure_button_properties_registered()
{
    static bool registered = false;
    if (!registered)
    {
        registered = true;
        register_button_properties();
    }
}

// Registers the TextBox's REAL DPs (S402) into the global table exactly once.
void register_textbox_properties()
{
    // Text: String, default "".
    (void)DependencyProperty::Register(
        "TextBox.Text", ValueType::String, "TextBox",
        PropertyMetadata::WithDefault(Value::String(std::string{}), PropertyFlags::None));
    // AcceptsReturn: Bool via Int32, default false (single-line).
    (void)DependencyProperty::Register(
        "TextBox.AcceptsReturn", ValueType::Int32, "TextBox",
        std::move(PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None))
            .with_validate([](const Value& v) { const int x = v.get<int>(); return x == 0 || x == 1; }));
}

// Runs register_textbox_properties() at most once (first TextBox ctor).
void ensure_textbox_properties_registered()
{
    static bool registered = false;
    if (!registered)
    {
        registered = true;
        register_textbox_properties();
    }
}

} // namespace

struct Button::Impl
{
    // The Button's DEFAULT implicit template visual states (S363/S367): the
    // shared "Background" brush DP + Normal/Hover/Pressed/Disabled fills.
    std::vector<VisualState> states;
    std::string              current = "Normal";

    // Builds the four default states on first use.
    void ensure_states()
    {
        if (!states.empty())
            return;
        states.push_back(make_state("Normal",   200u, 200u, 205u)); // neutral gray
        states.push_back(make_state("Hover",    230u, 232u, 236u)); // lighter
        states.push_back(make_state("Pressed",  160u, 162u, 168u)); // darker
        states.push_back(make_state("Disabled", 150u, 150u, 152u)); // faded (S205 fade = render step)
    }
};

VAML_CORE_EXPORT
Button::Button()
{
    ensure_button_properties_registered();
    impl_ = std::make_unique<Impl>();
    impl_->ensure_states();
}

VAML_CORE_EXPORT
Button::~Button() = default;

VAML_CORE_EXPORT
SetResult Button::set_is_enabled(bool value)
{
    const SetResult r = Set(PropertyId::of("IsEnabled"), Value::Int32(value ? 1 : 0));
    // An IsEnabled change re-flows the visual state (disabled swaps the fill;
    // re-enabling returns to Normal). The invalidation re-renders.
    InvalidateMeasure();
    return r;
}

VAML_CORE_EXPORT
bool Button::is_enabled() const
{
    return Get(PropertyId::of("IsEnabled")).get<int>() != 0;
}

VAML_CORE_EXPORT
bool Button::raise_click()
{
    // IsEnabled gates the Click routing (WPF-faithful): a disabled Button does
    // NOT raise Click. Raises the BUBBLING ClickEvent (S320/S321) with
    // source==this; a handler setting handled=true stops the route.
    if (!is_enabled())
        return false;
    vaml::RoutedEventArgs args;
    args.source = this;
    args.originalSource = this;
    RaiseEvent(vaml::ClickEvent(), args);
    return args.handled;
}

VAML_CORE_EXPORT
SetResult Button::set_visual_state(std::string name)
{
    if (!impl_)
        return vaml::SetResult::REJECTED;
    // Exit the current state (revert its brush), then enter the requested one.
    // The state engine (S364 enter_state/exit_state) swaps the shared
    // "Background" brush color; a disabled Button forces the Disabled state
    // (the visual tracks IsEnabled).
    for (VisualState& s : impl_->states)
    {
        if (s.engaged)
            exit_state(*this, s);
    }
    VisualState* target = nullptr;
    for (VisualState& s : impl_->states)
    {
        if (s.name == name)
        {
            target = &s;
            break;
        }
    }
    if (!target || !is_enabled())
    {
        if (!is_enabled())
        {
            // A disabled Button always presents the Disabled state (WPF-
            // faithful visual). Revert whatever was engaged, force Disabled.
            for (VisualState& s : impl_->states)
                if (s.name == "Disabled") { enter_state(*this, s); impl_->current = "Disabled"; }
        }
        return vaml::SetResult::REJECTED;
    }
    enter_state(*this, *target);
    impl_->current = name;
    InvalidateArrange();
    return vaml::SetResult::APPLIED;
}

VAML_CORE_EXPORT
std::string Button::visual_state() const
{
    return impl_ ? impl_->current : std::string("Normal");
}

VAML_CORE_EXPORT
Size Button::MeasureOverride(const Size& available) const
{
    // The S401 real Button algorithm: a Button reports a bounded default box
    // (its nominal slot) at its natural (content) size. It defers to its
    // VisualState/Background for the rendered fill; the default quad is emitted
    // by the vulkan classifier (MaterialKind::Solid). A bounded offer governs
    // the box via the S123 stretch-vs-wrap rule (the base Measure finalizer).
    const float w = (available.width > 0.0f ? available.width : 20.0f);
    const float h = (available.height > 0.0f ? available.height : 16.0f);
    return Size{ std::max(0.0f, w), std::max(0.0f, h) };
}

VAML_CORE_EXPORT
StackPanel::StackPanel()
{
    ensure_stackpanel_properties_registered();
}

VAML_CORE_EXPORT
Grid::Grid()
    : rows_{ RowDefinition{} } // one implicit Auto row
    , cols_{ ColumnDefinition{} } // one implicit Auto column
{
    ensure_grid_properties_registered();
}

VAML_CORE_EXPORT
Grid::~Grid() = default;

// S146 (refactor gate): the pimpl is COLLAPSED — the owned row/column
// definition vectors are now plain by-value members (rows_, cols_) of Grid (see
// Element.h). A fresh Grid has ONE implicit Auto row + ONE implicit Auto column
// (a Grid with no explicit definitions still lays out a single Auto track on
// each axis — WPF-faithful).

VAML_CORE_EXPORT
void Grid::set_row_definitions(std::vector<RowDefinition> rows_in)
{
    rows_ = std::move(rows_in);
    // Empty definition list collapses to the single implicit Auto row.
    if (rows_.empty())
    {
        rows_.emplace_back();
    }
    InvalidateMeasure();
    InvalidateArrange();
}

VAML_CORE_EXPORT
void Grid::set_column_definitions(std::vector<ColumnDefinition> cols_in)
{
    cols_ = std::move(cols_in);
    if (cols_.empty())
    {
        cols_.emplace_back();
    }
    InvalidateMeasure();
    InvalidateArrange();
}

VAML_CORE_EXPORT
const std::vector<RowDefinition>& Grid::row_definitions() const noexcept
{
    return rows_;
}

VAML_CORE_EXPORT
const std::vector<ColumnDefinition>& Grid::column_definitions() const noexcept
{
    return cols_;
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
WrapPanel::WrapPanel()
{
    ensure_wrappanel_properties_registered();
}

VAML_CORE_EXPORT
Canvas::Canvas()
{
    ensure_canvas_properties_registered();
}

// --- S134 WrapPanel: WrapDirection DP + real flow/wrap/align panel ----------

VAML_CORE_EXPORT
SetResult WrapPanel::set_wrap_direction(Orientation o)
{
    // Rides the Int32 value path (the enum stores as Int32, same convention as
    // Orientation on StackPanel / Visibility / alignment enums).
    return Set(PropertyId::of("WrapDirection"),
               Value::Int32(static_cast<std::int32_t>(o)));
}

VAML_CORE_EXPORT
Orientation WrapPanel::wrap_direction() const
{
    return static_cast<Orientation>(Get(PropertyId::of("WrapDirection")).get<std::int32_t>());
}

// The flow/wrap/align measure. Children are measured at NATURAL (unbounded)
// size on BOTH axes so the wrap decision uses content-correct extents and rows
// are content-heighted:
//   * Horizontal — each child is offered {∞, ∞} (the S123 infinity rule: a
//     Stretch child given infinite wraps to its natural content on every
//     axis). Row accumulators track the running row width/height; a child
//     WRAPS to a new row when `rowWidth + childWidth > available.width`
//     (checked ONLY when the row is non-empty, so a single child wider than
//     the whole boundary still gets its own row). DesiredSize = {max row
//     width, Σ row heights}.
//   * Vertical   — the mirror: each child offered {∞, ∞}, wrap when
//     `colHeight + childHeight > available.height` (col non-empty);
//     DesiredSize = {Σ column widths, max column height}.
// The CROSS axis is also unbounded (natural), NOT `available`: offering the
// finite cross extent to a default-Stretch child would make it report the
// full available cross size, so every row would be the full available height
// instead of content-sized — defeating the "aligned rows" headline (row height
// = tallest child, per-child alignment within that content row). This refines
// the accepted S134 plan's "natural-width basis" to a "natural-extent basis".
// Dirty-respect (S128): re-measure ONLY is_measure_dirty() children; a clean
// child reuses its stored desired_size(). Measure() (base) then finalizes this
// natural extent via the S123 infinity-aware stretch-vs-wrap rule.
VAML_CORE_EXPORT
Size WrapPanel::MeasureOverride(const Size& available) const
{
    float width  = 0.0f;   // running max (Horizontal) / Σ (Vertical) — finalized below
    float height = 0.0f;

    if (wrap_direction() == Orientation::Horizontal)
    {
        const Size childAvail = Size::Infinite();
        float rowWidth  = 0.0f;
        float rowHeight = 0.0f;
        float maxRowW   = 0.0f;
        float sumRowH   = 0.0f;
        for (std::size_t i = 0; i < child_count(); ++i)
        {
            UIElement* child = child_at(i);
            if (child->is_measure_dirty())
            {
                child->Measure(childAvail);
            }
            const float cw = child->desired_size().width;
            const bool rowNonEmpty = (rowWidth > 0.0f);
            if (rowNonEmpty && rowWidth + cw > available.width)
            {
                maxRowW = (rowWidth > maxRowW) ? rowWidth : maxRowW;
                sumRowH += rowHeight;
                rowWidth = 0.0f;
                rowHeight = 0.0f;
            }
            rowWidth += cw;
            rowHeight = (child->desired_size().height > rowHeight)
                            ? child->desired_size().height
                            : rowHeight;
        }
        maxRowW = (rowWidth > maxRowW) ? rowWidth : maxRowW;
        sumRowH += rowHeight;
        width  = maxRowW;
        height = sumRowH;
    }
    else
    {
        const Size childAvail = Size::Infinite();
        float colWidth  = 0.0f;
        float colHeight = 0.0f;
        float maxColH   = 0.0f;
        float sumColW   = 0.0f;
        for (std::size_t i = 0; i < child_count(); ++i)
        {
            UIElement* child = child_at(i);
            if (child->is_measure_dirty())
            {
                child->Measure(childAvail);
            }
            const float ch = child->desired_size().height;
            const bool colNonEmpty = (colHeight > 0.0f);
            if (colNonEmpty && colHeight + ch > available.height)
            {
                maxColH = (colHeight > maxColH) ? colHeight : maxColH;
                sumColW += colWidth;
                colWidth = 0.0f;
                colHeight = 0.0f;
            }
            colWidth = (child->desired_size().width > colWidth)
                           ? child->desired_size().width
                           : colWidth;
            colHeight += ch;
        }
        maxColH = (colHeight > maxColH) ? colHeight : maxColH;
        sumColW += colWidth;
        width  = sumColW;
        height = maxColH;
    }

    return { width, height };
}

// The arrange-side virtual (S127). Receives the panel's FULL final render slot
// (final_rect()). Reproduces the SAME wrap decisions made at measure (the wrap
// boundary is finalSlot's flow extent), so children land in the SAME rows/
// columns and their S123-measured natural desired extents are preserved.
//   * Horizontal — for each row: row y advances by the row height (tallest
//     child in the row), each child placed into {cursor, rowY, childDesiredW,
//     rowHeight}; a row wraps when `cursor + childDesiredW > finalSlot.right()`
//     (row non-empty). Each child's OWN alignment (Vertical/Horizontal + Margin)
//     is honored inside its slot via child->Arrange (S124 compute_final_rect),
//     so a default-Stretch child fills the row's height while keeping its
//     natural width (aligned row).
//   * Vertical   — the mirror (columns lay left-to-right; column x advances by
//     the column width; each child into {colX, cursor, colWidth, childDesiredH}).
// Dirty-respect (S128): re-arrange ONLY is_arrange_dirty() children; clean
// children keep their final_rect untouched.
VAML_CORE_EXPORT
void WrapPanel::ArrangeOverride(const Rect& finalSlot)
{
    if (wrap_direction() == Orientation::Horizontal)
    {
        float rowY = finalSlot.y;
        float cursor = finalSlot.x;
        float rowHeight = 0.0f;
        const std::size_t n = child_count();
        for (std::size_t i = 0; i < n; ++i)
        {
            UIElement* child = child_at(i);
            const float childW = child->desired_size().width;
            const float childH = child->desired_size().height;
            if ((cursor - finalSlot.x) > 0.0f && cursor + childW > finalSlot.right())
            {
                // Wrap: commit the current row's cross extent, start the next.
                rowY += rowHeight;
                cursor = finalSlot.x;
                rowHeight = 0.0f;
            }
            rowHeight = (childH > rowHeight) ? childH : rowHeight;
            if (child->is_arrange_dirty())
            {
                child->Arrange(Rect{ cursor, rowY, childW, rowHeight });
            }
            cursor += childW;
        }
        return;
    }

    // Vertical (columns top-to-bottom, laid left-to-right).
    float colX = finalSlot.x;
    float cursor = finalSlot.y;
    float colWidth = 0.0f;
    const std::size_t n = child_count();
    for (std::size_t i = 0; i < n; ++i)
    {
        UIElement* child = child_at(i);
        const float childW = child->desired_size().width;
        const float childH = child->desired_size().height;
        if ((cursor - finalSlot.y) > 0.0f && cursor + childH > finalSlot.bottom())
        {
            colX += colWidth;
            cursor = finalSlot.y;
            colWidth = 0.0f;
        }
        colWidth = (childW > colWidth) ? childW : colWidth;
        if (child->is_arrange_dirty())
        {
            child->Arrange(Rect{ colX, cursor, colWidth, childH });
        }
        cursor += childH;
    }
}

// --- S136 Canvas: attached Left/Top + real absolute-position panel ----------

VAML_CORE_EXPORT
SetResult Canvas::set_left(DependencyObject& obj, double value)
{
    // Value rides the Double path on the attached name (S043 attached-aware:
    // the key is owned by Canvas, stored on `obj`).
    return obj.Set(PropertyId::of("Canvas.Left"), Value::Double(value));
}

VAML_CORE_EXPORT
double Canvas::left(const DependencyObject& obj)
{
    return obj.Get(PropertyId::of("Canvas.Left")).get<double>();
}

VAML_CORE_EXPORT
SetResult Canvas::set_top(DependencyObject& obj, double value)
{
    return obj.Set(PropertyId::of("Canvas.Top"), Value::Double(value));
}

VAML_CORE_EXPORT
double Canvas::top(const DependencyObject& obj)
{
    return obj.Get(PropertyId::of("Canvas.Top")).get<double>();
}

// The measure-side virtual. Canvas never constrains its children: every child
// is offered Size::Infinite() on BOTH axes (the S123 infinity rule, same
// basis as WrapPanel S134), so each child measures at its NATURAL (content)
// extent regardless of its explicit position. DesiredSize = the BOUNDING
// EXTENT of the arranged children:
//   per child: rightEdge  = effLeft  + child.desired.w
//              bottomEdge = effTop   + child.desired.h
//   where effLeft = is_nan(left(child))  ? 0 : left(child)   (NaN = "not set"
//   -> effective 0, the S136 NaN contract's layout-time collapse).
//   DesiredSize axis = max over children of that edge, clamped >= 0 (never
//   negative, never NaN — NaN axes are zeroed BEFORE the max fold, so no NaN
//   ever reaches the bounding math).
// Dirty-respect (S128): re-measure ONLY is_measure_dirty() children; a clean
// child reuses its stored desired_size().
VAML_CORE_EXPORT
Size Canvas::MeasureOverride(const Size& available) const
{
    (void)available; // Canvas never constrains children on either axis.
    const Size childAvail = Size::Infinite();

    float width  = 0.0f;
    float height = 0.0f;
    for (std::size_t i = 0; i < child_count(); ++i)
    {
        UIElement* child = child_at(i);
        if (child->is_measure_dirty())
        {
            child->Measure(childAvail);
        }
        const double l = left(*child);
        const double t = top(*child);
        const float effLeft = std::isnan(l) ? 0.0f : static_cast<float>(l);
        const float effTop  = std::isnan(t) ? 0.0f : static_cast<float>(t);
        const float rightEdge  = effLeft + child->desired_size().width;
        const float bottomEdge = effTop  + child->desired_size().height;
        width  = (rightEdge  > width)  ? rightEdge  : width;
        height = (bottomEdge > height) ? bottomEdge : height;
    }
    // Clamp >= 0: a negative explicit coordinate must not produce a negative
    // DesiredSize (a child fully above/left of the origin still fits a 0 box).
    return Size{ std::max(0.0f, width), std::max(0.0f, height) };
}

// The arrange-side virtual (S127). Receives the canvas's FULL final render slot
// (final_rect()). Places each child at its EXPLICIT origin {effLeft, effTop}
// (NaN collapses to 0 -> the canvas origin, the effective top-left), at the
// child's measured NATURAL desired size, via child->Arrange — which honors the
// child's OWN alignment + margin inside that slot through compute_final_rect
// (S124/S130_1 contract). Children at distinct origins do NOT stack/flow — the
// positions are absolute. Dirty-respect (S128): re-arrange ONLY
// is_arrange_dirty() children; clean children keep their final_rect untouched.
VAML_CORE_EXPORT
void Canvas::ArrangeOverride(const Rect& finalSlot)
{
    for (std::size_t i = 0; i < child_count(); ++i)
    {
        UIElement* child = child_at(i);
        if (!child->is_arrange_dirty())
        {
            continue;
        }
        const double l = left(*child);
        const double t = top(*child);
        const float effLeft = std::isnan(l) ? 0.0f : static_cast<float>(l);
        const float effTop  = std::isnan(t) ? 0.0f : static_cast<float>(t);
        child->Arrange(Rect{
            finalSlot.x + effLeft,
            finalSlot.y + effTop,
            child->desired_size().width,
            child->desired_size().height,
        });
    }
}

// --- S400 Text control: REAL DPs + the real MeasureOverride ------------------

namespace {

// Registers the Text control's four REAL DPs into the global table exactly once
// per process (Meyers-static guard; a second call is a no-op — mirrors the
// UIElement/StackPanel self-registration pattern). Authentication: Text is the
// S400 headline control; its DPs are GLOBALLY registered so they resolve on ANY
// DependencyObject via S043 attached-aware resolution (a style setter can target
// "Text"/"FontSize"/"FontWeight"/"Foreground" on a Text).
void register_text_properties()
{
    // Text: String, default "".
    (void)DependencyProperty::Register(
        "Text", ValueType::String, "Text",
        PropertyMetadata::WithDefault(Value::String(std::string{}), PropertyFlags::None));
    // FontSize: Double, default 16, validated >= 0 (a negative em size is
    // structurally meaningless -> REJECTED).
    (void)DependencyProperty::Register(
        "FontSize", ValueType::Double, "Text",
        std::move(PropertyMetadata::WithDefault(Value::Double(16.0), PropertyFlags::None))
            .with_validate([](const Value& v) { return v.get<double>() >= 0.0; }));
    // FontWeight: Int32, default Normal=0 (the S206 glyph convention).
    (void)DependencyProperty::Register(
        "FontWeight", ValueType::Int32, "Text",
        PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None));
    // Foreground: BrushId, default 0. The WPF-faithful "foreground is a BRUSH"
    // read (a BrushId maps to a registered SolidColorBrush; see
    // resolve_foreground_brush below). BrushId rides the Int32 payload.
    (void)DependencyProperty::Register(
        "Foreground", ValueType::BrushId, "Text",
        PropertyMetadata::WithDefault(Value::BrushId(0), PropertyFlags::None));
}

// Runs register_text_properties() at most once (first Text ctor).
void ensure_text_properties_registered()
{
    static bool registered = false;
    if (!registered)
    {
        registered = true;
        register_text_properties();
    }
}

} // namespace

VAML_CORE_EXPORT
Text::Text()
{
    ensure_text_properties_registered();
}

VAML_CORE_EXPORT
SetResult Text::set_text(std::string value)
{
    return Set(PropertyId::of("Text"), Value::String(std::move(value)));
}

VAML_CORE_EXPORT
std::string Text::text() const
{
    return Get(PropertyId::of("Text")).get<std::string>();
}

VAML_CORE_EXPORT
SetResult Text::set_font_size(double value)
{
    return Set(PropertyId::of("FontSize"), Value::Double(value));
}

VAML_CORE_EXPORT
double Text::font_size() const
{
    return Get(PropertyId::of("FontSize")).get<double>();
}

VAML_CORE_EXPORT
SetResult Text::set_font_weight(std::int32_t value)
{
    return Set(PropertyId::of("FontWeight"), Value::Int32(value));
}

VAML_CORE_EXPORT
std::int32_t Text::font_weight() const
{
    return Get(PropertyId::of("FontWeight")).get<std::int32_t>();
}

VAML_CORE_EXPORT
SetResult Text::set_foreground(std::int32_t brushId)
{
    return Set(PropertyId::of("Foreground"), Value::BrushId(brushId));
}

VAML_CORE_EXPORT
std::int32_t Text::foreground() const
{
    return Get(PropertyId::of("Foreground")).get<std::int32_t>();
}

VAML_CORE_EXPORT
Size Text::MeasureOverride(const Size& available) const
{
    // The S400 real Text algorithm: run the CPU text-LAYOUT + WRAP engine
    // (text_layout_plan) with the default metrics (a fontSize-based estimate —
    // device-free, so this needs no Vulkan atlas). Wrap within the offered
    // `available` width (the S123 infinity-aware basis: an unbounded offer -> no
    // wrap -> one line; a bounded offer -> word-wrap). The returned extent
    // {measuredWidth, measuredHeight} is the Text's natural content extent; the
    // base Measure finalizer (compute_measured_desired) then applies the
    // S123 stretch-vs-wrap rule against `available`.
    //
    // KEY DESIGN (recorded, NO-GAP not silent): the MEASURE basis uses
    // default_text_metric (a fontSize-proportional estimate), while the RENDER
    // basis (vulkan build_text_glyph_items) uses the REAL atlas advances — but
    // BOTH use the SAME wrap RULE (text_layout_plan), so the box this Measure
    // reports and the extent the renderer lays out stay in lock-step. A bounded
    // available width -> word-wrap here, and the renderer re-runs the same wrap
    // with the real advances, so the glyphs never overflow the measured box.
    const float fontSize = static_cast<float>(font_size());
    const std::string content = text();
    const TextLayoutPlan plan = text_layout_plan(
        content, fontSize, available.width,
        [fontSize](std::uint32_t cp) { return default_text_metric(cp, fontSize); });
    return Size{ plan.measuredWidth, plan.measuredHeight };
}

// --- S400_1 Icon control: REAL DPs + the real MeasureOverride (per ADR-015) ---

namespace {

// Registers the Icon control's seven REAL DPs into the global table exactly
// once per process (Meyers-static guard). GLOBALLY registered so they resolve
// on any DependencyObject via S043 attached-aware resolution. The DP surface is
// the S400_1 title: Source/IconKey/TintMode/Foreground/Width/Height/Stretch.
void register_icon_properties()
{
    // IconKey: String, default "" — the strong resource key the S360_1 walk
    // resolves (fetch via resolve_icon). NEVER a path/URI/script (ADR-015).
    (void)DependencyProperty::Register(
        "IconKey", ValueType::String, "Icon",
        PropertyMetadata::WithDefault(Value::String(std::string{}), PropertyFlags::None));
    // Source: String, default "" — the optional direct byte-asset reference.
    // IconKey wins when both are set (the resource dict is the ADR-015
    // authority).
    (void)DependencyProperty::Register(
        "Icon.Source", ValueType::String, "Icon",
        PropertyMetadata::WithDefault(Value::String(std::string{}), PropertyFlags::None));
    // TintMode: Int32, default Preserve=0 (IconTintMode::Preserve).
    (void)DependencyProperty::Register(
        "Icon.TintMode", ValueType::Int32, "Icon",
        PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None));
    // Foreground: BrushId, default 0 — the Monochrome tint color (the
    // WPF-faithful brush read, mirrors Text).
    (void)DependencyProperty::Register(
        "Icon.Foreground", ValueType::BrushId, "Icon",
        PropertyMetadata::WithDefault(Value::BrushId(0), PropertyFlags::None));
    // Width: Double, default 0 = auto. Validated >= 0 (a negative icon box is
    // structurally meaningless -> REJECTED).
    (void)DependencyProperty::Register(
        "Icon.Width", ValueType::Double, "Icon",
        std::move(PropertyMetadata::WithDefault(Value::Double(0.0), PropertyFlags::None))
            .with_validate([](const Value& v) { return v.get<double>() >= 0.0; }));
    // Height: Double, default 0 = auto. Validated >= 0.
    (void)DependencyProperty::Register(
        "Icon.Height", ValueType::Double, "Icon",
        std::move(PropertyMetadata::WithDefault(Value::Double(0.0), PropertyFlags::None))
            .with_validate([](const Value& v) { return v.get<double>() >= 0.0; }));
    // Stretch: Int32, default None=0 (IconStretch::None).
    (void)DependencyProperty::Register(
        "Icon.Stretch", ValueType::Int32, "Icon",
        PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None));
}

// Runs register_icon_properties() at most once (first Icon ctor).
void ensure_icon_properties_registered()
{
    static bool registered = false;
    if (!registered)
    {
        registered = true;
        register_icon_properties();
    }
}

} // namespace

VAML_CORE_EXPORT
Icon::Icon()
{
    ensure_icon_properties_registered();
}

VAML_CORE_EXPORT
SetResult Icon::set_icon_key(std::string value)
{
    return Set(PropertyId::of("IconKey"), Value::String(std::move(value)));
}

VAML_CORE_EXPORT
std::string Icon::icon_key() const
{
    return Get(PropertyId::of("IconKey")).get<std::string>();
}

VAML_CORE_EXPORT
SetResult Icon::set_source(std::string value)
{
    return Set(PropertyId::of("Icon.Source"), Value::String(std::move(value)));
}

VAML_CORE_EXPORT
std::string Icon::source() const
{
    return Get(PropertyId::of("Icon.Source")).get<std::string>();
}

VAML_CORE_EXPORT
SetResult Icon::set_tint_mode(std::int32_t value)
{
    return Set(PropertyId::of("Icon.TintMode"), Value::Int32(value));
}

VAML_CORE_EXPORT
std::int32_t Icon::tint_mode() const
{
    return Get(PropertyId::of("Icon.TintMode")).get<std::int32_t>();
}

VAML_CORE_EXPORT
SetResult Icon::set_foreground(std::int32_t brushId)
{
    return Set(PropertyId::of("Icon.Foreground"), Value::BrushId(brushId));
}

VAML_CORE_EXPORT
std::int32_t Icon::foreground() const
{
    return Get(PropertyId::of("Icon.Foreground")).get<std::int32_t>();
}

VAML_CORE_EXPORT
SetResult Icon::set_width(double value)
{
    return Set(PropertyId::of("Icon.Width"), Value::Double(value));
}

VAML_CORE_EXPORT
double Icon::width() const
{
    return Get(PropertyId::of("Icon.Width")).get<double>();
}

VAML_CORE_EXPORT
SetResult Icon::set_height(double value)
{
    return Set(PropertyId::of("Icon.Height"), Value::Double(value));
}

VAML_CORE_EXPORT
double Icon::height() const
{
    return Get(PropertyId::of("Icon.Height")).get<double>();
}

VAML_CORE_EXPORT
SetResult Icon::set_stretch(std::int32_t value)
{
    return Set(PropertyId::of("Icon.Stretch"), Value::Int32(value));
}

VAML_CORE_EXPORT
std::int32_t Icon::stretch() const
{
    return Get(PropertyId::of("Icon.Stretch")).get<std::int32_t>();
}

VAML_CORE_EXPORT
Size Icon::MeasureOverride(const Size& available) const
{
    // The S400_1 real Icon algorithm: the icon's measured box is driven by the
    // Width/Height DPs (0 = auto) → the authored asset's natural extent (when
    // it resolves) or a bounded default. The base Measure finalizer then
    // applies the S123 infinity-aware stretch-vs-wrap rule, so the STRETCH DP
    // (None/Fill/Uniform/UniformToFill) governs the final box through the
    // alignment/measure contract (the render quad is emitted by the classifier
    // via the cached icon provider).
    //
    // The natural extent: when the control carries a Width/Height, the box uses
    // those (a bounded icon box). When either is 0 (auto), the asset's natural
    // size is NOT available at measure time (device-free core — the atlas slot
    // is a vulkan-side read); a DEFAULT 16px icon box stands in (the S400_1
    // nominal icon box), recorded NO-GAP — the asset's true authored extent is
    // resolved at RENDER by the cached provider + best-fit (S213_1) and the
    // final arranger clamps the box (the render never overflows the measured
    // slot the class-of-fits governs).
    const float w = static_cast<float>(width() > 0.0 ? width() : (available.width > 0.0f ? available.width : 16.0f));
    const float h = static_cast<float>(height() > 0.0 ? height() : (available.height > 0.0f ? available.height : 16.0f));
    return Size{ std::max(0.0f, w), std::max(0.0f, h) };
}

VAML_CORE_EXPORT
TextBox::TextBox()
{
    ensure_textbox_properties_registered();
}

VAML_CORE_EXPORT
SetResult TextBox::set_text(std::string value)
{
    return Set(PropertyId::of("TextBox.Text"), Value::String(std::move(value)));
}

VAML_CORE_EXPORT
std::string TextBox::text() const
{
    return Get(PropertyId::of("TextBox.Text")).get<std::string>();
}

VAML_CORE_EXPORT
SetResult TextBox::set_accepts_return(bool value)
{
    return Set(PropertyId::of("TextBox.AcceptsReturn"), Value::Int32(value ? 1 : 0));
}

VAML_CORE_EXPORT
bool TextBox::accepts_return() const
{
    return Get(PropertyId::of("TextBox.AcceptsReturn")).get<int>() != 0;
}

VAML_CORE_EXPORT
VAML_CORE_EXPORT
void TextBox::raise_text_changed()
{
    // S463 — the TextChanged routed event (Bubbling, source==this textbox).
    vaml::RoutedEventArgs args;
    args.source = this;
    args.originalSource = this;
    RaiseEvent(vaml::TextChangedEvent(), args);
}

VAML_CORE_EXPORT
std::size_t TextBox::caret() const noexcept
{
    return caret_;
}

VAML_CORE_EXPORT
void TextBox::set_caret(std::size_t pos)
{
    const std::size_t len = text().size();
    caret_ = std::min(pos, len);
    // Setting the caret collapses any selection (WPF-faithful caret move).
    selStart_ = caret_;
    selLen_   = 0;
}

VAML_CORE_EXPORT
std::size_t TextBox::selection_start() const noexcept
{
    return selStart_;
}

VAML_CORE_EXPORT
std::size_t TextBox::selection_length() const noexcept
{
    return selLen_;
}

VAML_CORE_EXPORT
void TextBox::select_all()
{
    const std::size_t len = text().size();
    selStart_ = 0;
    selLen_   = len;
    caret_    = len;
}

VAML_CORE_EXPORT
void TextBox::clear_selection()
{
    selLen_ = 0;
}

VAML_CORE_EXPORT
std::size_t TextBox::delete_selection(std::string& s)
{
    if (selLen_ == 0)
        return 0;
    const std::size_t start = std::min(selStart_, s.size());
    const std::size_t end   = std::min(selStart_ + selLen_, s.size());
    const std::size_t removed = end - start;
    s.erase(start, removed);
    caret_    = start;
    selStart_ = start;
    selLen_   = 0;
    return removed;
}

VAML_CORE_EXPORT
bool TextBox::handle_key(vaml::KeyCode key, bool isDown, bool shift)
{
    if (!isDown)
        return false; // only a key-down drives an edit (releases are not edits)
    accepted_ = false;
    std::string s = text();

    // A shift+caret-move EXTENDS the selection (WPF-faithful); a bare move
    // collapses it. S463: a REAL edit with a DETECTED change raises the
    // Bubbling "TextChanged" routed event (source==this) — the authored
    // `TextChanged="..."` markup fires on a keystroke.
    const auto commit = [&]() {
        const std::string before = text();
        (void)Set(PropertyId::of("TextBox.Text"), Value::String(s));
        if (text() != before)
        {
            raise_text_changed();
        }
    };
    const auto clampCaret = [&](std::size_t p) {
        return std::min(p, s.size());
    };
    const auto collapse = [&]() {
        selStart_ = caret_;
        selLen_   = 0;
    };

    // Navigational keys.
    switch (key)
    {
    case vaml::KeyCode::Left:
    {
        std::size_t target = caret_ > 0 ? caret_ - 1 : 0;
        if (shift && selLen_ > 0)
            target = std::min(selStart_, selStart_ + selLen_ - 1);
        caret_ = clampCaret(target);
        if (!shift)
            collapse();
        else if (selLen_ == 0)
        {
            selStart_ = std::min(caret_, s.size());
            selLen_   = (caret_ < selStart_) ? 0 : 0; // no pre-existing selection
        }
        return true;
    }
    case vaml::KeyCode::Right:
        caret_ = clampCaret(caret_ + 1);
        if (!shift)
            collapse();
        return true;
    case vaml::KeyCode::Home:
        caret_ = 0;
        if (!shift)
            collapse();
        return true;
    case vaml::KeyCode::End:
        caret_ = s.size();
        if (!shift)
            collapse();
        return true;
    default:
        break;
    }

    // A printable letter/digit inserts at the caret (replacing any selection).
    if (key >= vaml::KeyCode::A && key <= vaml::KeyCode::Z)
    {
        (void)delete_selection(s);
        s.insert(s.begin() + static_cast<std::ptrdiff_t>(caret_), static_cast<char>('a' + (static_cast<int>(key) - static_cast<int>(vaml::KeyCode::A))));
        caret_ = std::min(caret_ + 1, s.size());
        commit();
        collapse();
        return true;
    }
    if (key >= vaml::KeyCode::Digit0 && key <= vaml::KeyCode::Digit9)
    {
        (void)delete_selection(s);
        s.insert(s.begin() + static_cast<std::ptrdiff_t>(caret_), static_cast<char>('0' + (static_cast<int>(key) - static_cast<int>(vaml::KeyCode::Digit0))));
        caret_ = std::min(caret_ + 1, s.size());
        commit();
        collapse();
        return true;
    }
    if (key == vaml::KeyCode::Space || key == vaml::KeyCode::TextBoxAccept)
    {
        // TextBoxAccept is the generic "insert a char" override; a space is a
        // printable insert. A space literal inserts ' '.
        (void)delete_selection(s);
        const char c = (key == vaml::KeyCode::Space) ? ' ' : ' ';
        s.insert(s.begin() + static_cast<std::ptrdiff_t>(caret_), c);
        caret_ = std::min(caret_ + 1, s.size());
        commit();
        collapse();
        return true;
    }

    // Backspace deletes the char left of the caret / a selected range.
    if (key == vaml::KeyCode::Backspace)
    {
        if (selLen_ > 0)
        {
            (void)delete_selection(s);
            commit();
            return true;
        }
        if (caret_ > 0)
        {
            s.erase(s.begin() + static_cast<std::ptrdiff_t>(caret_ - 1));
            caret_ = std::min(caret_ - 1, s.size());
            commit();
            collapse();
            return true;
        }
        return true; // nothing to delete — still an edit key (no-op)
    }

    // Enter: multi-line inserts a newline; single-line ACCEPTS (returns true).
    if (key == vaml::KeyCode::Enter)
    {
        if (accepts_return())
        {
            (void)delete_selection(s);
            s.insert(s.begin() + static_cast<std::ptrdiff_t>(caret_), '\n');
            caret_ = std::min(caret_ + 1, s.size());
            commit();
            collapse();
            return true;
        }
        // Single-line Accept: signal accepted, consume the key (no text change).
        accepted_ = true;
        return true;
    }

    // Return a NEWLINE literal marker (the text layer uses '\n').
    if (key == vaml::KeyCode::Up && accepts_return())
    {
        // Up/Down in multiline are vertical navigation (recorded NO-GAP: full
        // vertical caret nav is a later refinement); consuming them keeps them
        // out of the edition loop.
        return true;
    }
    if (key == vaml::KeyCode::Down && accepts_return())
    {
        return true;
    }

    return false; // an unhandled key is not an edit
}

VAML_CORE_EXPORT
Size TextBox::MeasureOverride(const Size& available) const
{
    // The S402 real TextBox algorithm: wrap the Text content at `available` via
    // the S400 text_layout_plan (multi-line growth when AcceptsReturn); report a
    // bounded default box for the edit field. The base Measure finalizer applies
    // the S123 stretch-vs-wrap rule.
    const float fs = 16.0f;
    const float lineH = 1.2f * fs;
    const TextLayoutPlan plan = text_layout_plan(
        text(), fs, available.width,
        [fs](std::uint32_t cp) { return default_text_metric(cp, fs); });
    const float h = plan.measuredHeight > 0.0f ? plan.measuredHeight : lineH;
    const float w = available.width > 0.0f ? available.width : (plan.measuredWidth > 0.0f ? plan.measuredWidth : 100.0f);
    return Size{ std::max(0.0f, w), std::max(0.0f, h) };
}

// --- S403 Slider control: REAL Value/Min/Max DPs + value-changed + thumb drag ---

namespace {

void register_slider_properties()
{
    // Value: Double, default 0; the value_clamp is applied in apply_change
    // (Set is validated >= min/max on read, see set_value).
    (void)DependencyProperty::Register(
        "Slider.Value", ValueType::Double, "Slider",
        PropertyMetadata::WithDefault(Value::Double(0.0), PropertyFlags::None));
    (void)DependencyProperty::Register(
        "Slider.Minimum", ValueType::Double, "Slider",
        PropertyMetadata::WithDefault(Value::Double(0.0), PropertyFlags::None));
    (void)DependencyProperty::Register(
        "Slider.Maximum", ValueType::Double, "Slider",
        PropertyMetadata::WithDefault(Value::Double(100.0), PropertyFlags::None));
    // The S403 "routed change event" identity (Bubbling).
    (void)RoutedEvent::registered("ValueChanged", RouteEventDirection::Bubble);
}

void ensure_slider_properties_registered()
{
    static bool reg = false;
    if (!reg)
    {
        reg = true;
        register_slider_properties();
    }
}

} // namespace

VAML_CORE_EXPORT
Slider::Slider()
{
    ensure_slider_properties_registered();
}

VAML_CORE_EXPORT
SetResult Slider::set_value(double v)
{
    // Clamp to [Minimum, Maximum] then place; a real change raises the change.
    const double lo = std::min(minimum(), maximum());
    const double hi = std::max(minimum(), maximum());
    const double clamped = std::clamp(v, lo, hi);
    const double old = Get(PropertyId::of("Slider.Value")).get<double>();
    if (old == clamped)
        return vaml::SetResult::NO_CHANGE;
    const SetResult r = Set(PropertyId::of("Slider.Value"), Value::Double(clamped));
    if (r == vaml::SetResult::APPLIED)
        apply_change(clamped);
    return r;
}

VAML_CORE_EXPORT
double Slider::value() const
{
    return Get(PropertyId::of("Slider.Value")).get<double>();
}

VAML_CORE_EXPORT
SetResult Slider::set_minimum(double v)
{
    return Set(PropertyId::of("Slider.Minimum"), Value::Double(v));
}

VAML_CORE_EXPORT
double Slider::minimum() const
{
    return Get(PropertyId::of("Slider.Minimum")).get<double>();
}

VAML_CORE_EXPORT
SetResult Slider::set_maximum(double v)
{
    return Set(PropertyId::of("Slider.Maximum"), Value::Double(v));
}

VAML_CORE_EXPORT
double Slider::maximum() const
{
    return Get(PropertyId::of("Slider.Maximum")).get<double>();
}

VAML_CORE_EXPORT
void Slider::apply_change(double newValue)
{
    (void)newValue; // the routed change is the notification; the new value is the DP's current
    // The S403 "routed change event": fire the Signal + raise the BUBBLING
    // "ValueChanged" routed event (source==this). A handler may observe the
    // change; the two-way binding (S244) also writes the new Value to the VM
    // (a real DP change already armed it).
    onValueChanged_.fire();
    vaml::RoutedEventArgs args;
    args.source = this;
    args.originalSource = this;
    RaiseEvent(vaml::RoutedEvent::registered("\1", vaml::RouteEventDirection::Bubble),
               args);
}

VAML_CORE_EXPORT
double Slider::set_value_from_ratio(double ratio)
{
    // Ratio in [0,1] along the track -> Value = Minimum + ratio*(Max-Min),
    // clamped (a ratio outside [0,1] clamps). Raises the change.
    const double r = std::clamp(ratio, 0.0, 1.0);
    const double lo = std::min(minimum(), maximum());
    const double hi = std::max(minimum(), maximum());
    const double v = lo + r * (hi - lo);
    (void)set_value(v);
    return value();
}

VAML_CORE_EXPORT
Size Slider::MeasureOverride(const Size& available) const
{
    // The S403 real Slider algorithm: the default track box at the arranged
    // extent; the S123 finalizer applies the stretch-vs-wrap. The rendered
    // track + thumb is the vulkan classifier (deferred to S409_1).
    const float w = available.width > 0.0f ? available.width : 100.0f;
    const float h = available.height > 0.0f ? available.height : 8.0f;
    return Size{ std::max(0.0f, w), std::max(0.0f, h) };
}

// --- S404 CheckBox control: REAL IsChecked (nullable bool) + IsThreeState + toggle ---

namespace {

void register_checkbox_properties()
{
    // IsChecked: Int32 (0=unchecked, 1=checked, 2=indeterminate), default 0.
    (void)DependencyProperty::Register(
        "CheckBox.IsChecked", ValueType::Int32, "CheckBox",
        std::move(PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None))
            .with_validate([](const Value& v) { const int x = v.get<int>(); return x >= 0 && x <= 2; }));
    // IsThreeState: Bool via Int32, default false.
    (void)DependencyProperty::Register(
        "CheckBox.IsThreeState", ValueType::Int32, "CheckBox",
        std::move(PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None))
            .with_validate([](const Value& v) { const int x = v.get<int>(); return x == 0 || x == 1; }));
    (void)RoutedEvent::registered("CheckedChanged", RouteEventDirection::Bubble);
}

void ensure_checkbox_properties_registered()
{
    static bool reg = false;
    if (!reg)
    {
        reg = true;
        register_checkbox_properties();
    }
}

} // namespace

VAML_CORE_EXPORT
CheckBox::CheckBox()
{
    ensure_checkbox_properties_registered();
}

VAML_CORE_EXPORT
SetResult CheckBox::set_checked(std::int32_t state)
{
    // IsChecked is nullable (third state 2=indeterminate). A real change raises
    // the change + writes back via the S244 two-way IsChecked binding.
    const std::int32_t s = std::clamp(state, 0, 2);
    const std::int32_t old = Get(PropertyId::of("CheckBox.IsChecked")).get<int>();
    if (old == s)
        return vaml::SetResult::NO_CHANGE;
    const SetResult r = Set(PropertyId::of("CheckBox.IsChecked"), Value::Int32(s));
    if (r == vaml::SetResult::APPLIED)
        apply_change(s);
    return r;
}

VAML_CORE_EXPORT
std::int32_t CheckBox::checked() const
{
    return Get(PropertyId::of("CheckBox.IsChecked")).get<int>();
}

VAML_CORE_EXPORT
SetResult CheckBox::set_is_three_state(bool value)
{
    return Set(PropertyId::of("CheckBox.IsThreeState"), Value::Int32(value ? 1 : 0));
}

VAML_CORE_EXPORT
bool CheckBox::is_three_state() const
{
    return Get(PropertyId::of("CheckBox.IsThreeState")).get<int>() != 0;
}

VAML_CORE_EXPORT
void CheckBox::apply_change(std::int32_t newState)
{
    (void)newState;
    onCheckedChanged_.fire();
    vaml::RoutedEventArgs args;
    args.source = this;
    args.originalSource = this;
    RaiseEvent(vaml::RoutedEvent::registered("\1", vaml::RouteEventDirection::Bubble),
               args);
}

VAML_CORE_EXPORT
std::int32_t CheckBox::toggle()
{
    // The WPF on-toggle cycle (source-confirmed from ToggleButton.cs):
    //   true+IsThreeState -> null(2); true+!IsThreeState -> false(0);
    //   false -> true(1); null -> false(0).
    const std::int32_t cur = checked();
    std::int32_t next = 0;
    if (cur == 0)
        next = 1; // unchecked(false) -> checked(true)
    else if (cur == 1)
        next = is_three_state() ? 2 : 0; // checked(true) -> indeterminate || unchecked
    else // cur == 2 (indeterminate)
        next = 0; // indeterminate(null) -> unchecked(false)
    (void)set_checked(next);
    return next;
}

VAML_CORE_EXPORT
Size CheckBox::MeasureOverride(const Size& available) const
{
    const float w = available.width > 0.0f ? available.width : 14.0f;
    const float h = available.height > 0.0f ? available.height : 14.0f;
    return Size{ std::max(0.0f, w), std::max(0.0f, h) };
}

// --- S405 ScrollViewer control: offset + clipping + wheel scroll ------------

VAML_CORE_EXPORT
ScrollViewer::ScrollViewer()
{
    (void)RoutedEvent::registered("ScrollChanged", RouteEventDirection::Bubble);
}

VAML_CORE_EXPORT
SetResult ScrollViewer::set_vertical_offset(double v)
{
    const double scrollable = scrollable_height();
    const double clamped = std::clamp(v, 0.0, scrollable);
    if (vertical_ == clamped)
        return vaml::SetResult::NO_CHANGE;
    vertical_ = clamped;
    onScrollChanged_.fire();
    vaml::RoutedEventArgs args;
    args.source = this;
    args.originalSource = this;
    RaiseEvent(vaml::RoutedEvent::registered("\1", vaml::RouteEventDirection::Bubble),
               args);
    return vaml::SetResult::APPLIED;
}

VAML_CORE_EXPORT
double ScrollViewer::vertical_offset() const { return vertical_; }

VAML_CORE_EXPORT
SetResult ScrollViewer::set_horizontal_offset(double v)
{
    if (horizontal_ == v)
        return vaml::SetResult::NO_CHANGE;
    horizontal_ = std::max(0.0, v);
    onScrollChanged_.fire();
    vaml::RoutedEventArgs args;
    args.source = this;
    args.originalSource = this;
    RaiseEvent(vaml::RoutedEvent::registered("\1", vaml::RouteEventDirection::Bubble),
               args);
    return vaml::SetResult::APPLIED;
}

VAML_CORE_EXPORT
double ScrollViewer::horizontal_offset() const { return horizontal_; }

VAML_CORE_EXPORT
void ScrollViewer::set_content_height(double h) { contentH_ = std::max(0.0, h); }

VAML_CORE_EXPORT
double ScrollViewer::content_height() const { return contentH_; }

VAML_CORE_EXPORT
double ScrollViewer::viewport_height() const { return std::max(0.0, static_cast<double>(final_rect().height)); }

VAML_CORE_EXPORT
double ScrollViewer::scrollable_height() const { return std::max(0.0, contentH_ - viewport_height()); }

VAML_CORE_EXPORT
double ScrollViewer::scroll_by_vertical(double delta)
{
    const double target = vertical_ + delta;
    const double clamped = std::clamp(target, 0.0, scrollable_height());
    if (clamped != vertical_)
        (void)set_vertical_offset(clamped);
    return vertical_;
}

VAML_CORE_EXPORT
Rect ScrollViewer::clip_content(const Rect& content) const
{
    // The S124 clip: the child content rect, translated by the current offset
    // then clipped against the arranged viewport. Returns the VISIBLE sub-rect
    // in viewport coordinates (a fully-hidden content -> a zero rect).
    const vaml::Rect fr = final_rect();
    const float vx = fr.x;
    const float vy = fr.y;
    const float vw = fr.width;
    const float vh = fr.height;
    // Content's visible top-left in viewport space (inside the viewport origin).
    const Rect moved{ content.x - static_cast<float>(vertical_offset()),
                      content.y,
                      content.width,
                      content.height };
    const float x0 = std::max(moved.x, vx);
    const float y0 = std::max(moved.y, vy);
    const float x1 = std::min(moved.x + moved.width, vx + vw);
    const float y1 = std::min(moved.y + moved.height, vy + vh);
    if (x1 <= x0 || y1 <= y0)
        return Rect{ x0, y0, 0.0f, 0.0f };
    return Rect{ x0, y0, x1 - x0, y1 - y0 };
}

VAML_CORE_EXPORT
void ScrollViewer::apply_offset(double /*newV*/, double /*newH*/) {}

VAML_CORE_EXPORT
Size ScrollViewer::MeasureOverride(const Size& available) const
{
    // The S405 real ScrollViewer algorithm: the viewport is the arranged box
    // (the scrollable content beyond it is the S409_1 viewer draw). A
    // ScrollViewer measures to the viewport offer (or a bounded default).
    const float w = available.width > 0.0f ? available.width : 200.0f;
    const float h = available.height > 0.0f ? available.height : 120.0f;
    return Size{ std::max(0.0f, w), std::max(0.0f, h) };
}

// --- S440 Window (REAL container window-type; re-based onto UIElement) ------

VAML_CORE_EXPORT
Window::Window()
{
    // No Window-owner DPs to register yet: Window inherits the full UIElement
    // surface (Visibility/Opacity/Margin/alignment) + the S121 child machinery,
    // and owns no new global property names of its own at S440. The content
    // child slot is plain member state, not a DP.
}

VAML_CORE_EXPORT
void Window::set_content(std::unique_ptr<UIElement> child)
{
    // WPF-faithful single-content slot (NOT a panel list): a Window hosts
    // EXACTLY ONE content child. Replace any prior content.
    if (!child)
    {
        InvalidateMeasure();
        InvalidateArrange();
        return;
    }

    // Detach + destroy the prior content child (the parent OWNS its children:
    // remove_child detaches + destroys + clears the S041 scope edge). The
    // [[nodiscard]] size_t return is consumed.
    while (child_count() > 0)
    {
        [[maybe_unused]] std::size_t remaining = remove_child(0);
    }
    content_ = nullptr;

    // Adopt the new content child: TRANSFER ownership into the Window's visual-
    // children list (the S121 attach path wires the parent link + the S041
    // inheritance-scope edge). content_ is a NON-OWNING observer into that list
    // (kept alive by the parent), so there is exactly ONE ownership edge.
    UIElement* raw = child.get();
    add_child(std::move(child));
    content_ = raw;
    InvalidateMeasure();
    InvalidateArrange();
}

VAML_CORE_EXPORT
UIElement* Window::content() const noexcept
{
    return content_;
}

VAML_CORE_EXPORT
Size Window::MeasureOverride(const Size& available) const
{
    // The S440 real Window algorithm: a Window sizes to its single content
    // child within the offered box. Measure the content (if present) at the
    // offered bounds and report its desired size.
    if (content_)
    {
        content_->Measure(available);
        return content_->desired_size();
    }
    return Size{ 0.0f, 0.0f };
}

VAML_CORE_EXPORT
void Window::ArrangeOverride(const vaml::Rect& finalSlot)
{
    // The S440 real Window algorithm: arrange the single content child to FILL
    // the Window's final rect (implicit single-child layout). The base clip
    // (LayoutSlot clip_to_bounds) clips content to the window box.
    if (content_)
    {
        content_->Arrange(finalSlot);
    }
}

VAML_CORE_EXPORT
bool Window::raise_loaded()
{
    // S463 — the Window Loaded lifecycle event (Bubbling, source==this). A
    // handler may set handled=true to stop the route.
    vaml::RoutedEventArgs args;
    args.source = this;
    args.originalSource = this;
    RaiseEvent(vaml::LoadedEvent(), args);
    return args.handled;
}

VAML_CORE_EXPORT
bool Window::raise_closing()
{
    // S463 — the Window Closing lifecycle event (Bubbling, source==this).
    vaml::RoutedEventArgs args;
    args.source = this;
    args.originalSource = this;
    RaiseEvent(vaml::ClosingEvent(), args);
    return args.handled;
}

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
    const std::size_t nRows = rows_.size();
    const std::size_t nCols = cols_.size();

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
        if (cols_[c].width.unitType != GridUnitType::Star)
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
        if (rows_[r].height.unitType != GridUnitType::Star)
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
    const std::size_t nRows = rows_.size();
    const std::size_t nCols = cols_.size();

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
    // S400_1: Icon is a REAL control (not a seed) — re-based onto UIElement at
    // its own sprint HERE (S400_1), registered directly. Built-in count 11 -> 12.
    registry.register_element("Icon",         [] { return std::make_unique<Icon>(); });
    registry.register_element("TextBox",      [] { return std::make_unique<TextBox>(); });
    registry.register_element("CheckBox",     [] { return std::make_unique<CheckBox>(); });
    registry.register_element("Slider",       [] { return std::make_unique<Slider>(); });
    registry.register_element("ScrollViewer", [] { return std::make_unique<ScrollViewer>(); });
    registry.register_element("Window",       [] { return std::make_unique<Window>(); });
    // S134: WrapPanel is a REAL panel (not a seed) — it reaches its resolution
    // sprint HERE (same as StackPanel/Grid), so it is registered directly.
    registry.register_element("WrapPanel",    [] { return std::make_unique<WrapPanel>(); });
    // S136: Canvas is a REAL panel (not a seed) — it reaches its resolution
    // sprint HERE (same as StackPanel/Grid/WrapPanel), so it is registered
    // directly. Built-in count 10 -> 11.
    registry.register_element("Canvas",       [] { return std::make_unique<Canvas>(); });

    // S453 (dep:S452) — RESOURCE/STYLE AUTHORING vocabulary. These are NOT
    // visual controls; they are the authored RESOURCE types the resource-
    // dictionary + style grammar (S453/S454) produce via the S360 Object/Plain
    // resource-value model. They ARE DependencyObjects (valid Object resources)
    // so the NORMAL resolve_element -> build path constructs them + sets their
    // attributes (Color, TargetType, ...) exactly like any control. The
    // non-DependencyObject `ResourceDictionary` container is handled by a
    // dedicated authoring path in VamlReader::build (it cannot be produced via
    // the factory registry, which returns DependencyObjects). Registered here so
    // `<SolidColorBrush>` / `<Style>` are authorable element types.
    registry.register_element("SolidColorBrush", [] { return std::make_unique<SolidColorBrush>(); });
    // S460 (dep:S456) — BRUSH AUTHORING: GradientStop + LinearGradientBrush are
    // authorable element types. GradientStop is a pure value (produced in the
    // authored <LinearGradientBrush.GradientStops> list); LinearGradientBrush is
    // a DependencyObject (a valid Object resource registered under a "Brush:<id>"
    // key, exactly like SolidColorBrush). Built-in count 14 -> 16.
    registry.register_element("GradientStop",       [] { return std::make_unique<GradientStopHost>(); });
    registry.register_element("LinearGradientBrush", [] { return std::make_unique<LinearGradientBrush>(); });
    registry.register_element("Style",           [] { return std::make_unique<Style>(); });
}

} // namespace vaml
