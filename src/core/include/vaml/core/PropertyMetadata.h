#pragma once
// VAML PropertyMetadata — the per-registered-property metadata bundle.
// S034 (dep:S033). Consumed by the future DependencyProperty::Register (S036).
//
// Mirrors the roadmap's S034 field list exactly:
//   defaultValue            : Value        — applied when no local/inherited value resolves (S039)
//   flags                   : PropertyFlags — recomposes S032's set
//                            (inherits | readonly | bindsTwoWayByDefault)
//   changed cb              : (old, new)   — invoked on a real value change (S035)
//   coerce                  : Value(Value) — returns the coerced value (S035 "coerce applied")
//   validate                : Value(Value) -> bool — true accepted / false rejected (S035, S045)
//
// Design choices (recorded for forward steps):
//   - MUTABLE-AT-BUILD, IMMUTABLE-AFTER. Construction goes through the
//     canonical factory WithDefault(...) (required — there is NO default-
//     constructible empty metadata that could be silently mis-read), then
//     optional move-only builder hooks (with_coerce / with_validate /
//     with_changed) that return a NEW metadata by value. The original is
//     left untouched (return-by-value, &&-qualified to force move semantics).
//   - MOVE-ONLY (copy deleted). The callbacks are std::function under a pimpl;
//     a dependency property's metadata is installed into a registry/dictionary
//     once and never copied, so move-only is both correct and sufficient.
//   - PASSTHROUGH DEFAULT: an absent coerce is the identity, an absent
//     validate always accepts, an absent changed cb is a no-op. This keeps
//     S036's effective-value resolution branch-free for common (callback-less)
//     properties.
//   - TYPE-BOUND: the metadata's property value type is DEFAULT_TYPE =
//     defaultValue.type(). coerce/validate/notifyChanged are keyed to that
//     type: passing a Value whose tag != DEFAULT_TYPE throws vaml::ValueTypeMismatch
//     (the S030 contract) instead of silently operating on the wrong payload —
//     NO-LAZY guard against a callback receiving a wrong-type Value.
//
// S034 adds NO static registration surface. The global static registry
// instance + DependencyProperty::Register landing site is S036's concern
// (recorded, not silent).

#include "vaml/core/PropertyFlags.h"
#include "vaml/core/Value.h" // Value, ValueType, ValueTypeMismatch

#include <functional>

namespace vaml {

class VAML_CORE_EXPORT PropertyMetadata
{
public:
    // Callback signatures (any may be absent = empty std::function).
    using CoerceFunc   = std::function<Value(const Value&)>;        // input -> coerced
    using ValidateFunc = std::function<bool(const Value&)>;         // true accepted / false rejected
    using ChangedFunc  = std::function<void(const Value&, const Value&)>; // (old, new)

    // --- Canonical factory (required; no default-constructible metadata) --
    // `defaultValue` both seeds S039's fallback AND fixes the metadata's
    // bound value type (defaultValue.type()). `flags` recomposes S032's
    // set and must agree with the PropertyKey it is registered against (it
    // is a validating carrier, never a competing source of truth).
    [[nodiscard]] static PropertyMetadata WithDefault(Value defaultValue, PropertyFlags flags);

    // --- Move-only builder hooks (&&-qualified: force move, never copy) ---
    // Each returns a NEW metadata with the given callback installed, leaving
    // `this` untouched. No-op-install semantics: passing an empty callback is
    // legal and yields the same passthrough behavior as omitting it.
    [[nodiscard]] PropertyMetadata with_coerce(CoerceFunc) &&;
    [[nodiscard]] PropertyMetadata with_validate(ValidateFunc) &&;
    [[nodiscard]] PropertyMetadata with_changed(ChangedFunc) &&;

    // --- Accessors (immutable) ------------------------------------------
    [[nodiscard]] const Value& defaultValue() const noexcept;
    [[nodiscard]] PropertyFlags flags() const noexcept;
    [[nodiscard]] ValueType boundType() const noexcept; // alias of defaultValue().type()
    [[nodiscard]] bool has_coerce() const noexcept;
    [[nodiscard]] bool has_validate() const noexcept;
    [[nodiscard]] bool has_changed() const noexcept;

    // --- Behavior (defined below; all throw ValueTypeMismatch on wrong tag) --
    // coerce(v): invokes the stored coerce if present, else returns v unchanged.
    [[nodiscard]] Value coerce(const Value& v) const;
    // validate(v): true when the stored validate accepts; true when absent.
    [[nodiscard]] bool validate(const Value& v) const;
    // notifyChanged(old, new): invokes the changed cb if present, else no-op.
    void notifyChanged(const Value& oldValue, const Value& newValue) const;

    // --- Move-only (private ctor used by the factories; copy deleted) --------
    PropertyMetadata(PropertyMetadata&&) noexcept;
    PropertyMetadata& operator=(PropertyMetadata&&) noexcept;
    PropertyMetadata(const PropertyMetadata&) = delete;
    PropertyMetadata& operator=(const PropertyMetadata&) = delete;
    ~PropertyMetadata();

private:
    // S146 (refactor gate): the pimpl is COLLAPSED — the callbacks + default +
    // flags are now plain by-value members (no Impl heap node, no impl_->
    // trampoline, no per-object heap hop). The ctor below is used by the
    // factories (WithDefault + the &&-builder hooks); copy is deleted so the
    // metadata stays move-only (the callbacks are std::function members).
    PropertyMetadata(Value defaultValue, PropertyFlags flags);

    void ensureTag(ValueType actual) const; // callback keyed to defaultValue_.type()

    Value            defaultValue_;
    PropertyFlags    flags_        = PropertyFlags::None;
    CoerceFunc       coerce_;
    ValidateFunc     validate_;
    ChangedFunc      changed_;
};

} // namespace vaml
