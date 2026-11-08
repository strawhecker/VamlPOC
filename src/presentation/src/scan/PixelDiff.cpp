// S641 — PixelDiff.cpp (GIMP-style per-pixel diff + tolerance tool + PNG write).
//
// The colored diff image + auto-locate text tolerance band + full-missing-content
// detection, written as a REAL PNG via the vendored stb_image_write seam. Pure +
// headless (both CI lanes); encoding is pure data-munging, no window/GPU.

#include <vaml/presentation/scan/PixelDiff.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

// The stb_image_write IMPLEMENTATION is authored ONCE here (the ADR-011 single-TU
// discipline, mirroring StbImageImpl.cpp / NanosvgImpl.cpp). The header's export
// directive gates the single definition.
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace vaml::presentation::scan {

namespace {

std::uint8_t clamp_u8(int v) noexcept { return static_cast<std::uint8_t>(v < 0 ? 0 : (v > 255 ? 255 : v)); }

// Elementwise channel deviation as a 0..1 relative fraction (worst channel).
double channel_deviation(const std::uint8_t* a, const std::uint8_t* b)
{
    const int dr = std::abs((int)a[2] - (int)b[2]);
    const int dg = std::abs((int)a[1] - (int)b[1]);
    const int db = std::abs((int)a[0] - (int)b[0]);
    const double worst = static_cast<double>(std::max(dr, std::max(dg, db)));
    return worst / 255.0;
}

bool is_blank(const std::uint8_t* p) noexcept
{
    return (p[3] == 0u) || (p[3] < 8u && p[2] < 24u && p[1] < 24u && p[0] < 24u);
}

} // namespace

PixelDiff
pixel_diff(const Impl::ImageBuffer& actual, const Impl::ImageBuffer& expected,
           double tolerance)
{
    PixelDiff d;
    d.width  = actual.width;
    d.height = actual.height;
    const std::size_t n = static_cast<std::size_t>(d.width) * d.height;
    // A size mismatch cannot be diffed pixel-to-pixel — the whole frame is a diff
    // classification (never a silent pass). The image still gets a neutral canvas.
    d.image.resize(n * 4u, 0u);
    d.classifications.assign(n, DiffPixel::Different);
    // Retain the raw frames so a later per-quad band can re-evaluate them.
    d.actual_bgra.assign(actual.bgra.begin(), actual.bgra.begin() + std::min(actual.bgra.size(), n * 4u));
    d.expected_bgra.assign(expected.bgra.begin(), expected.bgra.begin() + std::min(expected.bgra.size(), n * 4u));

    const bool sameDims = (actual.width == expected.width && actual.height == expected.height);
    std::uint64_t* counts[3] = { &d.unchanged, &d.different, &d.missing };
    for (std::size_t idx = 0; idx < n; ++idx)
    {
        const std::uint8_t* a = actual.bgra.data() + idx * 4u;
        const std::uint8_t* e = expected.bgra.data() + idx * 4u;

        DiffPixel cls = DiffPixel::Different;
        if (!sameDims)
        {
            cls = DiffPixel::Different;
        }
        else
        {
            // Missing-content: blank in actual but inked in expected (beyond tolerance).
            if (is_blank(a) && !is_blank(e) && channel_deviation(a, e) > tolerance)
                cls = DiffPixel::MissingContent;
            else if (channel_deviation(a, e) <= tolerance)
                cls = DiffPixel::Unchanged;
            else
                cls = DiffPixel::Different;
        }

        ++(*counts[static_cast<int>(cls)]);
        d.classifications[idx] = cls;

        // Color-code the diff image.
        std::uint8_t* rgba = d.image.data() + idx * 4u;
        if (cls == DiffPixel::Unchanged)
        {
            // unchanged -> grey (the actual's color, desaturated toward grey).
            const int gray = (static_cast<int>(a[0]) + static_cast<int>(a[1]) + static_cast<int>(a[2])) / 3;
            rgba[0] = clamp_u8(gray);
            rgba[1] = clamp_u8(gray);
            rgba[2] = clamp_u8(gray);
            rgba[3] = 255u;
        }
        else if (cls == DiffPixel::MissingContent)
        {
            // red region (missing content).
            rgba[0] = 255u;
            rgba[1] = 0u;
            rgba[2] = 0u;
            rgba[3] = 255u;
        }
        else
        {
            // different -> HOT magenta/red.
            rgba[0] = 255u;
            rgba[1] = 0u;
            rgba[2] = 255u;
            rgba[3] = 255u;
        }
    }
    return d;
}

void classify_quad(PixelDiff& diff, float x, float y, float w, float h, double tolerance)
{
    // The per-quad tolerance band: RE-evaluate the pixels inside the declared quad
    // region against the RETAINED raw frames. This is where the roadmap's
    // "one blank, one present" -> full-missing-content catch fires EXACTLY at the
    // quad (not silently over the whole frame).
    if (diff.actual_bgra.size() < static_cast<std::size_t>(diff.width) * diff.height * 4u ||
        diff.expected_bgra.size() < static_cast<std::size_t>(diff.width) * diff.height * 4u)
        return; // no retained frames -> nothing to re-evaluate (never a silent pass claim)

    const std::uint32_t x0 = static_cast<std::uint32_t>(std::max(0, (int)x));
    const std::uint32_t y0 = static_cast<std::uint32_t>(std::max(0, (int)y));
    const std::uint32_t x1 = static_cast<std::uint32_t>(std::min((double)diff.width,  (double)x + w));
    const std::uint32_t y1 = static_cast<std::uint32_t>(std::min((double)diff.height, (double)y + h));
    if (x0 >= x1 || y0 >= y1)
        return; // degenerate quad (out of frame)

    for (std::uint32_t yy = y0; yy < y1; ++yy)
    {
        for (std::uint32_t xx = x0; xx < x1; ++xx)
        {
            const std::size_t idx = static_cast<std::size_t>(yy) * diff.width + xx;
            const std::uint8_t* a = diff.actual_bgra.data() + idx * 4u;
            const std::uint8_t* e = diff.expected_bgra.data() + idx * 4u;

            DiffPixel cls;
            if (is_blank(a) && !is_blank(e) && channel_deviation(a, e) > tolerance)
                cls = DiffPixel::MissingContent;
            else if (channel_deviation(a, e) <= tolerance)
                cls = DiffPixel::Unchanged;
            else
                cls = DiffPixel::Different;

            // Update the classification + the aggregate counters.
            DiffPixel& slot = diff.classifications[idx];
            switch (slot)
            {
                case DiffPixel::Unchanged:      --diff.unchanged; break;
                case DiffPixel::Different:      --diff.different; break;
                case DiffPixel::MissingContent: --diff.missing;   break;
            }
            switch (cls)
            {
                case DiffPixel::Unchanged:      ++diff.unchanged; break;
                case DiffPixel::Different:      ++diff.different; break;
                case DiffPixel::MissingContent: ++diff.missing;   break;
            }
            slot = cls;

            std::uint8_t* rgba = diff.image.data() + idx * 4u;
            if (cls == DiffPixel::Unchanged)
            {
                const int gray = (static_cast<int>(a[0]) + static_cast<int>(a[1]) + static_cast<int>(a[2])) / 3;
                rgba[0] = clamp_u8(gray);
                rgba[1] = clamp_u8(gray);
                rgba[2] = clamp_u8(gray);
                rgba[3] = 255u;
            }
            else if (cls == DiffPixel::MissingContent)
            {
                rgba[0] = 255u; rgba[1] = 0u; rgba[2] = 0u; rgba[3] = 255u;
            }
            else
            {
                rgba[0] = 255u; rgba[1] = 0u; rgba[2] = 255u; rgba[3] = 255u;
            }
        }
    }
}

bool pixel_diff_encode_png(const PixelDiff& diff, std::vector<std::uint8_t>& outPng)
{
    if (diff.width == 0u || diff.height == 0u || diff.image.empty())
        return false;
    // stb_image_write writes into a memory buffer via a callback.
    outPng.clear();
    struct Ctx { std::vector<std::uint8_t>* v; };
    Ctx ctx{ &outPng };
    auto write = [](void* context, void* data, int size) -> void {
        auto* c = static_cast<Ctx*>(context);
        const std::uint8_t* p = static_cast<const std::uint8_t*>(data);
        c->v->insert(c->v->end(), p, p + size);
    };
    const int ok = stbi_write_png_to_func(write, &ctx,
                                          static_cast<int>(diff.width),
                                          static_cast<int>(diff.height),
                                          4, diff.image.data(), 0);
    return ok != 0;
}

bool pixel_diff_write_png(const PixelDiff& diff, const std::string& path)
{
    std::vector<std::uint8_t> png;
    if (!pixel_diff_encode_png(diff, png))
        return false;
    FILE* f = nullptr;
#if defined(_MSC_VER)
    if (fopen_s(&f, path.c_str(), "wb") != 0)
        return false;
#else
    f = fopen(path.c_str(), "wb");
    if (!f)
        return false;
#endif
    const std::size_t written = fwrite(png.data(), 1, png.size(), f);
    fclose(f);
    return written == png.size();
}

} // namespace vaml::presentation::scan
