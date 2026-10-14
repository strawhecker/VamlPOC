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
// S089 delivered the {None,UnknownType,MalformedValue} taxonomy; S090
// (dep:S089) EXTENDS it with UnknownLiteral — the formal unknown-literal error
// category — and documents the three-way distinction. S090 is the resolution
// point for the unknown-literal corner S089 scoped out (recorded NO-GAP, not
// silent; no marker was left behind — it is delivered here).

#include <stdexcept>
#include <string>
#include <string_view>

namespace vaml {

// S089 + S090: the converter error taxonomy — what a conversion attempt can
// report. The three error categories are ORTHOGONAL and exhaustive:
//   - UnknownType    — the requested TARGET TYPE has no registered converter
//                      ("no converter for type X"); the literal is irrelevant.
//   - MalformedValue — the literal is MALFORMED FOR a known, requested target
//                      type (e.g. "#GG0000" is not a valid Color); the type's
//                      own parser rejected it.
//   - UnknownLiteral — the LITERAL is unrecognizable as a value of ANY type
//                      that the registry can produce (e.g. "banana"); there is
//                      no target type involved (or none was ever the "right"
//                      one) — the literal itself is the problem.
enum class ValueConversionError
{
    None,          // no conversion error
    UnknownType,   // no converter registered for the requested type name
    MalformedValue,// a registered converter / parser rejected its input literal
    UnknownLiteral // the literal matches no type the registry can produce
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
