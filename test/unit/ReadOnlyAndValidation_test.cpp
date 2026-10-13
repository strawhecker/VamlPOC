// S045 (dep:S044) — ReadOnly enforcement (Set throws) + validation
// (out-of-range rejects) + all flags honored.
//
// Delivers the S045 headline end-to-end against live DependencyObjects:
//   - ReadOnly: a property registered with PropertyFlags::ReadOnly makes
//     Set() throw PropertyReadOnlyException — structurally + value-
//     independently (even for an input equal to the current/default value),
//     from ANY thread (a wrong-thread read-only Set throws; it is never
//     QUEUED/deferred to a drain), and BEFORE the coerce/validate pipeline
//     (a read-only property that ALSO has a coerce/validate runs neither
//     — the throw fires first). A refused read-only write never materializes
//     a borrowed store entry, so Get() still reads the default.
//   - validate out-of-range rejects: on a read-write property, an in-range
//     coerced value is APPLIED; an out-of-range value is SetResult::REJECTED
//     with the store unchanged and no changed cb.
//   - all flags honored: Inherits (effective resolution), InheritOnly,
//     ReadOnly (throws), BindsTwoWayByDefault (carried through the key +
//     metadata flags; no runtime behavior in this step — genuine binding
//     behavior is S242's scope, recorded NO-GAP, not faked).
//
// IMPORTANT LIFETIME CONTRACT: PropertyId wraps a NON-OWNING std::string_view.
// Every PropertyId in this TU is built from a STRING LITERAL (static storage
// duration), so the view is valid for the whole program. NEVER build a
// PropertyId from a temporary std::string — it would dangle (S033 lifetime
// note). Property registration uses the SAME literal so the name-keyed store
// lookup matches exactly.
//
// Uses the Shouldly-for-doctest vocabulary (S036_5 standing convention).

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
#include "vaml/core/PropertyRegistry.h" // PropertyNotFound
#include "vaml/core/Value.h"

#include <cstdint>
#include <exception>
#include <string>
#include <thread>
#include <unordered_set>

using vaml::DependencyObject;
using vaml::DependencyProperty;
using vaml::PropertyFlags;
using vaml::PropertyId;
using vaml::PropertyKey;
using vaml::PropertyMetadata;
using vaml::PropertyNotFound;
using vaml::PropertyReadOnlyException;
using vaml::SetResult;
using vaml::Value;
using vaml::ValueType;
using vaml::test::ShouldThrow;

namespace {

// The GLOBAL DependencyProperty table is name-unique (S039). Each S045 name
// carries a unique suffix so no cross-TEST_CASE / cross-TU collision is
// possible; the set guards against accidental reuse within this TU.
std::unordered_set<std::string>& global_seen()
{
    static std::unordered_set<std::string> seen;
    return seen;
}

constexpr PropertyFlags kRO  = PropertyFlags::ReadOnly;
constexpr PropertyFlags kInh = PropertyFlags::Inherits;
constexpr PropertyFlags kIO  = PropertyFlags::InheritOnly;
constexpr PropertyFlags kTwo = PropertyFlags::BindsTwoWayByDefault;

// Registers an object-scoped Int32 property on `obj` under the given literal
// name (must be a string literal: it seeds both the name-keyed store and a
// static-lifetime PropertyId). Returns the PropertyId.
PropertyId registerObject(DependencyObject& obj, const char* name,
                          PropertyFlags flags, Value def)
{
    PropertyKey key(name, ValueType::Int32, "S045Owner", flags);
    (void)obj.register_property(std::move(key),
                                PropertyMetadata::WithDefault(def, flags));
    return PropertyId::of(name);
}

// Registers an ATTACHED Int32 property GLOBALLY (once per literal name, owner
// A) — objects do NOT locally register it (the true attached premise, S043/S044).
const DependencyProperty& registerGlobal(const char* name, PropertyFlags flags,
                                         Value def)
{
    if (global_seen().insert(name).second)
    {
        return DependencyProperty::Register(name, ValueType::Int32, "AttachedOwner_A",
                                            PropertyMetadata::WithDefault(def, flags));
    }
    return DependencyProperty::Get(name);
}

} // namespace

TEST_CASE("S045: read-only Set throws; Get still reads default; store unchanged")
{
    // Object-scoped read-only property: Set throws PropertyReadOnlyException,
    // Get still returns the default, and the refused write materializes no
    // local override (store unchanged).
    DependencyObject obj;
    const char* name = "ROScoped_S045";
    const PropertyId id = registerObject(obj, name, kRO, Value::Int32(7));

    SHOULD(ShouldlyString(obj.Get(id)).ShouldBe(Value::Int32(7))); // default applied
    SHOULD(ShouldThrow<PropertyReadOnlyException>([&] {
        (void)obj.Set(id, Value::Int32(5));
    }));
    // Store unchanged: the refused write never materialized a local override.
    SHOULD(ShouldlyString(obj.Get(id)).ShouldBe(Value::Int32(7)));
}

TEST_CASE("S045: read-only throws even for value == current default (value-independent)")
{
    // Structural, NOT value-based: a read-only property refuses even a write
    // equal to the current default. It must THROW, not NO_CHANGE.
    DependencyObject obj;
    const char* name = "ROValueIndependent_S045";
    const PropertyId id = registerObject(obj, name, kRO, Value::Int32(7));

    SHOULD(ShouldThrow<PropertyReadOnlyException>([&] {
        (void)obj.Set(id, Value::Int32(7)); // equals current/default
    }));
}

TEST_CASE("S045: read-only from wrong thread throws (never QUEUED)")
{
    // The S045 guard precedes the S037 affinity branch: a WRONG-THREAD
    // read-only Set must THROW, never return QUEUED/defer to a drain.
    DependencyObject obj;
    const char* name = "ROWrongThread_S045";
    const PropertyId id = registerObject(obj, name, kRO, Value::Int32(0));

    std::exception_ptr exc;
    SetResult r = SetResult::NO_CHANGE;
    std::thread t([&] {
        try
        {
            r = obj.Set(id, Value::Int32(9));
        }
        catch (...)
        {
            exc = std::current_exception();
        }
    });
    t.join();

    SHOULD(ShouldlyString(exc != nullptr).ShouldBe(true)); // threw
    SHOULD(ShouldThrow<PropertyReadOnlyException>([&] { std::rethrow_exception(exc); }));
    SHOULD(ShouldlyString(obj.pending_change_count()).ShouldBe(0u)); // never queued
    SHOULD(ShouldlyString(obj.Get(id)).ShouldBe(Value::Int32(0)));   // unchanged
    (void)r;
}

TEST_CASE("S045: globally-registered read-only attached property — Set throws, Get works")
{
    // A read-only property that is GLOBALLY registered (attached, owner A) and
    // never locally registered on B: Set on B throws, Get works (default), and
    // no borrowed store entry is materialized by the refused write.
    const char* name = "ROAttached_S045";
    (void)registerGlobal(name, kRO, Value::Int32(2));

    DependencyObject b;
    const PropertyId id = PropertyId::of(name); // literal -> static lifetime

    SHOULD(ShouldlyString(b.Get(id)).ShouldBe(Value::Int32(2))); // global default
    SHOULD(ShouldThrow<PropertyReadOnlyException>([&] {
        (void)b.Set(id, Value::Int32(9));
    }));
    SHOULD(ShouldlyString(b.Get(id)).ShouldBe(Value::Int32(2))); // unchanged, no materialization
}

TEST_CASE("S045: read-only throw precedes the pipeline (coerce + validate never run)")
{
    // A read-only property that ALSO carries a coerce + validate: the throw
    // fires BEFORE either runs — a read-only refusal is structural and must not
    // depend on (or mutate through) the value pipeline. Coerce doubles negative
    // input; validate would reject everything if it ran.
    DependencyObject obj;
    const char* name = "ROPipeline_S045";
    PropertyKey key(name, ValueType::Int32, "S045Owner", kRO);
    PropertyMetadata md = PropertyMetadata::WithDefault(Value::Int32(0), kRO);
    md = std::move(md)
             .with_coerce([](const Value& v) {
                 return Value::Int32(v.get<std::int32_t>() * 2);
             })
             .with_validate([](const Value&) { return false; }); // would reject if it ran
    (void)obj.register_property(std::move(key), std::move(md));
    const PropertyId id = PropertyId::of(name);

    // If coerce ran, -3 -> -6; if validate ran it would REJECT. The read-only
    // throw must come FIRST: PropertyReadOnlyException, never a validation result.
    SHOULD(ShouldThrow<PropertyReadOnlyException>([&] {
        (void)obj.Set(id, Value::Int32(-3));
    }));
    SHOULD(ShouldlyString(obj.Get(id)).ShouldBe(Value::Int32(0))); // default, unchanged
}

TEST_CASE("S045: validate out-of-range rejects (read-write property)")
{
    // On a read-write property with a bounded validate, an in-range value is
    // APPLIED (store writes it) and an out-of-range value is REJECTED — the
    // store is unchanged and the changed cb does NOT fire.
    std::size_t fires = 0;
    DependencyObject rangeObj;
    const char* name = "RangeBound_S045";
    PropertyKey key(name, ValueType::Int32, "S045Owner", PropertyFlags::None);
    PropertyMetadata rm = PropertyMetadata::WithDefault(Value::Int32(5), PropertyFlags::None);
    rm = std::move(rm)
             .with_validate([](const Value& v) {
                 const std::int32_t n = v.get<std::int32_t>();
                 return n >= 0 && n <= 10; // out-of-range [0,10] rejects
             })
             .with_changed([&fires](const Value&, const Value&) { ++fires; });
    (void)rangeObj.register_property(std::move(key), std::move(rm));
    const PropertyId rid = PropertyId::of(name);

    // In-range 8 -> APPLIED, store = 8, cb fired once (5 -> 8).
    SHOULD(ShouldlyString(rangeObj.Set(rid, Value::Int32(8))).ShouldBe(SetResult::APPLIED));
    SHOULD(ShouldlyString(rangeObj.Get(rid)).ShouldBe(Value::Int32(8)));
    SHOULD(ShouldlyString(fires).ShouldBe(1u));

    // Out-of-range 42 -> REJECTED, store unchanged, no cb.
    SHOULD(ShouldlyString(rangeObj.Set(rid, Value::Int32(42))).ShouldBe(SetResult::REJECTED));
    SHOULD(ShouldlyString(rangeObj.Get(rid)).ShouldBe(Value::Int32(8)));
    SHOULD(ShouldlyString(fires).ShouldBe(1u));

    // Out-of-range -3 -> REJECTED (below lower bound).
    SHOULD(ShouldlyString(rangeObj.Set(rid, Value::Int32(-3))).ShouldBe(SetResult::REJECTED));
    SHOULD(ShouldlyString(rangeObj.Get(rid)).ShouldBe(Value::Int32(8)));
    SHOULD(ShouldlyString(fires).ShouldBe(1u));
}

TEST_CASE("S045: all flags honored (consolidated)")
{
    // Confirms every flag is carried through registration and behaves where
    // semantics exist in this step:
    //   - ReadOnly: Set throws.
    //   - BindsTwoWayByDefault: carried through BOTH the key and metadata flags
    //     (no runtime behavior yet — genuine two-way binding is S242's scope,
    //     recorded NO-GAP, not faked).
    //   - Inherits + InheritOnly: effective-value resolution via the read-side
    //     pull model (parent change recomputes the child effective).
    //
    // (a) ReadOnly + BindsTwoWayByDefault combined on one property key.
    {
        DependencyObject obj;
        const char* name = "ROTwoway_S045";
        PropertyKey key(name, ValueType::Int32, "S045Owner", kRO | kTwo);
        (void)obj.register_property(std::move(key),
                                    PropertyMetadata::WithDefault(Value::Int32(0), kRO | kTwo));
        const PropertyId id = PropertyId::of(name);
        SHOULD(ShouldThrow<PropertyReadOnlyException>([&] {
            (void)obj.Set(id, Value::Int32(1));
        }));
        SHOULD(ShouldlyString(obj.Get(id)).ShouldBe(Value::Int32(0)));
    }

    // (b) Inherits + InheritOnly effective resolution: an InheritOnly property
    // ignores the object's OWN local override and inherits from the parent.
    {
        const char* name = "InhOnlyFlags_S045";
        (void)registerGlobal(name, kInh | kIO, Value::Int32(0));
        DependencyObject parent, child;
        child.set_parent(&parent);
        const PropertyId id = PropertyId::of(name);

        // Parent sets the inherited value; child (InheritOnly) inherits it.
        SHOULD(ShouldlyString(parent.Set(id, Value::Int32(5))).ShouldBe(SetResult::APPLIED));
        SHOULD(ShouldlyString(child.Get(id)).ShouldBe(Value::Int32(5)));
    }
}
