#pragma once
// S640a (dep:S640) — vaml::presentation::RenderTarget: WRITE THE RENDER TO AN AREA
// OF MEMORY (vaml::presentation::RenderTarget; the root of S641's compliance scanner).
//
// The driver mandate (verbatim): "write the result of rendering to an area of
// memory". S640a renders the RETAINED scene (the S212 `encode_visual_slice`
// producer path) to an OFFSCREEN image via the SAME UiScene/Vulkan pipeline as the
// window (byte-faithful, ZERO drift — a separate pipeline is REJECTED), then
// `copy_image_to_buffer` -> a CPU `ImageBuffer { width, height, std::vector<uint8_t>
// BGRA }`. The ImageBuffer is the ACTUAL GPU-emitted output (NOT a CPU re-raster,
// NOT a doctest re-statement) — the authoritative scrape source S641 consumes.
//
// "SAME PIPELINE, ZERO DRIFT" = REUSE of the same VkPipeline HANDLES. The window's
// retained SceneRenderer is built over a vaml::vulkan::ui::UiScene::Spec whose
// handles (the three kind pipelines sdf/solid/border + the SHARED pipeline layout +
// set layout + the atlas view/sampler) are render-pass-COMPATIBLE with any
// color-only render pass using the same SRGB color format + geometry. RenderTarget
// therefore:
//   * BORROWS the SAME three pipeline handles + shared layout + set layout + atlas
//     from the spec (never re-authors a pipeline),
//   * creates ITS OWN offscreen render pass compatible with them (color-only,
//     B8G8R8A8_SRGB, MSAA 1) + its own framebuffer + its own RETAINED SceneRenderer
//     built over the SAME spec.
// Because the offscreen attachment uses the same SRGB format, the stored bytes the
// sRGB hardware encodes are byte-faithful to what the window would present on those
// same pipelines (the sRGB-encoded values copy_image_to_buffer reads back).
//
// RESIZE-BUFFER pattern (the driver's external oversubscribed model): RenderTarget
// owns ONE oversized offscreen VkImage + framebuffer, so a logical resize WITHIN
// capacity reuses it (recompute extent + copy region — NO image/framebuffer/
// swapchain churn). Only an exceed-capacity resize GROWS the image once. The CPU
// side honors the external app's oversized `ImageBuffer` the same way
// (`image_buffer_ensure_capacity` reuses the std::vector, only re-allocating when it
// MUST grow — never shrinks). The policy is exposed as PURE seams (resize_buffer_
// decision / offscreen_extent_at / image_buffer_ensure_capacity) so it is
// headless-ctestable on both CI lanes with NO Vulkan runtime.
//
// ZERO NEW LOCKS (ADR-018): single-threaded record + copy on the render thread; the
// retained SceneRenderer's S209 fence is the sync primitive.
//
// SPLIT (the standing S162/S200/S206/S212 precedent):
//   * PURE DECISION SEAMS (headless-testable, NO Vulkan runtime) —
//       Impl::ImageBuffer { width, height, std::vector<uint8_t> bgra }  (the CPU result)
//       offscreen_format_for(windowFormat)     (the offscreen target reuses the SRGB format)
//       offscreen_extent_at(logicalW, logicalH, capW, capH)  (the clamped record viewport)
//       resize_buffer_decision(capW, capH, newW, newH)  (reuse-within-cap vs grow)
//       image_buffer_ensure_capacity(ImageBuffer&, w, h) (the external-app RESIZE-BUFFER)
//       offscreen_copy_region(w, h, rowPitch)  (the copy region honoring rowPitch)
//       render_target_loop_steps()             (the fixed per-render sequence)
//     These PROVE the offscreen render + resize-buffer contract in ctest WITHOUT a
//     live Vulkan runtime (the S640a proof surface — both CI lanes).
//   * `RenderTarget::create(...)` — the REAL live owner (RAII, move-only): owns the
//     oversized offscreen image/memory/view + the compatible render pass +
//     framebuffer + command pool/buffer + host-visible staging buffer + a RETAINED
//     SceneRenderer over the SAME scene spec, and records + copies the GPU output
//     into an ImageBuffer. Device-backed ([both apps, not CI] per the S206/S210
//     precedent; the REAL GPU proof is the S641 script-driven lane).
//
// NO-GAP recorded (not silent): the GPU script render -> ImageBuffer -> scan/compare
// is **S641's** job (S641 consumes this ImageBuffer as the ACTUAL GPU-emitted
// output, per the roadmap); the WPF reference emitter is S640b; the scanner is S641.
// ZERO MINIMAL REAL introduced here — ImageBuffer + every seam + the owner are real,
// permanent, kept-and-extended ADR-021 surface.

#include <vaml/vulkan/Vulkan.h>
#include <vaml/vulkan/Device.h>
#include <vaml/vulkan/VulkanLoader.h>
#include <vaml/vulkan/ui/UiScene.h>
#include <vaml/vulkan/ui/SceneRenderer.h>
#include <vaml/vulkan/ui/UiSceneEncoder.h>   // ElementVisual, encode_visual_tree
#include <vaml/vulkan/ui/MarshalledScene.h>  // MarshalledScene (the S211 postbox)

#include <vaml/core/UIElement.h>             // vaml::UIElement (the core visual tree)

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

#ifndef VAML_PRESENTATION_EXPORT
#  define VAML_PRESENTATION_EXPORT
#endif

namespace vaml::presentation {

// ---------------------------------------------------------------------------
// PURE DECISION SEAMS (headless-testable; no Vulkan runtime required)
// ---------------------------------------------------------------------------

// Table-of-contents namespace for S640a internals (kept public + linkable so the
// headless test proves the offscreen render + resize-buffer decision layer on both
// CI lanes with NO Vulkan runtime — the S162/S200 precedent).
namespace Impl {

// The CPU result of `RenderTarget::render_to`: the GPU-emitted BGRA pixel array at
// `width` x `height`. The SAME CPU surface S641 consumes as the authoritative scrape
// source. `bgra` has exactly width*height*4 bytes after a successful render_to.
// REAL permanent public type (ADR-021) — NOT MINIMAL REAL.
struct ImageBuffer
{
    std::uint32_t width  = 0;
    std::uint32_t height = 0;
    std::vector<std::uint8_t> bgra;   // BGRA, bottom-up? NO — row 0 = the image's top row
};

// The bytes a fully-filled ImageBuffer needs: width*height*4 (BGRA). Pure.
VAML_PRESENTATION_EXPORT std::size_t image_buffer_bytes(std::uint32_t width, std::uint32_t height) noexcept;

// The OFFSCREEN target format: the SAME SRGB format as the window's swapchain (so
// the offscreen image stores the same sRGB-encoded bytes the window presents — the
// byte-faithful zero-drift pairing). `windowFormat` is the S164 swapchain color
// format. Returns `windowFormat` verbatim when it is an SRGB format
// (vaml::vulkan::quad::is_srgb_format); returns VK_FORMAT_UNDEFINED otherwise (the
// S640a Guard: RenderTarget::create REJECTS a non-SRGB color format). Pure + headless.
VAML_PRESENTATION_EXPORT VkFormat offscreen_format_for(VkFormat windowFormat) noexcept;

// The record VIEWPORT/EXTENT: the LOGICAL render size (logicalW x logicalH) CLAMPED
// to the oversized offscreen buffer's capacity (capW x capH) — the render never
// exceeds the buffer (the resize-buffer contract's within-capacity reuse). Non-positive
// logical/negative capacity values are GUARDED to 1x1 (a zero/negative offscreen has
// no renderable area). Pure + headless.
VAML_PRESENTATION_EXPORT VkExtent2D
offscreen_extent_at(std::uint32_t logicalW, std::uint32_t logicalH,
                    std::uint32_t capW, std::uint32_t capH) noexcept;

// The RESIZE-BUFFER DECISION: true iff the oversized offscreen buffer must GROW to
// fit the new logical size (newW > capW OR newH > capH). FALSE = the new size fits
// WITHIN capacity -> the buffer is REUSED (zero image/framebuffer/swapchain churn —
// the driver's "avoid swapchain churn" mandate). A non-positive new size never forces
// a grow (returns false). Pure + headless.
VAML_PRESENTATION_EXPORT bool
resize_buffer_decision(std::uint32_t capW, std::uint32_t capH,
                       std::uint32_t newW, std::uint32_t newH) noexcept;

// The external-app RESIZE-BUFFER contract on the CPU side: ensures `out` has room for
// w x h BGRA pixels. REUSES the oversized `bgra` std::vector when it already holds
// enough bytes (no re-allocation, never shrinks — the "reuse an oversized buffer"
// mandate); re-allocates ONLY when it MUST grow to w*h*4. Sets out.width/out.height
// and returns `out`. Pure + headless.
VAML_PRESENTATION_EXPORT Impl::ImageBuffer&
image_buffer_ensure_capacity(Impl::ImageBuffer& out, std::uint32_t w, std::uint32_t h) noexcept;

// The COPY REGION honoring the image's per-row stride: `rowPitch` (bytes per row,
// from vkGetImageSubresourceLayout) is PAID FORWARD as the VkBufferImageCopy
// `bufferRowLength` so the copy maps each offscreen row onto the staging buffer
// WITHOUT assuming a tight 4*w packing (the sRGB image row stride can exceed
// width*4 for alignment). `w`/`h` are the copy extent (the clamped record size);
// `bufferOffset` is the staging-buffer byte offset the rows land at. Pure + headless.
VAML_PRESENTATION_EXPORT VkBufferImageCopy
offscreen_copy_region(std::uint32_t w, std::uint32_t h,
                      std::uint32_t rowPitch, VkDeviceSize bufferOffset = 0u) noexcept;

// The S640a fixed per-render LOOP STEPS (the S210/S212 loop-plan precedent): the
// ordered sequence a `render_to` runs ONCE per offscreen snapshot.
enum class RenderTargetLoopStep
{
    EncodeIfDirty,   // set_scene the retained SceneRenderer only when dirty
    RecordRetained,  // record the retained buffers onto the offscreen command buffer
    CopyToBuffer,    // vkCmdCopyImageToBuffer -> flush host-visible -> ImageBuffer
};

// The fixed ordered per-render sequence (see RenderTargetLoopStep). Every step runs
// per render_to; the [EncodeIfDirty] branch is conditional (only when the scene
// changed). Pure + headless.
VAML_PRESENTATION_EXPORT const std::vector<Impl::RenderTargetLoopStep>& render_target_loop_steps() noexcept;

} // namespace Impl

// ---------------------------------------------------------------------------
// The offscreen render host (RAII; move-only)
// ---------------------------------------------------------------------------

// Options for the LIVE create() path (the device-gated offscreen integration). The
// borrowed loader/device must outlive the returned host. `sceneSpec` supplies the
// SAME pipelines/layout/set-layout/atlas the window uses (the zero-drift pairing);
// its handles must outlive the host.
struct RenderTargetOptions
{
    const vaml::vulkan::VulkanLoader* loader = nullptr;  // borrowed; must outlive
    const vaml::vulkan::Device*       device = nullptr;  // borrowed; must outlive
    // The SAME scene spec the window's retained SceneRenderer uses (the S206/S210
    // Spec: the three kind pipelines + shared layout/set layout + atlas). RenderTarget
    // BORROWS these handles; it never re-authors a pipeline (zero drift).
    vaml::vulkan::ui::UiScene::Spec sceneSpec{};
    // The LOGICAL frame size the producer arranges + records at. May be UPDATED via
    // resize(); a resize WITHIN bufferCap capacity REUSES the single offscreen
    // image (no churn) — only an exceed-capacity resize grows it once.
    std::uint32_t logicalW = 0;
    std::uint32_t logicalH = 0;
    // The OVERSIZED offscreen buffer capacity (the driver's external oversubscribed
    // RESIZE-BUFFER model): the single VkImage is sized to (bufferCapW x bufferCapH)
    // so any logical size within it reuses the buffer. Defaults to the initial
    // logical size (an exact-fit single buffer).
    std::uint32_t bufferCapW = 0;
    std::uint32_t bufferCapH = 0;
    // The driver's "external-app callback, not a repo file": fired when a resize
    // would EXCEED capacity and the single offscreen image must GROW once. The
    // external app uses it to re-size its (oversized) CPU ImageBuffer if desired.
    // No-op (empty) by default. NOT a repo file — a callable the app supplies.
    std::function<void(std::uint32_t newLogicalW, std::uint32_t newLogicalH)> on_resize{};
};

// The S640a offscreen render host: renders the RETAINED scene (the producer path
// mirrors VamlRenderer: set_root/set_frame_size/set_visual_of/invalidate) to an
// OFFSCREEN image via the SAME scene pipeline, then copy_image_to_buffer -> a CPU
// ImageBuffer. The app uses the UI-thread surface (set_root/set_frame_size/
// set_visual_of/invalidate) + the render surface (render_to), mirroring the
// VamlRenderer split.
class VAML_PRESENTATION_EXPORT RenderTarget
{
public:
    // The LIVE offscreen owner: builds + owns the oversized offscreen
    // image/memory/view + the compatible color-only render pass + framebuffer + the
    // command pool/buffer + the host-visible staging buffer + a RETAINED SceneRenderer
    // over the SAME scene spec. Returns nullptr (no throw) if the SRGB guard fails or
    // any owner cannot be built. The borrowed loader/device + the sceneSpec handles
    // must outlive.
    [[nodiscard]] static std::unique_ptr<RenderTarget>
    create(const RenderTargetOptions& opts);

    ~RenderTarget();
    RenderTarget(RenderTarget&&) noexcept;
    RenderTarget& operator=(RenderTarget&&) noexcept;
    RenderTarget(const RenderTarget&)            = delete;
    RenderTarget& operator=(const RenderTarget&) = delete;

    // ----------------------- UI-thread surface (S212 producer path) ---------
    // Attaches the core visual tree root (BORROWED — the app owns it; must outlive
    // this host while in use). Mirrors VamlRenderer::set_root. Returns false if null.
    [[nodiscard]] bool set_root(vaml::UIElement* root) noexcept;
    // The arrange/frame size (logical px) — mirrors VamlRenderer::set_frame_size.
    void set_frame_size(std::uint32_t width, std::uint32_t height) noexcept;
    // The visualOf material seam (default_visual_of() unless overridden) — mirrors
    // VamlRenderer::set_visual_of.
    void set_visual_of(std::function<vaml::vulkan::ui::ElementVisual(const vaml::UIElement&)> visualOf);
    // The S212 producer step: Measure + Arrange the attached tree at the logical
    // frame size, encode via visualOf into the S206 draw list, then set_scene into
    // the RETAINED SceneRenderer (the EncodeIfDirty pulse for the next render_to).
    // Returns false when nothing was encoded (no root/frame size).
    bool invalidate();

    // ------------------------- render surface --------------------------------
    // The HEADLINE: render_to records the RETAINED scene against the offscreen
    // target, copy_image_to_buffer -> the CPU `out` ImageBuffer (the ACTUAL
    // GPU-emitted BGRA bytes). Honors image_buffer_ensure_capacity (the external-app
    // RESIZE-BUFFER: reuse an oversized out, only grow when it must). Returns false
    // on failure (no device-live owner / no root / a record/copy failure). On
    // success `out` holds width*height*4 bytes; on failure `out` is untouched/a
    // zero-size buffer.
    bool render_to(Impl::ImageBuffer& out);
    // Convenience: allocates + returns a fresh ImageBuffer on success (nullopt on
    // failure).
    [[nodiscard]] std::optional<Impl::ImageBuffer> render();

    // A resize REQUEST (logical size). Applies the RESIZE-BUFFER policy: within
    // capacity -> reuse the single offscreen image (recompute extent + copy region,
    // NO churn); exceed capacity -> grow the image ONCE + fire the on_resize callback.
    void resize(std::uint32_t width, std::uint32_t height);

    // State accessors (headless-observable).
    [[nodiscard]] bool has_root() const noexcept { return root_ != nullptr; }
    [[nodiscard]] std::uint32_t logical_width()  const noexcept { return logicalW_; }
    [[nodiscard]] std::uint32_t logical_height() const noexcept { return logicalH_; }
    [[nodiscard]] std::uint32_t buffer_cap_width()  const noexcept { return bufferCapW_; }
    [[nodiscard]] std::uint32_t buffer_cap_height() const noexcept { return bufferCapH_; }
    [[nodiscard]] float last_framebuffer_scale() const noexcept { return lastScale_; }
    [[nodiscard]] bool dirty() const noexcept { return dirty_; }

private:
    RenderTarget();

    // The CPU/merge state (the S212 producer surface).
    vaml::UIElement* root_ = nullptr;  // BORROWED; app-owned
    std::uint32_t logicalW_ = 0;
    std::uint32_t logicalH_ = 0;
    std::uint32_t bufferCapW_ = 0;
    std::uint32_t bufferCapH_ = 0;
    std::function<vaml::vulkan::ui::ElementVisual(const vaml::UIElement&)> visualOf_;
    bool dirty_ = false;
    float lastScale_ = 1.0f;
    // The on-resize callback slot (the driver's external-app callback).
    std::function<void(std::uint32_t, std::uint32_t)> onResize_;

    // The live composition state (null until create() succeeds).
    std::unique_ptr<vaml::vulkan::ui::SceneRenderer> sceneRenderer_;
    VkRenderPass    renderPass_   = VK_NULL_HANDLE;  // offscreen (owned)
    struct DeviceOwner;                              // the offscreen RAII resources
    std::unique_ptr<DeviceOwner> dev_;               // offscreen image/view/memory/framebuffer/command/staging
};

} // namespace vaml::presentation
