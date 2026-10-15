# test/unit — unit test target(s). S005: first real test harness.
# vaml-core-tests links vaml-core from an external consumer (the S005 acceptance
# surface) and applies the same warnings-as-errors policy as the libraries.
#
# This is an EXECUTABLE, not a library, so the artifact model
# (vaml_add_library / layer tags) does not apply here. doctest is a pure
# 0-feature vendored dependency (third_party/doctest, v2.4.11) — permanent test
# infrastructure, not a STUB / MINIMAL REAL marker.
add_executable(vaml-core-tests
    smoke_test.cpp
    shouldly_test.cpp
    Value_test.cpp
    Value_roundtrip_test.cpp
    PropertyKeyRegistry_test.cpp
    PropertyId_test.cpp
    PropertyRegistryExhaustive_test.cpp
    PropertyMetadata_test.cpp
    PropertyMetadata_semantics_test.cpp
    DependencyObject_test.cpp
    DependencyObject_marshalling_test.cpp
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
# + third_party marked SYSTEM so vendored doctest is treated as an *external*
# header on MSVC (excluded from /W4 /WX): upstream doctest triggers benign
# C5285 (std::tuple specialization false-positive on new MSVC), which must not
# fail our own strict bar.
target_include_directories(vaml-core-tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/include
)
target_include_directories(vaml-core-tests SYSTEM PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party
)

# On MSVC, keep external (vendored) headers fully quiet so /WX only governs our
# own sources. /external:I is emitted by the SYSTEM dirs above; /external:W0
# makes those headers warning-free by default.
if(MSVC)
    target_compile_options(vaml-core-tests PRIVATE /external:W0)
else()
    # Clang/Linux: third_party is already on a -isystem path (silences normal
    # warnings from system headers), but `#warning` directives are ALWAYS
    # reported even from system headers — upstream doctest includes the
    # compiler-provided <ciso646> (which carries a benign C++20 `#warning`) for
    # libc++ detection. We use libstdc++, so that diagnostic is irrelevant; on
    # C++20 clang -Werror escalates it. Downgrade only this one diagnostic
    # family for the test target; every other warning stays an error.
    target_compile_options(vaml-core-tests PRIVATE "-Wno-error=#warnings")
endif()

# Link the real library — proves an external consumer can consume vaml-core.
target_link_libraries(vaml-core-tests PRIVATE vaml-core)

# Register with CTest. Per-case discovery via doctest_discover_tests is deferred
# to a follow-up (scoped decision for S005: harness proven first; documented,
# not a silent omission).
add_test(NAME vaml-core-tests COMMAND vaml-core-tests)
