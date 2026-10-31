#pragma once
// VAML built-in element classes — MINIMAL REAL seeds (S087) + the real
// re-based controls (StackPanel, S129; Grid, S131).
//
// The remaining seeds are THIN, PERMANENT, LINKABLE concrete classes deriving
// DependencyObject that give the S087 ElementFactoryRegistry real registered
// type names to produce. They exist because S088 must Resolve "Button" ->
// instance and S091 must build a StackPanel+Button from text BEFORE any real
// control logic is implemented (UIElement = S120, controls = S400+). The first
// control to reach its resolution sprint is `StackPanel` (S129), which is
// RE-BASED onto UIElement at its own resolution step (NO-GAP, not a backfill).
//
// MINIMAL REAL contract (see docs/roadmap.md "Guard Rails"):
//   - Each class is a seed for the REAL control of the same name; it is KEPT and
//     EXTENDED into that control at its resolution sprint. It is NOT a stub
//     (throwaway) — it is a deliberate, declared, forward-resolvable seed.
//   - Each carries a searchable sentinel comment naming its resolution sprint.
//     A seed may ONLY be removed/replaced by the step that explicitly declares
//     its resolution.
//   - No seed is silently extended or backfilled; its real content (members,
//     logic) is authored at its OWN resolution sprint.
//
// Resolution sprints are already declared in docs/roadmap.md (verified):
//     StackPanel -> S129 (RESOLVED — re-based onto UIElement here)
//     Grid       -> S131 (RESOLVED — re-based onto UIElement here)
//     Button      -> S401   Text -> S400
//     TextBox     -> S402   CheckBox   -> S404   Slider -> S403
//     ScrollViewer-> S405   Window     -> S440
//
// Element.obj (Element.cpp) hosts register_builtin_elements() — the startup
// registration call that populates ElementFactoryRegistry::global() once.

#include "vaml/core/DependencyObject.h"
#include "vaml/core/GridLength.h" // GridLength, RowDefinition, ColumnDefinition (S131)
#include "vaml/core/TextLayout.h" // S400 — text_layout_plan / default_text_metric (Measure basis)
#include "vaml/core/UIElement.h"  // StackPanel/Grid's base (S129/S131 re-base — RESOLVED seeds)

#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

namespace vaml {

// Forward declaration of the S325 KeyCode scoped enum (defined in FocusManager.h
// which Element.h cannot include — the dependency cycle direction is Element ->
// UIElement -> FocusManager). A fixed underlying type (uint8_t) makes the scoped
// enum forward-declarable; the TextBox (S402) is the S10 control that consumes
// it on its editing entry point. FocusManager.h remains the single definition.
enum class KeyCode : std::uint8_t;

// Forward declaration of the S500 per-item template (defined in DataTemplate.h,
// which Element.h must NOT include — ListBox only needs the non-owning observer
// pointer to the authored DataTemplate; the full type is used in Element.cpp +
// the reader + the DataTemplate TU).
class DataTemplate;

// Forward declaration of the S500 minimal per-item host (defined in
// ContentPresenter.h, included by Element.cpp where the per-item rendering
// builds it; ListBox declares the type only via the render_items body, so this
// forward declaration is enough for the header).
class ContentPresenter;

// Button — REAL control (S401). RESOLVES the S087 MINIMAL REAL seed at its own
// sprint (NO-GAP, not a backfill): re-based onto UIElement so it participates
// in the visual tree + Measure/Arrange + input hit-testing + routed events.
//
// S401 surface (the S401 title: "template, hover/pressed visual, Click routed
// event, IsEnabled"):
//   - TEMPLATE / VISUAL STATES: a Button carries a ControlTemplate (S364) whose
//     VisualState set is Normal / Hover / Pressed (the templated Button restyle
//     already proven at S365). The DEFAULT implicit Button template (built on
//     S363/S367) has Normal=neutral gray / Hover=lighter / Pressed=darker fills
//     driven through the shared "Background" Color DP (registered by the S364
//     visual-state engine). set_visual_state(name) / visual_state() expose the
//     live swap + the current state.
//   - CLICK (routed event): a Button raises the BUBBLING ClickEvent (S320/S321)
//     when a press+release hit-tests the button itself — the S324 click flow
//     already raises Click on the hit element, and a re-based Button IS a
//     hit-testable UIElement, so a click on a Button automatically raises
//     ClickEvent with source==the Button. `raise_click()` lets a control /
//     markup raise it explicitly (e.g. IsEnabled-gated). `Click="OnClick"`
//     markup wiring (S322) already resolves ClickEvent handlers on real
//     UIElements.
//   - ISENABLED (REAL DP; Bool via Int32, default true): gates the Click
//     routing — an IsEnabled=false Button does NOT raise Click (WPF-faithful),
//     and reports the visual Disabled state (recorded: a disabled visual tint is
//     S401's default; the S205 shader-level disabled fade is the render step).
//     A disabled Button is not pressable (the input marshaller still hit-tests,
//     but raise_click is gated off).
//
// SCOPE (recorded, NOT silent): the FULL templated content tree + the Template
// Binding re-skin of a Button restyled by app markup is the S364/S365 machinery
// (already real); the S401 Button supplies its OWN default implicit template +
// the hover/pressed/disabled visual states. The rendered quad (the default
// fill + the hover/pressed swap at the renderer) is emitted by the vulkan
// classify_merge_element (a Button classifies to MaterialKind::Solid, the
// default Button material); the live per-state brush re-render is the
// [s401-live]/S409_1 path — recorded NO-GAP, not silent.
class VAML_CORE_EXPORT Button final : public UIElement
{
public:
    Button();
    // The pimpl owns the VisualState vector, so the destructor is out-of-line
    // (default in Element.cpp) for the Impl to be complete there.
    ~Button() override;

    // --- IsEnabled (REAL DP; Bool via Int32, default true) ----------------
    // Gates the Click routing: false -> raise_click is a no-op (WPF-faithful).
    [[nodiscard]] SetResult set_is_enabled(bool value);
    [[nodiscard]] bool is_enabled() const;

    // --- Click routed event (S401) -----------------------------------------
    // Raises the BUBBLING ClickEvent with source==this button, UNLESS the
    // button is disabled (is_enabled()==false -> no-op, WPF-faithful). The
    // S324/S323 input flow hit-tests the button and raises Click on it; this
    // accessor lets a control / markup raise it explicitly. A handled return
    // reflects whether any handler stopped the route.
    [[nodiscard]] bool raise_click();

    // --- Visual states (S401 template / hover / pressed) -------------------
    // The WPF-faithful named state: "Normal" / "Hover" / "Pressed" / "Disabled".
    // set_visual_state enters that state (swapping the Background brush color
    // through the S364 visual-state engine against the default implicit
    // template); visual_state() reports the current one (default "Normal").
    // The input flow (hovered/pressed from InputMarshaller) drives the Hover/
    // Pressed transitions; raise_click is IsEnabled-gated.
    [[nodiscard]] SetResult set_visual_state(std::string name);
    [[nodiscard]] std::string visual_state() const;

protected:
    // S401 real Button algorithm: a Button measures at its natural (content)
    // size — it defers to its VisualState/Background for the render fill and
    // reports a bounded default box (the S400_1 nominal 20x16 button slot). The
    // base ArrangeOverride is the no-op (a leaf places nothing); the default
    // fill quad is emitted by the vulkan classifier (MaterialKind::Solid).
    [[nodiscard]] vaml::Size MeasureOverride(const vaml::Size& available) const override;

private:
    // The default implicit Button template's VisualState set (built once, S363/
    // S367). Normal=neutral gray / Hover=lighter / Pressed=darker / Disabled=
    // faded (S205-level fade is the render step). Owned per Button (a real
    // templated Button carries its template).
    struct Impl;            // defined in Element.cpp
    std::unique_ptr<Impl> impl_;
};

// StackPanel — REAL panel control (S129). RESOLVES the S087 MINIMAL REAL
// seed at its own sprint (NO-GAP, not a backfill). Re-based onto UIElement so
// it participates in the visual tree + Measure/Arrange layout.
//
// Orientation rides the Int32 value path (same convention as Visibility /
// alighment enums). The stack algorithm + last-child-stretch live in
// Element.cpp's MeasureOverride/ArrangeOverride (see the S129 header notes).
//
// NO-GAP hand-off (recorded, not silent): S130 (dep:S129) is the dedicated
// exhaustive DesiredSize + stretch-correctness test suite; S131 (Grid) adopts
// this panel Measure/Arrange pattern.
enum class Orientation : std::uint8_t
{
    Vertical   = 0, // children stack top-to-bottom (default)
    Horizontal = 1, // children stack left-to-right
};

class VAML_CORE_EXPORT StackPanel final : public UIElement
{
public:
    StackPanel();

    // --- Orientation (REAL DP; default Vertical, not inherited) -----------
    // Typed round-trip on the Int32 value path, mirroring the alignment +
    // Visibility conventions. Also settable via Set/Get(PropertyId("Orientation")).
    [[nodiscard]] SetResult set_orientation(Orientation o);
    [[nodiscard]] Orientation orientation() const;

protected:
    // S129 real panel algorithm (see Element.cpp): MeasureOverride is
    // orientation-aware (vertical: {max child w, Σ child h}; horizontal:
    // {Σ child w, max child h}) and re-measures ONLY dirty children (S128);
    // ArrangeOverride stacks children and gives the LAST child the remaining
    // track (last-child-stretch), re-arranging ONLY dirty children (S128).
    [[nodiscard]] vaml::Size MeasureOverride(const vaml::Size& available) const override;
    void ArrangeOverride(const vaml::Rect& finalSlot) override;
};

// Grid — REAL panel control (S131). RESOLVES the S087 MINIMAL REAL seed at its
// own sprint (NO-GAP, not a backfill). Re-based onto UIElement (was
// DependencyObject) so it participates in the visual tree + Measure/Arrange.
//
// S131 delivers the row/column DEFINITION model + Absolute/Auto/Star
// resolution + the BASIC no-span cell measure/arrange. Span support
// (RowSpan/ColumnSpan) + the extended two-pass math is S132's NAMED scope;
// the exhaustive star+span suite is S133 — recorded NO-GAP, not silent.
//
// TRACK DEFINITIONS ride the registerable rows/columns vectors (below), so a
// Grid can be declared in VAML as Grid.RowDefinitions / Grid.ColumnDefinitions
// child collections of RowDefinition/ColumnDefinition objects. Children are
// placed into cells via the ATTACHED Grid.Row / Grid.Column DPs.
//
// NO-GAP hand-off (recorded, not silent): S132 = RowSpan/ColumnSpan + the
// extended measure/arrange math (cells consumed by spans, two-pass); S133 =
// exhaustive star-sizing + span test suite.
class VAML_CORE_EXPORT Grid final : public UIElement
{
public:
    Grid();
    // S131: the pimpl owns the row/column definition vectors, so the
    // destructor must be out-of-line (default) for Impl to be complete here.
    ~Grid() override;

    // --- Track definitions (REAL, S131) ----------------------------------
    // Row/column definitions are owned vectors managed by the Grid. Default:
    // one Auto-size row + one Auto-size column (a Grid with no definitions
    // still has a single implicit Auto track). Setters take an owned vector;
    // getters return a const view for inspection.
    void set_row_definitions(std::vector<RowDefinition> rows);
    void set_column_definitions(std::vector<ColumnDefinition> cols);
    [[nodiscard]] const std::vector<RowDefinition>& row_definitions() const noexcept;
    [[nodiscard]] const std::vector<ColumnDefinition>& column_definitions() const noexcept;

    // --- ATTACHED Grid.Row / Grid.Column (REAL DPs, S131) ----------------
    // The cell position of a child. These are ATTACHED properties: the key is
    // owned by Grid but the value is stored on any DependencyObject (the S043
    // attached-aware path). Default 0 (a child with no explicit cell lands in
    // row 0, column 0). Int32 value path; globally registered under owner
    // "Grid" via the S043 attached-aware machinery. Typed statics.
    // NOTE: Grid.Row/Column select the ANCHOR cell; a child whose
    // RowSpan/ColumnSpan (S132) > 1 fills the merged cell starting at the
    // anchor.
    [[nodiscard]] static SetResult set_row(DependencyObject& obj, std::int32_t value);
    [[nodiscard]] static std::int32_t row(const DependencyObject& obj);
    [[nodiscard]] static SetResult set_column(DependencyObject& obj, std::int32_t value);
    [[nodiscard]] static std::int32_t column(const DependencyObject& obj);

    // --- ATTACHED Grid.RowSpan / Grid.ColumnSpan (REAL DPs, S132) --------
    // The number of rows/columns a child CELL SPANS (RowSpan rows × ColumnSpan
    // columns). S132 extends the S131 basic no-span cells with the span
    // dimension ("cells consumed by spans") + the span-aware two-pass math.
    // Same S043 attached-aware model as Grid.Row/Grid.Column (value stored on
    // the child). Default 1 (a child without an explicit span occupies exactly
    // one cell). Spans clamp to ≥ 1 and to grid bounds. Typed statics:
    [[nodiscard]] static SetResult set_row_span(DependencyObject& obj, std::int32_t value);
    [[nodiscard]] static std::int32_t row_span(const DependencyObject& obj);
    [[nodiscard]] static SetResult set_column_span(DependencyObject& obj, std::int32_t value);
    [[nodiscard]] static std::int32_t column_span(const DependencyObject& obj);

protected:
    // S131/S132 real panel algorithm (see Element.cpp): MeasureOverride resolves
    // row/col track lengths (span-aware two-pass, S132) then measures each
    // child into its (merged or single) cell, re-measuring ONLY dirty children
    // (S128); ArrangeOverride places each child into its cell/merged-span rect,
    // re-arranging ONLY dirty children (S128).
    // row/col track lengths then measures each non-spanning child into its
    // cell, re-measuring ONLY dirty children (S128); ArrangeOverride places
    // each cell child at its cell origin + size, re-arranging ONLY dirty
    // children (S128). Basic no-span scope: spans are S132's named step.
    [[nodiscard]] vaml::Size MeasureOverride(const vaml::Size& available) const override;
    void ArrangeOverride(const vaml::Rect& finalSlot) override;

private:
    // S146 (refactor gate): the pimpl is COLLAPSED — the owned row/column
    // definition vectors are now plain by-value members (no Impl heap node, no
    // impl_-> trampoline). A default-constructed Grid has ONE implicit Auto row
    // + ONE implicit Auto column (track definitions are created by the ctor).
    std::vector<RowDefinition>     rows_;
    std::vector<ColumnDefinition>  cols_;
};

// WrapPanel — REAL panel control (S134). Flows children along the wrap axis
// (WrapDirection Horizontal: left-to-right rows; Vertical: top-to-bottom
// columns), WRAPPING to a new row/column when the boundary is exceeded, with
// each row/column ALIGNED (children share a line and align within its extent).
//
// NATURAL-WIDTH MEASURE BASIS: children are measured at NATURAL (unbounded)
// size so the wrap decision uses content-correct extents (the S123 infinity
// rule — a Stretch child given infinite wraps to content, never fabricates a
// size against infinity). At arrange each child is placed into its row/column
// slot = {cursor, rowOrigin, natural flow extent, row extent}, and child->Arrange
// (S124 compute_final_rect) honors the child's OWN alignment + Margin inside
// that slot — a Stretch child fills the row's cross extent while keeping its
// natural flow extent (the "aligned rows" headline).
//
// WrapDirection rides the Int32 value path (reuses the Orientation enum type:
// Vertical=0, Horizontal=1) with DEFAULT HORIZONTAL (WPF-faithful). The
// property name is "WrapDirection" (owner "WrapPanel") — deliberately NOT
// "Orientation", which StackPanel already owns in the global registry (a
// same-name pair under a different owner would be ambiguous to the S091
// dotted-key resolution). The flow/wrap/align algorithm lives in Element.cpp.
class VAML_CORE_EXPORT WrapPanel final : public UIElement
{
public:
    WrapPanel();

    // --- WrapDirection (REAL DP; default Horizontal, not inherited) -------
    // Also settable via Set/Get(PropertyId("WrapDirection")).
    [[nodiscard]] SetResult set_wrap_direction(Orientation o);
    [[nodiscard]] Orientation wrap_direction() const;

protected:
    // S134 real panel algorithm (see Element.cpp): measure children at natural
    // unbounded size, wrap when the flow axis exceeds `available`; DesiredSize
    // = {max row width, Σ row heights} (Horizontal) / {Σ col widths, max col
    // height} (Vertical); arrange lays children into row/column slots honoring
    // each child's own alignment within the row extent. Dirty-respect (S128):
    // re-measure/re-arrange ONLY dirty children.
    [[nodiscard]] vaml::Size MeasureOverride(const vaml::Size& available) const override;
    void ArrangeOverride(const vaml::Rect& finalSlot) override;
};

// Canvas — REAL panel control (S136). Positions each child at an EXPLICIT
// (left, top) coordinate via the ATTACHED Canvas.Left / Canvas.Top DPs, so
// children are placed ABSOLUTELY within the canvas (no panel flow/stacking).
//
// MEASURE BASIS: Canvas never constrains its children — MeasureOverride offers
// every child Size::Infinite() on BOTH axes (the S123 infinity rule, same
// basis as WrapPanel S134), so a child measures at its NATURAL (content)
// extent. DesiredSize = the BOUNDING EXTENT of the arranged children: the
// maximum over children of (effective Left + child.desired.w) on the width
// axis and (effective Top + child.desired.h) on the height axis, clamped >= 0
// (never negative, never NaN).
//
// ARRANGE BASIS: ArrangeOverride places each child at its EXPLICIT origin
// {eff_Left, eff_Top}, where eff_X = is_nan(v) ? 0 : v (a child with no
// explicit coordinate lands at the canvas origin 0,0 — the effective
// WPF-faithful top-left collapse), at the child's measured NATURAL desired
// size via child->Arrange, honoring the child's OWN margin/alignment inside
// that slot through compute_final_rect (S124/S130_1 contract). Children at
// distinct origins do NOT stack/flow — positions are absolute.
//
// NaN contract (introduced explicitly at this step — the S1 value path had no
// NaN convention before S136): NaN on an attached positional axis = "not set"
// -> effective 0. This is part of the product contract, not a stub.
//
// Dirty-respect (S128): MeasureOverride re-measures ONLY is_measure_dirty()
// children; ArrangeOverride re-arranges ONLY is_arrange_dirty() children.
class VAML_CORE_EXPORT Canvas final : public UIElement
{
public:
    Canvas();

    // --- ATTACHED Canvas.Left / Canvas.Top (REAL DPs, S136) --------------
    // The child's explicit (x, y) position within the canvas. ATTACHED
    // properties: the key is owned by Canvas but the value is stored on any
    // DependencyObject (the S043 attached-aware path). Default NaN = "not
    // set" -> a child with no explicit coordinate collapses to the origin
    // (0,0) at layout time. Double value path; globally registered under
    // owner "Canvas" via the S043 attached-aware machinery. Typed statics:
    [[nodiscard]] static SetResult set_left(DependencyObject& obj, double value);
    [[nodiscard]] static double left(const DependencyObject& obj);
    [[nodiscard]] static SetResult set_top(DependencyObject& obj, double value);
    [[nodiscard]] static double top(const DependencyObject& obj);

protected:
    // S136 real panel algorithm (see Element.cpp): MeasureOverride offers every
    // child Size::Infinite() on BOTH axes (natural extent, S123), re-measuring
    // ONLY dirty children (S128); DesiredSize = the bounding extent of arranged
    // children (max over children of eff coordinate + child extent, clamped
    // >= 0, never NaN). ArrangeOverride places each child at its explicit
    // origin {eff_Left, eff_Top} at its measured natural size, honoring the
    // child's own alignment + margin (compute_final_rect, S124/S130_1),
    // re-arranging ONLY dirty children (S128).
    [[nodiscard]] vaml::Size MeasureOverride(const vaml::Size& available) const override;
    void ArrangeOverride(const vaml::Rect& finalSlot) override;
};

// DockPanel — REAL panel control (S470, Sprint 11c). The ONLY missing layout
// container (StackPanel S129 / Grid S131 / WrapPanel S134 / Canvas S136 are
// ALREADY REAL). A WPF-faithful dock-stacking panel: each child is docked to an
// edge (Left/Right/Top/Bottom) in document order, consuming that edge's track,
// and the LAST child fills the remaining interior slot.
//
// WPF-FAITHFUL ALGORITHM (S470):
//   * ATTACHED `DockPanel.Dock` DP — Int32 path (enum Dock { Left=0, Top=1,
//     Right=2, Bottom=3, Fill=4 }), owner "DockPanel", globally registered via
//     the S043 attached-aware machinery. `DockPanel::set_dock/get_dock` typed
//     statics store the dock on ANY DependencyObject (a child). DEFAULT = FILL
//     (a child with no explicit Dock lands in the Fill slot) — WPF-faithful.
//     This RESOLVES the S462-recorded NO-GAP ("DockPanel.Dock float is S470")
//     at its own step.
//   * MeasureOverride: children are offered UNBOUNDED on both axes (the S123
//     infinity rule — a docked child measures at its natural content extent),
//     accumulating the consumed track: Left/Right consume width, Top/Bottom
//     consume height; the LAST child (Fill) takes the REMAINING slot. DesiredSize
//     = Σ consumed tracks. Dirty-respect (S128): re-measure ONLY is_measure_dirty()
//     children.
//   * ArrangeOverride: children are placed at cumulative dock origins, honoring
//     the child's OWN alignment + margin via child->Arrange (S124
//     compute_final_rect). The LAST (Fill) child takes the remaining interior.
//     Dirty-respect: re-arrange ONLY is_arrange_dirty() children.
//
// classify_element/classify_merge_element -> MaterialKind::Solid (the S207/S212
// classifier seam — a DockPanel renders as a solid fill at its arranged box,
// exactly like the other real panels).
class VAML_CORE_EXPORT DockPanel final : public UIElement
{
public:
    DockPanel();

    // --- ATTACHED DockPanel.Dock (REAL DP, S470) --------------------------
    // The child's dock edge. The key is owned by DockPanel but the value is
    // stored on any DependencyObject (the S043 attached-aware path). Default
    // Fill (a child with no explicit Dock is the last-child fill). Typed statics:
    [[nodiscard]] static SetResult set_dock(DependencyObject& obj, std::int32_t value);
    [[nodiscard]] static std::int32_t dock(const DependencyObject& obj);

protected:
    // S470 real panel algorithm (see Element.cpp): MeasureOverride measures
    // children unbounded + accumulates the consumed dock tracks; the last
    // (Fill) child takes the remaining slot. ArrangeOverride places children
    // at cumulative dock origins. Dirty-respect (S128).
    [[nodiscard]] vaml::Size MeasureOverride(const vaml::Size& available) const override;
    void ArrangeOverride(const vaml::Rect& finalSlot) override;
};

// The Dock edge enum (S470). Carried as Int32 via the DockPanel.Dock DP path.
enum class Dock : std::uint8_t
{
    Left   = 0,
    Top    = 1,
    Right  = 2,
    Bottom = 3,
    Fill   = 4,
};

// TextBlock — REAL control (S400). 11k-1 (dep:S400) RETIRES the invented `Text`
// name (S087) and re-bases the authored text leaf onto the WPF-true `TextBlock`
// (TextBlock : FrameworkElement — NOT a Control; in VAML the framework-element
// branch IS UIElement, so TextBlock : UIElement is the faithful mapping). The
// semantic content carries over unchanged under the correct name: re-based onto
// UIElement so it participates in the visual tree + Measure/Arrange layout, and
// supplies a REAL per-control material (MaterialKind::Text via the S400
// classifier) so the S207 encoder + the vulkan build_text_glyph_items emit SDF
// glyph quads. S167_1 corrective convention: the S400 Text history is
// SUPERSEDED BY 11k-1 (TextBlock), preserved, never edited to hide the error.
//
// WPF-FAITHFUL surface (title alignment, review-confirmed):
//   - Text   (String, default "") — the content. `<TextBlock>label</TextBlock>`
//     inline content lands here via the reader's TextBlock content routing
//     (WPF IAddChild behavior, 11k-1).
//   - FontSize (Double, default 16) — glyph em size.
//   - FontWeight (Int32, default Normal=0 — reuses the S206 convention).
//   - Foreground (BrushId, default 0 = a resolved default opaque black) — the
//     WPF-faithful "foreground is a BRUSH" read: WPF TextBlock.Foreground is a
//     Brush (TextElement.ForegroundProperty.AddOwner). A BrushId maps to a
//     registered brush via the S364 SolidColorBrush; an unresolvable id falls
//     back to opaque black + reports through the S250_1 diagnostics channel
//     (NO silent — mirrors binding diagnostics). ADR-011 held (render stays
//     vulkan; the Brush itself is core data).
//
// MEASURE/ARRANGE (the S123/S127 seams): MeasureOverride runs text_layout_plan
// with default_text_metric (a fontSize-based estimate, device-free) so the
// TextBlock measures to its WRAPPED line extent; the real per-glyph advance +
// UV data come from the vulkan SDF atlas at RENDER time (build_text_glyph_items),
// where the wrap is RE-COMPUTED with the REAL advances — the Measure basis and
// the render basis use the same wrap RULE, so the box they occupy matches.
// NO-GAP: real atlas-driven metrics + glyph UVs + the Text material are
// authored at the renderer (S400 build_text_glyph_items), NOT silently
// backfilled here. The TextBlock font/padding DP-owner split (its own DPs, not
// Control's) is 11k-8's scope — not pre-empted here.
// [MINIMAL REAL] TextBlock — RESOLVED BY 11k-1 (S087 seed → TextBlock; S400 real
// content re-based). Do NOT extend silently; do NOT backfill.
class VAML_CORE_EXPORT TextBlock final : public UIElement
{
public:
    TextBlock();
    [[nodiscard]] SetResult set_text(std::string value);
    [[nodiscard]] std::string text() const;

    // --- FontSize (REAL DP; Double, default 16, validated >= 0) ----------
    [[nodiscard]] SetResult set_font_size(double value);
    [[nodiscard]] double font_size() const;

    // --- FontWeight (REAL DP; Int32, default Normal=0) -------------------
    // Reuses the S206 glyph convention (Normal=0). Not authoritative yet — the
    // embedded font is single-weight; the DP is the WPF-faithful surface the
    // future multi-weight font resolves. Recorded, not silent.
    [[nodiscard]] SetResult set_font_weight(std::int32_t value);
    [[nodiscard]] std::int32_t font_weight() const;

    // --- Foreground (REAL DP; BrushId, default 0) -------------------------
    // The WPF-faithful brush read (see the header note). Map a BrushId to a
    // SolidColorBrush's Color via resolve_foreground_brush; an unresolved id
    // yields opaque black (the default) + a diagnostic.
    [[nodiscard]] SetResult set_foreground(std::int32_t brushId);
    [[nodiscard]] std::int32_t foreground() const;

protected:
    // S400 real TextBlock algorithm (see Element.cpp): MeasureOverride runs the
    // S400 text_layout_plan (default_text_metric basis, device-free) and returns
    // the wrapped extent {measuredWidth, measuredHeight}. ArrangeOverride is the
    // base no-op (a leaf places nothing). The render-time glyph emitter
    // (build_text_glyph_items, vulkan) re-runs the same wrap rule with the REAL
    // atlas advances.
    [[nodiscard]] vaml::Size MeasureOverride(const vaml::Size& available) const override;
};

// S400_1 (dep:S400) — the REAL Icon control (per ADR-015). Re-based onto
// UIElement so it participates in the visual tree + Measure/Arrange. The icon
// asset itself (an Object resource — the icon asset reference produced through
// the S360_1 ResourceDictionary + an owned asset object) is RESOLVED from the
// DPs; the SDF/atlas quad that renders it is the vulkan side (S400_1 render).
// This control owns the DECISION surface (the DPs + the resolution contract);
// the render emits the quad via the cached-icon provider + best-fit (recorded
// NO-GAP — see below). WPF's IconBase/ContentControl keep the layout box; here
// Width/Height/Stretch drive the final box via the S122 alignment + S123
// measure contract.
//
// DP SURFACE (the S400_1 title, recorded in the plan):
//   - IconKey   (String, default "") — the strong resource key THE S360_1 walk
//                resolves (fetch via resolve_icon). NEVER a path/URI/script
//                (ADR-015 structural rule).
//   - Source    (String, default "") — an optional DIRECT BYTE-asset reference
//                (a packaged icon asset the app registered under this key path;
//                mirrors Source as the raw asset slot). When both are set,
//                IconKey wins (the resource dictionary is the ADR-015 authority).
//   - TintMode  (Int32, default Preserve=0) — {Preserve | Monochrome}: map to
//                the S205_1 Monochrome UI shader kind (Mask) vs the multicolor
//                (Preserve) sample. See TintMode enum below.
//   - Foreground (BrushId, default 0) — resolves through the S364 brush
//                registry (WPF-faithful brush read, mirrors Text). Used as the
//                Monochrome tint color.
//   - Width / Height (Double, default 0 = auto) — the icon's LOGICAL box on the
//                axis; 0 = auto (the authored asset's natural size).
//   - Stretch   (Int32, default None=0) — how the rendered asset fits its box:
//                { None, Fill, Uniform, UniformToFill } (the S213_1 best-fit +
//                the box layout). Carried as Int32 (the S207 Int32-value-path
//                convention). Scope note: the IMAGE aspect-fit math that maps
//                Stretch -> the quad's physical box + the S213_1 best-fit
//                CROSS-REFERENCE is the render step; this control carries the DP.
//
// TintMode enum (the title's {Preserve | Monochrome} — the S400_1 identity,
// recorded NO-GAP in S205_1: TintMode=Preserve samples the multicolor texture
// via the existing quad/SDF-text kinds; TintMode=Monochrome consumes
// UiShaderKind::Monochrome).
enum class IconTintMode : std::uint8_t
{
    Preserve  = 0, // sample the asset's FULL COLOR (multicolor texture)
    Monochrome = 1, // single-channel alpha-mask, tinted by Foreground in-shader
};

// The icon STRETCH enum (the S400_1 title's Stretch — how the asset fits its
// box). Carried as Int32 via the set_stretch(int32) DP path.
enum class IconStretch : std::uint8_t
{
    None         = 0, // natural size (no scale) — the S213_1 "none" policy parity
    Fill         = 1, // stretch to the box (both axes)
    Uniform      = 2, // scale to fit, preserving aspect (letterbox)
    UniformToFill = 3, // scale to cover, preserving aspect (crop)
};

// Icon — the REAL S400_1 control (per ADR-015). The DP surface + the resolution
// contract live here (core, ADR-011 control logic); the RENDER side (the quad
// UVs through the cached icon provider + the best-fit + the fallback glyph) is
// the vulkan/presentation step, consumed at the classifier. NO-GAP recorded
// (not silent):
//   - IconKey -> resolve_icon (S360_1) -> the icon asset (S202_1 raster).
//   - The S213_1 DPI best-fit (logical size × last_framebuffer_scale) + the
//     cached IconBestFitCache decision + the monochrome sample = the RENDER
//     step this control feeds with its DPs.
//   - The FALLBACK GLYPH + structured diagnostic for a missing/unsupported/
//     out-of-set asset = the render/classifier step (mirrors binding
//     diagnostics via the S250_1 channel; NEVER silent, NEVER a crash).
class VAML_CORE_EXPORT Icon final : public UIElement
{
public:
    Icon();

    // --- IconKey (REAL DP; String, default "") -----------------------------
    [[nodiscard]] SetResult set_icon_key(std::string value);
    [[nodiscard]] std::string icon_key() const;

    // --- Source (REAL DP; String, default "") ------------------------------
    [[nodiscard]] SetResult set_source(std::string value);
    [[nodiscard]] std::string source() const;

    // --- TintMode (REAL DP; Int32, default Preserve=0) ---------------------
    [[nodiscard]] SetResult set_tint_mode(std::int32_t value);
    [[nodiscard]] std::int32_t tint_mode() const;

    // --- Foreground (REAL DP; BrushId, default 0) --------------------------
    // The WPF-faithful brush read (mirrors Text). The Monochrome tint color.
    [[nodiscard]] SetResult set_foreground(std::int32_t brushId);
    [[nodiscard]] std::int32_t foreground() const;

    // --- Width (REAL DP; Double, default 0 = auto) -------------------------
    [[nodiscard]] SetResult set_width(double value);
    [[nodiscard]] double width() const;

    // --- Height (REAL DP; Double, default 0 = auto) ------------------------
    [[nodiscard]] SetResult set_height(double value);
    [[nodiscard]] double height() const;

    // --- Stretch (REAL DP; Int32, default None=0) --------------------------
    [[nodiscard]] SetResult set_stretch(std::int32_t value);
    [[nodiscard]] std::int32_t stretch() const;

protected:
    // S400_1 real Icon algorithm (see Element.cpp): MeasureOverride honors the
    // Width/Height DPs (0 = the authored natural size when the asset resolves;
    // a bounded offer wraps to the DP extent or the natural size). The base
    // ArrangeOverride is the no-op (a leaf places nothing); the render quad is
    // emitted by the vulkan classifier.
    [[nodiscard]] vaml::Size MeasureOverride(const vaml::Size& available) const override;
};

// TextBox — REAL control (S402). RESOLVES the S087 MINIMAL REAL seed at its own
// sprint (NO-GAP, not a backfill): re-based onto UIElement so it participates
// in the visual tree + Measure/Arrange + hit-testing + focus + routed key
// events.
//
// S402 surface (the S402 title: "single+multi line, caret, editing, two-way
// binding, selection, focus"):
//   - Text (REAL DP; String, default "") — the edited content.
//   - CARET + SELECTION: `caret` (the insertion index) + `selectionStart`/
//     `selectionLength` give the WPF-style selection model; `move_caret`/
//     `extend_selection` and the key-driven editing maintain them. A caret is
//     always in [0, len]; selection [selectionStart, selectionStart +
//     selectionLength) clamps to the content.
//   - EDITING (key-driven): the routed key events (S325 KeyDownEvent on the
//     FOCUSED element) drive insert (printable -> insert at the caret, replacing
//     a selection), Backspace, Delete, Left/Right/Home/End caret movement,
//     Enter (accepts + raises an "editing accepted" signal when single-line).
//     The TextBox exposes `handle_key(KeyCode, isDown)` so a control / the
//     FocusManager routed key handler drives editing deterministically
//     (headless-testable). The InputMarshaller's key events raise on the
//     focused element; the TextBox is focusable (S325).
//   - TWO-WAY BINDING (S244): the target->source write-back of a TwoWay binding
//     already reacts to a REAL change on the Text DP (the S041 target-changed
//     subscription S244 arms); an edit landing on Text DP via handle_key is a
//     real change, so a TextBox.Text TwoWay binding writes the edit back to the
//     source VM. The explicit text edit -> DP change -> write-back pipeline is
//     the S244 wire (recorded — the TextBox does NOT invent a second binding
//     channel).
//   - FOCUS (S325): the TextBox is focusable, so the FocusManager tab-order
//     reaches it and routed key events deliver to it.
//   - MULTI-LINE: `AcceptsReturn` (Bool via Int32, default false). When true,
//     an Enter inserts a newline (the S400 wrap engine lays out multiple lines)
//     instead of accepting; when false (single-line) an Enter accepts. The
//     multi-line MEASURE is the S400 text_layout_plan wrap (the text box grows
//     to its content lines) — recorded NO-GAP: the full multiline caret
//     vertical navigation is a [s402-live]/later refinement, the S402 core
//     delivers the single+multi-line editing + the wrap measure.
//
// SCOPE (recorded, NOT silent): the rendered glyphs of the typed text through
// the SDF/atlas path is S409_1 (the viewer renders the Text content via the
// generic renderer); the S402 classifier emits a Solid-fill box for the TextBox
// field (the rendered edit field outline) with the text content deferred to the
// S400/S409_1 glyph path — recorded NO-GAP not silent.
class VAML_CORE_EXPORT TextBox final : public UIElement
{
public:
    TextBox();

    // --- Text (REAL DP; String, default "") -------------------------------
    // The edited content. A Set is a real DP change (the S244 two-way write-back
    // arm reacts to a target change). Editing (handle_key) mutates it + fires a
    // real change.
    [[nodiscard]] SetResult set_text(std::string value);
    [[nodiscard]] std::string text() const;

    // --- AcceptsReturn (REAL DP; Bool via Int32, default false) ------------
    // true = multi-line (Enter inserts a newline); false = single-line (Enter
    // accepts).
    [[nodiscard]] SetResult set_accepts_return(bool value);
    [[nodiscard]] bool accepts_return() const;

    // --- Caret (REAL DP-ish accessor; index in [0, len]) -------------------
    // The insertion caret position. Reading reflects the current caret; setting
    // clamps to [0, text().size()] + collapses any selection.
    [[nodiscard]] std::size_t caret() const noexcept;
    void set_caret(std::size_t pos);

    // --- Selection (WPF-style; [selectionStart, start+length)) -------------
    [[nodiscard]] std::size_t selection_start() const noexcept;
    [[nodiscard]] std::size_t selection_length() const noexcept;

    // --- Editing (key-driven; headless-testable) ---------------------------
    // The single edit entry point the FocusManager/routed key events call:
    //   - printable letters/digits (KeyCode::A..Z, Digit0..Digit9) INSERT at
    //     the caret (replacing a selection) + advance the caret;
    //   - KeyCode::Enter: multi-line (AcceptsReturn) inserts a newline; single-
    //     line ACCEPTS (returns true = accepted);              \
    //   - Backspace (key 0 non-printable marker) deletes the char left of the
    //     caret / a selected range;
    //   - Left/Right/Home/End move the caret (shift extends the selection);
    // Returns false when the key was NOT consumed as an edit (e.g. an
    // unhandled navigational key). Does not throw.
    [[nodiscard]] bool handle_key(vaml::KeyCode key, bool isDown, bool shift = false);

    // True when an AcceptsReturn=false Enter accepted the edit (the "editing
    // accepted" signal a handler can observe). Reset on the next edit.
    [[nodiscard]] bool accepted() const noexcept { return accepted_; }

    // --- S463 TextChanged event -------------------------------------------
    // Raises the BUBBLING "TextChanged" routed event (source==this textbox) on a
    // REAL edit commit (handle_key that mutated the Text DP). The authored
    // `TextChanged="..."` markup (wired via BindEvents) fires on a keystroke.
    void raise_text_changed();

    // --- Selection programmatic --------------------------------------------
    void select_all();
    void clear_selection();

protected:
    // S402 real TextBox algorithm: MeasureOverride wraps the Text content at
    // `available` via the S400 text_layout_plan (multi-line growth when
    // AcceptsReturn) and reports a bounded default box; the caret/selection
    // dictate the edit model. The base ArrangeOverride is the no-op.
    [[nodiscard]] vaml::Size MeasureOverride(const vaml::Size& available) const override;

private:
    // The caret + selection (inline member state — a small, real model).
    std::size_t caret_    = 0;
    std::size_t selStart_ = 0;
    std::size_t selLen_   = 0;
    bool        accepted_ = false;

    // Deletes the currently-selected range (returns the range length removed)
    // and collapses the caret to the selection start; used by insert/delete.
    std::size_t delete_selection(std::string& s);
};

// CheckBox — REAL control (S404). RESOLVES the S087 MINIMAL REAL seed at its own
// sprint (NO-GAP, not a backfill): re-based onto UIElement so it participates
// in the visual tree + hit-testing + routed events.
//
// S404 surface (the S404 title: "toggle + IsChecked two-way binding"):
//   - IsChecked (REAL DP; INULLABLE bool, the WPF-FAITHFUL third state —
//     `bool?` confirmed from E:\GitRepos\wpf ToggleButton.IsCheckedProperty —
//     carried on the Int32 payload as {0=unchecked, 1=checked, 2=indeterminate}):
//     the checked state, TWO-WAY BOUND (S244; a toggle writes the new state
//     back to the VM).
//   - IsThreeState (REAL DP; Bool via Int32, default false): when true the
//     toggle CYCLES checked -> indeterminate -> unchecked (WPF on-toggle cycle);
//     when false it toggles checked <-> unchecked (indeterminate unreachable).
//   - TOGGLE: a hit/release on the CheckBox (or a programmatic `toggle()`) calls
//     the WPF on-toggle cycle (source-confirmed): true+IsThreeState->null,
//     true+!IsThreeState->false, false->true, null->false.
//   - The rendered check-box glyph (a Solid box with the check/indeterminate
//     marker) is the vulkan classifier; the toggle + IsChecked + two-way binding
//     are the S404 real algorithm (core, headless-tested).
//
// SCOPE (recorded NOT silent): the rendered check glyph (the Solid box + the
// check marker) is the vulkan classifier (deferred to S409_1 for the live draw);
// the IsChecked toggle + two-way binding + tri-state are the S404 real algorithm.
class VAML_CORE_EXPORT CheckBox final : public UIElement
{
public:
    CheckBox();

    // The nullable bool surface (the WPF-faithful third state). 0=unchecked,
    // 1=checked, 2=indeterminate. set_checked accepts any of the three;
    // is_checked() returns the state.
    [[nodiscard]] SetResult set_checked(std::int32_t state); // 0/1/2
    [[nodiscard]] std::int32_t checked() const;              // 0/1/2

    // IsThreeState (real DP; Bool, default false) — toggles on/off the third
    // (indeterminate) state.
    [[nodiscard]] SetResult set_is_three_state(bool value);
    [[nodiscard]] bool is_three_state() const;

    // --- Toggle (S404) ------------------------------------------------------
    // The WPF on-toggle cycle (source-confirmed from ToggleButton.cs):
    //   IsThreeState==true : checked(1) -> indeterminate(2) -> unchecked(0) -> checked(1)
    //   IsThreeState==false: checked(1) <-> unchecked(0) (indeterminate unreachable)
    // A real change raises the BUBBLING "CheckedChanged" routed event +
    // fires the checked_changed Signal + writes back through the S244 two-way
    // IsChecked binding. Returns the new state.
    [[nodiscard]] std::int32_t toggle();

    // --- Value-changed notification ----------------------------------------
    [[nodiscard]] vaml::Signal<void()>& checked_changed() { return onCheckedChanged_; }

protected:
    // S404 real CheckBox algorithm: the default check-box slot box; the rendered
    // glyph is the vulkan classifier. The S123 finalizer applies the rule.
    [[nodiscard]] vaml::Size MeasureOverride(const vaml::Size& available) const override;

private:
    vaml::Signal<void()> onCheckedChanged_;
    void apply_change(std::int32_t newState);
};

// [MINIMAL REAL] Element CheckBox — RESOLVED BY S404 (Sprint 10). Do NOT extend silently; do NOT backfill.

// RadioButton — REAL control (S471, Sprint 11c). RESOLVES the S087 MINIMAL
// REAL seed at its own sprint (NO-GAP, not a backfill): re-based onto UIElement
// so it participates in the visual tree + hit-testing + routed events.
//
// S471 surface (the S471 title: "IsChecked + group semantics + Checked/Unchecked"):
//   - IsChecked (REAL DP; Bool via Int32, default false): the checked state.
//   - GROUP SEMANTICS (the honest single-parent model): a GROUPLESS RadioButton
//     (no GroupName) excludes ONLY its sibling RadioButtons under the SAME
//     visual parent. `set_checked(true)` on a button CLEARS every sibling
//     RadioButton's IsChecked (exclusive within the parent) — WPF-faithful for
//     the common single-group case. True `GroupName` string grouping is
//     recorded NO-GAP to a later step (not fabricated here).
//   - ROUTED Checked/Unchecked events (the Sprint-8 RoutedEvent engine):
//     checking raises the BUBBLING "Checked" event, unchecking raises
//     "Unchecked" (source==this), on a REAL check/uncheck.
//   - The rendered check-glyph (a Solid fill on the shared Background brush,
//     mirroring the S401 Button default template via the S364 visual-state
//     engine) is the vulkan classifier; the IsChecked + exclusive-group +
//     checked-changed are the S471 real algorithm (core, headless-tested).
class VAML_CORE_EXPORT RadioButton final : public UIElement
{
public:
    RadioButton();

    // --- IsChecked (REAL DP; Bool via Int32, default false) ----------------
    // Checking clears all SIBLING RadioButtons under the same visual parent
    // (exclusive group). A real change raises Checked/Unchecked + fires the
    // checked_changed Signal.
    [[nodiscard]] SetResult set_checked(bool value);
    [[nodiscard]] bool is_checked() const;

    // --- Checked/Unchecked routed events -----------------------------------
    [[nodiscard]] vaml::Signal<void()>& checked_changed() { return onCheckedChanged_; }

protected:
    // S471 real RadioButton algorithm: the default check-box slot box; the
    // rendered glyph is the vulkan classifier.
    [[nodiscard]] vaml::Size MeasureOverride(const vaml::Size& available) const override;

private:
    vaml::Signal<void()> onCheckedChanged_;
    void apply_change(bool newState);
};

// Slider — REAL control (S403). RESOLVES the S087 MINIMAL REAL seed at its own
// sprint (NO-GAP, not a backfill): re-based onto UIElement so it participates
// in the visual tree + hit-testing + arranged box.
//
// S403 surface (the S403 title: "value binding, thumb drag, routed change
// event"):
//   - Value (REAL DP; Double, default 0) — the slider's value, CLAMPED to
//     [Minimum, Maximum] on Set. This is the bound Value (S243/244 two-way
//     binding arm reacts to a real Value change — a slider drag writes the new
//     Value back to the VM).
//   - Minimum / Maximum (REAL DPs; Double, default 0 / 100) — the range.
//   - VALUE CHANGED (routed event): a real Value change raises the BUBBLING
//     "ValueChanged" routed event with source==this AND fires the `value_changed`
//     Signal (the S403 "routed change event"). The two-way binding (S244) also
//     writes a drag's new Value to the VM.
//   - THUMB DRAG (S323 input): `set_value_from_ratio(ratio)` maps a [0,1] drag
//     ratio (the thumb's horizontal position along the track, driven by the
//     input marshaller's mouse-drag on a hit Slider) to Value =
//     Minimum + ratio*(Maximum-Minimum), then raises the change. The mouse-drag
//     to a thumb is the S323 hit-test + drag flow landing on a Slider; the
//     ratio -> Value mapping + the change event are the S403 real algorithm
//     (headless-testable). The full pixel-track-to-thumb drag rendering is the
//     [s403-live]/S409_1 path — recorded NO-GAP, not silent.
//   - MeasureOverride: the default Slider box (its nominal track slot), the
//     S123 finalizer applies the stretch-vs-wrap rule. The rendered track+thumb
//     is the vulkan classifier (Solid), deferred to S409_1 for the live draw.
//
// SCOPE (recorded, NOT silent): the rendered slider track + thumb visuals
// through the classifier are the S409_1 viewer growth; the S403 classifier
// emits a Solid track box. The Value/range/change + two-way binding are the
// S403 real algorithm (core, headless-tested).
class VAML_CORE_EXPORT Slider final : public UIElement
{
public:
    Slider();

    // --- Value / Minimum / Maximum (REAL DPs; Double) ----------------------
    // Value clamps to [Minimum, Maximum] on Set (never escapes the range). The
    // bound Value (S244 two-way arm reacts to a real change).
    [[nodiscard]] SetResult set_value(double v);
    [[nodiscard]] double value() const;
    [[nodiscard]] SetResult set_minimum(double v);
    [[nodiscard]] double minimum() const;
    [[nodiscard]] SetResult set_maximum(double v);
    [[nodiscard]] double maximum() const;

    // --- Value-changed notification ----------------------------------------
    // Every real Value change raises the BUBBLING "ValueChanged" routed event
    // (source==this) + fires this Signal (the S403 "routed change event").
    // Subscribe here to observe every change (returns the Signal).
    [[nodiscard]] vaml::Signal<void()>& value_changed() { return onValueChanged_; }

    // --- Thumb drag (S323 input) -------------------------------------------
    // Maps a [0,1] drag ratio along the track to Value (clamped + min/max-stretched)
    // + raises the change. Returns the new Value. A ratio outside [0,1] clamps.
    [[nodiscard]] double set_value_from_ratio(double ratio);

protected:
    // S403 real Slider algorithm: the default track box at the arranged extent;
    // the S123 finalizer applies the stretch-vs-wrap rule. The rendered track +
    // thumb is the vulkan classifier (deferred to S409_1 for the live draw).
    [[nodiscard]] vaml::Size MeasureOverride(const vaml::Size& available) const override;

private:
    vaml::Signal<void()> onValueChanged_;
    void apply_change(double newValue);
};

// ScrollViewer — REAL control (S405). RESOLVES the S087 MINIMAL REAL seed at its
// own sprint (NO-GAP, not a backfill): re-based onto UIElement so it
// participates in the visual tree + Measure/Arrange + clipping + input.
//
// S405 surface (the S405 title: "scroll + clipping + wheel/scrollbar"):
//   - CONTENT OFFSET: `set_vertical_offset` / `set_horizontal_offset` (Double) +
//     the scrollable extent (`scrollable_height()` = content height - viewport
//     height, >= 0). The offset CLAMPS to [0, scrollable].
//   - CLIPPING (S124 clip_to_bounds): a ScrollViewer clips its arranged box; a
//     child's content beyond the viewport is CLIPPED. The headless proof clips
//     the content rect against the viewport via the offset.
//   - WHEEL: `scroll_by_vertical(delta)` — a wheel event (the S323 input wheel)
//     scrolls the content vertically (a positive delta scrolls down); returns
//     the new offset + raises the "ScrollChanged" routed event + the
//     scroll_changed Signal on a real change. The mouse-wheel to a ScrollViewer
//     is the S323 input flow; the offset math is the S405 real algorithm
//     (headless-testable). The on-screen scrollbar is the S409_1 viewer draw
//     (recorded NO-GAP not silent; the S405 classifier emits the viewport clip
//     box).
//   - MEASURE: a ScrollViewer measures its content; the VIEWPORT is the
//     arranged box, and the content beyond it is scrollable (recorded — a
//     ScrollViewer reports the viewport as its arranged extent and clips the
//     content; the full arrange-scroll coupling to a child's layout is S409_1
//     viewer, the S405 core delivers the offset/clip/wheel model).
class VAML_CORE_EXPORT ScrollViewer final : public UIElement
{
public:
    ScrollViewer();

    // --- Content offset (Double; clamps to [0, scrollable]) ----------------
    [[nodiscard]] SetResult set_vertical_offset(double v);
    [[nodiscard]] double vertical_offset() const;
    [[nodiscard]] SetResult set_horizontal_offset(double v);
    [[nodiscard]] double horizontal_offset() const;

    // --- Scrollable extent -------------------------------------------------
    // contentHeight is the arranged content measure; scrollable_height() =
    // max(0, contentHeight - viewport height). The viewport is the arranged box.
    void set_content_height(double h); // returns void (the content measure is not a DP result)
    [[nodiscard]] double content_height() const;
    [[nodiscard]] double viewport_height() const; // the arranged box height (clamped >= 0)
    [[nodiscard]] double scrollable_height() const;

    // --- Wheel scroll (S323 input) -----------------------------------------
    // scrolls content vertically by `delta` (positive = down); clamps +
    // raises "ScrollChanged" on a real change. Returns the new offset.
    [[nodiscard]] double scroll_by_vertical(double delta);

    // --- Clipping (S124) ----------------------------------------------------
    // The VISIBLE content rect: clips the child content rect (given its top-left
    // origin within the viewport) against the arranged viewport, honoring the
    // current offset. Headless-testable clipping proof.
    [[nodiscard]] vaml::Rect clip_content(const vaml::Rect& content) const;

    // --- Value-changed notification ----------------------------------------
    [[nodiscard]] vaml::Signal<void()>& scroll_changed() { return onScrollChanged_; }

protected:
    // S405 real ScrollViewer algorithm: MeasureOverride reports the viewport
    // (the arranged box); the S123 finalizer applies the rule. The rendered
    // scrollbar + clip is the vulkan classifier (deferred to S409_1).
    [[nodiscard]] vaml::Size MeasureOverride(const vaml::Size& available) const override;

private:
    double vertical_   = 0.0;
    double horizontal_ = 0.0;
    double contentH_   = 0.0;
    vaml::Signal<void()> onScrollChanged_;
    void apply_offset(double newV, double newH);
};

// ComboBox — REAL control (S472, Sprint 11c). RESOLVES the S087 MINIMAL REAL
// seed at its own sprint (NO-GAP, not a backfill): re-based onto UIElement so
// it participates in the visual tree + hit-testing + routed events.
//
// S472 surface (the S472 title: "ItemsSource/Items + SelectionChanged +
// dropdown interaction"):
//   - Items / ItemsSource (REAL DPs; String owned items, the WPF-free owned
//     item container): the combo's selectable items. Items is a literal (the
//     authored comma/space list); ItemsSource is the app-supplied source
//     string. Items is the display source when set.
//   - SelectedIndex / SelectedItem (REAL DPs): the selection. SelectedIndex
//     defaults −1 (nothing selected); set + round-trip; a malformed index
//     clamps and is readable (never a throw).
//   - SELECTIONCHANGED (real routed event): a real selection change raises the
//     BUBBLING "SelectionChanged" event (source==this) + fires the
//     selection_changed Signal.
//   - DROPDOWN INTERACTION (the minimal interactive surface): `is_open`
//     (expand) + `open(dropdown=true)` / `select_item(display)` — the S8 input
//     + the S364 template compose here. select_item(display) selects the item
//     whose display matches + raises SelectionChanged.
//   - classify_merge_element -> MaterialKind::Solid (an EditField-style fill
//     box with a `→` arrow indicator via the S205 Solid path). The editable /
//     fully-styled chrome is the S649-list continuation, RECORDED NO-GAP (not
//     fabricated); the ComboBox DP surface + selection + change are REAL.
class VAML_CORE_EXPORT ComboBox final : public UIElement
{
public:
    ComboBox();

    // --- Items / ItemsSource (REAL DPs; String) ---------------------------
    // Items is the literal/list source; ItemsSource the app-supplied string.
    // set_items also refreshes the display list.
    [[nodiscard]] SetResult set_items(std::string value);
    [[nodiscard]] std::string items() const;
    [[nodiscard]] SetResult set_items_source(std::string value);
    [[nodiscard]] std::string items_source() const;

    // --- SelectedIndex / SelectedItem (REAL DPs; Int32 / String) -----------
    [[nodiscard]] SetResult set_selected_index(std::int32_t index);
    [[nodiscard]] std::int32_t selected_index() const;
    [[nodiscard]] std::string selected_item() const;

    // --- Dropdown interaction (S472) ---------------------------------------
    [[nodiscard]] SetResult set_is_open(bool value);
    [[nodiscard]] bool is_open() const;
    // Opens or closes the dropdown. `open(true)` opens; `open(false)` closes.
    [[nodiscard]] bool open(bool dropdown);
    // Selects the item whose display equals `display` (raising SelectionChanged
    // on a real change). Returns true on a match.
    [[nodiscard]] bool select_item(const std::string& display);

    // --- SelectionChanged notification -------------------------------------
    [[nodiscard]] vaml::Signal<void()>& selection_changed() { return onSelectionChanged_; }

protected:
    // S472 real ComboBox algorithm: the default combo box slot; the rendered
    // field + arrow is the vulkan classifier.
    [[nodiscard]] vaml::Size MeasureOverride(const vaml::Size& available) const override;

private:
    vaml::Signal<void()> onSelectionChanged_;
    void apply_selection(std::int32_t newIndex);
    [[nodiscard]] std::vector<std::string> split_items() const;
};

// ListBox + ListView — REAL controls (S473, Sprint 11c). RESOLVE the S087
// MINIMAL REAL seeds at their own sprints (NO-GAP, not a backfill): re-based
// onto UIElement so they participate in the visual tree + hit-testing + routed
// events. ListView derives ListBox (the shared item-selection base).
//
// S473 surface (the S473 title: "Items/ItemsSource + SelectionChanged +
// ItemTemplate property-element authoring"):
//   - Items / ItemsSource (REAL DPs; String owned items) + SelectedIndex /
//     SelectedItem + a real SelectionChanged routed event (shared via ListBox).
//   - ITEM TEMPLATE: `ListBox.ItemTemplate` / `ListView.ItemTemplate` is a
//     property-element (the S452 grammar) that routes the authored template
//     into the S364 engine so items render authored per-item content. The
//     authored template's TEXT is stored on the ItemTemplate DP; `render_items`
//     instantiates one UIElement per item through the S364 `instantiate` seam
//     (the default selector output resolves the authored content). This is the
//     honest headless seam: authored content -> real element children, not raw
//     text.
//   - classify_merge_element -> MaterialKind::Solid (list fill).
class VAML_CORE_EXPORT ListBox : public UIElement
{
public:
    ListBox();

    // --- Items / ItemsSource (REAL DPs; String) ---------------------------
    [[nodiscard]] SetResult set_items(std::string value);
    [[nodiscard]] std::string items() const;
    [[nodiscard]] SetResult set_items_source(std::string value);
    [[nodiscard]] std::string items_source() const;

    // --- SelectedIndex / SelectedItem (REAL DPs; Int32 / String) -----------
    [[nodiscard]] SetResult set_selected_index(std::int32_t index);
    [[nodiscard]] std::int32_t selected_index() const;
    [[nodiscard]] std::string selected_item() const;

    // --- SelectionChanged notification (shared via ListBox) ---------------
    [[nodiscard]] vaml::Signal<void()>& selection_changed() { return onSelectionChanged_; }

    // --- ItemTemplate (REAL DP; String — the authored template, S452/S364) --
    [[nodiscard]] SetResult set_item_template(std::string value);
    [[nodiscard]] std::string item_template() const;
    // S500 (Sprint 11f): the authored DataTemplate OBJECT (non-owning;
    // nullptr when none). The `<ListBox.ItemTemplate>` property-element stores
    // a real DataTemplate; this accessor exposes it for per-item rendering.
    [[nodiscard]] DataTemplate* item_template_object() const noexcept;
    void set_item_template_object(DataTemplate* templ);

    // --- Rendering (S473) ---------------------------------------------------
    // Instantiates the authored ItemTemplate through the S364 instantiate seam
    // producing one UIElement child per item (the "items render authored
    // content" headline). Returns the number of item elements built.
    [[nodiscard]] std::size_t render_items();

protected:
    // S473 real ListBox algorithm: the default list box slot.
    [[nodiscard]] vaml::Size MeasureOverride(const vaml::Size& available) const override;

private:
    vaml::Signal<void()> onSelectionChanged_;
    void apply_selection(std::int32_t newIndex);
    [[nodiscard]] std::vector<std::string> split_items() const;
    // S500: the authored DataTemplate observer (non-owning — the reader owns the
    // DataTemplate object). The prior programmatic itemTemplateOwned_ retained
    // the wrapped TextBlock; S500 keeps a typed DataTemplate accessor.
    DataTemplate* itemTemplateObject_ = nullptr;
    std::vector<std::unique_ptr<UIElement>> itemTemplateOwned_;
};

class VAML_CORE_EXPORT ListView final : public ListBox
{
public:
    ListView();
};

// Window — REAL container window-type (S440). RESOLVES the S087 MINIMAL REAL
// seed at its own sprint (NO-GAP, not a backfill): re-based onto UIElement (was
// DependencyObject) so it participates in the visual tree + Measure/Arrange +
// the app-shell (SPRINT 11, the in-repo v1 viewer deliverable).
//
// S440 surface (the S440 title: "app-shell = an authored MainView.vaml rendered
// via the public surface"): Window is the thin WPF-faithful WINDOW-TYPE root of
// the app-shell — a UIElement CONTAINER that hosts ONE logical content child
// (the authored MainView.vaml Grid). As a UIElement it lays out its single
// content child to FILL its own box (implicit single-child layout: the child is
// arranged into the Window's full final rect), participates in hit-testing
// (Window IS the hit root of the app-shell tree), and renders through the same
// generic classifier both apps consume.
//
// A Window hosts EXACTLY ONE content child (the WPF ContentControl-faithful
// composition): add_child with a second child replaces the first (an owned
// single-content slot, not a panel list). MeasureOverride measures the single
// child at the window's offered box; ArrangeOverride arranges it to fill.
//
// S440 also authors the app-shell harness (vaml_viewer/app_shell.{h,cpp}, the
// ADR-021 consumer path) + the authored MainView.vaml + the `--app` viewer
// launch mode; Window is this core re-base that lets the app-shell root be a
// real renderable/layoutable UIElement. The Window's real content (the host
// + the app-shell tree) is authored AT THIS STEP in the viewer, not back-filled.
class VAML_CORE_EXPORT Window final : public UIElement
{
public:
    Window();

    // --- Content child (WPF-Faithful single-content slot) ------------------
    // Window hosts EXACTLY ONE content child. `set_content` adopts an owned
    // child (replacing any prior). The child is owned by the Window's visual-
    // children list (the S121 single owner); `content_` is a NON-OWNING observer
    // pointer (kept alive by the parent), so no second ownership edge exists.
    // `content()` returns the non-owning pointer (nullptr when empty).
    void set_content(std::unique_ptr<UIElement> child);
    [[nodiscard]] UIElement* content() const noexcept;

    // --- S463 lifecycle events (Window Loaded/Closing) --------------------
    // Raises the BUBBLING Loaded / Closing events (source==this window) so the
    // authored `Loaded="..."` / `Closing="..."` markup (wired via BindEvents)
    // fires. A handle/lifecycle transition (the host shows/closes the window)
    // calls these.
    [[nodiscard]] bool raise_loaded();
    [[nodiscard]] bool raise_closing();

protected:
    // S440 real Window algorithm: MeasureOverride measures the single content
    // child at the offered box and reports the child's desired size (the
    // window sizes to its content within the offered bounds); ArrangeOverride
    // arranges the content child to fill the Window's final rect (implicit
    // single-child layout). The base clip (LayoutSlot clip_to_bounds) clips
    // content to the window box.
    [[nodiscard]] vaml::Size MeasureOverride(const vaml::Size& available) const override;
    void ArrangeOverride(const vaml::Rect& finalSlot) override;

private:
    // NON-OWNING observer pointer to the single content child (owned by the
    // visual-children list). Kept so Measure/Arrange/content() can reach the
    // content without a list scan.
    UIElement* content_ = nullptr;
};

// UserControl — REAL content-container element (11j-1, GAP CLOSE, dep:S450_1).
// RESOLVES a corpus blocker: ~90 external WPF corpus files author
// `<UserControl .../>` as a standalone root / nested content container, which
// previously threw `unknown element 'UserControl'`.
//
// 11j-1 makes UserControl a REAL, permanent, loadable element: a self-contained
// UIElement that hosts EXACTLY ONE logical content child, WPF-faithful to the
// corpus's UserControl usage (a root/content container with one logical child
// that renders through the generic classifier). It carries a REAL `"Content"`
// DP (an owned/borrowed object identity via Value::UntypedPtr) + the Window-
// faithful single-content child slot (set_content/content()).
//
// SCOPE BOUNDARY (recorded, NO-GAP not silent): the full `ContentControl`
// ABSTRACT BASE is the S649 continuation (see the Sprint-11d header note — the
// control-content abstraction stays deferred). 11j-1 deliberately does NOT
// introduce a ContentControl base; UserControl is self-contained (Content DP +
// single-content slot) so it satisfies the corpus authoring surface WITHOUT
// pre-empting S649's later design. NO-LAZY: the content slot is REAL +
// renderable today (a UserControl root RENDERS its content child).
//
// Width/Height: inherited from the base UIElement layout DPs (S440-era, real).
// FontSize inheritance: the DP is hoisted at 11j-5's shared Control base move;
// 11j-1 uses the bare `"FontSize"` name contract (resolves against that owner
// once 11j-5 lands — recorded, not silent).
class VAML_CORE_EXPORT UserControl final : public UIElement
{
public:
    UserControl();

    // --- Content DP + single-content child slot (Window-faithful) ---------
    // `set_content` adopts an owned child (replacing any prior); the child is
    // owned by the visual-children list (S121 single owner); `content_` is a
    // NON-OWNING observer (kept alive by the parent) — no second ownership edge.
    void set_content(std::unique_ptr<UIElement> child);
    [[nodiscard]] UIElement* content() const noexcept;

protected:
    // 11j-1 real UserControl algorithm: MeasureOverride measures the single
    // content child at the offered box and reports its desired size (the
    // UserControl sizes to its content within the offered bounds); ArrangeOverride
    // arranges the content child to FILL its final rect (implicit single-child
    // layout — a UserControl root RENDERS its content).
    [[nodiscard]] vaml::Size MeasureOverride(const vaml::Size& available) const override;
    void ArrangeOverride(const vaml::Rect& finalSlot) override;

private:
    UIElement* content_ = nullptr;
};

// Application — REAL document-root element (11j-6, GAP CLOSE, dep:S450_1).
// RESOLVES the canonical WPF app entry `<Application ... StartupUri="...">`,
// which previously threw `unknown element 'Application'`. A thin, permanent,
// loadable no-op content shell (owner type `"Application"`).
//
// WPF-FAITHFUL: `Application` is NOT a FrameworkElement/UIElement — it is the
// app-level object (a DocumentApplication in XAML), so `Application final :
// DependencyObject` (NOT UIElement). It carries a REAL `"StartupUri"` DP
// (String) for the most-common `<Application StartupUri="...">` case; the
// loader resolves it without throwing. The reader's build() already handles a
// non-visual DependencyObject element as a root (returns it, nothing retained
// as a visual child; finalize_visual_tree skips a non-UIElement root).
//
// RECORDED NO-GAP (not silent): `StartupUri` is a record-only DP at this step —
// the NAVIGATION (actually opening the referenced window) is a later app-host
// concern. The loader only needs it to PARSE the app root.
class VAML_CORE_EXPORT Application final : public DependencyObject
{
public:
    Application();

    // --- StartupUri (REAL DP; String, default "") -------------------------
    [[nodiscard]] SetResult set_startup_uri(std::string value);
    [[nodiscard]] std::string startup_uri() const;
};

// ResourceDictionaryHolder — REAL document-root element (11j-2, GAP CLOSE,
// dep:S453). RESOLVES the corpus blocker where a standalone `.xaml` doc is a
// THEME/ICON/DataTemplate RESOURCE file whose root is `<ResourceDictionary>`.
//
// WPF-FAITHFUL (confirmed from the WPF source, ResourceDictionary.cs): WPF's
// `ResourceDictionary` is a PLAIN class (implements IDictionary /
// ISupportInitialize — it is NOT a DependencyObject), and it is the legal root
// of a standalone resource `.xaml`. Our Load API returns
// `std::unique_ptr<DependencyObject>`, so a root RD cannot be returned directly.
// The READ-only DependencyObject host below OWNS the built dictionary — the
// faithful mapping that lets `<ResourceDictionary>` be a loadable ROOT while
// reusing S453's capture (build_resource_dictionary) verbatim. The host is REAL
// + permanent (mirrors the S460 `GradientStopHost` pretscedent — a DependencyObject
// that hosts a non-elementary authored value); it is registered as the element
// name "ResourceDictionary" in the factory. A NESTED `<ResourceDictionary>` inside
// a legal container (`.Resources` / `.MergedDictionaries`) stays handled by
// build_resource_dictionary as before (no host needed there).
//
// The owned dictionary is accessible via dictionary() (non-owning) and retrieved
// by the reader per-Load (the host owns it; the reader keeps it alive). The full
// WPF resource-scope wiring (app/system) is the S360 lane, unchanged — this step
// only unblocks PARSING/LOADING the RD root.
class VAML_CORE_EXPORT ResourceDictionaryHolder final : public DependencyObject
{
public:
    ResourceDictionaryHolder();
    // Out-of-line destructor: the owned std::unique_ptr<ResourceDictionary>
    // member can only be destroyed where ResourceDictionary is COMPLETE
    // (Element.cpp includes ResourceDictionary.h). Declaring it out-of-line
    // keeps the holder usable via by-value/factory in any TU that only
    // forward-declares ResourceDictionary (clang requirement).
    ~ResourceDictionaryHolder() override;

    // The OWNED ResourceDictionary (set by the reader's build_resource_dictionary
    // root path). `dictionary()` returns the non-owning pointer (nullptr when the
    // holder has none).
    void set_dictionary(std::unique_ptr<ResourceDictionary> dict);
    [[nodiscard]] const ResourceDictionary* dictionary() const noexcept;
    [[nodiscard]] ResourceDictionary* dictionary() noexcept;

private:
    std::unique_ptr<ResourceDictionary> dict_;
};

// ---------------------------------------------------------------------------
// SPRINT 11d (S480–S486) — the core display + input controls. All are REAL
// re-based UIElements (not seeds): each participates in the visual tree +
// Measure/Arrange + the classifier (ADR-011 — "how to render" stays out of
// core; the render material is authored at the vulkan/presentation classifier).
// None of these adds a new MaterialKind; they reuse Solid / Border / Text /
// Monochrome. No ContentControl base class is introduced here (the control-
// content abstraction is the S649 continuation, recorded NO-GAP not silent).
// ---------------------------------------------------------------------------

// Label — REAL control (S480). The WPF-faithful legacy `ContentControl` kept
// for compatibility: a single string content value rendered as text. It is NOT
// derived from a ContentControl base (none exists yet); it is a self-contained
// UIElement whose Content DP is rendered through the S400 Text glyph path.
//
// S480 surface:
//   - Content (String, default "") — the label text.
//   - FontSize (Double, default 16, validated >= 0).
//   - FontFamily (String, default "") — a font family name (the embedded font
//     is single-weight; the DP is the WPF-faithful surface).
//   - Foreground (BrushId, default 0) — the WPF-faithful brush read (mirrors
//     Text; unresolved -> opaque black + diagnostic).
//   - HorizontalContentAlignment (Int32, default Left=0 — reuses the S122
//     alignment convention).
//
// MEASURE: MeasureOverride wraps the Content via the S400 text_layout_plan
// (the S400 basis) so the Label sizes to its wrapped text extent. The render
// material is the CLASSIFIER's (Solid at the vertical slice; the content glyphs
// defactor to the S400 Text path, recorded NO-GAP not silent).
class VAML_CORE_EXPORT Label final : public UIElement
{
public:
    Label();

    // --- Content (REAL DP; String, default "") ----------------------------
    [[nodiscard]] SetResult set_content(std::string value);
    [[nodiscard]] std::string content() const;

    // --- FontSize (REAL DP; Double, default 16, validated >= 0) -----------
    [[nodiscard]] SetResult set_font_size(double value);
    [[nodiscard]] double font_size() const;

    // --- FontFamily (REAL DP; String, default "") -------------------------
    [[nodiscard]] SetResult set_font_family(std::string value);
    [[nodiscard]] std::string font_family() const;

    // --- Foreground (REAL DP; BrushId, default 0) -------------------------
    [[nodiscard]] SetResult set_foreground(std::int32_t brushId);
    [[nodiscard]] std::int32_t foreground() const;

    // --- HorizontalContentAlignment (REAL DP; Int32, default Left=0) ------
    [[nodiscard]] SetResult set_horizontal_content_alignment(std::int32_t value);
    [[nodiscard]] std::int32_t horizontal_content_alignment() const;

protected:
    [[nodiscard]] vaml::Size MeasureOverride(const vaml::Size& available) const override;
};

// Border — REAL control (S481). A WPF-faithful single-content decorator that
// draws a border (BorderBrush/BorderThickness/CornerRadius) around its single
// child, with Padding between the border and the content. This is the FIRST
// production user of the dormant `MaterialKind::Border` + `border_rect(...)`
// path — the S205 Border pipeline is made LIVE at S481 (a real NO-GAP closure).
//
// S481 surface:
//   - BorderBrush (BrushId, default 0) — the stroke brush (resolved via the
//     S364 brush read, mirrors Text).
//   - BorderThickness (Thickness, default 0) — the four-edge stroke width.
//   - CornerRadius (Thickness, default 0) — the corner rounding (a uniform/2-
//     value thickness; the stroke path uses the vertical-slice box).
//   - Padding (Thickness, default 0) — the inset between border and content.
//   - Single content child (Window-faithful slot: set_content/content()).
//
// MEASURE/ARRANGE: MeasureOverride measures the single child + insets by
// Padding/BorderThickness; ArrangeOverride arranges the child into the inner
// slot (the base bezel is drawn by the classifier). The classifier emits the
// Border material (border_rect) at the arranged box — the FIRST live Border.
class VAML_CORE_EXPORT Border final : public UIElement
{
public:
    Border();

    // --- BorderBrush (REAL DP; BrushId, default 0) ------------------------
    [[nodiscard]] SetResult set_border_brush(std::int32_t brushId);
    [[nodiscard]] std::int32_t border_brush() const;

    // --- BorderThickness (REAL DP; Thickness, default 0) ------------------
    [[nodiscard]] SetResult set_border_thickness(vaml::Thickness value);
    [[nodiscard]] vaml::Thickness border_thickness() const;

    // --- CornerRadius (REAL DP; Thickness, default 0) ---------------------
    [[nodiscard]] SetResult set_corner_radius(vaml::Thickness value);
    [[nodiscard]] vaml::Thickness corner_radius() const;

    // --- Padding (REAL DP; Thickness, default 0) --------------------------
    [[nodiscard]] SetResult set_padding(vaml::Thickness value);
    [[nodiscard]] vaml::Thickness padding() const;

    // --- Content child (WPF-faithful single-content slot) -----------------
    void set_content(std::unique_ptr<UIElement> child);
    [[nodiscard]] UIElement* content() const noexcept;

protected:
    [[nodiscard]] vaml::Size MeasureOverride(const vaml::Size& available) const override;
    void ArrangeOverride(const vaml::Rect& finalSlot) override;

private:
    UIElement* content_ = nullptr;
};

// Rectangle / Ellipse / Line — REAL shape controls (S482). Each is a
// self-contained UIElement whose Fill/Stroke DPs drive a solid/border draw at
// the arranged box.
//
// S482 surface (shared shape DP shape):
//   - Fill (BrushId, default 0) — the interior fill brush.
//   - Stroke (BrushId, default 0) — the outline stroke brush.
//   - StrokeThickness (Double, default 0) — the stroke width.
//   - Width / Height (Double, default 0 = intrinsic box) — the shape's logical
//     box on each axis; 0 = the authored/derived natural box.
//
// CLASSIFIER (recorded, not silent): Rectangle/Ellipse classify to Solid (the
// fill box — the honest whole-quad vertical-slice default; Ellipse's CURVED
// outline is the S209/S649 curved-shape continuation, NOT fabricated). Line
// classifies to Solid/Border (a thin stroke rect along its axis; a zero-length
// Line is skipped by ui_item_degenerate).
class VAML_CORE_EXPORT Rectangle final : public UIElement
{
public:
    Rectangle();

    [[nodiscard]] SetResult set_fill(std::int32_t brushId);
    [[nodiscard]] std::int32_t fill() const;
    [[nodiscard]] SetResult set_stroke(std::int32_t brushId);
    [[nodiscard]] std::int32_t stroke() const;
    [[nodiscard]] SetResult set_stroke_thickness(double value);
    [[nodiscard]] double stroke_thickness() const;
    // Width/Height are the base UIElement WPF-style layout DPs (auto/NaN — the
    // S123 reconciler applies them, recorded NO-GAP not silent); a shape's box
    // is the inherited base Width/Height + the S123 stretch-vs-wrap finalizer.

protected:
    [[nodiscard]] vaml::Size MeasureOverride(const vaml::Size& available) const override;
};

class VAML_CORE_EXPORT Ellipse final : public UIElement
{
public:
    Ellipse();

    [[nodiscard]] SetResult set_fill(std::int32_t brushId);
    [[nodiscard]] std::int32_t fill() const;
    [[nodiscard]] SetResult set_stroke(std::int32_t brushId);
    [[nodiscard]] std::int32_t stroke() const;
    [[nodiscard]] SetResult set_stroke_thickness(double value);
    [[nodiscard]] double stroke_thickness() const;

protected:
    [[nodiscard]] vaml::Size MeasureOverride(const vaml::Size& available) const override;
};

class VAML_CORE_EXPORT Line final : public UIElement
{
public:
    Line();

    [[nodiscard]] SetResult set_fill(std::int32_t brushId);
    [[nodiscard]] std::int32_t fill() const;
    [[nodiscard]] SetResult set_stroke(std::int32_t brushId);
    [[nodiscard]] std::int32_t stroke() const;
    [[nodiscard]] SetResult set_stroke_thickness(double value);
    [[nodiscard]] double stroke_thickness() const;
    // Line has no authored Width/Height DPs of its own; it uses the base
    // UIElement Width/Height (auto/NaN) + the S123 finalizer, and its stroke
    // thickness drives the cross-axis rule height.

protected:
    [[nodiscard]] vaml::Size MeasureOverride(const vaml::Size& available) const override;
};

// Image — REAL control (S483). Rides the S400_1/S202_1 icon asset pipeline: it
// resolves its Source (direct asset reference) or IconKey (resource key) through
// the S400_1 Icon machinery and renders the image at its box.
//
// S483 surface:
//   - Source (String, default "") — a packaged icon asset / direct reference.
//   - IconKey (String, default "") — the S360_1 resource key resolved via the
//     S400_1 Icon machinery (wins over Source when both are set, per ADR-015).
//   - Stretch (Int32, default None=0 — reuses the S400_1 IconStretch enum).
//   - Width / Height (Double, default 0 = auto) — the image's logical box.
//
// CLASSIFIER: Image classifies to Monochrome (the image-texture path — mirrors
// the S400_1 Icon; the headless default emits the fallback glyph via
// IconGlyphPlan/IconBestFit; the real atlas quad is the S409_1-style render
// path, recorded NO-GAP not silent).
class VAML_CORE_EXPORT Image final : public UIElement
{
public:
    Image();

    [[nodiscard]] SetResult set_source(std::string value);
    [[nodiscard]] std::string source() const;
    [[nodiscard]] SetResult set_icon_key(std::string value);
    [[nodiscard]] std::string icon_key() const;
    [[nodiscard]] SetResult set_stretch(std::int32_t value);
    [[nodiscard]] std::int32_t stretch() const;
    // Width/Height are the base UIElement WPF-style layout DPs (auto/NaN — the
    // S123 reconciler applies them, recorded NO-GAP not silent).

protected:
    [[nodiscard]] vaml::Size MeasureOverride(const vaml::Size& available) const override;
};

// ProgressBar — REAL control (S484). A determinate progress indicator: Value
// clamped to [Minimum, Maximum], rendered as a track + fill.
//
// S484 surface:
//   - Value (Double, default 0, clamped to [Minimum,Maximum]).
//   - Minimum (Double, default 0).
//   - Maximum (Double, default 100).
//   - A real Value change raises the BUBBLING "ValueChanged" routed event +
//     the value_changed Signal (the S403 Slider precedent).
//
// CLASSIFIER: classifies to Solid — classify_merge_element emits a track box +
// a fill box whose width = Value/(Maximum-Minimum) × the arranged width (the
// fill fraction computed at the classifier from the range DPs). Indeterminate
// (0% or undefined) = a track-only box (recorded NO-GAP not silent; segmented/
//  marquee indeterminate chrome is the S649 continuation).
class VAML_CORE_EXPORT ProgressBar final : public UIElement
{
public:
    ProgressBar();

    [[nodiscard]] SetResult set_value(double v);
    [[nodiscard]] double value() const;
    [[nodiscard]] SetResult set_minimum(double v);
    [[nodiscard]] double minimum() const;
    [[nodiscard]] SetResult set_maximum(double v);
    [[nodiscard]] double maximum() const;

    [[nodiscard]] vaml::Signal<void()>& value_changed() { return onValueChanged_; }

protected:
    [[nodiscard]] vaml::Size MeasureOverride(const vaml::Size& available) const override;

private:
    vaml::Signal<void()> onValueChanged_;
    void apply_change(double newValue);
};

// PasswordBox — REAL control (S485). A masked single-line text input: the
// edited content is the Password DP (never the literal text); the on-screen
// glyphs are masked bullets.
//
// S485 surface:
//   - Password (String, default "") — the edited (masked) content.
//   - MaxLength (Int32, default 0 = unlimited).
//   - PasswordChar (String, default "*") — the mask bullet.
//   - Editing mirrors the S402 TextBox handle_key model (single-line Enter
//     accepts; a real change raises TextChanged). Classifier -> Solid (the
//     edit-field fill).
class VAML_CORE_EXPORT PasswordBox final : public UIElement
{
public:
    PasswordBox();

    [[nodiscard]] SetResult set_password(std::string value);
    [[nodiscard]] std::string password() const;
    [[nodiscard]] SetResult set_max_length(std::int32_t value);
    [[nodiscard]] std::int32_t max_length() const;
    [[nodiscard]] SetResult set_password_char(std::string value);
    [[nodiscard]] std::string password_char() const;

    // The S402-faithful single-line editing entry point: returns true when the
    // key drove a real edit/accept.
    [[nodiscard]] bool handle_key(vaml::KeyCode key, bool isDown, bool shift);

protected:
    [[nodiscard]] vaml::Size MeasureOverride(const vaml::Size& available) const override;

private:
    std::size_t caret_ = 0;
    void raise_password_changed();
};

// GroupBox / Expander — REAL header+content containers (S485).
//   - GroupBox: a bordered group with a Header (String) + a single content
//     child. Classifier -> Solid + Border.
//   - Expander: a header + expandable content, with IsExpanded (Int32 bool,
//     WPF-faithful). Classifier -> Solid (the expand chevron = S649).
class VAML_CORE_EXPORT GroupBox final : public UIElement
{
public:
    GroupBox();

    [[nodiscard]] SetResult set_header(std::string value);
    [[nodiscard]] std::string header() const;
    void set_content(std::unique_ptr<UIElement> child);
    [[nodiscard]] UIElement* content() const noexcept;

protected:
    [[nodiscard]] vaml::Size MeasureOverride(const vaml::Size& available) const override;
    void ArrangeOverride(const vaml::Rect& finalSlot) override;

private:
    UIElement* content_ = nullptr;
};

class VAML_CORE_EXPORT Expander final : public UIElement
{
public:
    Expander();

    [[nodiscard]] SetResult set_header(std::string value);
    [[nodiscard]] std::string header() const;
    [[nodiscard]] SetResult set_is_expanded(std::int32_t value);
    [[nodiscard]] bool is_expanded() const;
    void set_content(std::unique_ptr<UIElement> child);
    [[nodiscard]] UIElement* content() const noexcept;

protected:
    [[nodiscard]] vaml::Size MeasureOverride(const vaml::Size& available) const override;
    void ArrangeOverride(const vaml::Rect& finalSlot) override;

private:
    UIElement* content_ = nullptr;
};

// Separator — REAL control (S485). A thin horizontal/vertical divider.
class VAML_CORE_EXPORT Separator final : public UIElement
{
public:
    Separator();

    [[nodiscard]] SetResult set_thickness(double value);
    [[nodiscard]] double thickness() const;

protected:
    [[nodiscard]] vaml::Size MeasureOverride(const vaml::Size& available) const override;
};

// StatusBar — REAL control (S485). A docking item host that lays out its
// children along an edge (reuses the S470 DockPanel dock pattern; StatusBar
// items may be docked left/right). Classifier -> Solid.
class VAML_CORE_EXPORT StatusBar final : public UIElement
{
public:
    StatusBar();

    // --- Attached StatusBar.Dock (REAL DP, S485) --------------------------
    // The item's dock edge (Left=0 / Right=1, the WPF StatusBar left/right
    // dock). Stored on the child (the S043 attached-aware path). Typed statics.
    [[nodiscard]] static SetResult set_dock(DependencyObject& obj, std::int32_t value);
    [[nodiscard]] static std::int32_t dock(const DependencyObject& obj);

protected:
    [[nodiscard]] vaml::Size MeasureOverride(const vaml::Size& available) const override;
    void ArrangeOverride(const vaml::Rect& finalSlot) override;

private:
    // StatusBar uses the generic children() list (a bar hosts many items).
};

// ---------------------------------------------------------------------------
// 11k-2 — the missing WPF controls the corpus consumes that the roadmap never
// authored: RepeatButton (toolkit NumericUpDown/NumUpDown spin buttons),
// ScrollBar (the scrollbar chrome ScrollViewer S405 drives), Viewbox (the
// scale-contain single-child container, ColorPickerSwatch), ToolBar +
// ToolBarTray (the app's ToolbarControl basis). Each is a REAL UIElement
// re-base — participates in the visual tree + Measure/Arrange + hit-testing +
// routed events — registered as a built-in, classified. No ContentControl /
// Control / RangeBase / ItemsControl base exists yet (that is 11k-11's S649
// re-bind, recorded NO-GAP not silent); each control is self-contained exactly
// like Button/DockPanel/TextBox before it. classify: RepeatButton / ScrollBar /
// ToolBar / ToolBarTray -> Solid; Viewbox -> its content (None — the content
// child renders its own material).
// ---------------------------------------------------------------------------

// ScrollBar — REAL control (11k-2). The scrollbar chrome ScrollViewer (S405)
// drives: a RangeBase-faithful Value/Min/Max slider-like track + a scrolling
// routed event. WPF-faithful branch: ScrollBar : RangeBase (the Value/Min/Max
// axis) + its own Orientation/ViewportSize surface. The full WPF ScrollBar
// thumb-track chrome (the page-up/down repeat regions) is the S649/11k-11
// continuation (recorded NO-GAP not silent); the Value/range/scroll model is
// the real 11k-2 surface.
class VAML_CORE_EXPORT ScrollBar final : public UIElement
{
public:
    ScrollBar();

    // --- Value / Minimum / Maximum (REAL DPs; Double, RangeBase-faithful) -
    // Value clamps to [Minimum, Maximum] on Set (never escapes the range).
    [[nodiscard]] SetResult set_value(double v);
    [[nodiscard]] double value() const;
    [[nodiscard]] SetResult set_minimum(double v);
    [[nodiscard]] double minimum() const;
    [[nodiscard]] SetResult set_maximum(double v);
    [[nodiscard]] double maximum() const;

    // --- Orientation (REAL DP; Int32, default Vertical=0) -----------------
    [[nodiscard]] SetResult set_orientation(Orientation o);
    [[nodiscard]] Orientation orientation() const;

    // --- ViewportSize (REAL DP; Double, default 0) ------------------------
    [[nodiscard]] SetResult set_viewport_size(double v);
    [[nodiscard]] double viewport_size() const;

    // --- Scroll routed event + scroll_changed Signal ----------------------
    // Every real Value change raises the BUBBLING "Scroll" routed event
    // (source==this) + fires the Signal.
    [[nodiscard]] vaml::Signal<void()>& scroll_changed() { return onScrollChanged_; }

    // --- Thumb-drag seam (the scrollbar chrome, S405/S323 input style) -----
    // Maps a [0,1] drag ratio along the track to Value (clamped +
    // min/max-stretched) + raises the change. Returns the new Value.
    [[nodiscard]] double set_value_from_ratio(double ratio);

protected:
    // 11k-2 real ScrollBar algorithm: a default track box (the scrollbar slot);
    // the S123 finalizer applies the stretch-vs-wrap rule. The thumb + the
    // page-up/down chrome is the S649 continuation.
    [[nodiscard]] vaml::Size MeasureOverride(const vaml::Size& available) const override;

private:
    vaml::Signal<void()> onScrollChanged_;
    void apply_change(double newValue);
};

// RepeatButton — REAL control (11k-2). The toolkit NumericUpDown/NumUpDown
// SPIN-BUTTON base: a ButtonBase-faithful Click + Delay/Interval + IsPressed.
// WPF-faithful branch: RepeatButton : ButtonBase (Click + Command + IsPressed).
// The auto-repeat TIMER loop (fires Click repeatedly while pressed) is the
// S649/11k-11 continuation (recorded NO-GAP not silent); the Click raise +
// Delay/Interval/IsPressed surface is the real 11k-2 core.
class VAML_CORE_EXPORT RepeatButton final : public UIElement
{
public:
    RepeatButton();

    // --- Click routed event (ButtonBase-faithful) -------------------------
    // Raises the BUBBLING ClickEvent (S320/S321) with source==this; a handled
    // return reflects whether any handler stopped the route.
    [[nodiscard]] bool raise_click();

    // --- Delay / Interval (REAL DPs; Int32, milliseconds) -----------------
    [[nodiscard]] SetResult set_delay(std::int32_t v);
    [[nodiscard]] std::int32_t delay() const;
    [[nodiscard]] SetResult set_interval(std::int32_t v);
    [[nodiscard]] std::int32_t interval() const;

    // --- IsPressed (REAL DP; Bool via Int32, default false) ---------------
    [[nodiscard]] SetResult set_is_pressed(bool value);
    [[nodiscard]] bool is_pressed() const;

protected:
    // 11k-2 real RepeatButton algorithm: a bounded default button box; the
    // S123 finalizer applies the stretch-vs-wrap. The repeat timer loop is the
    // S649 continuation.
    [[nodiscard]] vaml::Size MeasureOverride(const vaml::Size& available) const override;
};

// Viewbox — REAL control (11k-2). The scale-contain single-child container
// (ColorPickerSwatch): a FrameworkElement-faithful decorator hosting EXACTLY
// ONE content child, scaled to FIT its box (Stretch/StretchDirection). WPF-
// faithful branch: Viewbox : Decorator (FrameworkElement). In VAML (the
// framework-element branch IS UIElement) Viewbox : UIElement with a single-
// content child slot (the Window/Border precedent). classify -> None (the
// content child renders its own material — "Viewbox -> its content").
class VAML_CORE_EXPORT Viewbox final : public UIElement
{
public:
    Viewbox();

    // --- Stretch / StretchDirection (REAL DPs; Int32, reuse IconStretch) --
    // Stretch: Int32 default None=0 (the S400_1 IconStretch enum reused: None,
    // Fill, Uniform, UniformToFill — the scale-contain policy). Classified as
    // the single content child's material (None) so the child renders its own
    // content.
    [[nodiscard]] SetResult set_stretch(std::int32_t value);
    [[nodiscard]] std::int32_t stretch() const;
    [[nodiscard]] SetResult set_stretch_direction(std::int32_t value);
    [[nodiscard]] std::int32_t stretch_direction() const;

    // --- Content child (single-content slot, the Window/Border precedent) -
    void set_content(std::unique_ptr<UIElement> child);
    [[nodiscard]] UIElement* content() const noexcept;

protected:
    // 11k-2 real Viewbox algorithm: measure the single content child at the
    // offered box and report its desired size; the scale-contain to the box is
    // the S123 finalizer + the stretch policy (the content child fills the
    // viewbox box via the decorator layout).
    [[nodiscard]] vaml::Size MeasureOverride(const vaml::Size& available) const override;
    void ArrangeOverride(const vaml::Rect& finalSlot) override;

private:
    UIElement* content_ = nullptr;
};

// ToolBar — REAL control (11k-2). The app's ToolbarControl basis: an
// ItemsControl-faithful strip that lays out its children along an orientation.
// WPF-faithful branch: ToolBar : ItemsControl (its own Orientation/Band/
// BandIndex/IsOverflowOpen/HasOverflowItems surface). The overflow menu +
// templated item chrome is the S649/11k-11 continuation (recorded NO-GAP not
// silent); the strip layout + the ToolBar DPs are the real 11k-2 surface.
class VAML_CORE_EXPORT ToolBar final : public UIElement
{
public:
    ToolBar();

    // --- Orientation (REAL DP; Int32, default Horizontal=1) ---------------
    [[nodiscard]] SetResult set_orientation(Orientation o);
    [[nodiscard]] Orientation orientation() const;

    // --- Band / BandIndex / IsOverflowOpen / HasOverflowItems (REAL DPs) --
    [[nodiscard]] SetResult set_band(std::int32_t v);
    [[nodiscard]] std::int32_t band() const;
    [[nodiscard]] SetResult set_band_index(std::int32_t v);
    [[nodiscard]] std::int32_t band_index() const;
    [[nodiscard]] SetResult set_is_overflow_open(bool value);
    [[nodiscard]] bool is_overflow_open() const;
    [[nodiscard]] SetResult set_has_overflow_items(bool value);
    [[nodiscard]] bool has_overflow_items() const;

protected:
    // 11k-2 real ToolBar algorithm: a dock-like strip layout along the
    // orientation (children stack along the flow axis — the StackPanel-style
    // strip). The S123 finalizer applies the stretch-vs-wrap. The overflow
    // chrome is the S649 continuation.
    [[nodiscard]] vaml::Size MeasureOverride(const vaml::Size& available) const override;
    void ArrangeOverride(const vaml::Rect& finalSlot) override;
};

// ToolBarTray — REAL control (11k-2). The ToolBar host: a Panel-faithful
// container that lays its ToolBar children out along the tray orientation.
// WPF-faithful branch: ToolBarTray : Panel (own Orientation/IsLocked surface).
class VAML_CORE_EXPORT ToolBarTray final : public UIElement
{
public:
    ToolBarTray();

    // --- Orientation (REAL DP; Int32, default Horizontal=1) ---------------
    [[nodiscard]] SetResult set_orientation(Orientation o);
    [[nodiscard]] Orientation orientation() const;

    // --- IsLocked (REAL DP; Bool via Int32, default false) ----------------
    [[nodiscard]] SetResult set_is_locked(bool value);
    [[nodiscard]] bool is_locked() const;

protected:
    // 11k-2 real ToolBarTray algorithm: lays its ToolBar children along the
    // tray orientation (a StackPanel-style host). The S123 finalizer applies
    // the stretch-vs-wrap.
    [[nodiscard]] vaml::Size MeasureOverride(const vaml::Size& available) const override;
    void ArrangeOverride(const vaml::Rect& finalSlot) override;
};

// ---------------------------------------------------------------------------
// SPRINT 11e (S490–S495) — the container + menu controls. All are REAL re-based
// UIElements (not seeds): each participates in the visual tree + Measure/
// Arrange + the classifier (ADR-011 — "how to render" stays in the vulkan
// classifier). No new MaterialKind; they reuse Solid / Border / Text / None.
// No ContentControl / ItemsControl / Selector abstract base is introduced here
// (the S649 continuation, recorded NO-GAP not silent); each control is
// self-contained exactly like Button/DockPanel/TextBox before it. All new DPs
// are owner-scoped (a reused name under a different owner would be ambiguous,
// per the S043/S131 precedent). classify: TabControl/TabItem/Menu/MenuItem/
// ContextMenu/ToolTip/Popup/GridSplitter -> Solid.
// ---------------------------------------------------------------------------

// TabControl + TabItem — REAL container controls (S490). WPF-faithful:
// TabControl is a Selector over TabItem headers; TabItem is a
// HeaderedContentControl (Header + content). TabControl owns the tab strip +
// the selected TabItem's content track; TabItem = header + a single content
// slot + IsSelected. Selection raises the BUBBLING "SelectionChanged" event
// (source==the TabControl). classify -> Solid (the tab strip + content chrome).
class VAML_CORE_EXPORT TabItem final : public UIElement
{
public:
    TabItem();

    // --- Header (REAL DP; String, default "") ---------------------------
    [[nodiscard]] SetResult set_header(std::string value);
    [[nodiscard]] std::string header() const;

    // --- IsSelected (REAL DP; Bool via Int32, default false) -------------
    [[nodiscard]] SetResult set_is_selected(bool value);
    [[nodiscard]] bool is_selected() const;

    // --- TabStripPlacement (REAL DP; Int32, default Top=0) ---------------
    [[nodiscard]] SetResult set_tab_strip_placement(std::int32_t value);
    [[nodiscard]] std::int32_t tab_strip_placement() const;

    // --- Content child (single-content slot, the Window/Border precedent) -
    void set_content(std::unique_ptr<UIElement> child);
    [[nodiscard]] UIElement* content() const noexcept;

protected:
    [[nodiscard]] vaml::Size MeasureOverride(const vaml::Size& available) const override;
    void ArrangeOverride(const vaml::Rect& finalSlot) override;

private:
    UIElement* content_ = nullptr;
};

class VAML_CORE_EXPORT TabControl final : public UIElement
{
public:
    TabControl();

    // --- SelectedIndex / SelectedItem (REAL DPs; Int32 / String) ----------
    [[nodiscard]] SetResult set_selected_index(std::int32_t index);
    [[nodiscard]] std::int32_t selected_index() const;
    [[nodiscard]] std::string selected_item() const; // the selected TabItem's Header

    // --- TabStripPlacement (REAL DP; Int32, default Top=0) ----------------
    [[nodiscard]] SetResult set_tab_strip_placement(std::int32_t value);
    [[nodiscard]] std::int32_t tab_strip_placement() const;

    // --- Tabs --------------------------------------------------------------
    // The owned TabItem children are the tabs. add_tab adopts an owned TabItem
    // (an alias over add_child that also preserves the item index for
    // selection). SelectionChanged is raised on a real change.
    void add_tab(std::unique_ptr<TabItem> tab);
    [[nodiscard]] std::size_t tab_count() const noexcept;
    [[nodiscard]] TabItem* tab_at(std::size_t index) const noexcept;

    // Selects the TabItem at `index` (clamped); raises SelectionChanged on a
    // real change. Returns the index selected.
    [[nodiscard]] std::int32_t select_index(std::int32_t index);

    // --- SelectionChanged notification -------------------------------------
    [[nodiscard]] vaml::Signal<void()>& selection_changed() { return onSelectionChanged_; }

protected:
    [[nodiscard]] vaml::Size MeasureOverride(const vaml::Size& available) const override;
    void ArrangeOverride(const vaml::Rect& finalSlot) override;

private:
    vaml::Signal<void()> onSelectionChanged_;
    void apply_selection(std::int32_t newIndex);
};

// Menu + MenuItem + ContextMenu — REAL menu controls (S491). WPF-faithful:
// Menu is an ItemsControl with an IsMainMenu surface; MenuItem is a
// HeaderedItemsControl (Header + nested items + Click + Checkable) carrying
// the S464 Command/CommandParameter hand-off; ContextMenu is a popup host
// (IsOpen + Placement). MenuItem.Click raises the BUBBLING "Click" event.
class VAML_CORE_EXPORT MenuItem final : public UIElement
{
public:
    MenuItem();

    // --- Header (REAL DP; String, default "") -------------------------------
    [[nodiscard]] SetResult set_header(std::string value);
    [[nodiscard]] std::string header() const;

    // --- Command / CommandParameter (REAL DPs; String) — the S464 surface ---
    // The authored Command name is resolved + EXECUTED through the S464 command
    // registry (RegisterCommand/ExecuteCommand). CommandParameter is the Value
    // passed on execute. The reader's attribute-authoring stores the names; the
    // exposure to a command registry is via execute_command(name, param).
    [[nodiscard]] SetResult set_command(std::string value);
    [[nodiscard]] std::string command() const;
    [[nodiscard]] SetResult set_command_parameter(std::string value);
    [[nodiscard]] std::string command_parameter() const;

    // --- Checkable / IsChecked (REAL DPs; Bool via Int32, default false) ----
    [[nodiscard]] SetResult set_is_checkable(bool value);
    [[nodiscard]] bool is_checkable() const;
    [[nodiscard]] SetResult set_is_checked(bool value);
    [[nodiscard]] bool is_checked() const;

    // --- IsSubmenuOpen / Role (REAL DPs) ------------------------------------
    [[nodiscard]] SetResult set_is_submenu_open(bool value);
    [[nodiscard]] bool is_submenu_open() const;
    [[nodiscard]] SetResult set_role(std::int32_t value);
    [[nodiscard]] std::int32_t role() const;

    // --- Nested items + Command execution -----------------------------------
    // add_item adopts an owned nested MenuItem (a submenu). A click raises the
    // BUBBLING "Click" event and, when a Command is authored, executes it via
    // the provided registry/execute-seam. Returns a handled flag.
    void add_item(std::unique_ptr<MenuItem> item);
    [[nodiscard]] std::size_t item_count() const noexcept;
    // Raises the Click routed event; when Command is non-empty this is the
    // S464 execute seam (the caller supplies the execute callback that resolves
    // the command). Returns true if any handler handled the route.
    [[nodiscard]] bool raise_click();

    [[nodiscard]] vaml::Signal<void()>& checked_changed() { return onCheckedChanged_; }

protected:
    [[nodiscard]] vaml::Size MeasureOverride(const vaml::Size& available) const override;

private:
    vaml::Signal<void()> onCheckedChanged_;
    void apply_checked(bool newState);
};

class VAML_CORE_EXPORT Menu final : public UIElement
{
public:
    Menu();

    // --- IsMainMenu (REAL DP; Bool via Int32, default false) ----------------
    [[nodiscard]] SetResult set_is_main_menu(bool value);
    [[nodiscard]] bool is_main_menu() const;

    // --- Items --------------------------------------------------------------
    void add_item(std::unique_ptr<MenuItem> item);
    [[nodiscard]] std::size_t item_count() const noexcept;

protected:
    [[nodiscard]] vaml::Size MeasureOverride(const vaml::Size& available) const override;
    void ArrangeOverride(const vaml::Rect& finalSlot) override;
};

class VAML_CORE_EXPORT ContextMenu final : public UIElement
{
public:
    ContextMenu();

    // --- IsOpen / Placement / PlacementTarget (REAL DPs) --------------------
    [[nodiscard]] SetResult set_is_open(bool value);
    [[nodiscard]] bool is_open() const;
    [[nodiscard]] SetResult set_placement(std::int32_t value);
    [[nodiscard]] std::int32_t placement() const;
    // PlacementTarget is a UIElement identity (UntypedPtr, the ContextMenu is
    // attached to). Stored as a raw observer pointer (the target owns the
    // menu, so no ownership edge).
    void set_placement_target(UIElement* target);
    [[nodiscard]] UIElement* placement_target() const noexcept;
    // HorizontalOffset/VerticalOffset (REAL DPs; Double, default 0).
    [[nodiscard]] SetResult set_horizontal_offset(double value);
    [[nodiscard]] double horizontal_offset() const;
    [[nodiscard]] SetResult set_vertical_offset(double value);
    [[nodiscard]] double vertical_offset() const;

    // --- Open/close routed surface ------------------------------------------
    // open(true) opens; open(false) closes; a real open/close raises the
    // BUBBLING "Opened"/"Closed" routed events + the signals.
    [[nodiscard]] bool open(bool value);
    [[nodiscard]] vaml::Signal<void()>& opened() { return onOpened_; }
    [[nodiscard]] vaml::Signal<void()>& closed() { return onClosed_; }

    // --- Items --------------------------------------------------------------
    void add_item(std::unique_ptr<MenuItem> item);
    [[nodiscard]] std::size_t item_count() const noexcept;

protected:
    [[nodiscard]] vaml::Size MeasureOverride(const vaml::Size& available) const override;

private:
    vaml::Signal<void()> onOpened_;
    vaml::Signal<void()> onClosed_;
    UIElement* placementTarget_ = nullptr;
    void apply_open(bool value);
};

// ToolTip + Popup — REAL popup-host controls (S492). Deferred show: the open/
// close is change-driven (a show request does not paint until the change-driven
// pass); placement resolves to a target rect. Opened/Closed are BUBBLING routed
// events. classify -> Solid (the tooltip/popup panel).
class VAML_CORE_EXPORT ToolTip final : public UIElement
{
public:
    ToolTip();

    [[nodiscard]] SetResult set_is_open(bool value);
    [[nodiscard]] bool is_open() const;
    [[nodiscard]] SetResult set_placement(std::int32_t value);
    [[nodiscard]] std::int32_t placement() const;
    [[nodiscard]] SetResult set_horizontal_offset(double value);
    [[nodiscard]] double horizontal_offset() const;
    [[nodiscard]] SetResult set_vertical_offset(double value);
    [[nodiscard]] double vertical_offset() const;

    // Deferred show: open(true) requests the show; the visible flag flips on
    // the change-driven pass (visible() is false until the next applied pass —
    // the deferred-show contract). Returns the requested state.
    [[nodiscard]] bool open(bool value);
    // The CURRENT applied-visible state (deferred show proof).
    [[nodiscard]] bool visible() const;

    [[nodiscard]] vaml::Signal<void()>& opened() { return onOpened_; }
    [[nodiscard]] vaml::Signal<void()>& closed() { return onClosed_; }

    void set_content(std::unique_ptr<UIElement> child);
    [[nodiscard]] UIElement* content() const noexcept;

protected:
    [[nodiscard]] vaml::Size MeasureOverride(const vaml::Size& available) const override;

private:
    vaml::Signal<void()> onOpened_;
    vaml::Signal<void()> onClosed_;
    UIElement* content_ = nullptr;
    bool requested_ = false;
    bool applied_   = false;
};

class VAML_CORE_EXPORT Popup final : public UIElement
{
public:
    Popup();

    [[nodiscard]] SetResult set_is_open(bool value);
    [[nodiscard]] bool is_open() const;
    [[nodiscard]] SetResult set_placement(std::int32_t value);
    [[nodiscard]] std::int32_t placement() const;
    [[nodiscard]] SetResult set_horizontal_offset(double value);
    [[nodiscard]] double horizontal_offset() const;
    [[nodiscard]] SetResult set_vertical_offset(double value);
    [[nodiscard]] double vertical_offset() const;
    // StaysOpen (REAL DP; Bool via Int32, default true — WPF-faithful).
    [[nodiscard]] SetResult set_stays_open(bool value);
    [[nodiscard]] bool stays_open() const;

    [[nodiscard]] bool open(bool value);
    [[nodiscard]] bool visible() const;

    void set_content(std::unique_ptr<UIElement> child);
    [[nodiscard]] UIElement* content() const noexcept;

    [[nodiscard]] vaml::Signal<void()>& opened() { return onOpened_; }
    [[nodiscard]] vaml::Signal<void()>& closed() { return onClosed_; }

protected:
    [[nodiscard]] vaml::Size MeasureOverride(const vaml::Size& available) const override;

private:
    vaml::Signal<void()> onOpened_;
    vaml::Signal<void()> onClosed_;
    UIElement* content_ = nullptr;
    bool requested_ = false;
    bool applied_   = false;
};

// GridSplitter — REAL control (S493). A Thumb-faithful drag handle bound to a
// Grid: dragging maps to a CONSTANT-TIME destination-rect change (the
// S174-composition spirit — a resize is a dest-rect mapping with ZERO object
// churn), re-measuring the adjacent Grid track via a GridLength delta.
// classify -> Solid (the splitter bar).
class VAML_CORE_EXPORT GridSplitter final : public UIElement
{
public:
    GridSplitter();

    // --- ResizeDirection / ResizeBehavior (REAL DPs; Int32) -----------------
    // Columns=0 / Rows=1 (WPF GridResizeDirection). PreviousAndNext=0 / etc.
    [[nodiscard]] SetResult set_resize_direction(std::int32_t value);
    [[nodiscard]] std::int32_t resize_direction() const;
    [[nodiscard]] SetResult set_resize_behavior(std::int32_t value);
    [[nodiscard]] std::int32_t resize_behavior() const;

    // --- DragIncrement / KeyboardIncrement (REAL DPs; Double) ----------------
    [[nodiscard]] SetResult set_drag_increment(double value);
    [[nodiscard]] double drag_increment() const;
    [[nodiscard]] SetResult set_keyboard_increment(double value);
    [[nodiscard]] double keyboard_increment() const;

    // --- Drag resize (the S174 constant-time dest-rect mapping) --------------
    // Resizes an ADJACENT Grid track (row/column) by `delta` pixels. `trackIndex`
    // names the track to resize; `defs` is the Grid's track-definition vector
    // (GridLength list). The delta is applied as a constant-time length change
    // (absolute deltas -> a new Pixel length; star/auto -> the delta is applied
    // against the resolved length and carried back as a Pixel length). Returns
    // a GridLength delta applied (the dest-rect mapped value). Zero object churn.
    [[nodiscard]] vaml::GridLength drag_resize_track(vaml::GridLength current, double delta);

protected:
    [[nodiscard]] vaml::Size MeasureOverride(const vaml::Size& available) const override;
};

// DataGrid — REAL control (S494). A NATIVE VAML analog (NOT a 3rd-party clone):
// ItemsSource/Items + the typed DataGrid*Column set + the editing/selection
// surface (CurrentCell, SelectionUnit, IsReadOnly) + the header/grid-lines
// rendering. classify -> the list/grid fill (Solid).
class VAML_CORE_EXPORT DataGridColumn
{
public:
    // The column kind (TextColumn / CheckBoxColumn / ComboBoxColumn /
    // TemplateColumn). Header is the column header text; Width is the column
    // track width.
    enum class Kind : std::uint8_t
    {
        Text        = 0,
        CheckBox    = 1,
        ComboBox    = 2,
        Template    = 3,
    };

    DataGridColumn() = default;
    DataGridColumn(Kind k, std::string header, float width)
        : kind_(k), header_(std::move(header)), width_(width) {}

    [[nodiscard]] Kind kind() const noexcept { return kind_; }
    [[nodiscard]] const std::string& header() const noexcept { return header_; }
    [[nodiscard]] float width() const noexcept { return width_; }
    void set_width(float w) noexcept { width_ = w; }

private:
    Kind        kind_   = Kind::Text;
    std::string header_ = {};
    float       width_  = 100.0f;
};

class VAML_CORE_EXPORT DataGrid final : public UIElement
{
public:
    DataGrid();

    // --- Items / ItemsSource (REAL DPs; String) -----------------------------
    [[nodiscard]] SetResult set_items(std::string value);
    [[nodiscard]] std::string items() const;
    [[nodiscard]] SetResult set_items_source(std::string value);
    [[nodiscard]] std::string items_source() const;

    // --- Columns (owned typed set) ------------------------------------------
    void set_columns(std::vector<DataGridColumn> cols);
    [[nodiscard]] const std::vector<DataGridColumn>& columns() const noexcept;
    // Adds one column; returns the new count. The typed DataGrid*Column authoring
    // (TextColumn/CheckBoxColumn/ComboBoxColumn/TemplateColumn) is exposed via
    // this owned set + the auto-generate helper below.
    [[nodiscard]] std::size_t add_column(DataGridColumn col);

    // --- Selection (SelectedIndex / SelectedItem) ---------------------------
    [[nodiscard]] SetResult set_selected_index(std::int32_t index);
    [[nodiscard]] std::int32_t selected_index() const;
    [[nodiscard]] std::string selected_item() const;

    // --- SelectionUnit / IsReadOnly (REAL DPs; Int32 / Bool via Int32) ------
    [[nodiscard]] SetResult set_selection_unit(std::int32_t value);
    [[nodiscard]] std::int32_t selection_unit() const;
    [[nodiscard]] SetResult set_is_read_only(bool value);
    [[nodiscard]] bool is_read_only() const;

    // --- HeadersVisibility / AutoGenerateColumns (REAL DPs; Int32) ----------
    [[nodiscard]] SetResult set_headers_visibility(std::int32_t value);
    [[nodiscard]] std::int32_t headers_visibility() const;
    [[nodiscard]] SetResult set_auto_generate_columns(bool value);
    [[nodiscard]] bool auto_generate_columns() const;

    // --- CurrentCell / Fraction ---------------------------------------------
    // The editing/selection surface: the current cell (row, column) + the
    // fill-fraction proof (the "list/grid fill" the classifier consumes).
    void set_current_cell(std::int32_t row, std::int32_t col);
    [[nodiscard]] std::int32_t current_row() const noexcept;
    [[nodiscard]] std::int32_t current_column() const noexcept;

    [[nodiscard]] vaml::Signal<void()>& selection_changed() { return onSelectionChanged_; }

protected:
    [[nodiscard]] vaml::Size MeasureOverride(const vaml::Size& available) const override;

private:
    vaml::Signal<void()> onSelectionChanged_;
    std::vector<DataGridColumn> cols_;
    void apply_selection(std::int32_t newIndex);
};

// Startup registration: registers every built-in element type name into
// ElementFactoryRegistry::global(). Called once at process startup (single-
// threaded) before any VAML document is parsed; S092/S286 reader paths consume
// it. Registering twice is a no-op guard against double startup population.
void VAML_CORE_EXPORT register_builtin_elements();

} // namespace vaml
