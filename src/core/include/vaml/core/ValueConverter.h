#pragma once
// S089 (dep:S088) — Value converter registry.
//
// Maps a TARGET TYPE NAME -> a converter std::function that turns a source
// string literal into a vaml::Value of that type's payload. This is the
// converter-domain step the S084 attribute-value grammar handed off to (S084
// DETECTS scalar numeric; S089 CONVERTS + owns the comma-list thickness form):
//   "42"      -> Value::Int32(42)
//   "10,20"   -> Value::Thickness(10,20,10,20)   (comma-list, S089 domain)
//   "#FF0000" -> Value::Color(255,0,0,255)
//   "true"    -> Value::Bool(true)
//
// DESIGN (mirrors the S032/S087 registry discipline):
//   - NAME-UNIQUE registration, enforcing dedupe. register_converter() throws
//     ValueAlreadyRegistered on a name collision; the registry is unchanged by
//     a rejected insert (the S032 discipline).
//   - The registry stores an OWNED copy of each registered name (same
//     SSO/dangle discipline as the property/element systems — callers may pass
//     a transient std::string_view; the map keeps a stable std::string).
//   - convert() is the resolving entry point: an UNREGISTERED type name ->
//     ValueConversionException{UnknownType} naming the type; a registered
//     converter that rejects its literal (e.g. a malformed Color/Thickness/
//     Int/Bool) throws ValueConversionException{MalformedValue} naming the
//     type + literal.
//   - registered_names() returns INSERTION ORDER (deterministic — never
//     unordered_map iteration).
//   - A GLOBAL singleton (Meyers) mirrors PropertyRegistry/DependencyProperty/
//     ElementFactoryRegistry::global() and is populated once at startup by
//     register_builtin_converters(). Documented single-threaded startup.
//
// RESOLVES the S030 Color + Thickness MINIMAL REAL markers: the parsing
// semantics live on the types themselves (parse_color/parse_thickness, in
// Color.h/Thickness.h) and are registered into this registry by
// register_builtin_converters(). The sentinel tags in Color.h/Thickness.h are
// REMOVED here per the resolution contract — that resolution is part of this
// feature's own step, not a backfill (NO-GAP).
//
// SCOPE BOUNDARY (NO-GAP, not silent): S089 delivered the four built-in
// conversions + the minimal {None,UnknownType,MalformedValue} taxonomy so
// conversion behavior is REAL. The full unknown-literal error taxonomy
// (UnknownLiteral) + the exhaustive error/edge test suite are S090's named
// step — delivered there (this S089 step's scope boundary is RESOLVED by
// S090, not silently backfilled). S090 adds try_infer_type (below) so
// UnknownLiteral is reachable and provable.
//
// MINIMAL REAL? No. ValueConverterRegistry + the four converters + the resolved
// parse_color/parse_thickness are delivered complete for their declared scope
// and tested; S090 extends tests only + adds the recognizer that reaches the
// unknown-literal corner S089 scoped out.

#include "vaml/core/Value.h"               // Value (converter result)
#include "vaml/core/ValueConversionError.h" // ValueConversionError / ValueConversionException

#include <cstddef>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace vaml {

// register_converter() saw a type name that is already registered (dedupe).
class ValueAlreadyRegistered : public std::logic_error
{
public:
    explicit ValueAlreadyRegistered(const char* what) : std::logic_error(what) {}
};

// A converter turns a source string literal into a vaml::Value of a target
// type's payload. A converter rejects malformed input by throwing
// ValueConversionException with kind() == MalformedValue.
using ValueConverter = std::function<Value(std::string_view literal)>;

class VAML_CORE_EXPORT ValueConverterRegistry
{
public:
    ValueConverterRegistry();
    ~ValueConverterRegistry();

    ValueConverterRegistry(const ValueConverterRegistry&) = delete;
    ValueConverterRegistry& operator=(const ValueConverterRegistry&) = delete;
    ValueConverterRegistry(ValueConverterRegistry&&) noexcept;
    ValueConverterRegistry& operator=(ValueConverterRegistry&&) noexcept;

    // Registers `typeName` -> `converter`. Type names are unique: registering
    // an already-present name throws ValueAlreadyRegistered and leaves the
    // registry unchanged (the converter is NOT replaced). The registry stores
    // an OWNED copy of `typeName`.
    void register_converter(std::string_view typeName, ValueConverter converter);

    // Converts `literal` to a vaml::Value of `typeName`. An unregistered name
    // throws ValueConversionException{UnknownType} naming the type; a
    // registered converter that rejects the literal throws
    // ValueConversionException{MalformedValue} naming the type + literal.
    [[nodiscard]] Value convert(std::string_view typeName, std::string_view literal) const;

    // S090: the recognizer that makes the UnknownLiteral error category
    // reachable and provable. Determines whether `literal` is a value of ANY
    // registered type, by probing registered converters IN INSERTION ORDER and
    // returning the first that fully accepts the literal. On success, writes
    // that type's name into *outType (if non-null) and returns true. On
    // failure (no registered converter accepts the literal) returns false and
    // leaves *outType untouched — the caller maps that to
    // ValueConversionError::UnknownLiteral.
    //
    // CONTRACT — this is a RECOGNIZER, not a best-match type-matcher:
    //   - First-match-by-registration-order is deterministic and documented:
    //     "10" infers "Int32" when Int32 is registered before Thickness, not
    //     "Thickness" (a single scalar is a valid 1-value thickness too).
    //   - "Accepts" means: the converter runs WITHOUT throwing. A converter
    //     that throws (any ValueConversionException — MalformedValue or
    //     otherwise) is treated as "does not accept this literal".
    //   - Registered converters must be PURE for this query (no mutation /
    //     side effects when invoked); the built-ins are. This probes the
    //     registry to answer "is this a known value at all?", which is S091's
    //     documented upstream use.
    [[nodiscard]] bool try_infer_type(std::string_view literal, std::string* outType) const;

    // True if a converter is registered under `typeName`. Never throws.
    [[nodiscard]] bool is_registered(std::string_view typeName) const noexcept;

    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] bool empty() const noexcept { return size() == 0; }

    // All registered type names in INSERTION ORDER (deterministic — not
    // unordered_map iteration). Returns a fresh copy (owned strings).
    [[nodiscard]] std::vector<std::string> registered_names() const;

    // The process-wide startup singleton populated once by
    // register_builtin_converters(). Thread-safe construction (Meyers
    // function-local static). Documented single-threaded startup population;
    // S091 consumes it from the reader path.
    [[nodiscard]] static ValueConverterRegistry& global() noexcept;

private:
    class Impl;                 // pimpl: hides std::unordered_map + order list
    std::unique_ptr<Impl> impl_;
};

// S089: idempotent startup registration of the four built-in converters
// (Int32, Thickness, Color, Bool) into ValueConverterRegistry::global().
// Registering twice is a no-op guard against double startup population
// (mirrors register_builtin_elements()).
void VAML_CORE_EXPORT register_builtin_converters();

} // namespace vaml
