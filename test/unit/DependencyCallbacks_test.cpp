// S046 (dep:S045) — DependencyProperty coercion + per-set validation callbacks,
// applied end-to-end against live DependencyObjects via the Shouldly
// vocabulary (S036_5 standing convention).
//
// CONTEXT / SCOPE DECISION (recorded, NOT silent):
//   The original S046 roadmap title was "registration-time dependency
//   validation (ValidateDependencies)". Reviewed against real commercial WPF
//   MVVM, this was REDIRECTED (user-accepted) to the WPF-faithful value hooks —
//   coercion + per-set validation — which are what DPs actually ship for
//   property coupling. The validate(raw) -> coerce -> validate(coerced) -> fire-on-real-change
//   -> store pipeline - the WPF order (DO.cs:653 raw validate, DO.cs:1692
//   coerce + re-validate) - is ALREADY implemented in DependencyObject::Set /
//   ProcessPendingChanges (S036/S037/S045, ordering corrected by S810/D1) and the callbacks already live in
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
using vaml::InvalidPropertyValueException;
using vaml::Value;
using vaml::ValueType;
using vaml::ValueTypeMismatch;
using vaml::WrongThreadException;
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
    return DependencyProperty::Get(name, "AttachedOwner_A");
}

// A non-affine Set() on an Int32 property, executed on a fresh thread. S811: a
// wrong-thread Set always THROWS WrongThreadException (WPF VerifyAccess); the
// throw is captured into `excOut` by a join barrier (no concurrency).
void setFromOtherThreadThrows(DependencyObject& obj, const PropertyId& id, Value v,
                              std::exception_ptr& excOut)
{
    std::thread t([&] {
        try
        {
            (void)obj.Set(id, v);
        }
        catch (...)
        {
            excOut = std::current_exception();
        }
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
    const PropertyId id = PropertyId::of(name, "AttachedOwner_A");

    DependencyObject b; // never locally registers the property

    // Raw 150 -> coerced to 100, validate(100) passes -> APPLIED.
    SHOULD(ShouldlyString(b.Set(id, Value::Int32(150))).ShouldBe(SetResult::APPLIED));
    SHOULD(ShouldlyString(b.Get(id)).ShouldBe(Value::Int32(100))); // the COERCED value stored

    // Raw -5 -> coerced to 0 (a real change 100 -> 0) -> APPLIED.
    SHOULD(ShouldlyString(b.Set(id, Value::Int32(-5))).ShouldBe(SetResult::APPLIED));
    SHOULD(ShouldlyString(b.Get(id)).ShouldBe(Value::Int32(0))); // coerced 0

    // Raw 7 -> coerced 7, validate(7) rejects -> THROWS
    // InvalidPropertyValueException (S810_1: WPF ArgumentException(
    // InvalidPropertyValue) at DO.cs:1692 for the post-coerce re-check;
    // SetResult::REJECTED retired), store unchanged.
    SHOULD(ShouldThrow<InvalidPropertyValueException>([&] {
        (void)b.Set(id, Value::Int32(7));
    }));
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
    const PropertyId id = PropertyId::of(name, "AttachedOwner_A");

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
    const PropertyId id = PropertyId::of(name, "AttachedOwner_A");

    DependencyObject b; // never locally registers
    SHOULD(ShouldlyString(b.Set(id, Value::Int32(42))).ShouldBe(SetResult::APPLIED));
    SHOULD(ShouldlyString(b.Get(id)).ShouldBe(Value::Int32(42)));
    SHOULD(ShouldlyString(b.Set(id, Value::Int32(42))).ShouldBe(SetResult::NO_CHANGE)); // repeat
    SHOULD(ShouldlyString(b.Get(id)).ShouldBe(Value::Int32(42)));
}

TEST_CASE("S046: wrong-thread coerced Set THROWS (S811 — coercing a wrong-thread write is refused, not deferred)")
{
    // S811: a non-affine Set ALWAYS THROWS WrongThreadException (WPF
    // VerifyAccess, DO.cs:412) — there is no queued-not-applied, no drain to
    // store a "coerced" value. Coerce/validate do NOT run for a wrong-thread
    // write either (the affinity guard precedes the pipeline). This test keeps
    // the S046 coercer on the property and proves a wrong-thread write is
    // refused outright; an AFFINE Set still coerces normally.
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
    const PropertyId id = PropertyId::of(name, "S046Owner");

    // Affine Set(50) -> applied 50; cb fired once (default 10 -> 50).
    SHOULD(ShouldlyString(obj.Set(id, Value::Int32(50))).ShouldBe(SetResult::APPLIED));
    SHOULD(ShouldlyString(obj.Get(id)).ShouldBe(Value::Int32(50)));
    SHOULD(ShouldlyString(fire).ShouldBe(1));

    // Non-affine Set(150): THROWS WrongThreadException; Get still 50 (nothing
    // applied); no cb.
    std::exception_ptr exc;
    setFromOtherThreadThrows(obj, id, Value::Int32(150), exc);
    SHOULD(ShouldlyString(exc != nullptr).ShouldBe(true));
    SHOULD(ShouldThrow<WrongThreadException>([&] { std::rethrow_exception(exc); }));
    SHOULD(ShouldlyString(obj.Get(id)).ShouldBe(Value::Int32(50)));
    SHOULD(ShouldlyString(fire).ShouldBe(1));

    // An AFFINE Set(150) still coerces to 100 and fires the changed cb.
    SHOULD(ShouldlyString(obj.Set(id, Value::Int32(150))).ShouldBe(SetResult::APPLIED));
    SHOULD(ShouldlyString(obj.Get(id)).ShouldBe(Value::Int32(100))); // coerced
    SHOULD(ShouldlyString(fire).ShouldBe(2));
    SHOULD(ShouldlyString(oldV).ShouldBe(50));
    SHOULD(ShouldlyString(newV).ShouldBe(100)); // the coerced delta, not raw 150
}

TEST_CASE("S046: wrong-thread sub-zero Set THROWS (S811); an AFFINE sub-zero Set still coerces to 0")
{
    // S811: a wrong-thread write is refused outright (no queue to clamp into).
    // An AFFINE Set(-5) still runs coerce -> 0 (a real change 10 -> 0, cb
    // fires).
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
    const PropertyId id = PropertyId::of(name, "S046Owner");

    // Non-affine Set(-5): THROWS WrongThreadException; Get still 10.
    std::exception_ptr exc;
    setFromOtherThreadThrows(obj, id, Value::Int32(-5), exc);
    SHOULD(ShouldlyString(exc != nullptr).ShouldBe(true));
    SHOULD(ShouldThrow<WrongThreadException>([&] { std::rethrow_exception(exc); }));
    SHOULD(ShouldlyString(obj.Get(id)).ShouldBe(Value::Int32(10)));
    SHOULD(ShouldlyString(fire).ShouldBe(0));

    // Affine Set(-5): coerces to 0, a real change 10 -> 0, cb fires once.
    SHOULD(ShouldlyString(obj.Set(id, Value::Int32(-5))).ShouldBe(SetResult::APPLIED));
    SHOULD(ShouldlyString(obj.Get(id)).ShouldBe(Value::Int32(0)));
    SHOULD(ShouldlyString(fire).ShouldBe(1));
}


TEST_CASE("S046: raw invalid-but-coerce-normalizable THROWS at the boundary (WPF DO.cs:653)")
{
    // D1 (S810) pipeline-order proof: WPF validates the RAW input BEFORE
    // coercion (DO.cs:653) and throws at the boundary - coercion is NOT given
    // a chance to normalize an invalid raw value. Setup: a property whose
    // validate REJECTS any raw value below a floor of 0, but whose coerce
    // WOULD clamp a negative raw to 0 (the value the OLD coerce-first order
    // would have silently accepted). Under the corrected order the raw negative
    // THROWS InvalidPropertyValueException at the boundary: store unchanged, no
    // changed cb.
    DependencyObject obj;
    const char* name = "RawBoundary_S046";
    PropertyKey key(name, ValueType::Int32, "S046Owner", PropertyFlags::None);
    int fire = 0;
    PropertyMetadata md = PropertyMetadata::WithDefault(Value::Int32(10), PropertyFlags::None);
    md = std::move(md)
             .with_validate([](const Value& v) { return v.get<std::int32_t>() >= 0; })
             .with_coerce([](const Value& v) {
                 const std::int32_t n = v.get<std::int32_t>();
                 return Value::Int32(n < 0 ? 0 : n); // clamps negatives to 0
             })
             .with_changed([&fire](const Value&, const Value&) { ++fire; });
    (void)obj.register_property(std::move(key), std::move(md));
    const PropertyId id = PropertyId::of(name, "S046Owner");

    // Raw -5 is INVALID on entry (validate(-5) rejects), even though coerce
    // could normalize it to 0. WPF throws at the boundary (DO.cs:653): the
    // store is unchanged (still default 10) and the changed cb NEVER fires.
    // S810_1: the boundary refusal is a THROWN InvalidPropertyValueException
    // (SetResult::REJECTED retired as the out-of-range disposition).
    SHOULD(ShouldThrow<InvalidPropertyValueException>([&] {
        (void)obj.Set(id, Value::Int32(-5));
    }));
    SHOULD(ShouldlyString(obj.Get(id)).ShouldBe(Value::Int32(10))); // unchanged
    SHOULD(ShouldlyString(fire).ShouldBe(0));                        // no cb

    // Cross-check the split stays coherent: a raw value that IS valid on entry
    // (>= 0) AND valid after coerce still applies normally - coerce runs and
    // stores the COERCED value (WPF DO.cs:1692).
    SHOULD(ShouldlyString(obj.Set(id, Value::Int32(7))).ShouldBe(SetResult::APPLIED));
    SHOULD(ShouldlyString(obj.Get(id)).ShouldBe(Value::Int32(7)));
    SHOULD(ShouldlyString(fire).ShouldBe(1)); // real change 10 -> 7 fired once
}
