// Shouldly.h — header-only fluent assertion port (vaml-core)
//
// Port of the user's original "PiuGlobalsCpp::NsShouldly" Shouldly file (Shouldly.h) into the
// vaml-core public include tree, per S006 (dep: S005).
//
// What changed from the original:
//   * Removed all MSTest coupling (Microsoft::VisualStudio::CppUnitTestFramework::Assert::Fail
//     and the wide-string AssertStdString path).
//   * Removed Windows-only bits (_wcsdup / wchar_t / free-based conversions) and the
//     SHOULDLY_USES_AWESOMELY MSTest-vs-throw preprocessor toggle -> failure is now
//     unconditionally routed through the pluggable failure sink (throw by default).
//   * Removed MSVC-only __forceinline / __inline hints (plain inline instead).
//   * Removed dead/commented code (the commented-out Actual overloads, the unused DynString and
//     is_str helpers, and AwesomelyException -- superseded by TestFailureException).
//   * Avoids the Windows `max` macro portably via the parenthesized-call form `(std::max)`
//     (no push_macro/undef guard needed; identical on MSVC + Clang).
//
// All predicate names are preserved (see the struct bodies below): ShouldBe, ShouldNotBe,
// ShouldBeGreaterThan, ShouldBeLessThan, ShouldBeGreaterThanOrEqualTo,
// ShouldBeLessThanOrEqualTo, ShouldBeInRangeTo, ShouldBeNull, ShouldNotBeNull,
// ShouldBeNullOrEmpty, ShouldNotBeNullOrEmpty, ShouldNotBeNullOrEmptyOrBe,
// ShouldNotBeNullOrEmptyButBe, ShouldNotBeNullOrEmptyButBeNearlyEqual,
// ShouldNotBeNullOrEmptyButMatchContents, plus the Diff helper.
//
// Namespace: vaml::test (NOT vaml::assert -- `assert` is a function-like macro from <cassert>,
// so `namespace assert {}` would be ill-formed if <cassert> ever loads. `assert` as a directory
// name is fine; as a namespace it is a trap.)

#ifndef VAML_CORE_ASSERT_SHOULDLY_H
#define VAML_CORE_ASSERT_SHOULDLY_H

#include <cmath>
#include <cstdlib>
#include <exception>
#include <functional>
#include <sstream>
#include <string>
#include <type_traits>
#include <typeinfo>

namespace vaml
{
namespace test
{
    // -------------------------------------------------------------------------------------------
    // Failure sink (the only API this port adds over the original).
    //
    // Default behaviour: throw TestFailureException. A caller can install an alternative sink
    // (e.g. doctest CHECK failure capture) via failure_sink() = ... so the assert harness stays
    // test-framework-agnostic. This is what S007's round-trip tests will use (adapted Shouldly).
    // -------------------------------------------------------------------------------------------

    class TestFailureException : public std::exception
    {
    public:
        explicit TestFailureException(char const* message)
            : message_(message)
        {
        }

        explicit TestFailureException(std::string const& message)
            : message_(message)
        {
        }

        char const* what() const noexcept override
        {
            return message_.c_str();
        }

    private:
        std::string message_;
    };

    using FailureSink = void (*)(char const* message);

    inline void DefaultFailureSink(char const* message)
    {
        throw TestFailureException(message);
    }

    // Thread-local-backed settable singleton: each thread may install its own sink without
    // cross-thread data races. Non-const ref so the caller can do `failure_sink() = ...`.
    inline FailureSink& failure_sink()
    {
        static thread_local FailureSink sink = &DefaultFailureSink;
        return sink;
    }

    // Routes a formatted message through whatever sink is currently installed (default: throw).
    inline void ReportFailure(std::string const& message)
    {
        failure_sink()(message.c_str());
    }

    // -------------------------------------------------------------------------------------------
    // Formatting + dispatch (predicate names preserved from the original).
    // -------------------------------------------------------------------------------------------

    inline void AssertStringStream(std::stringstream const& ss)
    {
        ReportFailure(ss.str());
    }

    template <typename T>
    void ThrowAwesomelyStr(T const& _actual, T const& _expected, char const* _customMessage, char const* comparor)
    {
        std::stringstream ss;
        ss
            << _customMessage
            << std::endl
            << "    should be "
            << comparor
            << std::endl
            << _expected
            << std::endl
            << "    but was"
            << std::endl
            << _actual;

        AssertStringStream(ss);
    }

    template <typename T>
    void ThrowAwesomelyRef(T const& _actual, T const& _expected, char const* _customMessage, char const* comparor)
    {
        std::stringstream ss;
        ss
            << _customMessage
            << std::endl
            << "    should be "
            << comparor
            << std::endl
            << std::to_string(_expected)
            << std::endl
            << "    but was"
            << std::endl
            << std::to_string(_actual);

        AssertStringStream(ss);
    }

    template <typename T>
    void ThrowAwesomelyPtr(T const* const _actual, T const* const _expected, char const* _customMessage, char const* comparor)
    {
        std::stringstream ss;
        ss
            << _customMessage
            << std::endl
            << "    should be "
            << comparor
            << std::endl
            << std::to_string(_expected)
            << std::endl
            << "    but was"
            << std::endl
            << std::to_string(_actual);

        AssertStringStream(ss);
    }

    template <typename T>
    void ThrowAwesomelyVal(T _actual, T _expected, char const* _customMessage, char const* comparor)
    {
        std::stringstream ss;
        ss
            << _customMessage
            << std::endl
            << "    should be "
            << comparor
            << std::endl
            << std::to_string(_expected)
            << std::endl
            << "    but was"
            << std::endl
            << std::to_string(_actual);

        AssertStringStream(ss);
    }

    // -------------------------------------------------------------------------------------------
    // Predicate structs (every predicate name preserved from the original).
    // -------------------------------------------------------------------------------------------

    template <typename T>
    struct ShouldlyVal
    {
        T actual;
        char const* line;

        ShouldlyVal(T t, char const* _line)
            : actual(t)
            , line(_line)
        {
        }

        inline void ShouldBe(T expected, std::function<char const*()> customMessageMaker)
        {
            if (!(actual == expected))
                ThrowAwesomelyVal<T>(actual, expected, customMessageMaker(), "equal");
        }

        inline void ShouldBe(T expected, char const* customMessage)
        {
            ShouldBe(expected, [&]() -> char const* { return customMessage; });
        }

        inline void ShouldBe(T expected)
        {
            ShouldBe(expected, [&]() -> char const* { return line; });
        }

        void ShouldNotBe(T expected, std::function<char const*()> customMessageMaker)
        {
            if (!(actual != expected))
                ThrowAwesomelyVal<T>(actual, expected, customMessageMaker(), "not equal");
        }

        void ShouldNotBe(T expected, char const* customMessage)
        {
            ShouldNotBe(expected, [&]() -> char const* { return customMessage; });
        }

        inline void ShouldNotBe(T expected)
        {
            ShouldNotBe(expected, [&]() -> char const* { return line; });
        }

        void ShouldBeGreaterThan(T expected, std::function<char const*()> customMessageMaker)
        {
            if (!(actual > expected))
                ThrowAwesomelyVal<T>(actual, expected, customMessageMaker(), "greater than");
        }

        void ShouldBeGreaterThan(T expected, char const* customMessage)
        {
            ShouldBeGreaterThan(expected, [&]() -> char const* { return customMessage; });
        }

        inline void ShouldBeGreaterThan(T expected)
        {
            ShouldBeGreaterThan(expected, [&]() -> char const* { return line; });
        }

        void ShouldBeLessThan(T expected, std::function<char const*()> customMessageMaker)
        {
            if (!(actual < expected))
                ThrowAwesomelyVal<T>(actual, expected, customMessageMaker(), "less than");
        }

        void ShouldBeLessThan(T expected, char const* customMessage)
        {
            ShouldBeLessThan(expected, [&]() -> char const* { return customMessage; });
        }

        inline void ShouldBeLessThan(T expected)
        {
            ShouldBeLessThan(expected, [&]() -> char const* { return line; });
        }

        void ShouldBeGreaterThanOrEqualTo(T expected, std::function<char const*()> customMessageMaker)
        {
            if (!(actual >= expected))
                ThrowAwesomelyVal<T>(actual, expected, customMessageMaker(), "greater than");
        }

        void ShouldBeGreaterThanOrEqualTo(T expected, char const* customMessage)
        {
            ShouldBeGreaterThanOrEqualTo(expected, [&]() -> char const* { return customMessage; });
        }

        inline void ShouldBeGreaterThanOrEqualTo(T expected)
        {
            ShouldBeGreaterThanOrEqualTo(expected, [&]() -> char const* { return line; });
        }

        void ShouldBeLessThanOrEqualTo(T expected, std::function<char const*()> customMessageMaker)
        {
            if (!(actual <= expected))
                ThrowAwesomelyVal<T>(actual, expected, customMessageMaker(), "less than");
        }

        void ShouldBeLessThanOrEqualTo(T expected, char const* customMessage)
        {
            ShouldBeLessThanOrEqualTo(expected, [&]() -> char const* { return customMessage; });
        }

        inline void ShouldBeLessThanOrEqualTo(T expected)
        {
            ShouldBeLessThanOrEqualTo(expected, [&]() -> char const* { return line; });
        }

        void ShouldBeInRangeTo(T expected, std::function<char const*()> customMessageMaker, T range)
        {
            if (!(actual >= expected - range && actual <= expected + range))
                ThrowAwesomelyVal<T>(actual, expected, customMessageMaker(), "range than");
        }

        void ShouldBeInRangeTo(T expected, char const* customMessage, T range)
        {
            ShouldBeInRangeTo(expected, [&]() -> char const* { return customMessage; }, range);
        }

        inline void ShouldBeInRangeTo(T expected, T range)
        {
            ShouldBeInRangeTo(expected, [&]() -> char const* { return line; }, range);
        }
    };

    template <typename T>
    struct ShouldlyRef
    {
        T const& actual;
        char const* line;

        ShouldlyRef(T const& t, char const* _line)
            : actual(t)
            , line(_line)
        {
        }

        void ShouldBe(T const& expected, std::function<char const*()> customMessageMaker)
        {
            if (!(actual == expected))
                ThrowAwesomelyRef<T>(actual, expected, customMessageMaker(), "equal");
        }

        void ShouldBe(T const& expected, char const* customMessage)
        {
            ShouldBe(expected, [&]() -> char const* { return customMessage; });
        }

        void ShouldNotBe(T const& expected, std::function<char const*()> customMessageMaker)
        {
            if (!(actual != expected))
                ThrowAwesomelyRef<T>(actual, expected, customMessageMaker(), "not equal");
        }

        void ShouldNotBe(T const& expected, char const* customMessage)
        {
            ShouldNotBe(expected, [&]() -> char const* { return customMessage; });
        }
    };

    template <typename T>
    struct ShouldlyStr
    {
        T const& actual;
        char const* line;

        ShouldlyStr(T const& t, char const* _line)
            : actual(t)
            , line(_line)
        {
        }

        void ShouldBe(T const& expected, std::function<char const*()> customMessageMaker)
        {
            if (!(actual == expected))
                ThrowAwesomelyStr<T>(actual, expected, customMessageMaker(), "equal");
        }

        void ShouldBe(T const& expected, char const* customMessage)
        {
            ShouldBe(expected, [&]() -> char const* { return customMessage; });
        }

        void ShouldBe(T const& expected)
        {
            ShouldBe(expected, [&]() -> char const* { return line; });
        }

        void ShouldNotBe(T const& expected, std::function<char const*()> customMessageMaker)
        {
            if (!(actual != expected))
                ThrowAwesomelyStr<T>(actual, expected, customMessageMaker(), "not equal");
        }

        void ShouldNotBe(T const& expected, char const* customMessage)
        {
            ShouldNotBe(expected, [&]() -> char const* { return customMessage; });
        }

        void ShouldNotBe(T const& expected)
        {
            ShouldNotBe(expected, [&]() -> char const* { return line; });
        }
    };

    template <typename T>
    struct ShouldlyPtr
    {
        T const* const actual;
        char const* line;

        ShouldlyPtr(T const* const t, char const* _line)
            : actual(t)
            , line(_line)
        {
        }

        void ShouldBeNull(std::function<char const*()> customMessageMaker)
        {
            if (!(actual == nullptr))
                ThrowAwesomelyVal((unsigned long long)actual, (unsigned long long)nullptr, customMessageMaker(), "equal");
        }

        void ShouldBeNull(char const* customMessage)
        {
            ShouldBeNull([&]() -> char const* { return customMessage; });
        }

        void ShouldBeNull()
        {
            ShouldBeNull([&]() -> char const* { return line; });
        }

        void ShouldNotBeNull(std::function<char const*()> customMessageMaker)
        {
            if (!(actual != nullptr))
                ThrowAwesomelyVal((unsigned long long)actual, (unsigned long long)nullptr, customMessageMaker(), "not equal");
        }

        void ShouldNotBeNull(char const* customMessage)
        {
            ShouldNotBeNull([&]() -> char const* { return customMessage; });
        }

        void ShouldNotBeNull()
        {
            ShouldNotBeNull([&]() -> char const* { return line; });
        }
    };

    template <typename T>
    struct ShouldlyAry
    {
        T const* const actual;
        size_t const len;
        char const* line;

        ShouldlyAry(T const* const t, size_t const n, char const* _line)
            : actual(t)
            , len(n)
            , line(_line)
        {
        }

        void ShouldBeNull(std::function<char const*()> customMessageMaker)
        {
            if (!(actual == nullptr))
                ThrowAwesomelyVal((unsigned long long)actual, (unsigned long long)nullptr, customMessageMaker(), "equal");
        }

        void ShouldBeNull(char const* customMessage)
        {
            ShouldBeNull([&]() -> char const* { return customMessage; });
        }

        void ShouldBeNull()
        {
            ShouldBeNull([&]() -> char const* { return line; });
        }

        void ShouldNotBeNull(std::function<char const*()> customMessageMaker)
        {
            if (!(actual != nullptr))
                ThrowAwesomelyVal((unsigned long long)actual, (unsigned long long)nullptr, customMessageMaker(), "not equal");
        }

        void ShouldNotBeNull(char const* customMessage)
        {
            ShouldNotBeNull([&]() -> char const* { return customMessage; });
        }

        void ShouldNotBeNull()
        {
            ShouldNotBeNull([&]() -> char const* { return line; });
        }

        void ShouldBeNullOrEmpty(std::function<char const*()> customMessageMaker)
        {
            // "null OR empty": passes iff the pointer is null OR the length is zero.
            // (The original required BOTH -- a bug the owner asked to correct here.)
            if (!(actual == nullptr || len == 0))
                ThrowAwesomelyVal((unsigned long long)actual, (unsigned long long)0, customMessageMaker(), "equal");
        }

        void ShouldBeNullOrEmpty(char const* customMessage)
        {
            ShouldBeNullOrEmpty([&]() -> char const* { return customMessage; });
        }

        void ShouldBeNullOrEmpty()
        {
            ShouldBeNullOrEmpty([&]() -> char const* { return line; });
        }

        void ShouldNotBeNullOrEmpty(std::function<char const*()> customMessageMaker)
        {
            if (!(actual != nullptr))
                ThrowAwesomelyVal((unsigned long long)actual, (unsigned long long)nullptr, customMessageMaker(), "not equal");
            if (!(len != 0))
                ThrowAwesomelyVal((unsigned long long)actual, (unsigned long long)0, customMessageMaker(), "not equal");
        }

        void ShouldNotBeNullOrEmpty()
        {
            ShouldNotBeNullOrEmpty([&]() -> char const* { return line; });
        }

        void ShouldNotBeNullOrEmpty(char const* customMessage)
        {
            ShouldNotBeNullOrEmpty([&]() -> char const* { return customMessage; });
        }

        void ShouldNotBeNullOrEmptyOrBe(T const def, std::function<char const*()> customMessageMaker)
        {
            if (!(actual != nullptr))
                ThrowAwesomelyVal((unsigned long long)actual, (unsigned long long)nullptr, customMessageMaker(), "not equal");
            if (!(len != 0))
                ThrowAwesomelyVal((unsigned long long)actual, (unsigned long long)0, customMessageMaker(), "not equal");

            for (size_t i = 0; i < len; ++i)
            {
                if (!(actual[i] != def))
                {
                    std::stringstream ss;
                    ss << customMessageMaker() << "[" << std::to_string(i) << "]";
                    ThrowAwesomelyVal<T>(actual[i], def, ss.str().c_str(), "not equal");
                }
            }
        }

        void ShouldNotBeNullOrEmptyOrBe(T const def, char const* customMessage)
        {
            ShouldNotBeNullOrEmptyOrBe(def, [&]() -> char const* { return customMessage; });
        }

        void ShouldNotBeNullOrEmptyOrBe(T const def)
        {
            ShouldNotBeNullOrEmptyOrBe(def, [&]() -> char const* { return line; });
        }

        void ShouldNotBeNullOrEmptyButBe(T const def, std::function<char const*()> customMessageMaker)
        {
            if (!(actual != nullptr))
                ThrowAwesomelyVal((unsigned long long)actual, (unsigned long long)nullptr, customMessageMaker(), "not equal");
            if (!(len != 0))
                ThrowAwesomelyVal((unsigned long long)actual, (unsigned long long)0, customMessageMaker(), "not equal");

            for (size_t i = 0; i < len; ++i)
            {
                if (!(actual[i] == def))
                {
                    std::stringstream ss;
                    ss << customMessageMaker() << "[" << std::to_string(i) << "]";
                    ThrowAwesomelyVal<T>(actual[i], def, ss.str().c_str(), "equal");
                }
            }
        }

        void ShouldNotBeNullOrEmptyButBe(T const def, char const* customMessage)
        {
            ShouldNotBeNullOrEmptyButBe(def, [&]() -> char const* { return customMessage; });
        }

        void ShouldNotBeNullOrEmptyButBe(T const def)
        {
            ShouldNotBeNullOrEmptyButBe(def, [&]() -> char const* { return line; });
        }

        inline bool Diff(T value1, T value2, T const e = (T)1e-6, T regularize = 0, T diff = 0)
        {
            regularize = (std::max)(std::abs(value1), std::abs(value2));
            if (regularize > 0)
            {
                diff = std::abs(value1 - value2);
                if (diff == regularize)
                {
                    if (diff > e)
                        return true;
                }
                else if (diff / regularize > e)
                    return true;
            }

            return false;
        }

        void ShouldNotBeNullOrEmptyButBeNearlyEqual(T const def, std::function<char const*()> customMessageMaker, T const e = (T)1, double regularize = 0, double diff = 0)
        {
            if (!(actual != nullptr))
                ThrowAwesomelyVal((unsigned long long)actual, (unsigned long long)nullptr, customMessageMaker(), "not equal");
            if (!(len != 0))
                ThrowAwesomelyVal((unsigned long long)actual, (unsigned long long)0, customMessageMaker(), "not equal");

            for (size_t i = 0; i < len; ++i)
            {
                bool complain = false;
                regularize = (std::max)(std::abs(actual[i]), std::abs(def));
                if (!regularize)
                    continue;

                diff = -log10(std::abs(actual[i] / regularize - def / regularize));
                if (std::fpclassify(diff) == FP_ZERO)
                    complain = Diff(actual[i], def);
                else if (!std::isnormal(diff))
                    continue;
                else if (diff < e)
                    complain = true;

                if (complain)
                {
                    std::stringstream ss;
                    ss << customMessageMaker() << "[" << std::to_string(i) << "]";
                    ThrowAwesomelyVal<T>(actual[i], def, ss.str().c_str(), "equal");
                }
            }
        }

        void ShouldNotBeNullOrEmptyButBeNearlyEqual(T const def, char const* customMessage, T const e = (T)6)
        {
            ShouldNotBeNullOrEmptyButBeNearlyEqual(def, [&]() -> char const* { return customMessage; }, e);
        }

        void ShouldNotBeNullOrEmptyButBeNearlyEqual(T const def, T const e = (T)6)
        {
            ShouldNotBeNullOrEmptyButBeNearlyEqual(def, [&]() -> char const* { return line; }, e);
        }

        void ShouldNotBeNullOrEmptyButBeNearlyEqual(T const* const def, std::function<char const*()> customMessageMaker, T const e = (T)1, double regularize = 0, double diff = 0)
        {
            if (!(actual != nullptr))
                ThrowAwesomelyVal((unsigned long long)actual, (unsigned long long)nullptr, customMessageMaker(), "not equal");
            if (!(len != 0))
                ThrowAwesomelyVal((unsigned long long)actual, (unsigned long long)0, customMessageMaker(), "not equal");

            for (size_t i = 0; i < len; ++i)
            {
                bool complain = false;
                regularize = (std::max)(std::abs(actual[i]), std::abs(def[i]));
                if (!regularize)
                    continue;

                diff = -log10(std::abs(actual[i] / regularize - def[i] / regularize));
                if (std::fpclassify(diff) == FP_ZERO)
                    complain = Diff(actual[i], def[i]);
                else if (!std::isnormal(diff))
                    continue;

                if (diff < e)
                    complain = true;

                if (complain)
                {
                    std::stringstream ss;
                    ss << customMessageMaker() << "[" << std::to_string(i) << "]";
                    ThrowAwesomelyVal<T>(actual[i], def[i], ss.str().c_str(), "nearly equal");
                }
            }
        }

        void ShouldNotBeNullOrEmptyButBeNearlyEqual(T const* const def, char const* customMessage, T const e = (T)6)
        {
            ShouldNotBeNullOrEmptyButBeNearlyEqual(def, [&]() -> char const* { return customMessage; }, e);
        }

        void ShouldNotBeNullOrEmptyButBeNearlyEqual(T const* const def, T const e = (T)6)
        {
            ShouldNotBeNullOrEmptyButBeNearlyEqual(def, [&]() -> char const* { return line; }, e);
        }

        void ShouldNotBeNullOrEmptyButMatchContents(T const* const comp, std::function<char const*()> customMessageMaker)
        {
            if (!(actual != nullptr))
                ThrowAwesomelyVal((unsigned long long)actual, (unsigned long long)nullptr, customMessageMaker(), "not equal");
            if (!(len != 0))
                ThrowAwesomelyVal((unsigned long long)actual, (unsigned long long)0, customMessageMaker(), "not equal");

            for (size_t i = 0; i < len; ++i)
            {
                if (!(actual[i] == comp[i]))
                {
                    std::stringstream ss;
                    ss << customMessageMaker() << "[" << std::to_string(i) << "]";
                    ThrowAwesomelyVal<T>(actual[i], comp[i], ss.str().c_str(), "equal");
                }
            }
        }

        void ShouldNotBeNullOrEmptyButMatchContents(T const* const comp, char const* customMessage)
        {
            ShouldNotBeNullOrEmptyButMatchContents(comp, [&]() -> char const* { return customMessage; });
        }

        void ShouldNotBeNullOrEmptyButMatchContents(T const* const comp)
        {
            ShouldNotBeNullOrEmptyButMatchContents(comp, [&]() -> char const* { return line; });
        }
    };

    // -------------------------------------------------------------------------------------------
    // Factory functions + convenience macros (predicate/entry names preserved).
    // -------------------------------------------------------------------------------------------

    template <typename T>
    inline auto Integral(T t, char const* line)
    {
        return ShouldlyVal<T>(t, line);
    }

    template <typename T>
    inline auto String(T const& t, char const* line)
    {
        return ShouldlyStr<T>(t, line);
    }

    template <typename T>
    inline auto Pointer(T const* const t, char const* line)
    {
        return ShouldlyPtr<T>(t, line);
    }

    template <typename T>
    inline auto Array(T const* const t, size_t const n, char const* line)
    {
        return ShouldlyAry<T>(t, n, line);
    }

    // -------------------------------------------------------------------------------------------
    // Typed-exception assertion category (S036_3).
    //
    // ShouldThrow<Ex>(fn) asserts that invoking fn() throws EXACTLY the exception type Ex. It is
    // the first-class replacement for doctest's CHECK_THROWS_AS / CHECK_THROWS_WITH(_AS) — and it
    // OUT-EXPOSES doctest: on a wrong-or-missing throw it reports the ACTUAL exception type (via
    // RTTI typeid()) and message, which CHECK_THROWS_AS cannot do at all (it only says "expected X,
    // different exception").
    //
    // Failure is routed through the same pluggable sink as every other predicate (default: throw
    // TestFailureException). That means ShouldThrow composes with the SHOULD(...) doctest seam:
    //
    //      SHOULD( ShouldThrow<vaml::PropertyNotFound>([&]{ obj.Get(PropertyId::of("Nope")); }) )
    //      SHOULD( ShouldThrow<vaml::PropertyNotFound>([&]{ obj.Get(PropertyId::of("Nope")); }, "expected message") )
    //
    // Pass  : Ex is thrown (and, for the message-matcher overload, its what() equals expected).
    // Fail 1: nothing thrown  -> "expected <Ex> but nothing was thrown".
    // Fail 2: wrong type      -> "expected <Ex> but got <ActualType> '<what()>'" (actual type + msg).
    // Fail 3: right type, wrong message -> "<Ex> with message '<expected>' but message was '<actual>'".
    //
    // This is NOT lazy-avoidance of CHECK_THROWS_AS: it is the deliberate, first-class typed-exception
    // assertion of the re-formulated Shouldly, resolving that category at S036_3 (NO-GAP/NO-LAZY-LLM).
    // -------------------------------------------------------------------------------------------

    namespace detail
    {
        // Summarizes a caught std::exception as "<type> '<what>'" for failure reporting.
        inline std::string describe_thrown_std_exception(std::exception const& e)
        {
            std::stringstream ss;
            ss << "<" << typeid(e).name() << "> '" << e.what() << "'";
            return ss.str();
        }
    } // namespace detail

    template <typename Ex, typename Fn>
    void ShouldThrow(Fn&& fn)
    {
        static_assert(std::is_base_of<std::exception, Ex>::value,
                      "ShouldThrow<Ex>: Ex must derive from std::exception to carry a message");

        if constexpr (std::is_same<Ex, std::exception>::value)
        {
            // Two distinct handlers only: Ex IS std::exception, so the first catch
            // already matches every std::exception; catch(...) is for non-std throws.
            try
            {
                fn();
            }
            catch (std::exception const&)
            {
                return; // a std::exception (of any derived type) was thrown -> pass
            }
            catch (...)
            {
                ReportFailure("expected <std::exception> but a non-std::exception was thrown");
                return;
            }
        }
        else
        {
            try
            {
                fn();
            }
            catch (Ex const&)
            {
                return; // expected derived type thrown -> pass
            }
            catch (std::exception const& e)
            {
                std::stringstream ss;
                ss << "expected <" << typeid(Ex).name() << "> but got "
                   << detail::describe_thrown_std_exception(e);
                ReportFailure(ss.str());
                return;
            }
            catch (...)
            {
                std::stringstream ss;
                ss << "expected <" << typeid(Ex).name()
                   << "> but a non-std::exception of unknown type was thrown";
                ReportFailure(ss.str());
                return;
            }
        }

        // No catch matched -> nothing was thrown.
        std::stringstream ss2;
        ss2 << "expected <" << typeid(Ex).name() << "> but nothing was thrown";
        ReportFailure(ss2.str());
    }

    template <typename Ex, typename Fn>
    void ShouldThrow(Fn&& fn, std::string const& expectedMessage)
    {
        static_assert(std::is_base_of<std::exception, Ex>::value,
                      "ShouldThrow<Ex>: Ex must derive from std::exception to carry a message");
        static_assert(!std::is_same<Ex, std::exception>::value,
                      "message-matcher overload of ShouldThrow<std::exception> is ambiguous; use a derived type");

        try
        {
            fn();
        }
        catch (Ex const& e)
        {
            if (std::string(e.what()) == expectedMessage)
            {
                return; // right type AND right message -> pass
            }
            std::stringstream ss;
            ss << "expected <" << typeid(Ex).name() << "> with message '" << expectedMessage
               << "' but message was '" << e.what() << "'";
            ReportFailure(ss.str());
            return;
        }
        catch (std::exception const& e)
        {
            std::stringstream ss;
            ss << "expected <" << typeid(Ex).name() << "> with message '" << expectedMessage
               << "' but got " << detail::describe_thrown_std_exception(e);
            ReportFailure(ss.str());
            return;
        }
        catch (...)
        {
            std::stringstream ss;
            ss << "expected <" << typeid(Ex).name()
               << "> but a non-std::exception of unknown type was thrown";
            ReportFailure(ss.str());
            return;
        }

        // No catch matched -> nothing was thrown.
        std::stringstream ss2;
        ss2 << "expected <" << typeid(Ex).name() << "> with message '" << expectedMessage
            << "' but nothing was thrown";
        ReportFailure(ss2.str());
    }

    // Overload accepting a message literal without an explicit std::string at the call site.
    template <typename Ex, typename Fn>
    void ShouldThrow(Fn&& fn, char const* expectedMessage)
    {
        ShouldThrow<Ex>(std::forward<Fn>(fn), std::string(expectedMessage));
    }

} // namespace test
} // namespace vaml

#define ShouldlyIntegral(stuff) vaml::test::Integral(stuff, #stuff)
#define ShouldlyString(stuff) vaml::test::String(stuff, #stuff)
#define ShouldlyPointer(stuff) vaml::test::Pointer(stuff, #stuff)
#define ShouldlyArray(stuff, num) vaml::test::Array(stuff, num, #stuff)

// -------------------------------------------------------------------------------------------
// Doctest routing seam (S036_1).
//
// A fluent Shouldly predicate call (ShouldlyIntegral(x).ShouldBe(y), ...) cannot, by itself,
// terminate inside doctest: doctest::detail::ResultBuilder is only valid while a test is
// running, and a fluent call evaluated inside an expression has no scoped ResultBuilder to
// hand its result to. So the S036_1 reform keeps the predicates EXACTLY as they are (on
// failure they throw vaml::test::TestFailureException -- the default sink contract) and moves
// the doctest integration to a macro that OPEN this seam in a test context:
//
//      SHOULD( ShouldlyIntegral(Get()).ShouldBe(3) )
//
// The macro opens a ResultBuilder with (file,line, stringified expr), runs the predicate,
// catches the thrown TestFailureException, and routes its MESSAGE into the builder as the
// failure decomposition. It then logs + reacts exactly like doctest's own asserts, so the
// failure lands in doctest's counters/summary/`-s` success output and honors severity:
//
//      SHOULD_REQUIRE(...)  -> DT_REQUIRE  (aborts the current test case on failure)
//      SHOULD(...)          -> DT_CHECK    (continue-on-failure; the default)
//
// Because the predicates keep throwing, the same failing expression can be asserted directly
// with CHECK_THROWS_AS(..., vaml::test::TestFailureException) when a test wants to prove a
// predicate actually fired -- no sink capture needed.
//
// These macros are defined ONLY when doctest.h has already been included (guarded by
// DOCTEST_LIBRARY_INCLUDED). The core library never includes doctest, so this block is inert
// outside the test harness; test TUs include <doctest/doctest.h> before <vaml/assert/Shouldly.h>.
// -------------------------------------------------------------------------------------------
#ifdef DOCTEST_LIBRARY_INCLUDED

#define VAML_SHOULD_IMPL(assert_type, ...)                                                        \
    do {                                                                                           \
        doctest::detail::ResultBuilder rb(doctest::assertType::assert_type, __FILE__,              \
                                          __LINE__, #__VA_ARGS__);                                 \
        try {                                                                                      \
            __VA_ARGS__;                                                                           \
            rb.m_failed = false;                                                                   \
        } catch (vaml::test::TestFailureException const& e) {                                      \
            rb.m_failed = true;                                                                    \
            rb.m_decomp = e.what();                                                                \
        } catch (...) {                                                                            \
            rb.translateException();                                                               \
        }                                                                                          \
        if (rb.log())                                                                              \
            DOCTEST_BREAK_INTO_DEBUGGER();                                                         \
        rb.react();                                                                                \
    } while (false)

// SHOULD: assert a Shouldly predicate expression; failures are DT_CHECK (continue).
#define SHOULD(...) VAML_SHOULD_IMPL(DT_CHECK, __VA_ARGS__)
// SHOULD_REQUIRE: assert a Shouldly predicate expression; failures are DT_REQUIRE (abort case).
#define SHOULD_REQUIRE(...) VAML_SHOULD_IMPL(DT_REQUIRE, __VA_ARGS__)

#endif // DOCTEST_LIBRARY_INCLUDED

#endif // VAML_CORE_ASSERT_SHOULDLY_H
