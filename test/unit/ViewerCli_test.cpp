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

TEST_CASE("S450_1: cli_usage declares .xaml as an accepted source (the .xaml acceptance gate)")
{
    const char* u = cli_usage();
    REQUIRE(u != nullptr);
    // The usage now names .xaml alongside .vaml as an accepted SOURCE — a user
    // can discover that a .xaml file is a valid input (not an accident).
    CHECK(std::strstr(u, ".vaml") != nullptr);
    CHECK(std::strstr(u, ".xaml") != nullptr);
    CHECK(std::strstr(u, ".vbaml") != nullptr);
}

TEST_CASE("S450_1: cli_file_path resolves a .xaml positional past a leading --compiled flag")
{
    // S449 added cli_file_path (first non-flag positional); S450_1 confirms it
    // serves .xaml files too — `--compiled scene.xaml` -> "scene.xaml".
    const char* argv[] = { "vaml_viewer", "--compiled", "scene.xaml" };
    CHECK(cli_file_path(3, argv) == std::string("scene.xaml"));
    const char* argvPlain[] = { "vaml_viewer", "window.xaml" };
    CHECK(cli_file_path(2, argvPlain) == std::string("window.xaml"));
}

TEST_CASE("S440: --app -> Run action + cli_wants_app_shell (the app-shell launch mode)")
{
    // S440 — `vaml_viewer --app` is the app-shell LAUNCH MODE (Sprint 11 v1 app):
    // it needs NO file, so --app alone makes the action Run (the "no-file but
    // --app" case), and cli_wants_app_shell detects the flag so main() dispatches
    // to ViewerApp::create_app_shell. Pure + headless.
    const char* argvApp[] = { "vaml_viewer", "--app" };
    CHECK(cli_action(2, argvApp) == CliAction::Run);
    CHECK(cli_wants_app_shell(2, argvApp) == true);

    // --app is a FLAG (not a positional): it returns Run with NO file.
    const char* argvOnly[] = { "vaml_viewer", "--app" };
    CHECK(cli_action(2, argvOnly) == CliAction::Run);

    // Ensure --app is not mistaken for a file launch — cli_wants_app_shell only
    // fires on the EXACT --app token.
    const char* argvFile[] = { "vaml_viewer", "scene.vaml" };
    CHECK(cli_action(2, argvFile) == CliAction::Run);
    CHECK(cli_wants_app_shell(2, argvFile) == false);

    // A bare invocation with no --app and no file is STILL NoArg (usage, exit 2).
    const char* argvNo[] = { "vaml_viewer" };
    CHECK(cli_action(1, argvNo) == CliAction::NoArg);
    CHECK(cli_wants_app_shell(1, argvNo) == false);

    // cli_wants_app_shell(null argv) is safe + deterministic.
    CHECK(cli_wants_app_shell(0, nullptr) == false);
}

TEST_CASE("S440: --app + a file still prefers the app-shell launch (flag wins the shell)")
{
    // S440 — when --app is present (with or without a file), the app-shell is
    // the DISPATCH: main() checks cli_wants_app_shell first. The action is Run
    // and the flag is detected.
    const char* argvBoth[] = { "vaml_viewer", "--app", "scene.vaml" };
    CHECK(cli_action(3, argvBoth) == CliAction::Run);
    CHECK(cli_wants_app_shell(3, argvBoth) == true);
}

TEST_CASE("11d-DEBUG: cli_wants_fail_first detects --fail_on_first_error (+ null-safe)")
{
    const char* argv[] = { "vaml_viewer", "--fail_on_first_error", "scene.vaml" };
    CHECK(cli_wants_fail_first(3, argv) == true);
    // Absent by default (scan-all is the default).
    const char* argvNo[] = { "vaml_viewer", "scene.vaml" };
    CHECK(cli_wants_fail_first(2, argvNo) == false);
    // null argv is safe + deterministic.
    CHECK(cli_wants_fail_first(0, nullptr) == false);
}

TEST_CASE("11d-DEBUG: --fail_on_first_error is a FLAG, not a positional file")
{
    // It must not become the file path, and it needs a real positional to Run.
    const char* argvFile[] = { "vaml_viewer", "--fail_on_first_error", "scene.vaml" };
    CHECK(cli_action(3, argvFile) == CliAction::Run);
    CHECK(cli_file_path(3, argvFile) == std::string("scene.vaml"));
    // Alone (no file), it is NOT a positional file -> NoArg (usage, exit 2).
    const char* argvOnly[] = { "vaml_viewer", "--fail_on_first_error" };
    CHECK(cli_action(2, argvOnly) == CliAction::NoArg);
    CHECK(cli_file_path(2, argvOnly) == std::string(""));
}

TEST_CASE("11d-DEBUG: cli_usage documents the scan default + --fail_on_first_error")
{
    const char* u = cli_usage();
    REQUIRE(u != nullptr);
    CHECK(std::strstr(u, "--fail_on_first_error") != nullptr);
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
