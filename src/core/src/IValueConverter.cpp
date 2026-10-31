// S501 (dep:S500) — IValueConverter definitions + the adapter into the generic
// S242/S243/S244 binding seam.

#include "vaml/core/IValueConverter.h"

#include "vaml/core/BindingExpression.h" // BindingConverter / BindingConverterFn

#include <memory>
#include <string>
#include <utility>

namespace vaml {

BindingConversion IValueConverter::Convert(const Value& value) const
{
    // Default: the identity converter — pass the resolved value through
    // unchanged (a converter subclass with no Convert works as passthrough).
    return BindingConversion{/*ok=*/true, value, {}};
}

BindingConversion IValueConverter::ConvertBack(const Value& value) const
{
    // DEFAULT = not supported. A binding whose converter cannot write back
    // surfaces this as WriteBackRejected (S244) — never a silent no-op, never
    // a throw (ADR-016; the WPF-not-supported convention).
    (void)value;
    return BindingConversion{
        /*ok=*/false, Value::Null(),
        "ConvertBack not supported by this converter"};
}

BindingConverter ToBindingConverter(std::shared_ptr<const IValueConverter> converter)
{
    // Adapter: an `IValueConverter` -> the generic `BindingConverter` the
    // binding engine consumes. Each arm captures the converter by value (a
    // shared_ptr copy) so the binding holds a stable, owned converter for the
    // life of the expression.
    BindingConverter bridge;
    bridge.convert = [converter](const Value& value) -> BindingConversion {
        if (!converter)
        {
            return BindingConversion{/*ok=*/false, Value::Null(),
                                     "null IValueConverter"};
        }
        return converter->Convert(value);
    };
    bridge.convertBack = [converter](const Value& value) -> BindingConversion {
        if (!converter)
        {
            return BindingConversion{/*ok=*/false, Value::Null(),
                                     "null IValueConverter"};
        }
        return converter->ConvertBack(value);
    };
    return bridge;
}

} // namespace vaml
