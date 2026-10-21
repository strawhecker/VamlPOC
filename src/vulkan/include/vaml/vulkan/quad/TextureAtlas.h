#pragma once
// S202 — the texture atlas + general staging → device-local sub-region upload +
// sub-region invalidation (vaml::vulkan::quad).
//
// S202 RESOLVES the S161/S200-recorded NO-GAP hand-offs into one real, permanent
// owner: (1) the general STAGING → DEVICE-LOCAL upload (S200 delivered only a
// single one-shot copy inside QuadTexture::create; S202 generalizes that into a
// persistent staging buffer + repeated sub-region flushes) and (2) the texture
// ATLAS with sub-region invalidation (a uniform grid of tiles in one device-local
// image; uploading one tile dirties only its flag, and flush() copies only the
// dirty sub-regions).
//
// DECISION (confirmed at plan time, recorded NOT silent): the atlas uses RAW S164
// device memory (matching QuadTexture/QuadScene — the S200 upload path), NOT VMA.
// The S161 note ("S202 consumes the allocator (VMA)") is DIVERGED BY DECLARED
// DECISION: one image + one staging buffer = no sub-allocation pressure yet, so
// raw S164 memory keeps the quad module consistent. VMA becomes the right choice
// when many images exist (at S202_1 / S203 / S204+ / S205) — recorded here, not
// backfilled.
//
// The tiles are an EXACT uniform grid (the atlas contract below): atlasW%cols==0
// && atlasH%rows==0 is REQUIRED for a valid layout, so no tile ever straddles a
// partial tile edge and the UV mapping is exact. Each tile is R8G8B8A8 (4 bytes/
// px, tightly packed per row); the CPU bookkeeper holds the full-atlas byte
// buffer + a per-tile dirty flag set. flush() maps the host-visible staging
// buffer, copies ONLY the dirty tiles' bytes into staging at their offsets,
// records ONE command buffer (a whole-image barrier currentLayout → TRANSFER_DST,
// one vkCmdCopyBufferToImage per dirty sub-region, a TRANSFER_DST → SHADER_READ_ONLY
// barrier), submits + waits idle, and clears the dirty set. The image is
// VK_IMAGE_USAGE_TRANSFER_DST_BIT | SAMPLED_BIT in R8G8B8A8_SRGB (matching the
// S200 SRGB pipeline — authored linear bytes are sRGB-encoded on sample, correct
// gamma).
//
// NO-GAP (not silent): the atlas is a real, permanent, kept-and-extended owner for
// S202_1 (icon assets), S203 (glyph atlas), S204 (SDF rasterization) and the
// S205 shared descriptor layout. It is NOT MINIMAL REAL / NOT a stub.
//
// LIFETIMES: `loader` and `device` must OUTLIVE the returned TextureAtlas; the
// graphics queue must be idle before destruction (flush() submits + waits, so the
// atlas holds no in-flight work).

#include <vaml/vulkan/Vulkan.h>
#include <vaml/vulkan/Device.h>
#include <vaml/vulkan/VulkanLoader.h>

#include <cstdint>
#include <memory>
#include <vector>

#ifndef VAML_VULKAN_EXPORT
#  define VAML_VULKAN_EXPORT
#endif

namespace vaml::vulkan::quad {

// ---------------------------------------------------------------------------
// PURE DECISION SEAMS (headless-testable; no Vulkan runtime required)
// ---------------------------------------------------------------------------

// The uniform atlas grid: an atlasW×atlasH image divided into cols×rows tiles.
// A layout is VALID only when atlasW%cols==0 && atlasH%rows==0 (the exact-grid
// contract — no tile straddles a partial edge). An all-zero (default) layout is
// the canonical INVALID layout (all coordinates/bytes are 0).
struct AtlasLayout
{
    std::uint32_t atlasW = 0;
    std::uint32_t atlasH = 0;
    std::uint32_t cols   = 0;
    std::uint32_t rows   = 0;
    std::uint32_t tileW  = 0;
    std::uint32_t tileH  = 0;
};

// A sub-region of the atlas in pixels (a tile's rect, or a dirty sub-rect).
struct AtlasRegion
{
    std::uint32_t x = 0;
    std::uint32_t y = 0;
    std::uint32_t w = 0;
    std::uint32_t h = 0;
};

// Builds the uniform grid layout for an atlasW×atlasH image divided into cols×rows
// equal tiles. Returns an INVALID (all-zero) layout unless every dimension is
// non-zero AND atlasW%cols==0 && atlasH%rows==0 (the exact-grid contract). Pure +
// headless-testable.
VAML_VULKAN_EXPORT AtlasLayout atlas_layout(std::uint32_t atlasW, std::uint32_t atlasH,
                                            std::uint32_t cols, std::uint32_t rows) noexcept;

// True iff `layout` is a valid (non-zero, exactly-aligned) atlas grid.
VAML_VULKAN_EXPORT bool atlas_layout_valid(const AtlasLayout& layout) noexcept;

// The number of tiles in a valid layout (cols*rows); 0 when invalid.
VAML_VULKAN_EXPORT std::uint32_t atlas_tile_count(const AtlasLayout& layout) noexcept;

// The pixel rect of tile `i` (row-major: tile 0 = the top-left tile, advance
// left-to-right then top-to-bottom). Requires a valid layout + i < tile_count;
// otherwise returns an all-zero region.
VAML_VULKAN_EXPORT AtlasRegion atlas_tile_region(const AtlasLayout& layout, std::uint32_t i) noexcept;

// The CPU byte offset of tile `i`'s first pixel in the full-atlas RGBA8 buffer
// ((y*atlasW + x)*4). Requires a valid layout + i < tile_count; else 0.
VAML_VULKAN_EXPORT std::uint32_t atlas_tile_offset(const AtlasLayout& layout, std::uint32_t i) noexcept;

// The number of bytes in one tile (tileW*tileH*4); 0 when invalid.
VAML_VULKAN_EXPORT std::uint32_t atlas_tile_bytes(const AtlasLayout& layout) noexcept;

// The number of bytes in the full atlas image (atlasW*atlasH*4); 0 when invalid.
VAML_VULKAN_EXPORT std::uint32_t atlas_image_bytes(const AtlasLayout& layout) noexcept;

// The SUB-REGION INVALIDATION derivation: maps a set of dirty tile indices to the
// list of their pixel sub-rects, in ASCENDING tile-index order, deduplicated.
// The indices are a set (an index appearing twice yields ONE region even if the
// layout would place it once — dedupe by index). Out-of-range indices are
// ignored. Pure + headless-testable.
VAML_VULKAN_EXPORT std::vector<AtlasRegion>
atlas_dirty_regions(const AtlasLayout& layout, const std::vector<std::uint32_t>& dirtyIndices) noexcept;

// ---------------------------------------------------------------------------
// Real texture-atlas owner (RAII; move-only)
// ---------------------------------------------------------------------------

// The real S202 texture-atlas owner: RAII + move-only, owning the device-local
// atlas image, its image view + device memory, the host-visible full-atlas
// staging buffer + its memory, the sampler, a persistent command pool + one
// reset-table command buffer, and the CPU bookkeeper (the full-atlas byte buffer +
// the per-tile dirty flag set). Constructed only via create(). See the header
// comment for the DECISION, the exact-grid contract, and LIFETIMES.
class VAML_VULKAN_EXPORT TextureAtlas
{
public:
    // Creates the device-local atlas on `device` via `loader` as a uniform
    // cols×rows grid over an atlasW×atlasH R8G8B8A8_SRGB image. Returns nullptr
    // (no throw) when the layout is invalid or any platform step fails. The
    // image starts UNDEFINED (no data); tiles are filled via upload_tile +
    // flush(). `loader` and `device` must outlive the returned object; the
    // graphics queue must be idle before destruction.
    [[nodiscard]] static std::unique_ptr<TextureAtlas>
    create(const VulkanLoader& loader, const Device& device,
           std::uint32_t atlasW, std::uint32_t atlasH,
           std::uint32_t cols, std::uint32_t rows);

    ~TextureAtlas();
    TextureAtlas(TextureAtlas&&) noexcept;
    TextureAtlas& operator=(TextureAtlas&&) noexcept;
    TextureAtlas(const TextureAtlas&)            = delete;
    TextureAtlas& operator=(const TextureAtlas&) = delete;

    // Writes tile `i`'s RGBA8 bytes into the CPU bookkeeper at its tile offset and
    // marks that tile DIRTY (SUB-REGION INVALIDATION: only tile `i`'s flag flips —
    // no other tile, no whole-image flag). Returns true on success. Returns false
    // (no state change) when `i` is out of range or `bytes` is not exactly
    // atlas_tile_bytes() wide. The bytes are tightly-packed RGBA8 (tileW*tileH*4).
    [[nodiscard]] bool upload_tile(std::uint32_t tileIndex,
                                   const std::vector<std::uint8_t>& bytesRGBA) noexcept;

    // Number of tiles currently marked dirty (0 = nothing to flush).
    [[nodiscard]] std::uint32_t dirty_count() const noexcept { return dirtyCount_; }
    [[nodiscard]] bool is_dirty(std::uint32_t tileIndex) const noexcept;

    // Flushes the dirty tiles to the device-local image: maps the staging buffer,
    // copies ONLY the dirty tiles' bytes into staging at their offsets, records ONE
    // command buffer (whole-image barrier currentLayout → TRANSFER_DST, one
    // vkCmdCopyBufferToImage per dirty sub-region with its bufferOffset +
    // imageExtent, barrier TRANSFER_DST → SHADER_READ_ONLY), submits + waits idle,
    // and clears the dirty set. Returns the number of tiles uploaded (0 when
    // nothing is dirty, or on failure — on failure the dirty set is NOT cleared,
    // so a later flush retries; the image track a currentLayout_). The atlas image
    // is shader-ready after a successful flush (SHADER_READ_ONLY_OPTIMAL).
    std::uint32_t flush();

    // The image view / sampler / image a shader samples (the atlas is one
    // combined image sampler; a quad's UVs select its tile).
    [[nodiscard]] VkImageView image_view() const noexcept { return imageView_; }
    [[nodiscard]] VkSampler   sampler()     const noexcept { return sampler_; }
    [[nodiscard]] VkImage     image()       const noexcept { return image_; }
    [[nodiscard]] const AtlasLayout& layout() const noexcept { return layout_; }

private:
    TextureAtlas(const VulkanLoader& loader, const Device& device);

    const VulkanLoader* loader_ = nullptr;
    VkDevice            device_ = VK_NULL_HANDLE;  // BORROWED from the Device
    VkQueue             graphicsQueue_ = VK_NULL_HANDLE;  // BORROWED from the Device
    AtlasLayout         layout_{};

    VkImage             image_          = VK_NULL_HANDLE;
    VkDeviceMemory      imageMemory_    = VK_NULL_HANDLE;
    VkImageView         imageView_      = VK_NULL_HANDLE;
    VkBuffer            stagingBuffer_  = VK_NULL_HANDLE;
    VkDeviceMemory      stagingMemory_  = VK_NULL_HANDLE;
    VkSampler           sampler_        = VK_NULL_HANDLE;
    VkCommandPool       commandPool_    = VK_NULL_HANDLE;
    VkCommandBuffer     commandBuffer_  = VK_NULL_HANDLE;
    VkImageLayout       currentLayout_  = VK_IMAGE_LAYOUT_UNDEFINED;

    std::vector<std::uint8_t> cpuBuffer_;   // full-atlas RGBA8 bookkeeper
    std::vector<std::uint8_t> dirtyFlags_;  // per-tile dirty flag set (0/1)
    std::uint32_t             dirtyCount_  = 0;
};

} // namespace vaml::vulkan::quad
