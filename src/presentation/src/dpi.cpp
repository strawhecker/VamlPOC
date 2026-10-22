// S213 (dep:S212_1) — DPI + framebuffer scale implementation (vaml::presentation::dpi).
//
// Implements the S213 pure decision seams (headless-testable, the standing split):
// the DERIVED framebuffer scale + the canvas projection + the self-consistent render
// plan + the logical→physical mapping. See dpi.h for the crisp invariant: uiScale
// MUST equal physical/logical so the S208 clip scissor aligns with the scaled
// geometry at the PHYSICAL viewport UiScene sets.

#include <vaml/presentation/dpi.h>

#include <vaml/vulkan/quad/QuadPipeline.h>  // quad::ui_projection (the S200 MVP)

namespace vaml::presentation::dpi {

float framebuffer_scale_for(std::uint32_t physicalW, std::uint32_t physicalH,
                            float logicalW, float logicalH) noexcept
{
    (void)physicalH;  // the X-axis ratio is the crisp scale; Y is derived the same way
    // Never divide by a non-positive logical extent (a not-yet-set frame size -> the
    // identity default). A zero physical extent also collapses to identity.
    if (!(logicalW > 0.0f) || !(logicalH > 0.0f) || physicalW == 0u || physicalH == 0u)
        return 1.0f;
    return static_cast<float>(physicalW) / logicalW;
}

vaml::vulkan::ui::Mat4 canvas_projection(float logicalW, float logicalH) noexcept
{
    // The S200 UI-scale MVP over the logical canvas with uiScale 1.0. When the
    // logical frame is not yet set (non-positive), ui_projection accepts it as-is
    // (its finite/NDC guard is a framebuffer-size guard, not a positivity one) — the
    // host only records once a real frame size is set.
    const std::uint32_t w = logicalW > 0.0f ? static_cast<std::uint32_t>(logicalW) : 0u;
    const std::uint32_t h = logicalH > 0.0f ? static_cast<std::uint32_t>(logicalH) : 0u;
    return vaml::vulkan::quad::ui_projection(w, h, 1.0f);
}

RenderPlan dpi_render_plan(std::uint32_t physicalW, std::uint32_t physicalH,
                           float logicalW, float logicalH) noexcept
{
    RenderPlan plan{};
    plan.mvp     = canvas_projection(logicalW, logicalH);
    plan.uiScale = framebuffer_scale_for(physicalW, physicalH, logicalW, logicalH);
    return plan;
}

PhysicalRect logical_to_physical(float x, float y, float w, float h, float scale) noexcept
{
    PhysicalRect out;
    out.x = x * scale;
    out.y = y * scale;
    out.w = w * scale;
    out.h = h * scale;
    return out;
}

} // namespace vaml::presentation::dpi
