// S203 — the single implementation TU for stb_truetype.
//
// STB_TRUETYPE_IMPLEMENTATION is defined EXACTLY ONCE per process here,
// mirroring the VMA single-TU discipline and the S202_1 StbImageImpl/NanosvgImpl
// precedent. FontAtlas.cpp includes the header WITHOUT the define; this TU emits
// the real implementation. The vendored header resolves via the SYSTEM include
// dir carried by vaml::image (the stb dir), so upstream vendor noise stays off
// our strict /W4 /WX bar.

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>
