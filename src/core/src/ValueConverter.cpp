// S089 (dep:S088) — Value converter registry out-of-line definitions (pimpl).
// Delivers the four headline conversions:
//   "42"->int, "10,20"->Thickness, "#FF0000"->Color, "true"->bool.
//
// The real string-parsing semantics of Color and Thickness live on the types
// directly (parse_color / parse_thickness, authored by this S089 step — the
// S030 MINIMAL REAL marker resolution). The converters here simply route the
// literal into those parsers; their malformed-input throw (a
// ValueConversionException with MalformedValue) propagates unchanged.

#include "vaml/core/ValueConverter.h"

#include "vaml/core/Color.h"     // parse_color
#include "vaml/core/Thickness.h" // parse_thickness

#include <charconv>
#include <string>
#include <unordered_map>
#include <utility>

namespace vaml {

class ValueConverterRegistry::Impl
{
public:
    // Returns true on a successful insert, false on a name collision (the
    // existing entry is left unchanged — the moving converter is consumed by
    // emplace only on success, per unordered_map guarantees).
    bool insert(std::string_view typeName, ValueConverter converter)
    {
        auto [prev, inserted] =
            map_.try_emplace(std::string(typeName), std::move(converter));
        (void)prev;
        return inserted;
    }

    const ValueConverter* find(std::string_view typeName) const noexcept
    {
        // MSVC lacks transparent heterogeneous lookup for string_view on an
        // unordered_map<std::string,...>; build a key string for the lookup.
        auto it = map_.find(std::string(typeName));
        return it == map_.end() ? nullptr : &it->second;
    }

    std::size_t size() const noexcept { return map_.size(); }

    // Insertion order: the order-list is appended only on a successful insert,
    // so it exactly mirrors registration order and never duplicates a name.
    std::vector<std::string> names() const { return order_; }

    void record_order(std::string_view typeName) { order_.emplace_back(typeName); }

private:
    std::unordered_map<std::string, ValueConverter> map_;
    std::vector<std::string> order_;
};

VAML_CORE_EXPORT
ValueConverterRegistry::ValueConverterRegistry()
    : impl_(std::make_unique<Impl>())
{
}

VAML_CORE_EXPORT
ValueConverterRegistry::~ValueConverterRegistry() = default;

VAML_CORE_EXPORT
ValueConverterRegistry::ValueConverterRegistry(ValueConverterRegistry&&) noexcept = default;

VAML_CORE_EXPORT
ValueConverterRegistry& ValueConverterRegistry::operator=(ValueConverterRegistry&&) noexcept = default;

VAML_CORE_EXPORT
void ValueConverterRegistry::register_converter(std::string_view typeName, ValueConverter converter)
{
    // Dedupe first; only record insertion order on a successful insert (so the
    // order-list never diverges from the map's names).
    if (!impl_->insert(typeName, std::move(converter)))
    {
        std::string msg = "value converter already registered: '" + std::string(typeName) + "'";
        throw ValueAlreadyRegistered(msg.c_str());
    }
    impl_->record_order(typeName);
}

VAML_CORE_EXPORT
Value ValueConverterRegistry::convert(std::string_view typeName, std::string_view literal) const
{
    const ValueConverter* converter = impl_->find(typeName);
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
bool ValueConverterRegistry::is_registered(std::string_view typeName) const noexcept
{
    return impl_->find(typeName) != nullptr;
}

VAML_CORE_EXPORT
std::size_t ValueConverterRegistry::size() const noexcept
{
    return impl_->size();
}

VAML_CORE_EXPORT
std::vector<std::string> ValueConverterRegistry::registered_names() const
{
    return impl_->names();
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
