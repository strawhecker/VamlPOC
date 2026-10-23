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

#include <cstdio>
#include <string>
#include <utility>   // std::move

namespace {

int render_init_failed(const std::string& path)
{
    std::fprintf(stderr, "vaml_viewer: render-init failed for '%s' (window/Vulkan/render)\n",
                 path.c_str());
    return 4;
}

int usage()
{
    std::fprintf(stderr, "usage: vaml_viewer <file.vaml>\n");
    return 2;
}

} // namespace

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        return usage();
    }

    const std::string path = argv[1];
    const vaml::LoadResult result = vaml::load_file(path);

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
    auto app = vaml::viewer::ViewerApp::create(config, std::move(result));
    if (!app)
        return render_init_failed(path);

    std::printf("vaml_viewer-linux: rendering '%s' (loaded tree -> retained render path)\n", path.c_str());
    return app->run_loop();
}
