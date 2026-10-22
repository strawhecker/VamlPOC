// S210 — SceneRenderer implementation (vaml::vulkan::ui).
//
// Implements the S210 render-loop decision seams + the real retained-scene owner.
// The owner builds + owns a UiScene (the S209 fence-aware batcher), encodes a
// logical scene ONCE into the retained buffers (set_scene), and each frame
// RECORDS those retained buffers (record — no re-encode) + RECYCLES the retired
// generations (recycle — the S209 fence-driven free). Single-thread first; UI-
// thread marshalling is S211 (recorded NO-GAP). See SceneRenderer.h for the contract
// + the loop integration.

#include <vaml/vulkan/ui/SceneRenderer.h>
#include <vaml/vulkan/quad/QuadPipeline.h>  // quad::ui_projection

namespace vaml::vulkan::ui {

const std::vector<RenderLoopStep>& render_loop_steps() noexcept
{
    // S211 inserts ConsumeMarshalled ahead of EncodeIfDirty (in place, the loop
    // stays forward-compatible): each frame the render thread first drains the
    // UI-thread's change postbox (once per frame), which sets the dirty/re-encode
    // pulse that EncodeIfDirty consumes. Idle frames (nothing new) skip both.
    static const std::vector<RenderLoopStep> plan = {
        RenderLoopStep::WaitFence,
        RenderLoopStep::ConsumeMarshalled,
        RenderLoopStep::EncodeIfDirty,
        RenderLoopStep::RecordRetained,
        RenderLoopStep::SubmitPresent,
        RenderLoopStep::Recycle,
    };
    return plan;
}

bool should_reencode(bool dirty) noexcept
{
    return dirty;
}

bool should_consume(bool hasNewer) noexcept
{
    return hasNewer;
}

std::unique_ptr<SceneRenderer>
SceneRenderer::create(const VulkanLoader& loader, const Device& device,
                      const UiScene::Spec& sceneSpec)
{
    auto scene = UiScene::create(loader, device, sceneSpec);
    if (!scene)
        return nullptr;

    std::unique_ptr<SceneRenderer> r(new SceneRenderer());
    r->scene_ = std::move(scene);
    r->dirty_ = false;
    return r;
}

SceneRenderer::SceneRenderer() = default;
SceneRenderer::SceneRenderer(SceneRenderer&&) noexcept = default;
SceneRenderer& SceneRenderer::operator=(SceneRenderer&&) noexcept = default;
SceneRenderer::~SceneRenderer() = default;

bool SceneRenderer::set_scene(std::span<const DrawItem> items)
{
    if (!scene_)
        return false;
    if (!scene_->set_items(items))
        return false;
    dirty_ = true;  // the next record() draws the new content
    ++lastEncodeCount_;  // a re-encode happened (retained-scene encode counter)
    return true;
}

bool SceneRenderer::set_texture_binding(std::uint32_t textureId, VkDescriptorSet set)
{
    if (!scene_)
        return false;
    return scene_->set_texture_binding(textureId, set);
}

bool SceneRenderer::record(VkCommandBuffer cmd, const Mat4& mvp, float uiScale)
{
    if (!scene_)
        return false;
    // Record the RETAINED buffers (no re-encode). The dirty flag is cleared on
    // the first record that consumes it (the "drawn once at the new content").
    const bool ok = scene_->record_draw(cmd, mvp, uiScale);
    if (ok)
        dirty_ = false;
    return ok;
}

bool SceneRenderer::record(VkCommandBuffer cmd)
{
    if (!scene_)
        return false;
    const bool ok = scene_->record_draw(cmd);
    if (ok)
        dirty_ = false;
    return ok;
}

bool SceneRenderer::consume_changes()
{
    // S211 — the render thread's per-frame marshalled-change drain (the loop's
    // ConsumeMarshalled). Copy the LATEST published snapshot out once; if a newer
    // version was drained, re-encode the retained buffers via set_scene (EncodeIfDirty).
    // An idle frame (nothing new) returns false WITHOUT re-encoding — the retained
    // buffers are recorded as-is (the "retained scene drawn each frame" across a
    // marshalled handoff). The postbox is lock-free (release/acquire atomics).
    std::vector<DrawItem> snapshot;
    const std::uint64_t before = marshalled_.last_consumed_version();
    const std::uint64_t drained = marshalled_.consume_latest(snapshot);
    if (drained == before)
        return false;  // nothing newer this frame
    // A newer UI-thread version was consumed: re-encode the retained scene with it.
    if (scene_)
        set_scene(snapshot);
    return true;
}

void SceneRenderer::recycle(VkFence /*fence*/)
{
    if (scene_)
        scene_->recycle_buffers();
}

} // namespace vaml::vulkan::ui
