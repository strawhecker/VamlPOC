// S501 (dep:S500) — BooleanToVisibilityConverter: bool -> Visibility Int32 DP.
// See BooleanToVisibilityConverter.h for the WPF ground truth + the one
// documented fail-loud deviation (non-bool Convert FAILS, never silently
// collapses).

#include "vaml/core/BooleanToVisibilityConverter.h"

#include "vaml/core/Value.h" // Value

namespace vaml {

BindingConversion BooleanToVisibilityConverter::Convert(const Value& value) const
{
    if (value.type() != ValueType::Bool)
    {
        // WPF coerces a non-bool to false (-> Collapsed) silently. VAML fails
        // loud so the S242 ConverterFailed + fallback path is reached and the
        // author is told — never a silent collapse (ADR-016: fail-loud beats
        // silent-coerce on the diagnostic channel).
        return BindingConversion{
            /*ok=*/false, Value::Null(),
            "BooleanToVisibilityConverter.Convert requires a Bool value"};
    }
    // bool -> Visibility Int32 DP (Visible=0, Collapsed=2).
    const bool b = value.get<bool>();
    return BindingConversion{
        /*ok=*/true, Value::Int32(b ? 0 : 2), {}};
}

BindingConversion BooleanToVisibilityConverter::ConvertBack(const Value& value) const
{
    if (value.type() != ValueType::Int32)
    {
        // Only an Int32 Visibility payload can convert back (never a silent
        // guess on a wrong-typed target value).
        return BindingConversion{
            /*ok=*/false, Value::Null(),
            "BooleanToVisibilityConverter.ConvertBack requires an Int32 (Visibility)"};
    }
    // Visibility: 0 (Visible) -> true; anything else (Hidden=1, Collapsed=2)
    // -> false. WPF-faithful.
    return BindingConversion{
        /*ok=*/true, Value::Bool(value.get<std::int32_t>() == 0), {}};
}

} // namespace vaml
