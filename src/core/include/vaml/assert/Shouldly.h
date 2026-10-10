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
        explicit TestFailureException(char const* const message)
            : std::exception(message)
        {
        }

        explicit TestFailureException(std::string const& message)
            : std::exception(message.c_str())
        {
        }
    };

    using FailureSink = void (*)(char const* const message);

    inline void DefaultFailureSink(char const* const message)
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
    void ThrowAwesomelyStr(T const& _actual, T const& _expected, char const* const _customMessage, char const* const comparor)
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
    void ThrowAwesomelyRef(T const& _actual, T const& _expected, char const* const _customMessage, char const* const comparor)
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
    void ThrowAwesomelyPtr(T const* const _actual, T const* const _expected, char const* const _customMessage, char const* const comparor)
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
    void ThrowAwesomelyVal(T _actual, T _expected, char const* const _customMessage, char const* const comparor)
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
        char const* const line;

        ShouldlyVal(T t, char const* const _line)
            : actual(t)
            , line(_line)
        {
        }

        inline void ShouldBe(T expected, std::function<char const* const()> customMessageMaker)
        {
            if (!(actual == expected))
                ThrowAwesomelyVal<T>(actual, expected, customMessageMaker(), "equal");
        }

        inline void ShouldBe(T expected, char const* const customMessage)
        {
            ShouldBe(expected, [&]() -> char const* const { return customMessage; });
        }

        inline void ShouldBe(T expected)
        {
            ShouldBe(expected, [&]() -> char const* const { return line; });
        }

        void ShouldNotBe(T expected, std::function<char const* const()> customMessageMaker)
        {
            if (!(actual != expected))
                ThrowAwesomelyVal<T>(actual, expected, customMessageMaker(), "not equal");
        }

        void ShouldNotBe(T expected, char const* const customMessage)
        {
            ShouldNotBe(expected, [&]() -> char const* const { return customMessage; });
        }

        inline void ShouldNotBe(T expected)
        {
            ShouldNotBe(expected, [&]() -> char const* const { return line; });
        }

        void ShouldBeGreaterThan(T expected, std::function<char const* const()> customMessageMaker)
        {
            if (!(actual > expected))
                ThrowAwesomelyVal<T>(actual, expected, customMessageMaker(), "greater than");
        }

        void ShouldBeGreaterThan(T expected, char const* const customMessage)
        {
            ShouldBeGreaterThan(expected, [&]() -> char const* const { return customMessage; });
        }

        inline void ShouldBeGreaterThan(T expected)
        {
            ShouldBeGreaterThan(expected, [&]() -> char const* const { return line; });
        }

        void ShouldBeLessThan(T expected, std::function<char const* const()> customMessageMaker)
        {
            if (!(actual < expected))
                ThrowAwesomelyVal<T>(actual, expected, customMessageMaker(), "less than");
        }

        void ShouldBeLessThan(T expected, char const* const customMessage)
        {
            ShouldBeLessThan(expected, [&]() -> char const* const { return customMessage; });
        }

        inline void ShouldBeLessThan(T expected)
        {
            ShouldBeLessThan(expected, [&]() -> char const* const { return line; });
        }

        void ShouldBeGreaterThanOrEqualTo(T expected, std::function<char const* const()> customMessageMaker)
        {
            if (!(actual >= expected))
                ThrowAwesomelyVal<T>(actual, expected, customMessageMaker(), "greater than");
        }

        void ShouldBeGreaterThanOrEqualTo(T expected, char const* const customMessage)
        {
            ShouldBeGreaterThanOrEqualTo(expected, [&]() -> char const* const { return customMessage; });
        }

        inline void ShouldBeGreaterThanOrEqualTo(T expected)
        {
            ShouldBeGreaterThanOrEqualTo(expected, [&]() -> char const* const { return line; });
        }

        void ShouldBeLessThanOrEqualTo(T expected, std::function<char const* const()> customMessageMaker)
        {
            if (!(actual <= expected))
                ThrowAwesomelyVal<T>(actual, expected, customMessageMaker(), "less than");
        }

        void ShouldBeLessThanOrEqualTo(T expected, char const* const customMessage)
        {
            ShouldBeLessThanOrEqualTo(expected, [&]() -> char const* const { return customMessage; });
        }

        inline void ShouldBeLessThanOrEqualTo(T expected)
        {
            ShouldBeLessThanOrEqualTo(expected, [&]() -> char const* const { return line; });
        }

        void ShouldBeInRangeTo(T expected, std::function<char const* const()> customMessageMaker, T range)
        {
            if (!(actual >= expected - range && actual <= expected + range))
                ThrowAwesomelyVal<T>(actual, expected, customMessageMaker(), "range than");
        }

        void ShouldBeInRangeTo(T expected, char const* const customMessage, T range)
        {
            ShouldBeInRangeTo(expected, [&]() -> char const* const { return customMessage; }, range);
        }

        inline void ShouldBeInRangeTo(T expected, T range)
        {
            ShouldBeInRangeTo(expected, [&]() -> char const* const { return line; }, range);
        }
    };

    template <typename T>
    struct ShouldlyRef
    {
        T const& actual;
        char const* const line;

        ShouldlyRef(T const& t, char const* const _line)
            : actual(t)
            , line(_line)
        {
        }

        void ShouldBe(T const& expected, std::function<char const* const()> customMessageMaker)
        {
            if (!(actual == expected))
                ThrowAwesomelyRef<T>(actual, expected, customMessageMaker(), "equal");
        }

        void ShouldBe(T const& expected, char const* const customMessage)
        {
            ShouldBe(expected, [&]() -> char const* const { return customMessage; });
        }

        void ShouldNotBe(T const& expected, std::function<char const* const()> customMessageMaker)
        {
            if (!(actual != expected))
                ThrowAwesomelyRef<T>(actual, expected, customMessageMaker(), "not equal");
        }

        void ShouldNotBe(T const& expected, char const* const customMessage)
        {
            ShouldNotBe(expected, [&]() -> char const* const { return customMessage; });
        }
    };

    template <typename T>
    struct ShouldlyStr
    {
        T const& actual;
        char const* const line;

        ShouldlyStr(T const& t, char const* const _line)
            : actual(t)
            , line(_line)
        {
        }

        void ShouldBe(T const& expected, std::function<char const* const()> customMessageMaker)
        {
            if (!(actual == expected))
                ThrowAwesomelyStr<T>(actual, expected, customMessageMaker(), "equal");
        }

        void ShouldBe(T const& expected, char const* const customMessage)
        {
            ShouldBe(expected, [&]() -> char const* const { return customMessage; });
        }

        void ShouldBe(T const& expected)
        {
            ShouldBe(expected, [&]() -> char const* const { return line; });
        }

        void ShouldNotBe(T const& expected, std::function<char const* const()> customMessageMaker)
        {
            if (!(actual != expected))
                ThrowAwesomelyStr<T>(actual, expected, customMessageMaker(), "not equal");
        }

        void ShouldNotBe(T const& expected, char const* const customMessage)
        {
            ShouldNotBe(expected, [&]() -> char const* const { return customMessage; });
        }

        void ShouldNotBe(T const& expected)
        {
            ShouldNotBe(expected, [&]() -> char const* const { return line; });
        }
    };

    template <typename T>
    struct ShouldlyPtr
    {
        T const* const actual;
        char const* const line;

        ShouldlyPtr(T const* const t, char const* const _line)
            : actual(t)
            , line(_line)
        {
        }

        void ShouldBeNull(std::function<char const* const()> customMessageMaker)
        {
            if (!(actual == nullptr))
                ThrowAwesomelyVal((unsigned long long)actual, (unsigned long long)nullptr, customMessageMaker(), "equal");
        }

        void ShouldBeNull(char const* const customMessage)
        {
            ShouldBeNull([&]() -> char const* const { return customMessage; });
        }

        void ShouldBeNull()
        {
            ShouldBeNull([&]() -> char const* const { return line; });
        }

        void ShouldNotBeNull(std::function<char const* const()> customMessageMaker)
        {
            if (!(actual != nullptr))
                ThrowAwesomelyVal((unsigned long long)actual, (unsigned long long)nullptr, customMessageMaker(), "not equal");
        }

        void ShouldNotBeNull(char const* const customMessage)
        {
            ShouldNotBeNull([&]() -> char const* const { return customMessage; });
        }

        void ShouldNotBeNull()
        {
            ShouldNotBeNull([&]() -> char const* const { return line; });
        }
    };

    template <typename T>
    struct ShouldlyAry
    {
        T const* const actual;
        size_t const len;
        char const* const line;

        ShouldlyAry(T const* const t, size_t const n, char const* const _line)
            : actual(t)
            , len(n)
            , line(_line)
        {
        }

        void ShouldBeNull(std::function<char const* const()> customMessageMaker)
        {
            if (!(actual == nullptr))
                ThrowAwesomelyVal((unsigned long long)actual, (unsigned long long)nullptr, customMessageMaker(), "equal");
        }

        void ShouldBeNull(char const* const customMessage)
        {
            ShouldBeNull([&]() -> char const* const { return customMessage; });
        }

        void ShouldBeNull()
        {
            ShouldBeNull([&]() -> char const* const { return line; });
        }

        void ShouldNotBeNull(std::function<char const* const()> customMessageMaker)
        {
            if (!(actual != nullptr))
                ThrowAwesomelyVal((unsigned long long)actual, (unsigned long long)nullptr, customMessageMaker(), "not equal");
        }

        void ShouldNotBeNull(char const* const customMessage)
        {
            ShouldNotBeNull([&]() -> char const* const { return customMessage; });
        }

        void ShouldNotBeNull()
        {
            ShouldNotBeNull([&]() -> char const* const { return line; });
        }

        void ShouldBeNullOrEmpty(std::function<char const* const()> customMessageMaker)
        {
            if (!(actual == nullptr))
                ThrowAwesomelyVal((unsigned long long)actual, (unsigned long long)nullptr, customMessageMaker(), "equal");
            if (!(len == 0))
                ThrowAwesomelyVal((unsigned long long)actual, (unsigned long long)0, customMessageMaker(), "equal");
        }

        void ShouldBeNullOrEmpty(char const* const customMessage)
        {
            ShouldBeNullOrEmpty([&]() -> char const* const { return customMessage; });
        }

        void ShouldBeNullOrEmpty()
        {
            ShouldBeNullOrEmpty([&]() -> char const* const { return line; });
        }

        void ShouldNotBeNullOrEmpty(std::function<char const* const()> customMessageMaker)
        {
            if (!(actual != nullptr))
                ThrowAwesomelyVal((unsigned long long)actual, (unsigned long long)nullptr, customMessageMaker(), "not equal");
            if (!(len != 0))
                ThrowAwesomelyVal((unsigned long long)actual, (unsigned long long)0, customMessageMaker(), "not equal");
        }

        void ShouldNotBeNullOrEmpty()
        {
            ShouldNotBeNullOrEmpty([&]() -> char const* const { return line; });
        }

        void ShouldNotBeNullOrEmpty(char const* const customMessage)
        {
            ShouldNotBeNullOrEmpty([&]() -> char const* const { return customMessage; });
        }

        void ShouldNotBeNullOrEmptyOrBe(T const def, std::function<char const* const()> customMessageMaker)
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

        void ShouldNotBeNullOrEmptyOrBe(T const def, char const* const customMessage)
        {
            ShouldNotBeNullOrEmptyOrBe(def, [&]() -> char const* const { return customMessage; });
        }

        void ShouldNotBeNullOrEmptyOrBe(T const def)
        {
            ShouldNotBeNullOrEmptyOrBe(def, [&]() -> char const* const { return line; });
        }

        void ShouldNotBeNullOrEmptyButBe(T const def, std::function<char const* const()> customMessageMaker)
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

        void ShouldNotBeNullOrEmptyButBe(T const def, char const* const customMessage)
        {
            ShouldNotBeNullOrEmptyButBe(def, [&]() -> char const* const { return customMessage; });
        }

        void ShouldNotBeNullOrEmptyButBe(T const def)
        {
            ShouldNotBeNullOrEmptyButBe(def, [&]() -> char const* const { return line; });
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

        void ShouldNotBeNullOrEmptyButBeNearlyEqual(T const def, std::function<char const* const()> customMessageMaker, T const e = (T)1, double regularize = 0, double diff = 0)
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
                if (fpclassify(diff) == FP_ZERO)
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

        void ShouldNotBeNullOrEmptyButBeNearlyEqual(T const def, char const* const customMessage, T const e = (T)6)
        {
            ShouldNotBeNullOrEmptyButBeNearlyEqual(def, [&]() -> char const* const { return customMessage; }, e);
        }

        void ShouldNotBeNullOrEmptyButBeNearlyEqual(T const def, T const e = (T)6)
        {
            ShouldNotBeNullOrEmptyButBeNearlyEqual(def, [&]() -> char const* const { return line; }, e);
        }

        void ShouldNotBeNullOrEmptyButBeNearlyEqual(T const* const def, std::function<char const* const()> customMessageMaker, T const e = (T)1, double regularize = 0, double diff = 0)
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
                if (fpclassify(diff) == FP_ZERO)
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

        void ShouldNotBeNullOrEmptyButBeNearlyEqual(T const* const def, char const* const customMessage, T const e = (T)6)
        {
            ShouldNotBeNullOrEmptyButBeNearlyEqual(def, [&]() -> char const* const { return customMessage; }, e);
        }

        void ShouldNotBeNullOrEmptyButBeNearlyEqual(T const* const def, T const e = (T)6)
        {
            ShouldNotBeNullOrEmptyButBeNearlyEqual(def, [&]() -> char const* const { return line; }, e);
        }

        void ShouldNotBeNullOrEmptyButMatchContents(T const* const comp, std::function<char const* const()> customMessageMaker)
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

        void ShouldNotBeNullOrEmptyButMatchContents(T const* const comp, char const* const customMessage)
        {
            ShouldNotBeNullOrEmptyButMatchContents(comp, [&]() -> char const* const { return customMessage; });
        }

        void ShouldNotBeNullOrEmptyButMatchContents(T const* const comp)
        {
            ShouldNotBeNullOrEmptyButMatchContents(comp, [&]() -> char const* const { return line; });
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
    inline auto Array(T const* const t, size_t const n, char const* const line)
    {
        return ShouldlyAry<T>(t, n, line);
    }
} // namespace test
} // namespace vaml

#define ShouldlyIntegral(stuff) vaml::test::Integral(stuff, #stuff)
#define ShouldlyString(stuff) vaml::test::String(stuff, #stuff)
#define ShouldlyPointer(stuff) vaml::test::Pointer(stuff, #stuff)
#define ShouldlyArray(stuff, num) vaml::test::Array(stuff, num, #stuff)

#endif // VAML_CORE_ASSERT_SHOULDLY_H
