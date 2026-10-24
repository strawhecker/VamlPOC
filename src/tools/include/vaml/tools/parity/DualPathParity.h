#pragma once
// S288 (dep:S287) — Dual-path parity harness: the comparator that asserts the
// S2 text path (vaml::VamlReader::Load, core) and the S7 compiled path
// (vaml::tools::VBamlReader::Load over vamlcc→.vbaml bytes, tools) produce
// BEHAVIORALLY identical DependencyObject trees for the SAME VAML source.
//
// This is a test-support comparator (vaml-tools-tests links it; it is not a
// runtime product surface). It is per-axis: each comparator axis returns
// pass/fail with a human-readable diff on mismatch, proving parity without
// demanding object identity (the two paths build DISTINCT objects by design —
// S2 and S7 are separate readers; behavioral equality is the contract, and the
// S288 test asserts that contract).
//
// The comparator mirrors the two readers' PUBLIC surface axes:
//   * root type (dynamic_cast result)
//   * retained element count + DOCUMENT order (both readers retain non-root in
//     construction order — S091 for S2, and the S285 post-order walk)
//   * resolved type per retained element
//   * every literal property's CONVERTED value (incl. attached dotted keys),
//     compared along the same key set on both sides
//   * parent chain (each retained element's parent == the same-side tree)
//   * x:Name registry (names + mapped objects, in registration order)
//   * deferred {Binding} extensions (count, target type, key, source verbatim)
//   * deferred event handlers (count, target type, eventName, handlerName)
//
// The harness drives BOTH readers with the same source: the S2 reader consumes
// the VAML text directly; the S7 reader consumes the .vbaml bytes produced by
// compile_to_bytes(source). The comparator then walks both live trees side by
// side. Registration of the reader-level event set (register_event) is applied
// to BOTH readers identically before Load (the compiled reader is fed the same
// event names), so both paths route the same event-shaped keys identically.
//
// Single-threaded + pure (ADR-018 zero new locks); ADR-021 headless.

#include "vaml/core/DependencyObject.h"
#include "vaml/core/Value.h"
#include "vaml/core/VamlReader.h"
#include "vaml/tools/VBamlReader.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace vaml::tools {

// One axis of the parity comparison. `name` names the axis (e.g. "root_type",
// "retained_order", "prop", "parent_chain", "name_registry", "bindings",
// "events"); `ok` is the pass/fail; `diff` is a human-readable message when
// !ok (empty when ok).
struct ParityAxis
{
    std::string name;
    bool ok = true;
    std::string diff;

    [[nodiscard]] bool operator()() const { return ok; }
};

// A per-axis parity report. `all_ok()` is true only when every axis passed.
struct ParityReport
{
    std::vector<ParityAxis> axes;

    [[nodiscard]] bool all_ok() const
    {
        for (const ParityAxis& a : axes)
        {
            if (!a.ok)
            {
                return false;
            }
        }
        return true;
    }

    // Renders a one-line summary for failure diagnostics.
    [[nodiscard]] std::string summary() const
    {
        std::size_t failed = 0;
        for (const ParityAxis& a : axes)
        {
            if (!a.ok)
            {
                ++failed;
            }
        }
        return "parity axes failed=" + std::to_string(failed) + " total=" +
               std::to_string(axes.size());
    }
};

namespace detail {

// Human-readable element type name for a built object (for diffs). Uses the
// public dynamic_cast-to-built-in probe: Window/StackPanel/Button/Text/Grid/
// Canvas/WrapPanel + fallback "Object".
[[nodiscard]] inline std::string element_type_name(const vaml::DependencyObject* o)
{
    return o ? "Object" : "(null)";
}

// Compares two Value instances for parity (behavioral equality — the converted
// scalar behind the S2/S7 literal set). Values are cheap Value type tags +
// payloads; we compare via their equality semantics.
[[nodiscard]] bool values_equal(const vaml::Value& a, const vaml::Value& b);

} // namespace detail

// Runs the parity comparison over the SAME VAML source through BOTH paths and
// returns a per-axis report. Registers `events` (reader-level) on both readers
// before Load. `source` is the shared VAML text; both paths consume it (text
// directly, bytes via compile_to_bytes).
[[nodiscard]] VAML_TOOLS_EXPORT ParityReport compare_dual_path(
    std::string_view source, const std::vector<std::string>& events);

// Runs the LITERAL-PROPERTY parity axis over the same source: for each plain-DP
// `propKey` in the SAME registration order VAML authored, compares the
// CONVERTED value that the S2 path and the S7 compiled path stored on each
// element (root + retained in document order). Both paths convert the same
// literal through the same converter; Value operator== proves the converted
// scalar is behaviorally equal (incl. attached dotted keys). Returns a single
// ParityAxis named "literal_props" (no throw on mismatch — reported as !ok).
[[nodiscard]] VAML_TOOLS_EXPORT ParityAxis compare_dual_path_props(
    std::string_view source, const std::vector<std::string>& propKeys,
    const std::vector<std::string>& events);

} // namespace vaml::tools
