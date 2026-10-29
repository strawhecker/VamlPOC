// S092 (dep:S091) — VamlReader out-of-line definitions. Builds a LIVE
// dependency-object tree from a parsed (S085) tree. See VamlReader.h for the
// full design + ownership model. S092 ADDS: the x:Name registry (Strand A),
// deferred markup extensions / the S2 tag (Strand B), and the MINIMAL REAL
// event-hook surface (Strand C, resolved by S322).

#include "vaml/core/VamlReader.h"

#include "vaml/core/BindingAccess.h"           // S250_1: view_binding_diagnostics + view_binding_policy_default
#include "vaml/core/BindingDiagnostics.h"      // S322: emitting shared binding-diagnostics notes
#include "vaml/core/BindingMarkup.h"           // S252: parse_markup_binding
#include "vaml/core/Brush.h"                   // S460: GradientStopHost + LinearGradientBrush
#include "vaml/core/DependencyProperty.h"      // DependencyProperty
#include "vaml/core/Element.h"                 // register_builtin_elements
#include "vaml/core/ElementResolver.h"         // resolve_element + exception
#include "vaml/core/GridLength.h"              // 11j-9: GridLength (grid definition authoring)
#include "vaml/core/Intern.h"                  // S140/S144: interned string_view keys for the x:Name registry
#include "vaml/core/ParsedElement.h"           // ParsedElement/ParsedAttribute + parse_vaml
#include "vaml/core/PropertyId.h"              // PropertyId
#include "vaml/core/PropertyPath.h"            // S454: DataTrigger Binding path (S240)
#include "vaml/core/ResourceDictionary.h"      // S361: x:Key capture -> element Resources
#include "vaml/core/ResourceMarkup.h"          // S361: parse_resource_markup (Static/Dynamic)
#include "vaml/core/ResourceResolver.h"        // S361: apply_markup / dynamic reapply
#include "vaml/core/Style.h"                   // S454: authored <Style>/<Setter>/<Trigger>
#include "vaml/core/UIElement.h"               // S322: add_routed_handler target
#include "vaml/core/ValueConverter.h"          // ValueConverterRegistry + register_builtin_converters
#include "vaml/core/XmlScanner.h"              // XmlDirectiveType

#include <algorithm>
#include <charconv> // 11j-9: std::from_chars for Grid track lengths (clang needs it explicitly)
#include <cmath>    // 11j-9: std::isfinite (clang does not pull <cmath> transitively)
#include <string>
#include <utility>

namespace vaml {

namespace {

// S092: true iff `name` is already registered in the reader's x:Name registry.
// Linear scan over interned string_view keys (S144 — the small per-Load registry).
[[nodiscard]] bool name_registered(
    const std::vector<std::pair<std::string_view, DependencyObject*>>& names,
    std::string_view name)
{
    const auto it = std::find_if(
        names.begin(), names.end(),
        [&](const std::pair<std::string_view, DependencyObject*>& entry) {
            return entry.first == name;
        });
    return it != names.end();
}

// S463 — the EVENT TAIL of an event-attribute key. The RAISED-ATTACHED form
// `Owner.Event` (e.g. `StackPanel.Button.Click="Handler"`) names the EVENT as
// the LAST dotted segment ("Click"; the WPF `Class.Owner.Event` raised-attached
// convention, where Owner = the element the event is authored on). A non-dotted
// key is its own tail. Used to route event declarations + BindEvents.
[[nodiscard]] std::string_view event_tail(std::string_view key) noexcept
{
    const std::size_t dot = key.rfind('.');
    return (dot == std::string_view::npos) ? key : key.substr(dot + 1);
}

// 11j-9 (dep:S131) — parses a Grid track length literal ("Auto" / "auto" /
// numeric / "N*" / "*") into a GridLength. A small pure parser reusing
// GridLength.h's model: Auto / numeric Pixel / Star. Anything else -> a
// structured VamlReaderException (MalformedValue-style, never silent).
[[nodiscard]] GridLength parse_grid_length(std::string_view s)
{
    auto asciiLower = [](char c) noexcept {
        return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
    };
    auto ieq = [&](std::string_view a, const char* b) noexcept {
        std::size_t i = 0;
        for (; i < a.size(); ++i)
        {
            if (b[i] == '\0' || asciiLower(a[i]) != asciiLower(b[i]))
            {
                return false;
            }
        }
        return b[i] == '\0';
    };

    if (s.empty())
    {
        throw ValueConversionException(
            ValueConversionError::MalformedValue, std::string("GridLength"),
            std::string(s), "malformed grid track length (empty)");
    }

    // Auto / auto / AUTO.
    if (ieq(s, "Auto"))
    {
        return GridLength::Auto();
    }

    // Star: "*" (weight 1) or "N*" (weight N).
    if (s.back() == '*')
    {
        if (s.size() == 1)
        {
            return GridLength::Star(1.0f);
        }
        const std::string_view num = s.substr(0, s.size() - 1);
        double w = 0.0;
        auto [ptr, ec] = std::from_chars(num.data(), num.data() + num.size(), w);
        if (ec != std::errc() || ptr != (num.data() + num.size()) || !std::isfinite(w) || w < 0.0)
        {
            throw ValueConversionException(
                ValueConversionError::MalformedValue, std::string("GridLength"),
                std::string(s), "malformed star grid track length '" + std::string(s) + "'");
        }
        return GridLength::Star(static_cast<float>(w));
    }

    // Numeric -> Pixel (an absolute length).
    double v = 0.0;
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), v);
    if (ec != std::errc() || ptr != (s.data() + s.size()) || !std::isfinite(v) || v < 0.0)
    {
        throw ValueConversionException(
            ValueConversionError::MalformedValue, std::string("GridLength"),
            std::string(s), "malformed grid track length '" + std::string(s) + "'");
    }
    return GridLength::Pixel(static_cast<float>(v));
}

} // namespace

VAML_CORE_EXPORT
VamlReaderException::VamlReaderException(VamlReaderError kind, std::string property,
                                         std::string what)
    : std::runtime_error(std::move(what))
    , kind_(kind)
    , property_(std::move(property))
{
}

VAML_CORE_EXPORT
VamlReader::VamlReader() = default;

VAML_CORE_EXPORT
VamlReader::~VamlReader() = default;

namespace {

// S091: the type-name a ValueType maps to for VALUE conversion. Only the tags
// the S089 built-in converters can produce are represented (Int32/Bool/Color/
// Thickness + S440's Double); String is handled by identity in convertLiteral
// and is NOT routed here. Returns an empty string for a value type with no
// converter (the caller throws UnknownType).
[[nodiscard]] std::string_view converterNameFor(ValueType t) noexcept
{
    switch (t)
    {
        case ValueType::Int32:     return "Int32";
        case ValueType::Bool:      return "Bool";
        case ValueType::Color:     return "Color";
        case ValueType::Thickness: return "Thickness";
        case ValueType::Double:    return "Double";   // S440: FontSize / Slider.* / Canvas.* route through the Double converter
        default:                   return "";
    }
}

// S091: idempotent startup. The reader depends on the GLOBAL element registry
// (S087) and the GLOBAL converter registry (S089) being populated. Registering
// twice is a no-op guard (mirrors the single-registration guard inside
// register_builtin_elements / register_builtin_converters), so calling this on
// every Load is cheap and safe. Documented single-threaded startup.
void ensure_startup_registrations()
{
    static bool done = false;
    if (!done)
    {
        register_builtin_elements();
        register_builtin_converters();
        done = true;
    }
}

// S144: counts the NON-ROOT nodes in a parsed tree (used to reserve retained_).
[[nodiscard]] std::size_t count_non_root_descendants(const ParsedElement& el)
{
    std::size_t n = el.children.size();
    for (const ParsedElement& c : el.children)
    {
        n += count_non_root_descendants(c);
    }
    return n;
}

} // namespace

VAML_CORE_EXPORT
Value VamlReader::convertLiteral(const DependencyProperty& dp,
                                 std::string_view literal)
{
    const ValueType bound = dp.metadata().boundType();

    // A String-typed property takes the literal verbatim (no converter).
    if (bound == ValueType::String)
    {
        return Value::String(std::string(literal)); // value PAST the parser holds owned data
    }

    const std::string_view typeName = converterNameFor(bound);
    if (typeName.empty())
    {
        // A property whose bound type has no registered converter cannot be set
        // from a plain literal — UnknownType, since no target converter exists.
        throw ValueConversionException(
            ValueConversionError::UnknownType, std::string(), std::string(literal),
            "no value converter registered for the property's bound type (UnknownType)");
    }

    return ValueConverterRegistry::global().convert(typeName, literal);
}

VAML_CORE_EXPORT
std::unique_ptr<DependencyObject> VamlReader::build(const ParsedElement& element,
                                                    std::size_t depth)
{
    // Step 1 — resolve into a live instance (global registry; unknown type
    // throws ElementResolutionException naming type + line:col — S088).
    std::unique_ptr<DependencyObject> obj = resolve_element(element);

    // 11j-2 (dep:S453) — RESOURCE-DICTIONARY as a LOADABLE ELEMENT (a standalone
    // `.xaml` root OR a nested legal position). A resolved `ResourceDictionary`
    // element is the ResourceDictionaryHolder; its authored children are the
    // RESOURCES (keyed, merged) captured via S453's build_resource_dictionary —
    // NEVER ordinary visual children. This reuses the existing capture verbatim
    // and attaches the owned dictionary to the holder.
    if (ResourceDictionaryHolder* rdh = dynamic_cast<ResourceDictionaryHolder*>(obj.get()))
    {
        std::unique_ptr<ResourceDictionary> dict =
            build_resource_dictionary(element, obj.get(), depth);
        rdh->set_dictionary(std::move(dict));
        return obj; // a resource store; nothing is retained as a visual child
    }

    // S454 — STYLE AUTHORING (the S362/S363 engine authored from .vaml). A
    // `<Style>` is a RESOURCE (a Dict capture / implicit-style), never a visual:
    // its TargetType/BasedOn/Setters/Triggers are consumed by build_style into
    // the live S362 data model, and the Style is returned as an OWNED dict
    // resource. This replaces the generic attribute/child handling for Style.
    if (Style* st = dynamic_cast<Style*>(obj.get()))
    {
        build_style(*st, element);
        return obj; // a resource; nothing is retamed as a visual child
    }

    // S460 (dep:S456) — BRUSH AUTHORING. A `<LinearGradientBrush>` is a RESOURCE
    // (captured by x:Key into the authored dict, registered under its "Brush:<id>"
    // convention), never a visual: its StartPoint/EndPoint + authored
    // `<GradientStop>` children are consumed into the brush's data model. A
    // `<GradientStop>` alone builds into a GradientStopHost (the authored stop is
    // folded into its owning brush by build_linear_gradient's property-element).
    if (LinearGradientBrush* lb = dynamic_cast<LinearGradientBrush*>(obj.get()))
    {
        build_linear_gradient(*lb, element);
        return obj; // a resource; nothing is retamed as a visual child
    }

    // Step 2 — attributes: set plain-literal properties, CONSUME the S092
    // strands (Name/Class directives, isExtension retention, event routing).
    for (const ParsedAttribute& attr : element.attributes)
    {
        // --- 11d — DESIGN-TIME attributes stripped, never resolved. --------
        // WPF-faithful minimal guard: an attribute whose PREFIX is the designer
        // abbreviation (`d`) is DESIGN-TIME "crap" (d:DesignHeight,
        // d:DataContext, ...) — skip it verbatim, before ANY directive/value/
        // extension/property handling, so it never reaches the name-keyed
        // resolver and never throws UnknownProperty. This is prefix-keyed (not
        // per-file, not via a propagated ignorable set), so it covers the
        // attributes on any depth with no propagation cost. `mc:Ignorable` is
        // honored only as belt-and-suspenders: the `mc:Ignorable` declaration
        // itself is consumed here too (it is a markup-compatibility directive,
        // not a property), so a file that declares it still loads. The general
        // abbreviation/URI namespace model is the 11h/11i syntax work — NOT
        // this sprint (roadmap token added there).
        if (attr.prefix == "d" || attr.key == "mc:Ignorable" || attr.key == "Ignorable")
        {
            continue;
        }

        // --- S092 Strand A — x:Name / x:Class directives (now CONSUMED). ---
        // S083 classified these; S091 skipped them; S092 records them. The
        // x:Null -> S084 path remains deferred unchanged.
        if (attr.directive == XmlDirectiveType::Name)
        {
            // x:Name registry keys are INTERNED string_views (S144 — S140
            // discipline: pool-backed, stable for this thread, content-equal),
            // NOT owned std::string per entry (no owned-name gotcha). The
            // duplicate scan + named() both compare BY CONTENT.
            const std::string_view name = vaml::intern(attr.value);
            if (name_registered(names_, name))
            {
                // WPF-faithful: a name scope with a repeated name is an error;
                // fail loud, never silently last-wins. (DuplicateName — the one
                // genuinely-new reader error S092 introduces.)
                throw VamlReaderException(
                    VamlReaderError::DuplicateName, std::string(name),
                    "duplicate x:Name '" + std::string(name) + "'");
            }
            names_.emplace_back(name, obj.get());
            continue;
        }
        if (attr.directive == XmlDirectiveType::Class)
        {
            // Record the AUTHORED root class name only; binding it to an owner
            // object/type is Sprint 8 / S286 / InitializeComponent-analogue
            // (recorded NO-GAP, not silent). S094 (the error-taxonomy gate
            // this hand-off was recorded for) resolves the deferred decision:
            // x:Class is root-only by definition (WPF-faithful), so a Class
            // directive authored on a NON-root element is an authoring error —
            // fail loud, never silently overwrite root_class_ (last-wins hole).
            if (depth != 0)
            {
                throw VamlReaderException(
                    VamlReaderError::ClassOnNonRoot, std::string(attr.value),
                    "x:Class on a non-root element '" + std::string(attr.value) + "'");
            }
            root_class_ = std::string(attr.value);
            continue;
        }
        if (attr.directive == XmlDirectiveType::Key)
        {
            // x:Key stays DEFERRED at the reader (S092 -> S361 deferral). The
            // S361 StaticResource/DynamicResource resolution (BindResources /
            // ReapplyDynamicResources) is delivered at S361; the x:Key-to-
            // dictionary AUTHORING capture requires the `<X.Resources>` /
            // `<ResourceDictionary>` property-element syntax this parser does
            // NOT yet accept — so a bare x:Key on a non-resource child remains
            // a visual/tree element here (WPF-correct for this reader surface;
            // the GoldenDocs contract asserts exactly this). Recorded NO-GAP:
            // the x:Key dictionary-capture is the S360/S366-style resource-
            // authoring surface, NOT silently backfilled. (See writeup.)
            continue;
        }
        if (attr.directive != XmlDirectiveType::None)
        {
            // Remaining directives (x:Null -> S084): still deferred, recorded
            // NO-GAP, NOT silent.
            continue;
        }

        // --- S462 (dep:S461) — FrameworkElement `Name` attribute. ----------
        // A plain (non-directive) `Name="submitButton"` attribute registers the
        // built object in the S092 name registry ON PAR with `x:Name` (the
        // FrameworkElement.Name analogue is not a DP — it is the name-scope
        // identity, so the same interned-registry path is used). It does NOT set
        // a DP (no registered "Name" global DP exists; a control that later owns
        // a Name DP is a recorded-NO-GAP future surface). A duplicate Name, like
        // a duplicate x:Name, is the DuplicateName authoring error (never last-wins).
        if (attr.key == "Name")
        {
            const std::string_view name = vaml::intern(attr.value);
            if (name_registered(names_, name))
            {
                throw VamlReaderException(
                    VamlReaderError::DuplicateName, std::string(name),
                    "duplicate Name '" + std::string(name) + "'");
            }
            names_.emplace_back(name, obj.get());
            continue;
        }

        // --- S092 Strand C — event routing (MINIMAL REAL seed). ------------
        // An attribute key whose EVENT TAIL exactly matches a registered event
        // name is an EVENT declaration, not a property: route it to a
        // DeferredEventHandler. S463 — the RAISED-ATTACHED form `Owner.Event`
        // (`StackPanel.Button.Click`) is recognized here: the key's last dotted
        // segment ("Click") is the event, so a dotted event key must NOT fall
        // through to the attached-aware property path. Never a property set,
        // never an unknown-property throw. Resolved by S322.
        const std::string_view tail = event_tail(attr.key);
        if (std::find(event_names_.begin(), event_names_.end(), tail) !=
            event_names_.end())
        {
            deferred_event_handlers_.push_back(
                DeferredEventHandler{obj.get(), attr.key, attr.value});
            continue;
        }

        // --- S092 Strand B — deferred markup extensions (the S2 tag). -------
        // An S084-classified isExtension attribute ({Binding Title}) is NOT
        // skipped silently anymore: record it as a DeferredExtension with the
        // ORIGINAL source text retained verbatim (S2 tag). The reader does NOT
        // resolve the extension — S252/S253 (the Binding sprint) consumes these
        // records ("S2 tag: resolved by S252"). Never converted/set here.
        if (attr.isExtension)
        {
            deferred_extensions_.push_back(
                DeferredExtension{obj.get(), attr.key, attr.value});
            continue;
        }

        // --- Ordinary plain-literal property (S091, unchanged). -------------
        // Resolve the property definition globally. An attribute naming a
        // property with no global DependencyProperty -> VamlReaderException
        // (S091's unknown-property baseline; S094 owns the full taxonomy).
        // 11d-DEBUG: strict = the throw (unchanged); collect = record + keep
        // scanning the document so ALL unknown properties surface in one pass.
        const DependencyProperty* dp = DependencyProperty::TryGet(attr.key);
        if (dp == nullptr)
        {
            report_unknown_property(
                std::string(attr.key),
                "unknown property '" + std::string(attr.key) + "'");
            continue;   // collect mode: skip the (never-valid) set, keep scanning
        }

        const Value value = convertLiteral(*dp, attr.value);
        (void)obj->Set(PropertyId(attr.key), value); // S043 attached-aware by name
    }

    // 11k-1 — TEXTBLOCK INLINE CONTENT ROUTING (the corpus's dominant usage).
    // WPF IAddChild: `<TextBlock>label</TextBlock>` inline authored content lands
    // on the Text DP (TextBlock's content-owning DP). This is SCOPED to
    // TextBlock (11k-1's deliverable); the GENERAL content→DP routing rule
    // (mixed/interleaved ordering) is 11k-11's in-11k step (NOT deferred here).
    // Whitespace trim mirrors the S452 property-element literal path (line 749).
    if (auto* tb = dynamic_cast<TextBlock*>(obj.get()))
    {
        std::string lit = element.content;
        const std::size_t b = lit.find_first_not_of(" \t\r\n");
        if (b != std::string::npos)
        {
            const std::size_t e = lit.find_last_not_of(" \t\r\n");
            lit = lit.substr(b, e - b + 1);
        }
        else
        {
            lit.clear();
        }
        if (!lit.empty())
        {
            const DependencyProperty* dp = DependencyProperty::TryGet("Text");
            if (dp != nullptr)
            {
                const Value value = convertLiteral(*dp, lit);
                (void)tb->Set(PropertyId::of("Text"), value);
            }
        }
    }

    // Step 3 — property-elements (S452, the S366 NO-GAP hand-off RESOLVED) then
    // ordinary children (visual tree transfer, S440).
    //
    // S452 — a PROPERTY-ELEMENT is a child whose tag is `<X.Owner.Property>`
    // (dotted; the '.' is NOT a namespace separator per S082). It does NOT
    // produce a visual child; instead it SETS the named Property onto the
    // PARENT (this built instance) via the S043 attached-aware DP path:
    //   - an OBJECT child element (`<Button.Content><StackPanel/></Button.Content>`)
    //     → the owned built child is set as the parent's DP VALUE as
    //     `Value::UntypedPtr` (the object is reader-retained; x:Name/child
    //     directives still apply to it). This is the "populated via the DP
    //     path" headline: a property-element's object value lands on the named
    //     DP, not in the S121 visual children (a post-S440 ContentControl
    //     materialization is a later, recorded step — see the S452 scope note).
    //   - a literal/extension (`<Button.Background>Red</Button.Background>`,
    //     or `{StaticResource X}`) → converted/resolved through the same DP
    //     path (a converted literal or a deferred extension).
    // The property-element itself is KEPT OUT of the ordinary children
    // recursion (it is not a visual child). The owner name in `<Owner.Property>`
    // matches the PARENT's local type name — it is a self-referential
    // "this-element's Property" notation (WPF-faithful).
    //
    // S452 SCOPE BOUND (recorded NO-GAP, not silent): the property-element
    // SETS the named DP. Whether an object-valued DP (e.g. a Content DP) ALSO
    // enters the S121 visual children is ContentControl/control-material work
    // (11c/11x); a property-element is NOT auto-attached as a render child at
    // S452. This keeps S452 the clean seam S453/S454 ride (a Resources dict = an
    // owned object value; a Style = an owned object value) without pre-empting
    // ContentControl semantics.
    for (const ParsedElement& child : element.children)
    {
        const std::string_view childLocal = child.local_name.empty() ? child.tagName
                                                                     : child.local_name;

        // A bare `<ResourceDictionary>` as a normal child is an authoring error
        // (it has no visual meaning); it may only appear under `.Resources` /
        // `.MergedDictionaries`. HANDLED by build_resource_dictionary / the
        // property-element branch where legal.
        if (childLocal == "ResourceDictionary")
        {
            throw VamlReaderException(
                VamlReaderError::UnknownProperty, std::string("ResourceDictionary"),
                "a <ResourceDictionary> may only appear inside <Owner.Resources> or "
                "<ResourceDictionary.MergedDictionaries>");
        }

        if (is_property_element(childLocal))
        {
            const auto [ownerName, propName] = split_property_element(childLocal);

            // --- S453 (dep:S452) — RESOURCE-DICTIONARY AUTHORING (the S366
            // NO-GAP hand-off RESOLVED). A `<X.Resources>` property-element
            // builds an OWNED ResourceDictionary and routes it into the S360
            // side-table (`set_element_resources`) — the resources dict IS the
            // S360 three-scope store, NOT a plain DP. Accepted-decision 3:
            // `<Owner.Resources>` routes into the side-table, never a DP Set.
            if (propName == "Resources")
            {
                UIElement* ownerUI = dynamic_cast<UIElement*>(obj.get());
                if (ownerUI == nullptr)
                {
                    throw VamlReaderException(
                        VamlReaderError::UnknownProperty, std::string("Resources"),
                        "resources can only be authored on a UIElement (property-element 'Resources')");
                }
                // The `.Resources` may hold a single bare `<ResourceDictionary>`
                // (its children authored inside that dictionary element) or an
                // inline dictionary (children authored directly under `.Resources`).
                std::unique_ptr<ResourceDictionary> ownedDict;
                if (child.children.size() == 1 &&
                    (child.children[0].local_name.empty() ? child.children[0].tagName
                                                          : child.children[0].local_name) ==
                        "ResourceDictionary")
                {
                    ownedDict = build_resource_dictionary(child.children[0], obj.get(), depth);
                }
                else
                {
                    ownedDict = build_resource_dictionary(child, obj.get(), depth);
                }
                detail::set_element_resources(ownerUI, std::move(ownedDict));
                continue; // NOT a visual child
            }

            // --- 11j-7 (dep:S454) — ELEMENT-LEVEL `<Owner.Triggers>`. -------
            // The corpus authors `<Window.Triggers>` / `<UserControl.Triggers>`
            // as element-level trigger sets (S454 authored ONLY `<Style.Triggers>`).
            // The `<Owner.Triggers>` children (PropertyTrigger / DataTrigger with
            // their child <Setter>s) are parsed into the SAME live S362 trigger
            // model a Style carries and attached to the element (the owned per-
            // element trigger set; triggers are behavior, not a DP — mirroring
            // Style's storage). Authoring is fully real + parseable; element-level
            // EVALUATION (re-evaluate on DP match) is the recorded continuation to
            // the control-material/content step (S454 styles already evaluate).
            if (propName == "Triggers")
            {
                DependencyObject* target = obj.get();
                std::vector<Trigger> trigs;
                for (const ParsedElement& trig : child.children)
                {
                    const std::string_view tl = trig.local_name.empty() ? trig.tagName
                                                                        : trig.local_name;
                    if (tl == "PropertyTrigger")
                    {
                        std::string prop;
                        std::string value;
                        for (const ParsedAttribute& a : trig.attributes)
                        {
                            if (a.directive == XmlDirectiveType::None && a.key == "Property")
                            {
                                prop = a.value;
                            }
                            else if (a.directive == XmlDirectiveType::None && a.key == "Value")
                            {
                                value = a.value;
                            }
                        }
                        const DependencyProperty* tdp = DependencyProperty::TryGet(prop);
                        if (tdp == nullptr)
                        {
                            throw VamlReaderException(
                                VamlReaderError::UnknownProperty, prop,
                                "unknown PropertyTrigger target property '" + prop + "'");
                        }
                        PropertyTrigger pt;
                        pt.property = PropertyId(vaml::intern(prop));
                        pt.value = convertLiteral(*tdp, value);
                        for (const ParsedElement& ts : trig.children)
                        {
                            if ((ts.local_name.empty() ? ts.tagName : ts.local_name) == "Setter")
                            {
                                Setter s;
                                parse_setter(ts, "PropertyTrigger", s);
                                pt.setters.push_back(std::move(s));
                            }
                        }
                        trigs.push_back(Trigger::MakeProperty(std::move(pt)));
                    }
                    else if (tl == "DataTrigger")
                    {
                        std::string binding;
                        std::string value;
                        for (const ParsedAttribute& a : trig.attributes)
                        {
                            if (a.directive == XmlDirectiveType::None && a.key == "Binding")
                            {
                                binding = a.value;
                            }
                            else if (a.directive == XmlDirectiveType::None && a.key == "Value")
                            {
                                value = a.value;
                            }
                        }
                        if (binding.empty())
                        {
                            throw VamlReaderException(
                                VamlReaderError::UnknownProperty, "Binding",
                                "a <DataTrigger> must name a 'Binding' path");
                        }
                        DataTrigger dt;
                        dt.path = PropertyPath::parse(binding);
                        dt.value = Value::String(value);
                        for (const ParsedElement& ts : trig.children)
                        {
                            if ((ts.local_name.empty() ? ts.tagName : ts.local_name) == "Setter")
                            {
                                Setter s;
                                parse_setter(ts, "DataTrigger", s);
                                dt.setters.push_back(std::move(s));
                            }
                        }
                        trigs.push_back(Trigger::MakeData(std::move(dt)));
                    }
                    else
                    {
                        throw VamlReaderException(
                            VamlReaderError::UnknownProperty, std::string(tl),
                            "unknown element-level trigger child '" + std::string(tl) + "'");
                    }
                }
                elementTriggers_.emplace_back(target, std::move(trigs));
                continue; // NOT a visual child
            }

            // --- 11j-9 (dep:S131) — `<Grid.RowDefinitions>` / `<Grid.ColumnDefinitions>`
            // markup authoring. S131's Grid track vectors exist in code
            // (set_row_definitions/set_column_definitions); the MARKUP has no DP
            // (RowDefinitions is real member state, not a DP — a bogus object DP
            // would shadow the track container). This arm wires the property-element
            // surface onto the code-level API: the authored `<RowDefinition Width=..>`
            // / `<ColumnDefinition Width=..>` children's Width/Height attributes are
            // parsed into GridLengths (Auto / numeric Pixel / "N*" Star) + handed to
            // grid->set_row_definitions / set_column_definitions.
            if (propName == "RowDefinitions" || propName == "ColumnDefinitions")
            {
                Grid* grid = dynamic_cast<Grid*>(obj.get());
                if (grid == nullptr)
                {
                    throw VamlReaderException(
                        VamlReaderError::UnknownProperty, std::string(propName),
                        "RowDefinitions/ColumnDefinitions can only be authored on a <Grid>");
                }
                const bool isColumns = (propName == "ColumnDefinitions");
                std::vector<ColumnDefinition> cols;
                std::vector<RowDefinition> rows;
                for (const ParsedElement& def : child.children)
                {
                    const std::string_view dl = def.local_name.empty() ? def.tagName
                                                                       : def.local_name;
                    const bool isColumnDef = (dl == "ColumnDefinition");
                    const bool isRowDef = (dl == "RowDefinition");
                    if (isColumns ? !isColumnDef : !isRowDef)
                    {
                        throw VamlReaderException(
                            VamlReaderError::UnknownProperty, std::string(dl),
                            std::string(isColumns ? "ColumnDefinitions" : "RowDefinitions") +
                                " may only contain <" +
                                std::string(isColumns ? "ColumnDefinition" : "RowDefinition") +
                                "> children (got '" + std::string(dl) + "')");
                    }
                    // Read the Width (column) / Height (row) attribute into a
                    // GridLength: "Auto"/"auto" -> Auto; a numeric -> Pixel;
                    // "N*" / "*" -> Star; malformed -> structured error.
                    const std::string_view lenAttr = isColumns ? "Width" : "Height";
                    std::string_view len;
                    for (const ParsedAttribute& a : def.attributes)
                    {
                        if (a.directive == XmlDirectiveType::None && a.key == lenAttr)
                        {
                            len = a.value;
                            break;
                        }
                    }
                    if (len.empty())
                    {
                        // No explicit length -> an Auto track (GridLength default).
                        len = "Auto";
                    }
                    GridLength gl = parse_grid_length(len);
                    if (isColumns)
                    {
                        ColumnDefinition cd;
                        cd.width = gl;
                        cols.push_back(cd);
                    }
                    else
                    {
                        RowDefinition rd;
                        rd.height = gl;
                        rows.push_back(rd);
                    }
                }
                if (isColumns)
                {
                    grid->set_column_definitions(std::move(cols));
                }
                else
                {
                    grid->set_row_definitions(std::move(rows));
                }
                continue; // NOT a visual child
            }

            // The OWNER name in a property-element must match the parent's type
            // name (it is "this.Element's Property"). We don't enforce a match
            // here beyond the dotted form: the property name is set on THIS
            // parent regardless (the S043 DP path resolves `propName` against
            // the parent). The owner portion is validated for a well-formed
            // dotted name by is_property_element.
            (void)ownerName;

            // Resolve the named property's global definition. An unknown dotted
            // property -> the S091 UnknownProperty reader error naming the key.
            const DependencyProperty* dp = DependencyProperty::TryGet(std::string(propName));
            if (dp == nullptr)
            {
                throw VamlReaderException(
                    VamlReaderError::UnknownProperty, std::string(propName),
                    "unknown property '" + std::string(propName) + "' (property-element)");
            }

            // Content: EXACTLY ONE value. That value is either
            //   (a) an OBJECT child element — built (recursively), retained,
            //       and set as the parent's DP via Value::UntypedPtr; or
            //   (b) inline text/extension/literal — converted via convertLiteral
            //       (or deferred as an extension), set through the DP path.
            if (child.children.size() == 1)
            {
                // (a) object value: build the child, retain it, set as UntypedPtr.
                std::unique_ptr<DependencyObject> owned = build(child.children[0], depth + 1);
                DependencyObject* raw = owned.get();
                raw->set_parent(obj.get()); // S039 parent-link (the object is reader-retained)
                retained_.push_back(std::move(owned));
                // 11j-8 (dep:S036): the object-typed WPF `Tag` is mapped to a
                // String-typed DP here (option A — the scalar literal path is the
                // corpus's PRIMARY use, and VAML's typed Value cannot hold an
                // arbitrary object in a single bound-typed slot). An OBJECT
                // `<X.Tag><SomeObject/></X.Tag>` is therefore built + RETAINED
                // (+ parent-linked) so it is represented in the load result, but
                // NOT set as the scalar DP value (an UntypedPtr would mismatch
                // the String-typed Tag and throw). WPF-faithful: the object slot
                // accepts a scalar (string) OR an object; VAML represents the
                // scalar on the DP and the object as reader-retained state.
                if (propName != "Tag")
                {
                    (void)obj->Set(PropertyId(propName), Value::UntypedPtr(raw));
                }
            }
            else if (!child.content.empty())
            {
                // (b) inline literal (after trimming whitespace): convert to the
                // DP's bound type and set.
                std::string lit = child.content;
                // Trim leading/trailing whitespace (multi-line authored literals).
                const std::size_t b = lit.find_first_not_of(" \t\r\n");
                if (b != std::string::npos)
                {
                    const std::size_t e = lit.find_last_not_of(" \t\r\n");
                    lit = lit.substr(b, e - b + 1);
                }
                else
                {
                    lit.clear();
                }
                if (!lit.empty())
                {
                    const Value value = convertLiteral(*dp, lit);
                    (void)obj->Set(PropertyId(propName), value);
                }
                // An extension authored as element content (e.g. a multiline
                // {StaticResource}) is recorded as a deferred extension on the
                // PARENT (the property-element's owner) exactly like an attribute
                // extension — so S361 BindResources resolves it.
                const std::string_view contentView = child.content;
                if (!contentView.empty() && contentView.front() == '{' && contentView.back() == '}')
                {
                    deferred_extensions_.push_back(
                        DeferredExtension{obj.get(), propName, vaml::intern(contentView)});
                }
            }
            // An empty property-element `<Owner.Property/>` is the degenerate
            // zero case (skip — nothing to set).
            continue; // NOT a visual child — keep it out of the recursion below.
        }

        // --- Ordinary visual child (S440 transfer into the S121 tree). ------
        std::unique_ptr<DependencyObject> childObj = build(child, depth + 1);
        childObj->set_parent(obj.get()); // S039/S041 parent-link + inheritance-scope seed
        retained_.push_back(std::move(childObj));
    }

    // Step 4 — the caller owns the root; children live in `retained_` until
    // the S440 finalize_visual_tree() hand-off (see that method).
    return obj;
}

VAML_CORE_EXPORT
std::unique_ptr<ResourceDictionary> VamlReader::build_resource_dictionary(
    const ParsedElement& resourceDict, DependencyObject* parent, std::size_t depth)
{
    // S453 — builds an OWNED ResourceDictionary from a parsed dictionary element.
    //
    // Each DIRECT authored child is a resource captured BY ITS x:Key (the S361
    // hand-off RESOLVED): the child is built (via build(), so a <Style> /
    // <SolidColorBrush> / nested whatever) and `set(key, owned object)` — an
    // S360 Object resource owned by the dictionary. The x:Key whose capture was
    // DEFERRED at S092 (`attr.directive == Key`) now becomes REAL for resource-
    // dictionary children: authored resources reach the S360 store.
    //
    // Children are the S360 merge semantics:
    //   - a plain resource child (x:Key) -> set(key, owned object);
    //   - a `<ResourceDictionary.MergedDictionaries>` property-element -> each
    //     nested `<ResourceDictionary>` child is built + `merge_with(child)`
    //     (self-wins + sibling-last-wins, the existing S360 semantics).
    //
    // A child with NO x:Key (and not a MergedDictionaries container) is an
    // authoring error — resources must be keyed — -> structured VamlReader
    // exception. A duplicate x:Key in one dictionary -> ResourceAlreadyRegistered
    // (the S360 dedupe-throws-unchanged contract).

    auto dict = std::make_unique<ResourceDictionary>();

    for (const ParsedElement& child : resourceDict.children)
    {
        const std::string_view childLocal = child.local_name.empty() ? child.tagName
                                                                     : child.local_name;

        // --- MergedDictionaries property-element -------------------------
        if (childLocal == "ResourceDictionary.MergedDictionaries" ||
            childLocal == "MergedDictionaries")
        {
            for (const ParsedElement& mergedChild : child.children)
            {
                const std::string_view mcLocal = mergedChild.local_name.empty()
                                                     ? mergedChild.tagName
                                                     : mergedChild.local_name;
                if (mcLocal != "ResourceDictionary")
                {
                    throw VamlReaderException(
                        VamlReaderError::UnknownProperty, std::string(mcLocal),
                        "MergedDictionaries contains a non-<ResourceDictionary> child");
                }
                // Build the merged child dictionary + merge (self-wins /
                // sibling-last-wins — the existing S360 merge semantics).
                auto mergedDict = build_resource_dictionary(mergedChild, parent, depth);
                dict->merge_with(*mergedDict);
                // Keep the merged dictionary alive: it must outlive the parent.
                // The S360 merge is a BORROWED child (non-owning). We retain the
                // owned merged dictionary here (reader-owned, per-Load cleared).
                owned_merged_.push_back(std::move(mergedDict));
            }
            continue;
        }

        // --- A plain authored resource: x:Key -> set(key, owned object). ----
        // Find the x:Key directive on the child.
        std::string_view key;
        bool haveKey = false;
        for (const ParsedAttribute& a : child.attributes)
        {
            if (a.directive == XmlDirectiveType::Key)
            {
                key = a.value;
                haveKey = true;
                break;
            }
        }
        std::unique_ptr<DependencyObject> resource = build(child, depth + 1);
        DependencyObject* raw = resource.get();
        if (parent != nullptr)
        {
            raw->set_parent(parent); // the owned resource is parent-linked to the owner
        }

        // S454 — IMPLICIT STYLE: a `<Style>` with NO x:Key is an implicit style
        // for its TargetType, registered under the S363 lookup key
        // "Style:{TargetType}" (what find_implicit_style / apply_implicit_style
        // resolve). An explicitly-keyed `<Style x:Key>` is captured under its
        // x:Key. Any OTHER resource without an x:Key is an authoring error.
        std::string captureKey;
        if (haveKey)
        {
            captureKey.assign(key);
        }
        else if (Style* st = dynamic_cast<Style*>(resource.get()))
        {
            if (st->target_type().empty())
            {
                throw VamlReaderException(
                    VamlReaderError::UnknownProperty, std::string(childLocal),
                    "an implicit <Style> (no x:Key) must declare a TargetType "
                    "(implicit-style key resolution 'Style:{TargetType}')");
            }
            captureKey = "Style:" + st->target_type();
        }
        else
        {
            throw VamlReaderException(
                VamlReaderError::UnknownProperty, std::string(childLocal),
                "an authored resource in a ResourceDictionary must carry an x:Key "
                "(resource '" + std::string(childLocal) + "' has none)");
        }

        dict->set(vaml::intern(captureKey), std::move(resource));
        (void)raw;
    }

    // S454 — record this built dictionary as a search target for Style BasedOn
    // resolution (find_style_by_key walks these to wire a derived Style's base).
    built_dicts_.push_back(dict.get());

    return dict;
}

VAML_CORE_EXPORT
void VamlReader::build_gradient_stop(GradientStopHost& stop, const ParsedElement& element)
{
    // Consumes the authored `<GradientStop Color= Offset=/>` attributes into the
    // stop's Color/Offset DPs (the S043 attached-aware Set path). GradientStop
    // has NO child elements (a stop is a scalar pair, not a container); a child
    // is an authoring error — fail loud, never silently ignored.
    for (const ParsedAttribute& attr : element.attributes)
    {
        if (attr.directive != XmlDirectiveType::None)
        {
            continue; // x:Key directives on a stop are ignored (a stop is owned)
        }
        if (attr.isExtension)
        {
            throw VamlReaderException(
                VamlReaderError::UnknownProperty, std::string(attr.key),
                "a <GradientStop> attribute cannot be a markup extension ('" +
                    std::string(attr.key) + "')");
        }
        const DependencyProperty* dp = DependencyProperty::TryGet(attr.key);
        if (dp == nullptr)
        {
            throw VamlReaderException(
                VamlReaderError::UnknownProperty, std::string(attr.key),
                "unknown <GradientStop> attribute '" + std::string(attr.key) + "'");
        }
        const Value value = convertLiteral(*dp, attr.value);
        (void)stop.Set(PropertyId(attr.key), value);
    }
    if (!element.children.empty())
    {
        throw VamlReaderException(
            VamlReaderError::UnknownProperty, std::string(element.local_name.empty() ? element.tagName
                                                                                      : element.local_name),
            "a <GradientStop> may not contain child elements");
    }
}

VAML_CORE_EXPORT
void VamlReader::build_linear_gradient(LinearGradientBrush& brush, const ParsedElement& element)
{
    // Consumes the authored `<LinearGradientBrush/>` content into its data model:
    //   StartPoint="x,y" / EndPoint="x,y"  (attribute values, "x,y" Double pairs)
    //   <LinearGradientBrush.GradientStops>  (a property-element whose authored
    //       <GradientStop Color= Offset=/> children are folded into the ordered
    //       stop vector — WPF's GradientStopCollection analogue).
    // The brush is a RESOURCE (never a visual child); its stops are pure data.

    // --- Attributes: StartPoint / EndPoint (Double "x,y" pairs). ----------
    for (const ParsedAttribute& attr : element.attributes)
    {
        if (attr.directive != XmlDirectiveType::None)
        {
            continue; // x:Key handled by the dict capture
        }
        if (attr.isExtension)
        {
            throw VamlReaderException(
                VamlReaderError::UnknownProperty, std::string(attr.key),
                "a <LinearGradientBrush> attribute cannot be a markup extension ('" +
                    std::string(attr.key) + "')");
        }
        if (attr.key == "StartPoint")
        {
            // "x,y" -> two Double literals (the S440 Double converter via the
            // registered GradientStop.Point DPs we reuse by name).
            const std::string_view v = attr.value;
            const std::size_t comma = v.find(',');
            if (comma == std::string_view::npos)
            {
                throw VamlReaderException(
                    VamlReaderError::UnknownProperty, std::string("StartPoint"),
                    "StartPoint must be a 'x,y' Double pair");
            }
            const Value x = convertLiteral(
                DependencyProperty::Get("Offset"), // a bound Double DP
                v.substr(0, comma));
            const Value y = convertLiteral(
                DependencyProperty::Get("Offset"),
                v.substr(comma + 1));
            brush.set_start_point(x.get<double>(), y.get<double>());
        }
        else if (attr.key == "EndPoint")
        {
            const std::string_view v = attr.value;
            const std::size_t comma = v.find(',');
            if (comma == std::string_view::npos)
            {
                throw VamlReaderException(
                    VamlReaderError::UnknownProperty, std::string("EndPoint"),
                    "EndPoint must be a 'x,y' Double pair");
            }
            const Value x = convertLiteral(
                DependencyProperty::Get("Offset"),
                v.substr(0, comma));
            const Value y = convertLiteral(
                DependencyProperty::Get("Offset"),
                v.substr(comma + 1));
            brush.set_end_point(x.get<double>(), y.get<double>());
        }
    }

    // --- Children: the <LinearGradientBrush.GradientStops> property-element. ---
    for (const ParsedElement& child : element.children)
    {
        const std::string_view childLocal = child.local_name.empty() ? child.tagName
                                                                     : child.local_name;
        if (childLocal != "LinearGradientBrush.GradientStops")
        {
            throw VamlReaderException(
                VamlReaderError::UnknownProperty, std::string(childLocal),
                "a <LinearGradientBrush> may only contain a "
                "<LinearGradientBrush.GradientStops> property-element (got '" +
                    std::string(childLocal) + "')");
        }
        // Each authored child must be a <GradientStop>; build + fold its
        // Color/Offset into the brush's ordered stop vector.
        for (const ParsedElement& stopEl : child.children)
        {
            const std::string_view stopLocal = stopEl.local_name.empty() ? stopEl.tagName
                                                                         : stopEl.local_name;
            if (stopLocal != "GradientStop")
            {
                throw VamlReaderException(
                    VamlReaderError::UnknownProperty, std::string(stopLocal),
                    "a <LinearGradientBrush.GradientStops> may only contain "
                    "<GradientStop> children (got '" + std::string(stopLocal) + "')");
            }
            GradientStopHost stopHost;
            build_gradient_stop(stopHost, stopEl);
            brush.add_stop(GradientStop{ stopHost.color(), stopHost.offset() });
        }
    }
}

VAML_CORE_EXPORT
void VamlReader::parse_setter(const ParsedElement& el, std::string owner, Setter& out)
{
    // Reads a `<Setter Property="P" Value="V"/>` (or the trigger-child variant)
    // into `out`.
    std::string prop;
    std::string value;
    for (const ParsedAttribute& a : el.attributes)
    {
        if (a.directive != XmlDirectiveType::None)
        {
            continue; // x:Key etc. are not setter fields
        }
        if (a.key == "Property")
        {
            prop = a.value;
        }
        else if (a.key == "Value")
        {
            value = a.value;
        }
    }
    if (prop.empty())
    {
        throw VamlReaderException(
            VamlReaderError::UnknownProperty, owner,
            "a <Setter> must name a 'Property' (in " + owner + ")");
    }

    const DependencyProperty* dp = DependencyProperty::TryGet(prop);
    if (dp == nullptr)
    {
        throw VamlReaderException(
            VamlReaderError::UnknownProperty, prop,
            "unknown setter target property '" + prop + "' (in " + owner + ")");
    }

    // A {StaticResource K} / {DynamicResource K} Value -> the reference SetterValue.
    const ResourceMarkup rm = parse_resource_markup(value);
    if (rm.recognized)
    {
        if (rm.kind == ResourceReferenceKind::Static)
        {
            out = Setter{PropertyId(vaml::intern(prop)), SetterValue::StaticResource(rm.key)};
            return;
        }
        if (rm.kind == ResourceReferenceKind::Dynamic)
        {
            out = Setter{PropertyId(vaml::intern(prop)), SetterValue::DynamicResource(rm.key)};
            return;
        }
    }
    // A {TemplateBinding T} Value -> the control-DP read SetterValue (S365).
    const std::string_view sv = value;
    const std::string_view tbMark = "{TemplateBinding ";
    if (sv.size() > tbMark.size() + 1 && sv.substr(0, tbMark.size()) == tbMark &&
        sv.back() == '}')
    {
        const std::string_view inner =
            sv.substr(tbMark.size(), sv.size() - tbMark.size() - 1);
        out = Setter{PropertyId(vaml::intern(prop)), SetterValue::TemplateBinding(inner)};
        return;
    }

    // A plain literal (converted to the property's bound type).
    out = Setter{PropertyId(vaml::intern(prop)), SetterValue::Literal(convertLiteral(*dp, value))};
}

VAML_CORE_EXPORT
void VamlReader::build_style(Style& style, const ParsedElement& element)
{
    // S454 — STYLE AUTHORING. Consumes ONLY the authored `<Style>` content into
    // the live S362 data model (TargetType, BasedOn, Setters, Triggers); a
    // Style has NO visual children (it is a resource), so this replaces the
    // generic build() attr/child handling for Style.
    for (const ParsedAttribute& attr : element.attributes)
    {
        if (attr.directive != XmlDirectiveType::None)
        {
            continue; // x:Key is read by the dictionary capture, not here
        }
        if (attr.key == "TargetType")
        {
            style.set_target_type(std::string(attr.value));
        }
        else if (attr.key == "BasedOn")
        {
            // `BasedOn="{StaticResource base}"` — record the WIRE for the end-of-
            // Load based_on resolution (the base Style is a dict resource).
            const ResourceMarkup rm = parse_resource_markup(attr.value);
            if (rm.recognized && rm.kind == ResourceReferenceKind::Static)
            {
                deferred_based_on_.emplace_back(&style, std::string(rm.key));
            }
            else
            {
                throw VamlReaderException(
                    VamlReaderError::UnknownProperty, "BasedOn",
                    "Style BasedOn must be a {StaticResource <key>}");
            }
        }
        // Any other Style attribute (e.g. a future one) is ignored reservedly.
    }

    for (const ParsedElement& child : element.children)
    {
        const std::string_view childLocal = child.local_name.empty() ? child.tagName
                                                                     : child.local_name;
        if (childLocal == "Setter")
        {
            Setter s;
            parse_setter(child, "Style", s);
            style.add_setter(std::move(s));
            continue;
        }
        if (childLocal == "Style.Triggers" || childLocal == "Triggers")
        {
            // Each trigger child: <PropertyTrigger Property Value> or
            // <DataTrigger Binding Value>, optionally with child <Setter>s.
            for (const ParsedElement& trig : child.children)
            {
                const std::string_view tl = trig.local_name.empty() ? trig.tagName
                                                                    : trig.local_name;
                if (tl == "PropertyTrigger")
                {
                    std::string prop;
                    std::string value;
                    for (const ParsedAttribute& a : trig.attributes)
                    {
                        if (a.directive == XmlDirectiveType::None && a.key == "Property")
                        {
                            prop = a.value;
                        }
                        else if (a.directive == XmlDirectiveType::None && a.key == "Value")
                        {
                            value = a.value;
                        }
                    }
                    const DependencyProperty* dp = DependencyProperty::TryGet(prop);
                    if (dp == nullptr)
                    {
                        throw VamlReaderException(
                            VamlReaderError::UnknownProperty, prop,
                            "unknown PropertyTrigger target property '" + prop + "'");
                    }
                    PropertyTrigger pt;
                    pt.property = PropertyId(vaml::intern(prop));
                    pt.value = convertLiteral(*dp, value);
                    // child <Setter>s (triggering setters).
                    for (const ParsedElement& ts : trig.children)
                    {
                        if ((ts.local_name.empty() ? ts.tagName : ts.local_name) == "Setter")
                        {
                            Setter s;
                            parse_setter(ts, "PropertyTrigger", s);
                            pt.setters.push_back(std::move(s));
                        }
                    }
                    style.add_trigger(Trigger::MakeProperty(std::move(pt)));
                }
                else if (tl == "DataTrigger")
                {
                    std::string binding;
                    std::string value;
                    for (const ParsedAttribute& a : trig.attributes)
                    {
                        if (a.directive == XmlDirectiveType::None && a.key == "Binding")
                        {
                            binding = a.value;
                        }
                        else if (a.directive == XmlDirectiveType::None && a.key == "Value")
                        {
                            value = a.value;
                        }
                    }
                    if (binding.empty())
                    {
                        throw VamlReaderException(
                            VamlReaderError::UnknownProperty, "Binding",
                            "a <DataTrigger> must name a 'Binding' path");
                    }
                    DataTrigger dt;
                    dt.path = PropertyPath::parse(binding);
                    // The DataTrigger's authored match Value: a plain literal
                    // String compared (by the S363 applier, S240 data-read
                    // equality) against the bound path's resolved value. A
                    // resource-REFERENCE DataTrigger match value is recorded
                    // forward (NO-GAP — the S363 engine evaluates literal-match
                    // data triggers today; reference-valued trigger values are
                    // a later resource/trigger refinement, not silent).
                    dt.value = Value::String(value);
                    for (const ParsedElement& ts : trig.children)
                    {
                        if ((ts.local_name.empty() ? ts.tagName : ts.local_name) == "Setter")
                        {
                            Setter s;
                            parse_setter(ts, "DataTrigger", s);
                            dt.setters.push_back(std::move(s));
                        }
                    }
                    style.add_trigger(Trigger::MakeData(std::move(dt)));
                }
                // Unknown trigger child -> structured authoring error.
                else
                {
                    throw VamlReaderException(
                        VamlReaderError::UnknownProperty, std::string(tl),
                        "unknown Style trigger child '" + std::string(tl) + "'");
                }
            }
            continue;
        }
        // A non-trigger/non-setter child of a Style is an authoring error.
        throw VamlReaderException(
            VamlReaderError::UnknownProperty, std::string(childLocal),
            "a <Style> may only contain <Setter> and <Style.Triggers> (got '" +
                std::string(childLocal) + "')");
    }
}

namespace {
// S454 — finds a Style* by key across a set of dictionaries (self + merged),
// used by resolve_deferred_based_on to wire a derived Style's base.
vaml::Style* find_style_by_key(const std::vector<vaml::ResourceDictionary*>& dicts,
                               const std::string& key) noexcept
{
    for (vaml::ResourceDictionary* d : dicts)
    {
        if (d == nullptr)
        {
            continue;
        }
        const vaml::ResourceValue* v = d->try_get(key);
        if (v == nullptr)
        {
            continue;
        }
        if (v->kind() != vaml::ResourceValue::Kind::Object)
        {
            continue;
        }
        // The authored Style is owned by the dictionary (per-Load); we wire a
        // NON-OWNING based_on pointer to it. The reader controls both lifetimes
        // (the dict lives in the S360 side-table / owned_merged_ for the Load),
        // so the const_cast is safe + documented: we are granting Style* access
        // to the SAME object the dict owns, for base-wiring within one Load.
        vaml::Style* s = const_cast<vaml::Style*>(
            dynamic_cast<const vaml::Style*>(v->object()));
        if (s != nullptr)
        {
            return s;
        }
    }
    return nullptr;
}
} // namespace

VAML_CORE_EXPORT
void VamlReader::resolve_deferred_based_on()
{
    for (auto& [derived, key] : deferred_based_on_)
    {
        vaml::Style* base = find_style_by_key(built_dicts_, key);
        if (base == nullptr)
        {
            throw VamlReaderException(
                VamlReaderError::UnknownProperty, key,
                "Style BasedOn '{StaticResource " + key +
                    "}' did not resolve to an authored Style");
        }
        derived->set_based_on(base);
    }
}

VAML_CORE_EXPORT
VAML_CORE_EXPORT
void VamlReader::report_unknown_property(std::string key, std::string what)
{
    // 11d-DEBUG — strict default throws (unchanged, fail-loud); collect mode
    // records + returns so the build continues scanning the whole document.
    if (!collectIssues_)
    {
        throw VamlReaderException(VamlReaderError::UnknownProperty,
                                  std::move(key), std::move(what));
    }
    issues_.push_back(ScanIssue{VamlReaderError::UnknownProperty, std::move(what)});
}

VAML_CORE_EXPORT
std::unique_ptr<DependencyObject> VamlReader::Load(const ParsedElement& root)
{
    ensure_startup_registrations();

    // Fresh read: each Load call must not leak children / S092 records from a
    // prior call (each object owned exactly once, per D1; every S092 registry
    // is per-Load and cleared here).
    retained_.clear();
    visual_finalized_ = false; // S440: a fresh Load re-arms the finalize hand-off
    names_.clear();
    root_class_.clear();
    deferred_extensions_.clear();
    deferred_event_handlers_.clear();
    live_bindings_.clear();          // S252: per-Load live binding scope (RAII detach)
    dynamicResourceLinks_.clear();   // S361: per-Load dynamic-resource scope
    owned_merged_.clear();           // S453: per-Load merged-resource scope
    built_dicts_.clear();            // S454: per-Load authored-dict index
    deferred_based_on_.clear();      // S454: per-Load Style BasedOn wires
    issues_.clear();                 // 11d-DEBUG: per-Load scan-issue scope

    // S144: reserve the per-Load vectors from the parsed tree's known size
    // (no reallocation churn while building).
    retained_.reserve(count_non_root_descendants(root));
    deferred_extensions_.reserve(root.attributes.size() + count_non_root_descendants(root));
    deferred_event_handlers_.reserve(root.attributes.size() + count_non_root_descendants(root));

    std::unique_ptr<DependencyObject> built = build(root, 0);

    // S454 — resolve every deferred Style BasedOn ({StaticResource base}) now
    // that all authored Style resources exist (across built_dicts_).
    resolve_deferred_based_on();

    return built;
}

// ------------------------------------------------------------------ S092 Strand A
VAML_CORE_EXPORT
DependencyObject* VamlReader::named(std::string_view name) const noexcept
{
    const auto it = std::find_if(
        names_.begin(), names_.end(),
        [&](const std::pair<std::string_view, DependencyObject*>& entry) {
            return entry.first == name;
        });
    return it == names_.end() ? nullptr : it->second;
}

VAML_CORE_EXPORT
std::size_t VamlReader::name_count() const noexcept
{
    return names_.size();
}

VAML_CORE_EXPORT
const std::vector<std::pair<std::string_view, DependencyObject*>>&
VamlReader::names() const noexcept
{
    return names_;
}

VAML_CORE_EXPORT
const std::string& VamlReader::root_class() const noexcept
{
    return root_class_;
}

// ------------------------------------------------------------------ S440 Hand-off
// The S120/S121 visual-tree hand-off (closes the roadmap-recorded NO-GAP:
// "children transfer into the UIElement visual tree at S120/S121"). See header.
VAML_CORE_EXPORT
std::size_t VamlReader::finalize_visual_tree()
{
    if (visual_finalized_)
    {
        return 0; // idempotent
    }

    std::size_t transferred = 0;

    // We need the built ROOT to walk parents: finalize must be invoked on the
    // reader AFTER Load (which owns the root). The reader does not retain the
    // root pointer, so REBUILD the parent->children adjacency from the parent
    // links: every retained child's parent() is its container. We transfer by
    // matching retained children to their parent (the parent link already
    // decoded by the S039/S041 seed during build), then append them to the
    // parent's S121 list in DOCUMENT order.
    //
    // Ownership: the child is owned by `retained_`; add_child takes ownership,
    // so we RELEASE from retained_ into the parent (no double own). Because we
    // iterate retained_ and erase while transferring, group children by parent
    // first (stable document order per parent), then transfer each. A child
    // whose parent is NOT a UIElement (or a non-UIElement child, or a
    // UIElement child under a non-UIElement parent) cannot enter an S121 list:
    // it STAYS owned here and is reported via the diagnostics channel.
    //
    // (Parent links were set during build via set_parent — S039/S041; every
    // retained child has a parent. The owner of the tree is the caller's root.)

    // Group index: parent raw repeat visited count is not reliable (a parent
    // may be a retained child too). Instead we process in two sub-steps:
    //   1) Nested parents first (children before their own parents) so when we
    //      attach a child to a parent that is ALSO a retained child+container,
    //      that parent is already attached to ITS parent (still owned here).
    //   2) Within finalize we only ATTACH a child to the parent; we do NOT
    //      detach the parent link (attach_child re-links), and we erase each
    //      transferred child from retained_ as we go.
    //
    // To satisfy document order + nested-parent-first correctly, we process the
    // retained list in REVERSE-by-construction is wrong; instead: repeatedly
    // walk retained_ transferring any child whose parent slot is not itself in
    // retained_ (i.e., a LEAF-most container is attached after its own subtree
    // is done). Simpler + correct: do up to N passes (N = retained size); on
    // each pass, for every child still in retained_, if the child's parent is
    // NOT still in retained_ (the parent is the caller's root, or was already
    // transferred), then finalize this child (set its parent = its target).
    //
    // Because add_child checks child->parent() == nullptr and throws, we must
    // CLEAR the S039 parent seed before attach, then let attach_child set the
    // correct parent. (The seed parent is the same object anyway.)

    // pass loop
    for (;;)
    {
        bool progress = false;
        for (std::size_t i = 0; i < retained_.size(); )
        {
            DependencyObject* child = retained_[i].get();
            UIElement* childUI = dynamic_cast<UIElement*>(child);
            DependencyObject* parent = child->parent();
            UIElement* parentUI = dynamic_cast<UIElement*>(parent);

            // Determine if the parent is still owned by retained_ (not yet
            // transferred): scan for an entry whose raw == parent.
            bool parentStillOwned = false;
            for (std::size_t j = 0; j < retained_.size(); ++j)
            {
                if (retained_[j].get() == parent)
                {
                    parentStillOwned = true;
                    break;
                }
            }

            if (childUI != nullptr && parentUI != nullptr && !parentStillOwned)
            {
                // Parent is the caller's root or already transferred: finalize.
                // add_child requires parent()==nullptr: detach the seed parent
                // (S039) first, then attach (which re-links to parentUI).
                childUI->set_parent(nullptr);
                std::unique_ptr<UIElement> adopt(
                    static_cast<UIElement*>(retained_[i].release()));
                // 11j-1 (GAP CLOSE): a `<UserControl>`'s direct child is its
                // single CONTENT — route it through set_content (the real
                // content slot, which ALSO add_childs it into the S121 list so
                // it renders via the generic classifier). This is the corpus's
                // `<UserControl><Grid/></UserControl>` root/content pattern.
                if (UserControl* uc = dynamic_cast<UserControl*>(parentUI))
                {
                    uc->set_content(std::move(adopt));
                }
                else
                {
                    parentUI->add_child(std::move(adopt));
                }
                retained_.erase(retained_.begin() + static_cast<std::ptrdiff_t>(i));
                ++transferred;
                progress = true;
                // do not advance i (element removed)
                continue;
            }

            // Non-transferable now: either child isn't a UIElement, parent not a
            // UIElement, or the parent still needs its own attach first.
            ++i;
        }

        if (!progress)
        {
            break;
        }
    }

    // Any child STILL in retained_ is NOT transferable into an S121 list (a
    // non-UIElement child, or a UIElement under a non-UIElement parent). These
    // stay owned here. This is never silent: the method returns the transfer
    // count so the caller can assert every child reached the visual tree, and
    // a dedicated S440 test asserts the untransferable set is exactly reported
    // by the remaining retained_ size. (No non-UIElement built-ins exist today,
    // so in practice transferred == the child count whenever the root is a
    // UIElement; the count is the contract the app-shell relies on.)

    visual_finalized_ = true;
    return transferred;
}

// ------------------------------------------------------------------ S092 Strand B
VAML_CORE_EXPORT
const std::vector<DeferredExtension>&
VamlReader::deferred_extensions() const noexcept
{
    return deferred_extensions_;
}

// ------------------------------------------------------------------ S252 — markup {Binding}
VAML_CORE_EXPORT
std::size_t VamlReader::BindExtensions()
{
    std::size_t bound = 0;

    for (const DeferredExtension& ext : deferred_extensions_)
    {
        // Parse the S2-tag source ("{Binding Title}" / "{Binding Title, Mode=TwoWay}"
        // / "{Command DoIt}" / "{StaticResource X}").
        const BindingMarkup markup = parse_markup_binding(ext.source);

        // kind None (StaticResource / x:Null / a bare-naming extension) -> NOT this
        // layer's extension; left deferred (S361). Not an error, not counted.
        if (markup.kind == MarkupExtensionKind::None)
        {
            continue;
        }

        // kind Command -> S253 resolves it (a command registry). Left deferred,
        // not an error, not counted (recorded NO-GAP at S252 -> S253).
        if (markup.kind == MarkupExtensionKind::Command)
        {
            continue;
        }

        // kind Static / Type -> S461 resolves them (the static/type registries).
        // Left deferred, not an error, not counted (the S461 extension surface).
        if (markup.kind == MarkupExtensionKind::Static || markup.kind == MarkupExtensionKind::Type)
        {
            continue;
        }

        // kind Binding. A malformed {Binding ...} source is a Class A AUTHORING
        // error — reported loudly, never silently left deferred.
        if (markup.malformed || markup.path.empty())
        {
            throw VamlReaderException(
                VamlReaderError::MalformedBindingMarkup, std::string(ext.key),
                "malformed {Binding} markup on property '" + std::string(ext.key) +
                    "': source '" + std::string(ext.source) + "'");
        }

        // Build the binding spec against the data context (the app-constructed
        // VM). The target is the object the extension was authored on
        // (ext.target); an unregistered target property is a Class B runtime
        // outcome (SetTargetFailed on the push), not a reader error here.
        BindingExpressionSpec spec;
        spec.source     = dataContext_;
        spec.sourcePath = std::string(markup.path); // Binding path
        spec.target     = const_cast<DependencyObject*>(ext.target);
        spec.targetProp = std::string(ext.key);
        spec.policy     = vaml::view_binding_policy_default();   // Fallback (S250_1)
        // WPF-faithful: {Binding} without Mode= is OneWay; Mode= overrides.
        spec.mode = markup.hasMode
                        ? (markup.mode == "TwoWay"   ? BindingMode::TwoWay
                           : markup.mode == "OneTime" ? BindingMode::OneTime
                                                       : BindingMode::OneWay)
                        : BindingMode::OneWay;
        spec.sourceType = "ObservableObject"; // the VM data context type (diagnostic metadata)

        auto expr = BindingExpression::create(spec);
        if (expr)
        {
            // Apply the FIRST transfer so the bound value lands on the target now
            // (the S242 first-push — the S2 tag resolved).
            (void)expr->TransferSourceToTarget();
            live_bindings_.push_back(std::move(expr));
            ++bound;
        }
    }

    return bound;
}

// ------------------------------------------------------------------ S361 — markup {StaticResource}/{DynamicResource}
VAML_CORE_EXPORT
std::size_t VamlReader::BindResources()
{
    ResourceResolver resolver;
    std::size_t applied = 0;
    for (const DeferredExtension& ext : deferred_extensions_)
    {
        // Parse the S2-tag source ("{StaticResource X}" / "{DynamicResource X}").
        const ResourceMarkup m = parse_resource_markup(ext.source);
        if (!m.recognized || m.malformed)
        {
            // Not a resource reference (a {Binding} / {Command} / x:Null / bare
            // extension) — NOT this layer's row. Left deferred to its own layer.
            continue;
        }
        // The resource markup is authored on a UIElement target (only a
        // UIElement has the tree-scope Resources walk + a DP to set). A non-UI
        // target is a no-op (recorded NO-GAP — resources resolve on UI elements).
        auto* ui = dynamic_cast<UIElement*>(const_cast<DependencyObject*>(ext.target));
        if (ui == nullptr)
        {
            continue;
        }
        // The resolver applies a Static (once, missing -> the SHARED decision-3
        // ResourceLookupException(MissingKey) — fail loud) or records + applies
        // a Dynamic link (applies now + records into dynamicResourceLinks_;
        // transiently-missing HOLDS, never throws).
        applied += resolver.apply_markup(*ui, ext.key, ext.source, dynamicResourceLinks_);
    }
    return applied;
}

VAML_CORE_EXPORT
std::size_t VamlReader::ReapplyDynamicResources()
{
    ResourceResolver resolver;
    return resolver.reapply_dynamic_resources(dynamicResourceLinks_);
}

// ------------------------------------------------------------------ S253 — {Command}
VAML_CORE_EXPORT
void VamlReader::register_command(std::string_view name, ICommand* command) noexcept
{
    // Idempotent: a repeated name keeps the FIRST registration (the app owns
    // the command; the reader stores a non-owning pointer). No throw.
    if (command == nullptr || name.empty())
        return;
    const auto it = std::find_if(commands_.begin(), commands_.end(),
                                 [&](const std::pair<std::string, ICommand*>& c) {
                                     return c.first == name;
                                 });
    if (it == commands_.end())
    {
        commands_.emplace_back(std::string(name), command);
    }
}

VAML_CORE_EXPORT
ICommand* VamlReader::ResolveCommand(std::string_view name)
{
    const auto it = std::find_if(commands_.begin(), commands_.end(),
                                 [&](const std::pair<std::string, ICommand*>& c) {
                                     return c.first == name;
                                 });
    return it == commands_.end() ? nullptr : it->second;
}

VAML_CORE_EXPORT
bool VamlReader::ExecuteCommand(std::string_view name, const Value& parameter)
{
    // S464 — resolve the app-registered command by name and EXECUTE it with the
    // parameter. When the parameter is Null, pass a Null parameter (the command
    // treats it as its default — S246). A CanExecute()==false command resolves
    // but does not execute (WPF-faithful, never a silent no-op: the caller can
    // query CanExecute separately / via a bound IsEnabled). An unregistered
    // name returns false (never throws).
    ICommand* cmd = ResolveCommand(name);
    if (cmd == nullptr)
    {
        return false;
    }
    if (!cmd->CanExecute(parameter))
    {
        return false; // resolve ok but cannot execute with this parameter
    }
    cmd->Execute(parameter);
    return true;
}

// ------------------------------------------------------------------ S461 — {x:Static}/{x:Type}
VAML_CORE_EXPORT
void VamlReader::register_static_value(std::string token, Value value)
{
    if (token.empty())
    {
        return;
    }
    // Idempotent: a repeated token keeps the FIRST registration (the app owns
    // the static value; the reader stores a copy). No throw.
    const auto it = std::find_if(static_values_.begin(), static_values_.end(),
                                 [&](const std::pair<std::string, Value>& s) {
                                     return s.first == token;
                                 });
    if (it == static_values_.end())
    {
        static_values_.emplace_back(std::move(token), std::move(value));
    }
}

VAML_CORE_EXPORT
Value VamlReader::ResolveStatic(std::string_view token) const noexcept
{
    // WPF-faithful token resolution: `{x:Static local:Constants.AppName}` names
    // the LAST `:`-split segment ("Constants.AppName"). The reader resolves by
    // that tail token (the app registered it without the namespace prefix).
    std::string_view t = token;
    const std::size_t colon = token.rfind(':');
    if (colon != std::string_view::npos)
    {
        t = token.substr(colon + 1);
    }
    const auto it = std::find_if(static_values_.begin(), static_values_.end(),
                                 [&](const std::pair<std::string, Value>& s) {
                                     return s.first == t;
                                 });
    return it == static_values_.end() ? Value::Null() : it->second;
}

VAML_CORE_EXPORT
void VamlReader::register_type_value(std::string name, std::string typeName)
{
    if (name.empty())
    {
        return;
    }
    const auto it = std::find_if(type_values_.begin(), type_values_.end(),
                                 [&](const std::pair<std::string, std::string>& t) {
                                     return t.first == name;
                                 });
    if (it == type_values_.end())
    {
        type_values_.emplace_back(std::move(name), std::move(typeName));
    }
}

VAML_CORE_EXPORT
Value VamlReader::ResolveType(std::string_view name) const noexcept
{
    const auto it = std::find_if(type_values_.begin(), type_values_.end(),
                                 [&](const std::pair<std::string, std::string>& t) {
                                     return t.first == name;
                                 });
    return it == type_values_.end() ? Value::Null() : Value::String(it->second);
}

VAML_CORE_EXPORT
std::size_t VamlReader::ResolveStaticTypeExtensions()
{
    std::size_t resolved = 0;
    for (const DeferredExtension& ext : deferred_extensions_)
    {
        const BindingMarkup markup = parse_markup_binding(ext.source);
        if (markup.kind != MarkupExtensionKind::Static && markup.kind != MarkupExtensionKind::Type)
        {
            continue; // not this layer's row
        }
        if (markup.malformed || markup.path.empty())
        {
            throw VamlReaderException(
                VamlReaderError::MalformedBindingMarkup, std::string(ext.key),
                "malformed {x:Static}/{x:Type} markup on property '" +
                    std::string(ext.key) + "': source '" + std::string(ext.source) + "'");
        }

        // Resolve the value (a static value / a type object) from the registries.
        const Value value = (markup.kind == MarkupExtensionKind::Static)
                                ? ResolveStatic(markup.path)
                                : ResolveType(markup.path);
        if (value.isNull())
        {
            throw VamlReaderException(
                VamlReaderError::MalformedBindingMarkup, std::string(ext.key),
                std::string(markup.kind == MarkupExtensionKind::Static ? "{x:Static }" : "{x:Type }") +
                    " to unresolved " +
                    std::string(markup.kind == MarkupExtensionKind::Static ? "static" : "type") +
                    " '" + std::string(markup.path) + "' on property '" +
                    std::string(ext.key) + "'");
        }

        // Push the resolved value onto the authored target's DP (the extension key).
        auto* target = const_cast<DependencyObject*>(ext.target);
        if (target != nullptr)
        {
            try
            {
                (void)target->Set(PropertyId(ext.key), value);
            }
            catch (...)
            {
                // Wrong-tag target DP — report via the binding-diagnostics channel
                // (never silent), skip.
                BindingDiagnostic diag;
                diag.targetProp = std::string(ext.key);
                diag.reasonCode = BindingReasonCode::SetTargetFailed;
                diag.sourcePath = std::string(markup.path);
                diag.direction  = BindingDirection::OneWay;
                diag.detail     = "resolve_static_type: target property '" +
                                  std::string(ext.key) + "' rejected the resolved value";
                vaml::binding_diagnostics().emit(diag);
                continue;
            }
        }
        ++resolved;
    }
    return resolved;
}

// ------------------------------------------------------------------ S092 Strand C
VAML_CORE_EXPORT
void VamlReader::register_event(std::string_view eventName)
{
    if (std::find(event_names_.begin(), event_names_.end(), eventName) ==
        event_names_.end())
    {
        event_names_.emplace_back(eventName);
    }
}

VAML_CORE_EXPORT
const std::vector<DeferredEventHandler>&
VamlReader::deferred_event_handlers() const noexcept
{
    return deferred_event_handlers_;
}

// ------------------------------------------------------------------ S322 — markup event wiring
VAML_CORE_EXPORT
void VamlReader::bind_class_handler(std::string_view handlerName,
                                    RoutedEventHandler handler)
{
    if (handlerName.empty() || !handler)
    {
        return;
    }
    // Idempotent: first registration wins (app-owned config, like register_command).
    const auto it = std::find_if(class_handlers_.begin(), class_handlers_.end(),
                                 [&](const std::pair<std::string, RoutedEventHandler>& h) {
                                     return h.first == handlerName;
                                 });
    if (it == class_handlers_.end())
    {
        class_handlers_.emplace_back(std::string(handlerName), std::move(handler));
    }
}

VAML_CORE_EXPORT
std::size_t VamlReader::BindEvents()
{
    std::size_t wired = 0;

    for (const DeferredEventHandler& rec : deferred_event_handlers_)
    {
        // 1) Map the authored event name -> a registered RoutedEvent. An
        //    UNREGISTERED event name is NOT wired and is reported through the
        //    S250_1 binding-diagnostics channel (one channel, two consumers) —
        //    never silent. The event must be registered (register_event plus a
        //    matching RoutedEvent static). S463 (dep:S462): resolves by the
        //    EVENT TAIL (so the raised-attached `Owner.Event` form
        //    `StackPanel.Button.Click` maps "Click") against the known bubbling
        //    set — the authored-set events Loaded/Closing/TextChanged are added
        //    to the Click/KeyDown/KeyUp table.
        const std::string_view tail = event_tail(rec.eventName);
        const RoutedEvent* event = nullptr;
        {
            if (tail == "Click")
            {
                event = &ClickEvent();
            }
            else if (tail == "KeyDown")
            {
                event = &KeyDownEvent();
            }
            else if (tail == "KeyUp")
            {
                event = &KeyUpEvent();
            }
            else if (tail == "Loaded")
            {
                event = &LoadedEvent();
            }
            else if (tail == "Closing")
            {
                event = &ClosingEvent();
            }
            else if (tail == "TextChanged")
            {
                event = &TextChangedEvent();
            }
        }

        if (event == nullptr)
        {
            // Unknown event name — an authoring-style note via the shared
            // diagnostics channel (S250_1 / ADR-016), never silent. Not a
            // throw (a future event may not yet be registered); reported.
            BindingDiagnostic diag;
            diag.targetProp = std::string(rec.eventName);
            diag.reasonCode = BindingReasonCode::SetTargetFailed; // a wiring-failure note
            diag.sourcePath = std::string(rec.handlerName);
            diag.direction  = BindingDirection::OneWay;
            diag.detail     = "bind_event: unknown routed event name '" +
                              std::string(rec.eventName) + "' (event must be registered)";
            vaml::binding_diagnostics().emit(diag);
            continue;
        }

        // 2) Resolve the handler by (class, handlerName) from the S322 class
        //    owner registry. A handlerName with NO registered handler is a
        //    Class A authoring error — fail LOUD (structured VamlReaderException
        //    naming the class + handler), never silent.
        const auto hit = std::find_if(class_handlers_.begin(), class_handlers_.end(),
                                      [&](const std::pair<std::string, RoutedEventHandler>& h) {
                                          return h.first == rec.handlerName;
                                      });
        if (hit == class_handlers_.end())
        {
            throw VamlReaderException(
                VamlReaderError::NoEventHandler, std::string(rec.handlerName),
                "no registered handler '" + std::string(rec.handlerName) +
                    "' for '" + (root_class_.empty() ? std::string("<root>") : root_class_) +
                    "' on event '" + std::string(rec.eventName) + "'");
        }

        // 3) Attach the live routed handler on the element the markup was
        //    authored on (the S092 record's target) — the S321 surface. The
        //    handler stays live for this Load (per-Load scope); the class-owner
        //    registration persists.
        auto* element = dynamic_cast<UIElement*>(const_cast<DependencyObject*>(rec.target));
        if (element == nullptr)
        {
            // The event was authored on a non-UIElement object — cannot hold a
            // routed handler. Report via the diagnostics channel (never silent).
            BindingDiagnostic diag;
            diag.targetProp = std::string(rec.eventName);
            diag.reasonCode = BindingReasonCode::SetTargetFailed;
            diag.sourcePath = std::string(rec.handlerName);
            diag.direction  = BindingDirection::OneWay;
            diag.detail     = "bind_event: target is not a UIElement; cannot wire event '" +
                              std::string(rec.eventName) + "'";
            vaml::binding_diagnostics().emit(diag);
            continue;
        }

        (void)element->add_routed_handler(*event, hit->second);
        ++wired;
    }

    return wired;
}

VAML_CORE_EXPORT
std::unique_ptr<DependencyObject> VamlReader::Load(std::string_view text)
{
    return Load(parse_vaml(text));
}

} // namespace vaml
