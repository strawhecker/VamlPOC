// S007 — Shouldly.h compile + assertion round-trip tests (dep: S006).
//
// This file is the S006->S007 compile gate made real: it compiles
// <vaml/assert/Shouldly.h> as a genuine TU inside the vaml-core-tests target
// under the SAME strict warning-as-error policy as the library (MSVC /W4 /WX,
// Clang -Wall -Wextra -Wpedantic -Werror), and it exercises every preserved
// Shouldly predicate in BOTH directions:
//   - pass  : predicate holds -> no failure routed to the sink
//   - fail  : predicate fails  -> ReportFailure routes to the pluggable sink
//            (captured here, and asserted on fragment content)
//
// The "round-trip" is: predicate -> ThrowAwesomely* -> AssertStringStream ->
// ReportFailure -> failure_sink() -> (default throw | custom capture).
//
// S006 already proved the DEFAULT throw contract; this file adds the custom-sink
// round-trip via a thread-local capture-sink RAII helper, and a default-sink-throws
// test for the DSL contract. No product symbols are introduced; this file is
// permanent test infrastructure (NOT a STUB / NOT MINIMAL REAL).
//
// NOTE: doctest's implementation+main is owned by smoke_test.cpp
// (DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN lives there); this TU only declares/uses
// doctest, so main and the doctest globals are not duplicated at link time.
#include "doctest/doctest.h"

#include <cmath>
#include <stdexcept>
#include <string>

#include <vaml/assert/Shouldly.h>

using vaml::test::ShouldThrow;

namespace
{
    // Thread-local capture buffer the test-only sink appends to. failure_sink() is
    // itself thread-local, so one buffer per thread is correct and race-free.
    static thread_local std::string g_captured;

    void capture_sink(char const* const message)
    {
        g_captured += message;
    }

    // RAII: install the capture sink on scope entry, restore the prior sink on exit.
    // Captures the "round-trip" - a failing predicate lands its formatted message
    // here instead of throwing, so we can assert on message content.
    struct CaptureOn
    {
        vaml::test::FailureSink prior_;
        CaptureOn()
            : prior_(vaml::test::failure_sink())
        {
            g_captured.clear();
            vaml::test::failure_sink() = &capture_sink;
        }
        ~CaptureOn()
        {
            vaml::test::failure_sink() = prior_;
        }
        std::string const& text() const
        {
            return g_captured;
        }
        void clear()
        {
            g_captured.clear();
        }
    };
} // namespace

// -----------------------------------------------------------------------------
// ShouldlyVal (Integral) - pass direction
// -----------------------------------------------------------------------------
TEST_CASE("ShouldlyVal passes - no failure routed")
{
    CaptureOn cap;

    ShouldlyIntegral(5).ShouldBe(5);
    ShouldlyIntegral(5).ShouldNotBe(6);
    ShouldlyIntegral(5).ShouldBeGreaterThan(4);
    ShouldlyIntegral(5).ShouldBeLessThan(6);
    ShouldlyIntegral(5).ShouldBeGreaterThanOrEqualTo(5);
    ShouldlyIntegral(5).ShouldBeLessThanOrEqualTo(5);
    ShouldlyIntegral(5).ShouldBeInRangeTo(5, 1);
    ShouldlyIntegral(5).ShouldBeInRangeTo(5, 0);

    CHECK(cap.text().empty());
}

// -----------------------------------------------------------------------------
// ShouldlyVal (Integral) - fail direction, round-trip to the capture sink
// -----------------------------------------------------------------------------
TEST_CASE("ShouldlyVal fails - routed to sink with expected fragment")
{
    CaptureOn cap;

    SUBCASE("ShouldBe")
    {
        ShouldlyIntegral(5).ShouldBe(6);
        CHECK(cap.text().find("should be") != std::string::npos);
        CHECK(cap.text().find("equal") != std::string::npos);
    }
    SUBCASE("ShouldNotBe")
    {
        cap.clear();
        ShouldlyIntegral(5).ShouldNotBe(5);
        CHECK(cap.text().find("not equal") != std::string::npos);
    }
    SUBCASE("ShouldBeGreaterThan")
    {
        cap.clear();
        ShouldlyIntegral(5).ShouldBeGreaterThan(6);
        CHECK(cap.text().find("greater than") != std::string::npos);
    }
    SUBCASE("ShouldBeLessThan")
    {
        cap.clear();
        ShouldlyIntegral(5).ShouldBeLessThan(5);
        CHECK(cap.text().find("less than") != std::string::npos);
    }
    SUBCASE("ShouldBeGreaterThanOrEqualTo")
    {
        cap.clear();
        ShouldlyIntegral(5).ShouldBeGreaterThanOrEqualTo(6);
        CHECK(cap.text().find("greater than") != std::string::npos);
    }
    SUBCASE("ShouldBeLessThanOrEqualTo")
    {
        cap.clear();
        ShouldlyIntegral(5).ShouldBeLessThanOrEqualTo(4);
        CHECK(cap.text().find("less than") != std::string::npos);
    }
    SUBCASE("ShouldBeInRangeTo")
    {
        cap.clear();
        ShouldlyIntegral(10).ShouldBeInRangeTo(5, 1);
        CHECK(cap.text().find("range") != std::string::npos);
    }
}

// -----------------------------------------------------------------------------
// ShouldlyStr (String) - pass direction
// -----------------------------------------------------------------------------
TEST_CASE("ShouldlyStr passes - no failure routed")
{
    CaptureOn cap;

    ShouldlyString(std::string("hello")).ShouldBe(std::string("hello"));
    ShouldlyString(std::string("hello")).ShouldNotBe(std::string("world"));

    CHECK(cap.text().empty());
}

// -----------------------------------------------------------------------------
// ShouldlyStr (String) - fail direction
// -----------------------------------------------------------------------------
TEST_CASE("ShouldlyStr fails - routed to sink with expected fragment")
{
    CaptureOn cap;

    SUBCASE("ShouldBe")
    {
        ShouldlyString(std::string("hello")).ShouldBe(std::string("world"));
        CHECK(cap.text().find("equal") != std::string::npos);
    }
    SUBCASE("ShouldNotBe")
    {
        cap.clear();
        ShouldlyString(std::string("hello")).ShouldNotBe(std::string("hello"));
        CHECK(cap.text().find("not equal") != std::string::npos);
    }
}

// -----------------------------------------------------------------------------
// ShouldlyPtr (Pointer) - pass + fail directions
// -----------------------------------------------------------------------------
TEST_CASE("ShouldlyPtr passes - no failure routed")
{
    CaptureOn cap;
    int value = 42;
    int* nullp = nullptr;

    ShouldlyPointer(&value).ShouldNotBeNull();
    ShouldlyPointer(nullp).ShouldBeNull();

    CHECK(cap.text().empty());
}

TEST_CASE("ShouldlyPtr fails - routed to sink with expected fragment")
{
    CaptureOn cap;
    int value = 42;
    int* nullp = nullptr;

    SUBCASE("ShouldBeNull")
    {
        ShouldlyPointer(&value).ShouldBeNull();
        CHECK(cap.text().find("equal") != std::string::npos);
    }
    SUBCASE("ShouldNotBeNull")
    {
        cap.clear();
        ShouldlyPointer(nullp).ShouldNotBeNull();
        CHECK(cap.text().find("not equal") != std::string::npos);
    }
}

// -----------------------------------------------------------------------------
// ShouldlyAry (Array) - null / empty / not-null pass + fail directions
// -----------------------------------------------------------------------------
TEST_CASE("ShouldlyAry null + empty passes - no failure routed")
{
    CaptureOn cap;
    int const data[] = {1, 2, 3, 4};
    int* nullp = nullptr;
    // A zero-length array cannot be declared in standard C++; use a non-null
    // pointer with len 0 to exercise the "empty" (len==0) path.
    int const* empty_ary = data;

    ShouldlyArray(data, 4).ShouldNotBeNull();
    ShouldlyArray(nullp, 0).ShouldBeNull();
    // ShouldBeNullOrEmpty means "null OR empty": null passes, and so does a
    // non-null but length-zero array.
    ShouldlyArray(nullp, 0).ShouldBeNullOrEmpty();
    ShouldlyArray(empty_ary, 0).ShouldBeNullOrEmpty();
    ShouldlyArray(data, 4).ShouldNotBeNullOrEmpty();

    CHECK(cap.text().empty());
}

TEST_CASE("ShouldlyAry null + empty fails - routed to sink")
{
    CaptureOn cap;
    int* nullp = nullptr;

    SUBCASE("ShouldNotBeNull")
    {
        ShouldlyArray(nullp, 0).ShouldNotBeNull();
        CHECK(cap.text().find("not equal") != std::string::npos);
    }
    SUBCASE("ShouldBeNull")
    {
        int const data[] = {1};
        cap.clear();
        ShouldlyArray(data, 1).ShouldBeNull();
        CHECK(cap.text().find("equal") != std::string::npos);
    }
    SUBCASE("ShouldBeNullOrEmpty nonempty")
    {
        int const data[] = {1};
        cap.clear();
        ShouldlyArray(data, 1).ShouldBeNullOrEmpty();
        CHECK(cap.text().find("equal") != std::string::npos);
    }
    SUBCASE("ShouldNotBeNullOrEmpty empty")
    {
        int const data[] = {1};
        int const* empty_ary = data;
        cap.clear();
        ShouldlyArray(empty_ary, 0).ShouldNotBeNullOrEmpty();
        CHECK(cap.text().find("not equal") != std::string::npos);
    }
}

// -----------------------------------------------------------------------------
// ShouldlyAry - elementwise predicates (OrBe / ButBe / MatchContents)
// -----------------------------------------------------------------------------
TEST_CASE("ShouldlyAry elementwise passes - no failure routed")
{
    CaptureOn cap;

    int const data[] = {1, 2, 3, 4};
    ShouldlyArray(data, 4).ShouldNotBeNullOrEmptyOrBe(0);     // all != 0
    ShouldlyArray(data, 4).ShouldNotBeNullOrEmptyButMatchContents(data); // equal to itself

    int const uniform[] = {7, 7, 7};
    ShouldlyArray(uniform, 3).ShouldNotBeNullOrEmptyButBe(7); // all == 7

    CHECK(cap.text().empty());
}

TEST_CASE("ShouldlyAry elementwise fails - routed to sink")
{
    CaptureOn cap;

    SUBCASE("ShouldNotBeNullOrEmptyOrBe")
    {
        int const data[] = {1, 2, 3};
        ShouldlyArray(data, 3).ShouldNotBeNullOrEmptyOrBe(2); // element[1] == 2
        CHECK(cap.text().find("not equal") != std::string::npos);
        CHECK(cap.text().find("[1]") != std::string::npos);
    }
    SUBCASE("ShouldNotBeNullOrEmptyButBe")
    {
        int const data[] = {1, 2, 3};
        cap.clear();
        ShouldlyArray(data, 3).ShouldNotBeNullOrEmptyButBe(9); // none == 9
        CHECK(cap.text().find("equal") != std::string::npos);
    }
    SUBCASE("ShouldNotBeNullOrEmptyButMatchContents")
    {
        int const data[] = {1, 2, 3};
        int const bad[] = {1, 9, 3};
        cap.clear();
        ShouldlyArray(data, 3).ShouldNotBeNullOrEmptyButMatchContents(bad);
        CHECK(cap.text().find("equal") != std::string::npos);
    }
}

// -----------------------------------------------------------------------------
// ShouldlyAry - nearly-equal (floating point) pass + fail directions
// -----------------------------------------------------------------------------
TEST_CASE("ShouldlyAry nearly-equal passes - no failure routed")
{
    CaptureOn cap;

    // Value overload: each element nearly equal to scalar def (e=6 default).
    double const close[] = {1.0, 1.0 + 1e-8, 1.0 - 1e-8};
    ShouldlyArray(close, 3).ShouldNotBeNullOrEmptyButBeNearlyEqual(1.0);

    // Pointer overload: element-wise near equal.
    double const a[] = {1.0, 2.0};
    double const b[] = {1.0, 2.0};
    ShouldlyArray(a, 2).ShouldNotBeNullOrEmptyButBeNearlyEqual(b);

    CHECK(cap.text().empty());
}

TEST_CASE("ShouldlyAry nearly-equal fails - routed to sink")
{
    CaptureOn cap;

    SUBCASE("value overload")
    {
        double const data[] = {1.0, 3.0};
        ShouldlyArray(data, 2).ShouldNotBeNullOrEmptyButBeNearlyEqual(1.0);
        CHECK(cap.text().find("equal") != std::string::npos);
    }
    SUBCASE("pointer overload")
    {
        double const a[] = {1.0, 2.0};
        double const b[] = {1.0, 900.0};
        cap.clear();
        ShouldlyArray(a, 2).ShouldNotBeNullOrEmptyButBeNearlyEqual(b);
        CHECK(cap.text().find("nearly equal") != std::string::npos);
    }
}

// -----------------------------------------------------------------------------
// Diff helper (ShouldlyAry member) - both directions
// -----------------------------------------------------------------------------
TEST_CASE("ShouldlyAry::Diff helper")
{
    double const data[] = {0.0, 0.0};
    vaml::test::ShouldlyAry<double> ary(data, 2, "data");

    CHECK(ary.Diff(1.0, 1.0) == false);              // equal -> no diff
    CHECK(ary.Diff(1.0, 1.0 + 1e-10) == false);      // within default eps -> no diff
    CHECK(ary.Diff(1.0, 999.0) == true);             // meaningfully different
}

// -----------------------------------------------------------------------------
// Default-sink contract: with NO custom sink installed, a failing predicate
// throws TestFailureException (the S006 DSL contract). The S036_1 doctest seam
// (SHOULD / SHOULD_REQUIRE) CONSUMES this exact throw to route a failure into
// doctest's ResultBuilder. These tests stay green because they assert the
// throw directly; they document the contract SHOULD relies upon.
// -----------------------------------------------------------------------------
TEST_CASE("default sink throws TestFailureException on failure")
{
    // Be explicit and independent of any prior test's sink state.
    vaml::test::failure_sink() = &vaml::test::DefaultFailureSink;

    CHECK_THROWS_AS(ShouldlyIntegral(5).ShouldBe(6), vaml::test::TestFailureException);
    CHECK_THROWS_AS(ShouldlyString(std::string("a")).ShouldBe(std::string("b")),
                    vaml::test::TestFailureException);
    CHECK_THROWS_AS(ShouldlyIntegral(5).ShouldBeGreaterThan(6), vaml::test::TestFailureException);
}

// -----------------------------------------------------------------------------
// S036_1 doctrouting seam: SHOULD / SHOULD_REQUIRE.
//
// A PASSING predicate wrapped in SHOULD/SHOULD_REQUIRE must compile at /WX and
// integrate with doctest's success path (m_failed=false -> log() is a no-op,
// react() does not throw). These run fully green.
// -----------------------------------------------------------------------------
TEST_CASE("SHOULD success path integrates with doctest (green)")
{
    SHOULD(ShouldlyIntegral(5).ShouldBe(5));
    SHOULD(ShouldlyIntegral(5).ShouldNotBe(6));
    SHOULD(ShouldlyIntegral(5).ShouldBeGreaterThan(4));
    SHOULD(ShouldlyIntegral(5).ShouldBeLessThan(6));
    SHOULD(ShouldlyIntegral(5).ShouldBeInRangeTo(5, 1));

    SHOULD_REQUIRE(ShouldlyIntegral(5).ShouldBe(5));
    SHOULD_REQUIRE(ShouldlyIntegral(5).ShouldBeGreaterThanOrEqualTo(5));
}

TEST_CASE("SHOULD success path on string and pointer predicates (green)")
{
    SHOULD(ShouldlyString(std::string("hello")).ShouldBe(std::string("hello")));
    SHOULD(ShouldlyString(std::string("hello")).ShouldNotBe(std::string("world")));

    int   value   = 42;
    int*  nullp   = nullptr;
    int*  nonnull = &value;
    SHOULD(ShouldlyPointer(nonnull).ShouldNotBeNull());
    SHOULD(ShouldlyPointer(nullp).ShouldBeNull());
}

// -----------------------------------------------------------------------------
// S036_1 failure routing: a FAILING SHOULD is caught by the macro and routed
// into doctest (counted, no throw escapes the test). This is demonstrated with
// doctest's `should_fail()` decorator so the suite stays GREEN while proving
// the failure does NOT propagate out of the macro (had it propagated, the test
// would crash/abort rather than reach a controlled end => visibly different
// from the expected-failure outcome).
// -----------------------------------------------------------------------------
TEST_CASE("S036_1: a failing SHOULD is swallowed and routed into doctest (demonstration)"
          * doctest::should_fail()) // this test is EXPECTED to have a failing assert; suite stays green
{
    // If SHOULD incorrectly rethrew TestFailureException, this uncaught throw
    // would surface as a test-case exception instead of a normal AssertFailure.
    SHOULD(ShouldlyIntegral(5).ShouldBe(6));
}

// -----------------------------------------------------------------------------
// S036_3 typed-exception assertion category: ShouldThrow<Ex>.
//
// Pass  : the expected type is thrown -> no failure routed.
// Fail  : nothing thrown, or a DIFFERENT type/message is thrown -> routed to the
//         sink (captured here) with the ACTUAL thrown type + message (the
//         ShouldThrow value-add: CHECK_THROWS_AS cannot report the actual type).
// -----------------------------------------------------------------------------
namespace
{
    struct DomainError : std::runtime_error
    {
        using std::runtime_error::runtime_error;
    };

    auto throwDomainError() -> void
    {
        throw DomainError("domain boom");
    }
    auto throwLogicError() -> void
    {
        throw std::logic_error("logic boom");
    }
    auto throwNothing() -> void {}
} // namespace

TEST_CASE("ShouldThrow passes - right type thrown, no failure routed")
{
    CaptureOn cap;

    ShouldThrow<DomainError>(throwDomainError);
    ShouldThrow<std::runtime_error>([] { throw std::runtime_error("x"); });
    // ShouldThrow<Base> accepts a DERIVED throw (typeid is checked at catch via Ex const&).
    ShouldThrow<std::exception>(throwDomainError);

    CHECK(cap.text().empty());
}

TEST_CASE("ShouldThrow fails - nothing thrown routes 'nothing was thrown'")
{
    CaptureOn cap;

    ShouldThrow<DomainError>(throwNothing);
    CHECK_FALSE(cap.text().empty());
    CHECK(cap.text().find("expected") != std::string::npos);
    CHECK(cap.text().find("nothing was thrown") != std::string::npos);
}

TEST_CASE("ShouldThrow fails - wrong type routes actual type + message")
{
    CaptureOn cap;

    ShouldThrow<DomainError>(throwLogicError);
    CHECK_FALSE(cap.text().empty());
    // Reports the ACTUAL thrown type (the ShouldThrow value-add over CHECK_THROWS_AS).
    CHECK(cap.text().find("expected <") != std::string::npos);
    CHECK(cap.text().find("logic boom") != std::string::npos);   // actual message surfaced
}

TEST_CASE("ShouldThrow message overload - right type + right message passes")
{
    CaptureOn cap;

    ShouldThrow<DomainError>(throwDomainError, std::string("domain boom"));
    ShouldThrow<DomainError>(throwDomainError, "domain boom"); // const char* overload

    CHECK(cap.text().empty());
}

TEST_CASE("ShouldThrow message overload - wrong message fails")
{
    CaptureOn cap;

    ShouldThrow<DomainError>(throwDomainError, std::string("wrong message"));
    CHECK_FALSE(cap.text().empty());
    CHECK(cap.text().find("wrong message") != std::string::npos);
    CHECK(cap.text().find("domain boom") != std::string::npos); // the ACTUAL message surfaced
}
