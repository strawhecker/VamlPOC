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

// Text — REAL control (S400). RESOLVES the S087 MINIMAL REAL seed at its own
// sprint (NO-GAP, not a backfill): re-based onto UIElement so it participates
// in the visual tree + Measure/Arrange layout, and supplies a REAL per-control
// material (MaterialKind::Text via the S400 classifier) so the S207 encoder +
// the vulkan build_text_glyph_items emit SDF glyph quads.
//
// WPF-FAITHFUL surface (title alignment, review-confirmed):
//   - Text   (String, default "") — the content.
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
// with default_text_metric (a fontSize-based estimate, device-free) so the Text
// measures to its WRAPPED line extent; the real per-glyph advance + UV data
// come from the vulkan SDF atlas at RENDER time (build_text_glyph_items), where
// the wrap is RE-COMPUTED with the REAL advances — the Measure basis and the
// render basis use the same wrap RULE, so the box they occupy matches. NO-GAP:
// real atlas-driven metrics + glyph UVs + the Text material are authored at the
// renderer (S400 build_text_glyph_items), NOT silently backfilled here.
class VAML_CORE_EXPORT Text final : public UIElement
{
public:
    Text();

    // --- Text (REAL DP; String, default "") ------------------------------
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
    // S400 real Text algorithm (see Element.cpp): MeasureOverride runs the S400
    // text_layout_plan (default_text_metric basis, device-free) and returns the
    // wrapped extent {measuredWidth, measuredHeight}. ArrangeOverride is the
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

// [MINIMAL REAL] Element Window — RESOLVED BY S440 (Sprint 11, app MainView). Do NOT extend silently; do NOT backfill.
class VAML_CORE_EXPORT Window final : public DependencyObject
{
public:
    Window();
};

// Startup registration: registers every built-in element type name into
// ElementFactoryRegistry::global(). Called once at process startup (single-
// threaded) before any VAML document is parsed; S092/S286 reader paths consume
// it. Registering twice is a no-op guard against double startup population.
void VAML_CORE_EXPORT register_builtin_elements();

} // namespace vaml
