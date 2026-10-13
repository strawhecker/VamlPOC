// S046 (dep:S045) — DependencyProperty coercion + per-set validation callbacks,
// applied end-to-end against live DependencyObjects via the Shouldly
// vocabulary (S036_5 standing convention).
//
// CONTEXT / SCOPE DECISION (recorded, NOT silent):
//   The original S046 roadmap title was "registration-time dependency
//   validation (ValidateDependencies)". Reviewed against real commercial WPF
//   MVVM, this was REDIRECTED (user-accepted) to the WPF-faithful value hooks —
//   coercion + per-set validation — which are what DPs actually ship for
//   property coupling. The coerce -> validate(coerced) -> fire-on-real-change
//   -> store pipeline is ALREADY implemented in DependencyObject::Set /
//   ProcessPendingChanges (S036/S037/S045) and the callbacks already live in
//   PropertyMetadata (S034). Therefore S046 introduces NO new product symbol
//   (a standalone DependencyProperty::CoerceValue seam or a duplicated
//   CoerceValueCallback alias would be DEAD/redundant surface — precisely what
//   S048's refactor gate removes; NO-LAZY-LLM). S046 instead proves the two
//   dimensions previous steps left UNCOVERED:
//     (1) ATTACHED / GLOBAL axis — a globally-registered DependencyProperty
//         carrying with_coerce + with_validate, set on an object that never
//         locally registers it (the S043 attached premise): the coerced value
//         is applied, and a value that fails validate-after-coerce is REJECTED
//         with the store unchanged.
//     (2) WRONG-THREAD QUEUED COERCION — coerce+validate run AT QUEUE TIME
//         (S037), so what is POSTED and later DRAINED is the COERCED value,
//         not the raw input.
//   Both are real, missing coverage: no existing test registers a global
//   DependencyProperty with a coerce hook and sets it on a live object, and no
//   existing test asserts the coerced value (rather than the raw write) is
//   what the wrong-thread drain stores.
//
// LIFETIME CONTRACT: PropertyId wraps a NON-OWNING std::string_view. Every
// PropertyId in this TU is built from a STRING LITERAL (static storage
// duration), so the view is valid for the whole program. NEVER build a
// PropertyId from a temporary std::string (S033).
//
// Uses the Shouldly-for-doctest vocabulary (S036_5 standing convention): no
// CHECK / no CHECK_THROWS_AS.

// doctest's main comes from smoke_test.cpp.
#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/DependencyObject.h"
#include "vaml/core/DependencyProperty.h"
#include "vaml/core/PropertyFlags.h"
#include "vaml/core/PropertyId.h"
#include "vaml/core/PropertyKey.h"
#include "vaml/core/PropertyMetadata.h"
#include "vaml/core/Value.h"

#include <cstdint>
#include <string>
#include <thread>
#include <unordered_set>

using vaml::DependencyObject;
using vaml::DependencyProperty;
using vaml::PropertyFlags;
using vaml::PropertyId;
using vaml::PropertyKey;
using vaml::PropertyMetadata;
using vaml::SetResult;
using vaml::Value;
using vaml::ValueType;
using vaml::ValueTypeMismatch;
using vaml::test::ShouldThrow;

namespace {

// The GLOBAL DependencyProperty table is name-unique (S039). Each S046 name
// carries a unique suffix so no cross-TEST_CASE / cross-TU collision is
// possible; the set guards against accidental reuse within this TU.
std::unordered_set<std::string>& global_seen()
{
    static std::unordered_set<std::string> seen;
    return seen;
}

// Registers an ATTACHED Int32 property GLOBALLY (once per literal name, owner
// A) — objects do NOT locally register it (the true attached premise, S043).
// `coerce` / `validate` hooks are threaded onto the metadata when provided, so
// the GLOBAL descriptor itself carries the coercion policy that any object can
// Set through.
const DependencyProperty& registerGlobal(const char* name,
                                         Value def,
                                         PropertyMetadata::CoerceFunc coerce = {},
                                         PropertyMetadata::ValidateFunc validate = {})
{
    if (global_seen().insert(name).second)
    {
        PropertyMetadata md = PropertyMetadata::WithDefault(def, PropertyFlags::None);
        if (coerce)
        {
            md = std::move(md).with_coerce(std::move(coerce));
        }
        if (validate)
        {
            md = std::move(md).with_validate(std::move(validate));
        }
        return DependencyProperty::Register(name, ValueType::Int32, "AttachedOwner_A", std::move(md));
    }
    return DependencyProperty::Get(name);
}

// A non-affine Set() on an Int32 property, executed on a fresh thread, the
// result captured back into `out` by a join barrier (no concurrency) — the
// S037 marshalling harness pattern.
void setFromOtherThread(DependencyObject& obj, const PropertyId& id, Value v, SetResult* out)
{
    std::thread t([&] {
        *out = obj.Set(id, v);
    });
    t.join();
}

} // namespace

TEST_CASE("S046: attached/global coerce+validate — coerced value applied on a non-registering object")
{
    // A GLOBALLY-registered Int32 property (owner A) whose metadata carries a
    // coerce (clamp to [0,100]) + validate (rejects exactly 7). Object B never
    // calls register_property — the true attached premise (S043).
    const char* name = "BarClamp_S046";
    registerGlobal(name,
                   Value::Int32(10),
                   /*coerce*/ [](const Value& v) {
                       const std::int32_t n = v.get<std::int32_t>();
                       return Value::Int32(n < 0 ? 0 : (n > 100 ? 100 : n));
                   },
                   /*validate*/ [](const Value& v) { return v.get<std::int32_t>() != 7; });
    const PropertyId id = PropertyId::of(name);

    DependencyObject b; // never locally registers the property

    // Raw 150 -> coerced to 100, validate(100) passes -> APPLIED.
    SHOULD(ShouldlyString(b.Set(id, Value::Int32(150))).ShouldBe(SetResult::APPLIED));
    SHOULD(ShouldlyString(b.Get(id)).ShouldBe(Value::Int32(100))); // the COERCED value stored

    // Raw -5 -> coerced to 0 (a real change 100 -> 0) -> APPLIED.
    SHOULD(ShouldlyString(b.Set(id, Value::Int32(-5))).ShouldBe(SetResult::APPLIED));
    SHOULD(ShouldlyString(b.Get(id)).ShouldBe(Value::Int32(0))); // coerced 0

    // Raw 7 -> coerced 7, validate(7) rejects -> REJECTED, store unchanged.
    SHOULD(ShouldlyString(b.Set(id, Value::Int32(7))).ShouldBe(SetResult::REJECTED));
    SHOULD(ShouldlyString(b.Get(id)).ShouldBe(Value::Int32(0)));
}

TEST_CASE("S046: attached/global coerce+validate — wrong-tag input throws on the global axis")
{
    // Same globally-registered coercion property: a wrong-tag input from the
    // global-attached path surfaces ValueTypeMismatch (S030/S034) and leaves
    // the store unchanged — the type-bound guard applies on the attached axis.
    const char* name = "BarClampType_S046";
    registerGlobal(name,
                   Value::Int32(10),
                   /*coerce*/ [](const Value& v) {
                       const std::int32_t n = v.get<std::int32_t>();
                       return Value::Int32(n < 0 ? 0 : (n > 100 ? 100 : n));
                   },
                   /*validate*/ [](const Value& v) { return v.get<std::int32_t>() != 7; });
    const PropertyId id = PropertyId::of(name);

    DependencyObject b; // never locally registers
    SHOULD(ShouldThrow<ValueTypeMismatch>([&] {
        (void)b.Set(id, Value::Double(2.5));
    }));
    SHOULD(ShouldlyString(b.Get(id)).ShouldBe(Value::Int32(10))); // global default, unchanged
}

TEST_CASE("S046: attached/global property with NO hooks stores as-given (backward compat)")
{
    // A globally-registered property with no coerce/validate: Set stores the
    // value as-given on the attached axis — passthrough semantics preserved
    // (S036 passthrough, now proven on the global/attached path).
    const char* name = "PlainAttached_S046";
    registerGlobal(name, Value::Int32(0)); // no hooks
    const PropertyId id = PropertyId::of(name);

    DependencyObject b; // never locally registers
    SHOULD(ShouldlyString(b.Set(id, Value::Int32(42))).ShouldBe(SetResult::APPLIED));
    SHOULD(ShouldlyString(b.Get(id)).ShouldBe(Value::Int32(42)));
    SHOULD(ShouldlyString(b.Set(id, Value::Int32(42))).ShouldBe(SetResult::NO_CHANGE)); // repeat
    SHOULD(ShouldlyString(b.Get(id)).ShouldBe(Value::Int32(42)));
}

TEST_CASE("S046: wrong-thread queued coercion — the COERCED value is what the drain stores")
{
    // S037: a non-affine Set runs coerce+validate AT QUEUE TIME and POSTS the
    // accepted (coerced) value. S046 proves the coercion actually ran: the
    // drain stores the COERCED value, not the raw input.
    DependencyObject obj;
    const char* name = "QueuedClamp_S046";
    PropertyKey key(name, ValueType::Int32, "S046Owner", PropertyFlags::None);
    int fire = 0;
    std::int32_t oldV = -1, newV = -1;
    PropertyMetadata md = PropertyMetadata::WithDefault(Value::Int32(10), PropertyFlags::None);
    md = std::move(md)
             .with_coerce([](const Value& v) {
                 const std::int32_t n = v.get<std::int32_t>();
                 return Value::Int32(n < 0 ? 0 : (n > 100 ? 100 : n));
             })
             .with_changed([&](const Value& o, const Value& n) {
                 oldV = o.get<std::int32_t>();
                 newV = n.get<std::int32_t>();
                 ++fire;
             });
    (void)obj.register_property(std::move(key), std::move(md));
    const PropertyId id = PropertyId::of(name);

    // Affine Set(50) -> applied 50; cb fired once (default 10 -> 50).
    SHOULD(ShouldlyString(obj.Set(id, Value::Int32(50))).ShouldBe(SetResult::APPLIED));
    SHOULD(ShouldlyString(obj.Get(id)).ShouldBe(Value::Int32(50)));
    SHOULD(ShouldlyString(fire).ShouldBe(1));

    // Non-affine Set(150): coerce runs at queue time -> 100, validate passes,
    // POSTED (coerced). Get still 50 (not applied); queue depth 1.
    SetResult r = SetResult::NO_CHANGE;
    setFromOtherThread(obj, id, Value::Int32(150), &r);
    SHOULD(ShouldlyString(r).ShouldBe(SetResult::QUEUED));
    SHOULD(ShouldlyString(obj.Get(id)).ShouldBe(Value::Int32(50))); // not applied
    SHOULD(ShouldlyString(obj.pending_change_count()).ShouldBe(1u));

    // Drain on the affinity thread: stores the COERCED value 100, not the raw
    // 150, and fires the changed cb a second time (50 -> 100).
    obj.ProcessPendingChanges();
    SHOULD(ShouldlyString(obj.Get(id)).ShouldBe(Value::Int32(100))); // coerced queued value
    SHOULD(ShouldlyString(obj.pending_change_count()).ShouldBe(0u));
    SHOULD(ShouldlyString(fire).ShouldBe(2));
    SHOULD(ShouldlyString(oldV).ShouldBe(50));
    SHOULD(ShouldlyString(newV).ShouldBe(100)); // the coerced delta, not raw 150
}

TEST_CASE("S046: wrong-thread queued coercion clamps a sub-zero raw write to 0")
{
    // Second coercion-at-queue proof: a raw negative is clamped to 0 BEFORE
    // it is queued, so the drain applies 0 (a real change 10 -> 0).
    DependencyObject obj;
    const char* name = "QueuedClampNeg_S046";
    PropertyKey key(name, ValueType::Int32, "S046Owner", PropertyFlags::None);
    int fire = 0;
    PropertyMetadata md = PropertyMetadata::WithDefault(Value::Int32(10), PropertyFlags::None);
    md = std::move(md)
             .with_coerce([](const Value& v) {
                 const std::int32_t n = v.get<std::int32_t>();
                 return Value::Int32(n < 0 ? 0 : n);
             })
             .with_changed([&fire](const Value&, const Value&) { ++fire; });
    (void)obj.register_property(std::move(key), std::move(md));
    const PropertyId id = PropertyId::of(name);

    // Non-affine Set(-5): coerce -> 0 at queue time, POSTED; default 10 -> Get
    // still 10.
    SetResult r = SetResult::NO_CHANGE;
    setFromOtherThread(obj, id, Value::Int32(-5), &r);
    SHOULD(ShouldlyString(r).ShouldBe(SetResult::QUEUED));
    SHOULD(ShouldlyString(obj.Get(id)).ShouldBe(Value::Int32(10)));
    SHOULD(ShouldlyString(obj.pending_change_count()).ShouldBe(1u));

    // Drain: applies the COERCED 0 (not -5) — a real change 10 -> 0, cb fires.
    obj.ProcessPendingChanges();
    SHOULD(ShouldlyString(obj.Get(id)).ShouldBe(Value::Int32(0)));
    SHOULD(ShouldlyString(obj.pending_change_count()).ShouldBe(0u));
    SHOULD(ShouldlyString(fire).ShouldBe(1));
}
