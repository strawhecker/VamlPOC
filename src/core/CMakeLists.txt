# src/core — vaml-core library (no Vulkan deps; ADR-011 layer rule).
# S001 scope: a buildable library baseline with the version API + a smoke symbol.
# S003: created via vaml_add_library() so the artifact model (VAML_BUILD_SHARED,
# export header, /WX policy, ADR-011 layer tag) applies uniformly. The version
# API is real (non-stub). Property system begins at S030 (Sprint 1).
vaml_add_library(core
    src/version.cpp
    src/Intern.cpp
    src/InternStore.cpp
    src/ObjectPool.cpp
    src/HeapStats.cpp
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
    src/GridLength.cpp    # 11k-3 — parse_grid_length (hoisted public; value-type element)
    src/Transform.cpp
    src/LayoutSlot.cpp
    src/LayoutManager.cpp
    src/ResourceDictionary.cpp    # S360 — ResourceDictionary + merged + three-scope lookup
    src/IconKey.cpp               # S360_1 — IconKey resource entry (ADR-015)
    src/ResourceMarkup.cpp        # S361 — {StaticResource}/{DynamicResource} parse + resolution seams
    src/ResourceResolver.cpp      # S361 — static/dynamic resource resolution over the S360 lookup
    src/Style.cpp                 # S362 — Style + Setter + BasedOn + PropertyTrigger + DataTrigger
    src/StyleApplier.cpp          # S363 — style application to typed elements (setters + triggers)
    src/Brush.cpp                 # S364 — SolidColorBrush (core data brush + Color DP, ADR-011)
    src/ControlTemplate.cpp       # S364/S365 — ControlTemplate + visual states + TemplateBinding
    src/PropertyPath.cpp
    src/BindingDiagnostics.cpp
    src/ObservableObject.cpp
    src/BindingExpression.cpp
    src/BindingMarkup.cpp
    src/RelayCommand.cpp
    src/RoutedEvent.cpp
    src/InputMarshaller.cpp
    src/FocusManager.cpp
    src/TextLayout.cpp          # S400 — TextLayout (pure CPU text layout + wrap engine)
)

# Core must never pull in Vulkan/GLFW (ADR-011). vaml_validate_layer_rules()
# (top-level) fails the build if this target ever links vaml-vulkan.
