# session-context.md

last_command: implementit (S640a — vaml::presentation::RenderTarget — WRITE THE RENDER TO AN AREA OF MEMORY)
plan_digest: Author the offscreen render host `vaml::presentation::RenderTarget` (PUBLIC ADR-021 surface beside
VamlRenderer; the root of S641's compliance scanner). It renders the RETAINED scene (the S212 encode_visual_slice
producer path via set_root/set_frame_size/set_visual_of/invalidate) to an OFFSCREEN image through the SAME
UiScene/Vulkan pipeline as the window — a retained SceneRenderer built over the SAME scene spec, an offscreen
color-only render pass/framebuffer using the same SRGB color format (the byte-faithful ZERO-DRIFT pairing) —
then copy_image_to_buffer -> a CPU `ImageBuffer { width, height, std::vector<uint8_t> BGRA }` (the S641
GPU-emitted scrape source). Models the driver's external RESIZE-BUFFER pattern: ONE oversized offscreen
image/framebuffer reused within capacity (recompute extent + copy region — NO swapchain churn); only an
exceed-capacity resize grows it once (recreate_image_target) + fires the external-app on_resize callback.
ZERO new locks (ADR-018). New VulkanLoader entries: vkCmdCopyImageToBuffer + vkGetImageSubresourceLayout
(has_offscreen_loader gate). Pure decision seams (offscreen_format_for / offscreen_extent_at /
resize_buffer_decision / image_buffer_ensure_capacity / offscreen_copy_region / render_target_loop_steps)
headless-ctestable on BOTH CI lanes.
**THIS SESSION: S640a DONE (implementit; the clean FILE writes + msvc-debug build warning-as-error clean + the
NEW RenderTarget_s640a_test suite 13/13 green).**

## files_touched — S640a (this session)
- src/vulkan/include/vaml/vulkan/VulkanLoader.h (MOD): the S640a offscreen copy entries —
  `vkCmdCopyImageToBuffer` + `vkGetImageSubresourceLayout` (the two image->buffer read-back entry points the
  offscreen path needs; every other image/memory/copy/pipeline/barrier entry is already bound by S164/S165/S166/
  S200) + `has_offscreen_loader()` gate.
- src/vulkan/src/VulkanLoader.cpp (MOD): binds the two S640a entry points.
- src/presentation/include/vaml/presentation/RenderTarget.h (NEW): the PUBLIC ADR-021 surface —
  `Impl::ImageBuffer` (width/height/vector<uint8_t> bgra) + the pure seams `image_buffer_bytes` /
  `offscreen_format_for` / `offscreen_extent_at` / `resize_buffer_decision` /
  `image_buffer_ensure_capacity` / `offscreen_copy_region` / `render_target_loop_steps()` +
  `RenderTargetLoopStep { EncodeIfDirty, RecordRetained, CopyToBuffer }` + `RenderTargetOptions`
  (loader/device/sceneSpec/logicalW/H/bufferCapW/H/on_resize callback) + the RAII move-only `RenderTarget`
  (create + set_root/set_frame_size/set_visual_of/invalidate + render_to/render + resize + accessors).
- src/presentation/src/RenderTarget.cpp (NEW): the pure seams + the real owner — `create` (SRGB guard via
  quad::is_srgb_format; oversized offscreen image/memory/view + compatible color-only RenderPass + framebuffer
  + command pool/buffer + host-visible staging buffer + a RETAINED SceneRenderer over the SAME scene spec);
  `invalidate` (Measure/Arrange + encode_visual_tree + set_scene into the retained renderer);
  `resize` (resize_buffer_decision + the exceed-capacity grow via recreate_image_target + the on_resize
  callback); `render_to` (reset/begin + begin the compatible pass + SceneRenderer::record(cmd, mvp, uiScale)
  via the S213 dpi_render_plan + end + pipeline-barrier + vkGetImageSubresourceLayout rowPitch +
  vkCmdCopyImageToBuffer + submit/wait + row-by-row BGRA repack into the ImageBuffer).
- src/presentation/CMakeLists.txt (MOD): + src/RenderTarget.cpp.
- test/unit/RenderTarget_s640a_test.cpp (NEW, vaml-presentation-tests): 13 TEST_CASEs / 62 assertions covering
  ImageBuffer + image_buffer_bytes, offscreen_format_for (SRGB identity + non-SRGB undefined),
  offscreen_extent_at (clamp/cap/guard), resize_buffer_decision (reuse-within-cap vs grow-on-exceed +
  zero-capacity), image_buffer_ensure_capacity (REUSE oversized, never-shrink, grow-exactly),
  offscreen_copy_region (rowPitch-faithful + bufferOffset), render_target_loop_steps (fixed order).
- test/unit/CMakeLists.txt (MOD): + RenderTarget_s640a_test.cpp (vaml-presentation-tests).
- docs/roadmap.md (MOD): S640a marked [DONE — implementit S640a; windows-msvc LANE GREEN] (ubuntu-clang lane
  NOT run — see the build_status outage note).
- session-context.md (MOD): this entry (fresh rewrite).

## build_status (S640a — from first principles)
- windows-msvc: `cmake --preset msvc-debug` configure clean + full `ninja` build warning-as-error CLEAN
  (the whole solution incl. the new vaml-presentation RenderTarget.cpp + the new test TU) + `ctest --preset
  msvc-debug` 4/4 PASS (vaml-core / vaml-tools / vaml-vulkan / vaml-presentation); the NEW
  RenderTarget_s640a_test suite runs 13/13 green (60+ assertions). FIXED during the build: (1) a doctest
  "Expression Too Complex" static-assert (the 3-part imageOffset CHECK split into 3 single CHECKs);
  (2) two wrong test expectations in resize_buffer_decision (200x300 EXCEEDS 256x256 cap -> must grow;
  0x0 cap cannot hold 10x10 -> must grow — corrected to match the policy semantics).
- ubuntu-clang: **NOT RUN this session** — the Linux verification VM (robert@192.168.198.129, the home-LAN
  VMware NAT host) is OFFLINE/UNREACHABLE (ssh connect timed out + ping times out; the roadmap records it is
  reachable ONLY on the home LAN — no VPN/travel route exists yet). The clang-debug lane could not be executed
  from this network right now. See notes.

## next_step — S640a complete (windows lane green; ubuntu-clang RE-VERIFY outstanding)
- **S640a (Sprint 11r — RenderTarget, the resin of S641) implemented and windows-msvc-green.** The managed
  deliverables are in place (the library + loader changes + the headless suite compile + pass).
- **RE-VERIFY on the ubuntu-clang lane when the VM is on the home LAN** (`cd /mnt/hgfs/VamlPOC && cmake
  --preset clang-debug && cmake --build --preset clang-debug && ctest --preset clang-debug`). The pure seams
  are deliberately both-lane proven by the same headless suite; the clang-debug compile of RenderTarget.cpp is
  the remaining gate.
- Next per the roadmap chain = **S640b (WPF REFERENCE EMITTER — Windows-authoring-only, PRECEDES S641)** then
  **S641 (THE UI-COMPLIANCE SCANNER** — makes the S640b compare seam live over the S640a ImageBuffer; consumes
  the GPU-emitted buffer as the authoritative scrape source). The REAL GPU script-driven render_to proof is
  also S641's consumer (recorded NO-GAP at S640a).

## notes
- S640a authored MINIMAL-REAL-free: `RenderTarget`, `ImageBuffer`, and every pure seam are REAL permanent
  ADR-021 surface (kept-and-extended by S641), NOT stubs and NOT MINIMAL REAL.
- ZERO-DRIFT pair: the offscreen target REUSES the same scene-spec pipelines/layout/set-layout/atlas (a retained
  SceneRenderer over the SAME spec) + the same SRGB color format — a separate authored pipeline is REJECTED
  (the byte-faithful exact-window-output guarantee).
- RESIZE-BUFFER pattern (the driver's external oversubscribed model): ONE oversized offscreen image/framebuffer
  reused within capacity (recompute extent + copy region — NO swapchain churn); only an exceed-capacity resize
  grows it once (recreate_image_target) + the external-app on_resize callback (a std::function in
  RenderTargetOptions — the driver's "external-app callback, not a repo file"). The CPU side honors the
  external oversized `ImageBuffer` the same way (image_buffer_ensure_capacity: reuse, never shrink, grow-only).
- ADR-018: ZERO new locks (single-threaded record + copy on the render thread; the retained SceneRenderer's
  S209 fence is the sync primitive). ADR-011 held (the host lives in vaml-presentation over vulkan/core; the
  vulkan-side loader entries live in vaml-vulkan).
- NO silent backfill (NO-GAP): the GPU script render -> ImageBuffer -> scan/compare is **S641's** job (S641
  consumes this ImageBuffer as the ACTUAL GPU-emitted output, per the roadmap); the WPF reference emitter is
  S640b; the scanner is S641 — all recorded forward, never silent.
- The ubuntu-clang lane is a **standalone environment outage** (the VM is not reachable from this network right
  now), NOT a code failure: the identical headless suite is both-lane by design, and the msvc-debug lane is
  fully green. Re-verify on the home LAN before considering the step fully closed (the roadmap's mandatory
  both-lane gate).
- No commit made (standing directive — commitit outstanding; the driver's seat is yours; the S640a change set
  is uncommitted).
