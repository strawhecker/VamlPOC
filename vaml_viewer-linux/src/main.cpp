// vaml_viewer-linux — the single official VAML consumer app (Linux).
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
// This file is THIN CLI GLUE ONLY; the shared host
// (../vaml_viewer/app.{h,cpp}) is the SINGLE SOURCE OF TRUTH for the live
// render path, single source of truth across the apps + the test target
// (ADR-021 consumer). exit codes 0/1/2/3 mirror Windows (+4 render-init fail).
//
// CONSUMER-APP PATTERN (ADR-021): this app includes ONLY public library headers
// (`<vaml/...>` from the public include roots) and owns its own `main()`.

#include "load_file.h"
#include "app.h"
#include "cli.h"           // S224: cli_action / cli_usage (the PURE --help entry seam)
#include "compiled_open.h" // S293: load_compiled_file + CompiledOpenResult (the compiled .vbaml seam)

#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>   // std::move

namespace {

int render_init_failed(const std::string& path)
{
    std::fprintf(stderr, "vaml_viewer: render-init failed for '%s' (window/Vulkan/render)\n",
                 path.c_str());
    return 4;
}

// S224 — the usage exit-code contract (mirrors Windows): --help/-h -> stdout, 0;
// no-arg -> stderr, 2.
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
    // S224 — the --help / no-arg CLI gate (the PURE seam, headless-testable).
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

    // S293 — the COMPILED open dispatch (mirrors Windows). --compiled forces the
    // compiled path; a `.vbaml` auto-detects it. Compiled mode opens via the S7
    // runtime VBamlReader (load_compiled_file).
    const bool compiled = vaml::viewer::cli_wants_compiled(argc, argv);
    const std::string path = argv[1];

    if (compiled)
    {
        vaml::viewer::CompiledOpenResult cres = vaml::viewer::load_compiled_file(path);
        if (!cres.ok)
        {
            std::fprintf(stderr, "vaml_viewer: compiled open failed for '%s': %s\n",
                         path.c_str(), cres.detail.c_str());
            return 3;
        }

        vaml::viewer::ViewerAppConfig config;
        config.title    = "VAML Viewer - " + vaml::viewer::loaded_filename(path);
        config.filePath = path;   // S222_1 arm: reload recompiles + reopens (S293)
        auto app = vaml::viewer::ViewerApp::create_compiled(config, std::move(cres));
        if (!app)
            return render_init_failed(path);

        std::printf("vaml_viewer-linux: rendering '%s' via the COMPILED path (text->vamlcc->.vbaml->VBamlReader->render)\n",
                    path.c_str());
        return app->run_loop();
    }

    vaml::LoadResult result = vaml::load_file(path);   // NON-const: moved into ViewerApp::create below

    if (!result.ok)
    {
        switch (result.error)
        {
        case vaml::FileLoadError::NoSuchFile:
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

    vaml::viewer::ViewerAppConfig config;
    // S224 — title = the loaded FILENAME (basename), the "title = loaded filename" polish.
    config.title    = "VAML Viewer - " + vaml::viewer::loaded_filename(path);
    config.filePath = path;   // S222_1: arm the file-watch live reload on the opened .vaml
    auto app = vaml::viewer::ViewerApp::create(config, std::move(result));
    if (!app)
        return render_init_failed(path);

    std::printf("vaml_viewer-linux: rendering '%s' (loaded tree -> retained render path)\n", path.c_str());
    return app->run_loop();
}
