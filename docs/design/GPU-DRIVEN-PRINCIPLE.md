# VAML Design — GPU-Driven, Retained-Data Rendering Principle  (STRATEGIC / governs all renderer work)

> Status: ADOPTED (codified before S001). This is a TOP-LEVEL design principle, not a feature.
> Every rendering decision (Sprints 4, 5, 14, and any Vulkan work) MUST adhere to it.
> If a step conflicts with this doc, that step is wrong — update the step, not this doc.
> Cross-refs: docs/adr/ADR-LOG.md (ADR-012), docs/architecture.md (§5), docs/design/DESIGN-SEEDS.md.

## 0. Origin / motivation
The author started at a company that rendered UI "the old way": start with a blank area and, at 50/60 Hz,
re-rasterize / re-issue the whole scene with CPU routines filling every element every frame — the UI
thread stayed busy doing driver calls and state churn. The author's successful correction was the
inverse: **put everything in the GPU; content changes flow TO the GPU; the CPU/UI thread stays nearly idle
because the GPU owns the heavy lifting.** VAML inherits that correction as its non-negotiable philosophy.

## 1. The principle (the one sentence everything must serve)
**The GPU owns the scene; the CPU owns only the small, explicit set of changes that move toward it.**
- Geometry, textures, fonts, pipelines are **retained** in GPU/VRAM — built once, reused every frame.
- Per-frame CPU work is a minimal change stream pushed to the GPU, not a re-description of all content.
- The UI/render-path threads stay **calm**; utilization is amortized, bursty only on real change.
- The GPU — not hidden driver state — authoritatively holds render state; in the full form it even decides
  how much to draw (indirect/draw-list).

## 2. Why Vulkan is the engine for it (honest comparison with OpenGL)
Background: OpenGL/GLES. The philosophy transfers; the mechanics differ.

Carries over (same mental model, more explicit): retained resources; keep CPU calls low; GPU holds state.

Vulkan's real wins over OpenGL:
1. **Real multi-threaded command recording** — concurrent per-thread command buffers. OpenGL's single-context
   affinity makes this painful. A structural advantage for a UI framework feeding a calm render thread.
2. **Explicit, reusable command buffers** — record once, replay; no per-frame re-issue. Retained UI = record once.
3. **GPU-driven rendering** — vkCmdDrawIndirect/IndexedIndirect + persistent descriptor sets
   (VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND, UPDATE_AFTER_BIND) let the GPU read its own count. "GPU does it all"
   at its legitimate extreme.
4. **Async queues + timeline semaphores** — graphics/compute/transfer overlap (transfer uploads while graphics
   renders).

The honest costs (what "doing it right" costs):
- **Ceremony/boilerplate.** Instance→device→swapchain→render pass→framebuffers→pipelines→descriptors→command
  pools→sync, before any pixel. One-time; must be contained (§4).
- **Explicit synchronization.** Manually emitted layout transitions + buffer barriers. Hides errors; new discipline.
- **Explicit memory.** Device/local residency, staging buffers, lifetime. We decide what lives in VRAM.
- **Pipelines are compiled objects.** Per-element churn is expensive/fragile. UI's saving grace: a small fixed
  set (§5).

Conclusion: for a UI framework the trade is favorable — small fixed pipeline set + retained scene is Vulkan's
strength, and its ceremony amortizes to near-zero per frame.

## 3. What "doing it right" means (requirements the renderer MUST satisfy)
- R1 **Retained-first.** Every GPU object created once, reused. No per-frame re-allocation of core resources.
  Rebuild only on structural change (resize, new control).
- R2 **Change-driven, not full-redraw.** A frame's CPU work is proportional to what changed, not total content.
  Unchanged content costs ~nothing on the CPU path.
- R3 **UI/render threads stay calm.** Steady state = consume change stream, record/replay, submit. NOT
  "rasterize everything from scratch at 60 Hz." UI thread idle until real change.
- R4 **Batching by material / small PSO set.** Draws grouped by material+texture+state across a small fixed
  pipeline set. Keeps Vulkan cheap and stable.
- R5 **Explicit, correct synchronization.** Every transition/barrier deliberate, verified by validation layers.
  No hidden driver sync.
- R6 **Explicit, correct lifetime/residency.** Staging→device-local upload; retired buffers freed only after
  the GPU fence passes. No leaks, no use-after-submit, clean exit (S168, S726).
- R7 **Async queues where they pay.** Transfer/compute asynchrony to overlap upload/processing with rendering
  (S14+). Never block the render thread on a hot-path copy.
- R8 **Full form = GPU-driven draw dispatch.** Target (S14/S622): indirect draw lists / persistent descriptors
  where the GPU reads its own count; CPU uploads a tiny change-buffer, GPU does the rest. Until then, the CPU
  encoder (S207) groups the same way so the step is a drop-in.
- R9 **Resize/DPI correctness without re-rasterizing logic.** Swapchain recreation + framebuffer scale touches
  only the render backend, not the retained scene/layout (S213).

## 4. Containment rule (boilerplate lives in one place)
All Vulkan ceremony wrapped in RAII ownership classes in `src/vulkan` (S168). No raw `vk*` owned at call sites
outside device.cpp/backend. Written once, correct, out of scene/layout code. CMake enforces layer rule
(core has zero Vulkan deps — ADR-011).

## 5. The fixed material/pipeline set (design constant)
Small fixed set so GPU-driven batching is cheap and stable:
- SDF text (soft-edge glyphs from the atlas)
- solid color fill (quads/rects)
- border / stroke / rounded rect
(Backgrounds/images via the same quad path using the texture atlas.) New materials require deliberate, explicit
addition — never ad-hoc pipeline creation per element.

## 6. How the sprint plan already serves it (mapping)
- Sprint 4 (S160–S171): ceremony contained in RAII (S168), sync explicit, validation debug-only (S162); proves
  the explicit model.
- Sprint 5 (S200–S215): retained scene → encoder groups by material (R4, S207); batching (S209); DPI framebuffer
  scale (R9, S213); first wire of retained UI (S212). Realizes R1–R6.
- Sprint 14 (S620–S629): async layout (S620), GPU-driven/indirect (S622–S623), per-frame arena (S624), zero-copy
  upload + caches (S625), multi UI threads + copy-on-write (S626), perf gates (S627–S628). Realizes R7–R8.

## 7. Test gates the principle requires
- Validation layers clean in debug every frame (S165/S214/S726).
- No per-frame resource allocation in steady state (profiled, S624/S628).
- Draw-call count near-constant under growing element count (S623).
- On-exit teardown zero validation errors / no leaks (S168/S726).
- Offscreen pixel-snapshot correctness tests for the renderer (S528).

## 8. Anti-patterns this doc forbids (the "old way")
- ❌ Blank-canvas-then-fill-everything at 50/60 Hz on the CPU path.
- ❌ Re-issuing unchanged content each frame (no retained state).
- ❌ Per-element pipeline / state churn.
- ❌ Blocking memory copies on the render thread in the hot path.
- ❌ Relying on hidden driver synchronization or state caching to stay correct.
