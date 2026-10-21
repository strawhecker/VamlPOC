// S202_1 — nanosvg implementation TU (single-TU define discipline, mirroring
// VMA). Declares NANOSVG_IMPLEMENTATION + NANOSVGRAST_IMPLEMENTATION exactly
// once in this process so the nanosvg parser + rasterizer function bodies (the
// SVG → RGBA8 path) exist and link against the S202_1 icon::raster_icon
// surface, which includes the headers WITHOUT the implementation macros.
// nanosvg is MIT, pure C, header-only, dependency-free. Its implementation body
// relies on the standard C89 runtime (<stdio.h> sscanf/FILE, <stdlib.h>
// malloc/free/abs) which a C compiler pulls in implicitly; under C++ the names
// must be declared, so include the standard headers BEFORE the implementation
// macro is seen.
#include <stdio.h>
#include <stdlib.h>

// Vendor code compiled inside this TU must not trip OUR strict bar (/W4 /WX on
// MSVC; -Werror on clang). nanosvg's implementation body emits benign upstream
// warnings (e.g. C4702 unreachable code in a helper under -O0 /RTC1). Silencing
// only THIS TU for the vendor scope is the standard vendor-quiet pattern — our
// own sources (IconPipeline.cpp/IconAtlas.cpp) stay fully strict.
#if defined(_MSC_VER)
#  pragma warning(push)
#  pragma warning(disable : 4702)  // unreachable code (nanosvg helper, benign)
#endif

#define NANOSVG_IMPLEMENTATION
#define NANOSVGRAST_IMPLEMENTATION
#include <nanosvg.h>
#include <nanosvgrast.h>

#if defined(_MSC_VER)
#  pragma warning(pop)
#endif
