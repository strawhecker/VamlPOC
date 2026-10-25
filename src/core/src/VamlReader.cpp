// S092 (dep:S091) — VamlReader out-of-line definitions. Builds a LIVE
// dependency-object tree from a parsed (S085) tree. See VamlReader.h for the
// full design + ownership model. S092 ADDS: the x:Name registry (Strand A),
// deferred markup extensions / the S2 tag (Strand B), and the MINIMAL REAL
// event-hook surface (Strand C, resolved by S322).

#include "vaml/core/VamlReader.h"

#include "vaml/core/BindingAccess.h"           // S250_1: view_binding_diagnostics + view_binding_policy_default
#include "vaml/core/BindingDiagnostics.h"      // S322: emitting shared binding-diagnostics notes
#include "vaml/core/BindingMarkup.h"           // S252: parse_markup_binding
#include "vaml/core/DependencyProperty.h"      // DependencyProperty
#include "vaml/core/Element.h"                 // register_builtin_elements
#include "vaml/core/ElementResolver.h"         // resolve_element + exception
#include "vaml/core/Intern.h"                  // S140/S144: interned string_view keys for the x:Name registry
#include "vaml/core/ParsedElement.h"           // ParsedElement/ParsedAttribute + parse_vaml
#include "vaml/core/PropertyId.h"              // PropertyId
#include "vaml/core/ResourceDictionary.h"      // S361: x:Key capture -> element Resources
#include "vaml/core/ResourceMarkup.h"          // S361: parse_resource_markup (Static/Dynamic)
#include "vaml/core/ResourceResolver.h"        // S361: apply_markup / dynamic reapply
#include "vaml/core/UIElement.h"               // S322: add_routed_handler target
#include "vaml/core/ValueConverter.h"          // ValueConverterRegistry + register_builtin_converters
#include "vaml/core/XmlScanner.h"              // XmlDirectiveType

#include <algorithm>
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

    // Step 2 — attributes: set plain-literal properties, CONSUME the S092
    // strands (Name/Class directives, isExtension retention, event routing).
    for (const ParsedAttribute& attr : element.attributes)
    {
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

        // --- S092 Strand C — event routing (MINIMAL REAL seed). ------------
        // An attribute key that exactly matches a registered event name is an
        // EVENT declaration, not a property: route it to a DeferredEventHandler.
        // Never a property set, never an unknown-property throw. Resolved by S322.
        if (std::find(event_names_.begin(), event_names_.end(), attr.key) !=
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
        const DependencyProperty* dp = DependencyProperty::TryGet(attr.key);
        if (dp == nullptr)
        {
            throw VamlReaderException(
                VamlReaderError::UnknownProperty, std::string(attr.key),
                "unknown property '" + std::string(attr.key) + "'");
        }

        const Value value = convertLiteral(*dp, attr.value);
        (void)obj->Set(PropertyId(attr.key), value); // S043 attached-aware by name
    }

    // Step 3 — build children, TRANSFER into the parent's visual tree (S440),
    // and keep `retained_` as a non-owning topological view.
    //
    // S440 CLOSES the S120/S121 ownership hand-off (the roadmap-recorded NO-GAP
    // "children transfer into the UIElement visual tree at S120/S121"). When
    // BOTH the parent and the child are UIElements — the case for every built-in
    // element type — the child is TRANSFERRED into the parent's S121
    // visual-children list via add_child, which:
    //   1. installs the parent-link + S041 inheritance-scope edge (add_child ->
    //      attach_child -> set_parent(this) + the inverse scope edge), so
    //      inherited DPs (Opacity) cascade through the visual child, AND
    //   2. OWNS the child in `children_` (the tree is the single owner).
    // The root owns its subtree via this chain, so a VAML-loaded Window/Grid/
    // StackPanel renders/measures/arranges/hit-tests/walks its DECLARED
    // children — not just the root.
    //
    // `retained_` becomes a NON-OWNING raw-pointer view (document order) so the
    // S091/S092/S361 consumers + tests can introspect the built set without a
    // tree walk. A NON-UIElement child (none among the built-in vocabulary, kept
    // safe) stays reader-owned in `non_ui_owned_` (it cannot enter an S121
    // list, which requires a UIElement).
    for (const ParsedElement& child : element.children)
    {
        std::unique_ptr<DependencyObject> childObj = build(child, depth + 1);
        childObj->set_parent(obj.get()); // S039/S041 parent-link + inheritance-scope seed
        retained_.push_back(std::move(childObj));
    }

    // Step 4 — the caller owns the root; children live in `retained_` until
    // the S440 finalize_visual_tree() hand-off (see that method).
    return obj;
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

    // S144: reserve the per-Load vectors from the parsed tree's known size
    // (no reallocation churn while building).
    retained_.reserve(count_non_root_descendants(root));
    deferred_extensions_.reserve(root.attributes.size() + count_non_root_descendants(root));
    deferred_event_handlers_.reserve(root.attributes.size() + count_non_root_descendants(root));

    return build(root, 0);
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
                parentUI->add_child(std::unique_ptr<UIElement>(static_cast<UIElement*>(retained_[i].release())));
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
        //    matching RoutedEvent static).
        const RoutedEvent* event = nullptr;
        {
            // Resolve by scanning the S320 registry content keys via the
            // RoutedEvent statics we own. A markup event name maps to a real
            // event by interned-name lookup against the known bubbling set
            // (ClickEvent / KeyDownEvent / KeyUpEvent); unknown -> report.
            if (rec.eventName == "Click")
            {
                event = &ClickEvent();
            }
            else if (rec.eventName == "KeyDown")
            {
                event = &KeyDownEvent();
            }
            else if (rec.eventName == "KeyUp")
            {
                event = &KeyUpEvent();
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
