// vaml_viewer — the single official VAML consumer app (Windows).
//
// S220 scaffold: MINIMAL version-print consumer scaffold. S221 added the REAL
// CLI entry (`vaml_viewer <file.vaml>` → load_file → live tree, or a TYPED
// error + non-zero exit). S222 (this step) makes the default action
// "soup-to-nuts": after the tree is loaded, open a REAL window + the Vulkan
// stack + `VamlRenderer::create_device` and render the DECLARED tree through
// the full retained-render path (S174 CompositionEngine + S173 Presenter +
// S207/S208 scene encoder + S210 SceneRenderer) via ONLY the public surface
// (ADR-021). This DEMONSTRATES S212_1 — create_device present — in the real
// window.
//
// This file is THIN CLI GLUE only — argv → the load_file seam → the shared
// ViewerApp host (app.{h,cpp}) → exit code. The host is the SINGLE SOURCE OF
// TRUTH for the live render path (shared verbatim with the Linux app + the
// test target, ADR-021 consumer path).
//
// exit codes: 0 = rendered; 2 = usage (no argument); 1 = missing file or read
//   error; 3 = the file read but VAML rejected it (parse error); 4 = the
//   window/Vulkan/render-init failed (never silent — a stderr diagnostic).
//   Every non-zero exit carries a stderr diagnostic — never silent.
//
// CONSUMER-APP PATTERN (ADR-021): this app includes ONLY public library
// headers (`<vaml/...>` from the public include roots) and owns its own
// `main()`. It never includes an internal header from under src/<lib>/src and
// never reaches into a library's private implementation.

#include "load_file.h"
#include "app.h"      // the S222 shared viewer render host
#include "cli.h"      // S224: cli_action / cli_usage (the PURE --help entry seam)

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <utility>   // std::move

namespace {

// Reports a non-silent render-init failure on stderr (exit 4).
int render_init_failed(const std::string& path)
{
    std::fprintf(stderr, "vaml_viewer: render-init failed for '%s' (window/Vulkan/render)\n",
                 path.c_str());
    return 4;
}

// S224 — the usage exit-code contract: --help/-h prints cli_usage() to STDOUT and
// returns 0; the no-arg case prints a terse note + the usage to STDERR and returns 2.
int usage_noarg()
{
    std::fprintf(stderr, "%s\n", vaml::viewer::cli_usage());
    return 2;
}

int help_stdout()
{
    std::printf("%s", vaml::viewer::cli_usage());
    return 0;
}

} // namespace

int main(int argc, char** argv)
{
    // S224 — the --help / no-arg CLI gate (the PURE seam, headless-testable). Help
    // wins over a positional whenever --help/-h appears; a missing argument -> the
    // usage on STDERR + exit 2 (the S221 never-silent contract).
    switch (vaml::viewer::cli_action(argc, argv))
    {
    case vaml::viewer::CliAction::Help:
        return help_stdout();
    case vaml::viewer::CliAction::NoArg:
        return usage_noarg();
    case vaml::viewer::CliAction::Run:
    default:
        break;   // fall through to the launch below
    }

    const std::string path = argv[1];
    vaml::LoadResult result = vaml::load_file(path);   // NON-const: moved into ViewerApp::create below

    if (!result.ok)
    {
        switch (result.error)
        {
        case vaml::FileLoadError::NoSuchFile:
            std::fprintf(stderr, "vaml_viewer: %s\n", result.detail.c_str());
            return 1;
        case vaml::FileLoadError::ReadError:
            std::fprintf(stderr, "vaml_viewer: %s\n", result.detail.c_str());
            return 1;
        case vaml::FileLoadError::ParseError:
            std::fprintf(stderr, "vaml_viewer: parse error: %s\n", result.detail.c_str());
            return 3;
        case vaml::FileLoadError::None:
        default:
            std::fprintf(stderr, "vaml_viewer: unexpected load failure\n");
            return 1;
        }
    }

    // S222: the loaded tree renders in a real window via the shared host. The
    // ViewerApp builds the window + Vulkan stack + VamlRenderer::create_device
    // and adopts the loaded tree. On ANY build failure -> exit 4 (never silent).
    vaml::viewer::ViewerAppConfig config;
    // S224 — title = the loaded FILENAME (basename), the "title = loaded filename" polish.
    config.title    = "VAML Viewer - " + vaml::viewer::loaded_filename(path);
    config.filePath = path;   // S222_1: arm the file-watch live reload on the opened .vaml
    auto app = vaml::viewer::ViewerApp::create(config, std::move(result));
    if (!app)
        return render_init_failed(path);

    std::printf("vaml_viewer: rendering '%s' (loaded tree -> retained render path)\n", path.c_str());
    return app->run_loop();
}
