#pragma once
// S210 (dep:S209) — the render-thread loop end-to-end (single thread first):
// retained scene drawn each frame (vaml::vulkan::ui).
//
// S210 closes the loop: S206/UiScene is a per-frame CPU list encoder + S209
// makes it a fence-safe RETAINED (retired-generations-not-freed) buffer owner.
// SceneRenderer is the SINGLE-THREAD retained-scene owner that draws the SAME
// encoded buffers EVERY FRAME without re-encoding: it owns a UiScene (the real
// GPU batcher), encodes a logical scene ONCE into the retained buffers, and each
// frame RECORDS those buffers (the S206 record_draw) + RECYCLES the S209
// retired generations via the in-flight fence (advancing the S209 pool's frame
// counter once per frame).
//
// The "single thread first" is DELIBERATE and recorded (NO-GAP, not silent):
// everything on ONE thread — set_scene() + record() + recycle() are called from
// the render thread/loop. UI-thread mutation → render-thread marshalling is
// S211 (scheduled; SceneRenderer is the single-thread foundation it builds on,
// NOT a silent omission).
//
// The loop integration (S210-B): SceneRenderer plugs into the S173 Presenter's
// DrawBody. Each present tick the draw body calls:
//     renderer.read_frame(cmd, mvp, uiScale);   // record RETAINED buffers
//     renderer.recycle();                        // free retired gens via fence
// and only when the app's scene CHANGED does it call set_scene() (encode once)
// between frames. Idle/unchanged frames still RECORD but re-encode ZERO times —
// the "retained scene drawn each frame" headline.
//
// Split (the S162/S200/S206 precedent):
//   * PURE DECISION SEAMS (headless-testable, no Vulkan runtime) —
//       render_loop_steps()       (the fixed ordered per-frame loop sequence)
//       should_reencode(dirty)    (the retain gate: only a dirty flag re-encodes)
//     These prove the loop contract in ctest WITHOUT a live runtime.
//   * `SceneRenderer::create(...)` — the real owner: builds a UiScene (the S209
//     fence-aware batcher) + owns it, encodes the retained scene via set_items.
//     Device-backed ([s210-live] POC-proven, the S206/S209 precedent).
//
// ADR-018: zero new locks (single-threaded loop; the S209 pool's fence IS the
// sync primitive, not a lock). Marshalling to a render/UI dual-thread model is
// S211 (recorded NO-GAP).

#include <vaml/vulkan/Vulkan.h>
#include <vaml/vulkan/Device.h>
#include <vaml/vulkan/VulkanLoader.h>
#include <vaml/vulkan/ui/UiScene.h>
#include <vaml/vulkan/ui/UiPipeline.h>

#include <cstdint>
#include <memory>
#include <span>
#include <vector>

#ifndef VAML_VULKAN_EXPORT
#  define VAML_VULKAN_EXPORT
#endif

namespace vaml::vulkan::ui {

// ---------------------------------------------------------------------------
// PURE DECISION SEAMS (headless-testable; no Vulkan runtime required)
// ---------------------------------------------------------------------------

// The S210 loop-plan (the S166/S201/S206 record-plan precedent): the fixed
// ordered per-frame sequence a render-thread loop runs to draw the RETAINED
// scene. scene->record_draw already encodes the S206 record steps; this is the
// HIGHER loop plan: WaitFence (the S166 in-flight handshake) -> [if dirty:
// encode/re-encode once] -> record the RETAINED buffers -> submit/present (the
// S173 Presenter owns this) -> recycle (the S209 fence-driven free). The
// [EncodeIfDirty] step is conditional (absent when the scene is unchanged).
enum class RenderLoopStep
{
    WaitFence,        // the S166 in-flight fence wait (frame slot freed for reuse)
    EncodeIfDirty,    // only when dirty: set_scene -> encode the retained buffers ONCE
    RecordRetained,   // record the RETAINED buffers (NO re-encode) each frame
    SubmitPresent,    // the S173 Presenter owns submit + present (not this seam)
    Recycle,          // the S209 fence-driven free of retired generations
};

// The fixed ordered per-frame loop sequence (all steps except EncodeIfDirty run
// EVERY frame; EncodeIfDirty is conditional). Pure + headless-testable.
VAML_VULKAN_EXPORT const std::vector<RenderLoopStep>& render_loop_steps() noexcept;

// The RETAIN gate: true iff the renderer must re-encode this frame. A dirty flag
// (the scene CHANGED since the last encode) forces a re-encode; an unchanged
// (not-dirty) frame does NOT re-encode — it only records the retained buffers.
// Pure + headless-testable.
VAML_VULKAN_EXPORT bool should_reencode(bool dirty) noexcept;

// ---------------------------------------------------------------------------
// Real retained-scene owner (RAII; move-only; owns a UiScene)
// ---------------------------------------------------------------------------

// The real S210 retained-scene owner: RAII + move-only, Owning a UiScene (the
// S209 fence-aware GPU batcher) + its retained buffers + the per-texture binding
// slots. Constructed only via create(). The app fills the scene ONCE via
// set_scene() (encode into the retained buffers), then every frame calls
// record() (draw the RETAINED buffers — no re-encode) + recycle() (the S209
// fence-driven free). A set_scene() with the SAME content is a NO-OP (no
// re-encode, no retained-buffer churn) — the "retained scene drawn each frame".
class VAML_VULKAN_EXPORT SceneRenderer
{
public:
    // Creates the renderer: builds + owns the UiScene (the S209 fence-aware
    // batcher) from `sceneSpec` (the S206/S209 Spec: the shared pipelines, set
    // layout, atlas, in-flight ring). Returns nullptr (no throw) on failure.
    // The loader/device + every sceneSpec handle must outlive the result.
    [[nodiscard]] static std::unique_ptr<SceneRenderer>
    create(const VulkanLoader& loader, const Device& device, const UiScene::Spec& sceneSpec);

    ~SceneRenderer();
    SceneRenderer(SceneRenderer&&) noexcept;
    SceneRenderer& operator=(SceneRenderer&&) noexcept;
    SceneRenderer(const SceneRenderer&)            = delete;
    SceneRenderer& operator=(const SceneRenderer&) = delete;

    // (Re)encodes a logical scene into the RETAINED buffers (clean feed to the
    // owned UiScene::set_items: compacts + encodes + recomputes batch_plan; a
    // grow RETIRES the old generation into the S209 pool — never frees it while
    // the in-flight ring may still read it). The app calls this ONLY when the
    // content actually CHANGED (a dirty frame) — an unchanged frame does NOT call
    // it, so record() below re-encodes ZERO times between dirty frames (the
    // "retained scene drawn each frame" guarantee + the loop-plan's conditional
    // EncodeIfDirty step). Returns false on a failure (no throw). An empty/
    // all-degenerate scene clears the retained list.
    bool set_scene(std::span<const DrawItem> items);

    // S209 — registers a per-texture descriptor set (forwarded to the owned
    // UiScene). Returns false on a null renderer (no throw).
    bool set_texture_binding(std::uint32_t textureId, VkDescriptorSet set);

    // Records the RETAINED buffers onto `cmd` (via the owned UiScene: binds the
    // retained vertex/index buffers + batch descriptor sets + pushes + draws).
    // Does NOT re-encode — the "retained scene drawn each frame". Returns false
    // on an empty scene or a record failure.
    bool record(VkCommandBuffer cmd, const Mat4& mvp, float uiScale);
    // Convenience: record with the projection computed from the owned scene extent.
    bool record(VkCommandBuffer cmd);

    // S210 — the fence-driven frame advance: forwards to the owned S209 pool's
    // recycle() (advances the pool frame counter once per frame), freeing any
    // retired generation whose slot fence has SIGNALED (the ring wrapped — no
    // UAF under the S166 3-in-flight window). The caller calls this once per
    // frame after submit+present. `fence` (the just-waited in-flight fence) is
    // the S210 sync contract but may be VK_NULL_HANDLE — the pool tracks the
    // ring by frame index, which the waited fences drive.
    void recycle(VkFence /*fence*/);

    // True until the first record is called (the renderer has not yet recorded).
    [[nodiscard]] bool dirty() const noexcept { return dirty_; }
    [[nodiscard]] std::uint32_t item_count()  const noexcept { return scene_ ? scene_->item_count() : 0u; }
    [[nodiscard]] std::uint32_t batch_count() const noexcept { return scene_ ? scene_->batch_count() : 0u; }
    [[nodiscard]] const UiScene* scene() const noexcept { return scene_.get(); }

private:
    SceneRenderer();

    std::unique_ptr<UiScene> scene_;
    bool dirty_ = false;            // a set_scene happened; the next record uses it
    std::uint32_t lastEncodeCount_ = 0;  // encode counter for the no-op retained check
};

} // namespace vaml::vulkan::ui
