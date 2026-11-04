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
// registry holds its vaml::detail::NameTable<LiteralConverter> BY VALUE (see
// ValueConverter.h). The delegating methods call table_ directly (no impl_->
// trampoline, no per-object heap hop, no Impl heap node).

#include "vaml/core/ValueConverter.h"

#include "vaml/core/Color.h"     // parse_color
#include "vaml/core/Thickness.h" // parse_thickness

#include <charconv>
#include <cmath>
#include <limits>   // std::numeric_limits (WPF `Auto` -> NaN sentinel)
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
void ValueConverterRegistry::register_converter(std::string_view typeName, LiteralConverter converter)
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
    const LiteralConverter* converter = table_.find(typeName);
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
        const LiteralConverter* converter = table_.find(typeName);
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

// Case-insensitive string equality (full ASCII A..Z fold). WPF/XAML is
// CASE-INSENSITIVE for Boolean literals ("true"/"True"/"TRUE") and for enum-
// name attributes ("Visible"/"visible"), so the built-in literal comparison
// must fold the FULL alphabet — an A..F-only fold (a camera-leftover from an
// early hex helper) wrongly rejected "True"/"TRUE"/"FALSE" and lowercase enum
// names, breaking corpus-faithful authoring (recorded + corrected at S11a1:
// the S090 "hex-only fold" note was a mis-codified quirk, not a real contract —
// hex/color literals use their OWN fold in Color.cpp's parse_color, so no hex
// path ever shared this helper). The Auto sentinel uses `iequals_auto` below,
// which is the same full fold; both accept "Auto"/"auto"/"AUTO".
bool iequals(std::string_view a, std::string_view b)
{
    if (a.size() != b.size())
    {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i)
    {
        const char x = a[i] >= 'A' && a[i] <= 'Z' ? static_cast<char>(a[i] - 'A' + 'a') : a[i];
        const char y = b[i] >= 'A' && b[i] <= 'Z' ? static_cast<char>(b[i] - 'A' + 'a') : b[i];
        if (x != y)
        {
            return false;
        }
    }
    return true;
}

// Full ASCII case-insensitive equality, used for the WPF `Auto` sentinel
// (Width="Auto"/"auto"/"AUTO" are all valid XAML authoring). This matches
// `iequals`' full A..Z fold; it is kept as a separate 4-char helper only for
// the empty/auto-specific shape (no semantic distinction from iequals).
bool iequals_auto(std::string_view s) noexcept
{
    if (s.size() != 4)
    {
        return false;
    }
    for (std::size_t i = 0; i < 4; ++i)
    {
        const char c = s[i];
        const char l = c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
        if (l != "auto"[i])
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

// Strict finite double parse: the WHOLE string must be one valid floating-point
// literal (integer "18" or real "18.5" both parse; no surrounding/junk/trailing
// characters — from_chars does not skip whitespace). NaN/Inf reject (Malformed)
// so no non-finite value can ride a Double DP (the renderer/layout never sees a
// NaN/Inf where a layout extent is expected — mirrors the finite-layout rule).
//
// EXCEPTION (the WPF `Auto` sentinel, 11d-DEBUG-resolved): the literal "Auto"
// (case-insensitive) maps to the Width/Height auto-size NaN sentinel. WPF
// authors `Width="Auto"`/`Height="Auto"` to mean "size to content"; the
// Width/Height DPs ALREADY default to NaN and layout ALREADY honors NaN as
// auto (UIElement::reconcile_axis returns the content extent for NaN/0, and
// compute_measured_desired never lets NaN into a measured/arranged rect), so
// the finite-layout rule is preserved AT THE CONSUMER, not by rejecting Auto
// here. No bare non-finite literal ("NaN"/"Inf") is accepted — Auto is the
// ONE authored auto-size token; it is unambiguous and never a typo of a value.
Value convert_double(std::string_view s)
{
    if (s.empty())
    {
        throw_malformed("Double", s);
    }
    if (iequals_auto(s))
    {
        // WPF Width="Auto"/Height="Auto" -> the auto-size NaN sentinel. This is
        // the ONLY non-numeric literal the Double converter accepts (a size
        // DP's auto marker); it rides the same NaN payload the DP defaults to.
        return Value::Double(std::numeric_limits<double>::quiet_NaN());
    }
    double value = 0.0;
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
    if (ec != std::errc() || ptr != (s.data() + s.size()) || !std::isfinite(value))
    {
        throw_malformed("Double", s);
    }
    return Value::Double(value);
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

// ---------------------------------------------------------------------------
// S600 (dep:S596) — the Int32 VALUE-PATH converter is now NUMERIC-ONLY.
//
// S440/S11j-4 previously carried the common ENUM-NAME map here (enum_name_to_int32).
// S11p (S600, root A1) REMOVES that cross-cut behavior-guess entirely: an
// Int32-typed DP's enum names are now resolved EXCLUSIVELY through the per-DP
// enum-domain table (EnumDomainRegistry::convert_int32_for_dp, keyed by the DP's
// DECLARED bound domain — WPF Enum.Parse semantics, see EnumDomainRegistry.h).
//
// Rationale (NO-GAP: no behavior-guess conversion survives): the shared map
// could not distinguish `Dock` (Left=0) from `HorizontalAlignment` (Left=1) and
// mis-resolved `DockPanel.Dock="Left"` to 1 (=Top). The registry-level "Int32"
// converter has NO DP context, so it must NOT guess enum names — doing so would
// reintroduce the very wrong-typed class A1 exists to kill. The readers now
// route Int32 literals through convert_int32_for_dp BEFORE reaching this
// registry converter, so this path serves only: (a) numeric Int32 DPs and
// (b) the try_infer_type recognizer (which needs numeric-only semantics — an
// enum name is not a type-inferable numeral).
Value convert_int32_value(std::string_view s)
{
    if (s.empty())
    {
        throw_malformed("Int32", s);
    }
    // Numeric literals are the ONLY valid registry-level Int32 input now.
    // (Enum names require a DP's declared domain — they never resolve here.)
    int value = 0;
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
    if (ec == std::errc() && ptr == (s.data() + s.size()))
    {
        return Value::Int32(static_cast<std::int32_t>(value));
    }
    throw_malformed("Int32", s);
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

    // S440 extends the S089 built-in converter set with the Double converter +
    // the Int32 common enum-name surface (see convert_int32_value / convert_double).
    // Registration order is the try_infer_type PROBE order: Int32 stays before
    // Double so a whole-number literal still infers as Int32 (numeric-only DPs
    // keep the Int32 tag), while convertLiteral routes by the DP's boundType
    // directly (a Double DP always takes the Double converter regardless of
    // registration order).
    registry.register_converter("Int32", [](std::string_view s) -> Value {
        return convert_int32_value(s);
    });

    registry.register_converter("Double", [](std::string_view s) -> Value {
        return convert_double(s);
    });

    registry.register_converter("Thickness", [](std::string_view s) -> Value {
        return Value::Thickness(vaml::parse_thickness(s));
    });

    // S601 (dep:S600) root A2 — GEOMETRIC/UNIT value type. GridLength becomes a
    // FIRST-CLASS registered converter (routes to the 11k-3 parse_grid_length:
    // "Auto"/"1*"/"42" with the correct GridUnitType), so a GridLength-bound DP
    // converts through the SAME ValueConverterRegistry authority as every other
    // value type — and try_infer_type/`<GridLength>` value-element share it.
    // WPF-faithful: GridLengthConverter.FromString (Auto / value±unit / Star).
    registry.register_converter("GridLength", [](std::string_view s) -> Value {
        return Value::GridLength(vaml::parse_grid_length(s));
    });

    registry.register_converter("Color", [](std::string_view s) -> Value {
        return Value::Color(vaml::parse_color(s));
    });

    registry.register_converter("Bool", [](std::string_view s) -> Value {
        return convert_bool(s);
    });
}

} // namespace vaml
