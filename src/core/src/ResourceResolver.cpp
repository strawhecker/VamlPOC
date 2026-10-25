// S361 (dep:S360) — ResourceResolver definitions. See ResourceResolver.h.

#include "vaml/core/ResourceResolver.h"

#include "vaml/core/BindingAccess.h"      // S250_1 view diagnostics channel (dynamic-hold note)
#include "vaml/core/BindingDiagnostics.h" // BindingDiagnostic + reason (ADR-016)
#include "vaml/core/PropertyId.h"         // PropertyId (Set target)
#include "vaml/core/Value.h"              // Value

#include <utility>

namespace vaml {

namespace {

// Applies a resolved Plain value to `element`'s `prop`. `out` receives false if
// the value could not be set as a scalar (wrong tag / an Object which does not
// derive the target's scalar type). Returns true on a successfully-set DP.
[[nodiscard]] bool apply_plain_to_element(UIElement& element, std::string_view prop,
                                          const Value& value, bool* outOk)
{
    *outOk = true;
    if (value.isNull())
    {
        // A Null plain value — set as Null (a valid DP state) is skipped; only
        // genuine values are pushed.
        return true;
    }
    try
    {
        const SetResult r = element.Set(PropertyId(prop), value);
        *outOk = true;
        (void)r;
        return true;
    }
    catch (...)
    {
        // Wrong-tag / unregistered target — a DP-set failure is a Class B-
        // style runtime outcome (this layer reports, never throws).
        *outOk = false;
        return false;
    }
}

} // namespace

VAML_CORE_EXPORT
std::size_t ResourceResolver::apply_markup(UIElement& element, std::string_view prop,
                                           std::string_view source,
                                           std::vector<DynamicResourceLink>& dynamicLinks)
{
    const ResourceMarkup m = parse_resource_markup(source);
    if (!m.recognized || m.malformed)
    {
        // Not a resource reference (or a malformed one) — not this layer's row.
        return 0;
    }

    if (m.kind == ResourceReferenceKind::Static)
    {
        // STATIC: resolve ONCE now. A missing static key is a Class A error —
        // fail loud via the SHARED decision-3 ResourceLookupException(MissingKey).
        const ResourceValue* rv = find_for_element(element, m.key);
        if (rv == nullptr)
        {
            throw ResourceLookupException(
                ResourceLookupError::MissingKey, std::string(m.key),
                "static resource '" + std::string(m.key) + "' not found");
        }
        // Apply a Plain value directly; an Object resource (Brush/Style) is NOT
        // a scalar DP value — it is applied by its own consumer (S364/S400).
        if (rv->kind() == ResourceValue::Kind::Plain)
        {
            bool ok = false;
            (void)apply_plain_to_element(element, prop, rv->value(), &ok);
            return ok ? 1u : 0u;
        }
        return 0u; // Object resource — not applied on the scalar DP path here
    }

    // DYNAMIC: apply the first value now (if resolvable) + record the live link.
    dynamicLinks.push_back(DynamicResourceLink{ &element, std::string(prop), std::string(m.key) });
    const ResourceValue* rv = find_for_element(element, m.key);
    if (rv != nullptr && rv->kind() == ResourceValue::Kind::Plain)
    {
        bool ok = false;
        (void)apply_plain_to_element(element, prop, rv->value(), &ok);
        return ok ? 1u : 0u;
    }
    // Transiently missing / non-Plain — hold (does NOT throw; the S361 dynamic
    // semantics). Emit a diagnostic note via the S250_1 channel (ADR-016).
    BindingDiagnostic note;
    note.targetProp = std::string(prop);
    note.reasonCode = BindingReasonCode::SetTargetFailed;
    note.sourcePath = std::string(m.key);
    note.direction  = BindingDirection::OneWay;
    note.detail     = "dynamic resource '" + std::string(m.key) +
                      "' resolved to a non-Plain/absent value; holding last value";
    vaml::view_binding_diagnostics().emit(note);
    return 0u; // applied 0 this call; the link stays for re-evaluation
}

VAML_CORE_EXPORT
std::size_t ResourceResolver::reapply_dynamic_resources(
    const std::vector<DynamicResourceLink>& dynamicLinks)
{
    std::size_t reapplied = 0;
    for (const DynamicResourceLink& link : dynamicLinks)
    {
        const ResourceValue* rv = find_for_element(*link.target, link.key);
        if (rv != nullptr && rv->kind() == ResourceValue::Kind::Plain)
        {
            bool ok = false;
            const bool set = apply_plain_to_element(*link.target, link.prop, rv->value(), &ok);
            if (set && ok)
            {
                ++reapplied;
            }
        }
        // A transiently-missing key HOLDs (no throw; stays recorded).
    }
    return reapplied;
}

} // namespace vaml
