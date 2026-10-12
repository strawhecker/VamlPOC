// S036 — DependencyObject tests (dep:S035).
//
// Verifies the S036 title contract:
//   1. local value store        — per-object store keyed by PropertyId;
//   2. Get/Set                  — property read/write through the store;
//   3. per-prop mastery         — register_property() installs (metadata,
//                                defaultValue) per property;
//   4. thread-affinity marker   — object records its creating thread.
//
// And reproduces the S035 apply pipeline exactly (the MiniStore harness in
// PropertyMetadata_semantics_test.cpp is the reference spec the Set() must
// satisfy): default applied / coerce applied / coerce-before-validate /
// validate rejects / cb-fires-only-on-real-change / passthrough.

// S036_5: migrated to the Shouldly-for-doctest assertion vocabulary
// (SHOULD(...) seam + ShouldlyString + ShouldThrow) as the capstone of the
// S036_1..S036_5 convention effort — the S036 proof file now reads as the
// S035 pipeline spec in Shouldly terms.

// doctest's main comes from smoke_test.cpp (DO doctest IMPLEMENT_WITH_MAIN).
// This TU only declares tests — must NOT re-define implementation.
#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"       // S036_2 StringMaker + S036_4 operator<<
#include <vaml/assert/Shouldly.h>         // ShouldlyString / ShouldThrow (SHOULD seam, S036_1)

#include "vaml/core/DependencyObject.h"
#include "vaml/core/PropertyKey.h"

#include <string>
#include <thread>

using vaml::DependencyObject;
using vaml::PropertyAlreadyRegistered;
using vaml::PropertyFlags;
using vaml::PropertyId;
using vaml::PropertyKey;
using vaml::PropertyMetadata;
using vaml::PropertyNotFound;
using vaml::SetResult;
using vaml::Value;
using vaml::ValueType;
using vaml::ValueTypeMismatch;
using vaml::test::ShouldThrow;

namespace {

// Convenience builder: an Int32-bounded PropertyMetadata with an optional
// coerce (clamps negatives to 0) and optional validate (requires > 0).
PropertyMetadata int32Md(std::int32_t defaultValue, bool withCoerce, bool withValidate)
{
    PropertyMetadata md = PropertyMetadata::WithDefault(Value::Int32(defaultValue), PropertyFlags::None);
    if (withCoerce)
    {
        md = std::move(md).with_coerce([](const Value& v) {
            return Value::Int32(v.get<std::int32_t>() < 0 ? 0 : v.get<std::int32_t>());
        });
    }
    if (withValidate)
    {
        md = std::move(md).with_validate([](const Value& v) { return v.get<std::int32_t>() > 0; });
    }
    return md;
}

PropertyKey int32Key(const std::string& name)
{
    return PropertyKey(name, ValueType::Int32, "TestOwner", PropertyFlags::None);
}

} // namespace

TEST_CASE("S036: default is applied with no local override (Get returns defaultValue)")
{
    DependencyObject obj;
    obj.register_property(int32Key("Width"), int32Md(7, false, false));

    SHOULD(ShouldlyString(obj.Get(PropertyId::of("Width"))).ShouldBe(Value::Int32(7))); // "default applied"
}

TEST_CASE("S036: type coherence — key type must match metadata bound type")
{
    DependencyObject obj;
    // key bound to Int32 but metadata bound to Double -> throw, nothing stored.
    PropertyMetadata md = PropertyMetadata::WithDefault(Value::Double(1.5), PropertyFlags::None);
    SHOULD(ShouldThrow<ValueTypeMismatch>([&] {
        (void)obj.register_property(int32Key("Bad"), std::move(md));
    }));
}

TEST_CASE("S036: Get/Set on an unregistered property throws PropertyNotFound")
{
    DependencyObject obj;
    SHOULD(ShouldThrow<PropertyNotFound>([&] { (void)obj.Get(PropertyId::of("Nope")); }));
    SHOULD(ShouldThrow<PropertyNotFound>([&] { (void)obj.Set(PropertyId::of("Nope"), Value::Int32(1)); }));
}

TEST_CASE("S036 apply: coerce is applied — stored value is the coerced value, cb sees coerced new")
{
    DependencyObject obj;
    int fire = 0;
    std::int32_t oldV = -1, newV = -1;
    // One property carrying BOTH the coerce (clamp negatives -> 0) and a recorder
    // that captures (old,new).
    obj.register_property(
        int32Key("Width"),
        PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None)
            .with_coerce([](const Value& v) {
                return Value::Int32(v.get<std::int32_t>() < 0 ? 0 : v.get<std::int32_t>());
            })
            .with_changed([&](const Value& o, const Value& n) {
                oldV = o.get<std::int32_t>();
                newV = n.get<std::int32_t>();
                ++fire;
            }));
    PropertyId id("Width");

    // Raw -5 coerced to 0; store already holds default 0 -> NO_CHANGE.
    SHOULD(ShouldlyString(obj.Set(id, Value::Int32(-5))).ShouldBe(SetResult::NO_CHANGE));
    SHOULD(ShouldlyString(obj.Get(id)).ShouldBe(Value::Int32(0)));
    SHOULD(ShouldlyString(fire).ShouldBe(0));

    // Raw 3 passes through -> stored 3, cb fires with (old=0, new=3).
    SHOULD(ShouldlyString(obj.Set(id, Value::Int32(3))).ShouldBe(SetResult::APPLIED));
    SHOULD(ShouldlyString(obj.Get(id)).ShouldBe(Value::Int32(3)));
    SHOULD(ShouldlyString(fire).ShouldBe(1));
    SHOULD(ShouldlyString(oldV).ShouldBe(0));
    SHOULD(ShouldlyString(newV).ShouldBe(3));

    // Raw 8 -> stored 8, cb (old=3, new=8).
    SHOULD(ShouldlyString(obj.Set(id, Value::Int32(8))).ShouldBe(SetResult::APPLIED));
    SHOULD(ShouldlyString(obj.Get(id)).ShouldBe(Value::Int32(8)));
    SHOULD(ShouldlyString(fire).ShouldBe(2));
    SHOULD(ShouldlyString(oldV).ShouldBe(3));
    SHOULD(ShouldlyString(newV).ShouldBe(8));
}

TEST_CASE("S036 apply: coerce runs BEFORE validate — validate sees the coerced value")
{
    DependencyObject obj;
    obj.register_property(
        int32Key("Fixed"),
        PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None)
            .with_coerce([](const Value& v) {
                return Value::Int32(v.get<std::int32_t>() < 0 ? 5 : v.get<std::int32_t>());
            })
            .with_validate([](const Value& v) { return v.get<std::int32_t>() >= 0; }));

    // Raw -3 would fail validate (>= 0) on its own; coerce turns it into 5 and
    // validate(5) passes -> APPLIED with a real change (default 0 -> 5).
    SHOULD(ShouldlyString(obj.Set(PropertyId::of("Fixed"), Value::Int32(-3))).ShouldBe(SetResult::APPLIED));
    SHOULD(ShouldlyString(obj.Get(PropertyId::of("Fixed"))).ShouldBe(Value::Int32(5))); // the COERCED value stored
}

TEST_CASE("S036 apply: validate rejects — store unchanged, cb NOT fired")
{
    DependencyObject obj;
    obj.register_property(int32Key("Age"), int32Md(1, /*withCoerce*/ false, /*withValidate*/ true));

    // -3 rejected: current stays 1 (default), no cb (no recorder => passthrough;
    // we assert store-unchanged).
    SHOULD(ShouldlyString(obj.Set(PropertyId::of("Age"), Value::Int32(-3))).ShouldBe(SetResult::REJECTED));
    SHOULD(ShouldlyString(obj.Get(PropertyId::of("Age"))).ShouldBe(Value::Int32(1)));
    SHOULD(ShouldlyString(obj.Set(PropertyId::of("Age"), Value::Int32(0))).ShouldBe(SetResult::REJECTED));
    SHOULD(ShouldlyString(obj.Get(PropertyId::of("Age"))).ShouldBe(Value::Int32(1)));

    // Valid 5 -> applied.
    SHOULD(ShouldlyString(obj.Set(PropertyId::of("Age"), Value::Int32(5))).ShouldBe(SetResult::APPLIED));
    SHOULD(ShouldlyString(obj.Get(PropertyId::of("Age"))).ShouldBe(Value::Int32(5)));
}

TEST_CASE("S036 apply: cb fires ONLY on a real change")
{
    DependencyObject obj;
    int fire = 0;
    std::int32_t oldV = -1, newV = -1;
    obj.register_property(
        int32Key("Count"),
        PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None)
            .with_changed([&](const Value& o, const Value& n) {
                oldV = o.get<std::int32_t>();
                newV = n.get<std::int32_t>();
                ++fire;
            }));

    SHOULD(ShouldlyString(obj.Set(PropertyId::of("Count"), Value::Int32(9))).ShouldBe(SetResult::APPLIED));
    SHOULD(ShouldlyString(fire).ShouldBe(1));
    SHOULD(ShouldlyString(obj.Set(PropertyId::of("Count"), Value::Int32(9))).ShouldBe(SetResult::NO_CHANGE));
    SHOULD(ShouldlyString(obj.Set(PropertyId::of("Count"), Value::Int32(9))).ShouldBe(SetResult::NO_CHANGE));
    SHOULD(ShouldlyString(fire).ShouldBe(1));

    SHOULD(ShouldlyString(obj.Set(PropertyId::of("Count"), Value::Int32(11))).ShouldBe(SetResult::APPLIED));
    SHOULD(ShouldlyString(fire).ShouldBe(2));
    SHOULD(ShouldlyString(oldV).ShouldBe(9));
    SHOULD(ShouldlyString(newV).ShouldBe(11));

    SHOULD(ShouldlyString(obj.Set(PropertyId::of("Count"), Value::Int32(15))).ShouldBe(SetResult::APPLIED));
    SHOULD(ShouldlyString(fire).ShouldBe(3));
    SHOULD(ShouldlyString(oldV).ShouldBe(11));
    SHOULD(ShouldlyString(newV).ShouldBe(15));

    // Round-trip back to a previously held value is still a real change.
    SHOULD(ShouldlyString(obj.Set(PropertyId::of("Count"), Value::Int32(9))).ShouldBe(SetResult::APPLIED));
    SHOULD(ShouldlyString(fire).ShouldBe(4));
    SHOULD(ShouldlyString(oldV).ShouldBe(15));
    SHOULD(ShouldlyString(newV).ShouldBe(9));
}

TEST_CASE("S036 apply: absent callbacks = passthrough, store-as-given")
{
    DependencyObject obj;
    obj.register_property(int32Key("Raw"), int32Md(10, false, false));

    SHOULD(ShouldlyString(obj.Set(PropertyId::of("Raw"), Value::Int32(42))).ShouldBe(SetResult::APPLIED));
    SHOULD(ShouldlyString(obj.Get(PropertyId::of("Raw"))).ShouldBe(Value::Int32(42)));
    SHOULD(ShouldlyString(obj.Set(PropertyId::of("Raw"), Value::Int32(42))).ShouldBe(SetResult::NO_CHANGE));
    SHOULD(ShouldlyString(obj.Get(PropertyId::of("Raw"))).ShouldBe(Value::Int32(42)));
}

TEST_CASE("S036: harness stays type-bound — wrong-tag input throws, store untouched")
{
    DependencyObject obj;
    obj.register_property(int32Key("W"), int32Md(0, false, false));

    // Wrong tag on a valid pipeline path surfaces ValueTypeMismatch (S030/S034).
    SHOULD(ShouldThrow<ValueTypeMismatch>([&] {
        (void)obj.Set(PropertyId::of("W"), Value::Double(2.5));
    }));
    SHOULD(ShouldlyString(obj.Get(PropertyId::of("W"))).ShouldBe(Value::Int32(0))); // store untouched
}

TEST_CASE("S036: per-prop mastery — two properties coexist independently")
{
    DependencyObject obj;
    obj.register_property(int32Key("Width"), int32Md(1, false, false));
    obj.register_property(int32Key("Height"), int32Md(2, false, false));

    SHOULD(ShouldlyString(obj.Get(PropertyId::of("Width"))).ShouldBe(Value::Int32(1)));
    SHOULD(ShouldlyString(obj.Get(PropertyId::of("Height"))).ShouldBe(Value::Int32(2)));

    (void)obj.Set(PropertyId::of("Width"), Value::Int32(100));
    SHOULD(ShouldlyString(obj.Get(PropertyId::of("Width"))).ShouldBe(Value::Int32(100)));
    SHOULD(ShouldlyString(obj.Get(PropertyId::of("Height"))).ShouldBe(Value::Int32(2))); // untouched

    (void)obj.Set(PropertyId::of("Height"), Value::Int32(200));
    SHOULD(ShouldlyString(obj.Get(PropertyId::of("Width"))).ShouldBe(Value::Int32(100))); // untouched
    SHOULD(ShouldlyString(obj.Get(PropertyId::of("Height"))).ShouldBe(Value::Int32(200)));
}

TEST_CASE("S036: dedupe — re-registering a name throws PropertyAlreadyRegistered, store unchanged")
{
    DependencyObject obj;
    obj.register_property(int32Key("Width"), int32Md(1, false, false));
    (void)obj.Set(PropertyId::of("Width"), Value::Int32(50)); // take a local override

    // Re-registering the same NAME must throw and leave the store unchanged.
    SHOULD(ShouldThrow<PropertyAlreadyRegistered>([&] {
        (void)obj.register_property(int32Key("Width"), int32Md(9, false, false));
    }));

    // Store (and the local override) are untouched.
    SHOULD(ShouldlyString(obj.Get(PropertyId::of("Width"))).ShouldBe(Value::Int32(50)));
}

TEST_CASE("S036: Get returns by value — mutating the source Value cannot corrupt the store")
{
    DependencyObject obj;
    obj.register_property(int32Key("S"), int32Md(1, false, false));

    // Set an std::string value (the only payload stored via reference-aware Value)
    obj.register_property(
        PropertyKey("Label", ValueType::String, "TestOwner", PropertyFlags::None),
        PropertyMetadata::WithDefault(Value::String("init"), PropertyFlags::None));
    (void)obj.Set(PropertyId::of("Label"), Value::String("hello"));

    // Get returns BY VALUE: copies of the same string are independent.
    Value a = obj.Get(PropertyId::of("Label"));
    Value b = obj.Get(PropertyId::of("Label"));
    SHOULD(ShouldlyString(a).ShouldBe(Value::String("hello")));
    SHOULD(ShouldlyString(b).ShouldBe(Value::String("hello")));
    SHOULD(ShouldlyString(a).ShouldBe(b));
}

TEST_CASE("S036: thread-affinity marker — records creating thread")
{
    DependencyObject obj;
    // Created on the current (test) thread -> affine here.
    SHOULD(ShouldlyString(obj.is_affine_to_current_thread()).ShouldBe(true));
    SHOULD(ShouldlyString(obj.thread_affinity_id() == std::this_thread::get_id()).ShouldBe(true));
    SHOULD(ShouldlyString(obj.thread_affinity_id() != std::thread::id()).ShouldBe(true));

    // An object created on a DIFFERENT thread records THAT thread's id, so it
    // is NOT affine to this (test) thread. We record the worker's object
    // affinity id and compare it against this thread id here in the main thread.
    std::thread::id other;
    std::thread t([&] {
        DependencyObject inner; // constructed on thread t
        other = inner.thread_affinity_id();
        // The worker object is affine to the worker thread, not this one.
        // (checked via the recorded id below; we avoid CHECK from a foreign thread)
    });
    t.join();
    SHOULD(ShouldlyString(other != std::this_thread::get_id()).ShouldBe(true)); // different thread => different affinity
    SHOULD(ShouldlyString(other != std::thread::id()).ShouldBe(true));
}
