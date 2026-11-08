// S642 — CapabilityCatalogue_s642_test.cpp (the LIB side + the SYNC-TOKEN parity
// proof, headless Rail 1).
//
// Sprint 11r (S642): the LIB side of the typed capability catalogue (typed slots +
// icon + text) + the PARITY assertion — for EVERY capability, our-side == WPF-side
// == matrix (parity yes or a NAMED gap, never silent). This is the enforceable half
// of the SYNC TOKEN: a capability may only be introduced by updating OUR side AND
// the WPF-emitter side AND the matrix IN THE SAME STEP (no drift).
//
// PURE + headless (both CI lanes, no Vulkan runtime).

#include <vaml/presentation/scan/CapabilityCatalogue.h>

#include "doctest/doctest.h"

#include <string>

using namespace vaml::presentation::scan;

TEST_CASE("S642 the embedded catalogue loads (single source of truth)")
{
    const auto& cat = capability_catalogue();
    REQUIRE(capability_catalogue_loaded());
    CHECK(cat.version == 1);
    CHECK(cat.counts_are_soft_start);
}

TEST_CASE("S642 soft-start-count families: 10 int / 10 bool / 10 string / 10 double / 10 icon / 10 text")
{
    const auto s = summarize(capability_catalogue());
    CHECK(s.total == 60u);
    CHECK(s.ints    == 10u);
    CHECK(s.bools   == 10u);
    CHECK(s.strings == 10u);
    CHECK(s.doubles == 10u);
    CHECK(s.icons   == 10u);
    CHECK(s.texts   == 10u);
}

TEST_CASE("S642 every capability is catalogued with the right kind + tolerance")
{
    const auto& cat = capability_catalogue();
    // a typed int slot -> Solid quad + the tight tolerance
    const Capability* si = find_capability(cat, "slot_int_01");
    REQUIRE(si != nullptr);
    CHECK(si->kind == QuadKind::Solid);
    CHECK(si->type == "int");
    CHECK(si->tolerance == doctest::Approx(0.02));

    // a string slot -> Text quad + the intermediate tolerance
    const Capability* ss = find_capability(cat, "slot_string_01");
    REQUIRE(ss != nullptr);
    CHECK(ss->kind == QuadKind::Text);
    CHECK(ss->type == "string");
    CHECK(ss->tolerance == doctest::Approx(0.08));

    // an icon -> Icon quad + the loose tolerance
    const Capability* ic = find_capability(cat, "icon_01");
    REQUIRE(ic != nullptr);
    CHECK(ic->kind == QuadKind::Icon);
    CHECK(ic->tolerance == doctest::Approx(0.25));

    // a text -> Text quad
    const Capability* tx = find_capability(cat, "text_01");
    REQUIRE(tx != nullptr);
    CHECK(tx->kind == QuadKind::Text);
    CHECK(tx->type == "text");
    CHECK(tx->tolerance == doctest::Approx(0.08));
}

TEST_CASE("S642 catalogue_tolerance_for: catalogue wins, else the built-in kind default")
{
    // A catalogued capability's declared tolerance wins.
    const Capability* ic = find_capability(capability_catalogue(), "icon_01");
    REQUIRE(ic != nullptr);
    CHECK(catalogue_tolerance_for(*ic) == doctest::Approx(0.25));

    // An un-catalogued capability falls back to the S641 built-in kind default.
    Capability synthetic;
    synthetic.kind = QuadKind::Solid;   // tolerance 0 default -> fallback to built-in 0.02
    synthetic.tolerance = 0.0;
    CHECK(catalogue_tolerance_for(synthetic) == doctest::Approx(0.02));
}

TEST_CASE("S642 family counts mirror the catalogue families (soft start)")
{
    const auto& cat = capability_catalogue();
    CHECK(family_count(cat, "slot_int")    == 10u);
    CHECK(family_count(cat, "slot_bool")   == 10u);
    CHECK(family_count(cat, "slot_string") == 10u);
    CHECK(family_count(cat, "slot_double") == 10u);
    CHECK(family_count(cat, "icon")        == 10u);
    CHECK(family_count(cat, "text")        == 10u);
    CHECK(family_count(cat, "nonexistent") == 0u);
}

TEST_CASE("S642 SYNC TOKEN: every capability has parity yes or a NAMED gap (never silent)")
{
    // This is the enforceable half of the sync token — the parity guarantee is
    // declared in the embedded catalogue + confirmed by the repo sync-check script
    // over the SAME committed JSON + the generated matrix. No capability may be
    // parity=no without a named gap.
    const auto& cat = capability_catalogue();
    for (const auto& cap : cat.capabilities)
    {
        if (!cap.parity_yes)
        {
            // parity=no MUST carry a named, non-empty gap — never silent.
            INFO("capability " << cap.id << " is parity=no with an EMPTY gap");
            CHECK_FALSE(cap.gap.empty());
        }
        else
        {
            // parity=yes -> both sides are present (a legal status, never unset).
            const bool ourOk = cap.our == CapabilityStatus::Introduced ||
                               cap.our == CapabilityStatus::Done ||
                               cap.our == CapabilityStatus::Planned ||
                               cap.our == CapabilityStatus::OutOfScope;
            CHECK(ourOk);
            const bool wpfOk = cap.wpf == CapabilityStatus::Introduced ||
                               cap.wpf == CapabilityStatus::Done ||
                               cap.wpf == CapabilityStatus::Planned ||
                               cap.wpf == CapabilityStatus::OutOfScope;
            CHECK(wpfOk);
        }
    }
}

TEST_CASE("S642 the embedded catalogue is intact (spot-check family/tolerance agreement)")
{
    // A quick integrity cross-check: the embedded bytes must not drift from the
    // committed tools/capability_catalogue.json (the matrix + WPF side read the
    // SAME JSON). This asserts the spot-checked rows are internally consistent.
    const auto& cat = capability_catalogue();
    const Capability* d = find_capability(cat, "slot_double_05");
    REQUIRE(d != nullptr);
    CHECK(d->type == "double");
    CHECK(d->kind == QuadKind::Solid);
    CHECK(d->tolerance == doctest::Approx(0.02));

    const Capability* t = find_capability(cat, "text_07");
    REQUIRE(t != nullptr);
    CHECK(t->type == "text");
    CHECK(t->kind == QuadKind::Text);
    CHECK(t->tolerance == doctest::Approx(0.08));
}
