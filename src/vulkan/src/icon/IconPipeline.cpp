// S202_1 — icon::raster_icon implementation: the IMPORT/RASTERIZATION seam.
//
// PNG → decoded with stb_image then resampled to the requested size;
// SVG → parsed with nanosvg then rasterized at the requested size. Both produce
// tightly-packed R8G8B8A8, wrapped in an IconRaster TAGGED with texelCount
// (== w*h) for S213_1 best-fit selection. This is the "rasterized ONCE" seam —
// callers (IconAtlas) cache the result; nothing re-rasterizes per-frame.
//
// Failure is ALWAYS structured and NON-CRASHING: bad bytes yield a false return
// + an IconErrorKind, never a throw, never a null deref, never a crash (ADR-015
// NO-GAP: the deliberate fallback GLYPH itself is S400_1's, not this module's).
//
// The two implementation TUs (StbImageImpl.cpp / NanosvgImpl.cpp) carry the
// STB_IMAGE_IMPLEMENTATION + NANOSVG_IMPLEMENTATION/NANOSVGRAST_IMPLEMENTATION
// defines exactly once; this TU includes the headers WITHOUT them.

#include <vaml/vulkan/icon/IconPipeline.h>

#include <nanosvg.h>
#include <nanosvgrast.h>
#include <stb_image.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace vaml::vulkan::icon {

namespace {

// Bounds the size of a legal raster so (w*h*4) cannot overflow/absurdly allocate.
// The S202 atlas contract keeps icons to modest tile sizes; this guard rejects
// degenerate or hostile dimensions.
constexpr std::uint32_t kMaxDimension = 16384u;  // 16384*16384*4 = 1 GiB cap

// A tiny bilinear sampler over a tightly-packed RGBA8 source. `u`,`v` are in
// normalized [0,1). Used to resample a decoded image to the requested size.
void sample_rgba(const std::uint8_t* src, std::uint32_t sw, std::uint32_t sh,
                 float u, float v, std::uint8_t out[4])
{
    const float fx = u * static_cast<float>(sw) - 0.5f;
    const float fy = v * static_cast<float>(sh) - 0.5f;
    const int x0 = static_cast<int>(std::floor(fx));
    const int y0 = static_cast<int>(std::floor(fy));
    const float tx = fx - static_cast<float>(x0);
    const float ty = fy - static_cast<float>(y0);
    const int x1 = std::min(x0 + 1, static_cast<int>(sw) - 1);
    const int y1 = std::min(y0 + 1, static_cast<int>(sh) - 1);
    const int xi0 = std::max(x0, 0);
    const int yi0 = std::max(y0, 0);
    const std::uint8_t* a = src + (yi0 * sw + xi0) * 4;
    const std::uint8_t* b = src + (yi0 * sw + x1) * 4;
    const std::uint8_t* c = src + (y1 * sw + xi0) * 4;
    const std::uint8_t* d = src + (y1 * sw + x1) * 4;
    for (int ch = 0; ch < 4; ++ch)
    {
        const float top    = (a[ch] * (1.0f - tx)) + (b[ch] * tx);
        const float bottom = (c[ch] * (1.0f - tx)) + (d[ch] * tx);
        out[ch] = static_cast<std::uint8_t>(top * (1.0f - ty) + bottom * ty + 0.5f);
    }
}

// Resamples a tightly-packed RGBA8 source (sw×sh) to dw×dh via bilinear.
std::vector<std::uint8_t> resize_rgba(const std::vector<std::uint8_t>& src,
                                      std::uint32_t sw, std::uint32_t sh,
                                      std::uint32_t dw, std::uint32_t dh)
{
    std::vector<std::uint8_t> dst(static_cast<std::size_t>(dw) * dh * 4u, 0u);
    for (std::uint32_t y = 0; y < dh; ++y)
    {
        const float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(dh);
        for (std::uint32_t x = 0; x < dw; ++x)
        {
            const float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(dw);
            std::uint8_t px[4];
            sample_rgba(src.data(), sw, sh, u, v, px);
            std::uint8_t* dstPx = dst.data() + (static_cast<std::size_t>(y) * dw + x) * 4u;
            dstPx[0] = px[0];
            dstPx[1] = px[1];
            dstPx[2] = px[2];
            dstPx[3] = px[3];
        }
    }
    return dst;
}

bool decode_png(const std::vector<std::uint8_t>& bytes,
                std::uint32_t sizeW, std::uint32_t sizeH,
                IconRaster& out) noexcept
{
    int w = 0, h = 0, comp = 0;
    // req_comp = 4 → stb_image returns tightly-packed RGBA8 (the S202 format).
    stbi_uc* px = stbi_load_from_memory(bytes.data(),
                                        static_cast<int>(bytes.size()),
                                        &w, &h, &comp, 4);
    if (!px || w <= 0 || h <= 0)
        return false;  // Malformed (or unsupported codec)
    if (static_cast<std::uint32_t>(w) > kMaxDimension ||
        static_cast<std::uint32_t>(h) > kMaxDimension)
    {
        stbi_image_free(px);
        return false;
    }
    const std::uint32_t sw = static_cast<std::uint32_t>(w);
    const std::uint32_t sh = static_cast<std::uint32_t>(h);
    const std::uint32_t swBytes = sw * sh * 4u;
    std::vector<std::uint8_t> src(px, px + swBytes);
    stbi_image_free(px);

    if (sw == sizeW && sh == sizeH)
    {
        out.w          = sizeW;
        out.h          = sizeH;
        out.rgba       = std::move(src);
        out.texelCount = sizeW * sizeH;
        return true;
    }
    out.w          = sizeW;
    out.h          = sizeH;
    out.rgba       = resize_rgba(src, sw, sh, sizeW, sizeH);
    out.texelCount = sizeW * sizeH;
    return true;
}

bool raster_svg(const std::vector<std::uint8_t>& bytes,
                std::uint32_t sizeW, std::uint32_t sizeH,
                IconRaster& out) noexcept
{
    // nanosvg parses in place and needs a NUL-terminated C string.
    std::vector<char> svgText(bytes.begin(), bytes.end());
    svgText.push_back('\0');
    NSVGimage* img = nsvgParse(svgText.data(), "px", 96.0f);
    if (!img)
        return false;  // Malformed SVG
    if (img->width <= 0.0f || img->height <= 0.0f)
    {
        nsvgDelete(img);
        return false;
    }
    NSVGrasterizer* rast = nsvgCreateRasterizer();
    if (!rast)
    {
        nsvgDelete(img);
        return false;
    }
    if (sizeW > kMaxDimension || sizeH > kMaxDimension ||
        sizeW == 0u || sizeH == 0u)
    {
        nsvgDeleteRasterizer(rast);
        nsvgDelete(img);
        return false;
    }
    // Fit (contain) the SVG's intrinsic aspect into sizeW×sizeH and centre it.
    const float scale = std::min(static_cast<float>(sizeW) / img->width,
                                 static_cast<float>(sizeH) / img->height);
    const float tx = (static_cast<float>(sizeW) - img->width * scale) * 0.5f;
    const float ty = (static_cast<float>(sizeH) - img->height * scale) * 0.5f;
    const std::size_t bytesSize = static_cast<std::size_t>(sizeW) * sizeH * 4u;
    std::vector<std::uint8_t> rgba(bytesSize, 0u);

    nsvgRasterize(rast, img, tx, ty, scale, rgba.data(),
                  static_cast<int>(sizeW), static_cast<int>(sizeH),
                  static_cast<int>(sizeW) * 4);

    nsvgDeleteRasterizer(rast);
    nsvgDelete(img);

    out.w          = sizeW;
    out.h          = sizeH;
    out.rgba       = std::move(rgba);
    out.texelCount = sizeW * sizeH;
    return true;
}

} // namespace

bool icon_raster_valid(const IconRaster& r) noexcept
{
    return r.w != 0u && r.h != 0u && r.texelCount == r.w * r.h &&
           r.rgba.size() == static_cast<std::size_t>(r.w) * r.h * 4u;
}

bool source_by_key(const IconSourceSet& set, std::uint32_t key,
                   IconSource& out) noexcept
{
    const auto it = set.find(key);
    if (it == set.end())
        return false;
    out = it->second;
    return true;
}

bool raster_icon(const IconSource& source,
                 std::uint32_t sizeW, std::uint32_t sizeH,
                 IconRaster& out,
                 IconErrorKind* outError) noexcept
{
    if (outError)
        *outError = IconErrorKind::None;

    if (sizeW == 0u || sizeH == 0u ||
        sizeW > kMaxDimension || sizeH > kMaxDimension)
    {
        if (outError)
            *outError = IconErrorKind::Unsupported;
        return false;
    }

    bool ok = false;
    switch (source.format)
    {
    case IconFormat::Png:
        ok = decode_png(source.bytes, sizeW, sizeH, out);
        break;
    case IconFormat::Svg:
        ok = raster_svg(source.bytes, sizeW, sizeH, out);
        break;
    default:
        if (outError)
            *outError = IconErrorKind::Unsupported;
        return false;
    }

    if (!ok)
    {
        if (outError)
            *outError = IconErrorKind::Malformed;
        return false;
    }
    if (!icon_raster_valid(out))
    {
        if (outError)
            *outError = IconErrorKind::Malformed;
        return false;
    }
    return true;
}

} // namespace vaml::vulkan::icon
