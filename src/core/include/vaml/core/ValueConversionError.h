#pragma once
// S089 (dep:S088) — Value conversion error types.
//
// These live in their OWN header (not ValueConverter.h) for a structural
// reason: the parsing semantics of Color and Thickness (parse_color/
// parse_thickness) must throw ValueConversionException, but Color.h/Thickness.h
// cannot include ValueConverter.h (which includes Value.h -> Color.h/Thickness.h
// — a cycle). Hoisting the error enum + exception into this lean header gives
// parse_color/parse_thickness and ValueConverterRegistry a shared,
// non-circular home for the conversion-error vocabulary.
//
// MINIMAL taxonomy for S089 — enough for the four conversions to be real;
// S090's formal unknown-literal error taxonomy confirms/extends it (recorded
// NO-GAP, not silent).

#include <stdexcept>
#include <string>
#include <string_view>

namespace vaml {

// S089: the converter error taxonomy — what a conversion attempt can report.
enum class ValueConversionError
{
    None,          // no conversion error
    UnknownType,   // no converter registered for the requested type name
    MalformedValue // a registered converter / parser rejected its input literal
};

// S089: the structured conversion error. what() renders
//   "cannot convert '<literal>' to <type> (MalformedValue)"       or
//   "no value converter registered for type '<type>' (UnknownType)".
// kind()/type_name()/literal() expose the structured parts.
class ValueConversionException : public std::runtime_error
{
public:
    ValueConversionException(ValueConversionError kind, std::string typeName,
                             std::string literal, std::string what);

    ValueConversionError kind() const noexcept { return kind_; }
    // The target type name the caller asked to convert to.
    const std::string& type_name() const noexcept { return type_name_; }
    // The offending source literal.
    const std::string& literal() const noexcept { return literal_; }

private:
    ValueConversionError kind_;
    std::string type_name_;
    std::string literal_;
};

} // namespace vaml
