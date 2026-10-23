# test/unit — unit test target(s). S005: first real test harness.
# vaml-core-tests links vaml-core from an external consumer (the S005 acceptance
# surface) and applies the same warnings-as-errors policy as the libraries.
#
# This is an EXECUTABLE, not a library, so the artifact model
# (vaml_add_library / layer tags) does not apply here. doctest is a pure
# 0-feature vendored dependency (VAML_DEPS_DIR/doctest, v2.4.11) — permanent test
# infrastructure, not a STUB / MINIMAL REAL marker.
add_executable(vaml-core-tests
    smoke_test.cpp
    shouldly_test.cpp
    BlockResidentStore_test.cpp
    ObjectPool_test.cpp
    Intern_test.cpp
    InternStore_test.cpp
    Value_test.cpp
    Value_roundtrip_test.cpp
    PropertyKeyRegistry_test.cpp
    PropertyId_test.cpp
    PropertyRegistryExhaustive_test.cpp
    MutexRelocation_rekey_test.cpp
    PropertyMetadata_test.cpp
    PropertyMetadata_semantics_test.cpp
    DependencyObject_test.cpp
    DependencyObject_marshalling_test.cpp
    HeapChurn_test.cpp
    Asan_teardown_test.cpp
    ChangeRecorder_test.cpp
    ChangeQueue_test.cpp
    ChangeNotificationPipeline_test.cpp
    DependencyProperty_test.cpp
    EffectiveValue_test.cpp
    InheritanceCascade_test.cpp
    Coalescing_test.cpp
    AttachedProperty_test.cpp
    AttachedInheritanceNotification_test.cpp
    ReadOnlyAndValidation_test.cpp
    DependencyCallbacks_test.cpp
    StringMaker_test.cpp
    XmlScanner_test.cpp
    XmlNamespace_test.cpp
    XmlDirective_test.cpp
    AttributeValueGrammar_test.cpp
    ParsedElement_test.cpp
    ParsedRoundtrip_test.cpp
    ElementFactoryRegistry_test.cpp
    ElementResolver_test.cpp
    UIElement_test.cpp
    VisualTree_test.cpp
    LayoutSlot_test.cpp
    Measure_test.cpp
    Arrange_test.cpp
    LeafPanelArchetype_test.cpp
    LayoutManager_test.cpp
    MeasureArrangeOverride_test.cpp
    DirtyOnlySubpass_test.cpp
    StackPanel_test.cpp
    StackPanel_stretch_test.cpp
    Grid_test.cpp
    GridSpan_test.cpp
    Grid_stretch_test.cpp
    WrapPanel_test.cpp
    Canvas_test.cpp
    Transform_render_test.cpp
    ValueConverter_test.cpp
    ValueConverterRegistry_test.cpp
    VamlReader_test.cpp
    VamlReader_defer_test.cpp
    VamlReader_s093_test.cpp
    VamlReader_s094_test.cpp
    GoldenDocs_test.cpp
    # S221 (dep:S220) — headless suite over the viewer's load_file seam. Compiles
    # the ACTUAL seam source (vaml_viewer/load_file.cpp) into the test target so
    # the golden-fixture tests exercise the exact code the CLI runs (single
    # source of truth across the apps + the test — ADR-021 consumer path,
    # never a duplicate copy). Deterministic through load_text (pure seam) with
    # a real temp-file I/O case through load_file.
    ViewerLoadFile_test.cpp
    ../../vaml_viewer/load_file.cpp
    # S222_1 (dep:S222) — file-watch LIVE RELOAD headless suites. Compiles the ACTUAL
    # seam sources (vaml_viewer/file_watch.cpp + vaml_viewer/live_reload.cpp) into the
    # test target so the golden-fixture tests exercise the exact code both apps run
    # (single source of truth, ADR-021 consumer). FileWatch_test proves the pure
    # decision table + the real-file lifecycle; LiveReload_test is the GOLDEN proof
    # (edit → reload → same tree) + the reload_action table.
    FileWatch_test.cpp
    LiveReload_test.cpp
    ../../vaml_viewer/file_watch.cpp
    ../../vaml_viewer/live_reload.cpp
)

# The test target is held to the same strict compile bar as the libraries.
vaml_apply_compiler_policy(vaml-core-tests)

# vaml-core public include (for <vaml/version.h>) — strict /WX applies here.
# + deps root marked SYSTEM so vendored doctest is treated as an *external*
# header on MSVC (excluded from /W4 /WX): upstream doctest triggers benign
# C5285 (std::tuple specialization false-positive on new MSVC), which must not
# fail our own strict bar.
target_include_directories(vaml-core-tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/src  # test-only seam: S141 InternStore internal header
    ${CMAKE_CURRENT_SOURCE_DIR}/../../vaml_viewer    # S221: the viewer's load_file.h (the consumer seam's header)
)
target_include_directories(vaml-core-tests SYSTEM PRIVATE
    ${VAML_DEPS_DIR}
)

# On MSVC, keep external (vendored) headers fully quiet so /WX only governs our
# own sources. /external:I is emitted by the SYSTEM dirs above; /external:W0
# makes those headers warning-free by default.
if(MSVC)
    target_compile_options(vaml-core-tests PRIVATE /external:W0)
else()
    # Clang/Linux: fully suppress the `#warning` diagnostic family. vendored
    # doctest includes the compiler-provided <ciso646> ONLY for clang (a libc++
    # detection heuristic) — on GNU libstdc++ that header carries a benign C++20
    # `#warning`. We never use libc++, so the heuristic is meaningless here; the
    # noisy Rust-style diagnostic must not pollute a warning-clean build. ciso646
    # is a system header we cannot patch, so we hide the family for the test
    # target. Every OTHER warning stays an error (strict bar intact).
    target_compile_options(vaml-core-tests PRIVATE "-Wno-#warnings")
endif()

# Link the real library — proves an external consumer can consume vaml-core.
target_link_libraries(vaml-core-tests PRIVATE vaml-core)

# Register with CTest. Per-case discovery via doctest_discover_tests is deferred
# to a follow-up (scoped decision for S005: harness proven first; documented,
# not a silent omission).
add_test(NAME vaml-core-tests COMMAND vaml-core-tests)

# ============================================================================
# vaml-vulkan-tests — S160+ headless tests of the vaml-vulkan surface.
# Links the real vaml-vulkan library (which transitively exposes the vendored
# GLFW include dir via vaml::glfw PUBLIC) so the S160 callback-surface tests can
# reference GLFW constants without their own include setup. Like vaml-core-tests,
# this is an EXECUTABLE (not a library), so the artifact model + ADR-011 layer
# tags do not apply — it links vaml-vulkan as an external consumer would.
# ============================================================================
add_executable(vaml-vulkan-tests
    PlatformWindow_test.cpp
    VulkanInit_test.cpp
    VulkanQueueFamily_test.cpp
    BestGpu_test.cpp
    Swapchain_test.cpp
    ResizeRecreate_test.cpp
    SwapchainGeneration_test.cpp
    MockIcd_test.cpp
    Pipeline_test.cpp
    CommandSync_test.cpp
    Msaa_test.cpp
    Device_test.cpp
    TeardownOrder_test.cpp
    Quad_test.cpp
    QuadScene_test.cpp
    TextureAtlas_test.cpp
    IconPipeline_test.cpp
    # S213_1 (dep:S213) — the DPI-aware icon best-fit SELECTION: the scaling policy
    # { none | down-only | both }, default none, computed once per (IconKey, logical
    # size) against framebufferScale then cached. Headless Rail 1 (select_best_fit /
    # physical_n_for / the cache invalidation contract).
    IconBestFit_test.cpp
    FontAtlas_test.cpp
    SdfAtlas_test.cpp
    UiPipeline_test.cpp
    MonochromeTint_test.cpp
    UiScene_test.cpp
    # S209 — the fence-aware retired-buffer pool lifetime decision (should_free_generation)
    # + the texture-aware batching tests live in UiScene_test.cpp (the S209 batching
    # half) + RetainedBufferPool_test.cpp (the pool-half).
    RetainedBufferPool_test.cpp
    UiSceneEncoder_test.cpp
    UiSceneEncoder_transform_test.cpp
    # S210 — the render-thread loop contract (render_loop_steps, should_reencode) +
    # the retained-scene owner structure. Device-backed record/recycle is [s210-live].
    SceneRenderer_test.cpp
    # S211 — the marshalled-change consumption per frame FIRST WIRE: the lock-free
    # scene-change postbox (coalescing producer / consume-once-per-frame gate /
    # backpressure) + the S211 loop-plan (ConsumeMarshalled + should_consume) + a
    # REAL two-thread handoff (UI mutates, render drains once per frame). Headless.
    MarshalledScene_test.cpp
)

vaml_apply_compiler_policy(vaml-vulkan-tests)

target_include_directories(vaml-vulkan-tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/vulkan/include
)
target_include_directories(vaml-vulkan-tests SYSTEM PRIVATE
    ${VAML_DEPS_DIR}
)

if(MSVC)
    target_compile_options(vaml-vulkan-tests PRIVATE /external:W0)
else()
    target_compile_options(vaml-vulkan-tests PRIVATE "-Wno-#warnings")
endif()

target_link_libraries(vaml-vulkan-tests PRIVATE vaml-vulkan)

add_test(NAME vaml-vulkan-tests COMMAND vaml-vulkan-tests)

# ============================================================================
# vaml-presentation-tests — Sprint 4B (S173-S175) headless tests of the OPTIONAL
# vaml-presentation framework.
# Links the real vaml-presentation library (which PUBLIC-links vaml-vulkan) so the
# Presenter + CompositionEngine pure decision seams (S173 loop/state/recovery +
# S174 viewport/scissor/reserve/clip math) are proven headlessly, mirroring the
# S160+ vaml-vulkan-tests precedent.
# ============================================================================
add_executable(vaml-presentation-tests
    PresentationTestMain.cpp
    Presenter_test.cpp
    Composition_test.cpp
    # S212 (dep:S211) — the MERGE POINT: the first real vertical slice (headless
    # Rail 1) through the vaml::presentation::VamlRenderer host — VAML disk ->
    # VamlReader::Load -> UIElement root -> arrange -> encode -> publish/consume.
    VamlRenderer_vertical_test.cpp
    # S213 (dep:S212_1) — DPI + framebuffer scale: logical px layout, render scales
    # by framebufferScale; high-DPI crisp. The PURE dpi:: seams (framebuffer_scale_for,
    # canvas_projection, dpi_render_plan, logical_to_physical crisp invariant) + the
    # S213_1 dpiScale input (last_framebuffer_scale). Headless, no Vulkan runtime.
    Dpi_test.cpp
    # S222 (dep:S221) — the "soup-to-nuts" viewer render-PLAN seam, headless:
    # the pure S222 wiring contract (viewer_stack_steps = the LIFETIMES build
    # order with VamlRenderer last / viewer_setup_ready = the S173+S200+S206
    # precondition gate / viewer_deadline_elapsed = the auto-close pure seam /
    # ViewerApp move-only RAII). Compiles the ACTUAL shared host source
    # (vaml_viewer/app.cpp) into the test target so the headless suite exercises
    # the exact code both apps run (single source of truth, ADR-021 consumer).
    # The LIVE create()/run_loop() (window + Vulkan stack + create_device) is
    # [both apps, not CI] — the DISPLAY proof, verified live on this box.
    ViewerRenderPlan_test.cpp
    ../../vaml_viewer/app.cpp
    ../../vaml_viewer/load_file.cpp  # S221 seam (live_reload.cpp below calls vaml::load_file)
    # S222_1 (dep:S222) — the viewer LIVE-RELOAD plan seam + the reload coordinator
    # sources. Compiles the ACTUAL shared host sources (app.cpp + the file-watch +
    # live-reload seams) into the presentation test target so the headless suite
    # exercises the exact code both apps run (ADR-021, single source of truth).
    # The goldens (edit → reload → same tree) live in vaml-core-tests.
    ViewerReloadPlan_test.cpp
    ../../vaml_viewer/file_watch.cpp
    ../../vaml_viewer/live_reload.cpp
)

vaml_apply_compiler_policy(vaml-presentation-tests)

target_include_directories(vaml-presentation-tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/presentation/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../vaml_viewer   # S222: the shared host's header (app.h)
)
target_include_directories(vaml-presentation-tests SYSTEM PRIVATE
    ${VAML_DEPS_DIR}
)

if(MSVC)
    target_compile_options(vaml-presentation-tests PRIVATE /external:W0)
else()
    target_compile_options(vaml-presentation-tests PRIVATE "-Wno-#warnings")
endif()

target_link_libraries(vaml-presentation-tests PRIVATE vaml-presentation)

add_test(NAME vaml-presentation-tests COMMAND vaml-presentation-tests)
