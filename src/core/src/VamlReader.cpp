// S092 (dep:S091) — VamlReader out-of-line definitions. Builds a LIVE
// dependency-object tree from a parsed (S085) tree. See VamlReader.h for the
// full design + ownership model. S092 ADDS: the x:Name registry (Strand A),
// deferred markup extensions / the S2 tag (Strand B), and the MINIMAL REAL
// event-hook surface (Strand C, resolved by S322).

#include "vaml/core/VamlReader.h"

#include "vaml/core/DependencyProperty.h"      // DependencyProperty
#include "vaml/core/Element.h"                 // register_builtin_elements
#include "vaml/core/ElementResolver.h"         // resolve_element + exception
#include "vaml/core/ParsedElement.h"           // ParsedElement/ParsedAttribute + parse_vaml
#include "vaml/core/PropertyId.h"              // PropertyId
#include "vaml/core/ValueConverter.h"          // ValueConverterRegistry + register_builtin_converters
#include "vaml/core/XmlScanner.h"              // XmlDirectiveType

#include <algorithm>
#include <string>
#include <utility>

namespace vaml {

namespace {

// S092: true iff `name` is already registered in the reader's x:Name registry.
// Linear scan (the name registry is small per Load); keeps the header free of
// a hash-set member and preserves registration order in `names_` (a set would
// not — NO-LAZY-LLM: the ordered vector is the documented store discipline).
[[nodiscard]] bool name_registered(
    const std::vector<std::pair<std::string, DependencyObject*>>& names,
    std::string_view name)
{
    const auto it = std::find_if(
        names.begin(), names.end(),
        [&](const std::pair<std::string, DependencyObject*>& entry) {
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
// Thickness); String is handled by identity in convertLiteral and is NOT routed
// here. Returns an empty string for a value type with no converter (the caller
// throws UnknownType).
[[nodiscard]] std::string_view converterNameFor(ValueType t) noexcept
{
    switch (t)
    {
        case ValueType::Int32:     return "Int32";
        case ValueType::Bool:      return "Bool";
        case ValueType::Color:     return "Color";
        case ValueType::Thickness: return "Thickness";
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

} // namespace

VAML_CORE_EXPORT
Value VamlReader::convertLiteral(const DependencyProperty& dp,
                                 const std::string& literal)
{
    const ValueType bound = dp.metadata().boundType();

    // A String-typed property takes the literal verbatim (no converter).
    if (bound == ValueType::String)
    {
        return Value::String(literal);
    }

    const std::string_view typeName = converterNameFor(bound);
    if (typeName.empty())
    {
        // A property whose bound type has no registered converter cannot be set
        // from a plain literal — UnknownType, since no target converter exists.
        throw ValueConversionException(
            ValueConversionError::UnknownType, std::string(), literal,
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
        // x:Key -> S361 and x:Null -> S084 paths remain deferred unchanged.
        if (attr.directive == XmlDirectiveType::Name)
        {
            const std::string& name = attr.value;
            if (name_registered(names_, name))
            {
                // WPF-faithful: a name scope with a repeated name is an error;
                // fail loud, never silently last-wins. (DuplicateName — the one
                // genuinely-new reader error S092 introduces.)
                throw VamlReaderException(
                    VamlReaderError::DuplicateName, name,
                    "duplicate x:Name '" + name + "'");
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
                    VamlReaderError::ClassOnNonRoot, attr.value,
                    "x:Class on a non-root element '" + attr.value + "'");
            }
            root_class_ = attr.value;
            continue;
        }
        if (attr.directive != XmlDirectiveType::None)
        {
            // Remaining directives (x:Key -> S361, x:Null -> S084): still
            // deferred, recorded NO-GAP, NOT silent.
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
                VamlReaderError::UnknownProperty, attr.key,
                "unknown property '" + attr.key + "'");
        }

        const Value value = convertLiteral(*dp, attr.value);
        (void)obj->Set(PropertyId(attr.key), value); // S043 attached-aware by name
    }

    // Step 3 — build children, link parent, retain. Each non-root child is
    // owned exactly once by `retained_`; the S121 visual-children list is a
    // later step (recorded NO-GAP, not silent).
    for (const ParsedElement& child : element.children)
    {
        std::unique_ptr<DependencyObject> childObj = build(child, depth + 1);
        childObj->set_parent(obj.get()); // S039/S041 parent-link + inheritance-scope seed
        retained_.push_back(std::move(childObj));
    }

    // Step 4 — the caller owns the root; children live in `retained_`.
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
    names_.clear();
    root_class_.clear();
    deferred_extensions_.clear();
    deferred_event_handlers_.clear();
    return build(root, 0);
}

// ------------------------------------------------------------------ S092 Strand A
VAML_CORE_EXPORT
DependencyObject* VamlReader::named(std::string_view name) const noexcept
{
    const auto it = std::find_if(
        names_.begin(), names_.end(),
        [&](const std::pair<std::string, DependencyObject*>& entry) {
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
const std::vector<std::pair<std::string, DependencyObject*>>&
VamlReader::names() const noexcept
{
    return names_;
}

VAML_CORE_EXPORT
const std::string& VamlReader::root_class() const noexcept
{
    return root_class_;
}

// ------------------------------------------------------------------ S092 Strand B
VAML_CORE_EXPORT
const std::vector<DeferredExtension>&
VamlReader::deferred_extensions() const noexcept
{
    return deferred_extensions_;
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

VAML_CORE_EXPORT
std::unique_ptr<DependencyObject> VamlReader::Load(std::string_view text)
{
    return Load(parse_vaml(text));
}

} // namespace vaml
