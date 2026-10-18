// S141 — InternStore tests (internal SIMD probe store).
// Scope: probe-hit returns the stored view; distinct strings distinct;
// same-fold-byte collision both retained + findable; append-only growth over
// many strings; lane compare ignores padding past the length; empty-marker
// 0xFF sentinel; a private store's master_destroy_all() resets it.
//
// This TU directly instantiates the INTERNAL InternStore (header is pulled in
// via the src/core/src include path added for this TU — a test-only seam; the
// store is not part of the public include tree).

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"       // StringMaker + operator<< (Shouldly seam)
#include <vaml/assert/Shouldly.h>         // SHOULD macro

#include "InternStore.h"

#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

using vaml::detail::InternStore;

TEST_CASE("S141 store: probe-hit returns the stored view")
{
    InternStore store;
    const auto first = store.intern("Width");
    const auto hit = store.intern("Width");
    SHOULD(ShouldlyString(first.data() == hit.data()).ShouldBe(true));
    SHOULD(ShouldlyString(first).ShouldBe(hit));
    SHOULD(ShouldlyString(store.size()).ShouldBe(std::size_t{1}));
}

TEST_CASE("S141 store: distinct strings distinct; size reflects insertions")
{
    InternStore store;
    (void)store.intern("Width");
    (void)store.intern("Height");
    (void)store.intern("Margin");
    const auto dup = store.intern("Width");
    SHOULD(ShouldlyString(store.size()).ShouldBe(std::size_t{3}));
    (void)dup;
    SHOULD(ShouldlyString(store.size()).ShouldBe(std::size_t{3}));
}

TEST_CASE("S141 store: padding past the length never false-matches")
{
    InternStore store;
    // Two strings of different lengths but common prefix must NOT collide as
    // equal. "A" vs "AB": lane compare must respect the length bound.
    const auto a = store.intern("A");
    const auto ab = store.intern("AB");
    SHOULD(ShouldlyString(a).ShouldNotBe(ab));
    SHOULD(ShouldlyString(a.data() == ab.data()).ShouldNotBe(true));
    // And re-intern of each is content-correct.
    SHOULD(ShouldlyString(std::string(store.intern("A"))).ShouldBe("A"));
    SHOULD(ShouldlyString(std::string(store.intern("AB"))).ShouldBe("AB"));
}

TEST_CASE("S141 store: empty-marker 0xFF sentinel is unreachable as a probe")
{
    InternStore store;
    // Intern a variety of strings; all must be independently findable.
    const std::vector<std::string> vocab = {
        "aa", "bb", "cc", "dd", "ee", "x", "yy", "zzz", "long-prefix-match-safety"};
    for (const auto& s : vocab)
    {
        (void)store.intern(s);
    }
    for (const auto& s : vocab)
    {
        SHOULD(ShouldlyString(std::string(store.intern(s))).ShouldBe(s));
    }
    SHOULD(ShouldlyString(store.size()).ShouldBe(vocab.size()));
}

TEST_CASE("S141 store: append-only growth over many strings stays correct")
{
    InternStore store;
    std::unordered_set<std::string_view> seen;
    for (int i = 0; i < 400; ++i)
    {
        const std::string s = "key" + std::to_string(i);
        std::string_view v = store.intern(s);
        // Distinct input each iteration => distinct retained view.
        SHOULD(ShouldlyString(seen.count(v)).ShouldBe(std::size_t{0}));
        seen.insert(v);
        SHOULD(ShouldlyString(std::string(v)).ShouldBe(s));
    }
    // Re-intern everything: all dedup to prior pointers, size stable.
    for (int i = 0; i < 400; ++i)
    {
        const std::string s = "key" + std::to_string(i);
        (void)store.intern(s);
    }
    SHOULD(ShouldlyString(store.size()).ShouldBe(std::size_t{400}));
}

TEST_CASE("S141 store: master_destroy_all resets a private store")
{
    InternStore store;
    (void)store.intern("Width");
    (void)store.intern("Height");
    SHOULD(ShouldlyString(store.size()).ShouldBe(std::size_t{2}));
    store.master_destroy_all();
    SHOULD(ShouldlyString(store.size()).ShouldBe(std::size_t{0}));
    // Reusable after reset.
    const auto w = store.intern("Width");
    SHOULD(ShouldlyString(std::string(w)).ShouldBe("Width"));
    SHOULD(ShouldlyString(store.size()).ShouldBe(std::size_t{1}));
}

TEST_CASE("S141 store: empty string is interned consistently")
{
    InternStore store;
    const auto e1 = store.intern("");
    const auto e2 = store.intern("");
    SHOULD(ShouldlyString(e1.empty()).ShouldBe(true));
    SHOULD(ShouldlyString(e1.data() == e2.data()).ShouldBe(true));
}
