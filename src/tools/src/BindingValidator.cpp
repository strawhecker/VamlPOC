// S280_1 (dep:S280) — Static binding validation in vamlcc, per ADR-016.
//
// Each {Binding Path} attribute is validated against its STATIC root type via
// the property registry (S032–S039). A typo / wrong hop type becomes a
// COMPILE-TIME error naming the source line/col. Roots that are truly runtime-
// typed (an object/dynamic ObservableObject DataContext, an untyped template
// item) are NOT validated here — they defer to bind-time Class A (S242_1
// Strict) and COUNT toward the coverage ratio (S826_1). This is the "compile-
// time is the rule; dynamic root is the documented, Strict-excludable escape
// hatch" contract, with Q1=A accepted.
//
// The WPF repo corroborates the split: DependencyObject-DP roots are
// statically checkable, but plain CLR/VM ObservableObject properties have no
// compile-time registry (resolved at runtime) — so VM bindings cannot be
// registry-validated at compile time and are correctly deferred.
//
// ADR-018: zero locks — the validator reads the static registries, never
// mutates shared state.

#include "vaml/tools/BindingValidator.h"

#include "vaml/core/DependencyProperty.h"
#include "vaml/core/ElementFactoryRegistry.h"
#include "vaml/core/PropertyPath.h" // PropertyPath::try_parse (MalformedPath)

#include <string>
#include <string_view>
#include <vector>

namespace vaml::tools {

namespace {

// Returns the element-root of a binding path: the text before the first '.'.
// "{Binding Toggle.IsChecked}" → "Toggle"; "{Binding Title}" → "" (dynamic).
std::string_view path_root(std::string_view path)
{
    const std::size_t dot = path.find('.');
    return dot == std::string_view::npos ? std::string_view{} : path.substr(0, dot);
}

} // namespace

VAML_TOOLS_EXPORT
std::string describe(BindingCompileError e) noexcept
{
    switch (e)
    {
        case BindingCompileError::UnknownTargetProperty:
            return "the target attribute is not a registered dependency property";
        case BindingCompileError::MalformedPath:
            return "the binding path is malformed";
        case BindingCompileError::WrongHopType:
            return "a binding hop property is not a registered dependency property";
        case BindingCompileError::UnknownElementType:
            return "the binding root names an unknown element type";
        case BindingCompileError::None:
        default:
            return "valid";
    }
}

VAML_TOOLS_EXPORT
BindingValidationResult validate_bindings(const CompiledProgram& prog, VamlcCounts& counts)
{
    BindingValidationResult out;
    for (const ElementRecord& e : prog.elements)
    {
        for (const AttributeRecord& a : e.attributes)
        {
            if (!a.isBinding)
            {
                continue;
            }

            BindingValidation v;
            v.elementId = e.id;
            v.attributeKey = a.key;
            v.bindingPath = a.bindingPath;
            v.line = e.line;
            v.col = e.col;

            // 1. The TARGET attribute key must be a registered DP — a typo in
            //    the target property is a compile-time UnknownTargetProperty.
            //    Every binding (static OR dynamic root) targets a DP.
            if (vaml::DependencyProperty::TryGet(a.key) == nullptr)
            {
                v.error = BindingCompileError::UnknownTargetProperty;
                out.bindings.push_back(v);
                throw VamlcException(VamlcError::BindingInvalid,
                                     "binding '" + a.bindingPath + "' on '" + a.key +
                                         "' is invalid: " + describe(v.error),
                                     e.line, e.col);
            }

            // 2. Parse the path (S240 Class A). A malformed path is a
            //    compile-time error for ANY binding — it cannot be deferred.
            vaml::PropertyPath parsed;
            vaml::PropertyPathError perr;
            if (!vaml::PropertyPath::try_parse(a.bindingPath, parsed, &perr))
            {
                v.error = BindingCompileError::MalformedPath;
                out.bindings.push_back(v);
                throw VamlcException(VamlcError::BindingInvalid,
                                     "binding '" + a.bindingPath + "' on '" + a.key +
                                         "' is invalid: " + describe(v.error),
                                     e.line, e.col);
            }

            // 3. Resolve the root. A rootless binding ("{Binding Title}") or a
            //    root that is not a registered element type is DYNAMIC →
            //    deferred, counted, NEVER an error here (S242_1 Strict at bind
            //    time; S826_1 coverage).
            const std::string_view root = path_root(a.bindingPath);
            if (root.empty() || !vaml::ElementFactoryRegistry::global().is_registered(root))
            {
                v.dynamic = true;
                out.bindings.push_back(v);
                ++out.dynamicCount;
                continue;
            }

            // 4. Element-rooted binding: validate every HOP's property name
            //    (the segments AFTER the root) is a registered DP (typo / wrong
            //    hop type → WrongHopType). The ROOT segment is the element REF
            //    (a named element, not a property) — it is skipped. The hop
            //    names are checked against the S032–S039 property registry
            //    (Q1=A: "via the property registry"). Per-type hop sets are not
            //    enumerable from the GLOBAL name-keyed registry — the honest
            //    compile-time level is "the hop names a real registered DP".
            const std::vector<vaml::PropertySegment>& segs = parsed.segments();
            for (std::size_t i = 1; i < segs.size(); ++i)
            {
                const vaml::PropertySegment& seg = segs[i];
                if (seg.kind == vaml::PropertySegmentKind::Property &&
                    vaml::DependencyProperty::TryGet(seg.name) == nullptr)
                {
                    v.error = BindingCompileError::WrongHopType;
                    out.bindings.push_back(v);
                    throw VamlcException(VamlcError::BindingInvalid,
                                         "binding '" + a.bindingPath + "' on '" + a.key +
                                             "' is invalid: hop '" + seg.name + "' " +
                                             describe(v.error),
                                         e.line, e.col);
                }
            }

            v.error = BindingCompileError::None;
            out.bindings.push_back(v);
            ++out.staticValid;
        }
    }
    counts.dynamicBindings = out.dynamicCount;
    return out;
}

} // namespace vaml::tools
