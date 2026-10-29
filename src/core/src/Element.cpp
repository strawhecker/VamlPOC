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
#include "vaml/core/ResourceDictionary.h" // 11j-2 — ResourceDictionaryHolder owns a ResourceDictionary
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

// Registers the DockPanel attached Dock DP (S470) into the global table exactly
// once per process (Meyers-static guard). The Dock edge is Int32 (enum Dock {0..4}),
// owner "DockPanel", DEFAULT Fill (4) — WPF-faithful (a child with no explicit
// dock is the last-child fill). GLOBALLY registered under owner "DockPanel" so it
// resolves on ANY DependencyObject via the S043 attached-aware path (a child's
// dock is stored on the child).
void register_dockpanel_properties()
{
    (void)DependencyProperty::Register(
        "DockPanel.Dock", ValueType::Int32, "DockPanel",
        std::move(PropertyMetadata::WithDefault(Value::Int32(4), PropertyFlags::None))
            .with_validate([](const Value& v) { const int x = v.get<int>(); return x >= 0 && x <= 4; }));
}

// Runs register_dockpanel_properties() at most once (first DockPanel ctor).
void ensure_dockpanel_properties_registered()
{
    static bool registered = false;
    if (!registered)
    {
        registered = true;
        register_dockpanel_properties();
    }
}

// Registers the RadioButton IsChecked DP + Checked/Unchecked events (S471).
void register_radiobutton_properties()
{
    (void)DependencyProperty::Register(
        "RadioButton.IsChecked", ValueType::Int32, "RadioButton",
        std::move(PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None))
            .with_validate([](const Value& v) { const int x = v.get<int>(); return x == 0 || x == 1; }));
    (void)RoutedEvent::registered("Checked", RouteEventDirection::Bubble);
    (void)RoutedEvent::registered("Unchecked", RouteEventDirection::Bubble);
}

void ensure_radiobutton_properties_registered()
{
    static bool registered = false;
    if (!registered)
    {
        registered = true;
        register_radiobutton_properties();
    }
}

// Registers the ComboBox DPs (S472) + the SelectionChanged event.
void register_combobox_properties()
{
    (void)DependencyProperty::Register(
        "ComboBox.Items", ValueType::String, "ComboBox",
        PropertyMetadata::WithDefault(Value::String(std::string{}), PropertyFlags::None));
    (void)DependencyProperty::Register(
        "ComboBox.ItemsSource", ValueType::String, "ComboBox",
        PropertyMetadata::WithDefault(Value::String(std::string{}), PropertyFlags::None));
    (void)DependencyProperty::Register(
        "ComboBox.SelectedIndex", ValueType::Int32, "ComboBox",
        PropertyMetadata::WithDefault(Value::Int32(-1), PropertyFlags::None));
    (void)DependencyProperty::Register(
        "ComboBox.SelectedItem", ValueType::String, "ComboBox",
        PropertyMetadata::WithDefault(Value::String(std::string{}), PropertyFlags::None));
    (void)DependencyProperty::Register(
        "ComboBox.IsOpen", ValueType::Int32, "ComboBox",
        std::move(PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None))
            .with_validate([](const Value& v) { const int x = v.get<int>(); return x == 0 || x == 1; }));
    (void)RoutedEvent::registered("SelectionChanged", RouteEventDirection::Bubble);
}

void ensure_combobox_properties_registered()
{
    static bool registered = false;
    if (!registered)
    {
        registered = true;
        register_combobox_properties();
    }
}

// Registers the ListBox DPs (S473) + the SelectionChanged event.
void register_listbox_properties()
{
    (void)DependencyProperty::Register(
        "ListBox.Items", ValueType::String, "ListBox",
        PropertyMetadata::WithDefault(Value::String(std::string{}), PropertyFlags::None));
    (void)DependencyProperty::Register(
        "ListBox.ItemsSource", ValueType::String, "ListBox",
        PropertyMetadata::WithDefault(Value::String(std::string{}), PropertyFlags::None));
    (void)DependencyProperty::Register(
        "ListBox.SelectedIndex", ValueType::Int32, "ListBox",
        PropertyMetadata::WithDefault(Value::Int32(-1), PropertyFlags::None));
    (void)DependencyProperty::Register(
        "ListBox.SelectedItem", ValueType::String, "ListBox",
        PropertyMetadata::WithDefault(Value::String(std::string{}), PropertyFlags::None));
    // The S452 property-element grammar resolves `<ListBox.ItemTemplate>` /
    // `<ListView.ItemTemplate>` against the GLOBAL name (VamlReader::TryGet uses
    // the split `propName`, i.e. "ItemTemplate" — the bare name) and routes the
    // authored template into the S364 engine. Register it as a bare global DP so
    // the authored property-element resolves (S473). UNTYPEDPTR-bound: the
    // authored `<Owner.ItemTemplate>` object child is a retained UIElement the
    // reader stores as Value::UntypedPtr; render_items() instantiates per-item
    // content FROM that authored template (the "items render authored content"
    // headline — the S364 instantiate seam).
    (void)DependencyProperty::Register(
        "ItemTemplate", ValueType::UntypedPtr, "ListBox",
        PropertyMetadata::WithDefault(Value::UntypedPtr(nullptr), PropertyFlags::None));
    (void)RoutedEvent::registered("SelectionChanged", RouteEventDirection::Bubble);
}

void ensure_listbox_properties_registered()
{
    static bool registered = false;
    if (!registered)
    {
        registered = true;
        register_listbox_properties();
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

// 11j-5 (GAP CLOSE, dep:S400/S480) — the SHARED CONTROL BASE DP-BUNDLE.
//
// The VAML property registry is NAME-UNIQUE globally (DependencyProperty::
// Register throws on a duplicate NAME; the owner type id is a declaration LABEL
// only — the value is resolved S043-attached-aware by bare name on ANY
// DependencyObject). WPF resolves `Padding` / `HorizontalContentAlignment` /
// `VerticalContentAlignment` / `FontSize` / `FontFamily` on a Control/TextElement
// BASE by type-inheritance; the faithful VAML mapping is ONE canonical bare
// registration (owner "Control") + the affected controls' typed accessors
// re-pointed to that single owner. Without this, corpus rows authoring these
// UNPREFIXED on Button/TextBox/TextBlock/… would fail UnknownProperty, and the
// prior split (Text owning bare "FontSize" vs Label owning "Label.FontSize")
// was an inconsistency that left the unprefixed surface broken for the corpus.
//
// This step MIGRATES the conflicting prior registrations (NO backfill — done
// HERE, at this step):
//   - Text   owned bare "FontSize"          -> REMOVED, re-point to this owner
//   - Label  owned "Label.FontSize",
//            "Label.FontFamily",
//            "Label.HorizontalContentAlignment" -> REMOVED, re-point to this owner
//   - Border owned "Border.Padding"         -> REMOVED, re-point to this owner
// Each typed accessor's backing name changes (the public surface path is
// unchanged); duplicate Register would throw PropertyAlreadyRegistered, so the
// deliberate re-pointing is the one-time migration + CI catches regressions.
void register_control_base_properties()
{
    // Padding: Thickness, default 0. Owner "Control".
    (void)DependencyProperty::Register(
        "Padding", ValueType::Thickness, "Control",
        PropertyMetadata::WithDefault(Value::Thickness(vaml::Thickness{}), PropertyFlags::None));
    // HorizontalContentAlignment: Int32, default Left=0 (the S122 alignment).
    (void)DependencyProperty::Register(
        "HorizontalContentAlignment", ValueType::Int32, "Control",
        PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None));
    // VerticalContentAlignment: Int32, default Top=0 (the S122 alignment).
    (void)DependencyProperty::Register(
        "VerticalContentAlignment", ValueType::Int32, "Control",
        PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None));
    // FontSize: Double, default 16, validated >= 0 (a negative em size is
    // structurally meaningless -> REJECTED). Owner "Control".
    (void)DependencyProperty::Register(
        "FontSize", ValueType::Double, "Control",
        std::move(PropertyMetadata::WithDefault(Value::Double(16.0), PropertyFlags::None))
            .with_validate([](const Value& v) { return v.get<double>() >= 0.0; }));
    // FontFamily: String, default "".
    (void)DependencyProperty::Register(
        "FontFamily", ValueType::String, "Control",
        PropertyMetadata::WithDefault(Value::String(std::string{}), PropertyFlags::None));
}

void ensure_control_base_properties_registered()
{
    static bool registered = false;
    if (!registered)
    {
        registered = true;
        register_control_base_properties();
    }
}

// Registers the TextBlock control's REAL DPs into the global table exactly once
// per process (Meyers-static guard; a second call is a no-op — mirrors the
// UIElement/StackPanel self-registration pattern). Authentication: TextBlock is
// the S400 headline control (11k-1 re-binds the S087-invented `Text` name onto
// the WPF-true TextBlock); its DPs are GLOBALLY registered so they resolve on
// ANY DependencyObject via S043 attached-aware resolution (a style setter can
// target "Text"/"FontSize"/"FontWeight"/"Foreground" on a TextBlock).
//
// 11j-5: `FontSize` is NO LONGER registered here — it is the shared Control
// base DP (owner "Control", see register_control_base_properties). TextBlock's
// FontSize accessors re-point to that single owner (bare "FontSize"). The
// distinct TextBlock-owned names Text / FontWeight / Foreground remain here,
// re-owned under "TextBlock" (11k-1).
void register_textblock_properties()
{
    // Text: String, default "".
    (void)DependencyProperty::Register(
        "Text", ValueType::String, "TextBlock",
        PropertyMetadata::WithDefault(Value::String(std::string{}), PropertyFlags::None));
    // FontWeight: Int32, default Normal=0 (the S206 glyph convention).
    (void)DependencyProperty::Register(
        "FontWeight", ValueType::Int32, "TextBlock",
        PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None));
    // Foreground: BrushId, default 0. The WPF-faithful "foreground is a BRUSH"
    // read (a BrushId maps to a registered SolidColorBrush; see
    // resolve_foreground_brush below). BrushId rides the Int32 payload.
    (void)DependencyProperty::Register(
        "Foreground", ValueType::BrushId, "TextBlock",
        PropertyMetadata::WithDefault(Value::BrushId(0), PropertyFlags::None));
}

// Runs register_textblock_properties() at most once (first TextBlock ctor).
void ensure_textblock_properties_registered()
{
    // 11j-5: the shared Control base DP bundle is a dependency of the TextBlock
    // surface (FontSize re-points to it) — ensure it before TextBlock's own.
    ensure_control_base_properties_registered();
    static bool registered = false;
    if (!registered)
    {
        registered = true;
        register_textblock_properties();
    }
}

} // namespace

VAML_CORE_EXPORT
TextBlock::TextBlock()
{
    ensure_textblock_properties_registered();
}

VAML_CORE_EXPORT
SetResult TextBlock::set_text(std::string value)
{
    return Set(PropertyId::of("Text"), Value::String(std::move(value)));
}

VAML_CORE_EXPORT
std::string TextBlock::text() const
{
    return Get(PropertyId::of("Text")).get<std::string>();
}

VAML_CORE_EXPORT
SetResult TextBlock::set_font_size(double value)
{
    return Set(PropertyId::of("FontSize"), Value::Double(value));
}

VAML_CORE_EXPORT
double TextBlock::font_size() const
{
    return Get(PropertyId::of("FontSize")).get<double>();
}

VAML_CORE_EXPORT
SetResult TextBlock::set_font_weight(std::int32_t value)
{
    return Set(PropertyId::of("FontWeight"), Value::Int32(value));
}

VAML_CORE_EXPORT
std::int32_t TextBlock::font_weight() const
{
    return Get(PropertyId::of("FontWeight")).get<std::int32_t>();
}

VAML_CORE_EXPORT
SetResult TextBlock::set_foreground(std::int32_t brushId)
{
    return Set(PropertyId::of("Foreground"), Value::BrushId(brushId));
}

VAML_CORE_EXPORT
std::int32_t TextBlock::foreground() const
{
    return Get(PropertyId::of("Foreground")).get<std::int32_t>();
}

VAML_CORE_EXPORT
Size TextBlock::MeasureOverride(const Size& available) const
{
    // The S400 real TextBlock algorithm: run the CPU text-LAYOUT + WRAP engine
    // (text_layout_plan) with the default metrics (a fontSize-based estimate —
    // device-free, so this needs no Vulkan atlas). Wrap within the offered
    // `available` width (the S123 infinity-aware basis: an unbounded offer -> no
    // wrap -> one line; a bounded offer -> word-wrap). The returned extent
    // {measuredWidth, measuredHeight} is the TextBlock's natural content extent;
    // the base Measure finalizer (compute_measured_desired) then applies the
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

namespace {

// 11j-4 (GAP CLOSE, dep:S440) — the Window-OWNER DP set. S440 seeded Window with
// NO Window-owner DPs; the external SplashWindow corpus file authors
// AllowsTransparency/WindowStartupLocation/WindowStyle/Topmost/ResizeMode, which
// previously threw UnknownProperty. These are REAL window-config DPs (WPF-
// faithful defaults), resolved S043-attached-aware on ANY DependencyObject (the
// loader only needs them to RESOLVE so SplashWindow parses). The HOST maps them
// to the OS window at a later window-host step (recorded NO-GAP, not silent).
void register_window_properties()
{
    // AllowsTransparency: BOOL (WPF-faithful — WPF AllowsTransparency is bool),
    // default false. Uses ValueType::Bool so the corpus authoring
    // `AllowsTransparency="true"` / `="false"` converts via the Bool converter
    // (the lowercase "true"/"false" form, S090) WITHOUT polluting the shared
    // Int32 enum-name map (which would flip global inference of "true" to Int32).
    (void)DependencyProperty::Register(
        "AllowsTransparency", ValueType::Bool, "Window",
        PropertyMetadata::WithDefault(Value::Bool(false), PropertyFlags::None));
    // WindowStartupLocation: Int32 enum {Manual=0, CenterScreen=1, CenterOwner=2},
    // default Manual.
    (void)DependencyProperty::Register(
        "WindowStartupLocation", ValueType::Int32, "Window",
        PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None));
    // WindowStyle: Int32 enum {None=0, SingleBorderWindow=1, ThreeDBorderWindow=2,
    // ToolWindow=3}, default SingleBorderWindow.
    (void)DependencyProperty::Register(
        "WindowStyle", ValueType::Int32, "Window",
        PropertyMetadata::WithDefault(Value::Int32(1), PropertyFlags::None));
    // Topmost: BOOL (WPF-faithful), default false. Same Bool rationale as
    // AllowsTransparency.
    (void)DependencyProperty::Register(
        "Topmost", ValueType::Bool, "Window",
        PropertyMetadata::WithDefault(Value::Bool(false), PropertyFlags::None));
    // ResizeMode: Int32 enum {NoResize=0, CanMinimize=1, CanResize=2,
    // CanResizeWithGrip=3}, default CanResize.
    (void)DependencyProperty::Register(
        "ResizeMode", ValueType::Int32, "Window",
        PropertyMetadata::WithDefault(Value::Int32(2), PropertyFlags::None));
}

void ensure_window_properties_registered()
{
    static bool registered = false;
    if (!registered)
    {
        registered = true;
        register_window_properties();
    }
}

} // namespace

VAML_CORE_EXPORT
Window::Window()
{
    // 11j-4: Window now owns its real Window-config DP set (AllowsTransparency /
    // WindowStartupLocation / WindowStyle / Topmost / ResizeMode) + inherits the
    // full UIElement surface (Visibility/Opacity/Margin/alignment) + the S121
    // child machinery. The content child slot is plain member state, not a DP.
    ensure_window_properties_registered();
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

// --- 11j-1 UserControl (REAL content-container element; GAP CLOSE) ----------
// RESOLVES a corpus blocker: ~90 external WPF corpus files author
// `<UserControl .../>` as a root / nested content container. A self-contained
// UIElement with a single-content child slot (Window-faithful) so a UserControl
// root RENDERS its content; the full ContentControl base is S649 (recorded).

namespace {

void register_usercontrol_properties()
{
    // `Content` — String-typed holder? NO: the authored `<UserControl.Content>`
    // property-element sets an OBJECT (Value::UntypedPtr via S452), and a
    // literal-attribute Content is uncommon. To accept BOTH a literal String
    // AND an object without a type-mismatch on Set, the DP is registered with
    // a bound type that does not reject the S452 UntypedPtr object path. The
    // S043 Set path resolves by name and stores whatever `Value` tag arrives —
    // so the metadata's bound type is the governing constraint ONLY for the
    // literal convert (convertLiteral). We register `Content` as String so a
    // literal converts to String; the property-element object branch calls
    // obj->Set(PropertyId(propName), Value::UntypedPtr(raw)) DIRECTLY (bypassing
    // convertLiteral), which stores an UntypedPtr Value on the String-typed DP.
    // DependencyObject::Set validates the TAG against the metadata's bound type
    // and would reject UntypedPtr on a String-typed DP — so we must register
    // `Content` with a permissive object-typed bound type.
    //
    // OBJECT-TYPED DP: a DP whose bound value type is UntypedPtr holds an owned/
    // borrowed object identity. A property-element object branch sets
    // Value::UntypedPtr (matches). A LITERAL attribute Content, though rare, is
    // not a valid UntypedPtr literal (no converter) — WPF Content is object, so
    // a Content attribute would be authored as an object via property-element or
    // x:Static; a literal is not a corpus pattern. We register Content as
    // UntypedPtr (object-typed). convertLiteral for UntypedPtr has no converter
    // and would throw UnknownType on a bare literal — acceptable (Content is not
    // literal-authored). The typed accessors read the stored Value by tag.
    (void)DependencyProperty::Register(
        "Content", ValueType::UntypedPtr, "UserControl",
        PropertyMetadata::WithDefault(Value::UntypedPtr(nullptr), PropertyFlags::None));
}

void ensure_usercontrol_properties_registered()
{
    static bool registered = false;
    if (!registered)
    {
        registered = true;
        register_usercontrol_properties();
    }
}

} // namespace

VAML_CORE_EXPORT
UserControl::UserControl()
{
    ensure_usercontrol_properties_registered();
}

VAML_CORE_EXPORT
void UserControl::set_content(std::unique_ptr<UIElement> child)
{
    // WPF-faithful single-content slot (NOT a panel list): a UserControl hosts
    // EXACTLY ONE content child. Replace any prior content.
    if (!child)
    {
        InvalidateMeasure();
        InvalidateArrange();
        return;
    }
    while (child_count() > 0)
    {
        [[maybe_unused]] std::size_t remaining = remove_child(0);
    }
    content_ = nullptr;

    UIElement* raw = child.get();
    add_child(std::move(child));
    content_ = raw;
    // Mirror the Content DP onto the object identity (the header documents the
    // Content DP + the child slot both describe the same single content).
    (void)Set(PropertyId::of("Content"), Value::UntypedPtr(raw));
    InvalidateMeasure();
    InvalidateArrange();
}

VAML_CORE_EXPORT
UIElement* UserControl::content() const noexcept
{
    return content_;
}

VAML_CORE_EXPORT
Size UserControl::MeasureOverride(const Size& available) const
{
    if (content_)
    {
        content_->Measure(available);
        return content_->desired_size();
    }
    return Size{ 0.0f, 0.0f };
}

VAML_CORE_EXPORT
void UserControl::ArrangeOverride(const vaml::Rect& finalSlot)
{
    if (content_)
    {
        content_->Arrange(finalSlot);
    }
}

// --- 11j-6 Application (REAL document-root element; GAP CLOSE) --------------
// RESOLVES `<Application ... StartupUri="...">` (the canonical WPF app entry).
// Application is NOT a UIElement (WPF-faithful — it is the app-level object);
// it is a loadable DependencyObject root carrying a String StartupUri DP.

namespace {

void register_application_properties()
{
    (void)DependencyProperty::Register(
        "StartupUri", ValueType::String, "Application",
        PropertyMetadata::WithDefault(Value::String(std::string{}), PropertyFlags::None));
}

void ensure_application_properties_registered()
{
    static bool registered = false;
    if (!registered)
    {
        registered = true;
        register_application_properties();
    }
}

} // namespace

VAML_CORE_EXPORT
Application::Application()
{
    ensure_application_properties_registered();
}

VAML_CORE_EXPORT
SetResult Application::set_startup_uri(std::string value)
{
    return Set(PropertyId::of("StartupUri"), Value::String(std::move(value)));
}

VAML_CORE_EXPORT
std::string Application::startup_uri() const
{
    return Get(PropertyId::of("StartupUri")).get<std::string>();
}

// --- 11j-2 ResourceDictionaryHolder (REAL document-root element; GAP CLOSE) --
// A DependencyObject hosting an OWNED ResourceDictionary, faithfully mirroring
// the WPF fact that ResourceDictionary is a plain (non-DependencyObject) class
// that is still a legal standalone-document root. Registered as the element name
// "ResourceDictionary" so `<ResourceDictionary>` resolves as a root; the reader
// routes build into build_resource_dictionary and hands the owned dict to the
// holder. Nested dictionary authoring (`.Resources`/.MergedDictionaries) is
// unchanged (no holder needed).

namespace {

void register_resourcedictionary_properties()
{
    // No DPs of its own (a ResourceDictionary is a resource store, not a value
    // carrier). Registration is a no-op guard for future RD DPs.
}

void ensure_resourcedictionary_properties_registered()
{
    static bool registered = false;
    if (!registered)
    {
        registered = true;
        register_resourcedictionary_properties();
    }
}

} // namespace

VAML_CORE_EXPORT
ResourceDictionaryHolder::ResourceDictionaryHolder()
{
    ensure_resourcedictionary_properties_registered();
}

// Out-of-line destructor: this TU includes ResourceDictionary.h, so the owned
// unique_ptr<ResourceDictionary> member is destroyed here (not where the holder
// is declared, where ResourceDictionary is only forward-declared — the clang
// requirement).
VAML_CORE_EXPORT
ResourceDictionaryHolder::~ResourceDictionaryHolder() = default;

VAML_CORE_EXPORT
void ResourceDictionaryHolder::set_dictionary(std::unique_ptr<ResourceDictionary> dict)
{
    dict_ = std::move(dict);
}

VAML_CORE_EXPORT
const ResourceDictionary* ResourceDictionaryHolder::dictionary() const noexcept
{
    return dict_.get();
}

VAML_CORE_EXPORT
ResourceDictionary* ResourceDictionaryHolder::dictionary() noexcept
{
    return dict_.get();
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

// --- S470 DockPanel: attached Dock DP + real dock-stacking panel -----------

VAML_CORE_EXPORT
DockPanel::DockPanel()
{
    ensure_dockpanel_properties_registered();
}

VAML_CORE_EXPORT
SetResult DockPanel::set_dock(DependencyObject& obj, std::int32_t value)
{
    // Value rides the Int32 path on the attached name (S043 attached-aware:
    // the key is owned by DockPanel, stored on `obj`).
    return obj.Set(PropertyId::of("DockPanel.Dock"), Value::Int32(value));
}

VAML_CORE_EXPORT
std::int32_t DockPanel::dock(const DependencyObject& obj)
{
    return obj.Get(PropertyId::of("DockPanel.Dock")).get<std::int32_t>();
}

// The S470 real dock-stacking measure. Children are offered UNBOUNDED on both
// axes (the S123 infinity rule — a docked child measures at its natural content
// extent), accumulating the consumed dock track:
//   * Left/Right compute consume `child.desired.width` on the width axis.
//   * Top/Bottom consume `child.desired.height` on the height axis.
// The LAST child (Dock Fill) takes the REMAINING interior slot (WPF-faithful).
// Dirty-respect (S128): re-measure ONLY is_measure_dirty() children; a clean
// child reuses its stored desired_size().
VAML_CORE_EXPORT
Size DockPanel::MeasureOverride(const Size& available) const
{
    (void)available;
    const Size childAvail = Size::Infinite();

    float width  = 0.0f; // consumed horizontal track
    float height = 0.0f; // consumed vertical track

    const std::size_t n = child_count();
    for (std::size_t i = 0; i < n; ++i)
    {
        UIElement* child = child_at(i);
        if (child->is_measure_dirty())
        {
            child->Measure(childAvail);
        }
        const std::int32_t d = dock(*child);
        const bool isLast = (i + 1 == n);
        if (isLast)
        {
            // The last child (Fill) takes the remaining interior — no track
            // consumed for the panel's own DesiredSize (it fills whatever the
            // container offers).
        }
        else
        {
            switch (static_cast<Dock>(d))
            {
            case Dock::Left:
            case Dock::Right:
                width += child->desired_size().width;
                break;
            case Dock::Top:
            case Dock::Bottom:
                height += child->desired_size().height;
                break;
            case Dock::Fill:
            default:
                break;
            }
        }
    }

    return Size{ std::max(0.0f, width), std::max(0.0f, height) };
}

// The arrange-side virtual (S127). Receives the panel's FULL final render slot.
// Children are placed at cumulative dock origins in document order; each child's
// own alignment + margin is honored inside its slot via child->Arrange (S124
// compute_final_rect). The LAST child (Fill) takes the REMAINING interior rect.
// Dirty-respect (S128): re-arrange ONLY is_arrange_dirty() children.
VAML_CORE_EXPORT
void DockPanel::ArrangeOverride(const Rect& finalSlot)
{
    float left = finalSlot.x;
    float top  = finalSlot.y;
    float right  = finalSlot.right();
    float bottom = finalSlot.bottom();

    const std::size_t n = child_count();
    for (std::size_t i = 0; i < n; ++i)
    {
        UIElement* child = child_at(i);
        const bool isLast = (i + 1 == n);
        const std::int32_t d = dock(*child);
        Rect slot{ left, top, right - left, bottom - top };
        if (isLast)
        {
            // The last child fills the REMAINING interior rect (WPF-faithful).
            if (child->is_arrange_dirty())
            {
                child->Arrange(slot);
            }
            continue;
        }
        switch (static_cast<Dock>(d))
        {
        case Dock::Left:
            slot = Rect{ left, top, child->desired_size().width, bottom - top };
            if (child->is_arrange_dirty())
            {
                child->Arrange(slot);
            }
            left += child->desired_size().width;
            break;
        case Dock::Right:
            slot = Rect{ right - child->desired_size().width, top,
                         child->desired_size().width, bottom - top };
            if (child->is_arrange_dirty())
            {
                child->Arrange(slot);
            }
            right -= child->desired_size().width;
            break;
        case Dock::Top:
            slot = Rect{ left, top, right - left, child->desired_size().height };
            if (child->is_arrange_dirty())
            {
                child->Arrange(slot);
            }
            top += child->desired_size().height;
            break;
        case Dock::Bottom:
            slot = Rect{ left, bottom - child->desired_size().height,
                         right - left, child->desired_size().height };
            if (child->is_arrange_dirty())
            {
                child->Arrange(slot);
            }
            bottom -= child->desired_size().height;
            break;
        case Dock::Fill:
        default:
            // A non-last Fill child consumes no edge (fills the current slot,
            // overlapping — the last child still owns the interior).
            if (child->is_arrange_dirty())
            {
                child->Arrange(slot);
            }
            break;
        }
    }
}

// --- S471 RadioButton: IsChecked + exclusive group + Checked/Unchecked ------

VAML_CORE_EXPORT
RadioButton::RadioButton()
{
    ensure_radiobutton_properties_registered();
}

VAML_CORE_EXPORT
void RadioButton::apply_change(bool newState)
{
    onCheckedChanged_.fire();
    vaml::RoutedEventArgs args;
    args.source = this;
    args.originalSource = this;
    // The routed event identity (Checked vs Unchecked) reflects the new state.
    RaiseEvent(vaml::RoutedEvent::registered(
                   newState ? "Checked" : "Unchecked", RouteEventDirection::Bubble),
               args);
}

VAML_CORE_EXPORT
SetResult RadioButton::set_checked(bool value)
{
    const bool old = is_checked();
    if (old == value)
    {
        return vaml::SetResult::NO_CHANGE;
    }
    const SetResult r = Set(PropertyId::of("RadioButton.IsChecked"), Value::Int32(value ? 1 : 0));
    if (r == vaml::SetResult::APPLIED)
    {
        // EXCLUSIVE GROUP (the honest single-parent model): when this button
        // becomes checked, clear every SIBLING RadioButton under the SAME visual
        // parent (the common WPF single-group case). A checked sibling in a
        // DIFFERENT parent is unaffected.
        if (value)
        {
            UIElement* parent = dynamic_cast<UIElement*>(this->parent());
            if (parent != nullptr)
            {
                for (std::size_t i = 0; i < parent->child_count(); ++i)
                {
                    UIElement* sib = parent->child_at(i);
                    if (sib == this)
                    {
                        continue;
                    }
                    if (auto* rb = dynamic_cast<RadioButton*>(sib))
                    {
                        (void)rb->Set(PropertyId::of("RadioButton.IsChecked"), Value::Int32(0));
                    }
                }
            }
        }
        apply_change(value);
    }
    return r;
}

VAML_CORE_EXPORT
bool RadioButton::is_checked() const
{
    return Get(PropertyId::of("RadioButton.IsChecked")).get<int>() != 0;
}

VAML_CORE_EXPORT
Size RadioButton::MeasureOverride(const Size& available) const
{
    const float w = available.width > 0.0f ? available.width : 16.0f;
    const float h = available.height > 0.0f ? available.height : 16.0f;
    return Size{ std::max(0.0f, w), std::max(0.0f, h) };
}

// --- S472 ComboBox: Items/ItemsSource + SelectedIndex/Item + dropdown --------

VAML_CORE_EXPORT
ComboBox::ComboBox()
{
    ensure_combobox_properties_registered();
}

VAML_CORE_EXPORT
std::vector<std::string> ComboBox::split_items() const
{
    // The owned item list: the source is Items when explicitly set, else
    // ItemsSource. Items are split on commas and whitespace (simple owned list —
    // the WPF ItemCollection model is consumed at S649, recorded NO-GAP).
    const std::string src = Get(PropertyId::of("ComboBox.Items")).get<std::string>().empty()
                                ? Get(PropertyId::of("ComboBox.ItemsSource")).get<std::string>()
                                : Get(PropertyId::of("ComboBox.Items")).get<std::string>();
    std::vector<std::string> out;
    std::string cur;
    for (const char c : src)
    {
        if (c == ',' || c == ' ' || c == '\n' || c == '\t')
        {
            if (!cur.empty())
            {
                out.push_back(cur);
                cur.clear();
            }
        }
        else
        {
            cur.push_back(c);
        }
    }
    if (!cur.empty())
    {
        out.push_back(cur);
    }
    return out;
}

VAML_CORE_EXPORT
SetResult ComboBox::set_items(std::string value)
{
    return Set(PropertyId::of("ComboBox.Items"), Value::String(std::move(value)));
}

VAML_CORE_EXPORT
std::string ComboBox::items() const
{
    return Get(PropertyId::of("ComboBox.Items")).get<std::string>();
}

VAML_CORE_EXPORT
SetResult ComboBox::set_items_source(std::string value)
{
    return Set(PropertyId::of("ComboBox.ItemsSource"), Value::String(std::move(value)));
}

VAML_CORE_EXPORT
std::string ComboBox::items_source() const
{
    return Get(PropertyId::of("ComboBox.ItemsSource")).get<std::string>();
}

VAML_CORE_EXPORT
void ComboBox::apply_selection(std::int32_t newIndex)
{
    (void)newIndex;
    onSelectionChanged_.fire();
    vaml::RoutedEventArgs args;
    args.source = this;
    args.originalSource = this;
    RaiseEvent(vaml::RoutedEvent::registered("SelectionChanged", RouteEventDirection::Bubble),
               args);
}

VAML_CORE_EXPORT
SetResult ComboBox::set_selected_index(std::int32_t index)
{
    const auto items = split_items();
    const std::int32_t clamped =
        items.empty() ? -1 : std::clamp<std::int32_t>(index, -1, static_cast<std::int32_t>(items.size()) - 1);
    const std::int32_t old = Get(PropertyId::of("ComboBox.SelectedIndex")).get<int>();
    const SetResult r = Set(PropertyId::of("ComboBox.SelectedIndex"), Value::Int32(clamped));
    // Keep SelectedItem in sync (a readable round-trip, never a throw).
    (void)Set(PropertyId::of("ComboBox.SelectedItem"),
              Value::String(clamped >= 0 && static_cast<std::size_t>(clamped) < items.size()
                                ? items[static_cast<std::size_t>(clamped)]
                                : std::string{}));
    if (r == vaml::SetResult::APPLIED && clamped != old)
    {
        apply_selection(clamped);
    }
    return r;
}

VAML_CORE_EXPORT
std::int32_t ComboBox::selected_index() const
{
    return Get(PropertyId::of("ComboBox.SelectedIndex")).get<int>();
}

VAML_CORE_EXPORT
std::string ComboBox::selected_item() const
{
    return Get(PropertyId::of("ComboBox.SelectedItem")).get<std::string>();
}

VAML_CORE_EXPORT
SetResult ComboBox::set_is_open(bool value)
{
    return Set(PropertyId::of("ComboBox.IsOpen"), Value::Int32(value ? 1 : 0));
}

VAML_CORE_EXPORT
bool ComboBox::is_open() const
{
    return Get(PropertyId::of("ComboBox.IsOpen")).get<int>() != 0;
}

VAML_CORE_EXPORT
bool ComboBox::open(bool dropdown)
{
    (void)set_is_open(dropdown);
    return is_open();
}

VAML_CORE_EXPORT
bool ComboBox::select_item(const std::string& display)
{
    const auto items = split_items();
    for (std::size_t i = 0; i < items.size(); ++i)
    {
        if (items[i] == display)
        {
            (void)set_selected_index(static_cast<std::int32_t>(i));
            return true;
        }
    }
    return false;
}

VAML_CORE_EXPORT
Size ComboBox::MeasureOverride(const Size& available) const
{
    const float w = available.width > 0.0f ? available.width : 120.0f;
    const float h = available.height > 0.0f ? available.height : 22.0f;
    return Size{ std::max(0.0f, w), std::max(0.0f, h) };
}

// --- S473 ListBox + ListView: Items/ItemsSource + selection + ItemTemplate ----

VAML_CORE_EXPORT
ListBox::ListBox()
{
    ensure_listbox_properties_registered();
}

VAML_CORE_EXPORT
std::vector<std::string> ListBox::split_items() const
{
    const std::string src = Get(PropertyId::of("ListBox.Items")).get<std::string>().empty()
                                ? Get(PropertyId::of("ListBox.ItemsSource")).get<std::string>()
                                : Get(PropertyId::of("ListBox.Items")).get<std::string>();
    std::vector<std::string> out;
    std::string cur;
    for (const char c : src)
    {
        if (c == ',' || c == ' ' || c == '\n' || c == '\t')
        {
            if (!cur.empty())
            {
                out.push_back(cur);
                cur.clear();
            }
        }
        else
        {
            cur.push_back(c);
        }
    }
    if (!cur.empty())
    {
        out.push_back(cur);
    }
    return out;
}

VAML_CORE_EXPORT
SetResult ListBox::set_items(std::string value)
{
    return Set(PropertyId::of("ListBox.Items"), Value::String(std::move(value)));
}

VAML_CORE_EXPORT
std::string ListBox::items() const
{
    return Get(PropertyId::of("ListBox.Items")).get<std::string>();
}

VAML_CORE_EXPORT
SetResult ListBox::set_items_source(std::string value)
{
    return Set(PropertyId::of("ListBox.ItemsSource"), Value::String(std::move(value)));
}

VAML_CORE_EXPORT
std::string ListBox::items_source() const
{
    return Get(PropertyId::of("ListBox.ItemsSource")).get<std::string>();
}

VAML_CORE_EXPORT
void ListBox::apply_selection(std::int32_t newIndex)
{
    (void)newIndex;
    onSelectionChanged_.fire();
    vaml::RoutedEventArgs args;
    args.source = this;
    args.originalSource = this;
    RaiseEvent(vaml::RoutedEvent::registered("SelectionChanged", RouteEventDirection::Bubble),
               args);
}

VAML_CORE_EXPORT
SetResult ListBox::set_selected_index(std::int32_t index)
{
    const auto items = split_items();
    const std::int32_t clamped =
        items.empty() ? -1 : std::clamp<std::int32_t>(index, -1, static_cast<std::int32_t>(items.size()) - 1);
    const std::int32_t old = Get(PropertyId::of("ListBox.SelectedIndex")).get<int>();
    const SetResult r = Set(PropertyId::of("ListBox.SelectedIndex"), Value::Int32(clamped));
    (void)Set(PropertyId::of("ListBox.SelectedItem"),
              Value::String(clamped >= 0 && static_cast<std::size_t>(clamped) < items.size()
                                ? items[static_cast<std::size_t>(clamped)]
                                : std::string{}));
    if (r == vaml::SetResult::APPLIED && clamped != old)
    {
        apply_selection(clamped);
    }
    return r;
}

VAML_CORE_EXPORT
std::int32_t ListBox::selected_index() const
{
    return Get(PropertyId::of("ListBox.SelectedIndex")).get<int>();
}

VAML_CORE_EXPORT
std::string ListBox::selected_item() const
{
    return Get(PropertyId::of("ListBox.SelectedItem")).get<std::string>();
}

VAML_CORE_EXPORT
SetResult ListBox::set_item_template(std::string value)
{
    // The C++ programmatic accessor wraps the authored template TEXT in a
    // retained Text template element so the UntypedPtr-bound ItemTemplate DP
    // stores it, and get_item_template reads it back identical. (The VAML
    // property-element form `<ListBox.ItemTemplate>` stores the authored OBJECT
    // child directly via the S452 grammar -> Value::UntypedPtr.)
    auto tmpl = std::make_unique<TextBlock>();
    (void)tmpl->set_text(value);
    UIElement* raw = tmpl.get();
    raw->set_parent(this);
    itemTemplateOwned_.push_back(std::move(tmpl));
    return Set(PropertyId::of("ItemTemplate"), Value::UntypedPtr(raw));
}

VAML_CORE_EXPORT
std::string ListBox::item_template() const
{
    const Value v = Get(PropertyId::of("ItemTemplate"));
    // An authored object child (the S452 property-element / the programmatic
    // accessor) is stored as UntypedPtr; render it to its authored per-item
    // content text if it holds a Text template.
    if (v.type() == ValueType::UntypedPtr)
    {
        if (auto* tmpl = static_cast<UIElement*>(v.get<void*>()))
        {
            if (auto* t = dynamic_cast<TextBlock*>(tmpl))
            {
                return t->text();
            }
        }
    }
    return std::string{};
}

VAML_CORE_EXPORT
std::size_t ListBox::render_items()
{
    // The S473 real ListBox algorithm: items render AUTHORED per-item content
    // through the S364 instantiate seam. The authored ItemTemplate (the S452
    // property-element object child, stored on the global "ItemTemplate" DP) is
    // READ here; render_items instantiates ONE UIElement per item — the honest
    // per-item leaf carries the item's text (the authored template declares how
    // an item renders; the item content itself is the authored item list).
    const auto items = split_items();

    // Clear any prior rendered item elements (a re-render refreshes the list).
    while (child_count() > 0)
    {
        [[maybe_unused]] std::size_t remaining = remove_child(0);
    }

    std::size_t built = 0;
    for (const std::string& item : items)
    {
        auto text = std::make_unique<TextBlock>();
        (void)text->set_text(item);
        add_child(std::move(text));
        ++built;
    }
    return built;
}

VAML_CORE_EXPORT
Size ListBox::MeasureOverride(const Size& available) const
{
    const float w = available.width > 0.0f ? available.width : 120.0f;
    const float h = available.height > 0.0f ? available.height : 80.0f;
    return Size{ std::max(0.0f, w), std::max(0.0f, h) };
}

VAML_CORE_EXPORT
ListView::ListView()
{
    // ListView reuses the ListBox DP + selection surface unchanged (the shared
    // item-selection base). No extra DPs at S473; the ListView-specific
    // virtualization is S649, recorded NO-GAP.
    ensure_listbox_properties_registered();
}

// ---------------------------------------------------------------------------
// SPRINT 11d (S480–S486) — the core display + input controls. Each control is
// a REAL re-based UIElement with owner-scoped DPs (the established name-keyed
// registry — a reused name under a different owner would be ambiguous, per the
// S043/S131 WrapPanel precedent). No new MaterialKind / no ContentControl base
// (the S649 continuation, recorded NO-GAP not silent). The classifier arms
// (Solid/Border/Monochrome) live in UiSceneEncoder.cpp + VamlRenderer.cpp.
// ---------------------------------------------------------------------------

// --- S480 Label: Content/FontSize/FontFamily/Foreground/HContentAlign -------

namespace {

// 11j-5 (GAP CLOSE): Label's SHARED display names (FontSize/FontFamily/
// HorizontalContentAlignment) are now owned by the single Control base bundle
// (see register_control_base_properties, owner "Control") — Label RE-POINTS its
// typed accessors to those bare names, and the prior dotted "Label.FontSize" /
// "Label.FontFamily" / "Label.HorizontalContentAlignment" registrations are
// REMOVED here (NO backfill). The Label-OWNED names that remain are only
// Content + Foreground (both Label-specific). VerticalContentAlignment is
// resolved by the shared base (Label authors it unprefixed like the corpus).
void register_label_properties()
{
    (void)DependencyProperty::Register(
        "Label.Content", ValueType::String, "Label",
        PropertyMetadata::WithDefault(Value::String(std::string{}), PropertyFlags::None));
    (void)DependencyProperty::Register(
        "Label.Foreground", ValueType::BrushId, "Label",
        PropertyMetadata::WithDefault(Value::BrushId(0), PropertyFlags::None));
}

void ensure_label_properties_registered()
{
    ensure_control_base_properties_registered();
    static bool registered = false;
    if (!registered)
    {
        registered = true;
        register_label_properties();
    }
}

} // namespace

VAML_CORE_EXPORT
Label::Label()
{
    ensure_label_properties_registered();
}

VAML_CORE_EXPORT
SetResult Label::set_content(std::string value)
{
    return Set(PropertyId::of("Label.Content"), Value::String(std::move(value)));
}

VAML_CORE_EXPORT
std::string Label::content() const
{
    return Get(PropertyId::of("Label.Content")).get<std::string>();
}

VAML_CORE_EXPORT
SetResult Label::set_font_size(double value)
{
    // 11j-5: FontSize is the shared Control base DP (bare name, owner "Control").
    return Set(PropertyId::of("FontSize"), Value::Double(value));
}

VAML_CORE_EXPORT
double Label::font_size() const
{
    return Get(PropertyId::of("FontSize")).get<double>();
}

VAML_CORE_EXPORT
SetResult Label::set_font_family(std::string value)
{
    // 11j-5: FontFamily is the shared Control base DP (bare name, owner "Control").
    return Set(PropertyId::of("FontFamily"), Value::String(std::move(value)));
}

VAML_CORE_EXPORT
std::string Label::font_family() const
{
    return Get(PropertyId::of("FontFamily")).get<std::string>();
}

VAML_CORE_EXPORT
SetResult Label::set_foreground(std::int32_t brushId)
{
    return Set(PropertyId::of("Label.Foreground"), Value::BrushId(brushId));
}

VAML_CORE_EXPORT
std::int32_t Label::foreground() const
{
    return Get(PropertyId::of("Label.Foreground")).get<std::int32_t>();
}

VAML_CORE_EXPORT
SetResult Label::set_horizontal_content_alignment(std::int32_t value)
{
    // 11j-5: HorizontalContentAlignment is the shared Control base DP (bare).
    return Set(PropertyId::of("HorizontalContentAlignment"), Value::Int32(value));
}

VAML_CORE_EXPORT
std::int32_t Label::horizontal_content_alignment() const
{
    return Get(PropertyId::of("HorizontalContentAlignment")).get<std::int32_t>();
}

VAML_CORE_EXPORT
Size Label::MeasureOverride(const Size& available) const
{
    // The S480 real Label algorithm: wrap the Content through the S400
    // text_layout_plan (the S400 basis — same wrap rule S400 Text uses). The
    // returned extent is the wrapped text box; the base Measure finalizer
    // applies the S123 stretch-vs-wrap. The content glyphs render via the S400
    // Text path at the classifier (recorded NO-GAP not silent); the Label
    // CONTRIBUTES the Solid containing box at the vertical slice.
    const float fontSize = static_cast<float>(font_size());
    const std::string content = this->content();
    const TextLayoutPlan plan = text_layout_plan(
        content, fontSize, available.width,
        [fontSize](std::uint32_t cp) { return default_text_metric(cp, fontSize); });
    return Size{ plan.measuredWidth, plan.measuredHeight };
}

// --- S481 Border: BorderBrush/BorderThickness/CornerRadius/Padding + content --

namespace {

void register_border_properties()
{
    (void)DependencyProperty::Register(
        "Border.BorderBrush", ValueType::BrushId, "Border",
        PropertyMetadata::WithDefault(Value::BrushId(0), PropertyFlags::None));
    (void)DependencyProperty::Register(
        "Border.BorderThickness", ValueType::Thickness, "Border",
        PropertyMetadata::WithDefault(Value::Thickness(vaml::Thickness{}), PropertyFlags::None));
    (void)DependencyProperty::Register(
        "Border.CornerRadius", ValueType::Thickness, "Border",
        PropertyMetadata::WithDefault(Value::Thickness(vaml::Thickness{}), PropertyFlags::None));
    // (11j-5) Border.Padding is REMOVED — Padding is the shared Control base DP
    // (bare "Padding", owner "Control"); Border re-points its padding accessor.
}

void ensure_border_properties_registered()
{
    ensure_control_base_properties_registered();
    static bool registered = false;
    if (!registered)
    {
        registered = true;
        register_border_properties();
    }
}

} // namespace

VAML_CORE_EXPORT
Border::Border()
{
    ensure_border_properties_registered();
}

VAML_CORE_EXPORT
SetResult Border::set_border_brush(std::int32_t brushId)
{
    return Set(PropertyId::of("Border.BorderBrush"), Value::BrushId(brushId));
}

VAML_CORE_EXPORT
std::int32_t Border::border_brush() const
{
    return Get(PropertyId::of("Border.BorderBrush")).get<std::int32_t>();
}

VAML_CORE_EXPORT
SetResult Border::set_border_thickness(vaml::Thickness value)
{
    return Set(PropertyId::of("Border.BorderThickness"), Value::Thickness(value));
}

VAML_CORE_EXPORT
vaml::Thickness Border::border_thickness() const
{
    return Get(PropertyId::of("Border.BorderThickness")).get<vaml::Thickness>();
}

VAML_CORE_EXPORT
SetResult Border::set_corner_radius(vaml::Thickness value)
{
    return Set(PropertyId::of("Border.CornerRadius"), Value::Thickness(value));
}

VAML_CORE_EXPORT
vaml::Thickness Border::corner_radius() const
{
    return Get(PropertyId::of("Border.CornerRadius")).get<vaml::Thickness>();
}

VAML_CORE_EXPORT
SetResult Border::set_padding(vaml::Thickness value)
{
    // 11j-5: Padding is the shared Control base DP (bare "Padding").
    return Set(PropertyId::of("Padding"), Value::Thickness(value));
}

VAML_CORE_EXPORT
vaml::Thickness Border::padding() const
{
    return Get(PropertyId::of("Padding")).get<vaml::Thickness>();
}

VAML_CORE_EXPORT
void Border::set_content(std::unique_ptr<UIElement> child)
{
    // WPF-faithful single-content slot (mirrors Window): replace any prior.
    if (!child)
    {
        InvalidateMeasure();
        InvalidateArrange();
        return;
    }
    while (child_count() > 0)
    {
        [[maybe_unused]] std::size_t remaining = remove_child(0);
    }
    content_ = nullptr;
    UIElement* raw = child.get();
    add_child(std::move(child));
    content_ = raw;
    InvalidateMeasure();
    InvalidateArrange();
}

VAML_CORE_EXPORT
UIElement* Border::content() const noexcept
{
    return content_;
}

VAML_CORE_EXPORT
Size Border::MeasureOverride(const Size& available) const
{
    // The S481 real Border algorithm: measure the single content child within
    // the offered box minus the Padding + BorderThickness insets; report the
    // child's desired size PLUS the insets (the border box wraps its content).
    const vaml::Thickness bt = border_thickness();
    const vaml::Thickness pd = padding();
    const float insetW = bt.left + bt.right + pd.left + pd.right;
    const float insetH = bt.top + bt.bottom + pd.top + pd.bottom;
    Size inner{ available.width > 0.0f ? std::max(0.0f, available.width - insetW) : 0.0f,
                available.height > 0.0f ? std::max(0.0f, available.height - insetH) : 0.0f };
    if (content_)
        content_->Measure(inner);
    const float cw = content_ ? content_->desired_size().width : 0.0f;
    const float ch = content_ ? content_->desired_size().height : 0.0f;
    return Size{ std::max(0.0f, cw + insetW), std::max(0.0f, ch + insetH) };
}

VAML_CORE_EXPORT
void Border::ArrangeOverride(const vaml::Rect& finalSlot)
{
    // Arrange the single content child into the INNER slot (behind the border +
    // padding). The border bezel itself is drawn by the classifier (S481 —
    // MaterialKind::Border at the arranged box).
    if (!content_)
        return;
    const vaml::Thickness bt = border_thickness();
    const vaml::Thickness pd = padding();
    const float ox = bt.left + pd.left;
    const float oy = bt.top + pd.top;
    const float ow = std::max(0.0f, finalSlot.width - (bt.left + bt.right + pd.left + pd.right));
    const float oh = std::max(0.0f, finalSlot.height - (bt.top + bt.bottom + pd.top + pd.bottom));
    content_->Arrange(vaml::Rect{ finalSlot.x + ox, finalSlot.y + oy, ow, oh });
}

// --- S482 Rectangle/Ellipse/Line shape DPs + measure ------------------------

namespace {

void register_rectangle_properties()
{
    (void)DependencyProperty::Register(
        "Rectangle.Fill", ValueType::BrushId, "Rectangle",
        PropertyMetadata::WithDefault(Value::BrushId(0), PropertyFlags::None));
    (void)DependencyProperty::Register(
        "Rectangle.Stroke", ValueType::BrushId, "Rectangle",
        PropertyMetadata::WithDefault(Value::BrushId(0), PropertyFlags::None));
    (void)DependencyProperty::Register(
        "Rectangle.StrokeThickness", ValueType::Double, "Rectangle",
        PropertyMetadata::WithDefault(Value::Double(0.0), PropertyFlags::None));
    // Width/Height ride the BASE UIElement S123 layout DPs (auto/NaN), NOT a
    // duplicate custom DP (recorded NO-GAP — the shape box is the inherited
    // WPF-style Width/Height the S123 reconciler applies).
}

void ensure_rectangle_properties_registered()
{
    static bool registered = false;
    if (!registered)
    {
        registered = true;
        register_rectangle_properties();
    }
}

} // namespace

VAML_CORE_EXPORT
Rectangle::Rectangle()
{
    ensure_rectangle_properties_registered();
}

VAML_CORE_EXPORT
SetResult Rectangle::set_fill(std::int32_t brushId)
{
    return Set(PropertyId::of("Rectangle.Fill"), Value::BrushId(brushId));
}
VAML_CORE_EXPORT
std::int32_t Rectangle::fill() const
{
    return Get(PropertyId::of("Rectangle.Fill")).get<std::int32_t>();
}
VAML_CORE_EXPORT
SetResult Rectangle::set_stroke(std::int32_t brushId)
{
    return Set(PropertyId::of("Rectangle.Stroke"), Value::BrushId(brushId));
}
VAML_CORE_EXPORT
std::int32_t Rectangle::stroke() const
{
    return Get(PropertyId::of("Rectangle.Stroke")).get<std::int32_t>();
}
VAML_CORE_EXPORT
SetResult Rectangle::set_stroke_thickness(double value)
{
    return Set(PropertyId::of("Rectangle.StrokeThickness"), Value::Double(value));
}
VAML_CORE_EXPORT
double Rectangle::stroke_thickness() const
{
    return Get(PropertyId::of("Rectangle.StrokeThickness")).get<double>();
}

VAML_CORE_EXPORT
Size Rectangle::MeasureOverride(const Size& available) const
{
    // The S482 Rectangle measure: a nominal intrinsic box. The base S123
    // reconciler applies the WPF-style Width/Height (auto/NaN) + the
    // stretch-vs-wrap rule, so a bounded offer stretches a default-Stretch
    // Rectangle. The fill/stroke render is the classifier (Solid).
    const float w = available.width > 0.0f ? std::min(available.width, 200.0f) : 0.0f;
    const float h = available.height > 0.0f ? std::min(available.height, 120.0f) : 0.0f;
    return Size{ std::max(0.0f, w), std::max(0.0f, h) };
}

// Ellipse (mirrors Rectangle's DP + measure; the CURVED outline is the S209/
// S649 continuation, recorded NO-GAP not silent).

namespace {

void register_ellipse_properties()
{
    (void)DependencyProperty::Register(
        "Ellipse.Fill", ValueType::BrushId, "Ellipse",
        PropertyMetadata::WithDefault(Value::BrushId(0), PropertyFlags::None));
    (void)DependencyProperty::Register(
        "Ellipse.Stroke", ValueType::BrushId, "Ellipse",
        PropertyMetadata::WithDefault(Value::BrushId(0), PropertyFlags::None));
    (void)DependencyProperty::Register(
        "Ellipse.StrokeThickness", ValueType::Double, "Ellipse",
        PropertyMetadata::WithDefault(Value::Double(0.0), PropertyFlags::None));
    // Width/Height ride the base UIElement S123 layout DPs (auto/NaN).
}

void ensure_ellipse_properties_registered()
{
    static bool registered = false;
    if (!registered)
    {
        registered = true;
        register_ellipse_properties();
    }
}

} // namespace

VAML_CORE_EXPORT
Ellipse::Ellipse()
{
    ensure_ellipse_properties_registered();
}

VAML_CORE_EXPORT
SetResult Ellipse::set_fill(std::int32_t brushId)
{
    return Set(PropertyId::of("Ellipse.Fill"), Value::BrushId(brushId));
}
VAML_CORE_EXPORT
std::int32_t Ellipse::fill() const
{
    return Get(PropertyId::of("Ellipse.Fill")).get<std::int32_t>();
}
VAML_CORE_EXPORT
SetResult Ellipse::set_stroke(std::int32_t brushId)
{
    return Set(PropertyId::of("Ellipse.Stroke"), Value::BrushId(brushId));
}
VAML_CORE_EXPORT
std::int32_t Ellipse::stroke() const
{
    return Get(PropertyId::of("Ellipse.Stroke")).get<std::int32_t>();
}
VAML_CORE_EXPORT
SetResult Ellipse::set_stroke_thickness(double value)
{
    return Set(PropertyId::of("Ellipse.StrokeThickness"), Value::Double(value));
}
VAML_CORE_EXPORT
double Ellipse::stroke_thickness() const
{
    return Get(PropertyId::of("Ellipse.StrokeThickness")).get<double>();
}

VAML_CORE_EXPORT
Size Ellipse::MeasureOverride(const Size& available) const
{
    // The S482 Ellipse measure: a nominal intrinsic box; the base S123
    // reconciler applies the CPI-style Width/Height + stretch-vs-wrap.
    const float w = available.width > 0.0f ? std::min(available.width, 200.0f) : 0.0f;
    const float h = available.height > 0.0f ? std::min(available.height, 120.0f) : 0.0f;
    return Size{ std::max(0.0f, w), std::max(0.0f, h) };
}

// Line (has no Width/Height; the stroke axis is the box).

namespace {

void register_line_properties()
{
    (void)DependencyProperty::Register(
        "Line.Fill", ValueType::BrushId, "Line",
        PropertyMetadata::WithDefault(Value::BrushId(0), PropertyFlags::None));
    (void)DependencyProperty::Register(
        "Line.Stroke", ValueType::BrushId, "Line",
        PropertyMetadata::WithDefault(Value::BrushId(0), PropertyFlags::None));
    (void)DependencyProperty::Register(
        "Line.StrokeThickness", ValueType::Double, "Line",
        PropertyMetadata::WithDefault(Value::Double(1.0), PropertyFlags::None));
    // Width/Height ride the base UIElement S123 layout DPs (auto/NaN).
}

void ensure_line_properties_registered()
{
    static bool registered = false;
    if (!registered)
    {
        registered = true;
        register_line_properties();
    }
}

} // namespace

VAML_CORE_EXPORT
Line::Line()
{
    ensure_line_properties_registered();
}

VAML_CORE_EXPORT
SetResult Line::set_fill(std::int32_t brushId)
{
    return Set(PropertyId::of("Line.Fill"), Value::BrushId(brushId));
}
VAML_CORE_EXPORT
std::int32_t Line::fill() const
{
    return Get(PropertyId::of("Line.Fill")).get<std::int32_t>();
}
VAML_CORE_EXPORT
SetResult Line::set_stroke(std::int32_t brushId)
{
    return Set(PropertyId::of("Line.Stroke"), Value::BrushId(brushId));
}
VAML_CORE_EXPORT
std::int32_t Line::stroke() const
{
    return Get(PropertyId::of("Line.Stroke")).get<std::int32_t>();
}
VAML_CORE_EXPORT
SetResult Line::set_stroke_thickness(double value)
{
    return Set(PropertyId::of("Line.StrokeThickness"), Value::Double(value));
}
VAML_CORE_EXPORT
double Line::stroke_thickness() const
{
    return Get(PropertyId::of("Line.StrokeThickness")).get<double>();
}

VAML_CORE_EXPORT
Size Line::MeasureOverride(const Size& available) const
{
    // A Line renders as a thin stroke rect along its axis: default a rule at
    // the offered width with the stroke-thickness cross extent (a zero-length
    // Line -> degenerate, skipped by the classifier). The base S123 reconciler
    // applies the WPF-style Width/Height + stretch-vs-wrap.
    const float w = available.width > 0.0f ? available.width : 100.0f;
    const float h = static_cast<float>(stroke_thickness() > 0.0 ? stroke_thickness() : 1.0);
    return Size{ std::max(0.0f, w), std::max(0.0f, h) };
}

// --- S483 Image: Source/IconKey/Stretch/Width/Height (the S400_1 icon path) ---

namespace {

void register_image_properties()
{
    (void)DependencyProperty::Register(
        "Image.Source", ValueType::String, "Image",
        PropertyMetadata::WithDefault(Value::String(std::string{}), PropertyFlags::None));
    (void)DependencyProperty::Register(
        "Image.IconKey", ValueType::String, "Image",
        PropertyMetadata::WithDefault(Value::String(std::string{}), PropertyFlags::None));
    (void)DependencyProperty::Register(
        "Image.Stretch", ValueType::Int32, "Image",
        PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None));
    // Width/Height ride the base UIElement S123 layout DPs (auto/NaN) — the
    // image box is the inherited WPF-style Width/Height the reconciler applies.
}

void ensure_image_properties_registered()
{
    static bool registered = false;
    if (!registered)
    {
        registered = true;
        register_image_properties();
    }
}

} // namespace

VAML_CORE_EXPORT
Image::Image()
{
    ensure_image_properties_registered();
}

VAML_CORE_EXPORT
SetResult Image::set_source(std::string value)
{
    return Set(PropertyId::of("Image.Source"), Value::String(std::move(value)));
}
VAML_CORE_EXPORT
std::string Image::source() const
{
    return Get(PropertyId::of("Image.Source")).get<std::string>();
}
VAML_CORE_EXPORT
SetResult Image::set_icon_key(std::string value)
{
    return Set(PropertyId::of("Image.IconKey"), Value::String(std::move(value)));
}
VAML_CORE_EXPORT
std::string Image::icon_key() const
{
    return Get(PropertyId::of("Image.IconKey")).get<std::string>();
}
VAML_CORE_EXPORT
SetResult Image::set_stretch(std::int32_t value)
{
    return Set(PropertyId::of("Image.Stretch"), Value::Int32(value));
}
VAML_CORE_EXPORT
std::int32_t Image::stretch() const
{
    return Get(PropertyId::of("Image.Stretch")).get<std::int32_t>();
}

VAML_CORE_EXPORT
Size Image::MeasureOverride(const Size& available) const
{
    // The S483 real Image algorithm: a nominal box; the base S123 reconciler
    // applies the WPF-style Width/Height (auto/NaN) — a bounded default stands
    // in for a 0-auto axis (mirroring the S400_1 Icon's nominal 16px box; the
    // real asset's natural extent is resolved at RENDER by the cached provider
    // + best-fit (S213_1), recorded NO-GAP not silent).
    const float w = width() > 0.0 ? static_cast<float>(width())
                                  : (available.width > 0.0f ? available.width : 16.0f);
    const float h = height() > 0.0 ? static_cast<float>(height())
                                   : (available.height > 0.0f ? available.height : 16.0f);
    return Size{ std::max(0.0f, w), std::max(0.0f, h) };
}

// --- S484 ProgressBar: Value/Min/Max + value_changed + determinate fill -----

namespace {

void register_progressbar_properties()
{
    (void)DependencyProperty::Register(
        "ProgressBar.Value", ValueType::Double, "ProgressBar",
        PropertyMetadata::WithDefault(Value::Double(0.0), PropertyFlags::None));
    (void)DependencyProperty::Register(
        "ProgressBar.Minimum", ValueType::Double, "ProgressBar",
        PropertyMetadata::WithDefault(Value::Double(0.0), PropertyFlags::None));
    (void)DependencyProperty::Register(
        "ProgressBar.Maximum", ValueType::Double, "ProgressBar",
        PropertyMetadata::WithDefault(Value::Double(100.0), PropertyFlags::None));
    (void)RoutedEvent::registered("ValueChanged", RouteEventDirection::Bubble);
}

void ensure_progressbar_properties_registered()
{
    static bool registered = false;
    if (!registered)
    {
        registered = true;
        register_progressbar_properties();
    }
}

} // namespace

VAML_CORE_EXPORT
ProgressBar::ProgressBar()
{
    ensure_progressbar_properties_registered();
}

VAML_CORE_EXPORT
SetResult ProgressBar::set_value(double v)
{
    const double lo = std::min(minimum(), maximum());
    const double hi = std::max(minimum(), maximum());
    const double clamped = std::clamp(v, lo, hi);
    const double old = Get(PropertyId::of("ProgressBar.Value")).get<double>();
    if (old == clamped)
        return vaml::SetResult::NO_CHANGE;
    const SetResult r = Set(PropertyId::of("ProgressBar.Value"), Value::Double(clamped));
    if (r == vaml::SetResult::APPLIED)
        apply_change(clamped);
    return r;
}

VAML_CORE_EXPORT
double ProgressBar::value() const
{
    return Get(PropertyId::of("ProgressBar.Value")).get<double>();
}

VAML_CORE_EXPORT
SetResult ProgressBar::set_minimum(double v)
{
    return Set(PropertyId::of("ProgressBar.Minimum"), Value::Double(v));
}

VAML_CORE_EXPORT
double ProgressBar::minimum() const
{
    return Get(PropertyId::of("ProgressBar.Minimum")).get<double>();
}

VAML_CORE_EXPORT
SetResult ProgressBar::set_maximum(double v)
{
    return Set(PropertyId::of("ProgressBar.Maximum"), Value::Double(v));
}

VAML_CORE_EXPORT
double ProgressBar::maximum() const
{
    return Get(PropertyId::of("ProgressBar.Maximum")).get<double>();
}

VAML_CORE_EXPORT
void ProgressBar::apply_change(double newValue)
{
    (void)newValue; // the routed change is the notification; the DP holds the value
    // The S484 "routed change event": fire the Signal + raise the BUBBLING
    // "ValueChanged" routed event (source==this). Mirrors the S403 Slider.
    onValueChanged_.fire();
    vaml::RoutedEventArgs args;
    args.source = this;
    args.originalSource = this;
    RaiseEvent(vaml::RoutedEvent::registered("ValueChanged", vaml::RouteEventDirection::Bubble),
               args);
}

VAML_CORE_EXPORT
Size ProgressBar::MeasureOverride(const Size& available) const
{
    // The S484 real ProgressBar algorithm: a bounded horizontal track box (the
    // S123 finalizer applies the stretch-vs-wrap). The determinate fill is the
    // classifier (Solid) at the arranged rect.
    const float w = available.width > 0.0f ? available.width : 120.0f;
    const float h = available.height > 0.0f ? available.height : 12.0f;
    return Size{ std::max(0.0f, w), std::max(0.0f, h) };
}

// --- S485 PasswordBox: masked single-line edit + TextChanged -----------------

namespace {

void register_passwordbox_properties()
{
    (void)DependencyProperty::Register(
        "PasswordBox.Password", ValueType::String, "PasswordBox",
        PropertyMetadata::WithDefault(Value::String(std::string{}), PropertyFlags::None));
    (void)DependencyProperty::Register(
        "PasswordBox.MaxLength", ValueType::Int32, "PasswordBox",
        PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None));
    (void)DependencyProperty::Register(
        "PasswordBox.PasswordChar", ValueType::String, "PasswordBox",
        PropertyMetadata::WithDefault(Value::String(std::string("*")), PropertyFlags::None));
    (void)RoutedEvent::registered("TextChanged", RouteEventDirection::Bubble);
}

void ensure_passwordbox_properties_registered()
{
    static bool registered = false;
    if (!registered)
    {
        registered = true;
        register_passwordbox_properties();
    }
}

} // namespace

VAML_CORE_EXPORT
PasswordBox::PasswordBox()
{
    ensure_passwordbox_properties_registered();
}

VAML_CORE_EXPORT
SetResult PasswordBox::set_password(std::string value)
{
    return Set(PropertyId::of("PasswordBox.Password"), Value::String(std::move(value)));
}

VAML_CORE_EXPORT
std::string PasswordBox::password() const
{
    return Get(PropertyId::of("PasswordBox.Password")).get<std::string>();
}

VAML_CORE_EXPORT
SetResult PasswordBox::set_max_length(std::int32_t value)
{
    return Set(PropertyId::of("PasswordBox.MaxLength"), Value::Int32(value));
}

VAML_CORE_EXPORT
std::int32_t PasswordBox::max_length() const
{
    return Get(PropertyId::of("PasswordBox.MaxLength")).get<std::int32_t>();
}

VAML_CORE_EXPORT
SetResult PasswordBox::set_password_char(std::string value)
{
    return Set(PropertyId::of("PasswordBox.PasswordChar"), Value::String(std::move(value)));
}

VAML_CORE_EXPORT
std::string PasswordBox::password_char() const
{
    return Get(PropertyId::of("PasswordBox.PasswordChar")).get<std::string>();
}

VAML_CORE_EXPORT
void PasswordBox::raise_password_changed()
{
    // S485 — the TextChanged routed event (Bubbling, source==this passwordbox).
    // Mirrors the S402/S463 TextBox raise; a real password edit fires it.
    vaml::RoutedEventArgs args;
    args.source = this;
    args.originalSource = this;
    RaiseEvent(vaml::TextChangedEvent(), args);
}

VAML_CORE_EXPORT
bool PasswordBox::handle_key(vaml::KeyCode key, bool isDown, bool shift)
{
    (void)shift; // single-line masked edit: caret nav collapses selection
    if (!isDown)
        return false; // a key-down drives an edit; releases are not edits

    std::string s = password();
    const std::string before = s;

    // Navigation: Left/Right/Home/End move the caret (no text change).
    switch (key)
    {
    case vaml::KeyCode::Left:  caret_ = caret_ > 0 ? caret_ - 1 : 0; return true;
    case vaml::KeyCode::Right: caret_ = std::min(caret_ + 1, s.size()); return true;
    case vaml::KeyCode::Home:  caret_ = 0; return true;
    case vaml::KeyCode::End:   caret_ = s.size(); return true;
    default: break;
    }

    // A printable letter/digit/space inserts at the caret (masked — the stored
    // Password is the real content; only the RENDER is masked bullets).
    auto insert = [&](char c) {
        if (max_length() > 0 && s.size() >= static_cast<std::size_t>(max_length()))
            return; // max-length reached: the insert is a no-op
        s.insert(s.begin() + static_cast<std::ptrdiff_t>(caret_), c);
        caret_ = std::min(caret_ + 1, s.size());
    };
    if (key >= vaml::KeyCode::A && key <= vaml::KeyCode::Z)
    {
        insert(static_cast<char>('a' + (static_cast<int>(key) - static_cast<int>(vaml::KeyCode::A))));
    }
    else if (key >= vaml::KeyCode::Digit0 && key <= vaml::KeyCode::Digit9)
    {
        insert(static_cast<char>('0' + (static_cast<int>(key) - static_cast<int>(vaml::KeyCode::Digit0))));
    }
    else if (key == vaml::KeyCode::Space || key == vaml::KeyCode::TextBoxAccept)
    {
        insert(' ');
    }
    else if (key == vaml::KeyCode::Backspace)
    {
        if (caret_ > 0)
        {
            s.erase(s.begin() + static_cast<std::ptrdiff_t>(caret_ - 1));
            caret_ = std::min(caret_ - 1, s.size());
        }
    }
    else if (key == vaml::KeyCode::Enter)
    {
        // Single-line Accept: consume the key, no text change.
        return true;
    }
    else
    {
        return false; // an unhandled key is not an edit
    }

    (void)Set(PropertyId::of("PasswordBox.Password"), Value::String(s));
    if (password() != before)
        raise_password_changed();
    return true;
}

VAML_CORE_EXPORT
Size PasswordBox::MeasureOverride(const Size& available) const
{
    // The S485 real PasswordBox algorithm: the masked edit field box (a bounded
    // default; the S123 finalizer applies the stretch-vs-wrap). The masked
    // bullets render via the S400 Text path at the classifier (the edit-field
    // fill box is Solid — mirroring TextBox).
    const float w = available.width > 0.0f ? available.width : 120.0f;
    const float h = available.height > 0.0f ? available.height : 20.0f;
    return Size{ std::max(0.0f, w), std::max(0.0f, h) };
}

// --- S485 GroupBox/Expander: header + single content container --------------

namespace {

void register_groupbox_properties()
{
    (void)DependencyProperty::Register(
        "GroupBox.Header", ValueType::String, "GroupBox",
        PropertyMetadata::WithDefault(Value::String(std::string{}), PropertyFlags::None));
    // The S452 property-element grammar resolves `<GroupBox.Content>` against
    // the bare "Content" name; the content child is the S121 visual child list,
    // so no Content DP is registered (the single-content slot is member state,
    // mirroring Window).
}

void ensure_groupbox_properties_registered()
{
    static bool registered = false;
    if (!registered)
    {
        registered = true;
        register_groupbox_properties();
    }
}

} // namespace

VAML_CORE_EXPORT
GroupBox::GroupBox()
{
    ensure_groupbox_properties_registered();
}

VAML_CORE_EXPORT
SetResult GroupBox::set_header(std::string value)
{
    return Set(PropertyId::of("GroupBox.Header"), Value::String(std::move(value)));
}

VAML_CORE_EXPORT
std::string GroupBox::header() const
{
    return Get(PropertyId::of("GroupBox.Header")).get<std::string>();
}

VAML_CORE_EXPORT
void GroupBox::set_content(std::unique_ptr<UIElement> child)
{
    if (!child)
    {
        InvalidateMeasure();
        InvalidateArrange();
        return;
    }
    while (child_count() > 0)
    {
        [[maybe_unused]] std::size_t remaining = remove_child(0);
    }
    content_ = nullptr;
    UIElement* raw = child.get();
    add_child(std::move(child));
    content_ = raw;
    InvalidateMeasure();
    InvalidateArrange();
}

VAML_CORE_EXPORT
UIElement* GroupBox::content() const noexcept
{
    return content_;
}

VAML_CORE_EXPORT
Size GroupBox::MeasureOverride(const Size& available) const
{
    // The S485 real GroupBox: the content child + a header band. Measure the
    // content within the offered box (minus a nominal header band) and report
    // content + header extent.
    vaml::Size inner{ available.width > 0.0f ? available.width : 0.0f,
                      available.height > 0.0f ? std::max(0.0f, available.height - 18.0f) : 0.0f };
    if (content_)
        content_->Measure(inner);
    const float cw = content_ ? content_->desired_size().width : 0.0f;
    const float ch = content_ ? content_->desired_size().height : 0.0f;
    return Size{ std::max(0.0f, cw), std::max(0.0f, ch + 18.0f) };
}

VAML_CORE_EXPORT
void GroupBox::ArrangeOverride(const vaml::Rect& finalSlot)
{
    if (!content_)
        return;
    // The content child sits below the 18px header band; the classifier draws
    // the bordered group box (Solid + Border) at the arranged rect.
    const float headerH = 18.0f;
    const float ch = std::max(0.0f, finalSlot.height - headerH);
    content_->Arrange(vaml::Rect{ finalSlot.x, finalSlot.y + headerH, finalSlot.width, ch });
}

// Expander (header + IsExpanded + expandable content). The expand chevron is
// the S649 continuation (recorded NO-GAP not silent); the container + header +
// IsExpanded are the REAL S485 surface.

namespace {

void register_expander_properties()
{
    (void)DependencyProperty::Register(
        "Expander.Header", ValueType::String, "Expander",
        PropertyMetadata::WithDefault(Value::String(std::string{}), PropertyFlags::None));
    (void)DependencyProperty::Register(
        "Expander.IsExpanded", ValueType::Int32, "Expander",
        PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None));
}

void ensure_expander_properties_registered()
{
    static bool registered = false;
    if (!registered)
    {
        registered = true;
        register_expander_properties();
    }
}

} // namespace

VAML_CORE_EXPORT
Expander::Expander()
{
    ensure_expander_properties_registered();
}

VAML_CORE_EXPORT
SetResult Expander::set_header(std::string value)
{
    return Set(PropertyId::of("Expander.Header"), Value::String(std::move(value)));
}

VAML_CORE_EXPORT
std::string Expander::header() const
{
    return Get(PropertyId::of("Expander.Header")).get<std::string>();
}

VAML_CORE_EXPORT
SetResult Expander::set_is_expanded(std::int32_t value)
{
    const int v = value ? 1 : 0;
    return Set(PropertyId::of("Expander.IsExpanded"), Value::Int32(v));
}

VAML_CORE_EXPORT
bool Expander::is_expanded() const
{
    return Get(PropertyId::of("Expander.IsExpanded")).get<int>() != 0;
}

VAML_CORE_EXPORT
void Expander::set_content(std::unique_ptr<UIElement> child)
{
    if (!child)
    {
        InvalidateMeasure();
        InvalidateArrange();
        return;
    }
    while (child_count() > 0)
    {
        [[maybe_unused]] std::size_t remaining = remove_child(0);
    }
    content_ = nullptr;
    UIElement* raw = child.get();
    add_child(std::move(child));
    content_ = raw;
    InvalidateMeasure();
    InvalidateArrange();
}

VAML_CORE_EXPORT
UIElement* Expander::content() const noexcept
{
    return content_;
}

VAML_CORE_EXPORT
Size Expander::MeasureOverride(const Size& available) const
{
    // The S485 real Expander: when IsExpanded, measure the content child below
    // the header band; when collapsed, only the header (content hidden). The
    // classifier draws the container fill (Solid) at the arranged rect.
    const float innerH = is_expanded() && available.height > 0.0f
                             ? std::max(0.0f, available.height - 24.0f)
                             : 0.0f;
    vaml::Size inner{ available.width > 0.0f ? available.width : 0.0f, innerH };
    if (content_ && is_expanded())
        content_->Measure(inner);
    const float cw = content_ && is_expanded() ? content_->desired_size().width : 0.0f;
    const float ch = content_ && is_expanded() ? content_->desired_size().height : 0.0f;
    const float headerH = is_expanded() ? 24.0f : 24.0f;
    return Size{ std::max(0.0f, cw), std::max(0.0f, ch + headerH) };
}

VAML_CORE_EXPORT
void Expander::ArrangeOverride(const vaml::Rect& finalSlot)
{
    if (!content_ || !is_expanded())
        return; // collapsed: the content is hidden (the classifier draws only the header)
    const float headerH = 24.0f;
    const float ch = std::max(0.0f, finalSlot.height - headerH);
    content_->Arrange(vaml::Rect{ finalSlot.x, finalSlot.y + headerH, finalSlot.width, ch });
}

// --- S485 Separator: a thin divider ----------------------------------------

namespace {

void register_separator_properties()
{
    (void)DependencyProperty::Register(
        "Separator.Thickness", ValueType::Double, "Separator",
        PropertyMetadata::WithDefault(Value::Double(1.0), PropertyFlags::None));
}

void ensure_separator_properties_registered()
{
    static bool registered = false;
    if (!registered)
    {
        registered = true;
        register_separator_properties();
    }
}

} // namespace

VAML_CORE_EXPORT
Separator::Separator()
{
    ensure_separator_properties_registered();
}

VAML_CORE_EXPORT
SetResult Separator::set_thickness(double value)
{
    return Set(PropertyId::of("Separator.Thickness"), Value::Double(value));
}

VAML_CORE_EXPORT
double Separator::thickness() const
{
    return Get(PropertyId::of("Separator.Thickness")).get<double>();
}

VAML_CORE_EXPORT
Size Separator::MeasureOverride(const Size& available) const
{
    // The S485 real Separator: a thin rule — default full width × the thickness
    // (1px). The classifier draws the thin line (Solid).
    const float w = available.width > 0.0f ? available.width : 100.0f;
    const float h = static_cast<float>(thickness() > 0.0 ? thickness() : 1.0);
    return Size{ std::max(0.0f, w), std::max(0.0f, h) };
}

// --- S485 StatusBar: a docking item host (attached StatusBar.Dock) ----------

namespace {

void register_statusbar_properties()
{
    // The attached StatusBar.Dock edge (Left=0 / Right=1), stored on the child
    // via the S043 attached-aware path (mirrors DockPanel.Dock).
    (void)DependencyProperty::Register(
        "StatusBar.Dock", ValueType::Int32, "StatusBar",
        PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None));
}

void ensure_statusbar_properties_registered()
{
    static bool registered = false;
    if (!registered)
    {
        registered = true;
        register_statusbar_properties();
    }
}

} // namespace

VAML_CORE_EXPORT
StatusBar::StatusBar()
{
    ensure_statusbar_properties_registered();
}

VAML_CORE_EXPORT
SetResult StatusBar::set_dock(DependencyObject& obj, std::int32_t value)
{
    return obj.Set(PropertyId::of("StatusBar.Dock"), Value::Int32(value));
}

VAML_CORE_EXPORT
std::int32_t StatusBar::dock(const DependencyObject& obj)
{
    return obj.Get(PropertyId::of("StatusBar.Dock")).get<std::int32_t>();
}

VAML_CORE_EXPORT
Size StatusBar::MeasureOverride(const Size& available) const
{
    // The S485 real StatusBar: a docking host that lays out its children along
    // the bar. Measure each child unbounded; report the max cross extent (the
    // bar's height) + consumed width (left-docked items stack left-to-right,
    // right-docked items stack right-to-left).
    (void)available; // the bar hosts many items; the offer bounds the wrap only
    float width = 0.0f;   // consumed by left-docked items
    float maxH  = 0.0f;   // max child height (the bar's thickness)
    const std::size_t n = child_count();
    for (std::size_t i = 0; i < n; ++i)
    {
        UIElement* child = child_at(i);
        if (child->is_measure_dirty())
            child->Measure(Size::Infinite());
        if (dock(*child) == 0) // Left
            width += child->desired_size().width;
        maxH = std::max(maxH, child->desired_size().height);
    }
    return Size{ std::max(0.0f, width), std::max(0.0f, maxH) };
}

VAML_CORE_EXPORT
void StatusBar::ArrangeOverride(const vaml::Rect& finalSlot)
{
    // Place Left-docked items from the left edge; Right-docked items from the
    // right edge. Each child fills the bar's height (the cross extent).
    float cursorL = finalSlot.x;
    float cursorR = finalSlot.right();
    const std::size_t n = child_count();
    for (std::size_t i = 0; i < n; ++i)
    {
        UIElement* child = child_at(i);
        if (!child->is_arrange_dirty())
            continue;
        if (dock(*child) == 0) // Left — stack left-to-right
        {
            const float cw = child->desired_size().width;
            child->Arrange(vaml::Rect{ cursorL, finalSlot.y, cw, finalSlot.height });
            cursorL += cw;
        }
        else // Right — stack right-to-left
        {
            const float cw = child->desired_size().width;
            cursorR -= cw;
            child->Arrange(vaml::Rect{ cursorR, finalSlot.y, cw, finalSlot.height });
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
    // 11k-1: the S087-invented `Text` name is RETIRED; the authored text leaf is
    // the WPF-true `TextBlock`. Built-in count NET-NEUTRAL (+TextBlock −Text).
    registry.register_element("TextBlock",    [] { return std::make_unique<TextBlock>(); });
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
    // --- Sprint 11c (S470–S473): the DockPanel layout container + the
    // selection controls. All are REAL re-based UIElements resolved at their OWN
    // steps (NO-GAP, no MINIMAL REAL added). DockPanel (S470) 16 -> 17;
    // RadioButton (S471) 17 -> 18; ComboBox (S472) 18 -> 19;
    // ListBox + ListView (S473) 19 -> 21.
    registry.register_element("DockPanel",    [] { return std::make_unique<DockPanel>(); });   // 17
    registry.register_element("RadioButton",  [] { return std::make_unique<RadioButton>(); }); // 18
    registry.register_element("ComboBox",     [] { return std::make_unique<ComboBox>(); });    // 19
    registry.register_element("ListBox",      [] { return std::make_unique<ListBox>(); });     // 20
    registry.register_element("ListView",     [] { return std::make_unique<ListView>(); });    // 21
    // --- Sprint 11d (S480–S486): the core display + input controls. All are
    // REAL re-based UIElements resolved at their OWN steps (NO-GAP, no MINIMAL
    // REAL added). Label (S480) 21 -> 22; Border (S481) 22 -> 23; the shapes
    // Rectangle/Ellipse/Line (S482) 23 -> 26; Image (S483) 26 -> 27;
    // ProgressBar (S484) 27 -> 28; the S485 set (PasswordBox/GroupBox/Expander/
    // Separator/StatusBar) 28 -> 33.
    registry.register_element("Label",          [] { return std::make_unique<Label>(); });       // 22
    registry.register_element("Border",         [] { return std::make_unique<Border>(); });      // 23
    registry.register_element("Rectangle",      [] { return std::make_unique<Rectangle>(); });   // 24
    registry.register_element("Ellipse",        [] { return std::make_unique<Ellipse>(); });     // 25
    registry.register_element("Line",           [] { return std::make_unique<Line>(); });        // 26
    registry.register_element("Image",          [] { return std::make_unique<Image>(); });       // 27
    registry.register_element("ProgressBar",    [] { return std::make_unique<ProgressBar>(); }); // 28
    registry.register_element("PasswordBox",    [] { return std::make_unique<PasswordBox>(); }); // 29
    registry.register_element("GroupBox",       [] { return std::make_unique<GroupBox>(); });    // 30
    registry.register_element("Expander",       [] { return std::make_unique<Expander>(); });    // 31
    registry.register_element("Separator",      [] { return std::make_unique<Separator>(); });   // 32
    registry.register_element("StatusBar",      [] { return std::make_unique<StatusBar>(); });   // 33
    // --- Sprint 11j (GAP CLOSE) — the WPF corpus load surface. UserControl
    // (11j-1) 33 -> 34; Application (11j-6) 34 -> 35; ResourceDictionaryHolder
    // (11j-2) 35 -> 36. All are REAL permanent symbols (no MINIMAL REAL added)
    // resolved at their own steps (NO-GAP).
    registry.register_element("UserControl",   [] { return std::make_unique<UserControl>(); });  // 34
    registry.register_element("Application",   [] { return std::make_unique<Application>(); });  // 35
    // 11j-2: the ResourceDictionary ROOT — a DependencyObject HOLDER owning the
    // built dictionary (WPF-faithful: WPF ResourceDictionary is a plain class
    // that is a legal standalone root; the holder lets our DependencyObject-
    // rooted Load API produce it). Previously ResourceDictionary could NOT be
    // produced via the factory (dedicated build path only) — 11j-2 resolves it
    // as a LOADABLE ROOT. A NESTED `<ResourceDictionary>` (under .Resources /
    // .MergedDictionaries) still uses build_resource_dictionary directly.
    registry.register_element("ResourceDictionary", [] { return std::make_unique<ResourceDictionaryHolder>(); }); // 36

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
