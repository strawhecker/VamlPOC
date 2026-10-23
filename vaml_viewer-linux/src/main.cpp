// vaml_viewer-linux — the single official VAML consumer app (Linux).
//
// S220 scaffold: this file is a MINIMAL consumer scaffold only — zero feature
// code. It proves that a standalone app links the built VAML static libraries
// through their PUBLIC surface (vaml-core's version API) and prints the version,
// exactly the way the deleted VamlPOC-linux harness did.
//
// CONSUMER-APP PATTERN (ADR-021): this app includes ONLY public library headers
// (`<vaml/...>` from the public include roots) and owns its own `main()`.

#include <vaml/version.h>

#include <cstdio>

int main()
{
    const vaml::Version ver = vaml::version();
    std::printf("vaml_viewer-linux: linking %.*s %d.%d.%d\n",
                static_cast<int>(vaml::engine_name().size()),
                vaml::engine_name().data(),
                ver.major, ver.minor, ver.patch);
    return 0;
}
