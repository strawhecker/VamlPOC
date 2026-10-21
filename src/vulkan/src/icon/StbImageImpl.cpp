// S202_1 — stb_image implementation TU (single-TU define discipline, mirroring
// VMA). Declares STB_IMAGE_IMPLEMENTATION exactly once in this process so the
// stb_image function bodies (the PNG decode path) exist and link against the
// S202_1 icon::raster_icon surface, which includes the header WITHOUT the
// implementation macro. stb_image is public-domain, pure C, header-only,
// dependency-free.
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
