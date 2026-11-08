# ============================================================================
# VAML — FindImageDecoders.cmake  (S202_1)
#
# Locates the two CPU rasterizer headers (stb_image for PNG decode + nanosvg
# for SVG rasterize) and exposes a single interface target `vaml::image` that
# carries BOTH include dirs. This is the S202_1 decode/raster source-asset
# machinery: PNG and SVG are both SOURCE formats, each rasterized ONCE into the
# S202 atlas exactly as fonts are (S203/S204 model) — no vector-on-screen, no
# second renderer (ADR-015). The vendored headers live under the unified deps
# root (${VAML_DEPS_DIR}, the gitignored third_party/deps fetch folder) so every
# host (GH windows/ubuntu runners, the VM's shared-folder mirror, the laptop)
# compiles the IDENTICAL source — no per-host package-manager drift (the
# GLFW/VMA no-drift strategy). ADR-011: only vaml-vulkan may consume these.
#
# The headers are purely the COMPILE-TIME interface here. The single
# implementation TUs (src/icon/StbImageImpl.cpp with STB_IMAGE_IMPLEMENTATION,
# src/icon/NanosvgImpl.cpp with NANOSVG_IMPLEMENTATION + NANOSVGRAST_IMPLEMENTATION)
# define the implementations once per process, mirroring the VMA single-TU
# discipline. stb_image (public domain) + nanosvg (MIT) are both pure C/C++,
# header-only, dependency-free, and cross-compile on MSVC + clang.
# ============================================================================

if(TARGET vaml::image)
    return()
endif()

set(_vml_img_stb "${VAML_DEPS_DIR}/stb")
set(_vml_img_nano "${VAML_DEPS_DIR}/nanosvg")
if(NOT EXISTS "${_vml_img_stb}/stb_image.h" OR NOT EXISTS "${_vml_img_stb}/stb_truetype.h")
    message(FATAL_ERROR
        "stb_image.h / stb_truetype.h not found. Expected the vendored single "
        "headers at VAML_DEPS_DIR/stb/stb_image.h + stb_truetype.h. Vendor stb "
        "(S202_1 decode + S203 TrueType load) to keep the build drift-free.")
endif()
# S641: the UI-COMPLIANCE SCANNER's GIMP-style diff DEPOSITS a real PNG via the
# vendored stb_image_write single header (the same stb family + the ADR-011
# single-TU discipline; the STB_IMAGE_WRITE_IMPLEMENTATION define lives in
# PixelDiff.cpp). It rides the SAME vaml::image SYSTEM include interface the
# decode/raster headers use (ADR-011: only vaml-presentation/vulkan consume it).
if(NOT EXISTS "${_vml_img_stb}/stb_image_write.h")
    message(FATAL_ERROR
        "stb_image_write.h not found. Expected the vendored single header at "
        "VAML_DEPS_DIR/stb/stb_image_write.h (S641 diff-PNG writer). Vendor the "
        "stb family to keep the build drift-free.")
endif()
if(NOT EXISTS "${_vml_img_nano}/nanosvg.h" OR NOT EXISTS "${_vml_img_nano}/nanosvgrast.h")
    message(FATAL_ERROR
        "nanosvg not found. Expected the vendored single headers at "
        "VAML_DEPS_DIR/nanosvg/nanosvg.h + nanosvgrast.h. Vendor nanosvg to keep "
        "the build drift-free.")
endif()

add_library(vaml::image INTERFACE IMPORTED)
# SYSTEM so the vendored rasterizer headers are treated as external on every
# consumer/build (MSVC routes them to the /external: path; Clang/GCC -isystem),
# keeping the library + test strict-bar (/W4 /WX / -Werror) quiet on upstream
# vendor noise — the same intent as the test targets' SYSTEM deps root.
target_include_directories(vaml::image SYSTEM INTERFACE "${_vml_img_stb}" "${_vml_img_nano}")
set(VAML_IMAGE_FOUND TRUE CACHE INTERNAL "" FORCE)
