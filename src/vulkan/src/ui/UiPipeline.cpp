// S205 — UiPipeline implementation.
//
// Implements the S205 pure decision seams (headless-testable) + the real
// UiPipeline::create() path. See UiPipeline.h for the shared-layout split + the
// LIFETIMES contract + the SRGB pairing. The border seams (sd_rounded_box +
// border_coverage) are the CPU-side authority that border.frag mirrors verbatim.

#include <vaml/vulkan/ui/UiPipeline.h>

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace vaml::vulkan::ui {

VkPushConstantRange
ui_push_constant() noexcept
{
    VkPushConstantRange range{};
    range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    range.offset     = 0;
    range.size       = 128;  // the shared UiPushConstants
    return range;
}

VkDescriptorSetLayoutBinding
ui_shared_set_layout_binding() noexcept
{
    VkDescriptorSetLayoutBinding b{};
    b.binding         = 0;
    b.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    b.descriptorCount = 1;
    b.stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;
    b.pImmutableSamplers = nullptr;
    return b;
}

VkSamplerCreateInfo
ui_sampler() noexcept
{
    // Same linear/clamp decision as the S200 quad sampler (SDF + solid/border
    // sample the same atlas the quads do), so a UI pipeline can share the atlas.
    return quad::quad_sampler();
}

VkWriteDescriptorSet
ui_descriptor_write(VkDescriptorSet set,
                    VkDescriptorImageInfo& info,
                    VkImageView imageView,
                    VkSampler sampler) noexcept
{
    // The caller-owned `info` is the descriptor image info the returned write's
    // pImageInfo points into — it MUST outlive the vkUpdateDescriptorSets call.
    info.sampler     = sampler;
    info.imageView   = imageView;
    info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet w{};
    w.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    w.dstSet          = set;
    w.dstBinding      = 0;
    w.dstArrayElement = 0;
    w.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    w.descriptorCount = 1;
    w.pImageInfo      = &info;
    return w;
}

void solid_style(UiPushConstants& pc, const Color4& color) noexcept
{
    pc.color  = color;
    pc.params = { 0.0f, 0.0f, 0.0f, 0.0f };  // no params
}

void border_style(UiPushConstants& pc, const Color4& color,
                  float thickness01, float radius01, float feather) noexcept
{
    pc.color  = color;
    pc.params = { thickness01, radius01, feather, 0.0f };
}

void sdf_style(UiPushConstants& pc, const Color4& color, float aa) noexcept
{
    pc.color  = color;
    pc.params = { aa, 0.0f, 0.0f, 0.0f };
}

float sd_rounded_box(float px, float py, float b, float r) noexcept
{
    // Matches border.frag's sdRoundBox EXACTLY (the CPU-side authority): the
    // signed distance to an axis-aligned rounded box centered at the origin with
    // half-extent `b` and corner radius `r` (negative inside, positive outside),
    // iq's rounded-box form: q = abs(p)-b+r; result = min(max(q),0) +
    // length(max(q,0)) - r. (Note the FULL radius r, matching the shader.)
    const float qx = std::abs(px) - b + r;
    const float qy = std::abs(py) - b + r;
    const float lenQ = std::sqrt(std::max(qx, 0.0f) * std::max(qx, 0.0f) +
                                 std::max(qy, 0.0f) * std::max(qy, 0.0f));
    const float inner = std::min(std::max(qx, qy), 0.0f);
    return lenQ + inner - r;
}

float border_coverage(float uvx, float uvy,
                      float thickness01, float radius01,
                      float feather, float aa) noexcept
{
    // Mirrors border.frag's main() verbatim (the CPU-side authority).
    const float t = std::clamp(thickness01, 0.0f, 0.5f);  // border band width
    const float r = std::clamp(radius01, 0.0f, 0.5f);     // corner rounding

    const float px = uvx - 0.5f;
    const float py = uvy - 0.5f;
    // Outer rounded-box distance (inside the quad <= 0) and inner rounded-box
    // distance (inside the inner hole > 0) — the band is between them.
    const float outer = sd_rounded_box(px, py, 0.5f, r);
    const float inner = sd_rounded_box(px, py, 0.5f - t, std::max(r - t, 0.0f));
    // Inside the band: -outer > 0 AND inner > 0; the tighter (`min`) wins.
    const float dist = std::min(-outer, inner);

    const float w = std::max(std::max(feather, aa), 0.000001f);
    // The linear AA ramp (dist < -w -> 0, dist > +w -> 1, edge -> ~0.5).
    const float alpha = std::clamp(dist / (2.0f * w) + 0.5f, 0.0f, 1.0f);
    return alpha;
}

std::unique_ptr<UiPipeline>
UiPipeline::create(const VulkanLoader& loader, const Device& device,
                   const UiPipelineSpec& spec, UiShaderKind kind)
{
    const VkDevice vkDevice = device.device();
    if (!loader.has_render_loader() || !loader.has_texture_loader())
        return nullptr;
    if (!spec.renderPass || !spec.vertexModule || !spec.fragmentModule ||
        spec.colorFormat == VK_FORMAT_UNDEFINED || spec.extent.width == 0 || spec.extent.height == 0 ||
        !quad::is_srgb_format(spec.colorFormat))  // the S200 SRGB pairing
    {
        return nullptr;
    }

    std::unique_ptr<UiPipeline> pl(new UiPipeline(loader, device));
    pl->kind_ = kind;
    pl->spec_ = spec;

    // --- SHARED descriptor set layout: set 0 binding 0 = combined image sampler ---
    const VkDescriptorSetLayoutBinding sharedBinding = ui_shared_set_layout_binding();
    VkDescriptorSetLayoutCreateInfo dsci{};
    dsci.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    dsci.bindingCount = 1;
    dsci.pBindings    = &sharedBinding;
    if (loader.vkCreateDescriptorSetLayout(vkDevice, &dsci, nullptr, &pl->descriptorSetLayout_) != VK_SUCCESS)
        return nullptr;

    // --- SHARED pipeline layout: one 128-byte VERTEX|FRAGMENT range + the set layout ---
    const VkPushConstantRange range = ui_push_constant();
    VkPipelineLayoutCreateInfo plci{};
    plci.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plci.setLayoutCount         = 1;
    plci.pSetLayouts            = &pl->descriptorSetLayout_;
    plci.pushConstantRangeCount = 1;
    plci.pPushConstantRanges    = &range;
    if (loader.vkCreatePipelineLayout(vkDevice, &plci, nullptr, &pl->pipelineLayout_) != VK_SUCCESS)
        return nullptr;

    // --- Shader stages: shared ui_vertex + the kind's fragment ---
    const VkPipelineShaderStageCreateInfo stages[] = {
        { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
          VK_SHADER_STAGE_VERTEX_BIT,   spec.vertexModule,   "main", nullptr },
        { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
          VK_SHADER_STAGE_FRAGMENT_BIT, spec.fragmentModule, "main", nullptr },
    };

    // Reuse the S200 quad vertex-input state (QuadVertex stride 32 + 3 attrs) +
    // the S200 straight-alpha blend (the shared UI geometry + blend contract).
    const VkPipelineVertexInputStateCreateInfo  vertexInput   = quad::quad_vertex_input_state();
    const VkPipelineInputAssemblyStateCreateInfo inputAssembly = [] {
        VkPipelineInputAssemblyStateCreateInfo ia{};
        ia.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        ia.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        ia.primitiveRestartEnable = VK_FALSE;
        return ia;
    }();
    const VkPipelineRasterizationStateCreateInfo rasterizer = [] {
        VkPipelineRasterizationStateCreateInfo r{};
        r.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        r.depthClampEnable        = VK_FALSE;
        r.rasterizerDiscardEnable = VK_FALSE;
        r.polygonMode             = VK_POLYGON_MODE_FILL;
        r.cullMode                = VK_CULL_MODE_NONE;
        r.frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        r.depthBiasEnable         = VK_FALSE;
        r.lineWidth               = 1.0f;
        return r;
    }();
    const VkPipelineMultisampleStateCreateInfo multisampling = [&] {
        VkPipelineMultisampleStateCreateInfo ms{};
        ms.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        ms.rasterizationSamples = spec.sampleCount;
        ms.sampleShadingEnable  = VK_FALSE;
        return ms;
    }();
    const VkPipelineDepthStencilStateCreateInfo depthStencil = [&] {
        VkPipelineDepthStencilStateCreateInfo ds{};
        ds.sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        ds.depthTestEnable       = spec.hasDepth ? VK_TRUE : VK_FALSE;
        ds.depthWriteEnable      = spec.hasDepth ? VK_TRUE : VK_FALSE;
        ds.depthCompareOp        = VK_COMPARE_OP_LESS;
        ds.depthBoundsTestEnable = VK_FALSE;
        ds.stencilTestEnable     = VK_FALSE;
        return ds;
    }();
    // The S200 straight-alpha blend (Alpha mode), the S200-style blend attachment.
    const VkPipelineColorBlendAttachmentState colorBlendAttach = quad::quad_blend_state(quad::QuadBlendMode::Alpha);
    const VkPipelineColorBlendStateCreateInfo colorBlending = [&] {
        VkPipelineColorBlendStateCreateInfo cb{};
        cb.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        cb.logicOpEnable   = VK_FALSE;
        cb.attachmentCount = 1;
        cb.pAttachments    = &colorBlendAttach;
        return cb;
    }();

    const VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates    = dynamicStates;

    VkPipelineViewportStateCreateInfo vp{};
    vp.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vp.viewportCount = 1;
    vp.pViewports    = nullptr;
    vp.scissorCount  = 1;
    vp.pScissors     = nullptr;

    VkGraphicsPipelineCreateInfo gpi{};
    gpi.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    gpi.stageCount          = 2;
    gpi.pStages             = stages;
    gpi.pVertexInputState   = &vertexInput;
    gpi.pInputAssemblyState = &inputAssembly;
    gpi.pViewportState      = &vp;
    gpi.pRasterizationState = &rasterizer;
    gpi.pMultisampleState   = &multisampling;
    gpi.pDepthStencilState  = &depthStencil;
    gpi.pColorBlendState    = &colorBlending;
    gpi.pDynamicState       = &dynamicState;
    gpi.layout              = pl->pipelineLayout_;
    gpi.renderPass          = spec.renderPass;
    gpi.subpass             = 0;

    if (loader.vkCreateGraphicsPipelines(vkDevice, VK_NULL_HANDLE, 1, &gpi, nullptr, &pl->pipeline_) != VK_SUCCESS)
        return nullptr;

    return pl;
}

UiPipeline::UiPipeline(const VulkanLoader& loader, const Device& device)
    : loader_(&loader)
    , device_(device.device())
{
}

UiPipeline::UiPipeline(UiPipeline&&) noexcept = default;
UiPipeline& UiPipeline::operator=(UiPipeline&&) noexcept = default;

UiPipeline::~UiPipeline()
{
    if (!loader_)
        return;
    if (pipeline_)
        loader_->vkDestroyPipeline(device_, pipeline_, nullptr);
    if (descriptorSetLayout_)
        loader_->vkDestroyDescriptorSetLayout(device_, descriptorSetLayout_, nullptr);
    if (pipelineLayout_)
        loader_->vkDestroyPipelineLayout(device_, pipelineLayout_, nullptr);
    pipeline_            = VK_NULL_HANDLE;
    descriptorSetLayout_ = VK_NULL_HANDLE;
    pipelineLayout_      = VK_NULL_HANDLE;
}

} // namespace vaml::vulkan::ui
