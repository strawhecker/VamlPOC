// S280_1 (dep:S280) — Static binding validation in vamlcc, per ADR-016 (Q1=A).
//
// Each {Binding Path} attribute is validated against its STATIC root type via
// the property registry (S032–S039). A typo / wrong hop type becomes a
// COMPILE-TIME error naming the source line/col. Roots that are truly runtime-
// typed (an object/dynamic ObservableObject DataContext, an untyped template
// item) are NOT validated here — they defer to bind-time Class A (S242_1
// Strict) and count toward the coverage ratio (S826_1).
//
// These tests drive the REAL compiler seam (vaml::tools::compile, which runs
// validate_bindings inline) + the validator directly, proving:
//   * a typo in the TARGET attribute key → UnknownTargetProperty + line/col
//   * an element-rooted binding hop typo → WrongHopType + line/col
//   * a rootless / non-element root binding ("{Binding Title}") → DYNAMIC,
//     deferred (never an error), counted toward dynamicBindings (S826_1)
//   * a malformed path → MalformedPath
//   * compile_text maps the first static failure into a typed CompileResult.
//
// Registers S280_1-unique global DPs (S039 discipline) to give bindings a
// real target registry. ADR-018 zero locks.

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/DependencyProperty.h"
#include "vaml/core/PropertyFlags.h"
#include "vaml/core/PropertyMetadata.h"
#include "vaml/core/Value.h"
#include "vaml/tools/Vamlc.h"
#include "vaml/tools/BindingValidator.h"

#include <string>
#include <unordered_set>

using vaml::DependencyProperty;
using vaml::PropertyFlags;
using vaml::PropertyMetadata;
using vaml::Value;
using vaml::tools::CompileResult;
using vaml::tools::CompiledProgram;
using vaml::tools::BindingCompileError;
using vaml::tools::VamlcError;
using vaml::tools::VamlcCounts;

namespace {

std::unordered_set<std::string>& seen_names()
{
    static std::unordered_set<std::string> seen;
    return seen;
}

const DependencyProperty& registerGlobal(const std::string& name, vaml::ValueType type,
                                         PropertyFlags flags, Value def)
{
    if (seen_names().insert(name).second)
    {
        return DependencyProperty::Register(name, type, "S280_1_Owner",
                                            PropertyMetadata::WithDefault(def, flags));
    }
    return DependencyProperty::Get(name);
}

// Registers the S280_1-unique global DPs (a target + a hop) so bindings have a
// real property registry to validate against.
void registerS280_1Properties()
{
    // Target DP for bindings on a StackPanel (Text carries user text)...
    // plus a registered hop property for element-rooted paths.
    (void)registerGlobal("S280_1Text", vaml::ValueType::String, PropertyFlags::None,
                         Value::String(""));
    (void)registerGlobal("S280_1IsChecked", vaml::ValueType::Bool, PropertyFlags::None,
                         Value::Bool(false));
}

int vk_i(VamlcError k) { return static_cast<int>(k); }

} // namespace

// ---------------------------------------------------------------------------
// 1. A valid ELEMENT-rooted binding validates (static, no error, not dynamic)
// ---------------------------------------------------------------------------

TEST_CASE("S280_1: an element-rooted binding with a registered hop validates")
{
    registerS280_1Properties();
    // "Text" is a registered element type; S280_1IsChecked is a registered DP.
    // Binding targets S280_1Text (a registered DP). Static → validated, NOT
    // counted dynamic.
    const CompileResult r = vaml::tools::compile_text(
        "<StackPanel S280_1Text=\"{Binding Text.S280_1IsChecked}\"/>");
    SHOULD(ShouldlyIntegral(static_cast<int>(r.ok)).ShouldBe(1));
    SHOULD(ShouldlyIntegral(r.counts.dynamicBindings).ShouldBe(0u));
}

// ---------------------------------------------------------------------------
// 2. A TARGET attribute-key typo → UnknownTargetProperty + line/col
// ---------------------------------------------------------------------------

TEST_CASE("S280_1: a target attribute-key typo is UnknownTargetProperty at compile time")
{
    registerS280_1Properties();
    // "S280_1Tex" (typo of S280_1Text) is not a registered DP.
    bool threw = false;
    try
    {
        (void)vaml::tools::compile("<StackPanel S280_1Tex=\"{Binding Title}\"/>");
    }
    catch (const vaml::tools::VamlcException& e)
    {
        threw = true;
        SHOULD(ShouldlyIntegral(vk_i(e.kind())).ShouldBe(vk_i(VamlcError::BindingInvalid)));
        SHOULD(ShouldlyIntegral(e.line()).ShouldBe(1u));
        SHOULD(ShouldlyIntegral(e.col()).ShouldBe(1u));
    }
    SHOULD(ShouldlyIntegral(static_cast<int>(threw)).ShouldBe(1));

    const CompileResult r = vaml::tools::compile_text("<StackPanel S280_1Tex=\"{Binding Title}\"/>");
    SHOULD(ShouldlyIntegral(static_cast<int>(r.ok)).ShouldBe(0));
    SHOULD(ShouldlyIntegral(vk_i(r.kind)).ShouldBe(vk_i(VamlcError::BindingInvalid)));
}

// ---------------------------------------------------------------------------
// 3. An attelement-rooted binding hop typo → WrongHopType
// ---------------------------------------------------------------------------

TEST_CASE("S280_1: an element-rooted binding hop typo is WrongHopType at compile time")
{
    registerS280_1Properties();
    // Root "Text" resolves; hop "S280_1Misspel" is not a registered DP.
    bool threw = false;
    try
    {
        (void)vaml::tools::compile(
            "<StackPanel S280_1Text=\"{Binding Text.S280_1Misspel}\"/>");
    }
    catch (const vaml::tools::VamlcException& e)
    {
        threw = true;
        SHOULD(ShouldlyIntegral(vk_i(e.kind())).ShouldBe(vk_i(VamlcError::BindingInvalid)));
    }
    SHOULD(ShouldlyIntegral(static_cast<int>(threw)).ShouldBe(1));
}

// ---------------------------------------------------------------------------
// 4. A ROOTLESS binding ("{Binding Title}") is DYNAMIC → deferred, NOT an error
// ---------------------------------------------------------------------------

TEST_CASE("S280_1: a rootless binding (VM data-context root) is dynamic, never an error")
{
    registerS280_1Properties();
    // "Title" is not a registered element type → the root is runtime-typed (a
    // VM ObservableObject data context). It defers to bind-time Class A
    // (S242_1 Strict) and counts toward S826_1 — compile succeeds.
    const CompileResult r = vaml::tools::compile_text(
        "<StackPanel S280_1Text=\"{Binding Title}\"/>");
    SHOULD(ShouldlyIntegral(static_cast<int>(r.ok)).ShouldBe(1));
    SHOULD(ShouldlyIntegral(r.counts.dynamicBindings).ShouldBe(1u));
}

// ---------------------------------------------------------------------------
// 5. A malformed path → MalformedPath
// ---------------------------------------------------------------------------

TEST_CASE("S280_1: a malformed binding path is MalformedPath at compile time")
{
    registerS280_1Properties();
    // "Toggle..IsChecked" is a Class A syntax error (S240 ConsecutiveDot).
    bool threw = false;
    try
    {
        (void)vaml::tools::compile(
            "<StackPanel S280_1Text=\"{Binding Toggle..IsChecked}\"/>");
    }
    catch (const vaml::tools::VamlcException& e)
    {
        threw = true;
        SHOULD(ShouldlyIntegral(vk_i(e.kind())).ShouldBe(vk_i(VamlcError::BindingInvalid)));
    }
    SHOULD(ShouldlyIntegral(static_cast<int>(threw)).ShouldBe(1));
}

// ---------------------------------------------------------------------------
// 6. validate_bindings returns the per-binding outcome + counts dynamic roots
// ---------------------------------------------------------------------------

TEST_CASE("S280_1: validate_bindings fills dynamicBindings + returns outcomes")
{
    registerS280_1Properties();
    // Compile a doc with one dynamic ({Binding Title}) + one static
    // (element-rooted) binding.
    const std::string doc =
        "<StackPanel S280_1Text=\"{Binding Title}\" "
        "S280_1IsChecked=\"{Binding Text.S280_1IsChecked}\"/>";
    const vaml::tools::CompiledProgram prog = vaml::tools::compile(doc);

    vaml::tools::VamlcCounts counts;
    const vaml::tools::BindingValidationResult res =
        vaml::tools::validate_bindings(prog, counts);
    // One dynamic (Title) + one static (Text.S280_1IsChecked).
    SHOULD(ShouldlyIntegral(res.dynamicCount).ShouldBe(1u));
    SHOULD(ShouldlyIntegral(res.staticValid).ShouldBe(1u));
    SHOULD(ShouldlyIntegral(res.bindings.size()).ShouldBe(2u));
    SHOULD(ShouldlyIntegral(counts.dynamicBindings).ShouldBe(1u));
    // The dynamic one is marked dynamic; the static one is not.
    SHOULD(ShouldlyIntegral(res.bindings[0].dynamic ? 1 : 0).ShouldBe(1));
    SHOULD(ShouldlyIntegral(res.bindings[1].dynamic ? 1 : 0).ShouldBe(0));
}
