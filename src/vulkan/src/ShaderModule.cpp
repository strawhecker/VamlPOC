// S165 — ShaderModule implementation + the embedded single-triangle SPIR-V.
//
// See ShaderModule.h for the shader contract + the pre-baked SPIR-V rationale
// (no SPIR-V toolchain in-repo; a real shader-authoring pipeline is S205). The
// SPIR-V below is hand-authored (SPIR-V 1.0) from the GLSL in these comments and
// is validated LIVE via vkCreateShaderModule on the POCs ([both apps, not CI]).
//
// The vertex shader is SELF-CONTAINED: it generates the three hardcoded
// clip-space triangle positions from gl_VertexIndex (S165 has NO vertex buffer —
// tri_vertex_input_state returns an empty vertex-input state), reads only the
// S165 push-constant MVP, and writes gl_Position. This matches the "hardcoded
// MVP" minimalism: the geometry is authored in clip space, so the MVP pushed
// (tri_mvp, see Pipeline) is the identity (documented; S200 turns tri_mvp into a
// real uniform MVP/UI-scale projection).
//
// === VERTEX ===
//   #version 450
//   layout(push_constant) uniform PushConstants { mat4 mvp; } pc;
//   layout(location = 0) out vec3 vColor;
//   void main() {
//     vec4 positions[3] = vec4[](
//       vec4(-0.8,  0.8, 0.0, 1.0),
//       vec4( 0.8,  0.8, 0.0, 1.0),
//       vec4( 0.0, -0.8, 0.0, 1.0));
//     vec4 pos = positions[gl_VertexIndex];
//     gl_Position = pc.mvp * pos;
//     vColor = vec3(0.5, 0.8, 1.0);
//   }
//
// === FRAGMENT ===
//   #version 450
//   layout(location = 0) in vec3 vColor;
//   layout(location = 0) out vec4 outColor;
//   void main() { outColor = vec4(vColor, 1.0); }

#include <vaml/vulkan/ShaderModule.h>

#include <array>

namespace vaml::vulkan {

namespace {

// The three hardcoded clip-space positions of the single counter-clockwise
// triangle (v0 top-left, v1 top-right, v2 bottom-center). These MUST match the
// in-shader positions (ShaderModule.h + Pipeline_test assert the contract).
constexpr std::array<float, 12> kTriPositions = {
    -0.8f,  0.8f, 0.0f, 1.0f,
     0.8f,  0.8f, 0.0f, 1.0f,
     0.0f, -0.8f, 0.0f, 1.0f,
};

// The hardcoded fragment color (light blue).
constexpr std::array<float, 4> kTriColor = { 0.5f, 0.8f, 1.0f, 1.0f };

// ---------------------------------------------------------------------------
// Embedded vertex shader SPIR-V.
// Compiled from the canonical GLSL above with glslangValidator 16.2.0
// (`-V tri.vert`), validated with spirv-val, then debug-stripped with
// spirv-opt --strip-debug. The raw word array is generated from the validated
// binary (build/s165_shaders/emit.py) so it is EXACTLY the device-shader bytes —
// hand-authoring SPIR-V would risk a silently-corrupt module (NO-LAZY-LLM).
// The shader generates the triangle from gl_VertexIndex (no vertex buffer yet),
// reads the S165 push-constant MVP, and writes gl_Position + a location-0 color.
// ---------------------------------------------------------------------------
constexpr std::array<std::uint32_t, 295> kTriVertexSpirv = {
    0x07230203u, 0x00010000u, 0x0008000bu, 0x00000033u, 0x00000000u, 0x00020011u,
    0x00000001u, 0x0006000bu, 0x00000001u, 0x4c534c47u, 0x6474732eu, 0x3035342eu,
    0x00000000u, 0x0003000eu, 0x00000000u, 0x00000001u, 0x0008000fu, 0x00000000u,
    0x00000004u, 0x6e69616du, 0x00000000u, 0x00000019u, 0x00000021u, 0x00000030u,
    0x00040047u, 0x00000019u, 0x0000000bu, 0x0000002au, 0x00030047u, 0x0000001fu,
    0x00000002u, 0x00050048u, 0x0000001fu, 0x00000000u, 0x0000000bu, 0x00000000u,
    0x00050048u, 0x0000001fu, 0x00000001u, 0x0000000bu, 0x00000001u, 0x00050048u,
    0x0000001fu, 0x00000002u, 0x0000000bu, 0x00000003u, 0x00050048u, 0x0000001fu,
    0x00000003u, 0x0000000bu, 0x00000004u, 0x00030047u, 0x00000024u, 0x00000002u,
    0x00040048u, 0x00000024u, 0x00000000u, 0x00000005u, 0x00050048u, 0x00000024u,
    0x00000000u, 0x00000007u, 0x00000010u, 0x00050048u, 0x00000024u, 0x00000000u,
    0x00000023u, 0x00000000u, 0x00040047u, 0x00000030u, 0x0000001eu, 0x00000000u,
    0x00020013u, 0x00000002u, 0x00030021u, 0x00000003u, 0x00000002u, 0x00030016u,
    0x00000006u, 0x00000020u, 0x00040017u, 0x00000007u, 0x00000006u, 0x00000004u,
    0x00040015u, 0x00000008u, 0x00000020u, 0x00000000u, 0x0004002bu, 0x00000008u,
    0x00000009u, 0x00000003u, 0x0004001cu, 0x0000000au, 0x00000007u, 0x00000009u,
    0x00040020u, 0x0000000bu, 0x00000007u, 0x0000000au, 0x0004002bu, 0x00000006u,
    0x0000000du, 0xbf4ccccdu, 0x0004002bu, 0x00000006u, 0x0000000eu, 0x3f4ccccdu,
    0x0004002bu, 0x00000006u, 0x0000000fu, 0x00000000u, 0x0004002bu, 0x00000006u,
    0x00000010u, 0x3f800000u, 0x0007002cu, 0x00000007u, 0x00000011u, 0x0000000du,
    0x0000000eu, 0x0000000fu, 0x00000010u, 0x0007002cu, 0x00000007u, 0x00000012u,
    0x0000000eu, 0x0000000eu, 0x0000000fu, 0x00000010u, 0x0007002cu, 0x00000007u,
    0x00000013u, 0x0000000fu, 0x0000000du, 0x0000000fu, 0x00000010u, 0x0006002cu,
    0x0000000au, 0x00000014u, 0x00000011u, 0x00000012u, 0x00000013u, 0x00040020u,
    0x00000015u, 0x00000007u, 0x00000007u, 0x00040015u, 0x00000017u, 0x00000020u,
    0x00000001u, 0x00040020u, 0x00000018u, 0x00000001u, 0x00000017u, 0x0004003bu,
    0x00000018u, 0x00000019u, 0x00000001u, 0x0004002bu, 0x00000008u, 0x0000001du,
    0x00000001u, 0x0004001cu, 0x0000001eu, 0x00000006u, 0x0000001du, 0x0006001eu,
    0x0000001fu, 0x00000007u, 0x00000006u, 0x0000001eu, 0x0000001eu, 0x00040020u,
    0x00000020u, 0x00000003u, 0x0000001fu, 0x0004003bu, 0x00000020u, 0x00000021u,
    0x00000003u, 0x0004002bu, 0x00000017u, 0x00000022u, 0x00000000u, 0x00040018u,
    0x00000023u, 0x00000007u, 0x00000004u, 0x0003001eu, 0x00000024u, 0x00000023u,
    0x00040020u, 0x00000025u, 0x00000009u, 0x00000024u, 0x0004003bu, 0x00000025u,
    0x00000026u, 0x00000009u, 0x00040020u, 0x00000027u, 0x00000009u, 0x00000023u,
    0x00040020u, 0x0000002cu, 0x00000003u, 0x00000007u, 0x00040017u, 0x0000002eu,
    0x00000006u, 0x00000003u, 0x00040020u, 0x0000002fu, 0x00000003u, 0x0000002eu,
    0x0004003bu, 0x0000002fu, 0x00000030u, 0x00000003u, 0x0004002bu, 0x00000006u,
    0x00000031u, 0x3f000000u, 0x0006002cu, 0x0000002eu, 0x00000032u, 0x00000031u,
    0x0000000eu, 0x00000010u, 0x00050036u, 0x00000002u, 0x00000004u, 0x00000000u,
    0x00000003u, 0x000200f8u, 0x00000005u, 0x0004003bu, 0x0000000bu, 0x0000000cu,
    0x00000007u, 0x0004003bu, 0x00000015u, 0x00000016u, 0x00000007u, 0x0003003eu,
    0x0000000cu, 0x00000014u, 0x0004003du, 0x00000017u, 0x0000001au, 0x00000019u,
    0x00050041u, 0x00000015u, 0x0000001bu, 0x0000000cu, 0x0000001au, 0x0004003du,
    0x00000007u, 0x0000001cu, 0x0000001bu, 0x0003003eu, 0x00000016u, 0x0000001cu,
    0x00050041u, 0x00000027u, 0x00000028u, 0x00000026u, 0x00000022u, 0x0004003du,
    0x00000023u, 0x00000029u, 0x00000028u, 0x0004003du, 0x00000007u, 0x0000002au,
    0x00000016u, 0x00050091u, 0x00000007u, 0x0000002bu, 0x00000029u, 0x0000002au,
    0x00050041u, 0x0000002cu, 0x0000002du, 0x00000021u, 0x00000022u, 0x0003003eu,
    0x0000002du, 0x0000002bu, 0x0003003eu, 0x00000030u, 0x00000032u, 0x000100fdu,
    0x00010038u,
};

// ---------------------------------------------------------------------------
// Embedded fragment shader SPIR-V.
// Compiled from the canonical GLSL above with glslangValidator 16.2.0
// (`-V tri.frag`), validated with spirv-val, then debug-stripped with
// spirv-opt --strip-debug. Reads the location-0 color from the vertex shader
// and writes the location-0 color attachment.
// ---------------------------------------------------------------------------
constexpr std::array<std::uint32_t, 108> kTriFragmentSpirv = {
    0x07230203u, 0x00010000u, 0x0008000bu, 0x00000013u, 0x00000000u, 0x00020011u,
    0x00000001u, 0x0006000bu, 0x00000001u, 0x4c534c47u, 0x6474732eu, 0x3035342eu,
    0x00000000u, 0x0003000eu, 0x00000000u, 0x00000001u, 0x0007000fu, 0x00000004u,
    0x00000004u, 0x6e69616du, 0x00000000u, 0x00000009u, 0x0000000cu, 0x00030010u,
    0x00000004u, 0x00000007u, 0x00040047u, 0x00000009u, 0x0000001eu, 0x00000000u,
    0x00040047u, 0x0000000cu, 0x0000001eu, 0x00000000u, 0x00020013u, 0x00000002u,
    0x00030021u, 0x00000003u, 0x00000002u, 0x00030016u, 0x00000006u, 0x00000020u,
    0x00040017u, 0x00000007u, 0x00000006u, 0x00000004u, 0x00040020u, 0x00000008u,
    0x00000003u, 0x00000007u, 0x0004003bu, 0x00000008u, 0x00000009u, 0x00000003u,
    0x00040017u, 0x0000000au, 0x00000006u, 0x00000003u, 0x00040020u, 0x0000000bu,
    0x00000001u, 0x0000000au, 0x0004003bu, 0x0000000bu, 0x0000000cu, 0x00000001u,
    0x0004002bu, 0x00000006u, 0x0000000eu, 0x3f800000u, 0x00050036u, 0x00000002u,
    0x00000004u, 0x00000000u, 0x00000003u, 0x000200f8u, 0x00000005u, 0x0004003du,
    0x0000000au, 0x0000000du, 0x0000000cu, 0x00050051u, 0x00000006u, 0x0000000fu,
    0x0000000du, 0x00000000u, 0x00050051u, 0x00000006u, 0x00000010u, 0x0000000du,
    0x00000001u, 0x00050051u, 0x00000006u, 0x00000011u, 0x0000000du, 0x00000002u,
    0x00070050u, 0x00000007u, 0x00000012u, 0x0000000fu, 0x00000010u, 0x00000011u,
    0x0000000eu, 0x0003003eu, 0x00000009u, 0x00000012u, 0x000100fdu, 0x00010038u,
};

} // namespace

std::span<const std::uint32_t> tri_vertex_spirv() noexcept
{
    return std::span<const std::uint32_t>(kTriVertexSpirv.data(), kTriVertexSpirv.size());
}

std::span<const std::uint32_t> tri_fragment_spirv() noexcept
{
    return std::span<const std::uint32_t>(kTriFragmentSpirv.data(), kTriFragmentSpirv.size());
}

// ---------------------------------------------------------------------------
// S200 embedded QUAD shader pair.
// Compiled from the canonical GLSL below with glslangValidator 16.2.0 on the
// Linux VM, validated with spirv-val, then debug-stripped with spirv-opt
// --strip-debug (the S165 precedent — NOT hand-authored bytes; a real toolchain
// produced + validated them). The vertex shader consumes the S200 80-byte
// QuadUniform push constant + the S200 vertex input; the fragment samples the
// S200 combined-image-sampler descriptor (set 0, binding 0) and multiplies by
// the straight-alpha vertex color.
//
// === QUAD VERTEX ===
//   #version 450
//   layout(push_constant) uniform PushConstants { mat4 mvp; float uiScale; } pc;
//   layout(location = 0) in vec2 position;
//   layout(location = 1) in vec2 uv;
//   layout(location = 2) in vec4 color;
//   layout(location = 0) out vec2 vUv;
//   layout(location = 1) out vec4 vColor;
//   void main() {
//     gl_Position = pc.mvp * vec4(position, 0.0, 1.0);
//     vUv = uv;
//     vColor = color;
//   }
//
// === QUAD FRAGMENT ===
//   #version 450
//   layout(set = 0, binding = 0) uniform sampler2D tex;
//   layout(location = 0) in vec2 vUv;
//   layout(location = 1) in vec4 vColor;
//   layout(location = 0) out vec4 outColor;
//   void main() {
//     vec4 sampled = texture(tex, vUv);
//     outColor = sampled * vColor;
//   }
// ---------------------------------------------------------------------------
constexpr std::array<std::uint32_t, 272> kQuadVertexSpirv = {
    0x07230203u, 0x00010000u, 0x0008000bu, 0x0000002bu, 0x00000000u, 0x00020011u,
    0x00000001u, 0x0006000bu, 0x00000001u, 0x4c534c47u, 0x6474732eu, 0x3035342eu,
    0x00000000u, 0x0003000eu, 0x00000000u, 0x00000001u, 0x000b000fu, 0x00000000u,
    0x00000004u, 0x6e69616du, 0x00000000u, 0x0000000du, 0x00000019u, 0x00000024u,
    0x00000025u, 0x00000027u, 0x00000029u, 0x00030047u, 0x0000000bu, 0x00000002u,
    0x00050048u, 0x0000000bu, 0x00000000u, 0x0000000bu, 0x00000000u, 0x00050048u,
    0x0000000bu, 0x00000001u, 0x0000000bu, 0x00000001u, 0x00050048u, 0x0000000bu,
    0x00000002u, 0x0000000bu, 0x00000003u, 0x00050048u, 0x0000000bu, 0x00000003u,
    0x0000000bu, 0x00000004u, 0x00030047u, 0x00000011u, 0x00000002u, 0x00040048u,
    0x00000011u, 0x00000000u, 0x00000005u, 0x00050048u, 0x00000011u, 0x00000000u,
    0x00000007u, 0x00000010u, 0x00050048u, 0x00000011u, 0x00000000u, 0x00000023u,
    0x00000000u, 0x00050048u, 0x00000011u, 0x00000001u, 0x00000023u, 0x00000040u,
    0x00040047u, 0x00000019u, 0x0000001eu, 0x00000000u, 0x00040047u, 0x00000024u,
    0x0000001eu, 0x00000000u, 0x00040047u, 0x00000025u, 0x0000001eu, 0x00000001u,
    0x00040047u, 0x00000027u, 0x0000001eu, 0x00000001u, 0x00040047u, 0x00000029u,
    0x0000001eu, 0x00000002u, 0x00020013u, 0x00000002u, 0x00030021u, 0x00000003u,
    0x00000002u, 0x00030016u, 0x00000006u, 0x00000020u, 0x00040017u, 0x00000007u,
    0x00000006u, 0x00000004u, 0x00040015u, 0x00000008u, 0x00000020u, 0x00000000u,
    0x0004002bu, 0x00000008u, 0x00000009u, 0x00000001u, 0x0004001cu, 0x0000000au,
    0x00000006u, 0x00000009u, 0x0006001eu, 0x0000000bu, 0x00000007u, 0x00000006u,
    0x0000000au, 0x0000000au, 0x00040020u, 0x0000000cu, 0x00000003u, 0x0000000bu,
    0x0004003bu, 0x0000000cu, 0x0000000du, 0x00000003u, 0x00040015u, 0x0000000eu,
    0x00000020u, 0x00000001u, 0x0004002bu, 0x0000000eu, 0x0000000fu, 0x00000000u,
    0x00040018u, 0x00000010u, 0x00000007u, 0x00000004u, 0x0004001eu, 0x00000011u,
    0x00000010u, 0x00000006u, 0x00040020u, 0x00000012u, 0x00000009u, 0x00000011u,
    0x0004003bu, 0x00000012u, 0x00000013u, 0x00000009u, 0x00040020u, 0x00000014u,
    0x00000009u, 0x00000010u, 0x00040017u, 0x00000017u, 0x00000006u, 0x00000002u,
    0x00040020u, 0x00000018u, 0x00000001u, 0x00000017u, 0x0004003bu, 0x00000018u,
    0x00000019u, 0x00000001u, 0x0004002bu, 0x00000006u, 0x0000001bu, 0x00000000u,
    0x0004002bu, 0x00000006u, 0x0000001cu, 0x3f800000u, 0x00040020u, 0x00000021u,
    0x00000003u, 0x00000007u, 0x00040020u, 0x00000023u, 0x00000003u, 0x00000017u,
    0x0004003bu, 0x00000023u, 0x00000024u, 0x00000003u, 0x0004003bu, 0x00000018u,
    0x00000025u, 0x00000001u, 0x0004003bu, 0x00000021u, 0x00000027u, 0x00000003u,
    0x00040020u, 0x00000028u, 0x00000001u, 0x00000007u, 0x0004003bu, 0x00000028u,
    0x00000029u, 0x00000001u, 0x00050036u, 0x00000002u, 0x00000004u, 0x00000000u,
    0x00000003u, 0x000200f8u, 0x00000005u, 0x00050041u, 0x00000014u, 0x00000015u,
    0x00000013u, 0x0000000fu, 0x0004003du, 0x00000010u, 0x00000016u, 0x00000015u,
    0x0004003du, 0x00000017u, 0x0000001au, 0x00000019u, 0x00050051u, 0x00000006u,
    0x0000001du, 0x0000001au, 0x00000000u, 0x00050051u, 0x00000006u, 0x0000001eu,
    0x0000001au, 0x00000001u, 0x00070050u, 0x00000007u, 0x0000001fu, 0x0000001du,
    0x0000001eu, 0x0000001bu, 0x0000001cu, 0x00050091u, 0x00000007u, 0x00000020u,
    0x00000016u, 0x0000001fu, 0x00050041u, 0x00000021u, 0x00000022u, 0x0000000du,
    0x0000000fu, 0x0003003eu, 0x00000022u, 0x00000020u, 0x0004003du, 0x00000017u,
    0x00000026u, 0x00000025u, 0x0003003eu, 0x00000024u, 0x00000026u, 0x0004003du,
    0x00000007u, 0x0000002au, 0x00000029u, 0x0003003eu, 0x00000027u, 0x0000002au,
    0x000100fdu, 0x00010038u,
};

constexpr std::array<std::uint32_t, 156> kQuadFragmentSpirv = {
    0x07230203u, 0x00010000u, 0x0008000bu, 0x0000001bu, 0x00000000u, 0x00020011u,
    0x00000001u, 0x0006000bu, 0x00000001u, 0x4c534c47u, 0x6474732eu, 0x3035342eu,
    0x00000000u, 0x0003000eu, 0x00000000u, 0x00000001u, 0x0008000fu, 0x00000004u,
    0x00000004u, 0x6e69616du, 0x00000000u, 0x00000011u, 0x00000015u, 0x00000018u,
    0x00030010u, 0x00000004u, 0x00000007u, 0x00040047u, 0x0000000du, 0x00000021u,
    0x00000000u, 0x00040047u, 0x0000000du, 0x00000022u, 0x00000000u, 0x00040047u,
    0x00000011u, 0x0000001eu, 0x00000000u, 0x00040047u, 0x00000015u, 0x0000001eu,
    0x00000000u, 0x00040047u, 0x00000018u, 0x0000001eu, 0x00000001u, 0x00020013u,
    0x00000002u, 0x00030021u, 0x00000003u, 0x00000002u, 0x00030016u, 0x00000006u,
    0x00000020u, 0x00040017u, 0x00000007u, 0x00000006u, 0x00000004u, 0x00040020u,
    0x00000008u, 0x00000007u, 0x00000007u, 0x00090019u, 0x0000000au, 0x00000006u,
    0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000001u, 0x00000000u,
    0x0003001bu, 0x0000000bu, 0x0000000au, 0x00040020u, 0x0000000cu, 0x00000000u,
    0x0000000bu, 0x0004003bu, 0x0000000cu, 0x0000000du, 0x00000000u, 0x00040017u,
    0x0000000fu, 0x00000006u, 0x00000002u, 0x00040020u, 0x00000010u, 0x00000001u,
    0x0000000fu, 0x0004003bu, 0x00000010u, 0x00000011u, 0x00000001u, 0x00040020u,
    0x00000014u, 0x00000003u, 0x00000007u, 0x0004003bu, 0x00000014u, 0x00000015u,
    0x00000003u, 0x00040020u, 0x00000017u, 0x00000001u, 0x00000007u, 0x0004003bu,
    0x00000017u, 0x00000018u, 0x00000001u, 0x00050036u, 0x00000002u, 0x00000004u,
    0x00000000u, 0x00000003u, 0x000200f8u, 0x00000005u, 0x0004003bu, 0x00000008u,
    0x00000009u, 0x00000007u, 0x0004003du, 0x0000000bu, 0x0000000eu, 0x0000000du,
    0x0004003du, 0x0000000fu, 0x00000012u, 0x00000011u, 0x00050057u, 0x00000007u,
    0x00000013u, 0x0000000eu, 0x00000012u, 0x0003003eu, 0x00000009u, 0x00000013u,
    0x0004003du, 0x00000007u, 0x00000016u, 0x00000009u, 0x0004003du, 0x00000007u,
    0x00000019u, 0x00000018u, 0x00050085u, 0x00000007u, 0x0000001au, 0x00000016u,
    0x00000019u, 0x0003003eu, 0x00000015u, 0x0000001au, 0x000100fdu, 0x00010038u,
};

std::span<const float> tri_vertex_positions() noexcept
{
    return std::span<const float>(kTriPositions.data(), kTriPositions.size());
}

std::span<const float> tri_fragment_color() noexcept
{
    return std::span<const float>(kTriColor.data(), kTriColor.size());
}

std::span<const std::uint32_t> quad_vertex_spirv() noexcept
{
    return std::span<const std::uint32_t>(kQuadVertexSpirv.data(), kQuadVertexSpirv.size());
}

std::span<const std::uint32_t> quad_fragment_spirv() noexcept
{
    return std::span<const std::uint32_t>(kQuadFragmentSpirv.data(), kQuadFragmentSpirv.size());
}

std::unique_ptr<ShaderModule>
ShaderModule::create(const VulkanLoader& loader,
                     const Device& device,
                     std::span<const std::uint32_t> spirv)
{
    const VkDevice vkDevice = device.device();
    if (!loader.vkCreateShaderModule || !vkDevice)
        return nullptr;
    // A module must have the 5-word header + at least one instruction.
    if (spirv.size() < 5u)
        return nullptr;

    std::unique_ptr<ShaderModule> sm(new ShaderModule(loader, device));

    VkShaderModuleCreateInfo smci{};
    smci.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    smci.codeSize = spirv.size_bytes();
    smci.pCode    = spirv.data();

    if (loader.vkCreateShaderModule(vkDevice, &smci, nullptr, &sm->module_) != VK_SUCCESS)
        return nullptr;
    return sm;
}

ShaderModule::ShaderModule(const VulkanLoader& loader, const Device& device)
    : loader_(&loader)
    , device_(device.device())
{
}

ShaderModule::ShaderModule(ShaderModule&&) noexcept = default;
ShaderModule& ShaderModule::operator=(ShaderModule&&) noexcept = default;

ShaderModule::~ShaderModule()
{
    if (!loader_)
        return;
    if (module_)
        loader_->vkDestroyShaderModule(device_, module_, nullptr);
    module_ = VK_NULL_HANDLE;
}

} // namespace vaml::vulkan
