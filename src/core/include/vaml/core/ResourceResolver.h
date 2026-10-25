#pragma once
// VAML ResourceResolver — S361 static/dynamic resource resolution over the S360
// three-scope lookup.
//
// S361 (dep:S360) — StaticResource/DynamicResource resolution; merge precedence;
// missing-key errors. RESOLVES the S092-recorded x:Key -> S361 deferral AND the
// S252/S253-recorded MarkupExtensionKind::None -> StaticResource/DynamicResource
// deferral.
//
// STATIC (S361): resolved ONCE at apply time from the S360 three-scope lookup
// anchored at the element; a missing key -> ResourceLookupException(MissingKey)
// (the shared decision-3 error; fail loud, Class A — the reader/tests surface it
// as a structured error).
//
// DYNAMIC (S361): a DEFERRED LIVE LINK. `resolve_dynamic` applies the first
// value now and records a DynamicResourceLink (the target element + property +
// the key). `reapply_dynamic_resources` re-runs find_for_element for each link
// and RE-PUSHES the resolved value to the target DP. It NEVER THROWS on a
// transiently-missing key: a dynamic link whose source is momentarily absent
// HOLDS (the previous/first value stays; the S361 diagnostic note is emitted via
// the S250_1 channel, ADR-016). The actual change-notification (re-push when a
// source dictionary key CHANGES) is realized here by the app/viewer calling
// reapply_dynamic_resources after a dictionary key change (the S041 subscriber
// wiring that auto-forwards dictionary changes is a recorded forward refinement
// — the reapply entry is the S361-approved re-evaluation seam, NOT silent).
//
// ADR-018: ZERO new locks (single-threaded UI use).

#include "vaml/core/DependencyObject.h" // target DP push
#include "vaml/core/ResourceDictionary.h" // ResourceValue, ResourceLookupError, find_for_element
#include "vaml/core/ResourceMarkup.h"  // ResourceMarkup, ResourceReferenceKind
#include "vaml/core/UIElement.h"       // UIElement (lookup anchor + dynamic link target)
#include "vaml/core/export.h"

#include <string>
#include <string_view>
#include <vector>

namespace vaml {

// ONE recorded dynamic resource link (the live-link re-evaluation record).
struct VAML_CORE_EXPORT DynamicResourceLink
{
    UIElement* target = nullptr;   // the element the markup was authored on
    std::string prop;              // the target property name
    std::string key;               // the {DynamicResource <key>} key
};

class VAML_CORE_EXPORT ResourceResolver
{
public:
    // RESOLVES a resource-markup source (`{StaticResource Key}` or
    // `{DynamicResource Key}`) against `element` and applies the resolved value
    // to element.get prop via Set(PropertyId(prop), value). Returns the number
    // of resources applied this call.
    //
    // Static: resolves once now. A MISSING static key throws
    // ResourceLookupException(MissingKey) (fail loud, Class A — the shared
    // decision-3 error); a Plain entry is applied as its Value; an Object entry
    // is NOT a scalar property value — the caller must extract the useful value
    // (a Brush/Style applies differently), so an Object static applied onto a
    // DP is rejected (TargetNotApplied counting the row but not setting a
    // scalar — recorded; only Plain values, and Object values that DERIVE the
    // target's bound scalar type, apply on the DP set path; the brush/style set
    // is S364/S400).
    //
    // Dynamic: applies the first value now (if resolvable) + records a
    // DynamicResourceLink so reapply_dynamic_resources can re-push. A
    // transiently-missing dynamic key does NOT throw — it holds.
    //
    // Not a resource reference -> returns 0 (nothing to do).
    [[nodiscard]] std::size_t apply_markup(UIElement& element, std::string_view prop,
                                           std::string_view source,
                                           std::vector<DynamicResourceLink>& dynamicLinks);

    // RE-EVALUATES every recorded dynamic link: re-runs find_for_element and
    // re-pushes. Never throws on a transiently-missing key (holds). Returns the
    // number of links re-applied (a missing link re-applies 0 but stays
    // recorded). The app/viewer calls this after a source dictionary key change
    // (S361 re-evaluation seam).
    [[nodiscard]] std::size_t reapply_dynamic_resources(
        const std::vector<DynamicResourceLink>& dynamicLinks);
};

} // namespace vaml
