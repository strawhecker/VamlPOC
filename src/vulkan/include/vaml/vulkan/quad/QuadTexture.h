#pragma once
// S200 — the S200 textured-quad half: a minimal single device-local texture
// (vaml::vulkan::quad).
//
// This is the SECOND half of S200's "textured quads": a REAL, minimal
// device-local texture (image + image view + device memory + sampler) built from
// a CPU byte array, sampled by the S200 combined-image-sampler descriptor + the
// quad shader. It uses the S164-bound vkCreateImage / vkAllocateMemory /
// vkBindImageMemory / vkCreateImageView entries (the standing S164 swapchain
// surface) + the S200-bound vkCreateSampler / vkCreateBuffer / vkCmdCopyBuffer
// entries. A produce a real textured quad with NO external asset via the
// procedural factory (create_procedural_checker builds a checkerboard RGBA image
// in CPU memory, so a textured quad renders REAL pixels headlessly-testable as
// byte arithmetic + live via the POCs).
//
// NO-GAP recorded (not silent): S200 delivers ONLY this one minimal textured path
// (a single device-local texture, CPU bytes → map/copy → device-local image) to
// make "textured quads" true. The general STAGING BUFFER → DEVICE-LOCAL upload +
// the texture ATLAS + sub-region invalidation is S202 (the recorded NO-GAP
// hand-off, NOT a backfill). S205 owns the shared descriptor layout + real UI
// shaders; the S200-local descriptor seam is kept+extended there. This owner is
// REAL + permanent (a single textured quad's texture source), NOT MINIMAL REAL /
// NOT a STUB.
//
// Layout contract: `bytesRGBA` is tightly-packed RGBA8 (w*h*4 bytes). The image
// is created VK_IMAGE_USAGE_TRANSFER_DST_BIT | SAMPLED_BIT in the SRGB format
// (R8G8B8A8_SRGB — matching the S200 SRGB pipeline: the CPU bytes are treated as
// linear and the SRGB image sRGB-encodes on sample, so authored colors render
// with correct gamma via the S200 quad pipeline). A CPU staging buffer is
// copied into the image with a pipeline-barrier TRANSFER_DST → SHADER_READ_ONLY
// transition. The image view is a 2D one-mip color view.

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

// The S200 procedural checkerboard texture: returns a tightly-packed RGBA8
// buffer `width`×`height` (4*width*height bytes) of a checker pattern — each
// cell is `cell` px, alternating opaque black/white with a 2-cell border color
// accent so the texture is visibly sampled (not a solid screen-fill). Pure
// byte arithmetic (headless-testable): the returned vector's size ==
// format_bytes(width,height) and the two corner cells differ. `cell` must be >= 1.
VAML_VULKAN_EXPORT std::vector<std::uint8_t>
create_procedural_checker(std::uint32_t width, std::uint32_t height, std::uint32_t cell = 8) noexcept;

// The bytes-per-pixel + row-byte contract of the S200 texture path:
// format_bytes(width, height) == width*height*4 (RGBA8) for the procedural
// checker layout. Pure + headless-testable.
VAML_VULKAN_EXPORT std::uint32_t texture_format_bytes(std::uint32_t width, std::uint32_t height) noexcept;

// The real S200 texture owner: RAII + move-only, owning the device-local image,
// its image view, its device memory, the staging buffer (+ its memory), and the
// sampler. Constructed only via create(); destroys sampler -> staging -> image
// view -> image memory -> image in reverse order on destruction. See the header
// comment for LIFETIMES + NO-GAP.
class VAML_VULKAN_EXPORT QuadTexture
{
public:
    // Creates the device-local texture on `device` via `loader` from `bytesRGBA`
    // (tightly-packed RGBA8, exactly `width`*`height`*4 bytes). Runs the S200
    // sampler/descriptor seams, creates the device-local image + view + memory
    // (the S164 entries) + a CPU staging buffer, copies staging → device-local
    // via vkCmdCopyBufferToImage with a TRANSFER_DST → SHADER_READ_ONLY barrier
    // (interior layout intact for the S201 descriptor bind). Returns nullptr (no
    // throw) on failure. `loader` and `device` must outlive the returned object;
    // the graphics queue must be idle before destruction (the copy is submitted
    // on it and waited).
    [[nodiscard]] static std::unique_ptr<QuadTexture>
    create(const VulkanLoader& loader, const Device& device,
           std::uint32_t width, std::uint32_t height,
           const std::vector<std::uint8_t>& bytesRGBA);

    // Convenience: creates the texture from the procedural checker at (width, height).
    [[nodiscard]] static std::unique_ptr<QuadTexture>
    create_procedural(const VulkanLoader& loader, const Device& device,
                      std::uint32_t width, std::uint32_t height, std::uint32_t cell = 8);

    ~QuadTexture();
    QuadTexture(QuadTexture&&) noexcept;
    QuadTexture& operator=(QuadTexture&&) noexcept;
    QuadTexture(const QuadTexture&)            = delete;
    QuadTexture& operator=(const QuadTexture&) = delete;

    // The image view the S201 descriptor samples (set-0-binding-0).
    [[nodiscard]] VkImageView image_view() const noexcept { return imageView_; }
    [[nodiscard]] VkSampler   sampler()     const noexcept { return sampler_; }
    [[nodiscard]] VkImage     image()       const noexcept { return image_; }
    [[nodiscard]] std::uint32_t width()     const noexcept { return width_; }
    [[nodiscard]] std::uint32_t height()    const noexcept { return height_; }

private:
    QuadTexture(const VulkanLoader& loader, const Device& device);

    const VulkanLoader* loader_ = nullptr;
    VkDevice            device_ = VK_NULL_HANDLE;  // BORROWED from the Device
    VkImage             image_          = VK_NULL_HANDLE;
    VkDeviceMemory      imageMemory_    = VK_NULL_HANDLE;
    VkImageView         imageView_      = VK_NULL_HANDLE;
    VkBuffer            stagingBuffer_  = VK_NULL_HANDLE;
    VkDeviceMemory      stagingMemory_  = VK_NULL_HANDLE;
    VkSampler           sampler_        = VK_NULL_HANDLE;
    std::uint32_t       width_  = 0;
    std::uint32_t       height_ = 0;
};

} // namespace vaml::vulkan::quad
