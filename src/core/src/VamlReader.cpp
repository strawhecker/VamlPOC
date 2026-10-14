// S091 (dep:S090) — VamlReader out-of-line definitions. Builds a LIVE
// dependency-object tree from a parsed (S085) tree. See VamlReader.h for the
// full design + ownership model.

#include "vaml/core/VamlReader.h"

#include "vaml/core/DependencyProperty.h"      // DependencyProperty
#include "vaml/core/Element.h"                 // register_builtin_elements
#include "vaml/core/ElementResolver.h"         // resolve_element + exception
#include "vaml/core/ParsedElement.h"           // ParsedElement/ParsedAttribute + parse_vaml
#include "vaml/core/PropertyId.h"              // PropertyId
#include "vaml/core/ValueConverter.h"          // ValueConverterRegistry + register_builtin_converters
#include "vaml/core/XmlScanner.h"              // XmlDirectiveType

#include <string>
#include <utility>

namespace vaml {

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
std::unique_ptr<DependencyObject> VamlReader::build(const ParsedElement& element)
{
    // Step 1 — resolve into a live instance (global registry; unknown type
    // throws ElementResolutionException naming type + line:col — S088).
    std::unique_ptr<DependencyObject> obj = resolve_element(element);

    // Step 2 — set plain-literal properties.
    for (const ParsedAttribute& attr : element.attributes)
    {
        // Deferred scope (recorded NO-GAP, NOT silent):
        //   - x-language directives (x:Name/x:Class -> S092, x:Key -> S361,
        //     x:Null -> S084): skip the value; not set here.
        if (attr.directive != XmlDirectiveType::None)
        {
            continue;
        }
        //   - markup-extensions ({Binding ...} -> S092 resolution): skip; the
        //     extension is resolved by its dedicated step, not a literal set.
        if (attr.isExtension)
        {
            continue;
        }

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
        std::unique_ptr<DependencyObject> childObj = build(child);
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

    // Fresh read: each Load call must not leak children from a prior call
    // (each object owned exactly once, per D1).
    retained_.clear();
    return build(root);
}

VAML_CORE_EXPORT
std::unique_ptr<DependencyObject> VamlReader::Load(std::string_view text)
{
    return Load(parse_vaml(text));
}

} // namespace vaml
