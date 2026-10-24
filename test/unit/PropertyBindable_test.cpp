// S247_1 — Bindable-property convenience layer (vaml/mvvm). Sprint 6 (dep:S247).
//
// Proves the S247_1 headline — the PROPERTY_BINDABLE(type, Name)-style
// generator + the Toolkit-analog `vaml::mvvm::set_property(...)` free template
// over the S240/S241 core:
//   1. PROPERTY_BINDABLE expands to a typed backing field + getter + setter;
//   2. the default backing value is TYPE{} before any set;
//   3. setter returns true on the first REAL change; getter AND the S241
//      ObservableObject Value store both update;
//   4. the real S241 PropertyChanged signal fires (id, old, new) with the right
//      id + old value;
//   5. a SAME-value set returns false with NO store write and NO signal
//      (S035/S241 real-change discipline threaded through set_property);
//   6. multiple bindable properties on one VM are independent;
//   7. reader parity: observable_object_reader()->Get(name) through the S240
//      evaluator reads the LIVE value after a set;
//   8. real-change old-value across multiple sets (old, new) sequencing;
//   9. using the free set_property template directly (non-macro path);
//  10. Color + Thickness round-trip through the bounded Value store.
//
// ADR-018: zero new locks (single-threaded value set on the VM's owner thread).

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"       // StringMaker + operator<< for Value/enums
#include <vaml/assert/Shouldly.h>         // ShouldlyString / SHOULD seam

#include "vaml/core/Color.h"
#include "vaml/core/ObservableObject.h"
#include "vaml/core/PropertyPath.h"
#include "vaml/core/Thickness.h"
#include "vaml/core/Value.h"
#include "vaml/mvvm/property_bindable.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

using vaml::ObservableObject;
using vaml::PropertyId;
using vaml::PropertyPath;
using vaml::PropertyRead;
using vaml::Value;
using vaml::mvvm::set_property;

namespace {

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

// A test ViewModel authoring bindable properties across representative bounded
// payload types: String, Int32, Double, Bool, Color, Thickness.
class TestViewModel : public ObservableObject
{
public:
    PROPERTY_BINDABLE(std::string, Title)
    PROPERTY_BINDABLE(std::int32_t, Count)
    PROPERTY_BINDABLE(double, Scale)
    PROPERTY_BINDABLE(bool, IsEnabled)
    PROPERTY_BINDABLE(vaml::Color, Accent)
    PROPERTY_BINDABLE(vaml::Thickness, Padding)

    // A non-bindable manual property written through base Set (the documented
    // non-desync path — store-only, no field).
    void set_manual(std::int32_t v) { (void)ObservableObject::Set(PropertyId::of("S2471Manual"), Value::Int32(v)); }
};

} // namespace

// ===========================================================================
// 1/2/3. Default backing value; setter returns true on a real change; getter
//         AND the ObservableObject Value store both update.
// ===========================================================================

TEST_CASE("S247_1 PROPERTY_BINDABLE: default backing value is TYPE{}")
{
    TestViewModel vm;
    SHOULD(ShouldlyString(vm.Title()).ShouldBe(std::string{}));
    SHOULD(ShouldlyIntegral(vm.Count()).ShouldBe(std::int32_t{ 0 }));
    SHOULD(ShouldlyString(vm.Scale()).ShouldBe(0.0));
    SHOULD(ShouldlyString(vm.IsEnabled()).ShouldBe(false));
    SHOULD(ShouldlyString(vm.Accent()).ShouldBe(vaml::Color{ 0, 0, 0, 0 }));
    SHOULD(ShouldlyString(vm.Padding()).ShouldBe(vaml::Thickness{ 0, 0, 0, 0 }));
}

TEST_CASE("S247_1 setter: returns true on a real change; getter + store both update")
{
    TestViewModel vm;
    const bool changed = vm.set_Title("Hello");
    SHOULD(ShouldlyIntegral(changed).ShouldBe(true));
    // Getter (typed field) updated.
    SHOULD(ShouldlyString(vm.Title()).ShouldBe(std::string("Hello")));
    // The ObservableObject Value store mirrored (S241 Set stored it).
    SHOULD(ShouldlyString(vm.Get(PropertyId::of("Title"))).ShouldBe(Value::String("Hello")));
    // The store knows the property is present.
    SHOULD(ShouldlyIntegral(vm.has(PropertyId::of("Title"))).ShouldBe(true));
}

TEST_CASE("S247_1 setter: INT32 + DOUBLE + BOOL real changes update getter and store")
{
    TestViewModel vm;
    SHOULD(ShouldlyIntegral(vm.set_Count(42)).ShouldBe(true));
    SHOULD(ShouldlyIntegral(vm.Count()).ShouldBe(std::int32_t{ 42 }));
    SHOULD(ShouldlyString(vm.Get(PropertyId::of("Count"))).ShouldBe(Value::Int32(42)));

    SHOULD(ShouldlyString(vm.set_Scale(1.5)).ShouldBe(true));
    SHOULD(ShouldlyString(vm.Scale()).ShouldBe(1.5));
    SHOULD(ShouldlyString(vm.Get(PropertyId::of("Scale"))).ShouldBe(Value::Double(1.5)));

    SHOULD(ShouldlyIntegral(vm.set_IsEnabled(true)).ShouldBe(true));
    SHOULD(ShouldlyString(vm.IsEnabled()).ShouldBe(true));
    SHOULD(ShouldlyString(vm.Get(PropertyId::of("IsEnabled"))).ShouldBe(Value::Bool(true)));
}

// ===========================================================================
// 4. Real PropertyChanged signal (id, old, new).
// ===========================================================================

TEST_CASE("S247_1 signal: setter fires PropertyChanged with correct (id, old, new)")
{
    TestViewModel vm;
    Recorder rec;
    (void)vm.add_property_changed([&rec](PropertyId id, const Value& o, const Value& n) {
        rec.record(id, o, n);
    });

    (void)vm.set_Title("first");
    (void)vm.set_Title("second");

    SHOULD(ShouldlyIntegral(rec.size()).ShouldBe(std::size_t{ 2 }));
    // First change: old = absent -> Value::Null(); new = "first".
    SHOULD(ShouldlyString(rec.entries[0].id.name()).ShouldBe(std::string_view("Title")));
    SHOULD(ShouldlyString(rec.entries[0].oldValue.isNull()).ShouldBe(true));
    SHOULD(ShouldlyString(rec.entries[0].newValue).ShouldBe(Value::String("first")));
    // Second change: old = "first"; new = "second".
    SHOULD(ShouldlyString(rec.entries[1].id.name()).ShouldBe(std::string_view("Title")));
    SHOULD(ShouldlyString(rec.entries[1].oldValue).ShouldBe(Value::String("first")));
    SHOULD(ShouldlyString(rec.entries[1].newValue).ShouldBe(Value::String("second")));
}

TEST_CASE("S247_1 signal: per-property subscription receives its property only")
{
    TestViewModel vm;
    Recorder titleOnly;
    (void)vm.add_property_changed(PropertyId::of("Title"),
                            [&titleOnly](PropertyId id, const Value& o, const Value& n) {
                                titleOnly.record(id, o, n);
                            });

    (void)vm.set_Title("t");
    (void)vm.set_Count(7); // other property -> no Title event
    (void)vm.set_Title("t2");

    SHOULD(ShouldlyIntegral(titleOnly.size()).ShouldBe(std::size_t{ 2 }));
    SHOULD(ShouldlyString(titleOnly.entries[0].id.name()).ShouldBe(std::string_view("Title")));
    SHOULD(ShouldlyString(titleOnly.entries[1].id.name()).ShouldBe(std::string_view("Title")));
}

// ===========================================================================
// 5. Same-value set -> no store write, no signal (real-change discipline).
// ===========================================================================

TEST_CASE("S247_1 setter: a same-value set returns false with NO signal and NO store write")
{
    TestViewModel vm;
    Recorder rec;
    (void)vm.add_property_changed([&rec](PropertyId id, const Value& o, const Value& n) {
        rec.record(id, o, n);
    });

    SHOULD(ShouldlyIntegral(vm.set_Title("same")).ShouldBe(true));
    const bool second = vm.set_Title("same");
    SHOULD(ShouldlyIntegral(second).ShouldBe(false));
    // Only the first (real) change fired.
    SHOULD(ShouldlyIntegral(rec.size()).ShouldBe(std::size_t{ 1 }));
    // Getter + store unchanged (still "same").
    SHOULD(ShouldlyString(vm.Title()).ShouldBe(std::string("same")));
    SHOULD(ShouldlyString(vm.Get(PropertyId::of("Title"))).ShouldBe(Value::String("same")));
}

TEST_CASE("S247_1 setter: same-value is real-change per type (Color/Thickness/Double)")
{
    TestViewModel vm;
    Recorder rec;
    (void)vm.add_property_changed([&rec](PropertyId id, const Value& o, const Value& n) {
        rec.record(id, o, n);
    });

    (void)vm.set_Accent(vaml::Color{ 1, 2, 3, 255 });
    SHOULD(ShouldlyIntegral(vm.set_Accent(vaml::Color{ 1, 2, 3, 255 })).ShouldBe(false));
    (void)vm.set_Padding(vaml::Thickness{ 1, 2, 3, 4 });
    SHOULD(ShouldlyIntegral(vm.set_Padding(vaml::Thickness{ 1, 2, 3, 4 })).ShouldBe(false));
    (void)vm.set_Scale(2.0);
    SHOULD(ShouldlyString(vm.set_Scale(2.0)).ShouldBe(false));

    // Only the 3 first (real) sets fired — each same-value second set was silent.
    SHOULD(ShouldlyIntegral(rec.size()).ShouldBe(std::size_t{ 3 }));
}

// ===========================================================================
// 6. Multiple properties independent.
// ===========================================================================

TEST_CASE("S247_1 independence: multiple bindable properties on one VM")
{
    TestViewModel vm;
    (void)vm.set_Count(5);
    (void)vm.set_Title("x");
    (void)vm.set_IsEnabled(true);

    // Mutating one does not disturb the others.
    (void)vm.set_Count(6);
    SHOULD(ShouldlyString(vm.Title()).ShouldBe(std::string("x")));
    SHOULD(ShouldlyString(vm.IsEnabled()).ShouldBe(true));
    SHOULD(ShouldlyIntegral(vm.Count()).ShouldBe(std::int32_t{ 6 }));
    SHOULD(ShouldlyString(vm.Get(PropertyId::of("Count"))).ShouldBe(Value::Int32(6)));
    SHOULD(ShouldlyString(vm.Get(PropertyId::of("Title"))).ShouldBe(Value::String("x")));
}

// ===========================================================================
// 7. Reader parity through the S240 evaluator.
// ===========================================================================

namespace {
PropertyRead eval(ObservableObject& vm, std::string_view path)
{
    Value root = Value::UntypedPtr(&vm);
    return vaml::evaluate_property_path(root, PropertyPath::parse(path),
                                        vaml::observable_object_reader());
}
} // namespace

TEST_CASE("S247_1 reader parity: observable_object_reader reads the live value")
{
    TestViewModel vm;
    (void)vm.set_Count(9);

    PropertyRead r = eval(vm, "Count");
    SHOULD(ShouldlyIntegral(r.ok).ShouldBe(true));
    SHOULD(ShouldlyString(r.value).ShouldBe(Value::Int32(9)));

    // An absent bindable name is a normal MissingProperty (never a throw).
    PropertyRead r2 = eval(vm, "Title");
    SHOULD(ShouldlyIntegral(r2.ok).ShouldBe(false));
    SHOULD(ShouldlyIntegral(static_cast<int>(r2.error))
               .ShouldBe(static_cast<int>(vaml::PropertyPathError::MissingProperty)));
}

// ===========================================================================
// 9. Direct (non-macro) set_property usage.
// ===========================================================================

TEST_CASE("S247_1 set_property: free template works directly on a plain ObservableObject")
{
    ObservableObject vm;
    std::int32_t field = 0;
    Recorder rec;
    (void)vm.add_property_changed([&rec](PropertyId id, const Value& o, const Value& n) {
        rec.record(id, o, n);
    });

    SHOULD(ShouldlyIntegral(set_property(vm, PropertyId::of("Raw"), field, std::int32_t{ 10 })).ShouldBe(true));
    SHOULD(ShouldlyIntegral(field).ShouldBe(std::int32_t{ 10 }));
    SHOULD(ShouldlyString(vm.Get(PropertyId::of("Raw"))).ShouldBe(Value::Int32(10)));

    // Same value -> no change.
    SHOULD(ShouldlyIntegral(set_property(vm, PropertyId::of("Raw"), field, std::int32_t{ 10 })).ShouldBe(false));
    SHOULD(ShouldlyIntegral(rec.size()).ShouldBe(std::size_t{ 1 }));
    SHOULD(ShouldlyString(rec.entries[0].oldValue.isNull()).ShouldBe(true));
    SHOULD(ShouldlyString(rec.entries[0].newValue).ShouldBe(Value::Int32(10)));
}

// ===========================================================================
// 10. Color + Thickness round-trip through the bounded Value store.
// ===========================================================================

TEST_CASE("S247_1 Color/Thickness round-trip through the Value store")
{
    TestViewModel vm;
    (void)vm.set_Accent(vaml::Color{ 200, 150, 100, 255 });
    (void)vm.set_Padding(vaml::Thickness{ 5, 6, 7, 8 });

    // Getter holds the typed value.
    SHOULD(ShouldlyString(vm.Accent()).ShouldBe(vaml::Color{ 200, 150, 100, 255 }));
    SHOULD(ShouldlyString(vm.Padding()).ShouldBe(vaml::Thickness{ 5, 6, 7, 8 }));
    // Store holds the same tagged Value (round-trip).
    SHOULD(ShouldlyString(vm.Get(PropertyId::of("Accent"))).ShouldBe(Value::Color(vaml::Color{ 200, 150, 100, 255 })));
    SHOULD(ShouldlyString(vm.Get(PropertyId::of("Padding"))).ShouldBe(Value::Thickness(vaml::Thickness{ 5, 6, 7, 8 })));
}
