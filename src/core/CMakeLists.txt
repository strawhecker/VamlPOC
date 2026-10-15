# src/core — vaml-core library (no Vulkan deps; ADR-011 layer rule).
# S001 scope: a buildable library baseline with the version API + a smoke symbol.
# S003: created via vaml_add_library() so the artifact model (VAML_BUILD_SHARED,
# export header, /WX policy, ADR-011 layer tag) applies uniformly. The version
# API is real (non-stub). Property system begins at S030 (Sprint 1).
vaml_add_library(core
    src/version.cpp
    src/Value.cpp
    src/PropertyFlags.cpp
    src/PropertyKey.cpp
    src/PropertyRegistry.cpp
    src/PropertyMetadata.cpp
    src/DependencyObject.cpp
    src/DependencyProperty.cpp
    src/ChangeRecorder.cpp
    src/ChangeQueue.cpp
    src/ChangeProcessor.cpp
    src/XmlScanner.cpp
    src/ParsedElement.cpp
    src/ElementFactoryRegistry.cpp
    src/Element.cpp
    src/ElementResolver.cpp
    src/UIElement.cpp
    src/VamlReader.cpp
    src/ValueConverter.cpp
    src/Color.cpp
    src/Thickness.cpp
)

# Core must never pull in Vulkan/GLFW (ADR-011). vaml_validate_layer_rules()
# (top-level) fails the build if this target ever links vaml-vulkan.
