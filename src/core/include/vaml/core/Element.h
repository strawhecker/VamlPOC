#pragma once
// VAML built-in element classes — MINIMAL REAL seeds (S087) + the first real
// re-based control (StackPanel, S129).
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
//     Button      -> S401   Grid -> S131   Text -> S400
//     TextBox     -> S402   CheckBox   -> S404   Slider -> S403
//     ScrollViewer-> S405   Window     -> S440
//
// Element.obj (Element.cpp) hosts register_builtin_elements() — the startup
// registration call that populates ElementFactoryRegistry::global() once.

#include "vaml/core/DependencyObject.h"
#include "vaml/core/UIElement.h" // StackPanel's base (S129 re-base — RESOLVED seed)

#include <cstdint>
#include <memory>
#include <string_view>

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

// [MINIMAL REAL] Element Grid — RESOLVED BY S131 (Sprint 3 layout). Do NOT extend silently; do NOT backfill.
class VAML_CORE_EXPORT Grid final : public DependencyObject
{
public:
    Grid();
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
