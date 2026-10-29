#pragma once
// VAML typeless Value — tag + tagged union storage + type-safe get().
// S030 (Sprint 1, first property-system feature). Foundation for the DP
// property system (S032+) and the value-converter registry (S089).
//
// Storage is a hand-rolled tagged union (NOT std::variant):
//   - the ValueType tag maps 1:1 to the roadmap discriminator list,
//   - the payload layout is stable and explicit (a value a real property
//     system must hold), and
//   - std::string lifetime is managed explicitly (copy/move/destroy).
//
// get<T>() is type-safe: it returns the payload as T and throws
// vaml::ValueTypeMismatch when T is not a valid payload type for the stored
// tag. Hot-path tag check is inline; the throw path is out-of-line in Value.cpp.

#include "vaml/core/Color.h"
#include "vaml/core/GridLength.h"
#include "vaml/core/Thickness.h"
#include "vaml/core/Transform.h"
#include "vaml/core/export.h"
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>

namespace vaml {

// Discriminator tags — values follow the roadmap order exactly.
// Underlying type is uint8_t (12 tags, room to grow). Matrix2D is APPENDED
// (S208 additive — enum-stable: existing tags keep their ordinal, so S030's
// stored/round-tripped values and the S034 value-path DPs are unaffected).
enum class ValueType : std::uint8_t
{
    Null = 0,
    Bool,
    Int32,
    Int64,
    Float,
    Double,
    String,
    Color,
    Thickness,
    BrushId,
    UntypedPtr,
    Matrix2D,   // S208: the REAL per-element affine transform (RenderTransform)
    GridLength, // 11k-3: the REAL GridLength value (WPF GridLengthConverter-faithful)
};

// Thrown by Value::get<T>() when T is not a valid payload type for the tag.
class ValueTypeMismatch : public std::logic_error
{
public:
    explicit ValueTypeMismatch(const char* what) : std::logic_error(what) {}
};

// Maps a C++ payload type to the set of ValueType tags it may be read from.
// Most payloads map to exactly one tag. Two are intentionally shared by two
// tags because the underlying storage is identical:
//   - std::int32_t is the payload of BOTH Int32 and BrushId (BrushId is a
//     tagged scalar id with no dedicated C++ type in S030 — thin by design,
//     no registry contract). get<int32_t> accepts either tag.
//   - void* is the payload of UntypedPtr only.
template <typename T>
struct ValueTypeOf;

template <typename T>
struct ValueTypeOf
{
    // A given type may be readable from one or more tags.
    static constexpr bool accepts(ValueType) noexcept { return false; }
};

template <> struct ValueTypeOf<bool>
{ static constexpr bool accepts(ValueType t) noexcept { return t == ValueType::Bool; } };
template <> struct ValueTypeOf<std::int32_t>
{ static constexpr bool accepts(ValueType t) noexcept { return t == ValueType::Int32 || t == ValueType::BrushId; } };
template <> struct ValueTypeOf<std::int64_t>
{ static constexpr bool accepts(ValueType t) noexcept { return t == ValueType::Int64; } };
template <> struct ValueTypeOf<float>
{ static constexpr bool accepts(ValueType t) noexcept { return t == ValueType::Float; } };
template <> struct ValueTypeOf<double>
{ static constexpr bool accepts(ValueType t) noexcept { return t == ValueType::Double; } };
template <> struct ValueTypeOf<std::string>
{ static constexpr bool accepts(ValueType t) noexcept { return t == ValueType::String; } };
template <> struct ValueTypeOf<vaml::Color>
{ static constexpr bool accepts(ValueType t) noexcept { return t == ValueType::Color; } };
template <> struct ValueTypeOf<vaml::Thickness>
{ static constexpr bool accepts(ValueType t) noexcept { return t == ValueType::Thickness; } };
template <> struct ValueTypeOf<vaml::GridLength>
{ static constexpr bool accepts(ValueType t) noexcept { return t == ValueType::GridLength; } };
template <> struct ValueTypeOf<vaml::Matrix2D>
{ static constexpr bool accepts(ValueType t) noexcept { return t == ValueType::Matrix2D; } };
template <> struct ValueTypeOf<void*>
{ static constexpr bool accepts(ValueType t) noexcept { return t == ValueType::UntypedPtr; } };

class VAML_CORE_EXPORT Value
{
public:
    // --- Factories -----------------------------------------------------
    [[nodiscard]] static Value Null() noexcept { return Value(); }
    [[nodiscard]] static Value Bool(bool v) noexcept { return Value(ValueType::Bool, v); }
    [[nodiscard]] static Value Int32(std::int32_t v) noexcept { return Value(ValueType::Int32, v); }
    [[nodiscard]] static Value Int64(std::int64_t v) noexcept { return Value(ValueType::Int64, v); }
    [[nodiscard]] static Value Float(float v) noexcept { return Value(ValueType::Float, v); }
    [[nodiscard]] static Value Double(double v) noexcept { return Value(ValueType::Double, v); }
    [[nodiscard]] static Value String(std::string v) { return Value(ValueType::String, std::move(v)); }
    [[nodiscard]] static Value Color(vaml::Color v) noexcept { return Value(ValueType::Color, v); }
    [[nodiscard]] static Value Thickness(vaml::Thickness v) noexcept { return Value(ValueType::Thickness, v); }
    [[nodiscard]] static Value GridLength(vaml::GridLength v) noexcept { return Value(ValueType::GridLength, v); }
    [[nodiscard]] static Value Matrix2D(vaml::Matrix2D v) noexcept { return Value(ValueType::Matrix2D, v); }
    // BrushId stores a scalar id (int32 payload) under a dedicated tag. No
    // named registry exists yet (S9 resources / S200+ renderer give ids
    // meaning); S030 stores the id and lets callers retrieve it via
    // get<std::int32_t> — which accepts the BrushId tag.
    [[nodiscard]] static Value BrushId(std::int32_t v) noexcept { return Value(ValueType::BrushId, v); }
    [[nodiscard]] static Value UntypedPtr(void* v) noexcept { return Value(ValueType::UntypedPtr, v); }

    // --- Introspection -------------------------------------------------
    [[nodiscard]] ValueType type() const noexcept { return type_; }
    [[nodiscard]] bool isNull() const noexcept { return type_ == ValueType::Null; }

    // --- Type-safe accessor --------------------------------------------
    // Returns a copy of the stored payload (std::string by copy). Throws
    // ValueTypeMismatch when T is not a valid payload type for the stored
    // tag. The tag check is inline (hot path); the throw path is out-of-line.
    template <typename T>
    [[nodiscard]] T get() const
    {
        if (ValueTypeOf<T>::accepts(type_))
        {
            return read<T>();
        }
        throw_mismatch(type_);
    }

    // --- Equality -------------------------------------------------------
    // Same tag + equal payload (string/color/thickness compared by value).
    [[nodiscard]] bool operator==(const Value& other) const noexcept;

    // --- Lifetime management (std::string member) -----------------------
    ~Value();
    Value(const Value&);
    Value& operator=(const Value&);
    Value(Value&& other) noexcept;
    Value& operator=(Value&& other) noexcept;

private:
    // Data members first so the nested Storage type is visible to the
    // member-function declarations below (member declarations look up names
    // only among members declared before them).

    ValueType type_ = ValueType::Null;

    union Storage
    {
        Storage() noexcept {}
        ~Storage() noexcept {}

        bool            asBool;
        std::int32_t    asInt32;
        std::int64_t    asInt64;
        float           asFloat;
        double          asDouble;
        std::string     asString;
        vaml::Color     asColor;
        vaml::Thickness asThickness;
        vaml::GridLength asGridLength; // 11k-3 GridLength payload
        vaml::Matrix2D  asMatrix2D; // S208 RenderTransform
        void*           asPtr; // UntypedPtr
    } storage_;

    // Out-of-line throw (defined in Value.cpp).
    [[noreturn]] void throw_mismatch(ValueType actual) const;

    // Reconstructs a copy of src's active union member into dst (which must
    // be freshly constructed). Defined in Value.cpp.
    static void copy_union(Storage& dst, const Storage& src, ValueType t);

    // Union read-by-type (inline). Precondition: ValueTypeOf<T>::accepts(type_).
    // NOT noexcept: read<std::string> returns by copy and may allocate/throw.
    // (Scalar specializations below re-add noexcept individually.)
    template <typename T>
    [[nodiscard]] T read() const;

    Value() noexcept = default;

    // Placement-new into the union; String forwards/moves its std::string.
    // NOT noexcept: construction of a std::string payload may allocate and
    // throw. (Scalar factories stay noexcept — their payloads are trivially
    // constructible, so no throw can actually escape; the noexcept there is
    // accurate.)
    template <typename T>
    Value(ValueType t, T&& payload) : type_(t)
    {
        new (&storage_) std::decay_t<T>(std::forward<T>(payload));
    }
};

// --- read<T>() definitions (inline, after the union is visible) ---------
// Exception spec deliberately omits noexcept on every specialization so it
// matches the primary template declaration (which must permit throw for
// read<std::string>); Clang rejects a mismatched exception spec between a
// template and its explicit specialization.

template <>
[[nodiscard]] inline bool Value::read<bool>() const { return storage_.asBool; }

template <>
[[nodiscard]] inline std::int32_t Value::read<std::int32_t>() const { return storage_.asInt32; }

template <>
[[nodiscard]] inline std::int64_t Value::read<std::int64_t>() const { return storage_.asInt64; }

template <>
[[nodiscard]] inline float Value::read<float>() const { return storage_.asFloat; }

template <>
[[nodiscard]] inline double Value::read<double>() const { return storage_.asDouble; }

template <>
[[nodiscard]] inline std::string Value::read<std::string>() const { return storage_.asString; }

template <>
[[nodiscard]] inline vaml::Color Value::read<vaml::Color>() const { return storage_.asColor; }

template <>
[[nodiscard]] inline vaml::Thickness Value::read<vaml::Thickness>() const { return storage_.asThickness; }

template <>
[[nodiscard]] inline vaml::GridLength Value::read<vaml::GridLength>() const { return storage_.asGridLength; }

template <>
[[nodiscard]] inline vaml::Matrix2D Value::read<vaml::Matrix2D>() const { return storage_.asMatrix2D; }

template <>
[[nodiscard]] inline void* Value::read<void*>() const { return storage_.asPtr; }

} // namespace vaml
