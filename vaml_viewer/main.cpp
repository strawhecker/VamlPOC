// vaml_viewer — the single official VAML consumer app (Windows).
//
// S220 scaffold: this file is a MINIMAL consumer scaffold only — zero feature
// code. It proves that a standalone app links the built VAML libraries through
// their PUBLIC surface (vaml-core's version API) and prints the version, exactly
// the way the deleted POC harnesses did. The real viewer features build up from
// here across Sprint 5a (CLI entry = S221, "soup-to-nuts" render = S222, ...).
//
// CONSUMER-APP PATTERN (ADR-021): this app includes ONLY public library
// headers (`<vaml/...>` from the public include roots) and owns its own `main()`.
// It never includes an internal header from under src/<lib>/src and never
// reaches into a library's private implementation.

#include <vaml/version.h>

#include <cstdio>

int main()
{
    const vaml::Version ver = vaml::version();
    std::printf("vaml_viewer: linking %.*s %d.%d.%d\n",
                static_cast<int>(vaml::engine_name().size()),
                vaml::engine_name().data(),
                ver.major, ver.minor, ver.patch);
    return 0;
}
