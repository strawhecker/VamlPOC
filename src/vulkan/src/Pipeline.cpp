// S165 — Pipeline implementation.
//
// Implements the S165 pure decision seams (headless-testable) + the real
// Pipeline::create() path. See Pipeline.h for the full split + the LIFETIMES
// contract.

#include <vaml/vulkan/Pipeline.h>

#include <cstring>

namespace vaml::vulkan {

ViewportScissor
viewport_scissor(VkExtent2D extent) noexcept
{
    ViewportScissor vs;
    vs.viewport.x        = 0.0f;
    vs.viewport.y        = 0.0f;
    vs.viewport.width    = static_cast<float>(extent.width);
    vs.viewport.height   = static_cast<float>(extent.height);
    vs.viewport.minDepth = 0.0f;
    vs.viewport.maxDepth = 1.0f;
    vs.scissor.offset    = VkOffset2D{ 0, 0 };
    vs.scissor.extent    = extent;
    return vs;
}

VkPushConstantRange
push_constant_range() noexcept
{
    VkPushConstantRange range{};
    range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    range.offset     = 0;
    range.size       = 64;  // one mat4 (4 columns x 16 bytes)
    return range;
}

VkPipelineVertexInputStateCreateInfo
tri_vertex_input_state() noexcept
{
    // EMPTY vertex input: no binding, no attribute, no vertex buffer. The S165
    // triangle geometry is generated in the vertex shader from gl_VertexIndex.
    // Real vertex/index buffers arrive with S200 (recorded NO-GAP, not silent).
    VkPipelineVertexInputStateCreateInfo info{};
    info.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    info.vertexBindingDescriptionCount    = 0;
    info.pVertexBindingDescriptions       = nullptr;
    info.vertexAttributeDescriptionCount  = 0;
    info.pVertexAttributeDescriptions     = nullptr;
    return info;
}

VkPipelineMultisampleStateCreateInfo
describe_multisample_state(VkSampleCountFlagBits sampleCount) noexcept
{
    VkPipelineMultisampleStateCreateInfo ms{};
    ms.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = sampleCount;  // S168: honor the pass's sample count (S165 was 1)
    ms.sampleShadingEnable  = VK_FALSE;     // no per-sample shading (edge AA only)
    return ms;
}

Mat4
tri_mvp(std::uint32_t width, std::uint32_t height) noexcept
{
    (void)width;
    (void)height;
    // S165: the shader already authors the triangle in clip space, so the MVPs
    // for any framebuffer size is the identity (column-major). This is kept +
    // extended by S200/S208 into the real uniform MVP/UI-scale projection.
    Mat4 m{};
    m[0]  = 1.0f;
    m[5]  = 1.0f;
    m[10] = 1.0f;
    m[15] = 1.0f;
    return m;
}

std::unique_ptr<Pipeline>
Pipeline::create(const VulkanLoader& loader, VkDevice device, const PipelineSpec& spec)
{
    // The S165 render-pipeline entry points are required.
    if (!loader.has_render_loader() || !device)
        return nullptr;
    if (!spec.renderPass || !spec.vertexModule || !spec.fragmentModule ||
        spec.colorFormat == VK_FORMAT_UNDEFINED || spec.extent.width == 0 || spec.extent.height == 0)
    {
        return nullptr;
    }

    std::unique_ptr<Pipeline> pl(new Pipeline(loader, device));
    pl->spec_ = spec;

    // --- Pipeline layout: one push-constant range (the S165 MVP) ---
    const VkPushConstantRange range = push_constant_range();
    VkPipelineLayoutCreateInfo plci{};
    plci.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plci.setLayoutCount         = 0;          // no descriptor sets yet (S205)
    plci.pSetLayouts            = nullptr;
    plci.pushConstantRangeCount = 1;
    plci.pPushConstantRanges    = &range;

    if (loader.vkCreatePipelineLayout(device, &plci, nullptr, &pl->pipelineLayout_) != VK_SUCCESS)
        return nullptr;

    // --- Shader stages ---
    const VkPipelineShaderStageCreateInfo stages[] = {
        { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
          VK_SHADER_STAGE_VERTEX_BIT,   spec.vertexModule,   "main", nullptr },
        { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
          VK_SHADER_STAGE_FRAGMENT_BIT, spec.fragmentModule, "main", nullptr },
    };

    const VkPipelineVertexInputStateCreateInfo   vertexInput   = tri_vertex_input_state();
    const VkPipelineInputAssemblyStateCreateInfo  inputAssembly = [] {
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
        r.cullMode                = VK_CULL_MODE_NONE;    // no cull (S165 single triangle)
        r.frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        r.depthBiasEnable         = VK_FALSE;
        r.lineWidth               = 1.0f;
        return r;
    }();
    const VkPipelineMultisampleStateCreateInfo multisampling =
        describe_multisample_state(spec.sampleCount);  // S168: match the pass's sample count
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
    const VkPipelineColorBlendAttachmentState colorBlendAttach = [] {
        VkPipelineColorBlendAttachmentState ba{};
        ba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        ba.blendEnable    = VK_FALSE;  // no blend (S200 alpha blend)
        return ba;
    }();
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

    // Viewport/scissor are dynamic (S166 sets them per-frame) — the state struct
    // carries the counts with null pointers, so the pipeline is created without
    // fixed states.
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

    if (loader.vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &gpi, nullptr, &pl->pipeline_) != VK_SUCCESS)
    {
        return nullptr;
    }

    return pl;
}

Pipeline::Pipeline(const VulkanLoader& loader, VkDevice device)
    : loader_(&loader)
    , device_(device)
{
}

Pipeline::Pipeline(Pipeline&&) noexcept = default;
Pipeline& Pipeline::operator=(Pipeline&&) noexcept = default;

Pipeline::~Pipeline()
{
    if (!loader_)
        return;
    if (pipeline_)
        loader_->vkDestroyPipeline(device_, pipeline_, nullptr);
    if (pipelineLayout_)
        loader_->vkDestroyPipelineLayout(device_, pipelineLayout_, nullptr);
    pipeline_        = VK_NULL_HANDLE;
    pipelineLayout_  = VK_NULL_HANDLE;
}

} // namespace vaml::vulkan
