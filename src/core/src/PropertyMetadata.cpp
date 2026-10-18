// S034 — PropertyMetadata out-of-line definitions. The three callables are
// std::function under a pimpl (Impl) so the public header never exposes the
// ABI of std::function and the metadata stays move-only by deleting copy at
// the public boundary.
//
// S146 (refactor gate) — the pimpl is COLLAPSED: the three callables + default
// + flags are now plain by-value members of PropertyMetadata (see
// PropertyMetadata.h). No Impl heap node, no impl_-> trampoline, no per-object
// heap hop; the metadata stays move-only (copy deleted at the public boundary).

#include "vaml/core/PropertyMetadata.h"

#include <utility>

namespace vaml {

VAML_CORE_EXPORT
PropertyMetadata PropertyMetadata::WithDefault(Value defaultValue, PropertyFlags flags)
{
    return PropertyMetadata(std::move(defaultValue), flags);
}

VAML_CORE_EXPORT
PropertyMetadata PropertyMetadata::with_coerce(CoerceFunc f) &&
{
    coerce_ = std::move(f);
    // Return a NEW metadata carrying ALL accumulated state (default, flags, and
    // any callbacks previously installed via an earlier builder hook).
    PropertyMetadata out(std::move(defaultValue_), flags_);
    out.coerce_    = std::move(coerce_);
    out.validate_  = std::move(validate_);
    out.changed_   = std::move(changed_);
    return out;
}

VAML_CORE_EXPORT
PropertyMetadata PropertyMetadata::with_validate(ValidateFunc f) &&
{
    validate_ = std::move(f);
    PropertyMetadata out(std::move(defaultValue_), flags_);
    out.coerce_    = std::move(coerce_);
    out.validate_  = std::move(validate_);
    out.changed_   = std::move(changed_);
    return out;
}

VAML_CORE_EXPORT
PropertyMetadata PropertyMetadata::with_changed(ChangedFunc f) &&
{
    changed_ = std::move(f);
    PropertyMetadata out(std::move(defaultValue_), flags_);
    out.coerce_    = std::move(coerce_);
    out.validate_  = std::move(validate_);
    out.changed_   = std::move(changed_);
    return out;
}

VAML_CORE_EXPORT
const Value& PropertyMetadata::defaultValue() const noexcept { return defaultValue_; }

VAML_CORE_EXPORT
PropertyFlags PropertyMetadata::flags() const noexcept { return flags_; }

VAML_CORE_EXPORT
ValueType PropertyMetadata::boundType() const noexcept { return defaultValue_.type(); }

VAML_CORE_EXPORT
bool PropertyMetadata::has_coerce() const noexcept { return static_cast<bool>(coerce_); }

VAML_CORE_EXPORT
bool PropertyMetadata::has_validate() const noexcept { return static_cast<bool>(validate_); }

VAML_CORE_EXPORT
bool PropertyMetadata::has_changed() const noexcept { return static_cast<bool>(changed_); }

VAML_CORE_EXPORT
Value PropertyMetadata::coerce(const Value& v) const
{
    ensureTag(v.type());
    if (coerce_)
    {
        return coerce_(v);
    }
    return v;
}

VAML_CORE_EXPORT
bool PropertyMetadata::validate(const Value& v) const
{
    ensureTag(v.type());
    if (validate_)
    {
        return validate_(v);
    }
    return true;
}

VAML_CORE_EXPORT
void PropertyMetadata::notifyChanged(const Value& oldValue, const Value& newValue) const
{
    ensureTag(oldValue.type());
    ensureTag(newValue.type());
    if (changed_)
    {
        changed_(oldValue, newValue);
    }
}

VAML_CORE_EXPORT
PropertyMetadata::PropertyMetadata(PropertyMetadata&&) noexcept = default;

VAML_CORE_EXPORT
PropertyMetadata& PropertyMetadata::operator=(PropertyMetadata&&) noexcept = default;

VAML_CORE_EXPORT
PropertyMetadata::~PropertyMetadata() = default;

VAML_CORE_EXPORT
PropertyMetadata::PropertyMetadata(Value defaultValue, PropertyFlags flags)
    : defaultValue_(std::move(defaultValue))
    , flags_(flags)
{
}

// Self-check helper: the callback is keyed to defaultValue_.type().
VAML_CORE_EXPORT
void PropertyMetadata::ensureTag(ValueType actual) const
{
    if (actual != defaultValue_.type())
    {
        throw ValueTypeMismatch("PropertyMetadata: value tag does not match the property's bound value type");
    }
}

} // namespace vaml
