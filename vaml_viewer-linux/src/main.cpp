// vaml_viewer-linux — the single official VAML consumer app (Linux).
//
// S220 scaffold: MINIMAL version-print consumer scaffold. S221 (this step)
// adds the REAL CLI entry: `vaml_viewer <file.vaml>` → load_file → live tree,
// or a TYPED error + non-zero exit on missing/unreadable/parse-error (NEVER
// silent). As on Windows, this file is THIN CLI GLUE ONLY; the shared seam
// (../vaml_viewer/load_file.{h,cpp}) is the HEADLESS-ctestable entry path,
// single source of truth across the apps + the test target (ADR-021 consumer).
//
// CONSUMER-APP PATTERN (ADR-021): this app includes ONLY public library headers
// (`<vaml/...>` from the public include roots) and owns its own `main()`.

#include "load_file.h"

#include <cstdio>
#include <string>

namespace {

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

    std::printf("vaml_viewer-linux: loaded '%s' OK (live tree)\n", path.c_str());
    std::printf("  root: %s\n", result.root ? "DependencyObject" : "null");
    return 0;
}
