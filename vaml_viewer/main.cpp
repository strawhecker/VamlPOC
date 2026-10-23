// vaml_viewer — the single official VAML consumer app (Windows).
//
// S220 scaffold: MINIMAL version-print consumer scaffold. S221 (this step)
// adds the REAL CLI entry: `vaml_viewer <file.vaml>` → load_file → live tree,
// or a TYPED error + non-zero exit on missing/unreadable/parse-error (NEVER
// silent). This file is THIN CLI GLUE only — argv → the load_file seam → exit
// code. The seam (load_file.{h,cpp}) is the HEADLESS-ctestable entry path; it
// is shared verbatim with the Linux app and the test target (single source of
// truth, ADR-021 consumer path).
//
// exit codes: 0 = loaded the tree; 2 = usage (no argument); 1 = missing file
//   or read error; 3 = the file read but VAML rejected it (parse error).
//   A non-zero exit is emitted with a diagnostic on stderr — never silent.
//
// CONSUMER-APP PATTERN (ADR-021): this app includes ONLY public library
// headers (`<vaml/...>` from the public include roots) and owns its own
// `main()`. It never includes an internal header from under src/<lib>/src and
// never reaches into a library's private implementation.

#include "load_file.h"

#include <cstdio>
#include <string>

namespace {

// Renders the live-tree summary. S221's scope ends at "loaded the tree";
// rendering the declared UI is S222 ("soup-to-nuts"). Here we report the load
// succeeded and identify the root by its dynamic type name (the tree's root is
// the piece an operator can trust without a window yet).
void print_summary(const vaml::LoadResult& result, const std::string& path)
{
    std::printf("vaml_viewer: loaded '%s' OK (live tree)\n", path.c_str());
    std::printf("  root: %s\n", result.root ? "DependencyObject" : "null");
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

    print_summary(result, path);
    return 0;
}
