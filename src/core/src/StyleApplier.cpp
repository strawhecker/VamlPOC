// S363 (dep:S362) — StyleApplier definitions. See StyleApplier.h.

#include "vaml/core/StyleApplier.h"

#include "vaml/core/DependencyObject.h"     // Get/Set
#include "vaml/core/Element.h"              // StackPanel/Grid/... (type names)
#include "vaml/core/PropertyId.h"           // PropertyId
#include "vaml/core/ResourceDictionary.h"   // find_for_element
#include "vaml/core/ResourceMarkup.h"       // parse_resource_markup
#include "vaml/core/ResourceResolver.h"     // apply_markup (dynamic setter re-push)
#include "vaml/core/UIElement.h"

#include <string>
#include <typeinfo>
#include <unordered_set>
#include <utility>

namespace vaml {

namespace {

// An element's TYPE NAME for implicit-style resolution. Because the built-in
// element classes expose no virtual type_name(), the applier maps the RTTI
// concrete class to the canonical element name (matching Element.h + the
// ElementFactoryRegistry names). Any other concrete class has no implicit-style
// key (returns empty -> no implicit style). Used ONLY by find_implicit_style.
[[nodiscard]] std::string type_name_of(const UIElement& element)
{
    const std::type_info& ti = typeid(element);
    if (ti == typeid(UIElement)) return "UIElement";
    if (ti == typeid(StackPanel)) return "StackPanel";
    if (ti == typeid(Grid)) return "Grid";
    if (ti == typeid(WrapPanel)) return "WrapPanel";
    if (ti == typeid(Canvas)) return "Canvas";
    return ""; // unknown concrete type -> no implicit style key
}

// LOCAL-WINS snapshot. A style pass must NOT clobber an AUTHORED local the
// element held BEFORE the pass began. Values written by the pass itself (base
// setter, then derived override, then a trigger) are all STYLE-components and
// follow style precedence — they must NOT be treated as pre-existing locals.
// `LocalsSnapshot` captures, at pass entry, which target properties already had
// a local override, so apply_value_to can skip exactly those.
class LocalsSnapshot
{
public:
    // Collects the pre-pass local-ness of every distinct setter target in
    // `chain` (base..derived) and the trigger setters.
    void capture(UIElement& element, const Style& style)
    {
        for (const Style* s = &style; s != nullptr; s = s->based_on())
        {
            capture_setters(element, s->setters());
            for (const Trigger& t : s->triggers())
            {
                capture_setters(element, (t.kind == TriggerKind::Property)
                                             ? t.property.setters
                                             : t.data.setters);
            }
        }
        capture_setters(element, style.setters());
        for (const Trigger& t : style.triggers())
        {
            capture_setters(element, (t.kind == TriggerKind::Property)
                                         ? t.property.setters
                                         : t.data.setters);
        }
    }

    // True when `prop` had a pre-pass authored local (-> the style must skip it).
    [[nodiscard]] bool had_local(std::string_view prop) const
    {
        return hadLocal_.find(std::string(prop)) != hadLocal_.end();
    }

private:
    void capture_setters(UIElement& element, const std::vector<Setter>& setters)
    {
        for (const Setter& s : setters)
        {
            if (element.has_local_value(s.targetProperty))
            {
                hadLocal_.insert(std::string(s.targetProperty.name()));
            }
        }
    }
    std::unordered_set<std::string> hadLocal_;
};

// Applies a resolved Value to `element`'s `prop`, honoring LOCAL-WINS via the
// pre-pass locals snapshot (see LocalsSnapshot). Returns (applied, value).
[[nodiscard]] StyleApply apply_value_to(UIElement& element, std::string_view prop,
                                        const Value& value, const LocalsSnapshot& locals)
{
    StyleApply out;
    out.property = PropertyId::of(prop);
    out.value = value;
    const PropertyId id = PropertyId::of(prop);
    // LOCAL-WINS: an authored local value from BEFORE the pass wins over the style.
    if (locals.had_local(id.name()))
    {
        out.applied = false; // the style setter must NOT clobber a pre-pass local
        out.value = element.Get(id);
        return out;
    }
    try
    {
        const SetResult r = element.Set(id, value);
        out.applied = (r == SetResult::APPLIED);
        (void)r;
    }
    catch (...)
    {
        out.applied = false; // wrong tag / unregistered target — report, never throw
    }
    return out;
}

} // namespace

VAML_CORE_EXPORT
std::vector<StyleApply>
StyleApplier::apply_style(UIElement& element, const Style& style,
                          std::vector<DynamicResourceLink>& dynamicLinks)
{
    std::vector<StyleApply> out;

    if (!style_valid(style))
    {
        return out; // an invalid style applies nothing (style_valid gate)
    }

    // LOCAL-WINS snapshot: capture which setter targets had an authored local
    // BEFORE this pass (so the pass's own writes are NOT mistaken for locals).
    LocalsSnapshot locals;
    locals.capture(element, style);

    // 1) BASE-FIRST chain: walk the BasedOn chain to the ROOT base first, then
    //    apply each style's OWN setters. We collect the chain (base..derived)
    //    then apply in base->derived order so the derived style overrides.
    std::vector<const Style*> chain;
    for (const Style* s = &style; s != nullptr; s = s->based_on())
    {
        chain.push_back(s);
    }
    // chain is [derived..base]; reverse to apply base-first.
    for (auto it = chain.rbegin(); it != chain.rend(); ++it)
    {
        for (const Setter& setter : (*it)->setters())
        {
            // Only Literal values apply through the direct DP path here; the
            // resource kinds (Static/Dynamic) resolve through the resolver;
            // TemplateBinding is S365's.
            switch (setter.value.kind())
            {
                case SetterValue::Kind::Literal:
                {
                    StyleApply a = apply_value_to(element, setter.targetProperty.name(),
                                                  setter.value.literal(), locals);
                    if (a.applied)
                    {
                        out.push_back(std::move(a));
                    }
                    break;
                }
                case SetterValue::Kind::StaticResource:
                case SetterValue::Kind::DynamicResource:
                {
                    // Resolve through the S360/S361 three-scope lookup via the
                    // ResourceResolver's apply_markup (which handles Static once +
                    // Dynamic link recording + re-push).
                    std::string source;
                    if (setter.value.kind() == SetterValue::Kind::StaticResource)
                        source = "{StaticResource " +
                                 std::string(setter.value.reference()) + "}";
                    else
                        source = "{DynamicResource " +
                                 std::string(setter.value.reference()) + "}";
                    // Route through the resolver so the same Static-loud /
                    // Dynamic-live semantics as the markup path hold.
                    ResourceResolver resolver;
                    const std::size_t n = resolver.apply_markup(
                        element, setter.targetProperty.name(), source, dynamicLinks);
                    if (n > 0)
                    {
                        out.push_back(StyleApply{ setter.targetProperty,
                                                  element.Get(setter.targetProperty),
                                                  true });
                    }
                    break;
                }
                case SetterValue::Kind::TemplateBinding:
                {
                    // S365 owns TemplateBinding resolution (read the templated
                    // parent's DP). Not applied here — the caller (a templated
                    // control) wires it. Recorded NO-GAP, not silent.
                    break;
                }
            }
        }
    }

    // 2) TRIGGERS: evaluate each trigger against the element's CURRENT state and
    //    apply the setters of each currently-FIRING one. A PropertyTrigger fires
    //    when element.Get(property) == value; a DataTrigger (S240 path) fires
    //    when the resolved data value == value (resolved through the element's
    //    effective state; S243 reader integration is a recorded forward). The
    //    self-change re-fire/un-fire subscription (S041 add_property_changed) is
    //    the S363 described LIVE behavior — delivered here as the apply-time
    //    evaluation; the element-DP-change re-arm is exercised by the tests
    //    re-running apply_style after a state change (the applier is stateless).
    for (const Trigger& trig : style.triggers())
    {
        bool fires = false;
        switch (trig.kind)
        {
            case TriggerKind::Property:
                try
                {
                    fires = (element.Get(trig.property.property) == trig.property.value);
                }
                catch (...)
                {
                    fires = false; // unregistered trigger property — never fires
                }
                break;
            case TriggerKind::Data:
            {
                // DataTrigger: resolve the S240 path against the element itself
                // (the default dependency_object_reader reads registered DPs). A
                // match on the resolved Value fires.
                const Value root = Value::UntypedPtr(&element);
                const PropertyRead pr = evaluate_property_path(root, trig.data.path,
                                                              dependency_object_reader());
                fires = (pr.ok && pr.value == trig.data.value);
                break;
            }
            default:
                fires = false;
                break;
        }
        if (!fires)
        {
            continue;
        }
        const std::vector<Setter>* setters =
            (trig.kind == TriggerKind::Property) ? &trig.property.setters : &trig.data.setters;
        for (const Setter& setter : *setters)
        {
            if (setter.value.kind() == SetterValue::Kind::Literal)
            {
                StyleApply a = apply_value_to(element, setter.targetProperty.name(),
                                              setter.value.literal(), locals);
                if (a.applied)
                {
                    out.push_back(std::move(a));
                }
            }
            // resource/TemplateBinding trigger setters resolve at later layers
        }
    }

    return out;
}

VAML_CORE_EXPORT
std::vector<StyleApply>
StyleApplier::apply_style_base(UIElement& element, const Style& style,
                               std::vector<DynamicResourceLink>& dynamicLinks)
{
    // Re-applies the base setter chain WITHOUT the trigger setters.
    std::vector<StyleApply> out;
    if (!style_valid(style))
    {
        return out;
    }
    LocalsSnapshot locals;
    locals.capture(element, style);
    std::vector<const Style*> chain;
    for (const Style* s = &style; s != nullptr; s = s->based_on())
    {
        chain.push_back(s);
    }
    for (auto it = chain.rbegin(); it != chain.rend(); ++it)
    {
        for (const Setter& setter : (*it)->setters())
        {
            if (setter.value.kind() != SetterValue::Kind::Literal)
            {
                continue;
            }
            StyleApply a = apply_value_to(element, setter.targetProperty.name(),
                                          setter.value.literal(), locals);
            if (a.applied)
            {
                out.push_back(std::move(a));
            }
        }
    }
    (void)dynamicLinks;
    return out;
}

VAML_CORE_EXPORT
const Style* find_implicit_style(const UIElement& element) noexcept
{
    const std::string typeName = type_name_of(element);
    if (typeName.empty())
    {
        return nullptr;
    }
    const std::string key = "Style:" + typeName;
    const ResourceValue* rv = find_for_element(element, key);
    if (rv == nullptr || rv->kind() != ResourceValue::Kind::Object)
    {
        return nullptr;
    }
    return dynamic_cast<const Style*>(rv->object());
}

VAML_CORE_EXPORT
std::size_t apply_implicit_style(UIElement& element,
                                 std::vector<DynamicResourceLink>& dynamicLinks)
{
    const Style* style = find_implicit_style(element);
    if (style == nullptr)
    {
        return 0;
    }
    StyleApplier applier;
    const auto applied = applier.apply_style(element, *style, dynamicLinks);
    return applied.size();
}

} // namespace vaml
