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
#include "vaml/core/UIElement.h"  // StackPanel/Grid's base (S129/S131 re-base — RESOLVED seeds)

#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

namespace vaml {

// [MINIMAL REAL] Element Button — RESOLVED BY S401 (Sprint 10). Do NOT extend silently; do NOT backfill.
class VAML_CORE_EXPORT Button final : public DependencyObject
{
public:
    Button();
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
    // Pimpl so the owned row/column definition vectors are complete here.
    struct Impl;
    std::unique_ptr<Impl> impl_;
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

// [MINIMAL REAL] Element Text — RESOLVED BY S400 (Sprint 10). Do NOT extend silently; do NOT backfill.
class VAML_CORE_EXPORT Text final : public DependencyObject
{
public:
    Text();
};

// [MINIMAL REAL] Element TextBox — RESOLVED BY S402 (Sprint 10). Do NOT extend silently; do NOT backfill.
class VAML_CORE_EXPORT TextBox final : public DependencyObject
{
public:
    TextBox();
};

// [MINIMAL REAL] Element CheckBox — RESOLVED BY S404 (Sprint 10). Do NOT extend silently; do NOT backfill.
class VAML_CORE_EXPORT CheckBox final : public DependencyObject
{
public:
    CheckBox();
};

// [MINIMAL REAL] Element Slider — RESOLVED BY S403 (Sprint 10). Do NOT extend silently; do NOT backfill.
class VAML_CORE_EXPORT Slider final : public DependencyObject
{
public:
    Slider();
};

// [MINIMAL REAL] Element ScrollViewer — RESOLVED BY S405 (Sprint 10). Do NOT extend silently; do NOT backfill.
class VAML_CORE_EXPORT ScrollViewer final : public DependencyObject
{
public:
    ScrollViewer();
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
