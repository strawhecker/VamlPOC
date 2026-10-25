// vaml_viewer — CLI entry seams (S224, dep:S223).
//
// S224's contract: boot/window polish — `--help` usage. This header is the PURE
// CLI decision layer (mirroring the S222_1 live_reload.h / S223 drop_reopen.h
// split), proven headless (ctest without a live window) exactly like
// `reload_action` / `drop_choice`. It answers TWO headless questions:
//   1. `cli_action(argc, argv)` — what should main() do? Run / Help / NoArg.
//   2. `cli_usage()` — the usage text main() prints (stdout for --help, stderr
//      for the no-arg case).
//
// The SPLIT (the standing S222 precedent):
//   * PURE seams here (inline, header-only, shared verbatim by BOTH apps + the
//     test target — the ADR-021 consumer path): `CliAction` + `cli_action` +
//     `cli_usage` + the exit-code contract.
//   * The REAL dispatch lives in each app's `main()` (thin glue): Help -> print
//     usage to STDOUT + return 0; NoArg -> print usage to STDERR + return 2;
//     Run -> the existing launch (path = argv[1]).
//
// Exit-code contract (the S221 never-silent contract, extended at S224):
//   0 = --help/-h usage printed (stdout); 2 = no argument (usage to stderr);
//   the S221 codes stay unchanged (1 = missing/read error, 3 = parse error,
//   4 = render-init failure).
//
// NO MINIMAL REAL introduced — the CLI seams are REAL permanent code,
// kept-and-extended (loaded-filename title polish consumes them; the app frames
// main() around them on both platforms).

#pragma once

#include <cstddef>
#include <string>

namespace vaml::viewer {

// What main() should do, derived from the raw argv (pure). Deterministic.
enum class CliAction
{
    Run,    // a positional <file.vaml> argument present, OR --app requested -> launch the viewer
    Help,   // --help / -h requested -> print usage to stdout, exit 0
    NoArg,  // no argument AND no help flag AND no --app -> usage to stderr, exit 2
};

// Scans argv[1..argc) for the help flag. --help / -h (case-sensitive, the
// conventional GNU short+long forms) -> Help. NO positional argument at all
// (other than the program name) -> NoArg. Otherwise -> Run. `argv` may be
// nullptr when argc <= 1 (still safe). Pure + headless-testable.
//
// S440 — the --app flag: `vaml_viewer --app` launches the in-repo app-shell
// (the Sprint 11 v1 app) with NO file. It is a LAUNCH FLAG, not a positional:
// --app alone makes the action Run (the "no-file but --app" case — the app-shell
// is the default shell in that mode), exactly as a positional file does.
[[nodiscard]] inline CliAction cli_action(int argc, const char* const* argv) noexcept
{
    bool sawArg = false;
    bool sawApp = false;
    if (argv)
    {
        for (int i = 1; i < argc; ++i)
        {
            if (argv[i] == nullptr)
                continue;
            const char* a = argv[i];
            // The help flag (exact token; a positional that merely STARTS with
            // "--" but is not --help/-h is NOT help — it is an arg).
            if (a[0] == '-' && a[1] == '-')
            {
                // A "long" token: only "--help" is help; anything else is an arg.
                if (a[2] == 'h' && a[3] == 'e' && a[4] == 'l' && a[5] == 'p' && a[6] == '\0')
                    return CliAction::Help;
                // --compiled is a flag (not a positional); a Run still requires
                // a positional file argument to launch.
                if (a[2] == 'c' && a[3] == 'o' && a[4] == 'm' && a[5] == 'p' && a[6] == 'i' &&
                    a[7] == 'l' && a[8] == 'e' && a[9] == 'd' && a[10] == '\0')
                    sawArg = false;
                // --app is the S440 app-shell LAUNCH FLAG (not a positional):
                // it alone makes the action Run so `vaml_viewer --app` launches.
                else if (a[2] == 'a' && a[3] == 'p' && a[4] == 'p' && a[5] == '\0')
                    sawApp = true;
                else
                    sawArg = true;   // some other long option -> treated as a positional
            }
            else if (a[0] == '-' && a[1] == 'h' && a[2] == '\0')
            {
                return CliAction::Help;   // the short -h form
            }
            else
            {
                sawArg = true;   // a positional argument
            }
        }
    }
    return (sawArg || sawApp) ? CliAction::Run : CliAction::NoArg;
}

// Scans argv for the S293 `--compiled` flag (forces the compiled open path on
// a `.vaml` source; a `.vbaml` auto-detects compiled-anyway). Returns true only
// when the exact `--compiled` token appears. Pure + headless-testable.
[[nodiscard]] inline bool cli_wants_compiled(int argc, const char* const* argv) noexcept
{
    if (!argv)
    {
        return false;
    }
    for (int i = 1; i < argc; ++i)
    {
        if (argv[i] == nullptr)
            continue;
        const char* a = argv[i];
        if (a[0] == '-' && a[1] == '-' &&
            a[2] == 'c' && a[3] == 'o' && a[4] == 'm' && a[5] == 'p' && a[6] == 'i' &&
            a[7] == 'l' && a[8] == 'e' && a[9] == 'd' && a[10] == '\0')
        {
            return true;
        }
    }
    return false;
}

// Scans argv for the S440 `--app` flag (launches the in-repo APP-SHELL — the
// Sprint 11 v1 app — with NO file). Returns true only when the exact `--app`
// token appears. Pure + headless-testable.
[[nodiscard]] inline bool cli_wants_app_shell(int argc, const char* const* argv) noexcept
{
    if (!argv)
    {
        return false;
    }
    for (int i = 1; i < argc; ++i)
    {
        if (argv[i] == nullptr)
            continue;
        const char* a = argv[i];
        if (a[0] == '-' && a[1] == '-' &&
            a[2] == 'a' && a[3] == 'p' && a[4] == 'p' && a[5] == '\0')
        {
            return true;
        }
    }
    return false;
}

// The S224 `--help` usage text — the single source of truth for BOTH apps'
// main(). `--help` prints it to STDOUT (exit 0); the no-arg case prints it
// (or a short equivalent) to STDERR (exit 2). Pure + headless-testable.
[[nodiscard]] inline const char* cli_usage() noexcept
{
    return
        "usage: vaml_viewer [--app | --compiled] [<file>]\n"
        "\n"
        "Renders a VAML document to a window (file -> parse -> tree -> layout -> Vulkan render),\n"
        "or launches the in-repo app-shell (Sprint 11 v1 app) with --app and no file.\n"
        "\n"
        "Options:\n"
        "  -h, --help     show this help and exit\n"
        "  --app          launch the APP-SHELL (the in-repo v1 app): loads the authored\n"
        "                 MainView.vaml shell with NO file argument. [S440 VIEWER GROWTH: app]\n"
        "  --compiled     open via the COMPILED path: a .vaml is recompiled to .vbaml\n"
        "                 (vamlcc, the runtime VBamlReader) before render; a .vbaml is\n"
        "                 loaded directly. [S293 VIEWER GROWTH: compiled path]\n"
        "\n"
        "The rendered view supports PAN (left-drag) and ZOOM (mouse wheel). The window\n"
        "title shows the loaded file's name (or the app-shell title in --app mode). Live\n"
        "file-reload (S222_1) and drag-and-drop reopen (S223) are active when a file loads.\n";
}

// The S224 window-TITLE polish: the LOADED FILENAME (basename) derived from a
// full path. The viewer's window title shows the loaded file's NAME (not the whole
// path — the S224 "title = loaded filename" polish). Handles both '/' (POSIX) and
// '\\' (Windows) separators, and returns the input unchanged when it has no
// separator (a bare filename). Pure + headless-testable.
[[nodiscard]] inline std::string loaded_filename(const std::string& path)
{
    std::size_t pos = std::string::npos;
    const std::size_t slash = path.rfind('/');
    const std::size_t backslash = path.rfind('\\');
    if (slash != std::string::npos)
        pos = slash;
    if (backslash != std::string::npos && (pos == std::string::npos || backslash > pos))
        pos = backslash;
    return (pos == std::string::npos) ? path : path.substr(pos + 1);
}

} // namespace vaml::viewer
