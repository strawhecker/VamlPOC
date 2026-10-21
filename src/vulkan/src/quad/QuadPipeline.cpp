// S200 — QuadPipeline implementation.
//
// Implements the S200 pure decision seams (headless-testable) + the real
// QuadPipeline::create() path. See QuadPipeline.h for the split + the LIFETIMES
// contract + the SRGB pairing.

#include <vaml/vulkan/quad/QuadPipeline.h>

#include <cmath>

namespace vaml::vulkan::quad {

VkPipelineColorBlendAttachmentState
quad_blend_state(QuadBlendMode mode) noexcept
{
    VkPipelineColorBlendAttachmentState ba{};
    ba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    if (mode == QuadBlendMode::Alpha)
    {
        // Straight-alpha blending: color src*a + dst*(1-a), alpha src + dst*(1-a).
        ba.blendEnable         = VK_TRUE;
        ba.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        ba.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        ba.colorBlendOp        = VK_BLEND_OP_ADD;
        ba.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        ba.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        ba.alphaBlendOp        = VK_BLEND_OP_ADD;
    }
    else
    {
        ba.blendEnable = VK_FALSE;  // None: write-all no blend
    }
    return ba;
}

bool describe_color_blend(QuadBlendMode mode) noexcept
{
    return mode == QuadBlendMode::Alpha;
}

VkPipelineVertexInputStateCreateInfo
quad_vertex_input_state() noexcept
{
    // One binding (QuadVertex stride 32) + 3 attributes: pos (vec2 @ 0),
    // uv (vec2 @ 8), color (vec4 @ 16).
    static const VkVertexInputBindingDescription binding = [] {
        VkVertexInputBindingDescription b{};
        b.binding   = 0;
        b.stride    = sizeof(QuadVertex);
        b.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return b;
    }();
    static const VkVertexInputAttributeDescription attrs[3] = {
        { 0u, 0u, VK_FORMAT_R32G32_SFLOAT, 0u },
        { 1u, 0u, VK_FORMAT_R32G32_SFLOAT, 8u },
        { 2u, 0u, VK_FORMAT_R32G32B32A32_SFLOAT, 16u },
    };
    VkPipelineVertexInputStateCreateInfo info{};
    info.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    info.vertexBindingDescriptionCount   = 1;
    info.pVertexBindingDescriptions      = &binding;
    info.vertexAttributeDescriptionCount = 3;
    info.pVertexAttributeDescriptions    = attrs;
    return info;
}

VkPushConstantRange
quad_push_constant() noexcept
{
    VkPushConstantRange range{};
    range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    range.offset     = 0;
    range.size       = 80;  // one QuadUniform (Mat4 mvp + uiScale + pad)
    return range;
}

Mat4
ui_projection(std::uint32_t width, std::uint32_t height, float uiScale) noexcept
{
    // Orthographic logical-px → clip. The viewport is `width`×`height` logical
    // px; a logical-px origin is top-left with +Y DOWN, clip is +Y UP, so y is
    // flipped. uiScale scales the mapped extent (higher scale = more of the
    // layout visible in the same viewport).
    const float sx = (width > 0u) ? ((2.0f * uiScale) / static_cast<float>(width)) : 0.0f;
    const float sy = (height > 0u) ? ((2.0f * uiScale) / static_cast<float>(height)) : 0.0f;

    Mat4 m{};
    m[0]  = sx;            // x column-x
    m[5]  = -sy;           // y column-y (flip for +Y-up clip)
    m[10] = 1.0f;          // z identity
    m[12] = -1.0f;         // x column-w: -1 (maps x=0 → -1 NDC left)
    m[13] = 1.0f;          // y column-w: +1 (maps y=0 → +1 NDC top)
    m[15] = 1.0f;          // w column-w
    return m;
}

VkSamplerCreateInfo
quad_sampler() noexcept
{
    VkSamplerCreateInfo s{};
    s.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    s.magFilter               = VK_FILTER_LINEAR;
    s.minFilter               = VK_FILTER_LINEAR;
    s.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    s.addressModeU            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    s.addressModeV            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    s.addressModeW            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    s.mipLodBias              = 0.0f;
    s.anisotropyEnable        = VK_FALSE;
    s.maxAnisotropy           = 1.0f;
    s.compareEnable           = VK_FALSE;
    s.minLod                  = 0.0f;
    s.maxLod                  = 0.0f;
    s.borderColor             = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    s.unnormalizedCoordinates = VK_FALSE;
    return s;
}

VkDescriptorSetLayoutBinding
quad_set_layout_binding() noexcept
{
    VkDescriptorSetLayoutBinding b{};
    b.binding         = 0;
    b.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    b.descriptorCount = 1;
    b.stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;
    b.pImmutableSamplers = nullptr;
    return b;
}

VkWriteDescriptorSet
quad_descriptor_write(VkDescriptorSet set,
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

bool is_srgb_format(VkFormat format) noexcept
{
    return format == VK_FORMAT_B8G8R8A8_SRGB || format == VK_FORMAT_R8G8B8A8_SRGB;
}

std::uint32_t quad_format_bytes(VkFormat format) noexcept
{
    return is_srgb_format(format) ? 4u : 0u;
}

std::unique_ptr<QuadPipeline>
QuadPipeline::create(const VulkanLoader& loader, const Device& device, const QuadPipelineSpec& spec)
{
    const VkDevice vkDevice = device.device();
    if (!loader.has_render_loader() || !loader.has_quad_loader())
        return nullptr;
    if (!spec.renderPass || !spec.vertexModule || !spec.fragmentModule ||
        spec.colorFormat == VK_FORMAT_UNDEFINED || spec.extent.width == 0 || spec.extent.height == 0 ||
        !is_srgb_format(spec.colorFormat))  // the S200 SRGB pairing (shader linear → sRGB-encode on present)
    {
        return nullptr;
    }

    std::unique_ptr<QuadPipeline> pl(new QuadPipeline(loader, device));
    pl->spec_ = spec;

    // --- S200-local descriptor set layout: one combined image sampler ---
    const VkDescriptorSetLayoutBinding texBinding = quad_set_layout_binding();
    VkDescriptorSetLayoutCreateInfo dsci{};
    dsci.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    dsci.bindingCount = 1;
    dsci.pBindings    = &texBinding;
    if (loader.vkCreateDescriptorSetLayout(vkDevice, &dsci, nullptr, &pl->descriptorSetLayout_) != VK_SUCCESS)
        return nullptr;

    // --- Pipeline layout: one 80-byte push-constant range + the S200 set layout ---
    const VkPushConstantRange range = quad_push_constant();
    VkPipelineLayoutCreateInfo plci{};
    plci.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plci.setLayoutCount         = 1;
    plci.pSetLayouts            = &pl->descriptorSetLayout_;
    plci.pushConstantRangeCount = 1;
    plci.pPushConstantRanges    = &range;
    if (loader.vkCreatePipelineLayout(vkDevice, &plci, nullptr, &pl->pipelineLayout_) != VK_SUCCESS)
        return nullptr;

    // --- Shader stages ---
    const VkPipelineShaderStageCreateInfo stages[] = {
        { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
          VK_SHADER_STAGE_VERTEX_BIT,   spec.vertexModule,   "main", nullptr },
        { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
          VK_SHADER_STAGE_FRAGMENT_BIT, spec.fragmentModule, "main", nullptr },
    };

    const VkPipelineVertexInputStateCreateInfo   vertexInput   = quad_vertex_input_state();
    const VkPipelineInputAssemblyStateCreateInfo  inputAssembly = [] {
        VkPipelineInputAssemblyStateCreateInfo ia{};
        ia.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        ia.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;  // quads = indexed triangle list
        ia.primitiveRestartEnable = VK_FALSE;
        return ia;
    }();
    const VkPipelineRasterizationStateCreateInfo rasterizer = [] {
        VkPipelineRasterizationStateCreateInfo r{};
        r.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        r.depthClampEnable        = VK_FALSE;
        r.rasterizerDiscardEnable = VK_FALSE;
        r.polygonMode             = VK_POLYGON_MODE_FILL;
        r.cullMode                = VK_CULL_MODE_NONE;    // no cull (quad is a front-facing unit quad)
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
    // The S200 alpha blend (Alpha mode by default; S201 can record a per-scene
    // blend change). The blend attachment references the color attachment of the
    // pass — one color attachment (the SRGB swapchain color + optional resolve).
    const VkPipelineColorBlendAttachmentState colorBlendAttach = quad_blend_state(QuadBlendMode::Alpha);
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

QuadPipeline::QuadPipeline(const VulkanLoader& loader, const Device& device)
    : loader_(&loader)
    , device_(device.device())
{
}

QuadPipeline::QuadPipeline(QuadPipeline&&) noexcept = default;
QuadPipeline& QuadPipeline::operator=(QuadPipeline&&) noexcept = default;

QuadPipeline::~QuadPipeline()
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

} // namespace vaml::vulkan::quad
