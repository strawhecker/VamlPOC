# src/presentation — vaml-presentation library (Sprint 4B S173-S175 OPTIONAL framework).
#
# The OPTIONAL presentation/composition framework ABOVE the VAML primitives. The
# library (vaml-core/vaml-vulkan) stays a NEUTRAL toolkit; the COMPOSITION policy
# lives ONLY here, so a user can DIY their own on the library tools OR adopt the
# framework ("with-or-without-framework" is preserved — not linking vaml-presentation
# = the primitive-only consumer).
#
# Layer: vaml-presentation is a VULKAN-layer target (it wraps vaml-vulkan: S167_1 +
# S166 + S165 + the S169 Device), but it is deliberately NOT appended to
# VAML_CORE_LIBRARIES by the artifact model — so ADR-011's core-link check never
# trips on it and the framework stays optional. It links vaml-vulkan PUBLIC (its
# includes/type surface) so consuming headers (Presenter.h / CompositionEngine.h)
# can reference the vaml::vulkan types.
#
# vaml_add_library() creates the vaml-presentation target (with its generated export
# header vaml/presentation/export.h). The libraries MUST be built via the artifact
# model — see cmake/artifact-model.cmake.
vaml_add_library(presentation
    src/Presenter.cpp
    src/CompositionEngine.cpp
    # S212 (dep:S211) — the MERGE-POINT vertical-slice render host: composes the
    # S207/S208 scene encoder + the S211 postbox + the S210 retained SceneRenderer
    # + the S173 Presenter behind ONE public surface ("UI declared in VAML renders
    # to window via the full S5 path"). The host owns the CPU/merge logic headlessly
    # (create) + the device-backed integration (create_device, [both apps, not CI]).
    src/VamlRenderer.cpp
    # S640a (dep:S640) — the offscreen render host: renders the RETAINED scene to an
    # OFFSCREEN image via the SAME UiScene/Vulkan pipeline as the window (the SRGB
    # byte-faithful zero-drift pairing), then copy_image_to_buffer -> a CPU ImageBuffer
    # (the S641 compliance scanner's authoritative GPU-emitted scrape source). The pure
    # seams (offscreen_format_for / offscreen_extent_at / resize_buffer_decision /
    # image_buffer_ensure_capacity / offscreen_copy_region / render_target_loop_steps)
    # are headless-ctestable; the real owner is [both apps, not CI] (S641's script lane).
    src/RenderTarget.cpp
    # S213 (dep:S212_1): the DPI + framebuffer-scale pure seams (logical-px layout,
    # render scales by framebufferScale; high-DPI crisp). The host (VamlRenderer)
    # consumes dpi_render_plan in record_draw to call the S206 SceneRenderer's
    # 3-arg record with the derived uiScale (the set_frame_size uiScale consumer).
    src/dpi.cpp
    # S592 (dep:S591): PER-WINDOW CLEAR ALPHA + BACKGROUND COMPOSITION — the PURE
    # clear-decision seam (the 2-branch WPF HwndTarget.SetBackgroundColor mirror).
    # `clear::decide_clear(allowsTransparency, resolvedBackground) -> ClearValue`.
    src/clear.cpp
    # S641 (dep:S640b) — THE UI-COMPLIANCE SCANNER (the root capability the whole
    # sprint rides on; lives in the dedicated tester — the vaml::presentation::scan
    # namespace). Authors the compare seam (compliance_compare, the tolerance
    # scaffold, the in-XAML SCAN DECLARATION parser) + makes it LIVE: scrape the
    # rendered ImageBuffer + the arranged core tree -> ScanReport -> per-datum
    # PASS/FAIL -> colored diff PNG. PURE + headless (both CI lanes): the four
    # scan TUs are decision-layer only, no Vulkan runtime; the REAL GPU render that
    # FILLS the buffer is S640a's RenderTarget [both apps, not CI].
    src/scan/ScanDeclaration.cpp   # in-XAML SCAN DECLARATION parser
    src/scan/Scanner.cpp           # ScrapeGrid + compliance_compare + ScanReport
    src/scan/EmitterMetadata.cpp   # forward-compatible sidecar JSON reader
    src/scan/PixelDiff.cpp         # GIMP-style diff + tolerance band + PNG write
    src/version.cpp
)

# Only vaml-presentation links the framework onto the primitives; it NEVER joins the
# CORE aggregation (ADR-011 stays intact — the framework is Vulkan-layer, optional).
target_link_libraries(vaml-presentation
    PUBLIC vaml-vulkan
)
