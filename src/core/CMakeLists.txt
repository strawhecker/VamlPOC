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
    src/ElementDpFidelity.cpp    # 11k-9 — the DP-FIDELITY CLOSURE (the 11k-8 audit table → code; guarded/idempotent)
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
    src/VectorDrawing.cpp         # 11k-4 — geometry/drawing/vector-brush resources (core data)
    src/ControlTemplate.cpp       # S364/S365 — ControlTemplate + visual states + TemplateBinding
    src/DataTemplate.cpp          # S500 (11f) — DataTemplate + clone_element + load_content
    src/ContentPresenter.cpp      # S500 (11f) — the minimal per-item ContentPresenter host
    src/Scaffolding.cpp           # S550 (11m) — the derived-base + scaffolding surface
    src/TailMechanism.cpp         # S556 (11m) — the service/mechanism + U+ support types (converters + validation + item-container contracts)
    src/PropertyPath.cpp
    src/BindingDiagnostics.cpp
    src/ObservableObject.cpp
    src/BindingExpression.cpp
    src/BindingMarkup.cpp
    src/IValueConverter.cpp            # S501 — IValueConverter + the binding-converter adapter
    src/BooleanToVisibilityConverter.cpp  # S501 — the sole framework bool->Visibility converter
    src/RelayCommand.cpp
    src/AutomationProperties.cpp     # 11n-1 — the AutomationProperties static attached-property class
    src/Freezable.cpp                # 11n-2 — the immutable Freezable base (InputBinding/Animatable base)
    src/RoutedCommand.cpp            # 11n-2 — the routed command (DAG ICommand->RoutedCommand+CommandManager)
    src/CommandManager.cpp           # 11n-2 — the routed-command table + RequerySuggested hub
    src/InputGesture.cpp             # 11n-2 — KeyGesture/MouseGesture
    src/InputBinding.cpp             # 11n-2 — InputBinding(Freezable,ICommandSource)->KeyBinding/MouseBinding
    src/FrameworkTemplate.cpp        # 11n-3 — the shared FrameworkTemplate base
    src/ObservableCollection.cpp     # 11n-4 — the reactive-spine ObservableCollection core
    src/BindingBase.cpp              # 11n-4 — BindingBase/MarkupExtension + Binding/MultiBinding/PriorityBinding recipes
    src/CollectionView.cpp           # 11n-4 — CollectionView (DispatcherObject, ICollectionView) + DECISION-2 change-log/drain
    src/ItemsControl.cpp             # 11n-4 — ItemsControl incremental per-item containers on CollectionChanged
    src/Behavior.cpp                 # S502 (11f) — Behavior base attach/detach
    src/BehaviorCollection.cpp       # S502 (11f) — BehaviorCollection + i:Interaction.Behaviors
    src/TriggerAction.cpp            # S502 (11f) — TriggerAction base
    src/InvokeCommandAction.cpp      # S502 (11f) — InvokeCommandAction : TriggerAction
    src/EventTriggerBehavior.cpp     # S502 (11f) — EventTrigger : Behavior (the i: surface)
    src/RoutedEvent.cpp
    src/Animation.cpp                # S510 (11g) — the animation engine primitives
    src/Storyboard.cpp               # S510 (11g) — the Storyboard container + fused clock
    src/Effect.cpp                   # S511 (11g) — the effects model + pure shadow seams
    src/BeginStoryboard.cpp          # S512a (11g) — the TriggerAction that begins a Storyboard
    src/EventTriggerAction.cpp       # S512a (11g) — LIVE evaluation of the EventTrigger Actions payload
    src/InputMarshaller.cpp
    src/FocusManager.cpp
    src/TextLayout.cpp          # S400 — TextLayout (pure CPU text layout + wrap engine)
    src/Media3D.cpp             # 11n-5 — the 3D value/base spine (Point3D/Matrix3D + Animatable : Freezable + projection)
    src/Geometry3D.cpp          # 11n-5 — Geometry3D + MeshGeometry3D
    src/Model3D.cpp             # 11n-5 — Model3D + GeometryModel3D + Model3DGroup + Material
    src/Visual3D.cpp            # 11n-5 — Visual3D + ModelVisual3D + IVisual3DContainer
    src/Viewport3D.cpp          # 11n-5 — Viewport3D + the Camera family
    src/VisualTreeHelper.cpp    # 11n-6 — the static visual-tree navigation helper
    src/Adorner.cpp             # 11n-6 — the abstract Adorner base (WPF Adorner.cs)
    src/AdornerLayer.cpp        # 11n-6 — the AdornerLayer owner (WPF AdornerLayer.cs)
    src/AdornerDecorator.cpp    # 11n-6 — the AdornerDecorator host (WPF AdornerDecorator.cs)
)

# Core must never pull in Vulkan/GLFW (ADR-011). vaml_validate_layer_rules()
# (top-level) fails the build if this target ever links vaml-vulkan.
