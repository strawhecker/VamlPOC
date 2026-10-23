// S224 (dep:S223) — the viewer CLI `--help` ENTRY seam (headless Rail 1).
//
// Proves the S224 `--help`/`-h` usage decision WITHOUT a live window/Vulkan runtime
// (the standing S160+ precedent), through the PURE CLI seams
// (vaml_viewer/cli.h, compiled into the test target = the ADR-021 consumer path,
// single source of truth across both apps + the test):
//   (1) `cli_action(argc, argv)` — Run / Help / NoArg determinism.
//   (2) `cli_usage()` — the usage text (non-empty, documents pan/zoom + reload/drop).
// The EXIT-CODE contract (0 = --help on stdout; 2 = no-arg usage on stderr) is
// asserted in the live verify_s224.cmd CLI proofs; this TU proves the DECISION.

#include "cli.h"   // the S224 CLI entry seams

#include "doctest/doctest.h"

#include <cstring>

using namespace vaml::viewer;

TEST_CASE("S224: --help -> Help action (long form)")
{
    const char* argv[] = { "vaml_viewer", "--help" };
    CHECK(cli_action(2, argv) == CliAction::Help);
}

TEST_CASE("S224: -h -> Help action (short form)")
{
    const char* argv[] = { "vaml_viewer", "-h" };
    CHECK(cli_action(2, argv) == CliAction::Help);
}

TEST_CASE("S224: --help among other args -> Help (help wins wherever it appears)")
{
    // Even with a trailing positional, an explicit --help still means Help.
    const char* argv1[] = { "vaml_viewer", "scene.vaml", "--help" };
    CHECK(cli_action(3, argv1) == CliAction::Help);
    const char* argv2[] = { "vaml_viewer", "--help", "scene.vaml" };
    CHECK(cli_action(3, argv2) == CliAction::Help);
}

TEST_CASE("S224: no argument at all -> NoArg (usage on stderr, exit 2)")
{
    const char* argv[] = { "vaml_viewer" };
    CHECK(cli_action(1, argv) == CliAction::NoArg);
    // argc <= 1 (including a null argv) is safe + deterministic.
    CHECK(cli_action(1, nullptr) == CliAction::NoArg);
    CHECK(cli_action(0, nullptr) == CliAction::NoArg);
}

TEST_CASE("S224: a positional argument -> Run")
{
    const char* argv[] = { "vaml_viewer", "scene.vaml" };
    CHECK(cli_action(2, argv) == CliAction::Run);
}

TEST_CASE("S224: a non-help long option is treated as a positional (Run), not help")
{
    // "--verbose" must NOT be mistaken for --help (only the exact --help/-h tokens).
    const char* argv[] = { "vaml_viewer", "--verbose" };
    CHECK(cli_action(2, argv) == CliAction::Run);
}

TEST_CASE("S224: cli_usage is non-empty and documents the pan/zoom + reload/drop view")
{
    const char* u = cli_usage();
    REQUIRE(u != nullptr);
    CHECK(std::strlen(u) > 0u);
    // The usage names the entry + the S224 rendered-view pan/zoom + live reload/drop.
    CHECK(std::strstr(u, "vaml_viewer") != nullptr);
    CHECK(std::strstr(u, "PAN") != nullptr);
    CHECK(std::strstr(u, "ZOOM") != nullptr);
    CHECK(std::strstr(u, "--help") != nullptr);
}

TEST_CASE("S224: loaded_filename derives the basename (title = loaded filename polish)")
{
    CHECK(loaded_filename("/home/robert/scene.vaml") == "scene.vaml");
    CHECK(loaded_filename("C:/scenes/Scene.VAML") == "Scene.VAML");
    CHECK(loaded_filename("C:\\scenes\\win.vaml") == "win.vaml");   // Windows separator
    CHECK(loaded_filename("mixed/a\\b.vaml") == "b.vaml");          // last separator wins
    CHECK(loaded_filename("bare.vaml") == "bare.vaml");             // no separator
    CHECK(loaded_filename("dir/") == "");                           // trailing separator
}
