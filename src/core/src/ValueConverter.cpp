// S089 (dep:S088) — Value converter registry out-of-line definitions (pimpl).
// Delivers the four headline conversions:
//   "42"->int, "10,20"->Thickness, "#FF0000"->Color, "true"->bool.
//
// The real string-parsing semantics of Color and Thickness live on the types
// directly (parse_color / parse_thickness, authored by this S089 step — the
// S030 MINIMAL REAL marker resolution). The converters here simply route the
// literal into those parsers; their malformed-input throw (a
// ValueConversionException with MalformedValue) propagates unchanged.
//
// S146 (refactor gate) — the pimpl (class Impl + impl_) is COLLAPSED: the
// registry holds its vaml::detail::NameTable<ValueConverter> BY VALUE (see
// ValueConverter.h). The delegating methods call table_ directly (no impl_->
// trampoline, no per-object heap hop, no Impl heap node).

#include "vaml/core/ValueConverter.h"

#include "vaml/core/Color.h"     // parse_color
#include "vaml/core/Thickness.h" // parse_thickness

#include <charconv>
#include <string>
#include <utility>

namespace vaml {

VAML_CORE_EXPORT
ValueConverterRegistry::ValueConverterRegistry() = default;

VAML_CORE_EXPORT
ValueConverterRegistry::~ValueConverterRegistry() = default;

VAML_CORE_EXPORT
ValueConverterRegistry::ValueConverterRegistry(ValueConverterRegistry&&) noexcept = default;

VAML_CORE_EXPORT
ValueConverterRegistry& ValueConverterRegistry::operator=(ValueConverterRegistry&&) noexcept = default;

VAML_CORE_EXPORT
void ValueConverterRegistry::register_converter(std::string_view typeName, ValueConverter converter)
{
    // The NameTable's content insert returns nullptr on a duplicate and leaves
    // the existing entry unchanged (dedupe, S032 discipline).
    if (table_.insert(std::string(typeName), std::move(converter)) == nullptr)
    {
        std::string msg = "value converter already registered: '" + std::string(typeName) + "'";
        throw ValueAlreadyRegistered(msg.c_str());
    }
}

VAML_CORE_EXPORT
Value ValueConverterRegistry::convert(std::string_view typeName, std::string_view literal) const
{
    const ValueConverter* converter = table_.find(typeName);
    if (converter == nullptr)
    {
        // No converter registered for this type -> structured UnknownType error
        // naming the type (and echoing the literal for context).
        std::string name(typeName);
        std::string what =
            "no value converter registered for type '" + name + "' (UnknownType)";
        throw ValueConversionException(ValueConversionError::UnknownType, name,
                                       std::string(literal), what);
    }
    // A converter that rejects its input throws ValueConversionException
    // (MalformedValue) — it propagates to the caller; no remapping here (the
    // converter is the authority on its own value grammar).
    return (*converter)(literal);
}

VAML_CORE_EXPORT
bool ValueConverterRegistry::try_infer_type(std::string_view literal, std::string* outType) const
{
    // Probe registered converters in INSERTION ORDER (table_.names() mirrors
    // registration order exactly — the NameTable's append-only rows_). The
    // FIRST type whose converter accepts the literal wins. "Accepts" = runs
    // without throwing; any ValueConversionException (MalformedValue or
    // otherwise) means "this type does not recognize the literal" — keep
    // probing.
    for (const std::string_view typeName : table_.names())
    {
        const ValueConverter* converter = table_.find(typeName);
        if (converter == nullptr)
        {
            continue; // defensive: the order-list never diverges from the map
        }
        try
        {
            (void)(*converter)(literal); // recognizer: only success/failure matters
        }
        catch (const ValueConversionException&)
        {
            continue; // this type rejects the literal -> try the next
        }
        if (outType != nullptr)
        {
            *outType = std::string(typeName);
        }
        return true;
    }
    return false; // no registered type accepts the literal -> UnknownLiteral
}

VAML_CORE_EXPORT
bool ValueConverterRegistry::is_registered(std::string_view typeName) const noexcept
{
    return table_.find(typeName) != nullptr;
}

VAML_CORE_EXPORT
std::size_t ValueConverterRegistry::size() const noexcept
{
    return table_.size();
}

VAML_CORE_EXPORT
std::vector<std::string_view> ValueConverterRegistry::registered_names() const
{
    return table_.names();
}

VAML_CORE_EXPORT
ValueConverterRegistry& ValueConverterRegistry::global() noexcept
{
    // Meyers singleton: function-local static constructed on first use,
    // destroyed in reverse construction order at exit. No static-init-order
    // fiasco (C++11 thread-safe function-local static init). Matches
    // PropertyRegistry::global()/DependencyProperty::global()/
    // ElementFactoryRegistry::global().
    static ValueConverterRegistry instance;
    return instance;
}

VAML_CORE_EXPORT
ValueConversionException::ValueConversionException(ValueConversionError kind,
                                                   std::string typeName,
                                                   std::string literal,
                                                   std::string what)
    : std::runtime_error(std::move(what))
    , kind_(kind)
    , type_name_(std::move(typeName))
    , literal_(std::move(literal))
{
}

namespace {

// Case-insensitive string equality ("true"/"false" / hex handled case-insensitively).
bool iequals(std::string_view a, std::string_view b)
{
    if (a.size() != b.size())
    {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i)
    {
        const char x = a[i] >= 'A' && a[i] <= 'F' ? static_cast<char>(a[i] - 'A' + 'a') : a[i];
        const char y = b[i] >= 'A' && b[i] <= 'F' ? static_cast<char>(b[i] - 'A' + 'a') : b[i];
        if (x != y)
        {
            return false;
        }
    }
    return true;
}

// Centralized MalformedValue throw for the built-in non-parser converters
// (Int32, Bool). Color/Thickness route through parse_color/parse_thickness,
// which throw the same exception from their own parser module.
[[noreturn]] void throw_malformed(std::string_view typeName, std::string_view literal)
{
    throw ValueConversionException(
        ValueConversionError::MalformedValue, std::string(typeName), std::string(literal),
        "cannot convert '" + std::string(literal) + "' to " + std::string(typeName) +
            " (MalformedValue)");
}

// Strict signed-32-bit integer parse: the WHOLE string must be one valid int
// (no surrounding/junk/trailing characters, no leading whitespace — from_chars
// does not skip whitespace).
Value convert_int32(std::string_view s)
{
    if (s.empty())
    {
        throw_malformed("Int32", s);
    }
    int value = 0;
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
    if (ec != std::errc() || ptr != (s.data() + s.size()))
    {
        throw_malformed("Int32", s);
    }
    return Value::Int32(static_cast<std::int32_t>(value));
}

Value convert_bool(std::string_view s)
{
    if (iequals(s, "true"))
    {
        return Value::Bool(true);
    }
    if (iequals(s, "false"))
    {
        return Value::Bool(false);
    }
    throw_malformed("Bool", s);
}

} // namespace

VAML_CORE_EXPORT
void register_builtin_converters()
{
    ValueConverterRegistry& registry = ValueConverterRegistry::global();

    // register_builtin_converters() is documented single-threaded startup; the
    // bool guards against a stray second call (which would otherwise throw
    // ValueAlreadyRegistered). NO-LAZY: this is a startup-ordering guard, not
    // dead code — a duplicated population is a real double-registration hazard
    // for a process-wide singleton (mirrors register_builtin_elements()).
    static bool registered = false;
    if (registered)
    {
        return;
    }
    registered = true;

    registry.register_converter("Int32", [](std::string_view s) -> Value {
        return convert_int32(s);
    });

    registry.register_converter("Thickness", [](std::string_view s) -> Value {
        return Value::Thickness(vaml::parse_thickness(s));
    });

    registry.register_converter("Color", [](std::string_view s) -> Value {
        return Value::Color(vaml::parse_color(s));
    });

    registry.register_converter("Bool", [](std::string_view s) -> Value {
        return convert_bool(s);
    });
}

} // namespace vaml
