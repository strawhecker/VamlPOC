// S640a — RenderTarget pure decision seams (headless Rail 1).
//
// Sprint 11r (S640a): the offscreen render host's CPU decision layer — proven
// headlessly WITHOUT a live Vulkan runtime (the standing S162/S200/S212 precedent),
// on BOTH CI lanes (windows-msvc + ubuntu-clang). It proves the RESIZE-BUFFER +
// zero-drift + copy-region + loop-step contract that the S641 compliance scanner
// builds on. The REAL GPU render_to (record -> copy_image_to_buffer -> ImageBuffer)
// is the [both apps, not CI] script-driven S641 lane — recorded NO-GAP, never a
// ctest re-statement.
//
// This TU asserts:
//   * ImageBuffer shape + image_buffer_bytes (BGRA 4-byte math + overflow guard)
//   * offscreen_format_for — the SRGB identity (zero drift) + non-SRGB undefined
//   * offscreen_extent_at — the clamp/cap/guard (never exceeds the oversized buffer)
//   * resize_buffer_decision — reuse-within-capacity vs grow-on-exceed
//   * image_buffer_ensure_capacity — REUSE the oversized vector, never shrink, grow-only
//   * offscreen_copy_region — the rowPitch-faithful VkBufferImageCopy
//   * render_target_loop_steps — the fixed per-render sequence order

#include <vaml/presentation/RenderTarget.h>

#include "doctest/doctest.h"

#include <cstdint>

using namespace vaml::presentation;
using vaml::presentation::Impl::ImageBuffer;
using vaml::presentation::Impl::RenderTargetLoopStep;

// ---------------------------------------------------------------------------
// ImageBuffer + image_buffer_bytes (the BGRA CPU surface S641 consumes)
// ---------------------------------------------------------------------------
TEST_CASE("S640a image_buffer_bytes is the 4-byte BGRA size, zero-guarded")
{
    CHECK(Impl::image_buffer_bytes(0u, 10u) == 0u);
    CHECK(Impl::image_buffer_bytes(10u, 0u) == 0u);
    CHECK(Impl::image_buffer_bytes(1u, 1u) == 4u);
    CHECK(Impl::image_buffer_bytes(2u, 3u) == 24u);
    CHECK(Impl::image_buffer_bytes(100u, 200u) == 100u * 200u * 4u);
}

TEST_CASE("S640a ImageBuffer default-constructs empty (the S641 consumer contract)")
{
    ImageBuffer buf;
    CHECK(buf.width == 0u);
    CHECK(buf.height == 0u);
    CHECK(buf.bgra.empty());
}

// ---------------------------------------------------------------------------
// offscreen_format_for — the zero-drift SRGB pairing
// ---------------------------------------------------------------------------
TEST_CASE("S640a offscreen_format_for keeps the SRGB window format (zero drift)")
{
    // The window swapchain uses an SRGB color format; the offscreen target REUSES it
    // verbatim so the sRGB-encoded bytes read back equal what the window presents.
    CHECK(Impl::offscreen_format_for(VK_FORMAT_B8G8R8A8_SRGB) == VK_FORMAT_B8G8R8A8_SRGB);
    CHECK(Impl::offscreen_format_for(VK_FORMAT_R8G8B8A8_SRGB) == VK_FORMAT_R8G8B8A8_SRGB);
}

TEST_CASE("S640a offscreen_format_for rejects a non-SRGB format (the create guard)")
{
    // A non-SRGB color format cannot be a byte-faithful offscreen twin -> UNDEFINED
    // (RenderTarget::create rejects it).
    CHECK(Impl::offscreen_format_for(VK_FORMAT_B8G8R8A8_UNORM) == VK_FORMAT_UNDEFINED);
    CHECK(Impl::offscreen_format_for(VK_FORMAT_UNDEFINED)       == VK_FORMAT_UNDEFINED);
}

// ---------------------------------------------------------------------------
// offscreen_extent_at — the clamp/cap/guard (never exceeds the oversized buffer)
// ---------------------------------------------------------------------------
TEST_CASE("S640a offscreen_extent_at clamps the logical size to the buffer capacity")
{
    // Within capacity -> the logical size is used verbatim.
    auto e = Impl::offscreen_extent_at(100u, 80u, 256u, 256u);
    CHECK(e.width == 100u);
    CHECK(e.height == 80u);

    // Exceeds capacity on one axis -> clamped to the capacity (never exceeds the buffer).
    e = Impl::offscreen_extent_at(300u, 50u, 256u, 256u);
    CHECK(e.width == 256u);
    CHECK(e.height == 50u);
    e = Impl::offscreen_extent_at(50u, 300u, 256u, 256u);
    CHECK(e.width == 50u);
    CHECK(e.height == 256u);

    // Exceeds on both axes -> fully clamped to the capacity.
    e = Impl::offscreen_extent_at(800u, 600u, 256u, 256u);
    CHECK(e.width == 256u);
    CHECK(e.height == 256u);
}

TEST_CASE("S640a offscreen_extent_at guards non-positive sizes to 1x1 (no renderable area)")
{
    auto e = Impl::offscreen_extent_at(0u, 0u, 256u, 256u);
    CHECK(e.width == 1u);
    CHECK(e.height == 1u);
    // A zero buffer capacity also degrades to the 1x1 floor (no dead zero-extent pass).
    e = Impl::offscreen_extent_at(0u, 0u, 0u, 0u);
    CHECK(e.width == 1u);
    CHECK(e.height == 1u);
}

// ---------------------------------------------------------------------------
// resize_buffer_decision — the reuse-within-capacity vs grow-on-exceed policy
// ---------------------------------------------------------------------------
TEST_CASE("S640a resize_buffer_decision reuses within capacity (ZERO churn)")
{
    // A logical resize that FITS the single oversized buffer -> reuse (no image/
    // framebuffer/swapchain churn — the driver's "avoid swapchain churn" mandate).
    CHECK_FALSE(Impl::resize_buffer_decision(256u, 256u, 100u, 80u));
    CHECK_FALSE(Impl::resize_buffer_decision(256u, 256u, 256u, 256u));
    // 200x300 still fits the 256x256 cap? NO — 300 exceeds the 256 CAPACITY on the
    // height axis, so it MUST grow (exceed-capacity is the grow trigger). A size can
    // only be reused WITHOUT churn when it stays WITHIN both cap dimensions.
    CHECK(Impl::resize_buffer_decision(256u, 256u, 200u, 300u));
}

TEST_CASE("S640a resize_buffer_decision GROWS only when the new size exceeds capacity")
{
    // Exceed capacity on either axis -> must grow the single buffer once.
    CHECK(Impl::resize_buffer_decision(256u, 256u, 300u, 256u));
    CHECK(Impl::resize_buffer_decision(256u, 256u, 256u, 300u));
    CHECK(Impl::resize_buffer_decision(256u, 256u, 300u, 300u));
    // A tiny-capacity buffer (0,0) cannot hold any positive size -> any non-zero new
    // size grows it (there is no within-capacity reuse for a zero-capacity buffer).
    CHECK(Impl::resize_buffer_decision(0u, 0u, 10u, 10u));

    // A non-positive new size never forces a grow (the buffer is reused/kept).
    CHECK_FALSE(Impl::resize_buffer_decision(256u, 256u, 0u, 0u));
    CHECK_FALSE(Impl::resize_buffer_decision(256u, 256u, 0u, 256u));
}

// ---------------------------------------------------------------------------
// image_buffer_ensure_capacity — the external-app RESIZE-BUFFER contract (CPU side)
// ---------------------------------------------------------------------------
TEST_CASE("S640a image_buffer_ensure_capacity reuses an oversized vector (never shrinks)")
{
    ImageBuffer buf;
    // The app pre-allocates an OVERSIZED buffer (the external oversubscribed model).
    buf.bgra.resize(1024u * 1024u * 4u);
    const std::size_t capBefore = buf.bgra.capacity();

    // A logical size WITHIN the oversized buffer -> REUSE (no re-alloc, no shrink).
    Impl::image_buffer_ensure_capacity(buf, 100u, 80u);
    CHECK(buf.width == 100u);
    CHECK(buf.height == 80u);
    CHECK(buf.bgra.capacity() == capBefore);  // the vector's capacity is untouched
    CHECK(buf.bgra.size() >= 100u * 80u * 4u);

    // Another within-capacity resize keeps the same capacity (no churn).
    Impl::image_buffer_ensure_capacity(buf, 128u, 64u);
    CHECK(buf.bgra.capacity() == capBefore);
}

TEST_CASE("S640a image_buffer_ensure_capacity grows ONLY when it must exceed the size")
{
    ImageBuffer buf;
    Impl::image_buffer_ensure_capacity(buf, 32u, 32u);
    const std::size_t sizeAfterFirst = buf.bgra.size();
    CHECK(sizeAfterFirst == 32u * 32u * 4u);

    // A LARGER size grows the buffer to exactly what's needed.
    Impl::image_buffer_ensure_capacity(buf, 64u, 64u);
    CHECK(buf.width == 64u);
    CHECK(buf.height == 64u);
    CHECK(buf.bgra.size() >= 64u * 64u * 4u);

    // A SMALLER size NEVER shrinks (the oversized-buffer reuse contract).
    Impl::image_buffer_ensure_capacity(buf, 8u, 8u);
    CHECK(buf.width == 8u);
    CHECK(buf.height == 8u);
    CHECK(buf.bgra.size() >= 64u * 64u * 4u);  // still holds the larger allocation
}

// ---------------------------------------------------------------------------
// offscreen_copy_region — the rowPitch-faithful VkBufferImageCopy
// ---------------------------------------------------------------------------
TEST_CASE("S640a offscreen_copy_region honors the image row stride (rowPitch)")
{
    // A rowPitch that EXCEEDS the tight 4*w packing (aligned stride) is paid forward
    // as bufferRowLength so the copy maps each row without a tight-repack assumption.
    const VkBufferImageCopy region = Impl::offscreen_copy_region(100u, 50u, 512u, 0u);
    CHECK(region.bufferOffset == 0u);
    CHECK(region.bufferRowLength == 512u);
    CHECK(region.bufferImageHeight == 50u);
    CHECK(region.imageSubresource.aspectMask == VK_IMAGE_ASPECT_COLOR_BIT);
    CHECK(region.imageSubresource.mipLevel == 0u);
    CHECK(region.imageSubresource.layerCount == 1u);
    CHECK(region.imageOffset.x == 0);
    CHECK(region.imageOffset.y == 0);
    CHECK(region.imageOffset.z == 0);
    CHECK(region.imageExtent.width == 100u);
    CHECK(region.imageExtent.height == 50u);
    CHECK(region.imageExtent.depth == 1u);
}

TEST_CASE("S640a offscreen_copy_region accepts a non-zero buffer offset")
{
    const VkBufferImageCopy region = Impl::offscreen_copy_region(10u, 10u, 40u, 0x1000u);
    CHECK(region.bufferOffset == 0x1000u);
}

// ---------------------------------------------------------------------------
// render_target_loop_steps — the fixed per-render sequence
// ---------------------------------------------------------------------------
TEST_CASE("S640a render_target_loop_steps is the fixed EncodeIfDirty/Rendering/CopyByBuffer order")
{
    const auto& steps = Impl::render_target_loop_steps();
    REQUIRE(steps.size() == 3u);
    CHECK(steps[0] == RenderTargetLoopStep::EncodeIfDirty);
    CHECK(steps[1] == RenderTargetLoopStep::RecordRetained);
    CHECK(steps[2] == RenderTargetLoopStep::CopyToBuffer);
}
