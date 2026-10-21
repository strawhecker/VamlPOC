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
    ValueConverter_test.cpp
    ValueConverterRegistry_test.cpp
    VamlReader_test.cpp
    VamlReader_defer_test.cpp
    VamlReader_s093_test.cpp
    VamlReader_s094_test.cpp
    GoldenDocs_test.cpp
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
    FontAtlas_test.cpp
    SdfAtlas_test.cpp
    UiPipeline_test.cpp
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
)

vaml_apply_compiler_policy(vaml-presentation-tests)

target_include_directories(vaml-presentation-tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/presentation/include
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
