#pragma once
// S280_1 (dep:S280) — Static binding validation in vamlcc, per ADR-016.
//
// Each {Binding Path} attribute is validated against its STATIC root type via
// the property registry (S032–S039): a typo / wrong hop type becomes a
// COMPILE-TIME error naming the source line/col (Q1=A, accepted).
//
// SCOPE — what IS validated here (compile-time is the rule):
//   * The TARGET attribute key of a {Binding Path} on a known element must be a
//     registered DependencyProperty (a typo → UnknownTargetProperty).
//   * The path text must be well-formed (S240 PropertyPath Class A; a malformed
//     path → MalformedPath).
//   * When the binding ROOT is a statically DependencyObject-derived element
//     type (registered in the element factory table), EVERY hop's property name
//     must resolve to a registered DP (typo / wrong hop type → WrongHopType).
//
// SCOPE — what is NOT validated here (the documented dynamic escape hatch):
//   * Roots that are TRUE runtime-typed — an object/dynamic DataContext
//     (ObservableObject VM binding, e.g. "{Binding Title}" whose root is not a
//     known element-type DP set) or an untyped template item — are NOT
//     static-checked. They defer to bind-time Class A at S242_1 (Strict) and
//     are COUNTED toward the coverage ratio (S826_1). The WPF repo corroborates:
//     DP roots are statically checkable, but plain CLR/VM ObservableObject
//     properties have no compile-time registry (resolved at runtime), so they
//     cannot be registry-validated at compile time.

#include "vaml/tools/Vamlc.h" // CompiledProgram, VamlcException, VamlcError

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace vaml::tools {

// S280_1: the binding-validation error taxonomy (distinct from VamlcError — it
// names the exact static-binding failure the validator can produce).
enum class BindingCompileError : std::uint8_t
{
    None,                // statically valid (or dynamic — deferred)
    UnknownTargetProperty, // the TARGET attribute key is not a DP for its element
    MalformedPath,       // the binding path text is malformed (S240 Class A)
    WrongHopType,        // a path hop property is not a registered DP of its root
    UnknownElementType,  // (reserved — folded into WrongHopType path checks here)
};

// S280_1: one binding-validation OUTCOME.
struct BindingValidation
{
    std::size_t elementId = 0; // the compiled element carrying the attribute
    std::string attributeKey;  // the FULL target attribute key (e.g. "Text")
    std::string bindingPath;   // the parsed path text (e.g. "Toggle.IsChecked")
    std::size_t line = 0;      // 1-based source line of the element
    std::size_t col  = 0;      // 1-based source column
    BindingCompileError error = BindingCompileError::None;
    bool dynamic = false;      // runtime-typed root (deferred, NOT an error)
};

// S280_1: the result of validating ONE CompiledProgram.
struct BindingValidationResult
{
    std::vector<BindingValidation> bindings; // every binding attribute, doc order
    std::size_t staticValid = 0;             // static (element-rooted) that passed
    std::size_t dynamicCount = 0;            // DYNAMIC (deferred; counted → S826_1)

    // True when every binding that WAS statically validated passed (no
    // compile-time error). Because validate_bindings throws on the FIRST static
    // failure, a returned result is always ok() == true.
    [[nodiscard]] bool ok() const noexcept { return true; }
};

// Human-readable description of a BindingCompileError (for what() / detail).
VAML_TOOLS_EXPORT std::string describe(BindingCompileError e) noexcept;

// S280_1: validates every compiled element's {Binding Path} attributes. On the
// FIRST static failure it THROWS VamlcException(BindingInvalid) naming the
// source line/col (fail-fast per ADR-016). Dynamic roots are recorded
// (dynamicCount), never an error. Fills `counts.dynamicBindings` so the
// coverage ratio (S826_1) can observe them. Returns the per-binding result.
[[nodiscard]] VAML_TOOLS_EXPORT BindingValidationResult validate_bindings(
    const CompiledProgram& prog, VamlcCounts& counts);

} // namespace vaml::tools
