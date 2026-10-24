// S241 — ObservableObject: Set() + typed PropertyChanged signal
// (INotifyPropertyChanged-analogue). Sprint 6 (dep:S240).
//
// Proves the S241 headline against the REAL vaml::ObservableObject:
//   1. Set stores + Get reads back (round-trip typed values);
//   2. Set fires the BROADCAST signal with correct (id, old, new) on a REAL
//      change;
//   3. Set same value (real-change discipline) -> NO signal, NO storage change;
//   4. per-property subscription fires only for its property);
//   5. registration order preserved; remove_property_changed stops a handler;
//      re-add appends;
//   6. has() distinguishes a set-to-Value::Null property (present) from a
//      never-set one (absent);
//   7. the S240 reader through evaluate_property_path: present property
//      resolves (final), final-null ok=true, absent -> MissingProperty +
//      segmentIndex, non-final-null -> NullIntermediate, Index on an
//      ObservableObject -> NotReadable, non-pointer/non-ObservableObject root
//      -> NotReadable.
//
// S241-unique property names (no global-name pollution).

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"       // StringMaker + operator<< for Value/enums
#include <vaml/assert/Shouldly.h>         // ShouldlyString / ShouldThrow (SHOULD seam)

#include "vaml/core/ObservableObject.h"
#include "vaml/core/PropertyPath.h"
#include "vaml/core/Value.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

using vaml::ObservableObject;
using vaml::PropertyId;
using vaml::PropertyPath;
using vaml::PropertyPathError;
using vaml::PropertyRead;
using vaml::Value;
using vaml::ValueType;

namespace {

// S241-unique property names.
const char* kNameTitle = "S241Title";
const char* kNameCount = "S241Count";
const char* kNameOn    = "S241On";
const char* kNameEmpty = "S241Null";

// A recording sink that captures (id, old, new) invocation records.
struct Recorder
{
    struct Entry
    {
        PropertyId id;
        Value      oldValue;
        Value      newValue;
    };
    std::vector<Entry> entries;

    void record(PropertyId id, const Value& oldValue, const Value& newValue)
    {
        entries.push_back(Entry{ id, oldValue, newValue });
    }
    std::size_t size() const { return entries.size(); }
};

} // namespace

// ===========================================================================
// Set / Get (lightweight typed store)
// ===========================================================================

TEST_CASE("S241 Set/Get: round-trips typed values (String/Int32/Bool)")
{
    ObservableObject vm;
    SHOULD(ShouldlyIntegral(vm.Set(PropertyId::of(kNameTitle), Value::String("Hello"))).ShouldBe(true));
    SHOULD(ShouldlyIntegral(vm.Set(PropertyId::of(kNameCount), Value::Int32(42))).ShouldBe(true));
    SHOULD(ShouldlyIntegral(vm.Set(PropertyId::of(kNameOn), Value::Bool(true))).ShouldBe(true));

    SHOULD(ShouldlyString(vm.Get(PropertyId::of(kNameTitle))).ShouldBe(Value::String("Hello")));
    SHOULD(ShouldlyString(vm.Get(PropertyId::of(kNameCount))).ShouldBe(Value::Int32(42)));
    SHOULD(ShouldlyString(vm.Get(PropertyId::of(kNameOn))).ShouldBe(Value::Bool(true)));
}

TEST_CASE("S241 Set/Get: an unset property reads Value::Null() and is absent")
{
    ObservableObject vm;
    SHOULD(ShouldlyString(vm.Get(PropertyId::of(kNameTitle)).isNull()).ShouldBe(true));
    SHOULD(ShouldlyIntegral(vm.has(PropertyId::of(kNameTitle))).ShouldBe(false));
}

TEST_CASE("S241 Set: same value (real-change discipline) does not store and returns false")
{
    ObservableObject vm;
    SHOULD(ShouldlyIntegral(vm.Set(PropertyId::of(kNameTitle), Value::String("A"))).ShouldBe(true));
    SHOULD(ShouldlyIntegral(vm.Set(PropertyId::of(kNameTitle), Value::String("A"))).ShouldBe(false));
    // Storage unchanged.
    SHOULD(ShouldlyString(vm.Get(PropertyId::of(kNameTitle))).ShouldBe(Value::String("A")));
}

// ===========================================================================
// Typed PropertyChanged signal
// ===========================================================================

TEST_CASE("S241 signal: broadcast fires with correct (id, old, new) on a real change")
{
    ObservableObject vm;
    Recorder rec;
    (void)vm.add_property_changed([&rec](PropertyId id, const Value& o, const Value& n) {
        rec.record(id, o, n);
    });

    (void)vm.Set(PropertyId::of(kNameTitle), Value::String("first"));
    (void)vm.Set(PropertyId::of(kNameTitle), Value::String("second"));

    SHOULD(ShouldlyIntegral(rec.size()).ShouldBe(2));
    SHOULD(ShouldlyString(rec.entries[0].id.name()).ShouldBe(kNameTitle));
    // First change: old = absent -> Value::Null().
    SHOULD(ShouldlyString(rec.entries[0].oldValue.isNull()).ShouldBe(true));
    SHOULD(ShouldlyString(rec.entries[0].newValue).ShouldBe(Value::String("first")));
    // Second change: old == first, new == second.
    SHOULD(ShouldlyString(rec.entries[1].oldValue).ShouldBe(Value::String("first")));
    SHOULD(ShouldlyString(rec.entries[1].newValue).ShouldBe(Value::String("second")));
}

TEST_CASE("S241 signal: no signal on a no-change Set")
{
    ObservableObject vm;
    Recorder rec;
    (void)vm.add_property_changed([&rec](PropertyId id, const Value& o, const Value& n) {
        rec.record(id, o, n);
    });

    (void)vm.Set(PropertyId::of(kNameTitle), Value::String("same"));
    (void)vm.Set(PropertyId::of(kNameTitle), Value::String("same"));
    SHOULD(ShouldlyIntegral(rec.size()).ShouldBe(1));
}

TEST_CASE("S241 signal: per-property subscription fires only for its property")
{
    ObservableObject vm;
    Recorder titleOnly;
    (void)vm.add_property_changed(PropertyId::of(kNameTitle),
                            [&titleOnly](PropertyId id, const Value& o, const Value& n) {
                                titleOnly.record(id, o, n);
                            });

    (void)vm.Set(PropertyId::of(kNameTitle), Value::String("t"));
    (void)vm.Set(PropertyId::of(kNameCount), Value::Int32(7)); // other property
    (void)vm.Set(PropertyId::of(kNameTitle), Value::String("t2"));

    SHOULD(ShouldlyIntegral(titleOnly.size()).ShouldBe(2));
    SHOULD(ShouldlyString(titleOnly.entries[0].id.name()).ShouldBe(kNameTitle));
    SHOULD(ShouldlyString(titleOnly.entries[1].id.name()).ShouldBe(kNameTitle));
}

TEST_CASE("S241 signal: broadcast + per-property both fire, registration order preserved")
{
    ObservableObject vm;
    Recorder rec;
    Recorder countOnly;

    (void)vm.add_property_changed([&rec](PropertyId id, const Value& o, const Value& n) {
        rec.record(id, o, n);
    });
    (void)vm.add_property_changed(PropertyId::of(kNameCount),
                            [&countOnly](PropertyId id, const Value& o, const Value& n) {
                                countOnly.record(id, o, n);
                            });

    (void)vm.Set(PropertyId::of(kNameCount), Value::Int32(5));
    (void)vm.Set(PropertyId::of(kNameTitle), Value::String("x"));

    // Broadcast saw BOTH changes, in Set order.
    SHOULD(ShouldlyIntegral(rec.size()).ShouldBe(2));
    SHOULD(ShouldlyString(rec.entries[0].id.name()).ShouldBe(kNameCount));
    SHOULD(ShouldlyString(rec.entries[1].id.name()).ShouldBe(kNameTitle));
    // Per-property saw only the count change.
    SHOULD(ShouldlyIntegral(countOnly.size()).ShouldBe(1));
    SHOULD(ShouldlyString(countOnly.entries[0].id.name()).ShouldBe(kNameCount));
}

TEST_CASE("S241 signal: remove_property_changed stops a handler; re-add appends")
{
    ObservableObject vm;
    Recorder rec;
    auto token = vm.add_property_changed([&rec](PropertyId id, const Value& o, const Value& n) {
        rec.record(id, o, n);
    });

    (void)vm.Set(PropertyId::of(kNameTitle), Value::String("a"));
    vm.remove_property_changed(token);
    (void)vm.Set(PropertyId::of(kNameTitle), Value::String("b")); // handler removed
    (void)vm.add_property_changed([&rec](PropertyId id, const Value& o, const Value& n) {
        rec.record(id, o, n);
    });
    (void)vm.Set(PropertyId::of(kNameTitle), Value::String("c")); // re-added handler

    // Only "a" (first) and "c" (re-added) fired; "b" was silent.
    SHOULD(ShouldlyIntegral(rec.size()).ShouldBe(2));
    SHOULD(ShouldlyString(rec.entries[0].newValue).ShouldBe(Value::String("a")));
    SHOULD(ShouldlyString(rec.entries[1].newValue).ShouldBe(Value::String("c")));
    SHOULD(ShouldlyIntegral(vm.has(PropertyId::of(kNameTitle))).ShouldBe(true));
}

TEST_CASE("S241 has(): a Set-to-Value::Null property is present; a never-set name is absent")
{
    ObservableObject vm;
    (void)vm.Set(PropertyId::of(kNameEmpty), Value::Null());
    SHOULD(ShouldlyIntegral(vm.has(PropertyId::of(kNameEmpty))).ShouldBe(true));
    SHOULD(ShouldlyString(vm.Get(PropertyId::of(kNameEmpty)).isNull()).ShouldBe(true));
    SHOULD(ShouldlyIntegral(vm.has(PropertyId::of(kNameTitle))).ShouldBe(false));
}

// ===========================================================================
// ObservableObject through the S240 evaluator (observable_object_reader)
// ===========================================================================

namespace {
PropertyRead eval(ObservableObject& vm, std::string_view path)
{
    Value root = Value::UntypedPtr(&vm);
    return vaml::evaluate_property_path(root, PropertyPath::parse(path),
                                        vaml::observable_object_reader());
}
} // namespace

TEST_CASE("S241 reader: a present property resolves to its value (final)")
{
    ObservableObject vm;
    (void)vm.Set(PropertyId::of(kNameTitle), Value::String("Hello"));

    PropertyRead r = eval(vm, kNameTitle);
    SHOULD(ShouldlyIntegral(r.ok).ShouldBe(true));
    SHOULD(ShouldlyString(r.value).ShouldBe(Value::String("Hello")));
}

TEST_CASE("S241 reader: an absent name -> MissingProperty at segmentIndex 0 (never a raw throw)")
{
    ObservableObject vm;
    PropertyRead r = eval(vm, "S241Nope");
    SHOULD(ShouldlyIntegral(r.ok).ShouldBe(false));
    SHOULD(ShouldlyIntegral(static_cast<int>(r.error))
               .ShouldBe(static_cast<int>(PropertyPathError::MissingProperty)));
    SHOULD(ShouldlyIntegral(r.segmentIndex).ShouldBe(0));
}

TEST_CASE("S241 reader: a stored null read as the FINAL segment is a valid ok=true result")
{
    ObservableObject vm;
    (void)vm.Set(PropertyId::of(kNameEmpty), Value::Null());
    PropertyRead r = eval(vm, kNameEmpty);
    SHOULD(ShouldlyIntegral(r.ok).ShouldBe(true));
    SHOULD(ShouldlyString(r.value.isNull()).ShouldBe(true));
}

TEST_CASE("S241 reader: a stored null on a NON-final segment -> NullIntermediate at its index")
{
    ObservableObject vm;
    (void)vm.Set(PropertyId::of(kNameEmpty), Value::Null());
    // "S241Null.Foo": segment0 = S241Null -> Non-final null -> NullIntermediate at 0.
    PropertyRead r = eval(vm, std::string(kNameEmpty) + ".Foo");
    SHOULD(ShouldlyIntegral(r.ok).ShouldBe(false));
    SHOULD(ShouldlyIntegral(static_cast<int>(r.error))
               .ShouldBe(static_cast<int>(PropertyPathError::NullIntermediate)));
    SHOULD(ShouldlyIntegral(r.segmentIndex).ShouldBe(0));
}

TEST_CASE("S241 reader: a chain through an ObservableObject as a non-final node resolves")
{
    // A host ObservableObject whose property points at a SECOND ObservableObject.
    ObservableObject inner;
    (void)inner.Set(PropertyId::of(kNameTitle), Value::String("deep"));
    ObservableObject outer;
    (void)outer.Set(PropertyId::of("S241Inner"), Value::UntypedPtr(&inner));

    Value root = Value::UntypedPtr(&outer);
    PropertyRead r = vaml::evaluate_property_path(
        root, PropertyPath::parse("S241Inner." + std::string(kNameTitle)),
        vaml::observable_object_reader());
    SHOULD(ShouldlyIntegral(r.ok).ShouldBe(true));
    SHOULD(ShouldlyString(r.value).ShouldBe(Value::String("deep")));
}

TEST_CASE("S241 reader: Index on an ObservableObject -> NotReadable")
{
    ObservableObject vm;
    PropertyRead r = eval(vm, "[0]");
    SHOULD(ShouldlyIntegral(r.ok).ShouldBe(false));
    SHOULD(ShouldlyIntegral(static_cast<int>(r.error))
               .ShouldBe(static_cast<int>(PropertyPathError::NotReadable)));
}

TEST_CASE("S241 reader: a non-ObservableObject / non-UntypedPtr root -> NotReadable")
{
    // A scalar root (no ObservableObject) is NotReadable.
    Value scalar = Value::Int32(5);
    PropertyRead r1 = vaml::evaluate_property_path(scalar, PropertyPath::parse(kNameTitle),
                                                   vaml::observable_object_reader());
    SHOULD(ShouldlyIntegral(r1.ok).ShouldBe(false));
    SHOULD(ShouldlyIntegral(static_cast<int>(r1.error))
               .ShouldBe(static_cast<int>(PropertyPathError::NotReadable)));
}
