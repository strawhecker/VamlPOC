// S035 — PropertyMetadata apply-semantics tests (dep:S034).
//
// Scope: verifies the *Set-time pipeline* semantics that the S034 metadata
// bundle enables, exactly as S035's title lists them:
//   1. default applied        — a fresh holder reads its defaultValue();
//   2. cb fires only on real change — assigning the same value does NOT fire;
//   3. coerce applied         — the stored value is the *coerced* value;
//   4. validate rejects       — a rejected value is refused, cb not fired.
// Plus the ordering contract (coerce runs BEFORE validate, so validate sees
// the coerced value) and the passthrough case (no callbacks = store-as-given).
//
// Why a harness: the real value-holder is S036's DependencyObject. S035 is a
// pure TEST step by plan — it delivers NO product symbol. The doctest-scope
// MiniStore<T> below is test infrastructure (same class as the vendored
// Shouldly harness), NOT a feature symbol, so it carries no MINIMAL REAL
// sentinel and is never shipped. It models the documented apply pipeline and
// stands as the *reference spec* for S036's DependencyObject::Set(), which
// must reproduce these same outcomes.
//
// Contract to S036 (recorded in roadmap/progress so it is searchable, not
// silent): DependencyObject::Set must apply coerce -> validate (on the
// coerced value) -> fire changed cb (only on a real change) -> store.

// doctest's main comes from smoke_test.cpp (DO doctest IMPLEMENT_WITH_MAIN).
// This TU only declares tests — must NOT re-define implementation.
#include "doctest/doctest.h"

#include "vaml/core/PropertyMetadata.h"

#include <utility>

using vaml::PropertyFlags;
using vaml::PropertyMetadata;
using vaml::Value;
using vaml::ValueType;
using vaml::ValueTypeMismatch;

namespace {

// ---------------------------------------------------------------------------
// MiniStore<T> — a minimal local value-holder modeling S036's Set() pipeline.
//
// It owns a PropertyMetadata<T> plus a `current` value. `current` is seeded
// from the metadata's defaultValue() at construction == "default applied".
// trySet(proposed) runs the documented pipeline and reports the outcome:
//   REJECTED  — validate refused the (coerced) value; store unchanged, no cb.
//   NO_CHANGE — coerced == current; no cb (cb fires ONLY on a real change).
//   APPLIED   — value accepted and different; cb fired with (old,new); stored.
//
// It also records every cb invocation (fireCount + last (old,new)) so tests
// can assert exact call behavior. It is local to this TU.
// ---------------------------------------------------------------------------
template <typename T>
class MiniStore
{
public:
    enum class Outcome
    {
        APPLIED,
        REJECTED,
        NO_CHANGE,
    };

    explicit MiniStore(PropertyMetadata md)
        : md_(std::move(md)),
          current_(md_.defaultValue()) // "default applied" at construction
    {
    }

    // Runs the pipeline: coerce -> validate(coerced) -> fire-on-real-change -> store.
    Outcome trySet(const Value& proposed)
    {
        const Value coerced = md_.coerce(proposed);
        if (!md_.validate(coerced))
        {
            return Outcome::REJECTED; // store unchanged, NO cb
        }
        if (coerced == current_)
        {
            return Outcome::NO_CHANGE; // real change only => no cb
        }
        md_.notifyChanged(current_, coerced); // cb fires with (old,new)
        current_ = coerced;
        return Outcome::APPLIED;
    }

    // Installs a changed cb that records (old,new) and counts fires.
    PropertyMetadata& attachRecorder(std::int32_t& oldOut, std::int32_t& newOut, int& fireCount)
    {
        md_ = std::move(md_).with_changed([&](const Value& o, const Value& n) {
            oldOut     = o.get<std::int32_t>();
            newOut     = n.get<std::int32_t>();
            ++fireCount;
        });
        return md_;
    }

    const Value& current() const noexcept { return current_; }
    const PropertyMetadata& metadata() const noexcept { return md_; }

private:
    PropertyMetadata md_;
    Value current_;
};

} // namespace

// Convenience builder: a metadata bound to Int32 with an optional coerce that
// clamps negatives to 0 and an optional validate that requires > 0.
static PropertyMetadata int32Md(
    std::int32_t defaultValue,
    bool withCoerce,
    bool withValidate,
    bool requirePositive = true)
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
        md = std::move(md).with_validate([requirePositive](const Value& v) {
            return requirePositive ? (v.get<std::int32_t>() > 0) : (v.get<std::int32_t>() != 0);
        });
    }
    return md;
}

TEST_CASE("S035 apply: default is applied at construction (fresh holder reads defaultValue)")
{
    MiniStore<std::int32_t> store(int32Md(7, false, false));
    CHECK(store.current() == Value::Int32(7)); // "default applied"

    // Setting a value equal to the default is not a real change -> NO_CHANGE, no cb.
    int fire = 0;
    std::int32_t oldV = -1, newV = -1;
    store.attachRecorder(oldV, newV, fire);
    CHECK(store.trySet(Value::Int32(7)) == MiniStore<std::int32_t>::Outcome::NO_CHANGE);
    CHECK(store.current() == Value::Int32(7));
    CHECK(fire == 0);
}

TEST_CASE("S035 apply: coerce is applied — stored value is the coerced value, cb sees coerced new")
{
    MiniStore<std::int32_t> store(int32Md(0, /*withCoerce*/ true, /*withValidate*/ false));
    int fire = 0;
    std::int32_t oldV = -1, newV = -1;
    store.attachRecorder(oldV, newV, fire);

    // Raw -5 is coerced to 0, but the store already holds default 0 -> NO_CHANGE.
    CHECK(store.trySet(Value::Int32(-5)) == MiniStore<std::int32_t>::Outcome::NO_CHANGE);
    CHECK(store.current() == Value::Int32(0)); // default preserved
    CHECK(fire == 0);

    // Raw 3 passes through -> stored 3, cb fires with (old=0, new=3).
    CHECK(store.trySet(Value::Int32(3)) == MiniStore<std::int32_t>::Outcome::APPLIED);
    CHECK(store.current() == Value::Int32(3));
    CHECK(fire == 1);
    CHECK(oldV == 0);
    CHECK(newV == 3);

    // Raw 8 -> stored 8 (coerced identity for positives).
    CHECK(store.trySet(Value::Int32(8)) == MiniStore<std::int32_t>::Outcome::APPLIED);
    CHECK(store.current() == Value::Int32(8));
    CHECK(fire == 2);
    CHECK(oldV == 3);
    CHECK(newV == 8);
}

TEST_CASE("S035 apply: coerce runs BEFORE validate — validate sees the coerced value")
{
    // coerce: negative -> 5 (a *distinct* positive value); validate: requires >= 0.
    // Raw -3 is negative, so it would FAIL validate (>= 0) as-is. But the
    // pipeline runs coerce FIRST, turning -3 into 5, and validate(5) passes —
    // so the set is APPLIED with a real change (default 0 -> 5). This proves
    // coerce-before-validate ordering: validate sees the coerced value.
    PropertyMetadata md = PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None)
        .with_coerce([](const Value& v) {
            return Value::Int32(v.get<std::int32_t>() < 0 ? 5 : v.get<std::int32_t>());
        })
        .with_validate([](const Value& v) { return v.get<std::int32_t>() >= 0; });

    MiniStore<std::int32_t> store(std::move(md));
    int fire = 0;
    std::int32_t oldV = -1, newV = -1;
    store.attachRecorder(oldV, newV, fire);

    // Raw -3 would fail validate on its own; coercion to 5 makes it pass, and
    // 0->5 is a real change -> APPLIED. Proves coerce-before-validate ordering.
    CHECK(store.trySet(Value::Int32(-3)) == MiniStore<std::int32_t>::Outcome::APPLIED);
    CHECK(store.current() == Value::Int32(5)); // the COERCED value is validated+stored
    CHECK(fire == 1);
    CHECK(oldV == 0); // default
    CHECK(newV == 5); // coerced new
}

TEST_CASE("S035 apply: validate rejects — store unchanged, cb NOT fired")
{
    // validate requires > 0; default 1.
    MiniStore<std::int32_t> store(int32Md(1, /*withCoerce*/ false, /*withValidate*/ true));
    int fire = 0;
    std::int32_t oldV = -1, newV = -1;
    store.attachRecorder(oldV, newV, fire);

    // -3 rejected: current stays 1, no cb.
    CHECK(store.trySet(Value::Int32(-3)) == MiniStore<std::int32_t>::Outcome::REJECTED);
    CHECK(store.current() == Value::Int32(1));
    CHECK(fire == 0);

    // 0 rejected (not > 0): current stays 1, no cb.
    CHECK(store.trySet(Value::Int32(0)) == MiniStore<std::int32_t>::Outcome::REJECTED);
    CHECK(store.current() == Value::Int32(1));
    CHECK(fire == 0);

    // Valid 5 -> applied.
    CHECK(store.trySet(Value::Int32(5)) == MiniStore<std::int32_t>::Outcome::APPLIED);
    CHECK(store.current() == Value::Int32(5));
    CHECK(fire == 1);
    CHECK(oldV == 1);
    CHECK(newV == 5);
}

TEST_CASE("S035 apply: cb fires ONLY on a real change")
{
    MiniStore<std::int32_t> store(int32Md(0, /*withCoerce*/ false, /*withValidate*/ false));
    int fire = 0;
    std::int32_t oldV = -1, newV = -1;
    store.attachRecorder(oldV, newV, fire);

    // Setting the same value repeatedly never fires.
    CHECK(store.trySet(Value::Int32(9)) == MiniStore<std::int32_t>::Outcome::APPLIED);
    CHECK(fire == 1);
    CHECK(store.trySet(Value::Int32(9)) == MiniStore<std::int32_t>::Outcome::NO_CHANGE);
    CHECK(store.trySet(Value::Int32(9)) == MiniStore<std::int32_t>::Outcome::NO_CHANGE);
    CHECK(fire == 1); // unchanged after second/third identical set

    // Distinct values each fire once with the correct (old,new).
    CHECK(store.trySet(Value::Int32(11)) == MiniStore<std::int32_t>::Outcome::APPLIED);
    CHECK(fire == 2);
    CHECK(oldV == 9);
    CHECK(newV == 11);

    CHECK(store.trySet(Value::Int32(15)) == MiniStore<std::int32_t>::Outcome::APPLIED);
    CHECK(fire == 3);
    CHECK(oldV == 11);
    CHECK(newV == 15);

    // Round-trip back to a previously held value is still a real change.
    CHECK(store.trySet(Value::Int32(9)) == MiniStore<std::int32_t>::Outcome::APPLIED);
    CHECK(fire == 4);
    CHECK(oldV == 15);
    CHECK(newV == 9);
}

TEST_CASE("S035 apply: absent callbacks = passthrough, no fires")
{
    // No coerce/validate/changed anywhere.
    PropertyMetadata md = PropertyMetadata::WithDefault(Value::Int32(10), PropertyFlags::None);
    MiniStore<std::int32_t> store(std::move(md));

    CHECK_FALSE(store.metadata().has_coerce());
    CHECK_FALSE(store.metadata().has_validate());
    CHECK_FALSE(store.metadata().has_changed());

    int fire = 0;
    std::int32_t oldV = -1, newV = -1;
    store.attachRecorder(oldV, newV, fire);

    // Raw value stored as-given (identity coerce, always-accept validate).
    CHECK(store.trySet(Value::Int32(42)) == MiniStore<std::int32_t>::Outcome::APPLIED);
    CHECK(store.current() == Value::Int32(42));
    CHECK(fire == 1);

    // Same value again -> no change, no fire.
    CHECK(store.trySet(Value::Int32(42)) == MiniStore<std::int32_t>::Outcome::NO_CHANGE);
    CHECK(store.current() == Value::Int32(42));
    CHECK(fire == 1);
}

TEST_CASE("S035 apply: harness stays type-bound — wrong-tag proposed value throws")
{
    // The harness drives metadata.coerce/validate, so a wrong-tag proposed
    // value surfaces the S030 (S034) ValueTypeMismatch contract through the
    // pipeline — before any store/accept decision is made.
    MiniStore<std::int32_t> store(int32Md(0, false, false));
    CHECK_THROWS_AS((void)store.trySet(Value::Double(2.5)), ValueTypeMismatch);
    // Store untouched by the throw path.
    CHECK(store.current() == Value::Int32(0));
}
