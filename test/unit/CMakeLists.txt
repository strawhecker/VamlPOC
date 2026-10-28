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
    # S452 (dep:S451) — PROPERTY-ELEMENT + ATTRIBUTE-ELEMENT authoring grammar
    # (the S366 NO-GAP hand-off). The S2 text reader accepts
    # `<X.Content><StackPanel/></X.Content>` and `<X.Background>Literal</...>` —
    # content into the named DP via the S043 attached-aware path, NOT a visual
    # child. Pure dotted-name helpers (is_property_element /
    # split_property_element) + object-value set as UntypedPtr + literal
    # content convert + empty skip + unknown-dotted -> the structured
    # UnknownProperty reader error, + nested property-elements.
    PropertyElement_s452_test.cpp
    # S453 (dep:S452) — RESOURCE-DICTIONARY AUTHORING + x:Key (the S366 NO-GAP
    # hand-off RESOLVED). A `<X.Resources>` property-element routes an OWNED
    # ResourceDictionary into the S360 side-table; authored children carrying
    # x:Key are built + captured as S360 Object resources (the S361 x:Key
    # deferral resolved); MergedDictionaries merges (self-wins + sibling-last-
    # wins); {StaticResource}/{DynamicResource} resolve over the authored dict
    # via find_for_element; duplicate x:Key -> ResourceAlreadyRegistered;
    # missing authored key -> MissingKey (Class A); an authored resource with no
    # x:Key in a dictionary -> structured reader error.
    ResourceAuthoring_s453_test.cpp
    # S454 (dep:S453) — STYLE AUTHORING + TRIGGERS (the S362/S363 engine AUTHORED
    # from .vaml). <Style TargetType BasedOn> + <Setter Property Value> (literal /
    # Static/Dynamic/TemplateBinding SetterValues) + <Style.Triggers>
    # (PropertyTrigger/DataTrigger with child setters) authored into the live S362
    # model; BasedOn resolved at end-of-Load; explicit-vs-implicit application
    # (implicit <Style> with no x:Key -> registered under "Style:{TargetType}"
    # + applied via apply_implicit_style; an explicit x:Key'd style does NOT
    # auto-apply).
    StyleAuthoring_s454_test.cpp
    # S360 (dep:S329) — ResourceDictionary + merged dictionaries + three-scope
    # lookup (tree + app + system). The Sprint-9 resource foundation: the
    # OWNED ResourceValue model (Object/Plain/Deferred), interned-keyed ordered
    # store with dedupe-throws-unchanged, WPF merge precedence (self-wins +
    # sibling-last-wins), get() -> shared ResourceLookupException(MissingKey),
    # the find_for_element tree->...->app->system walk with shadowing at each
    # scope, and the UIElement.resources() slot.
    ResourceDictionary_test.cpp
    # S360_1 (dep:S360) — IconKey resource entry (ADR-015): a strong content-
    # equal key a ViewModel exposes (never a path/SVG/render object), resolved
    # through the ResourceDictionary. resolve_icon reuses the shared S360
    # ResourceLookupError::MissingKey (decision 3 — NOT duplicated; S361 reuses
    # the same error); a Plain/non-Object entry -> MissingKey; the S360 tree
    # -> app walk with element-scope shadowing.
    IconKey_test.cpp
    # S361 (dep:S360) — StaticResource/DynamicResource resolution; merge
    # precedence; missing-key errors. RESOLVES the S092 x:Key and the S252/S253
    # MarkupExtensionKind::None deferrals. The parse_resource_markup seams
    # ({StaticResource X}/{DynamicResource X}; non-resource -> recognized=false;
    # empty-key -> malformed), the ResourceResolver static path (applied once,
    # missing -> the SHARED decision-3 ResourceLookupException(MissingKey)), the
    # merged-dictionary precedence through markup, and the dynamic path (applies
    # now + records the link; reapply_dynamic_resources re-pushes on a source
    # change; NEVER throws on a transiently-missing key — holds).
    ResourceResolution_test.cpp
    # S362 (dep:S361) — Style + Setter + BasedOn + PropertyTrigger + DataTrigger:
    # the WPF-faithful style DATA MODEL + seams. setter_value_kind discrimination,
    # Style target_type + ordered setters, BasedOn pointer wiring, trigger kind
    # discrimination (Property vs Data with the S240 PropertyPath on DataTrigger),
    # style_valid gate.
    Style_test.cpp
    # S363 (dep:S362) — Style applied to type: explicit setters land on DPs,
    # BasedOn chain (base then child-overrides), local-wins (authored local not
    # clobbered), PropertyTrigger fires/un-fires on DP-equality, DataTrigger fires
    # through the S240 path + dependency-object reader, and implicit style by
    # target type (find_implicit_style + apply_implicit_style from the tree/app
    # scope).
    StyleApply_test.cpp
    # S364 (dep:S363) — ControlTemplate + visual states: template record + owned
    # content, instantiate -> live tree, apply_control_template, enter_state/
    # exit_state brush swap (SolidColorBrush Color lands on the target brush DP),
    # find_state, target-type checking. SolidColorBrush is core data (ADR-011).
    ControlTemplate_test.cpp
    # S365 (dep:S364) — TemplateBinding to control props; templatable Button
    # restyle: the {TemplateBinding prop} read of the control DP, control-DP
    # change observed (re-skin), the templated Button restyle + visual-state
    # attach. Real Button remains its own S401 (mechanism proven here).
    TemplateBinding_test.cpp
    # S240 (dep:S226) — PropertyPath parser + evaluator (dot-path; indexers;
    # null-safe; errors reported). The Class A syntax suite (valid dot/indexer/
    # mixed paths + every syntax error as a typed PropertyPathException naming
    # the offending fragment + try_parse never-throws) + the Class B evaluator
    # suite (test-local indexable host for the indexer axis, real DependencyObjects
    # via the default reader for the property/null-safe/missing/not-readable
    # axis). Registers S240-unique property names.
    PropertyPath_test.cpp
    # S240_1 (dep:S240) — BindingDiagnostics: the record + IBindingDiagnostics
    # sink. Class A/B tagging authority (binding_class_of, exhaustive over all
    # 11 reason codes), the two factories (diagnostic_from_property_read maps
    # MissingProperty/NullIntermediate/NotReadable -> Class B; the degenerate
    # empty-path -> EmptyPath Class A; diagnostic_from_path_error maps a
    # PropertyPathError -> MalformedPath/EmptyPath Class A), the NO-OP default
    # sink (emit is a safe no-op) and the opt-in/recurrent set_binding_diagnostics
    # (a recording sink receives emission through binding_diagnostics().emit;
    # set returns the previous sink; restore default -> no-op again). Pure
    # value-domain — registers no property names.
    BindingDiagnostics_test.cpp
    # S241 (dep:S240) — ObservableObject: Set() + typed PropertyChanged signal
    # (INotifyPropertyChanged-analogue). Set/Get round-trip (String/Int32/Bool),
    # real-change-only firing (same value -> no signal), broadcast vs per-property
    # subscriptions + registration order + remove/re-add, has() distinguishing
    # null-valued-present from absent, and the S240 observable_object_reader()
    # through evaluate_property_path (present resolves, final-null ok=true,
    # absent -> MissingProperty, non-final-null -> NullIntermediate, Index ->
    # NotReadable, non-ObservableObject root -> NotReadable). ADR-018: zero locks.
    ObservableObject_test.cpp
    # S242 (dep:S241) — BindingExpression: source/target, mode one/one-time/two-way,
    # converter, fallback value, and the first transfer. Registers S242-unique
    # object-scoped DPs on real DependencyObject targets + reads real
    # ObservableObject sources through the S241 reader + S240 evaluator. Covers
    # OneTime initial APPLIED transfer + resolve-once (no live re-push — that is
    # S243/S244), mode()/to_binding_direction, Class B fallback on MissingProperty/
    # NullIntermediate/NotReadable, no-fallback -> ResolveFailed with target
    # untouched, converter String->Int32 APPLIED, converter failure -> ConverterFailed
    # + fallback -> FallbackApplied, Class A malformed/empty path -> create nullptr
    # (emitted, NO fallback applied), NoChange, wrong-typed -> TargetRejected (the
    # SetTargetFailed emission is S243's — NOT emitted here), monotonic bindingId,
    # and exact reason codes per arm via a recording sink. NO-GAP forward contracts
    # to S243/S244 (live wires) + S242_1 (Strict) recorded, not silent.
    BindingExpression_test.cpp
    # S242_1 (dep:S242) — BindingPolicy { Strict | Fallback } on BindingExpression.
    # Strict promotes a Class A path error to a BIND-TIME THROW (BindingPolicyException,
    # emit-then-throw per ADR-016); Class B NEVER throws under either policy (fallback +
    # diagnostic only). Registers S242_1-unique object-scoped DPs. Covers the default
    # Fallback policy (S242 parity: malformed path -> create nullptr, emitted, no throw),
    # Strict + malformed path -> throws BindingPolicyException with matching kind() + path
    # while the MalformedPath diagnostic is STILL emitted (observation is never the control
    # flow), Strict + empty/whitespace -> EmptyPath-kinded throw, policy() carried through
    # create, the ADR-016 headline (Class B MissingProperty/NullIntermediate/NotReadable/
    # ConverterFailed NEVER throw under Strict — TransferResult + exact reason codes),
    # and Strict + valid path -> normal Applied transfer.
    BindingPolicy_test.cpp
    # S243 (dep:S242) — BindingExpression live ONE-WAY. The live one-way source→target
    # subscription (source ObservableObject per-property subscription keyed on the FIRST
    # PATH SEGMENT, armed at create for OneWay; on a real source change the exact S242
    # pipeline re-runs and re-pushes) + the S240_1 SetTargetFailed FORWARD ARM (a live
    # one-way re-push whose target Set is rejected emits BindingReasonCode::SetTargetFailed
    # once, Class B observation never control-flow per ADR-016; the caller-driven first
    # push and the OneTime path emit NOTHING — S242 parity preserved) + DP invalidation
    # COALESCING proven from the real-change discipline (S241 source Set fires only on a
    # real change; S035 target Set returns NO_CHANGE on an equal push): an interleaved
    # A,A,B,B,C burst yields EXACTLY 3 target writes. Registers S243-unique object-scoped
    # DPs; counts target changed-callbacks + subscription-invocation effects. Covers OneWay
    # initial APPLIED push, LIVE auto-re-push, distinct burst 1->2->3, source-side + 
    # target-side coalescing, mode introspection (armed only for OneWay; OneTime none),
    # OneTime unchanged (no auto-update), converter re-run live, fallback + live restore,
    # SetTargetFailed emission (live only), SetTargetFailed != fallback (a target rejection
    # is never masked by a bound fallback), lifetime/dtor unsubscribe, and move re-arm.
    BindingOneWay_test.cpp
    # S244 (dep:S243) — BindingExpression live TWO-WAY (feed-back: target→source).
    # The TARGET→SOURCE WRITE-BACK — a TwoWay binding arms a per-property
    # subscription on the TARGET DependencyObject (S041 add_property_changed keyed
    # on targetProp_; the "target changed" signal = a TextBox edit landing on the
    # target DP) and, on a real target change, writes the target's value BACK to
    # the source VM (the "TextBox edit → VM updates" headline). The FORWARD wire is
    # the EXACT S243 shared layer now armed for TwoWay too. RESOLVES S242's
    # convertBack seam (reverse converter wire target→source) + binds the S240_1
    # WriteBackRejected FORWARD ARM (a live two-way write-back that is rejected —
    # failed convertBack / no source / no source key — emits
    # BindingReasonCode::WriteBackRejected, Class B, never a throw). LOOP-SAFETY by
    # the real-change discipline (a source→target→write-back→source cycle is
    # BOUNDED, no reentrancy flags — NO-LAZY-LLM). Registers S244-unique object-scoped
    # DPs on real DependencyObject targets + real ObservableObject sources. Covers
    # initial push APPLIED + both wires armed, live forward (S243 reuse), the
    # HEADLINE target-edit→source, convertBack (forward String→Int32 + target edit
    # convertBack'd→source), bounded echo/cycle, stability (edit to equal source:
    # no re-trigger), WriteBackRejected (failed convertBack / no source), WriteBackRejected
    # != SetTargetFailed (never conflated), OneWay parity (forward only),
    # OneTime inert, no-converter round-trip, lifetime/dtor, and move re-arm (both
    # wires).
    BindingTwoWay_test.cpp
    # S245 (dep:S244) — BindingExpression EXPRESSION LIFECYCLE: detach on element
    # destroy. The target DependencyObject's destruction notification (S245
    # DependencyObject::add_destroyed) auto-DETACHES the binding before the
    # element's memory is reclaimed; a detached binding writes NOTHING (a later
    # source/target change emits BindingReasonCode::Detached — the S240_1
    # forward arm, Class B, a SILENCED non-error — exactly once, never a
    # dangling-target write); explicit Detach() is idempotent/no-throw/no-emission
    # + unsubscribes both wires; OneWay/OneTime parity; Detached is Class B;
    # move re-arms the destruction wire; dtor leaves no dangling callback.
    # Registers S245-unique object-scoped DPs on real DependencyObject targets +
    # real ObservableObject sources. NO-GAP: resolves the S240_1 Detached forward
    # arm + the S243/S244 "S245 follows" hand-offs in place; S252 ({Binding}
    # markup) builds on this.
    BindingLifecycle_test.cpp
    # S245_1 (dep:S245) — BINDING-FAILURE test suite: the harness SUBSCRIBES to
    # the sink and asserts the EXACT reasonCode per Class (not just fallback
    # values); detach/lifecycle (S245) is a Class-B-tagged, silenced non-error;
    # Diagnostics ≠ control-flow (the subscriber gets info, never control).
    # TEST-ONLY consolidation (the S040/S042/S044/S125/S128/S130/S133/S135/S137
    # precedent): exercises EVERY failure arm of the real binding engine through
    # the REAL BindingExpression on the REAL S241 reader + S240 evaluator + S240_1
    # sink, asserted by exact reasonCode sequences recorded by a subscribing
    # RecordingSink. Covers Class A malformed/empty -> create nullptr + exactly
    # [MalformedPath]/[EmptyPath] (StaticAuthoring, no fallback), Class A Strict
    # (emit-then-throw, emit never the control flow), Class B MissingProperty with/
    # without fallback -> exactly [MissingProperty] ([FallbackApplied]), Class B
    # NullIntermediate/NotReadable/ConverterFailed (+ fallback) -> exact codes,
    # Class B live OneWay SetTargetFailed (first push silent, S242/S243 parity),
    # Class B live TwoWay WriteBackRejected (source untouched), HEADLINE detach/
    # lifecycle -> [Detached] once + Class B + never throws, and the
    # diagnostics≠control-flow proof (NO-OP vs recording sink -> identical
    # TransferResult + target). ADR-016 + ADR-018 honored. NO-GAP: no new
    # forward contracts (S246 RelayCommand is the next chain link).
    BindingFailure_test.cpp
    # S247 (dep:S246) — RelayCommand TESTS. Verifies the S247 title against the
    # REAL vaml::RelayCommand through the REAL vaml::ICommand interface where it
    # matters: invoke (Execute), CanExecute, CanExecuteChanged — full, no stub.
    # Covers the execute-only always-executable default, a predicate governing
    # CanExecute per parameter, Execute invoking the action with the exact
    # parameter Value, subscribe -> NotifyCanExecuteChanged fires once, multiple
    # subscribers fire in REGISTRATION order, remove removes one + the remainder
    # fire + re-add appends, distinct monotonic tokens, zero-subscriber safe
    # no-op, side-effect-free CanExecute (fires nothing), a Value::Null()
    # parameter, the MOVE-ONLY transfer contract (post-move fires, moved-from
    # inert, move-assign re-arms), and the re-query contract (a captured flag
    # flips CanExecute + a subscribed handler observes via NotifyCanExecuteChanged).
    # Pure value-domain — registers no property names. ADR-018 zero locks.
    RelayCommand_test.cpp
    # S247_1 (dep:S247) — Bindable-property convenience layer (vaml/mvvm):
    # PROPERTY_BINDABLE(type, Name) + the vaml::mvvm::set_property free template
    # over the S240/S241 core. Verifies the macro's typed backing field + getter/
    # setter, the REAL-change contract (same-value set -> false, no store write,
    # no signal), the genuine S241 PropertyChanged (id, old, new) firing with the
    # right id, independence of multiple bindable props, reader parity through
    # observable_object_reader + the S240 evaluator, direct (non-macro)
    # set_property usage, and Color/Thickness round-trip through the bounded
    # Value store. NO runtime DI, NO reflection — plain C++ templates. ADR-018
    # zero locks. [VIEWER GROWTH: consumed at S256.]
    PropertyBindable_test.cpp
    # S250 (dep:S247_1) — STATIC APP-OWNED COMPOSITION (docs + audit; the lib
    # ships NO DI). Audit (grep-refactor-gate) proves the library ships NO
    # CompositionRoot type, NO runtime container, NO ViewModelLocator, NO
    # pub/sub anywhere in src/core/include, src/vulkan/include,
    # src/presentation/include — only the documented pattern exists. The docs
    # (docs/adr/ADR-LOG.md ADR-006/ADR-008 + docs/guide/03-mvvm-core.md §3.4)
    # are revised in place to state plainly: the library ships NO composition
    # type; the engineer authors their own VM/repository/command construction
    # via PLAIN CONSTRUCTORS + their wiring; the lib only DOCUMENTS the pattern.
    # SRP: docs-only (no code changes). NO-GAP: S250_1 (accessors) + S251
    # (app-constructed VM tests) prove the pattern at the binding level.
    # S250_1 (dep:S250) — ONE CHANNEL, TWO CONSUMERS (ADR-016) via a PLAIN
    # ACCESSOR on EACH layer, NOT a library DI slot: the VM-layer accessor
    # (vaml/mvvm/diagnostics.h: vaml::mvvm::binding_diagnostics /
    # set_binding_diagnostics / binding_policy_default) + the view/control-layer
    # accessor (vaml/core/BindingAccess.h: vaml::view_binding_diagnostics /
    # set_view_binding_diagnostics / view_binding_policy_default), BOTH
    # forwarding to the SAME core channel (BindingDiagnostics.cpp's atomic
    # current sink) — one sink, two consumers. The accessors are pure forwards;
    # zero state, zero injection, zero new locks (ADR-018). BindingAccess_test
    # proves identity (VM accessor == view accessor), emission through both
    # directions, policy-default agreement (Fallback) + the not-a-DI-slot
    # forward-only contract. Pure value-domain — registers no property names.
    BindingAccess_test.cpp
    # S251 (dep:S250) — APP-CONSTRUCTED VM-WITH-INJECTED-SERVICE tests: the app
    # authors its OWN VM (real vaml::ObservableObject + PROPERTY_BINDABLE) and
    # passes its OWN service in via PLAIN CONSTRUCTOR WIRING (no library
    # container, no CompositionRoot) — then VAML binds ONE-WAY (S243 live) and
    # TWO-WAY (S244 write-back) to that VM, and a RelayCommand invoked from the
    # VM calls the INJECTED service + its resulting property change auto-pushes
    # through a one-way binding to a real DependencyObject target. Proves the
    # S250 "engineer wires, we bind" headline. Registers S251-unique object-scoped
    # DPs. ADR-018 zero locks.
    VmAgnosticInjection_test.cpp
    # S252 (dep:S251) — BindingMarkup: the {Binding ...} / {Command ...} markup
    # parser (the S2-tag resolution point). Pure value-domain — registers no
    # property names. Covers the {Binding} path + Mode variants, {Command}, kind
    # None (StaticResource/x:Null), and malformed -> structured malformed flag.
    BindingMarkup_test.cpp
    # S252 (dep:S251) + S253 (dep:S252) — VamlReader markup {Binding} resolution:
    # BindExtensions resolves the deferred S2 tag into a LIVE BindingExpression
    # against a data context (the app-constructed S250/S251 VM) + target DP,
    # applying the first transfer + the live S243 one-way re-push; Mode=TwoWay
    # arms the S244 write-back end-to-end; malformed {Binding} -> a Class A
    # VamlReaderException(MalformedBindingMarkup); kind None (StaticResource)
    # stays deferred. S253 {Command DoIt} is left deferred, then resolved +
    # invocable via the reader's command registry + the S246 ICommand surface.
    VamlReader_binding_test.cpp
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
    # S223 (dep:S222_1) — drag-and-drop → reopen DECISION seams (vaml_viewer/
    # drop_reopen.h, the PURE drop-dispatch half — is_vaml_drop + drop_choice).
    # Header-only like live_reload.h's reload_action table (compiled from the
    # vaml_viewer include path, ADR-021 single source of truth). The reopen
    # HANDLER is the S222_1 apply_reload machinery (golden in LiveReload_test).
    ViewerDropPlan_test.cpp
    # S256 (dep:S253) — VIEWER-GROWTH: the reload populates a VM (plain ctor;
    # NO DI) + renders `{Binding}` (the S240–S252 binding path visible at the
    # viewer boundary). Headless through vaml_viewer/viewer_binding.h (the S256
    # seam — the app-constructed VM + run_viewer_binding_plan, the ADR-021
    # consumer path): proves the S250/S251 app-constructed VM (plain ctor +
    # injected service), the pure bind-render decision table, the REAL plan
    # runner resolving `{Binding Dim}` onto the target UIElement Opacity DP
    # (the S242 first transfer — the visible dim the S208 encoder folds), and
    # the S243 LIVE WIRE staying armed through the RETAINED reader (a later
    # set_Dim re-pushes the bound Opacity). Registers no S256-unique DPs (binds
    # onto the REAL UIElement Opacity DP, S120). ADR-018 zero locks.
    ViewerBindingPlan_test.cpp
    # S329_1 (dep:S323) — VIEWER-GROWTH INTERACTIVE: a click on a rendered element
    # -> a routed ClickEvent (S320/S321) -> the markup handler (S322) -> the VM ->
    # the live binding repaints. Proves "input works in the live viewer" headlessly
    # through vaml_viewer/viewer_interactive.h (the ADR-021 consumer path; the
    # S256 precedent). Registers no S329_1-unique DPs. ADR-018 zero locks.
    ViewerInteractivePlan_test.cpp
    # S224 (dep:S223) — the viewer CLI `--help` ENTRY seam (vaml_viewer/cli.h,
    # the PURE cli_action/cli_usage decision layer) + the RENDERED-VIEW PAN & ZOOM
    # view model (vaml_viewer/view_transform.h, the PURE ViewState seams folding
    # the S208 Matrix2D). Both header-only, compiled from the vaml_viewer include
    # path (ADR-021 single source of truth) — headless, no window/runtime.
    ViewerCli_test.cpp
    ViewerViewState_test.cpp
    # S400 (dep:S368) — the Text control + the pure TextLayout seam (vaml::core;
    # the CORE half — wrap/advance math + the real Text DPs + MeasureOverride +
    # the foreground BrushId resolution). The Material/TextGlyph render half
    # lives in TextGlyphPlan_test.cpp (vaml-vulkan-tests) + ViewerTextMaterial_
    # test.cpp (vaml-presentation-tests).
    Text_test.cpp
    # S401 (dep:S400) — the Button control (the S087 Button MINIMAL-REAL seed
    # RESOLVED at S401): the real re-based Button (IsEnabled DP + the Click
    # routed event + the hover/pressed/disabled visual states + Measure). The
    # material + the classifier half live in the vulkan/presentation targets.
    Button_test.cpp
    # S402 (dep:S401) — the TextBox control (the S087 TextBox MINIMAL-REAL seed
    # RESOLVED at S402): the real re-based TextBox (Text/AcceptsReturn DPs,
    # caret/selection model, key-driven editing, single/multi-line Enter, the
    # S244 two-way write-back on an edit, the S400 wrap Measure). The material
    # + classifier half live in the vulkan/presentation targets.
    TextBox_test.cpp
    # S403/S404/S405 (dep:S402/S403/S404) — the Slider / CheckBox / ScrollViewer
    # controls (the S087 seeds RESOLVED at S403/S404/S405 — all re-based onto
    # UIElement): the combined core controls suite (Slider Value/Min/Max + the
    # value-changed routed event + thumb-drag; CheckBox nullable IsChecked +
    # IsThreeState + the WPF on-toggle cycle; ScrollViewer offset/clipping/wheel).
    # The material + classifier half live in the vulkan/presentation targets.
    Controls_test.cpp
    # S406 (dep:S405) — Focus + tab order + Enter-activates-default + routed key
    # events: extends the S325 FocusManager (default-button + the Enter
    # activation). The material/classifier half lives in the vulkan/presentation
    # targets.
    Focus406_test.cpp
    # S407 (dep:S406) — the Sprint-10 Milestone: every S10 control is a real
    # UIElement re-base (Text/Button/TextBox/Slider/CheckBox/ScrollViewer/Icon),
    # each registered concrete + tested, no DependencyObject-only seeds remain.
    Milestone407_test.cpp
    # S440 (dep:S408) — the dedicated Window RE-BASE test (core). Window is the
    # app-shell root of Sprint 11's in-repo v1 viewer; it is re-based onto
    # UIElement here (resolving the S087 `[MINIMAL REAL] Element Window` sentinel
    # at its own step — NO-GAP, not a backfill). Proves Window IS-A UIElement,
    # resolves via the S087 factory by name, hosts EXACTLY ONE content child (the
    # WPF-faithful single-content slot), and lays its content child out to FILL
    # its box (the implicit single-child layout — a Window root RENDERS content).
    Window_rebase_test.cpp
    # S440 (dep:S408) — app-shell authored + MainView.vaml + the S120/S121 gap
    # closure (AppShellPlan_s440_test.cpp) is compiled into the presentation
    # target below.
    # S441 (dep:S440) + S442 (dep:S441) — the app-shell MainViewModel (final
    # ObservableObject + PROPERTY_BINDABLE props the markup binds: Text/Checked/
    # SliderValue/Dim/Status) + RelayCommands (S246/ICommand) wired to the S442
    # AppShellDemoService via PLAIN CONSTRUCTORS (the S250/S256 wiring pattern),
    # + the S442 static composition seam (build_app_shell_composition). Compiled
    # from the vaml_viewer include + the REAL app_shell_vm.cpp (ADR-021).
    AppShellViewModel_test.cpp
    ../../vaml_viewer/app_shell_vm.cpp       # S441/S442: the VM + service impls (single source of truth)
    # S443 (dep:S442) — app-shell ↔ MainViewModel wired via {Binding}+{Command}:
    # the authored MainView.vaml driven through the S252/S253/S322 reader path
    # (set_data_context + BindExtensions + register_event("Click")/bind_class_handler
    # + BindEvents), reusing/extending the S256/S329_1 plan pattern into
    # run_app_shell_binding_plan (retained reader/VM alive — S243/S244 wires armed).
    # Proves one-way first-transfer; VM set re-pushes the target DP; TextBox.Text
    # two-way write-back (S244); {Command} resolves to the VM RelayCommand + Click
    # via BindEvents runs handler → VM → binding repaint; malformed binding →
    # structured never-silent error. Header-only seam (app_shell_binding.h), no TU.
    AppShellBinding_test.cpp
    # S445 (dep:S444) — the app-shell E2E VERTICAL SLICE (headless): the full
    # authored MainView.vaml driven end-to-end through the S443/S445 binding plan
    # against the retained reader/VM + the target DPs. Proves RENDER (finalized
    # Window), CLICK (RunButton → routed Click → handler → VM → one-way Status
    # repaint), TYPE (TextBox two-way → VM Text), SLIDE (the S445 SliderValue
    # TwoWay wire → VM prop), CHECK (the S445 app-owned CheckBox↔VM wire → VM
    # Checked), + a cohesive flow (all four drive the same VM + bound targets).
    AppShellE2E_test.cpp
    # S320+S321 (dep:S292/S320) — Signal<T> + RoutedEvent primitive + the routed-
    # event engine (bubbling + tunneling + handled over the REAL parent chain).
    RoutedEvent_test.cpp
    # S322 (dep:S321) — markup event wiring: Click="OnClick" -> handler on the
    # x:Class owner via the S322 class-owner registry + the S320/S321 engine.
    VamlReader_event_test.cpp
    # S323 (dep:S322) — hit-testing (deepest/topmost/z/clip/visibility) + the
    # InputMarshaller (input -> routed events; the S160 resolution point).
    S323_Input_test.cpp
    # S324 (dep:S323) — a click hit-tests a real element and raises a bubbling
    # routed event with the correct source: the formal end-to-end gate.
    S324_ClickFlow_test.cpp
    # S325 (dep:S324) — keyboard focus + routed key events (bubble): the
    # Focusable DP, FocusManager tab-order + RaiseKeyRouted.
    S325_Focus_test.cpp
    # S326/S327 (Dispatcher/Task/async) are DEFERRED to Sprint 11a (the
    # multithreaded-UI construction) — NOT tested in Sprint 8 (single-threaded).
    # NO S326/S327 test TUs are present in this target.
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
# vaml-tools-tests — S280 (dep:S255) headless tests of the vamlcc compiler.
# Links the real vaml-tools library (which PRIVATE-links vaml-core), so the
# S280→S283 suites exercise the S2 scanner + factory/value/property tables the
# compiler parses and resolves against — single source of truth, ADR-021
# consumer pattern, never a spawned subprocess in CI.
# ============================================================================
add_executable(vaml-tools-tests
    smoke_test.cpp         # doctest main (DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN), same as vaml-core-tests
    Vamlc_s280_test.cpp
    Vamlc_s280_1_test.cpp
    VbamlSchema_s281_test.cpp
    VbamlSerializer_s282_test.cpp
    Vamlc_s283_test.cpp
    # S284 (dep:S283) — GOLDENS: a full VAML file compiles to a .vbaml with a
    # correct token stream. TEST-ONLY golden gate over compile_to_bytes() — the
    # produced token stream asserted DECODED FROM REAL BYTES (magic/version,
    # schema tables, element-id records in document order, attribute
    # flag/extension/binding/target-prop bits, string-table dedup) + the S282
    # byte-identity + decode-equality re-proof on the golden + positive/negative
    # controls (clean-minimal compiles; bad magic -> VbamlMalformedBuffer).
    Vamlc_s284_test.cpp
    # S285 (dep:S284) — Runtime VBamlReader (vaml::tools): bytes -> DependencyObject
    # tree (InitializeComponent-style). Compiles a golden .vaml to bytes then loads
    # through the REAL compiled-path reader; asserts root type, retained count/order,
    # parent chain, converted literal props (incl. attached dotted keys) + the S285
    # error taxonomy (UnknownProperty/MalformedValue/MalformedBuffer, exported
    # VBamlException element-id + detail).
    VBamlReader_s285_test.cpp
    # S452 (dep:S451) — PROPERTY-ELEMENT authoring grammar, COMPILED-PATH (S7)
    # parity. The S7 vamlcc flat-property-element grammar reproduces the SAME
    # DP-set outcome as the S2 text reader for both value kinds: literal-content
    # folded as a parent attr (propertyElement bit) Sets the parent's DP; object-
    # value emitted as a marked child (propertyElementTarget) Sets the parent's
    # DP to UntypedPtr AND is excluded from the S121 visual-children transfer;
    # unknown-dotted -> VBamlException(UnknownProperty).
    PropertyElementS7_s452_test.cpp
    # S470 (dep:S466) — DockPanel control RESOLVED (11c): the ONLY missing layout
    # container. Attached DockPanel.Dock DP (resolves the S462-recorded NO-GAP),
    # the WPF-faithful dock-stacking Measure/Arrange (Left/Right consume width,
    # Top/Bottom consume height, last-child Fill takes the remaining), DesiredSize
    # = Σ tracks, dirty-respect (S128), registered built-in 16 -> 17.
    DockPanel_test.cpp
    # S471 (dep:S470) — RadioButton control RESOLVED (11c): IsChecked + the
    # honest single-parent exclusive group semantic + routed Checked/Unchecked,
    # registered built-in 17 -> 18.
    RadioButton_test.cpp
    # S472 (dep:S471) — ComboBox control RESOLVED (11c): Items/ItemsSource +
    # SelectedIndex/SelectedItem + SelectionChanged + the dropdown interaction
    # (is_open/open/select_item), registered built-in 18 -> 19.
    ComboBox_test.cpp
    # S473 (dep:S472) — ListBox + ListView controls RESOLVED (11c): the shared
    # item-selection base + ItemTemplate property-element authoring -> items
    # render authored content through the S364 seam, registered built-ins 19->21.
    ListBoxListView_test.cpp
    # S286 (dep:S285) — Reader instantiates via factory registry, sets props,
    # attaches deferred events (S8). The full compiled-path deferred surface:
    # x:Name/x:Class/x:Key registries (DuplicateName/ClassOnNonRoot/root_class),
    # {Binding} DeferredExtensions recorded verbatim (NOT set/resolved), event
    # attach via register_event -> DeferredEventHandler (never unknown-property),
    # per-Load clearing + error-taxonomy distinctness. Parity of the deferred
    # api-shape with the S092 text reader is asserted structurally + behaviorally.
    VBamlReader_s286_test.cpp
    # S287 (dep:S286) — vamlcc integrated in CMake: custom command per .vaml ->
    # .vbaml embedded or sidecar. RESOLVES the S003 tools MINIMAL REAL marker.
    # Three proofs: (1) the embed generator units (write_embed + symbol
    # sanitization), (2) the CMake-WIRED format — the demo .vaml is compiled at
    # build time by vaml_compile_vaml() into BOTH a side-car .vbaml AND an
    # embedded C++ pair whose generated .cpp is COMPILED here as a real source
    # (VAML_S287_EMBED_CPP) so the test proves the embedded span == the sidecar
    # bytes AND the sidecar loads through the S285 reader, (3) the tools version
    # marker is now REAL tooling surface (S003 resolved; sentinel removed).
    Vamlc_s287_test.cpp
    # S288 (dep:S287) — Dual-path parity harness: runtime-parsed (S2 core
    # VamlReader) vs compiled (S7 tools VBamlReader over vamlcc->.vbaml bytes)
    # trees behaviorally identical (asserted). Drives BOTH real readers over the
    # same VAML source and asserts the per-axis comparator (compare_dual_path +
    # compare_dual_path_props) reports all green: root type, retained
    # count/order/types, parent chain, x:Name registry, deferred {Binding}
    # extensions, deferred event handlers, literal-prop converted values (incl.
    # attached dotted keys). Proven on the S284 golden + a focused control; the
    # harness is honest (no false-green). Release-consumer contract: S7 is the
    # release path, S2 the dev tool — parity, not duplication.
    VBamlReader_s288_parity_test.cpp
    # S289 (dep:S288) — Pack/identity: x:Class name mapping; embedded resource
    # lookup; release uses compiled only. Tests the REAL pack/identity layer
    # (PackUri parse matrix, ResourcePack register/resolve/dedupe-throws/
    # registered_keys order, the add_embedded "embedded resource lookup",
    # resolve_pack_uri pack-identity -> bytes seam) + the VBamlReader
    # InitializeComponent / InitializeComponentFrom compiled-release entries
    # (x:Class owner binding + class mapping + OwnerClassMismatch + the
    # no-text-parameter static_assert for "release uses compiled only").
    Pack_s289_test.cpp
    # S366 (dep:S365) — Dual-path shared style/template resolution parity: a
    # styling doc loads through BOTH the S2 text reader and the S7 compiled
    # reader; implicit-style + {StaticResource} resolution reach the SAME core
    # surface (find_implicit_style/apply_implicit_style/find_for_element)
    # identically on both paths. Proves S2+S7 agree on the styling RESOLUTION
    # surface. (Full resource-<authoring>-grammar in the S2/S7 parsers is a
    # recorded NO-GAP forward surface; the mechanism tokens S360–S365 ship the
    # resolution engine this test drives through the shared core functions.)
    TemplateParity_test.cpp
    # S369 (dep:S368) — VIEWER-GROWTH: live re-skin on template/style/brush edit.
    # The S9 anti-grave-whistle gate token. Compiles the REAL viewer seam
    # (vaml_viewer/viewer_restyle.h, ADR-021) into the test target: the pure
    # re-skin decision table + the plan runner that re-Loads a styled .vaml and
    # applies an EDITED brush through the S364 state/brush-DP engine — the
    # rendered fill re-skis live (edited red -> green). Missing target readable,
    # bad markup fails with structured detail.
    ViewerRestylePlan_test.cpp
    # S455 (dep:S454) — VIEWER-GROWTH(11a): the RESOURCE/STYLE authoring grammar
    # (S453 + S454) driving a LIVE re-skin. Extends the S369 viewer_restyle.h seam
    # with run_viewer_style_restyle_plan: a styled .vaml (authored ResourceDictionary
    # + implicit Style) is re-Loaded + the authored implicit Style re-applied by
    # TARGET TYPE (apply_implicit_style) — the re-skin headline driven from the
    # fresh authoring grammar. Proves the authored <Setter> lands on the target,
    # an authored-Background edit re-skis red->green, an EXPLICIT-only style does
    # NOT auto-re-skin (implicit only, never silent), missing target + bad markup
    # are readable (never silent).
    ViewerRestyleS11a_s455_test.cpp
    # S460 (dep:S456) — BRUSH AUTHORING: LinearGradientBrush + GradientStop (11b).
    # The S2 text reader authors a gradient brush from real .vaml: an authored
    # <LinearGradientBrush x:Key> (StartPoint/EndPoint + an authored
    # <LinearGradientBrush.GradientStops> list of <GradientStop Color Offset>)
    # is captured BY its x:Key as an S360 Object resource, the authored stops +
    # start/end settle into the brush's data model, and the gradient registers +
    # resolves by id through the S460 "Brush:<id>" convention
    # (resolve_foreground_color recognizes a gradient's scalar representative
    # color — the same BrushId path a SolidColorBrush rides). The element registry
    # gate grows 14 -> 16. [MINIMAL REAL LinearGradientBrush — RESOLVED BY S465.]
    BrushAuthoring_s460_test.cpp
    # S461 (dep:S460) — MARKUP EXTENSIONS x:Static + x:Type (11b; the S2-tag
    # deferral CLOSED). The {Binding} markup parser recognizes the Static/Type
    # kinds; the reader resolves an app-registered static VALUE (`{x:Static
    # local:Constants.AppName}`, WPF tail-token lookup, S250 no-reflection) + a
    # type OBJECT (`{x:Type Button}`, Value::String for TargetType/ContentControl)
    # and pushes them onto the authored target DPs; an unresolved / malformed
    # static or type is a Class A structured reader error (never silent).
    MarkupStaticType_s461_test.cpp
    # S462 (dep:S461) — ATTACHED-PROPERTY AUTHORING + FrameworkElement Name (11b).
    # The generic dotted-key reader path authors attached DPs (Canvas.Left/Top
    # prove the path; Grid.Row/Grid.Column already worked), and a plain
    # `Name="submitButton"` attribute registers the object in the S092 name
    # registry ON PAR with x:Name (named() resolves; a duplicate -> the
    # DuplicateName authoring error, never silent). DockPanel.Dock is recorded
    # NO-GAP -> S470. ADR-018 zero locks.
    AttachedAuthoring_s462_test.cpp
    # S463 (dep:S462) — EVENT-ATTRIBUTE wiring for the authored set (11b): the
    # S322 markup-event wiring extends to Window Loaded/Closing (raised by the
    # Window), TextBox TextChanged (raised on the S402 edit commit), and the
    # RAISED-ATTACHED form StackPanel.Button.Click (a dotted event key whose
    # event TAIL "Click" routes + bubbles to the authored-on panel over the
    # Sprint-8 engine) — completing the S322 event-hook resolution for the
    # authored surface.
    EventAttr_s463_test.cpp
    # S464 (dep:S463) — COMMAND + CommandParameter attributes (11b): the S253
    # {Command} surface extended to a PARAMETERIZED execute. `Command="{Binding
    # SaveCommand}"` resolves the command BY NAME (the app-registered S253
    # registry); `CommandParameter="{Binding SelectedItem}"` provides a parameter
    # VALUE; VamlReader::ExecuteCommand(name, value) gates on S246 CanExecute then
    # invokes ICommand::Execute(Value) — the reader seam a Button/MenuItem command
    # drives. MenuItem.Header/Command/CommandParameter ride the same attribute
    # surface (element = S491/11e, recorded forward). ADR-018 zero locks.
    CommandParam_s464_test.cpp
    # S465 (dep:S464) — VIEWER-GROWTH(11b): the remaining-attribute surface rendered.
    # A fully-authored S11b sample (gradient brush S460 + x:Static/x:Type S461 +
    # attached props + Name S462 + event attrs S463 + Command/CommandParameter S464)
    # LOADS cleanly; resolve_gradient_fill() samples the authored gradient along
    # its stops (the vertex-sampled tint over the S206 solid-rect path — RESOLVING
    # the S460 LinearGradientBrush MINIMAL REAL seed at its own step, sentinel
    # removed); a wired Click + a parameterized Command execute visibly. Headless
    # Rail 1 (the [both-apps] window display stays the local gate). ADR-018.
    ViewerGrowth_s465_test.cpp
    # S293 (dep:S288) — VIEWER-GROWTH: vaml_viewer opens a COMPILED `.vbaml`
    # (the S280→S289 path visible). Compiles the REAL viewer seam
    # (vaml_viewer/compiled_open.h + the shared app.h) into the TOOLS test target
    # (it links vaml-tools: the compiled_open seam consumes vamlcc compile_to_bytes
    # + the S285 VBamlReader). Drives load_compiled_bytes over the REAL compiled
    # pipeline (malformed → typed failure; retained reader keeps named()/retained
    # alive), compile_to_vbaml → VBamlReader round-trip, load_compiled_file
    # (.vaml compiles-first vs .vbaml direct-load parity), and the
    # recompile-then-reopen contract (an edited source recompiles to a NEW tree).
    ViewerCompiledOpen_s293_test.cpp
    # S444 (dep:S443) — the vamlcc compile step for the app-shell's MainView.vaml
    # (release path). Compiles the REAL viewer seam (vaml_viewer/app_shell_compiled.h)
    # into the TOOLS test target (links vaml-tools: the S288 comparator + the S285
    # reader + the S289 pack). Proves .vbaml→VBamlReader decodes to the SAME live
    # tree as the text path (S288 parity), the on-disk MainView.vaml compiles +
    # decodes to the shell, the embedded span == the build sidecar (S287), the
    # sidecar loads (S285/S440 release file path), the embedded twin initializes
    # via InitializeComponentFrom (S289 single-exe path), and malformed bytes →
    # typed never-silent failure. Compiles the ACTUAL CMake-generated embed .cpp.
    AppShellCompile_test.cpp
    # S447 (dep:S446) — IN-REPO E2E INTEGRATION of the app-shell (lives in
    # vaml-tools-tests — it links vaml-tools so leg (D) can load the release
    # compiled sidecar, and it already hosts the S444 suite + the vaml_viewer
    # include). One coherent narrative across all FIVE surfaces: (A) text parse →
    # live Window tree (S440), (B) VM/binding/command wiring (S443), (C) each
    # interaction → VM props + bound target DPs (S445), (D) release-compiled +
    # dev-text serving the SAME named shell in ONE scene (S444 single-scene
    # contraction — NOT S444's per-path gold), (E) the LiveReload re-wire (S222_1:
    # edit → reload → fresh tree → binding plan re-run → S243/S244 wires re-arm).
    # Necessarily compiles the binding-plan + reload seam TUs (app_shell_vm.cpp +
    # load_file.cpp + file_watch.cpp + live_reload.cpp — the ADR-021 single source
    # of truth, per the S443/S445/S221/S222_1 precedent in the other targets).
    AppShellIntegration_test.cpp
    ../../vaml_viewer/app_shell_vm.cpp      # S447: run_app_shell_binding_plan + the VM/composition impls
    ../../vaml_viewer/load_file.cpp         # S447/S221: the reload load path (vaml::load_file)
    ../../vaml_viewer/file_watch.cpp        # S447/S222_1: the file watcher (LiveReload core)
    ../../vaml_viewer/live_reload.cpp       # S447/S222_1: the reload coordinator
)

# Link the S287 test against the CMake-generated embed source (compiles the
# ACTUAL generated code — ADR-021 single source of truth) + the sidecar path.
# The generated .vbaml/.h/.cpp are produced by the vamlcc_demo_main custom
# target; the test must DEPEND on it so the files exist before the test
# compiles and includes the generated header.
if(VAML_S287_EMBED_CPP AND VAML_S287_EMBED_HDR)
    target_sources(vaml-tools-tests PRIVATE "${VAML_S287_EMBED_CPP}")
    target_include_directories(vaml-tools-tests PRIVATE "${VAML_S287_EMBED_INC}")
    target_compile_definitions(vaml-tools-tests PRIVATE
        S287_DEMO_VBAML="${VAML_S287_EMBED_SIDECAR}"
        S287_DEMO_EMBED_H="${VAML_S287_EMBED_HDR}"
    )
    add_dependencies(vaml-tools-tests vamlcc_demo_main)
endif()

# S444 (dep:S443): compile the ACTUAL CMake-generated app-shell embed source +
# feed the sidecar + the on-disk MainView.vaml (the vamlcc INPUT) to the S444
# test. The generated files are produced by the vamlcc_app_shell custom target;
# the test must DEPEND on it so they exist before AppShellCompile_test.cpp
# compiles + includes the generated header (ADR-021 single source of truth).
if(VAML_S444_EMBED_CPP AND VAML_S444_EMBED_HDR)
    target_sources(vaml-tools-tests PRIVATE "${VAML_S444_EMBED_CPP}")
    target_include_directories(vaml-tools-tests PRIVATE "${VAML_S444_EMBED_INC}")
    target_compile_definitions(vaml-tools-tests PRIVATE
        VAML_S444_EMBED_SIDECAR="${VAML_S444_EMBED_SIDECAR}"
        VAML_S444_EMBED_H="${VAML_S444_EMBED_HDR}"
        VAML_S444_EMBED_INPUT_VAML="${VAML_S444_EMBED_INPUT_VAML}"
    )
    add_dependencies(vaml-tools-tests vamlcc_app_shell)
endif()

vaml_apply_compiler_policy(vaml-tools-tests)

target_include_directories(vaml-tools-tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/tools/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../vaml_viewer  # S221 precedent: consumer seams
)
target_include_directories(vaml-tools-tests SYSTEM PRIVATE
    ${VAML_DEPS_DIR}
)

if(MSVC)
    target_compile_options(vaml-tools-tests PRIVATE /external:W0)
else()
    target_compile_options(vaml-tools-tests PRIVATE "-Wno-#warnings")
endif()

target_link_libraries(vaml-tools-tests PRIVATE vaml-tools)

add_test(NAME vaml-tools-tests COMMAND vaml-tools-tests)

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
    # S400 (dep:S368) — the RENDER side of the Text control (vaml::vulkan::ui):
    # text_glyph_uv (the S202 exact-grid UV derivation) + build_text_glyph_items
    # (one S206 text_glyph quad per rendered glyph, atlas-cell sized + titled) +
    # classify_element -> MaterialKind::Text for a real Text. Pure + headless.
    TextGlyphPlan_test.cpp
    # S400_1 (dep:S400) — the Icon control (per ADR-015): the DP surface +
    # the S400_1 render seam (IconGlyphPlan) — the S213_1 best-fit quad +
    # the S205_1 Monochrome tint path + the DELIBERATE fallback glyph + the
    # structured diagnostic (ADR-015 NO-silent). Pure + headless.
    IconGlyphPlan_test.cpp
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
    ../../vaml_viewer/app_shell_vm.cpp  # S443/S445: the app-shell VM + composition (run_app_shell_binding_plan — app.cpp now uses it)
    ../../vaml_viewer/load_file.cpp  # S221 seam (live_reload.cpp below calls vaml::load_file)
    # S222_1 (dep:S222) — the viewer LIVE-RELOAD plan seam + the reload coordinator
    # sources. Compiles the ACTUAL shared host sources (app.cpp + the file-watch +
    # live-reload seams) into the presentation test target so the headless suite
    # exercises the exact code both apps run (ADR-021, single source of truth).
    # The goldens (edit → reload → same tree) live in vaml-core-tests.
    ViewerReloadPlan_test.cpp
    ../../vaml_viewer/file_watch.cpp
    ../../vaml_viewer/live_reload.cpp
    # S224 (dep:S223) — the RENDERED-VIEW PAN & ZOOM at the LIBRARY surface: the
    # pure `compose_view_into_render_plan` seam (S208 Matrix2D -> S213 RenderPlan
    # mvp, DPI uiScale untouched) + the host's set_view_transform/view_transform
    # (headless-observable via the S212 create() path). Links vaml-presentation,
    # so the S208 transform compose into the S213 render-plan surface is proven.
    ViewerPanZoom_test.cpp
    # S293 (dep:S288) — VIEWER-GROWTH compiled-path PLAN: the app COMPILED-mode
    # decision table (--compiled / .vbaml auto-detect) + the compiled-open factory
    # contract (ViewerApp::create_compiled accepts a UIElement-rooted compiled
    # result, rejects a non-ok / no-root one — never a blank window). Links
    # vaml-tools + app.cpp (the shared host) + the cli_seam + compiled_open seam.
    ViewerCompiledPlan_s293_test.cpp
    # S400 (dep:S368) — the Text material at the presentation vertical-slice
    # boundary: classify_merge_element maps a real Text to MaterialKind::Text +
    # a TextGlyph DrawItem at the arranged origin (the S409_1 consumer seam's
    # default). Headless.
    ViewerTextMaterial_test.cpp
    # S400_1 (dep:S400) — the Icon material at the presentation vertical-slice
    # boundary: classify_merge_element maps a real Icon to MaterialKind::
    # Monochrome + the S400_1 fallback glyph at the arranged origin (the honest
    # headless render without a live device atlas; the real atlas quad is the
    # [s400_1-live]/S409_1 path). Headless.
    ViewerIconMaterial_test.cpp
    # S401 (dep:S400) — the Button material at the presentation vertical-slice
    # boundary: classify_merge_element maps a real Button to MaterialKind::Solid
    # at its arranged rect, honoring the visual-state Background + IsEnabled (a
    # disabled Button's fill is faded). Headless.
    ViewerButtonMaterial_test.cpp
    # S402 (dep:S401) — the TextBox material at the presentation vertical-slice
    # boundary: classify_merge_element maps a real TextBox to MaterialKind::Solid
    # (the S402 edit-field fill box) at its arranged rect. Headless.
    ViewerTextBoxMaterial_test.cpp
    # S403/S404/S405 — the Slider / CheckBox / ScrollViewer materials at the
    # presentation vertical-slice boundary (Solid material + the enabled/checked/
    # viewport default draws). Headless.
    ViewerControlsMaterial_test.cpp
    # S409_1 (dep:S408) — VIEWER-GROWTH: the live viewer hosts the REAL S10
    # control set render + interactive via the generic renderer (the "same for
    # S10+" commitment). Headless proof that the viewer's generic classifier
    # renders EVERY S10 control to a real material (the sample.vaml is the
    # [both-apps] live-display proof).
    ViewerControlsPlan_s409_1_test.cpp
    # S475 (dep:S474) — VIEWER-GROWTH(11c): the sample .vaml authors the DockPanel
    # layout + the selection controls; the LIVE viewer renders the DockPanel
    # layout + each selection control interactively (RadioButton exclusive group,
    # ComboBox dropdown selection). Headless proof of the [both-apps] sample.
    ViewerGrowth_s475_test.cpp
    # S11c (S470–S473) — the DockPanel + selection controls (RadioButton/
    # ComboBox/ListBox/ListView) materials at the vertical-slice boundary:
    # classify_merge_element -> MaterialKind::Solid for each (DockPanel fill /
    # RadioButton check-glyph / ComboBox field / ListBox-ListView fill), + the
    # checked-vs-unchecked RadioButton alpha. Headless.
    ViewerSelectionMaterial_test.cpp
    # S440 (dep:S408) — the APP-SHELL: the authored MainView.vaml + the REAL
    # Window (re-based onto UIElement) + run_app_shell_plan, which Loads the
    # shell through a RETAINED VamlReader and calls finalize_visual_tree() — the
    # S120/S121 hand-off that TRANSFERS every declared child into its parent's
    # S121 visual-children list, so the Window RENDERS its declared
    # Grid/StackPanel/Text/TextBox/Button/Slider/CheckBox subtree (NOT just the
    # root). This closes the Sprint-10 S120/S121 gap at its real home (the core
    # reader) — the "make all S10 controls functional with children defined in a
    # running app" directive of Sprint 11. Header-only seam (app_shell.h).
    AppShellPlan_s440_test.cpp
    # S446 (dep:S445) — the app-shell HARDENING/VERIFICATION suite (headless Rail
    # 1, vaml-presentation-tests — it links vaml-presentation so it can drive the
    # S212 headless VamlRenderer host + the REAL app-shell tree + the S146 funnel).
    # PROVES the three S446 title claims against the REAL app-shell through the
    # REAL library surfaces. (a) NO LAYOUT THRASH: the S146 heap-churn contract
    # (heap_allocation_count/bytes) elevated to the app-shell arrange+encode+
    # publish steady state (FLAT after warmup across repeated passes). (b) NO LEAK
    # OVER 10K FRAMES (headless equivalent): 10,000 repeated producer passes stay
    # heap-flat AND the retained-reader reachability (RunButton/InputBox/DimSlider/
    # EnabledCheck/StatusText) stays intact. (c) RETAINED-STABLE AFTER 10K: the
    # S443/S445 TwoWay wires still write back to the VM store. The authoritative
    # zero-leak on BOTH compilers is the ASan/LSan lane (verify_s446_asan.cmd)
    # running THIS suite under ASan.
    AppShellChurn_test.cpp
)

vaml_apply_compiler_policy(vaml-presentation-tests)

target_include_directories(vaml-presentation-tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/presentation/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/tools/include   # S293: app.cpp includes <vaml/tools/VBamlReader.h>
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

target_link_libraries(vaml-presentation-tests PRIVATE vaml-presentation vaml-tools)

add_test(NAME vaml-presentation-tests COMMAND vaml-presentation-tests)
