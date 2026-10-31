#pragma once
// S501 (dep:S500) — IValueConverter: the WPF-truthful BINDING value-converter
// interface. "Converters are logic, not rendering" (ADR-011).
//
// WPF GROUND TRUTH (verified against E:\GitRepos\wpf): the binding converter
// surface is the `IValueConverter` INTERFACE
// (PresentationFramework\System\Windows\Data\IValueConverter.cs) — there is NO
// `ValueConverter` base class anywhere in the WPF tree. This header is that
// interface rendered for the VAML binding seam. The app authors its OWN
// `*Converter` (incl. `NullTo*`) by implementing `IValueConverter`, exactly as
// it writes its own ViewModels and commands (the S250 app-owned composition
// pattern — no reflection, no DI).
//
// The S089 `vaml::ValueConverter` name was a past MISNOMER (it is the ATTRIBUTE-
// LITERAL / type converter, `std::function<Value(std::string_view)>`, the WPF
// type-converter analog). S501 renames that alias `LiteralConverter` so the
// binding surface owns the honest term. See ValueConverter.h.
//
// The converter seam that S242/S243/S244 consume is the generic
// `BindingConverterFn` / `BindingConverter` bridge (BindingExpression.h): a
// binding applies ONE -- convert (source -> target) and, for TwoWay, convertBack
// (target -> source). This interface is that same seam expressed as an
// overridable object so the app can author a converter CLASS and register it.
//
// ConvertBack DEFAULT (WPF-truthful): a converter that does not support the
// reverse trip returns a failed BindingConversion (ok=false) — exactly what
// WPF's own list/type converters do when ConvertBack is unsupported. A TwoWay
// binding whose converter cannot write back emits WriteBackRejected (S244) and
// skips the write-back (never a throw — ADR-016, Class B observation).
//
// ADR-018: ZERO new locks (a converter is used on one thread, its binding's
// affinity thread).

#include "vaml/core/BindingExpression.h" // BindingConversion
#include "vaml/core/export.h"

namespace vaml {

// The WPF-truthful binding value-converter interface. Implement Convert to turn
// a resolved source Value into the target DP's value; implement ConvertBack to
// support the TwoWay reverse trip (leave it DEFAULT for a one-way converter).
//
// WPF-faithful contract:
//   - Convert  : invoked on every source->target transfer when the binding
//                carries a converter. Return ok=false (or throw) to FAIL the
//                conversion — the engine emits ConverterFailed and, if a
//                fallback is bound, applies it (S242).
//   - ConvertBack : invoked on the TwoWay target->source write-back. The
//                DEFAULT returns ok=false ("ConvertBack not supported") — the
//                honest WPF-not-supported convention, which a TwoWay binding
//                surfaces as WriteBackRejected (S244), never a silent no-op.
//
// A converter's Convert typically inspects `value` by type (v.type()) and
// returns a converted Value; it NEVER mutates the input.
class VAML_CORE_EXPORT IValueConverter
{
public:
    virtual ~IValueConverter() = default;

    // Convert: value (resolved source) -> target DP value. Default: pass the
    // value through unchanged (the identity converter). Override for real
    // conversions.
    [[nodiscard]] virtual BindingConversion Convert(const Value& value) const;

    // ConvertBack: value (target DP value) -> source value. DEFAULT = not
    // supported (returns ok=false) — the WPF-not-supported convention. A
    // TwoWay binding surfaces this as WriteBackRejected, never a silent no-op.
    [[nodiscard]] virtual BindingConversion ConvertBack(const Value& value) const;
};

// --- The adapter into the generic S242/S243/S244 binding seam -----------------
// The binding engine consumes `BindingConverter` (BindingExpression.h): a
// `BindingConverterFn convert` (source -> target) + `convertBack`. This adapter
// wraps an `IValueConverter` into that generic form, capturing the interface by
// SHARED pointer so the binding can hold a stable, owned converter.
[[nodiscard]] VAML_CORE_EXPORT BindingConverter
ToBindingConverter(std::shared_ptr<const IValueConverter> converter);

} // namespace vaml
