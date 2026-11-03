// VAML BindingBase family — the binding RECIPE objects (WPF
// System.Windows.Data.BindingBase : MarkupExtension). 11n-4.
// REUSES the existing S240-S245 BindingExpression engine.

#include "vaml/core/BindingBase.h"
#include "vaml/core/DependencyObject.h"

#include <utility>

namespace vaml {

// --- BindingBase -----------------------------------------------------------
VAML_CORE_EXPORT
void BindingBase::fill_spec(BindingExpressionSpec& spec) const
{
    spec.mode        = mode_;
    spec.policy      = policy_;
    spec.converter   = converter_;
    spec.converterName = converterName_;
    spec.fallback    = fallback_;
}

// --- Binding ---------------------------------------------------------------
VAML_CORE_EXPORT
Binding::Binding()
{}

VAML_CORE_EXPORT
Binding::Binding(std::string path)
    : path_(std::move(path))
{}

VAML_CORE_EXPORT
BindingExpressionSpec Binding::make_spec() const
{
    BindingExpressionSpec spec;
    spec.source     = source_;
    spec.sourcePath = path_;
    spec.target     = target_;
    spec.targetProp = targetProp_;
    fill_spec(spec);
    return spec;
}

VAML_CORE_EXPORT
Value Binding::provide_value() const
{
    // A Binding recipe's "provided value" is the CONFIGURED binding, consumed
    // by the reader/engine via make_spec(). MarkupExtension::provide_value is
    // the generic Value carrier (used by scalar value-extensions); a Binding
    // provides its spec (the real integration point), so it returns a null
    // sentinel here — the reader paths that bind call make_spec(), never this.
    return Value::Null();
}

// --- MultiBinding ----------------------------------------------------------
VAML_CORE_EXPORT
Value MultiBinding::provide_value() const
{
    // A MultiBinding combines its child bindings' resolved values through the
    // IMultiValueConverter. This is the reader/engine integration point; the
    // generic Value carrier returns a null sentinel (the engine consumes the
    // child spec list + multi_converter directly, see MultiBinding usage at
    // the center host). Children that yield a real scalar may be combined:
    // when a multi_converter is present we synthesize the combined Value.
    if (multiConverter_ != nullptr && !bindings_.empty())
    {
        std::vector<Value> values;
        values.reserve(bindings_.size());
        for (const auto& b : bindings_)
            values.push_back(b->provide_value());
        return multiConverter_->convert(values);
    }
    return Value::Null();
}

// --- PriorityBinding -------------------------------------------------------
VAML_CORE_EXPORT
Value PriorityBinding::provide_value() const
{
    // The first candidate that yields a NON-NULL value wins (the fallback
    // ladder). Reads child provide_value + the child spec in priority order.
    for (const auto& b : bindings_)
    {
        const Value v = b->provide_value();
        if (!v.isNull())
            return v;
    }
    return Value::Null();
}

} // namespace vaml
