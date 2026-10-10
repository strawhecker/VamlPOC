// S005 — 0-feature smoke test.
// Proves the whole S002/S003/S005 chain from a single test harness:
//   - doctest is vendored at third_party/doctest (single header, v2.4.11).
//   - vaml-core actually links from an external consumer (the version symbol is
//     exercised below, not just the harness).
// Deliberately 0-feature by design (no product behavior). This file is permanent
// test infrastructure — it is NOT a STUB and NOT a MINIMAL REAL marker. It is
// kept and extended as CI grows.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

#include <vaml/version.h>

TEST_CASE("smoke")
{
    // The harness itself works.
    CHECK(true);

    // vaml-core is genuinely linked: exercise its real version() symbol.
    CHECK(vaml::version().major == 0);

    // Sanity on the engine identity (also links the compiled TU, not just header).
    CHECK(vaml::engine_name() == "vaml-core");
}
