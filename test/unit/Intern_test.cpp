// S140_1 — vaml::intern() tests.
// Scope: same-thread duplicate -> SAME pointer (content-dedup); distinct ->
// distinct; implicit std::string conversion; empty string consistent; a
// retained view stays valid after the source std::string dies (caller-
// independent lifetime); growth over many distinct strings; per-thread
// semantics (no cross-thread reachability assumption — cross-thread view
// passing is forbidden by the contract).

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"       // StringMaker + operator<< (Shouldly seam)
#include <vaml/assert/Shouldly.h>         // SHOULD macro

#include "vaml/core/Intern.h"

#include <string>
#include <string_view>

using vaml::intern;

TEST_CASE("S140_1 intern: duplicate spelling returns the SAME pointer (same thread)")
{
    const std::string_view first = intern("Width");
    const std::string_view second = intern("Width");
    // Content-dedup: identical spelling on the same thread is one view.
    SHOULD(ShouldlyString(first.data() == second.data()).ShouldBe(true));
    SHOULD(ShouldlyString(first.size()).ShouldBe(second.size()));
    SHOULD(ShouldlyString(first).ShouldBe(second));
}

TEST_CASE("S140_1 intern: distinct spellings return DISTINCT views")
{
    const std::string_view a = intern("Width");
    const std::string_view b = intern("Height");
    // Distinct strings may land on distinct addresses; what must hold is that
    // their CONTENT differs (equality is by content, never pointer).
    SHOULD(ShouldlyString(a).ShouldNotBe(b));
    SHOULD(ShouldlyString(a.data() == b.data()).ShouldNotBe(true));
}

TEST_CASE("S140_1 intern: accepts std::string via implicit conversion")
{
    const std::string owner = "Button";
    const std::string_view v = intern(owner);
    SHOULD(ShouldlyString(v.size()).ShouldBe(owner.size()));
    SHOULD(ShouldlyString(std::string(v)).ShouldBe(owner));
}

TEST_CASE("S140_1 intern: empty string is consistent")
{
    const std::string_view e1 = intern("");
    const std::string_view e2 = intern("");
    SHOULD(ShouldlyString(e1.empty()).ShouldBe(true));
    SHOULD(ShouldlyString(e2.empty()).ShouldBe(true));
    // Content-dedup also holds for the empty string (same thread).
    SHOULD(ShouldlyString(e1.data() == e2.data()).ShouldBe(true));
}

TEST_CASE("S140_1 intern: retained view outlives the source std::string")
{
    std::string_view view;
    {
        std::string transient = "TransientValue";
        view = intern(transient);
    }
    // temporary is gone; the intern view must still be valid + correct
    // (caller-independent lifetime — intern retains the bytes).
    SHOULD(ShouldlyString(std::string(view)).ShouldBe("TransientValue"));
}

TEST_CASE("S140_1 intern: many distinct strings grow the store without corruption")
{
    // Intern a large distinct vocabulary; each comes back content-correct.
    std::vector<std::string_view> views;
    for (int i = 0; i < 512; ++i)
    {
        const std::string s = "prop_" + std::to_string(i);
        views.push_back(intern(s));
    }
    // Spot-check dedup + content correctness across the growth.
    for (int i = 0; i < 512; ++i)
    {
        const std::string expect = "prop_" + std::to_string(i);
        SHOULD(ShouldlyString(std::string(views[static_cast<std::size_t>(i)])).ShouldBe(expect));
        // Re-intern returns the same content (and same pointer on this thread).
        const auto again = intern(expect);
        SHOULD(ShouldlyString(again.data() == views[static_cast<std::size_t>(i)].data()).ShouldBe(true));
    }
}

TEST_CASE("S140_1 intern: same spelling interned twice after interleaving stays deduped")
{
    const auto w1 = intern("Opacity");
    const auto h1 = intern("Visibility");
    const auto w2 = intern("Opacity");
    SHOULD(ShouldlyString(w1.data() == w2.data()).ShouldBe(true));
    SHOULD(ShouldlyString(h1.data() == w2.data()).ShouldNotBe(true));
}
