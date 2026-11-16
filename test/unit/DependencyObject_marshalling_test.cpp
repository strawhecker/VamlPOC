// S037 (corrected by S811) — DependencyObject wrong-thread access THROWS tests
// (marshalling foundation, dep:S036_5). CONSUMES S036's thread-affinity marker
// (thread_affinity_id / is_affine_to_current_thread) as the basis for
// wrong-thread detection; S037 (corrected) makes a NON-affine Set()/Get()
// THROW WrongThreadException.
//
// S811 RETRACTS the original S037 title contract (wrong-thread write QUEUING —
// queued-not-applied + ProcessPendingChanges drain + SetResult::QUEUED) and the
// S041 fabricated change-notification pipeline. Proves the S811-corrected
// contract:
//   5. Set()/Get() are thread-affinity aware: any access from a thread other
//      than the object's creating (affinity) thread THROWS WrongThreadException
//      (mirror WPF DependencyObject.VerifyAccess, DO.cs:161/412/...). No value
//      is queued, no store mutation, no cb.
//
// Each test uses a join barrier so the demonstration is single-threaded-safe.
//
// This is the S037/S811 marshalling proof — distinct from DependencyObject_test.cpp
// (the S036_5 capstone proof).

// doctest's main comes from smoke_test.cpp (DO doctest IMPLEMENT_WITH_MAIN).
// This TU only declares tests — must NOT re-define implementation.
#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"       // S036_2 StringMaker + S036_4 operator<<
#include <vaml/assert/Shouldly.h>         // ShouldlyString / ShouldThrow (SHOULD seam, S036_1)

#include "vaml/core/DependencyObject.h"
#include "vaml/core/PropertyKey.h"

#include <exception>
#include <string>
#include <thread>

using vaml::DependencyObject;
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

PropertyKey int32Key(const std::string& name)
{
    return PropertyKey(name, ValueType::Int32, "TestOwner", PropertyFlags::None);
}

// A non-affine Set() on an Int32 property, executed on a fresh thread; the throw
// (WrongThreadException, or an S810_1 InvalidPropertyValueException for a
// validate-refused value) is captured into `excOut`. The value is NEVER applied
// (a wrong-thread Set always throws before touching the store).
void setFromOtherThread(DependencyObject& obj, const PropertyId& id, Value v, std::exception_ptr* excOut = nullptr)
{
    std::thread t([&] {
        try
        {
            (void)obj.Set(id, v);
            if (excOut != nullptr)
            {
                *excOut = nullptr;
            }
        }
        catch (...)
        {
            if (excOut != nullptr)
            {
                *excOut = std::current_exception();
            }
            else
            {
                throw;
            }
        }
    });
    t.join();
}

} // namespace

TEST_CASE("S037(S811): wrong-thread Set THROWS WrongThreadException, store unchanged")
{
    DependencyObject obj;
    obj.register_property(int32Key("Width"), PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None));

    // Affine (this) thread Set still applies normally.
    SHOULD(ShouldlyString(obj.Set(PropertyId::of("Width", "TestOwner"), Value::Int32(0))).ShouldBe(SetResult::NO_CHANGE));

    // Wrong-thread write throws WrongThreadException.
    std::exception_ptr exc;
    setFromOtherThread(obj, PropertyId::of("Width", "TestOwner"), Value::Int32(42), &exc);
    SHOULD(ShouldlyString(exc != nullptr).ShouldBe(true));
    SHOULD(ShouldThrow<WrongThreadException>([&] { std::rethrow_exception(exc); }));

    // Nothing applied: store unchanged.
    SHOULD(ShouldlyString(obj.Get(PropertyId::of("Width", "TestOwner"))).ShouldBe(Value::Int32(0)));
}

TEST_CASE("S037(S811): wrong-thread Get THROWS WrongThreadException")
{
    DependencyObject obj;
    obj.register_property(int32Key("W"), PropertyMetadata::WithDefault(Value::Int32(7), PropertyFlags::None));

    // A wrong-thread read throws (WPF VerifyAccess, DO.cs:161) — there is no
    // cross-thread read.
    bool caught = false;
    std::thread t([&] {
        try
        {
            (void)obj.Get(PropertyId::of("W", "TestOwner"));
        }
        catch (WrongThreadException const&)
        {
            caught = true;
        }
    });
    t.join();
    SHOULD(ShouldlyString(caught).ShouldBe(true));

    // Affine read still works.
    SHOULD(ShouldlyString(obj.Get(PropertyId::of("W", "TestOwner"))).ShouldBe(Value::Int32(7)));
}

TEST_CASE("S037(S811): a WRONG-THREAD Set on a value equal to the current effective STILL THROWS (no no-op deferral)")
{
    DependencyObject obj;
    int fire = 0;
    obj.register_property(
        int32Key("W"),
        PropertyMetadata::WithDefault(Value::Int32(3), PropertyFlags::None)
            .with_changed([&](const Value&, const Value&) { ++fire; }));

    // A wrong-thread Set throws regardless of value equality — there is no
    // queued-not-applied judgement; the access itself is refused.
    std::exception_ptr exc;
    setFromOtherThread(obj, PropertyId::of("W", "TestOwner"), Value::Int32(3), &exc);
    SHOULD(ShouldlyString(exc != nullptr).ShouldBe(true));
    SHOULD(ShouldThrow<WrongThreadException>([&] { std::rethrow_exception(exc); }));

    SHOULD(ShouldlyString(fire).ShouldBe(0)); // no cb (nothing applied)
    SHOULD(ShouldlyString(obj.Get(PropertyId::of("W", "TestOwner"))).ShouldBe(Value::Int32(3)));
}

TEST_CASE("S037(S811): affine Set applies normally; wrong-thread Set never applies")
{
    DependencyObject obj;
    obj.register_property(int32Key("W"), PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None));

    // Affine Set is applied.
    SHOULD(ShouldlyString(obj.Set(PropertyId::of("W", "TestOwner"), Value::Int32(1))).ShouldBe(SetResult::APPLIED));
    SHOULD(ShouldlyString(obj.Get(PropertyId::of("W", "TestOwner"))).ShouldBe(Value::Int32(1)));

    // A wrong-thread write throws; nothing is applied.
    std::exception_ptr exc;
    setFromOtherThread(obj, PropertyId::of("W", "TestOwner"), Value::Int32(7), &exc);
    SHOULD(ShouldlyString(exc != nullptr).ShouldBe(true));
    SHOULD(ShouldThrow<WrongThreadException>([&] { std::rethrow_exception(exc); }));
    SHOULD(ShouldlyString(obj.Get(PropertyId::of("W", "TestOwner"))).ShouldBe(Value::Int32(1)));
}
