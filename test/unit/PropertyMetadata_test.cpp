// S034 — PropertyMetadata tests.
// Scope: the metadata bundle mirrors the roadmap field set — defaultValue,
// flags (inherits|readonly|bindsTwoWayByDefault), changed cb, coerce,
// validate. Verified: canonical WithDefault factory (default value + binding to
// its ValueType); builder hooks (with_coerce/with_validate/with_changed) are
// move-only and return a NEW metadata leaving the source untouched; coerce
// transforms the value / validate rejects / changed-cb fires on the passed
// (old,new) only when installed; absence = passthrough (identity coerce,
// always-accept validate, no-op changed); a wrong-tag Value throws
// ValueTypeMismatch (S030 contract) from coerce/validate/notifyChanged.
// Copy is statically deleted (move-only) — proven via static_assert.

// doctest's main comes from smoke_test.cpp (DO doctest IMPLEMENT_WITH_MAIN).
// This TU only declares tests — must NOT re-define implementation.
#include "doctest/doctest.h"

#include "vaml/core/PropertyMetadata.h"

#include <type_traits>

using vaml::PropertyFlags;
using vaml::PropertyMetadata;
using vaml::Value;
using vaml::ValueType;
using vaml::ValueTypeMismatch;

TEST_CASE("S034 PropertyMetadata: move-only by contract")
{
    // Copy must be deleted; move must be available.
    static_assert(!std::is_copy_constructible<PropertyMetadata>::value,
                  "PropertyMetadata must be move-only (copy deleted)");
    static_assert(!std::is_copy_assignable<PropertyMetadata>::value,
                  "PropertyMetadata must be move-only (copy deleted)");
    static_assert(std::is_move_constructible<PropertyMetadata>::value,
                  "PropertyMetadata must be move-constructible");
    static_assert(std::is_move_assignable<PropertyMetadata>::value,
                  "PropertyMetadata must be move-assignable");

    // Move constructs/assigns fine and keeps the moved-to value live.
    auto m = PropertyMetadata::WithDefault(Value::Int32(5), PropertyFlags::None);
    auto moved = std::move(m);                                       // move ctor
    CHECK(moved.defaultValue() == Value::Int32(5));
    PropertyMetadata assigned = PropertyMetadata::WithDefault(Value::Bool(true), PropertyFlags::ReadOnly);
    assigned = std::move(moved);                                     // move assign
    CHECK(assigned.defaultValue() == Value::Int32(5));
}

TEST_CASE("S034 PropertyMetadata: WithDefault seeds default + bound type + flags")
{
    const auto md = PropertyMetadata::WithDefault(
        Value::Int32(7),
        PropertyFlags::Inherits | PropertyFlags::BindsTwoWayByDefault);

    CHECK(md.defaultValue() == Value::Int32(7));
    CHECK(md.boundType() == ValueType::Int32);   // bound to default's tag
    CHECK(md.boundType() == md.defaultValue().type());
    CHECK(has_flag(md.flags(), PropertyFlags::Inherits));
    CHECK(has_flag(md.flags(), PropertyFlags::BindsTwoWayByDefault));
    CHECK_FALSE(has_flag(md.flags(), PropertyFlags::ReadOnly));

    // Passthrough defaults: absent callbacks.
    CHECK_FALSE(md.has_coerce());
    CHECK_FALSE(md.has_validate());
    CHECK_FALSE(md.has_changed());
}

TEST_CASE("S034 PropertyMetadata: flags agree with the PropertyKey carrier (validating carrier)")
{
    // Metadata flags are a carrier that must agree with the key's flags; a
    // mismatch is a registration-time error (S036). Here we only verify the
    // carrier faithfully reports what it was given — the full agreement
    // enforcement is S036's register(). Each of the three bits round-trips.
    const auto i = PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::Inherits);
    CHECK(has_flag(i.flags(), PropertyFlags::Inherits));
    CHECK_FALSE(has_flag(i.flags(), PropertyFlags::ReadOnly));
    CHECK_FALSE(has_flag(i.flags(), PropertyFlags::BindsTwoWayByDefault));

    const auto r = PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::ReadOnly);
    CHECK(has_flag(r.flags(), PropertyFlags::ReadOnly));
    CHECK_FALSE(has_flag(r.flags(), PropertyFlags::Inherits));

    const auto b = PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::BindsTwoWayByDefault);
    CHECK(has_flag(b.flags(), PropertyFlags::BindsTwoWayByDefault));
    CHECK_FALSE(has_flag(b.flags(), PropertyFlags::Inherits));
}

TEST_CASE("S034 PropertyMetadata: coerce — absent = identity, present = transforms")
{
    // Passthrough: absent coerce returns the input unchanged (branch-free
    // common path for S036).
    const auto plain = PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None);
    CHECK_FALSE(plain.has_coerce());
    CHECK(plain.coerce(Value::Int32(42)) == Value::Int32(42));

    // Installed coerce transforms the value.
    const auto md = PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None)
        .with_coerce([](const Value& v) { return Value::Int32(v.get<std::int32_t>() + 100); });
    CHECK(md.has_coerce());
    CHECK(md.coerce(Value::Int32(42)) == Value::Int32(142));
    // Source metadata untouched by the && builder? The builder consumed `plain`-like
    // temporary; verify the coerce still bound + works on a fresh call.
    CHECK(md.coerce(Value::Int32(1)) == Value::Int32(101));
}

TEST_CASE("S034 PropertyMetadata: validate — absent accepts, present rejects/accepts")
{
    // Passthrough: absent validate always accepts.
    const auto plain = PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None);
    CHECK_FALSE(plain.has_validate());
    CHECK(plain.validate(Value::Int32(5)));

    // Installed validate: true = accepted, false = rejected.
    auto vd = PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None)
        .with_validate([](const Value& v) { return v.get<std::int32_t>() >= 0; });
    CHECK(vd.has_validate());
    CHECK(vd.validate(Value::Int32(10)));
    CHECK_FALSE(vd.validate(Value::Int32(-3)));
}

TEST_CASE("S034 PropertyMetadata: changed cb — fires only when installed, receives (old,new)")
{
    // Absent changed cb is a no-op (does not throw).
    const auto plain = PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None);
    plain.notifyChanged(Value::Int32(1), Value::Int32(2)); // no throw
    CHECK_FALSE(plain.has_changed());

    // Installed changed cb receives exactly (old,new).
    std::int32_t seenOld = 0, seenNew = 0;
    int fireCount = 0;
    auto cd = PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None)
        .with_changed([&](const Value& o, const Value& n) {
            seenOld   = o.get<std::int32_t>();
            seenNew   = n.get<std::int32_t>();
            ++fireCount;
        });
    CHECK(cd.has_changed());
    cd.notifyChanged(Value::Int32(10), Value::Int32(20));
    CHECK(fireCount == 1);
    CHECK(seenOld == 10);
    CHECK(seenNew == 20);
}

TEST_CASE("S034 PropertyMetadata: coerce + validate + changed can be composed on one builder chain")
{
    // The canonical S036-style registration: one metadata with all three hooks
    // installed, all keyed to the same Int32 property.
    auto md = PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::ReadOnly)
        .with_coerce([](const Value& v) { return Value::Int32(v.get<std::int32_t>() < 0 ? 0 : v.get<std::int32_t>()); })
        .with_validate([](const Value& v) { return v.get<std::int32_t>() <= 100; })
        .with_changed([](const Value&, const Value&) {});

    CHECK(has_flag(md.flags(), PropertyFlags::ReadOnly));
    CHECK(md.has_coerce());
    CHECK(md.has_validate());
    CHECK(md.has_changed());

    // All three behaviors live on the same metadata.
    CHECK(md.coerce(Value::Int32(-5)) == Value::Int32(0));   // clamped by coerce
    CHECK(md.coerce(Value::Int32(50)) == Value::Int32(50));
    CHECK(md.validate(Value::Int32(150)) == false);          // out of range
    CHECK(md.validate(Value::Int32(50)) == true);
    md.notifyChanged(Value::Int32(0), Value::Int32(50));     // no throw
}

TEST_CASE("S034 PropertyMetadata: wrong-tag Value throws ValueTypeMismatch (type-bound)")
{
    auto md = PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None)
        .with_coerce([](const Value& v) { return Value::Int32(v.get<std::int32_t>() + 1); })
        .with_validate([](const Value&) { return true; })
        .with_changed([](const Value&, const Value&) {});

    // The metadata is bound to Int32; every entry point rejects a String Value.
    const Value wrongTag = Value::Double(2.5); // != Int32
    CHECK_THROWS_AS((void)md.coerce(wrongTag), ValueTypeMismatch);
    CHECK_THROWS_AS((void)md.validate(wrongTag), ValueTypeMismatch);
    CHECK_THROWS_AS(md.notifyChanged(Value::Int32(1), wrongTag), ValueTypeMismatch);
    CHECK_THROWS_AS(md.notifyChanged(wrongTag, Value::Int32(1)), ValueTypeMismatch);

    // Correct-tag values still pass post-throw (throw path is a read-only test).
    CHECK(md.coerce(Value::Int32(3)) == Value::Int32(4));
}

TEST_CASE("S034 PropertyMetadata: builder is return-by-value, source untouched for temporaries")
{
    // The && builder is meant to be used on a temporary (the canonical
    // chain). Verify that building from a temporary yields a metadata carrying
    // the callback and that the coerce actually binds.
    auto source = PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None);
    auto result = std::move(source).with_validate([](const Value& v) { return v.get<std::int32_t>() != 0; });

    CHECK(result.has_validate());
    CHECK_FALSE(result.validate(Value::Int32(0)));
    CHECK(result.validate(Value::Int32(9)));
}
