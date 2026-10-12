// S034 — PropertyMetadata out-of-line definitions. The three callables are
// std::function under a pimpl (Impl) so the public header never exposes the
// ABI of std::function and the metadata stays move-only by deleting copy at
// the public boundary.

#include "vaml/core/PropertyMetadata.h"

#include <utility>

namespace vaml {

struct PropertyMetadata::Impl
{
    Impl(Value def, PropertyFlags fl)
        : defaultValue_(std::move(def))
        , flags_(fl)
    {
    }

    // Self-check helper: the callback is keyed to defaultValue_.type().
    void ensureTag(ValueType actual) const
    {
        if (actual != defaultValue_.type())
        {
            throw ValueTypeMismatch("PropertyMetadata: value tag does not match the property's bound value type");
        }
    }

    Value            defaultValue_;
    PropertyFlags    flags_        = PropertyFlags::None;
    CoerceFunc       coerce_;
    ValidateFunc     validate_;
    ChangedFunc      changed_;
};

VAML_CORE_EXPORT
PropertyMetadata PropertyMetadata::WithDefault(Value defaultValue, PropertyFlags flags)
{
    return PropertyMetadata(std::make_unique<Impl>(std::move(defaultValue), flags));
}

VAML_CORE_EXPORT
PropertyMetadata PropertyMetadata::with_coerce(CoerceFunc f) &&
{
    impl_->coerce_ = std::move(f);
    return PropertyMetadata(std::move(impl_));
}

VAML_CORE_EXPORT
PropertyMetadata PropertyMetadata::with_validate(ValidateFunc f) &&
{
    impl_->validate_ = std::move(f);
    return PropertyMetadata(std::move(impl_));
}

VAML_CORE_EXPORT
PropertyMetadata PropertyMetadata::with_changed(ChangedFunc f) &&
{
    impl_->changed_ = std::move(f);
    return PropertyMetadata(std::move(impl_));
}

VAML_CORE_EXPORT
const Value& PropertyMetadata::defaultValue() const noexcept { return impl_->defaultValue_; }

VAML_CORE_EXPORT
PropertyFlags PropertyMetadata::flags() const noexcept { return impl_->flags_; }

VAML_CORE_EXPORT
ValueType PropertyMetadata::boundType() const noexcept { return impl_->defaultValue_.type(); }

VAML_CORE_EXPORT
bool PropertyMetadata::has_coerce() const noexcept { return static_cast<bool>(impl_->coerce_); }

VAML_CORE_EXPORT
bool PropertyMetadata::has_validate() const noexcept { return static_cast<bool>(impl_->validate_); }

VAML_CORE_EXPORT
bool PropertyMetadata::has_changed() const noexcept { return static_cast<bool>(impl_->changed_); }

VAML_CORE_EXPORT
Value PropertyMetadata::coerce(const Value& v) const
{
    impl_->ensureTag(v.type());
    if (impl_->coerce_)
    {
        return impl_->coerce_(v);
    }
    return v;
}

VAML_CORE_EXPORT
bool PropertyMetadata::validate(const Value& v) const
{
    impl_->ensureTag(v.type());
    if (impl_->validate_)
    {
        return impl_->validate_(v);
    }
    return true;
}

VAML_CORE_EXPORT
void PropertyMetadata::notifyChanged(const Value& oldValue, const Value& newValue) const
{
    impl_->ensureTag(oldValue.type());
    impl_->ensureTag(newValue.type());
    if (impl_->changed_)
    {
        impl_->changed_(oldValue, newValue);
    }
}

VAML_CORE_EXPORT
PropertyMetadata::PropertyMetadata(PropertyMetadata&&) noexcept = default;

VAML_CORE_EXPORT
PropertyMetadata& PropertyMetadata::operator=(PropertyMetadata&&) noexcept = default;

VAML_CORE_EXPORT
PropertyMetadata::~PropertyMetadata() = default;

VAML_CORE_EXPORT
PropertyMetadata::PropertyMetadata(std::unique_ptr<Impl> impl)
    : impl_(std::move(impl))
{
}

} // namespace vaml
