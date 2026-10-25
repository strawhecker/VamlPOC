# VAML ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â PROGRESS / LLM CONTINUATION FILE  (SINGLE SOURCE OF TRUTH)

> ## HOW TO RESUME (an LLM must read this first, with no prior context)
> 1. Read ONLY this header block + the two `## CHECKPOINT` lines below.
> 2. That tells you: repo state, last completed step token, next step token, and the
>    surviving-state fingerprint. Do NOT re-read the history section to resume.
> 3. If a step is partially done (PENDING), read its SURVIVOR block directly.
> 4. Generated artifacts are never trusted; regenerate any derived file from the
>    surviving source files. Trust this file for WHERE things stand, trust source
>    files for WHAT the code is.

---

## HEADER ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â VERSION 1
- Framework: VAML (XAML-faithful, Vulkan-native, pure C++)
- Language/stack: C++ latest only. No C#, no Python, no managed code (includes build tooling).
- Distribution: consumer chooses shared (dll/so) or static (lib/.a); default = static as a build-model
  preference (determinism + LTO), NOT IP-hiding ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â this MIT-public lib has no IP to protect. A consumer app
  may statically link to compile ITS OWN IP into a single exe (app-level, not this repo's). [ADR-003 revised]
- Repos: this `VamlPOC` repo IS the product ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â v1 built at v2 quality (ADR-017). NOT a throwaway POC;
  there is no separate production repo to open later. The repo is production-framed from here on.
- Build: MSVC (VS 2026) on Windows and Clang on Linux; CMake is the canonical driver. [ADR-007]
- Tests: doctest framework + adapted Shouldly-style fluent asserts (ported, MSTest-decoupled). [ADR-009]
- Async: Task/Executor continuation model, NOT compiler coroutine keywords. [ADR-010]
- MVVM: VAML ships its own lightweight MVVM layer (ObservableObject, RelayCommand, RoutedEvent). No library
  composition type, no Messenger, no DI container — app composition is STATIC APP-OWNED (plain constructors +
  the engineer's wiring), only the pattern is documented. No Prism-style runtime modularity. [ADR-008 revised at S250]

## CHECKPOINT ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â DO NOT EDIT BY HAND (agents regenerate)
- Last completed step : S255 (dep:S254, Sprint 6 close) - S252 -> S253 -> S254 -> S255 -> S256 complete + NO-GAP'd + BOTH CI lanes green this session. S252 = MARKUP {Binding} RESOLVED IN VamlReader (BindingMarkup parser + VamlReader::set_data_context/BindExtensions - the S2-tag resolution; malformed -> MalformedBindingMarkup throw; kind None stays deferred S361). S253 = COMMAND via {Command} (register_command + ResolveCommand, the S246 ICommand surface; one/two-way markup binding end-to-end). S254 = REFACTOR GATE(S6): full MVVM in v1 (all modes + converters + Fallback/Strict policy + diagnostics, no stubs) + ASan/LSan zero-leak. S255 = SURVIVOR + checkpoint advance to S280. S256 = VIEWER-GROWTH: reload populates a VM (plain ctor, NO DI) + renders {Binding} via vaml_viewer/viewer_binding.h + bind_and_render (the visible dim; the S243 live wire stays armed through the retained reader). Full digests in the Sprint 6 SURVIVOR block below + docs/roadmap.md. --[legacy checkpoint: S251 digest preserved below]-- S251 (dep:S250) - Sprint 6 - App-constructed VM-with-injected-service tests (the "engineer wires, we bind" proof at the binding level). S251 DONE + VERIFIED from first principles BOTH CI lanes this session, together with S250 + S250_1 (the S250->S250_1->S251 trio). S250 = STATIC APP-OWNED COMPOSITION (no library DI) - documented by REVISING docs/adr/ADR-LOG.md (ADR-008: struck "VAML ships its OWN static CompositionRoot + Messenger"; the library ships NO composition type, NO runtime container, NO ViewModelLocator/DI/pub-sub; the engineer authors plain-constructor wiring; ADR-016 one-channel restate) + docs/guide/03-mvvm-core.md §3.4 (Q1 "Who owns this object?" grounded in the actually-shipped ObservableObject + PROPERTY_BINDABLE/set_property + RelayCommand + BindingExpression; plain-constructor wiring, no library container; the "engineer wires, we bind" contract) + a grep-refactor-gate AUDIT proving the library ships NO DI surface (CompositionRoot/ViewModelLocator/Messenger/ISubscribe/Subscribe/Publish/Container/IServiceProvider across all public include dirs + src/**/*.cpp = ZERO matches). S250_1 = ONE CHANNEL, TWO CONSUMERS (ADR-016) via a PLAIN ACCESSOR on EACH layer, NOT a library DI slot: NEW src/core/include/vaml/core/BindingAccess.h (the view/control-layer accessor: vaml::view_binding_diagnostics / set_view_binding_diagnostics / view_binding_policy_default - pure inline forwards to the SAME core channel, kept out of DependencyObject.h to avoid the BindingDiagnostics->PropertyPath include cycle) + NEW src/core/include/vaml/mvvm/diagnostics.h (the VM-layer accessor: vaml::mvvm::vm_binding_diagnostics / set_vm_binding_diagnostics / binding_policy_default - vm_/view_ prefix symmetry mirrors the view accessor + avoids the ADL overload collision with the identically-shaped core vaml::binding_diagnostics) + NEW test/unit/BindingAccess_test.cpp (6 TEST_CASEs: identity / emission both directions / policy-default agreement / not-a-DI-slot) + MOD test/unit/CMakeLists.txt. S251 = APP-CONSTRUCTED VM-WITH-INJECTED-SERVICE tests: NEW test/unit/VmAgnosticInjection_test.cpp (4 TEST_CASEs) - app-authored S251DemoService + S251ViewModel : vaml::ObservableObject (PROPERTY_BINDABLE Name), service injected via PLAIN CONSTRUCTOR wiring (S251ViewModel(S251DemoService&), NO container); one-way (service-driven set_Name -> real DependencyObject target DP live via the real S243 wire), two-way (target edit -> VM write-back verified through vm.Get per the S244 convention), command->service (RelayCommand invoked on the VM calls the INJECTED service + the resulting set_Name change auto-pushes through a one-way binding to the target), no-library-composition (the ONLY construction path is "construct service, construct VM with &service"; nothing registered/resolved/ViewModelLocator'd). NEW build/verify_s250.cmd (covers the whole trio, msvc-debug). NO-GAP (recorded not silent): the S250_1 accessors are REAL permanent Cat-1, forward-consumed by S252/S253 {Binding} + S256 viewer growth. ADR-018: ZERO new locks (both accessors forward to the existing core atomic sink in BindingDiagnostics.cpp). NO MINIMAL REAL introduced/resolved. BOTH CI LANES GREEN from first principles: windows-msvc (build/verify_s250.cmd: msvc-debug configure clean + build warning-as-error CLEAN + ctest 3/3 PASS incl. BindingAccess 6 cases + VmAgnosticInjection 4 cases; prior suites green) + ubuntu-clang (/mnt/hgfs/VamlPOC clang-debug: configure clean + build warnings==0 + ctest 3/3 PASS incl. the S250_1 + S251 suites). No commits. S246 (the ICommand-analogue + RelayCommand implementation, FULL + no stub): NEW src/core/include/vaml/core/ICommand.h (the pure abstract `vaml::ICommand` - Execute(Value) the invoke, CanExecute(Value) const the pure side-effect-free query, add_can_execute_changed(handler)->token / remove_can_execute_changed(token)->bool the CanExecuteChanged EVENT exposed on the interface, WPF-faithful, with the token-subscription registration-ordered pattern mirroring ObservableObject S241 / DependencyObject S041 - a strict superset of the external delegate.h/Accupulse iterator-handle style, deliberately NOT imported cross-repo; ADR-018 zero locks) + NEW src/core/include/vaml/core/RelayCommand.h + NEW src/core/src/RelayCommand.cpp (`vaml::RelayCommand final : ICommand` - ctor REQUIRES an ExecuteAction std::function<void(const Value&)> + optional CanExecutePredicate (null/absent => CanExecute returns TRUE, the WPF default); Execute invokes the action with the exact parameter; CanExecute runs the predicate side-effect-free; NotifyCanExecuteChanged() raises the event in REGISTRATION order (snapshot-before-invoke, the S244 discipline); MOVE-ONLY per the S170 move-only RAII precedent - owns the handler list, moved-from INERT (Execute no-op / CanExecute true / list emptied)) + MOD src/core/CMakeLists.txt (+RelayCommand.cpp). S247 (RelayCommand TESTS): NEW test/unit/RelayCommand_test.cpp (13 TEST_CASEs / 52 assertions, Shouldly vocabulary, pure value-domain, registers NO property names) - execute-only always-executable (incl. through the interface), predicate-governed CanExecute per parameter, Execute invokes the action with the EXACT parameter Value, subscribe -> NotifyCanExecuteChanged fires once, multiple subscribers fire in REGISTRATION order, remove removes one + the remainder fire + re-add appends (true/false for present/unknown tokens), tokens distinct + monotonic, zero-subscriber safe no-op, side-effect-free CanExecute, Value::Null() parameter, the MOVE-ONLY transfer contract (move ctor + move-assign re-arm action/predicate/handler-list onto the destination, moved-from inert), and the re-query contract (a captured flag flips CanExecute + a subscribed handler observes the CURRENT flag via NotifyCanExecuteChanged) + MOD test/unit/CMakeLists.txt (+RelayCommand_test.cpp) + NEW build/verify_s246.cmd + NEW build/verify_s247.cmd. NO MINIMAL REAL introduced (both symbols complete + permanent + consumed by S247's tests + the S253 {Command} markup / S441 app-shell). NO-GAP recorded (not silent): S247 = the test suite; S247_1 (Bindable-property convenience) + S253 ({Command}) + S441 (app-shell RelayCommands) consume this command primitive. ADR-018: ZERO new locks. BOTH CI LANES green from first principles: windows-msvc (verify_s246.cmd msvc-debug warning-as-error CLEAN + ctest 3/3 PASS incl. the S247 suite 13/13, 52/52; prior suites green) + ubuntu-clang (/mnt/hgfs/VamlPOC clang-debug: build warnings==0 + ctest 3/3 PASS incl. the S247 suite 13 cases / 52 assertions SUCCESS). No commits. [--legacy checkpoint: S245_1 full digest preserved below--] - Sprint 6 - Binding-failure test suite: harness subscribes to the sink and asserts exact reasonCode per Class (not just fallback values); detach/lifecycle (S245) is a Class-B-tagged, silenced non-error; Diagnostics != control-flow (subscriber gets info, never control). S245_1 DONE + VERIFIED from first principles BOTH CI lanes this session. TEST-ONLY binding-failure CONSOLIDATION SUITE (the S040/S042/S044/S125/S128/S130/S133/S135/S137 test-only precedent); ZERO new product symbols; NO MINIMAL REAL introduced/resolved. NEW test/unit/BindingFailure_test.cpp (12 TEST_CASEs / 83 assertions, Shouldly vocabulary, S245_1-unique object-scoped DPs + real ObservableObject sources through the real S241 reader + S240 evaluator + S240_1 sink) - the harness SUBSCRIBES to the sink via a subscribing RecordingSink (SinkGuard installs + restores) and asserts the EXACT reasonCode sequence PER CLASS (not just fallback values). Class A (StaticAuthoring): malformed/empty path -> create returns nullptr + exactly [MalformedPath]/[EmptyPath] with NO fallback (Class A never falls back); Class A Strict -> emits [MalformedPath] THEN throws BindingPolicyException (the emit is never the control flow, the throw is). Class B (DefinedRuntimeOutcome), exact codes: [MissingProperty], [MissingProperty, FallbackApplied], [NullIntermediate], [NotReadable], [ConverterFailed] (+ [FallbackApplied] when bound), live OneWay [SetTargetFailed] (caller-driven FIRST push silent - S242/S243 parity re-proof), live TwoWay [WriteBackRejected] (source untouched). HEADLINE DETACH/LIFECYCLE (S245) IS A CLASS-B-TAGGED SILENCED NON-ERROR: destroying the target element -> a later write emits exactly [Detached] ONCE, binding_class_of(Detached)==DefinedRuntimeOutcome, distinct from SetTargetFailed/WriteBackRejected, never throws, never a dangling write. DIAGNOSTICS != CONTROL-FLOW: the same Class B failure run with the NO-OP sink vs a RECORDING sink yields the IDENTICAL TransferResult + target state, and a Class A default-Fallback create returns nullptr with its diagnostic still emitted (the subscriber observes, never steers). NEW build/verify_s245_1.cmd + MOD test/unit/CMakeLists.txt (+BindingFailure_test.cpp). BOTH CI LANES green from first principles: windows-msvc (verify_s245_1.cmd msvc-debug warning-as-error CLEAN + ctest 3/3 PASS incl. the S245_1 suite 12/12, 83/83; prior suites green) + ubuntu-clang (/mnt/hgfs/VamlPOC clang-debug: build warnings==0 + ctest 3/3 PASS incl. the S245_1 suite 12/12, 83/83). No commits. NO-GAP (recorded, not silent): this TEST-ONLY step adds NO new forward contracts; S246 (RelayCommand) is the next chain link (per the S245_1 NO-GAP). ADR-016 (diagnostics = observation, never control) + ADR-018: ZERO new locks. NO MINIMAL REAL introduced/resolved. [--legacy checkpoint: S245 full digest preserved below--] - Sprint 6 - Expression lifecycle (detach on element destroy) + the S240_1 Detached arm. S245 DONE + VERIFIED from first principles BOTH CI lanes this session. EXPRESSION LIFECYCLE (detach-on-element-destroy) + the Detached silenced non-error (the S240_1 forward arm) RESOLVED IN PLACE. DECISION (recorded): the title fragment "Height/glide target (fallback)" is a STRAY (matches zero roadmap lines + zero code; scope = the binding-lifecycle second half); Option 1 chosen - a real DependencyObject::add_destroyed/remove_destroyed destruction event (fired at the START of the destructor BEFORE member teardown, copy-before-invoke, registration order, empty-by-default zero-cost, moved with the object's inline state) as the honest "element destroy" notification. NEW DependencyObject::add_destroyed(handler)->DestroyedToken / remove_destroyed(token) / fire_destroyed() + NEW BindingExpression: detached() + public Detach() (idempotent, no-throw, emits NOTHING - normal teardown, unsubscribes the forward + write-back wires), create() arms target_->add_destroyed (auto-detach on element destroy BEFORE memory reclaim), and WRITE GUARDS in TransferSourceToTarget() + transfer_write_back() (a detached binding emits BindingReasonCode::Detached - Class B silenced non-error per ADR-016 - ONCE and skips WITHOUT writing, never dereferencing a freed target); RAII - the dtor unsubscribes the destroyed wire too, move ctor/assign RE-ARM it onto the new this + mark moved-from inert. NEW test/unit/BindingLifecycle_test.cpp (10 TEST_CASEs, S245-unique DPs + real ObservableObject sources): fresh not-detached; explicit Detach() -> detached + both wires unsubscribed + zero emissions + idempotent; HEADLINE destroying the target auto-detaches -> later source change emits Detached exactly once + no dangling write; source change after explicit Detach() -> Detached once + target untouched; target edit after Detach() -> source untouched + no false emission; OneWay parity; OneTime parity; Detached is Class B + binding_class_of; move re-arms the destroyed wire; dtor leaves no dangling callback. NEW build/verify_s245.cmd + MOD src/core/include/vaml/core/DependencyObject.h + MOD src/core/src/DependencyObject.cpp + MOD src/core/include/vaml/core/BindingExpression.h + MOD src/core/src/BindingExpression.cpp + test/unit/CMakeLists.txt. BOTH CI LANES green from first principles: windows-msvc (verify_s245.cmd msvc-debug warning-as-error CLEAN + ctest 3/3 PASS incl. the S245 suite, prior suites green) + ubuntu-clang (/mnt/hgfs/VamlPOC clang-debug: build warnings==0 + ctest 3/3 PASS incl. the S245 suite). No commits. NO-GAP (recorded, not silent): resolves the S240_1 Detached -> [fwd->S245] arm + the S243/S244 "S245 (expression lifecycle/detach, the Detached arm) follows" hand-offs IN PLACE; S245_1 (the binding-failure suite asserting exact reasonCodes per Class, incl. Detached as the silenced non-error) + S252 ({Binding} markup) build on this lifecycle. ADR-016 (Detached = silenced observation, never control-flow; Class B non-throwing) + ADR-018: ZERO new locks (the destroyed-subscriber list is single-threaded affinity, mirrors subs_; no new shared state). NO MINIMAL REAL introduced/resolved. [--legacy checkpoint: S244 full digest preserved below--] - Sprint 6 - Binding engine two-way: target→source write-back (TextBox edit → VM updates); converter both directions. S244 DONE + VERIFIED from first principles BOTH CI lanes this session. NEW TARGET→SOURCE WRITE-BACK on vaml::BindingExpression + the convertBack reverse-converter wire + the S240_1 WriteBackRejected forward arm + the S041 self-fire completion + the WPF-faithful IsInTransfer guard. (1) TARGET→SOURCE WRITE-BACK - for mode==TwoWay, `create(spec)` ARMS a SECOND per-property subscription onto the TARGET DependencyObject via S041 add_property_changed(PropertyId(targetProp)) (the "target changed" signal = a TextBox edit landing on the target DP). On a REAL target change the handler runs `transfer_write_back()`: read the target's current value (Get, catching PropertyNotFound) -> run convertBack if present (S244 RESOLVES S242's declared convertBack seam; catching a throwing converter) -> write back to the source via the FIRST PATH SEGMENT's key (source_->Set(PropertyId::of(first_segment_name(path_))); multi-hop write-back re-observation is a S252/S244+ concern, recorded not silent). THE FORWARD (source→target) WIRE for TwoWay is the EXACT S243 shared layer (arm_subscription broadened to arm OneWay AND TwoWay), reused unchanged - no duplication. `live_writeback()` introspection (true only for armed TwoWay; OneWay/OneTime never write back). RAII-bound: unsubscribe_writeback() removes the token; the move ctor/assign transfer BOTH subscriptions (forward + write-back) onto the moved-to `this` + mark the moved-from inert; dtor unsubscribes both. Caller lifetime contract unchanged (a binding must not outlive its source ObservableObject or target DependencyObject). (2) WriteBackRejected EMISSION (the S240_1 [fwd->S244] arm, bound here) - a live TwoWay write-back that is REJECTED (failing convertBack ok=false, no source to write back to, no first-segment source key, or a value-type mismatch on the source Set) emits BindingReasonCode::WriteBackRejected (Class B, observation never control-flow per ADR-016 - never a throw; the target value is NOT written to the source). GATED to the live TwoWay write-back path exactly as S243 gated SetTargetFailed to the live OneWay path - the caller-driven FIRST push + the OneWay/OneTime paths emit NOTHING (S242/S243 parity). WriteBackRejected != SetTargetFailed (a rejected write-back emits only WriteBackRejected; a rejected forward target Set emits only SetTargetFailed - tested distinct). (3) LOOP-SAFETY via BOTH WPF-faithful guards (studied against the authoritative E:\GitRepos\wpf BindingExpression/Base - NeedsUpdate + IsInTransfer) - (a) the real-change discipline already established (S241 source Set false-on-equal -> no re-trigger; S035 target Set NO_CHANGE-on-equal -> no redundant notification) AND (b) the `transferring_` IsInTransfer-equivalent guard: the WHOLE forward transfer (first push AND live re-push) runs with a private `transferring_` flag set (RAII scope-guard in TransferSourceToTarget) and transfer_write_back() short-circuits while it is set (suppressed, never emitted, never stored). (b) closes the non-injective-converter round-trip that (a) alone cannot. A source→target→write-back→source cycle is therefore BOUNDED (proven by a bounded-cycle test). (4) S041 SELF-FIRE COMPLETION - DependencyObject::Set (on-affine real change) AND the pending-drain apply (ProcessPendingChanges) now call a new private fire_self_subscribers(name, old, new) that fires THIS object's own registration-ordered add_property_changed subscribers (copy-before-invoke, same discipline as ProcessNotificationQueue). The S041 header ("handlers fire on an on-affine Set that produced a real change") + S041 title contract (5)(6) always promised this; the S041/S042/S043 suites only drove pushed/queued changes so never exercised it. S244 is the first consumer of the self-change seam + completes the contract (a private-member addition to DependencyObject.h/.cpp; the refactor-culture refactor, not a scope dodge). NEW test/unit/BindingTwoWay_test.cpp (14 TEST_CASEs / 56 assertions, Shouldly vocabulary, S244-unique object-scoped DPs + real ObservableObject sources + a CountingTarget): initial push APPLIED + both wires armed on create; TwoWay LIVE forward (shared S243 wire); HEADLINE target edit -> source VM updated (no manual call); convertBack (forward String→Int32 + a target Int32 edit convertBack'd -> String reaches the source); echo/loop termination (bounded + sane target write count); stability (target edit to equal source: no forward re-trigger); WriteBackRejected on a failing convertBack (once per rejection); WriteBackRejected with no source (Class B, never a throw); WriteBackRejected != SetTargetFailed; OneWay parity (forward only; a target edit does NOT push to source); OneTime inert (neither wire); no-converter two-way round-trip; lifetime/dtor (no dangling callback); move (re-arms BOTH wires). NEW build/verify_s244.cmd + MOD src/core/include/vaml/core/BindingExpression.h + MOD src/core/src/BindingExpression.cpp + MOD src/core/include/vaml/core/DependencyObject.h + MOD src/core/src/DependencyObject.cpp + test/unit/CMakeLists.txt. BOTH CI LANES green from first principles: windows-msvc (verify_s244.cmd msvc-debug warning-as-error CLEAN + ctest 3/3 PASS incl. the S244 suite, prior suites green) + ubuntu-clang (/mnt/hgfs/VamlPOC clang-debug: build warnings==0 + ctest 3/3 PASS incl. the S244 suite; a -Wunused-function caught + fixed). No commits. NO-GAP (recorded, not silent): S245 (expression lifecycle/detach, the Detached arm) follows; S252 ({Binding} markup) builds on this; multi-hop write-back re-observation is a S252/S244+ concern. ADR-016 maintained (WriteBackRejected = observation, never control-flow; Class B non-throwing). ADR-018: ZERO new locks (the write-back subscription fires synchronously inside the target's Set on the target's owner thread; the transferring_ guard is a private bool; no new shared state). NO MINIMAL REAL introduced/resolved.
- Next step to do     : S320 (dep:S292) - Sprint 8 - RoutedEvent primitive: Signal<T> + token + route-policy hook;
  reentrancy-safe add/remove. SPRINT 7 CLOSED (S289..S293): S289 (pack/identity: PackUri + ResourcePack + the
  compiled-only InitializeComponentFrom + x:Class owner mapping), S290 (dual-path contract: S2=dev / S7=release,
  documented + grep-gated), S291 (REFACTOR GATE(S7): full text->vbaml->tree parity, no stubs), S293 (VIEWER-GROWTH
  compiled path: viewer opens a compiled .vbaml via the runtime VBamlReader; reload recompiles .vaml->.vbaml then
  reopens), all verified BOTH CI lanes green from first principles (msvc-debug full 4/4 + clang-debug 4/4,
  warnings==0) + the S292 SURVIVOR wrapped it. The full Sprint 7 block S280..S293 is complete: the VBAML pipeline
  (text -> vamlcc -> .vbaml -> VBamlReader -> tree) is end-to-end GREEN + the compiled path is demonstrated LIVE in
  the viewer. Session-checkpoint regenerated here to Sprint 8 (S320).
- PENDING work        : none (S289 -> S290 -> S291 -> S293 -> S292 fully complete + NO-GAP'd; both CI lanes green from first principles on build/verify_s289.cmd + /mnt/hgfs/VamlPOC clang-debug + the viewer printed path; the gapless dep chain continues at S320 - the Sprint 8 first token).
  **State**: S147 (dep:S146) is the Sprint-3.5 SURVIVOR + checkpoint wrap Ã¢â‚¬â€ DOCS-ONLY (no build, per the
  S009/S049/S097/S139 precedent; both CI lanes were verified green + ASan no-leak at S146 from first
  principles). (1) RELOCATE: Sprint 3.5 (S140Ã¢â‚¬â€œS147) is consolidated into the `## SPRINT 3.5 Ã¢â‚¬â€ SURVIVOR`
  block below (intern/SIMD lockless store S140/S141/S140_1, block-resident collapse S142, mutex
  relocation + registry re-key S143, registry/accessor migration S144, object block pool S145, refactor
  gate + HeapStats/ASan S146). (2) RETARGET verified: S160 already reads `dep:S147` Ã¢â‚¬â€ no roadmap change
  needed; S147 re-confirms its own dependency, so Sprint 4 (S160) now depends on the complete, wrapped
  Sprint 3.5. (3) ALLOCATOR-POLICY ADR recorded as ADR-020 (feeds S722 Ã¢â‚¬â€ the Sprint-3.5 allocation/
  ownership discipline finalized there). (4) THREAD-LOCAL POOL NOTE recorded (feeds S624 the thread-affine
  lockless object pool + intern store pattern for the arena/element-pool work, and S626 the
  copy-on-write multi-UI-thread scale-out rule: cross-thread transfer carries literals/owned strings,
  never interned views). (5) LOCK GOVERNANCE ADR-018 reaffirmed with the explicit Sprint-3.5 accounting:
  ZERO new locks added across S140Ã¢â‚¬â€œS147 Ã¢â‚¬â€ the intern store (S141/S140_1/S140) is thread-local lockless,
  S145's object block pool is thread-local + lockless (thread-affine per S037, so it needs NO lock),
  S146's HeapStats is a relaxed atomic (no mutex), and the ONLY lock action was S143's relocation of the
  standing pre-approved S037/S041 ChangeQueue mutex (moved, not created). The roadmap GUARD RAILS + ADR-LOG
  ADR-018 carry this reaffirmation. Checkpoint regenerated: last = S147, next = S160 (Sprint 4 Ã¢â‚¬â€ Vulkan
  window + swapchain + first pixels). PENDING none. No commits.
- Surviving-state digest: (computed when first build artifact exists; place SHA-256 of
  `build/state/surviving-nodes.ser` here once that pipeline is live)
- Key invariants intact: [ADR-001..012] [layer-rule: core has NO Vulkan/GLFW deps; only vulkan backend touches GPU]
  [no-managed] [no-stub rule] [MINIMAL REAL: vulkanâ†’RESOLVED at S161, toolsâ†’S287, Colorâ†’S089, Thicknessâ†’S089] [NO-GAP GUARANTEE] [NO LAZY LLM] [ADR-018 lock governance: ONLY the human architect approves any lock; Sprint 4 (S160â€“S172) adds ZERO new locks]

## GUARD RAILS (codified from S003 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â mirror of docs/roadmap.md; survives roadmap trimming)
- **MINIMAL REAL**: permanent, linkable, forward-resolvable seed (kept+extended, NOT a stub);
  tagged with searchable sentinel `// [MINIMAL REAL] <comp> ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â RESOLVED BY <token>`; removable only
  by its declaring step. Active: vulkan → RESOLVED at S161 (Sprint 4), toolsÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢S287, built-in element seeds (S087,
  in `vaml/core/Element.h`) ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ ButtonÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢S401, TextÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢S400, TextBoxÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢S402,
  CheckBoxÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢S404, SliderÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢S403, ScrollViewerÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢S405, WindowÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢S440; VamlReader event hooks (S092,
  `vaml/core/VamlReader.h`) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â S322.
  RESOLVED: Color + Thickness (S089 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â their real parsing
  semantics now live on the types via parse_color/parse_thickness); StackPanel (S129 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â real panel);
  Grid (S131 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â real panel: row/col definitions + Absolute/Auto/Star resolution + attached
  Grid.Row/Grid.Column; S132 adds attached Grid.RowSpan/Grid.ColumnSpan + span-aware
  measure/arrange); WrapPanel (S134 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â REAL panel: UIElement-derived, WrapDirection DP default
  Horizontal, flow/wrap/align; a NEW element, not a seed); Canvas (S136 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â REAL panel:
  UIElement-derived, attached Canvas.Left/Canvas.Top DPs (Double, default NaN = "not set"),
  absolute-position measure/arrange; a NEW element, not a seed); **UIElement transform-owner (S120) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â
  RESOLVED at S208** (the S208 Transform.h Matrix2D affine value type + the RenderTransform DP +
  the encoder CPU-fold honors the model/transform; sentinel removed at S208).
- **NO-GAP GUARANTEE**: no feature/symbol left for silent backfill; every MINIMAL REAL has an
  explicit resolution authored at the same step that delivers real content.
- **NO LAZY LLM SYNDROME**: default posture is to reason the opposite of lazy; MINIMAL REAL is
  explicitly NOT lazy ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â it is strategic, documented-by-design.

## SURVIVING STATE (what a fresh LLM must assume is true)
- S000 DONE: directory skeleton created ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â docs/ (adr, design), src/ (core, vulkan, tools, cmake),
  test/ (unit, integration), VamlPOC/ with SoC subfolders {hmi, core, infra, viewmodels}/ for the
  sample APPLICATION (distinct from the library source tree in src/); tracking files present:
  progress.md (ROOT), docs/ROADMAP.md (S000ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Å“S999 tokenized), docs/architecture.md (seed),
  docs/adr/ADR-LOG.md (ADR-001..012), docs/design/GPU-DRIVEN-PRINCIPLE.md, docs/design/DESIGN-SEEDS.md.
- S001 DONE: top-level CMake (project(VAML) 0.1.0, C++20 CXX), vaml_apply_compiler_policy(),
  src/ + src/core CMake, vaml-core STATIC lib with version API (non-stub), CMakePresets.json,
  .gitignore, build/verify_msvc.cmd. VERIFIED: MSVC 19.51, warnings-as-errors ON, vaml-core.lib
  builds+links green. NOTE: no Vulkan/GLFW/vcpkg at S001 (correct ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â core has zero Vulkan deps,
  ADR-011); Next = S002.
- Build system: NOT started (S001 is the first code step).
- Property system, parser, Vulkan, binding, etc.: NOT started. See ROADMAP.md for the full S000ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Å“S999
  tokenized step list. The "no-stub" rule means a step is only ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦ when its behavior is real and tested.

---

---

## SPRINT 0 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â SURVIVOR  (the consolidated surviving state a fresh LLM must assume ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â S009 wrap)
- **Goal met (S000ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Å“S008): green skeleton, zero feature code.** Repo, toolchain, CI, and test
  harness are real and verified; no product feature (property system, parser, renderer, etc.) is
  started. All Sprint-0 gates are ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦.
- **Artifacts / libraries:** three STATIC libs ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â `vaml-core`, `vaml-vulkan`, `vaml-tools`
  (`VAML_BUILD_SHARED` default OFF; shared path proven but not the default). Artifact model +
  ADR-011 layer rule enforced in `cmake/artifact-model.cmake` (`vaml_add_library`, generate_export_header,
  `VAML_LAYER`, `vaml_validate_layer_rules`). Include-surface convention: internal impl headers under
  `src/<lib>/src`, ONLY public API under `src/<lib>/include/vaml/<lib>`.
- **Test harness:** vendored doctest single-header (v2.4.11) + ported `Shouldly.h`
  (`vaml::test` facade) with pluggable failure sink. `vaml-core-tests` EXECUTABLE =
  `smoke_test.cpp` + `shouldly_test.cpp`; `ctest` green 1/1.
- **Build lanes (both verified live):** MSVC VS 2026 (`msvc-debug` / `msvc-debug-shared`,
  warnings-as-errors ON) and real Ubuntu VM Clang 21.1.8 (`clang-debug`) via shared folder
  `/mnt/hgfs/VamlPOC` (cmake 4.2.3, ninja). Presets: `cmake/presets/{base,configure,build,test}.json`
  nested include chain; toolchains under `cmake/toolchains/`. MSVC lane driven via `build/verify_s*.cmd`
  (clean PATH + vcvars64). CI: `.github/workflows/ci.yml` (windows-msvc + ubuntu-clang).
- **MINIMAL REAL seeds carried forward UNRESOLVED (never backfilled in S0):**
  `vaml-vulkan` version marker ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ **S161**; `vaml-tools` version marker ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ **S287**. No other
  MINIMAL REAL active. Both carry searchable sentinel comments.
- **Invariants intact:** [ADR-001..012], layer-rule (core has NO Vulkan/GLFW deps), no-managed,
  no-stub, NO-GAP GUARANTEE, NO LAZY LLM SYNDROME.
- **Next = S030** (Sprint 1, Property system core ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â first token of the real feature work).

---

## SPRINT 1 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â SURVIVOR  (the consolidated surviving state a fresh LLM must assume ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â S030..S048, wrapped at S049)
- **Goal met (S030ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Å“S048): the property-system core is complete, green on both compilers, and clean.
  The "heart" of VAML (full DP semantics) is real, tested, and refactor-gated.** Sprint-1 closing =
  S046 (value hook proofs) ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ S047 (suite-complete gate) ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ S048 (refactor gate) ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ S049 (wrap).
- **Full DP semantics delivered (real public symbols, none MINIMAL REAL / none STUB):**
  `Typeless Value` (11 tags, type-safe get) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â S030; `PropertyKey` + `PropertyRegistry` (nameÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢key,
  name-unique dedupe, pimpl) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â S032; `PropertyId` (constexpr-safe name-token) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â S033;
  `PropertyMetadata` (default/coerce/validate/changed, move-only pimpl) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â S034;
  `DependencyObject` (local store, Get/Set with the S035 pipeline, thread-affinity) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â S036;
  wrong-thread write QUEUING (marshalling) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â S037; `ChangeRecorder` ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â S038;
  global `DependencyProperty::Register` + `GetEffectiveValue` (local ?? inherited ?? default,
  InheritOnly) + parent link ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â S039; inheritance cascade (proof) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â S040;
  `ChangeQueue` + `ChangeProcessor` + registration-ordered subscribers + descendant push ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â S041;
  coalescing (proof) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â S042; ATTACHED properties (ownerÃƒÂ¢Ã¢â‚¬Â°Ã‚Â class, metadata-provider borrowed) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â S043;
  attached inheritance + notification (the passive-descendant gap-fix) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â S044;
  ReadOnly enforcement + validation + all-flags ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â S045; coercion + per-set-validation value-hook
  proofs (attached/global axis + wrong-thread-queued-coercion) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â S046.
- **Assertion vocabulary (standing convention):** the *Shouldly-for-doctest* vocabulary ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â the
  SHOULD/SHOULD_REQUIRE predicate seam + doctest StringMaker + `ShouldThrow<Ex>` ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â is adopted
  suite-wide from S036_5 onward; S047 certified ZERO unadapted feature `CHECK`/`CHECK_THROWS_AS`
  in every migrated feature file. Sanctioned non-feature exceptions: PropertyMetadata_semantics
  (the S035 spec harness), smoke_test (runner), StringMaker_test (stringifier test), shouldly_test
  (predicate/routing library test), REQUIRE null-deref precondition guards.
- **Key design decisions recorded (searchable, NO-GAP / NO-LAZY-LLM):** internal store keys on an
  OWNED std::string, not the non-owning PropertyId view; coerceÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢validate(coerced)ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢fire-on-real-
  changeÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢store pipeline contract (PropertyMetadata_semantics_test MiniStore is the reference spec);
  the move-only PropertyMetadata is NEVER copied into an object's entry (attached/borrowed
  PropEntry); descendant push carries each descendant's own effective-oldÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢effective-new delta
  (local-wins respected through an intermediate); S046 REDIRECT (recorded): the registry-level
  "ValidateDependencies" graph is INERT for real WPF MVVM (VM composition is instance-level, not
  registry linkage) ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ replaced by WPF-faithful coercion + per-set validation value hooks.
- **Deferred to future sprints (all recorded, NOT silent):** BindsTwoWayByDefault runtime behavior ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢
  S242; owner-internal read-only write path ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ S120+ controls; visual tree / UIElement (derives
  DependencyObject) ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ S120; full store-race freedom (TSan) ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ S620/S723; genuine two-way binding ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢
  S242.
- **MINIMAL REAL carried forward RESOLVED/unresolved:** NONE resolved in Sprint 1. Still unresolved:
  `vaml-vulkan` ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ S161; `vaml-tools` ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ S287; `Color`/`Thickness` (S030) ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ S089. All carry searchable
  sentinel comments; NO-GAP intact.
- **Invariants intact:** [ADR-001..012], layer-rule (core has NO Vulkan/GLFW deps), no-managed,
  no-stub, NO-GAP GUARANTEE, NO LAZY LLM SYNDROME.
- **Build lanes both green (from first principles):** windows-msvc (msvc-debug via build/verify_s*.cmd,
  VS 2026) + ubuntu-clang (clang-debug via /mnt/hgfs/VamlPOC, Ubuntu VM clang 21) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â configure/build
  warning-as-error clean + ctest 1/1 PASS. windows-clang-debug is NOT a lane.
- **Next = S080** (Sprint 2, VAML markup parser ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â XML scanner; the first token of the next sprint).

---

## SPRINT 2 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â SURVIVOR  (the consolidated surviving state a fresh LLM must assume ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â S080..S096, wrapped at S097)
- **Goal met (S080ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Å“S096): the VAML markup parser + runtime reader (text ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ ParsedElement tree ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢
  live DependencyObject tree) is complete, green on both compilers, and refactor-gated.** The
  runtime path is FIRST ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â Sprint 2 delivers textÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢treeÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢live objects before the compiled VBAML path
  (Sprint 7). Sprint-2 closing = S095 (golden-doc full suite) ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ S096 (refactor gate) ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ S097 (wrap).
- **Full parser/reader pipeline delivered (real public symbols, none MINIMAL REAL / none STUB):**
  `XmlScanner` (elements, attributes, self-closing, comments, CDATA, entities, nested tags, per-element
  namespace scope, x-language directive classification, attribute-value grammar) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â S080ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Å“S084;
  `XmlScanException` structured scan-error taxonomy (19 categories, 1:1 per malformed path, line/col) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â
  S081; `ParsedElement`/`ParsedAttribute` + `parse_vaml()` (tags + children + content tree) +
  `XmlParseException::UnterminatedElement` ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â S085; round-trip golden (S086); `ElementFactoryRegistry` +
  `Element` MINIMAL REAL seeds + `register_builtin_elements()` ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â S087; `resolve_element()` +
  `ElementResolutionException(UnknownType)` naming type+line ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â S088; `ValueConverterRegistry` +
  `register_builtin_converters()` (Int32/Thickness/Color/Bool) + `parse_color`/`parse_thickness`
  (RESOLVES the S030 Color/Thickness markers) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â S089; `VamlReaderError::UnknownLiteral` +
  `try_infer_type` ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â S090; `VamlReader::Load(text/ParsedElement)` live tree builder + retained_objects
  + parent links + unknown-property baseline ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â S091; x:Name/x:Class registry + deferred markup
  extensions (S2 tag) + MINIMAL REAL event hooks + DuplicateName ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â S092; integration gates ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â S093/S094
  (S093 no-hooks pure-text; S094 error-taxonomy consolidation + `ClassOnNonRoot`); FULL-SUITE golden
  docs ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â S095.
- **Golden-doc full suite (S095, the capstone):** `GoldenDocs_test.cpp` asserts the ENTIRE S2 pipe
  (`text ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ parse_vaml ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ ParsedElement ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ Load ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ live tree`) on realistic documents across FOUR axes ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â
  parser golden canonical string, live reader tree (types/retained order/parent chain/x:Name registry/
  root_class/converted values incl. attached), deferred surface (MerkupExtension {Binding} retained
  verbatim, event handler routed, x:Key deferred), and live text-path==parsed-path parity ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â plus a
  golden B error-taxonomy gate (unknown element/attribute, malformed value, duplicate x:Name, x:Class
  on non-root each fire the RIGHT structured exception) and a clean-minimal no-hooks/no-extensions
  control. TEST-ONLY, zero new product symbols.
- **Key design decisions recorded (searchable, NO-GAP / NO-LAZY-LLM):** the scanner is hand-rolled
  (no external XML lib ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â no manual XML dep, verified at S096); namespaces resolve on the URI not the
  letter "x" (any prefix mapped to kXamlLanguageNamespace is the x-language); attached-style dots
  (Grid.Row) are NOT namespace separators; markup-extension detection honors balanced braces;
  ParsedElement is tags+children+content (not a pseudo-element sentinel) so readers distinguish text
  from elements at the type level; element type resolution keyed on local_name (so `<vm:Button>`
  resolves "Button"); "carried, not re-scanned" ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â the tree carries the scanner's classified metadata;
  `resolve_element` is the SINGLE-ELEMENT primitive, consumed by build(); `ValueConversionError` has
  three orthogonal categories (UnknownType/MalformedValue/UnknownLiteral); the reader routes registered
  event keys to DeferredEventHandlers and records isExtension values as DeferredExtensions (the "S2
  tag" {Binding}, resolved by S252).
- **Deferred to future sprints (all recorded, NOT silent):** {Binding} markup resolution ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ S252/S253;
  x:Key resource-key ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ S361; x:Null ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ S084 (tagged, already deferred); event-handler wiring ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ S322;
  VBAML compiled path ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ Sprint 7; re-serializer/serializer ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ S282/S283; visual-tree children list ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢
  S121; real control logic (from the element seeds) ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ each control's sprint (S129/S131/S400-S405/S440).
- **MINIMAL REAL carried forward RESOLVED/unresolved:** RESOLVED in Sprint 2 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â Color + Thickness
  (S089, parse_color/parse_thickness, sentinels removed). Introduced + still UNRESOLVED: built-in
  element seeds (S087, Element.h) ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ ButtonÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢S401, StackPanelÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢S129, GridÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢S131, TextÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢S400, TextBoxÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢S402,
  CheckBoxÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢S404, SliderÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢S403, ScrollViewerÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢S405, WindowÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢S440; VamlReader event hooks (S092) ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ S322.
  Still unresolved (pre-existing): vulkanÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢S161, toolsÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢S287. All carry searchable sentinels; NO-GAP intact.
- **Invariants intact:** [ADR-001..012], layer-rule (core has NO Vulkan/GLFW deps), no-managed,
  no-stub, NO-GAP GUARANTEE, NO LAZY LLM SYNDROME.
- **Build lanes both green (from first principles):** windows-msvc (msvc-debug via build/verify_s*.cmd,
  VS 2026) + ubuntu-clang (clang-debug via /mnt/hgfs/VamlPOC, Ubuntu VM clang 21) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â configure/build
  warning-as-error clean (except the pre-existing, downgraded upstream <ciso646> doctest deprecation) +
  ctest 1/1 PASS through the S096 gate.
- **Next = S120** (Sprint 3, element/scene graph + measure/arrange ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â UIElement base, first token).

---

## SPRINT 3 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â SURVIVOR  (the consolidated surviving state a fresh LLM must assume ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â S120..S138, wrapped at S139)
- **Goal met (S120ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Å“S138): the element/scene-graph + measure/arrange layout system is complete, green on
  both compilers, and refactor-gated. UIElement base + visual tree + a WPF-faithful infinity-aware
  Measure/Arrange contract + four REAL panels (StackPanel/Grid/WrapPanel/Canvas) + a dirty-flag
  LayoutManager driver, all real + tested + green on both CI lanes.** Sprint-3 closing = S136 (Canvas
  product) ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ S137 (Canvas tests) ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ S138 (refactor gate) ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ S139 (wrap).
- **Full element/layout system delivered (real public symbols, none MINIMAL REAL / none STUB):**
  layout value primitives ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â `LayoutSlot.h` + `Size`/`Rect`/`layout_slot` + `compute_final_rect(slot,
  desired, margin, hAlign, vAlign)` (the arrange math: origin advances + slot shrinks by margin,
  non-Stretch pins inside) + `clip_to_bounds(content, clipBounds)` (rectangular render-slot clip) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â
  S122/S124; `UIElement` base (derives DependencyObject; parent link, visual children + traversal,
  `Margin`/`HorizontalAlignment`/`VerticalAlignment`/`Width`/`Height`/`MinWidth`/`MaxWidth`/ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â¦ DPs,
  infinity-aware `Measure(available)` finalizer `compute_measured_desired` ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â finite-fills / unbounded-
  wraps / non-Stretch clamps to min(natural,available), the S123 stretch-vs-wrap rule) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â
  S120ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Å“S123/S126; the protected virtual `MeasureOverride(const Size&)` (natural content extent) +
  `ArrangeOverride(const Rect&)` (auto-drive: `Arrange` computes final_rect then auto-invokes
  ArrangeOverride) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â S127; the `LayoutManager` dirty-flag driver (`InvalidateMeasure`/`InvalidateArrange`
  propagate up-to-root with short-circuit, `UpdateLayout` runs a measure pass iff root measure-dirty +
  arrange pass iff root arrange-dirty, TRUE no-op when clean) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â S126; **REAL panels**: `StackPanel`
  (Orientation DP default Vertical, last-child-stretch, dirty-only re-measure/re-arrange) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â S129/S130;
  `Grid` (GridLength {Auto/Pixel/Star} + RowDefinition/ColumnDefinition, resolve_grid_lengths, attached
  Grid.Row/Grid.Column, Absolute/Auto/Star resolution, attached Grid.RowSpan/Grid.ColumnSpan + span-aware
  two-pass D1/D2 math, dirty-only) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â S131/S132/S133; `WrapPanel` (WrapDirection DP default Horizontal,
  natural-extent flow/wrap/align, aligned rows, dirty-only) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â S134/S135; `Canvas` (attached Left/Top
  Double DPs default **NaN = "not set"**; absolute-position measure/arrange; bounding-extent DesiredSize
  clamped >= 0; dirty-only) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â S136/S137.
- **Key design decisions recorded (searchable, NO-GAP / NO-LAZY-LLM):** the Infinity sentinel is the
  "available is unconstrained" marker and is guarded by `is_infinite()` in the finalizers; `Measure` is
  WPF-faithful margin handling (shrink available by Margin ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ MeasureOverride(inner) ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ finalize ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ add
  Margin back into DesiredSize, clamped to available) and `compute_final_rect` strips the margin back
  out (margin never counted twice) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â the S130_1 resolution; the Arrange auto-drive (ArrangeOverride
  receiving final_rect) is what lets LayoutManager arrange a whole subtree from the root alone; dirty-
  confinement: a child invalidation re-measures/re-arranges only that child's subtree-influence (leaf +
  ancestor chain), never friend subtrees or clean siblings (S128 proof on R>(P1>(A,B),P2>(C,D)) +
  4-level chain); each REAL panel adopts the dirty-only-subpass discipline inside MeasureOverride/
  ArrangeOverride (re-measure/re-arrange ONLY is_*_dirty() children, reuse clean desired_size/final_rect);
  four REAL panels share one pattern: measure offers each child, derives panel DesiredSize from children,
  arrange places each child into its slot honoring alignment+margin via child->Arrange.
- **Deferred to future sprints (all recorded, NOT silent):** `Canvas`'s and `ScrollViewer`'s render-slot
  clip consumption ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ S208 (renderer) / S405 (ScrollViewer); `Text`/`Button`/etc. real control logic from
  the S087 element seeds ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ each control's sprint (S400ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Å“S405/S440); visual-tree children list already
  delivered S121; transform-owner ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ S208; the S209 renderer consumes final_rect() as render slot +
  clip_to_bounds.
- **MINIMAL REAL carried forward RESOLVED/unresolved:** RESOLVED in Sprint 3 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â `StackPanel` (S129, real
  panel, sentinel removed), `Grid` (S131, real panel + S132 spans, sentinel removed), `WrapPanel` (S134,
  real panel, NEW element no sentinel), `Canvas` (S136, real panel, NEW element no sentinel). Still
  UNRESOLVED (carried forward): `vulkan`ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢S161; `tools`ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢S287; built-in element seeds ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ ButtonÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢S401,
  TextÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢S400, TextBoxÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢S402, CheckBoxÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢S404, SliderÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢S403, ScrollViewerÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢S405, WindowÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢S440; VamlReader event
  hooksÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢S322; UIElement transform-ownerÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢S208. All carry searchable sentinels; NO-GAP intact.
- **Invariants intact:** [ADR-001..012], layer-rule (core has NO Vulkan/GLFW deps), no-managed, no-stub,
  NO-GAP GUARANTEE, NO LAZY LLM SYNDROME.
- **Build lanes both green (from first principles, at the S138 gate):** windows-msvc (msvc-debug via
  build/verify_s137.cmd, VS 2026, warning-as-error clean + ctest 1/1 PASS) + ubuntu-clang (clang-debug
  via /mnt/hgfs/VamlPOC, Ubuntu VM clang 21, ctest 1/1 PASS; only pre-existing upstream <ciso646> doctest
  deprecation from third_party, non-error). windows-clang-debug is NOT a lane (not built/tested/flagged
  per standing rule).
- **Next = S140** (Sprint 3.5, Allocation & ownership discipline retrofit ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â intern property names to a
  canonical UTF-8 identity; the first token of the next sprint).

## SPRINT 3.5 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â SURVIVOR  (the consolidated surviving state a fresh LLM must assume ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â S140..S147, wrapped at S147)
- **Goal met (S140ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Å“S147): the allocation & ownership discipline retrofit eliminated the two orthogonal
  sources of heap churn in the shipped hot paths (per-object make_unique<Impl>() churn + transient
  std::string(id.name()) per lookup) before the renderer (S210/S211) and binding engine (S240+) are built
  on them. Sprint 3.5 closed with an ADR-recorded allocator policy (ADR-020, feeds S722), a
  thread-local-pool note (feeds S624/S626), and an explicit ZERO-new-locks reaffirmation of ADR-018.**
  Sprint-3.5 closing = S145 (object block pool) ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ S146 (refactor gate + HeapStats + ASan) ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ S147 (wrap).
- **Full allocation retrofit delivered (real public symbols, none STUB):**
  `InternStore` (the CONTENT-keyed SIMD probe store: insertion-ordered append-only array, 32-byte/32-lane
  groups, empty marker 0xFF, probe = fold_bytes % 255, memmove+append growth, never re-probe/reorder/shrink;
  thread-local + LOCKLESS by construction) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â S141; the PUBLIC `vaml::intern(string_view) -> string_view`
  retain-forever retained-string primitive (SIMD-probe by value + full compare on the calling thread's
  store ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â HIT returns the existing view, MISS retains+inserts+returns the new view; lifetime contract:
  retained for the life of the interning thread, do NOT pass interned views across threads) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â S140_1;
  property names + parser/reader tree text interned to canonical UTF-8 identity (kills the per-lookup
  transient std::string at Get/Set/effective/prop-subscribe/drain + the parser owned-string churn) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â S140;
  `DependencyObject`/`UIElement` pimpl + store COLLAPSED into ONE block-resident unit (object header ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢
  block-slot offset, no per-object heap hop; single PD type; store inline-started in the slot, heap-grown
  on demand; pimpl removed NOT preserved, rationale re-grounded on SPEED per ADR-003 revised + ADR-019) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â
  S142; the per-object pending mutex RELOCATED into the thread-affine ChangeQueue + ChangeQueue/
  ChangeRecorder/PropertyRegistry/DependencyProperty::GlobalTable re-keyed onto the S140 token + S141
  store via `vaml::detail::NameTable<T>` (content-keyed probe store, no unordered_map, self-owned stable
  keys, no transient std::string on lookup) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â S143 (the kept S037/S041 ChangeQueue mutex, moved not
  created ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â ADR-018); the three lookup registries migrated to the token-keyed store (ElementFactoryRegistry,
  ValueConverterRegistry, DependencyProperty global table) + public accessors return string_view /
  out-params (registered_names(), VamlReader::names(), UIElement::children(),
  collect_descendant_effectives) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â S144; the thread-local LOCKLESS object block pool free-list
  (`vaml::detail::ObjectPool`, class-scoped operator new/delete on DependencyObject routing every
  allocation/deallocation beneath the unchanged unique_ptr ownership, dole from blocks / recycle to
  free-list / never delete, opt-in master_destroy_all, the S142 inline SIMD store rides the block slot) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â
  S145; the Sprint-3.5 REFACTOR GATE (ALL remaining pimpls collapsed; the last unordered_map node subs_
  re-keyed to interned pool-stable views; parser/scanner owned-string members audited as internal booking
  buffers; public-API string_view/intern-only audit; NEW public `vaml::intern_master_destroy_all()` +
  `vaml::detail::HeapStats` + public `vaml::heap_allocation_count()/heap_allocation_bytes()` Ã¢â‚¬â€ the
  steady-state heap-churn counter feeding S628, funneled ONLY on the VAML-owned monotonic growth paths;
  ASan zero-leak via an exit-time pool-teardown hook) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â S146.
- **Key design decisions recorded (ADR-020 Ã¢â‚¬â€ the allocator policy, feeds S722):** the standing
  allocation/ownership discipline is now (1) content-keyed registries Ã¢â‚¬â€ SIMD by-value full compare,
  NEVER pointer-identity (a caller's address is never a usable pre-check; cross-thread interned views are
  compared by content only); (2) `vaml::intern()` is a retained-forever THREAD-LOCAL pool Ã¢â‚¬â€ bounded
  vocabulary + parsed document text only (WARNING: unbounded/reclaimed-never per thread; do NOT intern
  unbounded or unpredictable-valued data); (3) the object pool is THREAD-LOCAL + LOCKLESS (thread-affine
  per S037/ADR-018, so it needs NO lock, unlike the rejected process-global string-pool framing) with
  class-scoped new/delete beneath unique_ptr ownership, slab reuse never delete; (4) RECYCLE == SLAB
  REUSE, NOT STATE CARRYOVER (dtor tears down internal state normally, only the object bytes are reused);
  (5) HeapStats is funneled on MONOTONIC growth ONLY (InternStore arena/entry growth + ObjectPool block
  spill), never recycle/dedup-hit, so steady state is provably FLAT. Cross-thread discipline is
  LITERALS / OWNED STRINGS, never interned views. This policy is finalized as an ADR at S722.
- **THREAD-LOCAL POOL NOTE (feeds S624/S626):** the S141/S140_1 intern store + the S145 ObjectPool are
  both THREAD-LOCAL + LOCKLESS. For S624 (the per-frame arena allocator + element pool + the S142
  "children slab" re-scheduled here) this means the arena/element-pool MUST stay thread-affine lockless
  (mirror the ObjectPool pattern; the children-slab is a distinct low-level data-structure change, kept
  here per the S145 NO-GAP record). For S626 (thread vs render ring scale-out: multiple UI threads,
  copy-on-write hot nodes) the rule is binding: copy-on-write hot nodes MUST stay thread-affine Ã¢â‚¬â€
  cross-thread transfer carries literals / owned strings, NEVER interned views (an interned view is valid
  only on the thread that interned it; carrying it across threads is a lifetime bug). This is the same
  thread-affinity discipline S037/S145 already enforce.
- **LOCK GOVERNANCE ADR-018 reaffirmed (Sprint 3.5 accounting):** Sprint 3.5 (S140ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Å“S147) added ZERO
  new locks by design Ã¢â‚¬â€ the intern subsystem (S141/S140_1/S140) is thread-local lockless, S145's object
  block pool is thread-local + lockless (thread-affine per S037, needs NO lock), S146's HeapStats is a
  relaxed atomic (no mutex), and the ONLY lock action across the sprint was S143's relocation of the
  standing pre-approved S037/S041 ChangeQueue mutex (moved, NOT created). Any future lock (e.g. S620
  async layout) requires the human-architect approval gate.
- **Deferred to future sprints (all recorded, NOT silent):** the children-slab container (S142
  NO-GAP) ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ re-scheduled to S624 (per the S145 record); allocator-policy ADR FINALIZATION ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ S722;
  memory profile gate / bounded memory (ASan) ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ S628; TSan race-freedom ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ S620/S723.
- **MINIMAL REAL carried forward RESOLVED/unresolved:** NONE resolved in Sprint 3.5. Still unresolved
  (carried forward): `vulkan`ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢S161; `tools`ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢S287; built-in element seeds ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ ButtonÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢S401, TextÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢S400,
  TextBoxÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢S402, CheckBoxÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢S404, SliderÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢S403, ScrollViewerÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢S405, WindowÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢S440; VamlReader event
  hooksÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢S322; UIElement transform-ownerÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢S208. All carry searchable sentinels; NO-GAP intact.
- **Invariants intact:** [ADR-001..019] (incl. ADR-020 allocator policy, ADR-018 lock governance),
  layer-rule (core has NO Vulkan/GLFW deps), no-managed, no-stub, NO-GAP GUARANTEE, NO LAZY LLM
  SYNDROME.
- **Build lanes both green (from first principles, at the S146 gate):** windows-msvc (msvc-debug via
  build/verify_s146.cmd, VS 2026, warning-as-error clean + ctest 1/1 PASS) + ubuntu-clang (clang-debug
  via /mnt/hgfs/VamlPOC, Ubuntu VM clang 21, ctest 1/1 PASS) + ASan zero-leak both compilers
  (build/verify_s146_asan.cmd); only pre-existing upstream <ciso646> doctest deprecation from
  third_party, non-error. windows-clang-debug is NOT a lane (not built/tested/flagged per standing rule).
- **Next = S160** (Sprint 4, Vulkan window + swapchain + first pixels ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â the first token of the next sprint;
  S160's dep re-targeted to S147).

### Sprint 0 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â Repo, toolchain, CI, test harness   (S000ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Å“S019)
- [x] S000 repo skeleton + tracking files (this file, ROADMAP.md, ADR seeds, architecture.md) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦
  SURVIVOR: layout is docs/{adr,design}/, src/{core,vulkan,tools,cmake}/, test/{unit,integration}/,
  VamlPOC/{app+hmi,core,infra,viewmodels}/. progress.md lives at repo ROOT (lowercase). Next = S001.
- [x] S001 top-level CMake + compiler policy + vaml-core baseline ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦
  SURVIVOR: CMakeLists.txt (project(VAML) 0.1.0, C++20, /W4 /permissive- /EHsc /WX MSVC;
  -Wall -Wextra -Wpedantic -Werror Clang), vaml_apply_compiler_policy() helper, src/CMakeLists.txt,
  src/core/CMakeLists.txt (vaml-core STATIC), include/vaml/version.h (Version + version() function),
  src/version.cpp, CMakePresets.json (msvc/clang), .gitignore, build/verify_msvc.cmd. Build verified
  MSVC 19.51 warnings-as-errors ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ vaml-core.lib links. No Vulkan at S001 (correct, ADR-011).
  GOTCHA: use vcvars64.bat before cl; use VS-bundled cmake.exe + ninja.exe (not on base PATH).
- [x] S002 cmake/ toolchain files + compiler presets (MSVC VS2026, Linux Clang) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦
  SURVIVOR: policy extracted to cmake/compiler-policy.cmake (vaml_apply_compiler_policy, flags
  byte-identical to S001: MSVC /W4 /permissive- /EHsc /Zc:* /WX + /wd4100; Clang -Wall -Wextra
  -Wpedantic -Werror). Toolchains: cmake/toolchains/{msvc-vs2026,linux-clang,windows-clang}.cmake.
  Presets split into cmake/presets/{base,configure,build,test}.json (Alt B) with a nested include
  CHAIN (base<-configure<-build<-test, root includes cmake/presets/test.json) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â siblings do NOT
  cross-inherit in CMake. Root CMakePresets.json is a thin include stub. Presets anchored to
  toolchainFiles: msvc-debug/release->msvc-vs2026, clang-debug->linux-clang,
  windows-clang-debug->windows-clang. VERIFIED: clean configure --preset msvc-debug via include
  chain + toolchainFile -> MSVC 19.51, /WX active, vaml-core.lib builds green. Linux + windows-clang
  toolchains are static (no-build-on-host; review-validated only). Next = S003.
  GOTCHA: Ninja generator does NOT support CMAKE_GENERATOR_TOOLSET (MSVC toolchain file must NOT pin
  a toolset; vcvars64 selects it). GOTCHA: preset inheritance must be a nested include chain, not
  sibling includes.

### (remaining S003ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Å“S999 steps live in docs/ROADMAP.md, the authoritative tokenized list)

- [x] S003 static-library artifact model + guard rails ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦
  SURVIVOR: cmake/artifact-model.cmake (NEW ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â vaml_add_library() helper, VAML_BUILD_SHARED
  default OFF, generate_export_header per lib, OUTPUT_NAME vaml-*, VAML_LAYER tag,
  vaml_validate_layer_rules() ADR-011 guard). Top-level CMakeLists includes artifact-model +
  calls vaml_validate_layer_rules() after add_subdirectory(src). src/core/CMakeLists.txt refactored
  to vaml_add_library(core). src/vulkan/CMakeLists.txt + src/tools/CMakeLists.txt NEW, un-commented
  in src/CMakeLists.txt; each builds a version-marker MINIMAL REAL lib. Libs now: vaml-core,
  vaml-vulkan, vaml-tools. Generated export headers committed at
  src/{core,vulkan,tools}/include/vaml/{core,vulkan,tools}/export.h (VAML_{CORE,VULKAN,TOOLS}_EXPORT;
  empty when static). Presets: msvc-debug-shared added (configure/build/test) to prove shared path.
  VERIFIED: static (`msvc-debug`) builds vaml-core.lib+vaml-vulkan.lib+vaml-tools.lib green, /WX ON;
  shared (`msvc-debug-shared`) builds vaml-core.dll/vulkan/tools green. ADR-011 guard negative-tested
  (vaml-core linking vaml-vulkan ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ configure FATAL "ADR-011 VIOLATION", then reverted to green).
  Linux + windows-clang static: review-validated only (no host build; consistent with S002).
  GUARD-RAIL CODIFICATION: MINIMAL REAL, NO-GAP GUARANTEE, NO LAZY LLM SYNDROME terms added to both
  docs/roadmap.md (Guard Rails section) + progress.md (Guard Rails block); S003/S161/S287 lines in
  roadmap carry the forward-declared resolution contracts (vulkanÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢S161, toolsÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢S287). No new md files.
  Next = S004.
  GOTCHA: layer guard introspects LINK_LIBRARIES (direct edges) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â sufficient because GLFW/Vulkan are
  only reachable via vaml-vulkan in this project. GOTCHA: export.h generated into the PUBLIC include
  tree (absolute EXPORT_FILE_NAME) so targets consumers include via vaml/{lib}/export.h.
- [x] S004 CI matrices (.github/workflows) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦
  SURVIVOR: .github/workflows/ci.yml (NEW, sole deliverable of S004). Single matrix-driven file, two
  live jobs exactly matching ADR-007 core matrix: windows-msvc (windows-latest, VS 2026 cl.exe) ->
  preset msvc-debug; ubuntu-clang (ubuntu-latest, clang++) -> preset clang-debug. Both jobs run the
  identical checkout -> install toolchain -> cmake --preset -> cmake --build --preset -> ctest
  --preset --output-on-failure sequence, reusing S002/S003 presets verbatim (msvc-debug/clang-debug
  are named identically across configure/build/test). MSVC lane: microsoft/setup-msbuild (cl.exe +
  vcvars on PATH) + choco install ninja -y; Linux: apt clang + ninja-build. Guards: per-job
  timeout-minutes=30, warnings-as-errors ON via presets, no pinned runner labels (rolling -latest,
  keeps MSVC lane on same VS as local dev + follows platform). NO windows-clang lane in any form;
  NOT commented into YAML (toolchain seed stays unbuilt in cmake/toolchains/). Non-goals: no
  artifacts, no coverage, no packaging (future sprints own those). VERIFICATION: MSVC lane
  reproducible locally against the same msvc-debug preset (VS 2026) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â re-ran clean, catches preset
  name typos pre-push. Ubuntu lane = no-build-on-host; acceptance is GH Actions on push/PR (free for
  public repos). All preset names in the workflow cross-checked against configure/build/test.json.
  Roadmap S004 line marked [DONE]. No build-system/feature code changed. Next = S005.
  GOTCHA: a workflow depends on preset NAMES being identical across configure/build/test JSON ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â
  CI calls only --preset <name>, never CMake directly, so a rename in presets silently breaks a lane
  (checked: msvc-debug + clang-debug are consistent). GOTCHA: MSVC lane needs the VS dev shell env
  (cl.exe), so setup-msbuild MUST precede cmake --preset; base.json sets Ninja generator so Ninja
  must be installed (choco) before configure.
- [x] S005 vendor doctest + test harness + 0-feature smoke test ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦
  SURVIVOR: third_party/doctest/doctest.h (NEW ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â official doctest SINGLE HEADER, pinned v2.4.11,
  vendored VERBATIM, no upstream edits; SHA-256
  44FAA038E9C3F9728EFBDA143748D01124EA0A27F4BF78F35A15D8FAB2E039FB ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â regenerate from the
  https://raw.githubusercontent.com/doctest/doctest/v2.4.11/doctest/doctest.h tag, never hand-edit).
  test/CMakeLists.txt + test/unit/CMakeLists.txt (+NEW): single EXECUTABLE vaml-core-tests
  from smoke_test.cpp, links vaml-core (proves consumer-link), vaml_apply_compiler_policy(),
  registered via add_test(NAME vaml-core-tests). test/unit/smoke_test.cpp (NEW):
  DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN + TEST_CASE("smoke") with 3 CHECKs (true, version().major==0,
  engine_name()) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â exercises doctest + the vaml-core version symbol; 0-feature by design.
  Top-level CMakeLists.txt (MOD): enable_testing() + add_subdirectory(test) after
  vaml_validate_layer_rules(); test subtree contributes no library symbols. verify_s005.cmd +
  verify_s005_shared.cmd (NEW in build/, mirror S004 pattern; resets PATH so VS-bundled cmake/ninja
  win over Cygwin PATH). VERIFIED BOTH COMPILERS: msvc-debug (VS 2026) configure+build+ctest green
  (1/1 PASS, doctest 2.4.11, 3/3 assertions), and REAL LINUX clang-debug on the Ubuntu VM
  (clang 21.1.8, cmake 4.2.3) via shared folder /mnt/hgfs/VamlPOC green (1/1 PASS). msvc-debug-shared
  still configures (no S003 regression). NOT a STUB / NOT MINIMAL REAL: doctest is permanent test
  infra, 0 product symbols introduced, so no sentinel + no resolution token (recorded here so it is
  not mistaken for a silent backfill). Per-case discovery via doctest_discover_tests deferred as a
  documented follow-up, not a silent omission. Next = S006.
  GOTCHA: vendored/3rd-party headers must be EXCLUDED from the strict /WX bar on both compilers.
  MSVC: mark third_party SYSTEM + /external:W0 (doctest triggers benign C5285 std::tuple
  specialization false-positive on new MSVC). Clang: -isystem already silences normal warnings but
  `#warning` directives are ALWAYS reported ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â upstream doctest includes compiler <ciso646> (carries
  a benign C++20 `#warning`; doctest uses it only for libc++ detection, we use libstdc++) so add
  -Wno-error=#warnings to the test target. GOTCHA: in CMakeLists, quote CMake arguments containing
  `#` (`"-Wno-error=#warnings"`) or CMake parses them as comments ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ "Function missing ending )".
  GOTCHA: build must be run from a clean PATH + vcvars64 (VS-bundled cmake/ninja); a stale
  msvc-debug cache created under a Cygwin PATH must be deleted before reconfigure (cygdrive path
  mismatch in CMakeCache.txt).
- [x] S006 port user Shouldly.h -> src/core/include/vaml/assert/Shouldly.h (namespace vaml::test) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦
  SURVIVOR: src/core/include/vaml/assert/Shouldly.h (NEW ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â header-only port of the user's
  PiuGlobalsCpp::NsShouldly Shouldly file; the spec's "src/core/assert/Shouldly.h" lands at
  src/core/include/vaml/assert/Shouldly.h to match the project's public-include convention).
  Namespace vaml::test (NOT vaml::assert ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â `assert` is a function-like macro from <cassert>, so
  `namespace assert {}` is ill-formed if <cassert> loads; `assert` as a dir name is fine).
  STRIPPED: MSTest (Microsoft::...::Assert::Fail + wide-string AssertStdString path), Windows bits
  (_wcsdup/wchar_t/free conversions + SHOULDLY_USES_AWESOMELY toggle -> failure now unconditional),
  __forceinline/__inline hints (plain inline), dead/commented code (commented-out Actual overloads,
  unused DynString + is_str helpers, AwesomelyException superseded). KEPT: portable
  #pragma push_macro("max")/#undef max guard. ADDED (only API): TestFailureException : std::exception,
  FailureSink, DefaultFailureSink (throw TestFailureException), failure_sink() (thread_local-static-
  backed settable singleton ref), ReportFailure(string) -> routes to failure_sink()(). All predicate
  names preserved: ShouldBe, ShouldNotBe, ShouldBeGreaterThan, ShouldBeLessThan,
  ShouldBeGreaterThanOrEqualTo, ShouldBeLessThanOrEqualTo, ShouldBeInRangeTo, ShouldBeNull,
  ShouldNotBeNull, ShouldBeNullOrEmpty, ShouldNotBeNullOrEmpty, ShouldNotBeNullOrEmptyOrBe,
  ShouldNotBeNullOrEmptyButBe, ShouldNotBeNullOrEmptyButBeNearlyEqual, ShouldNotBeNullOrEmptyButMatchContents
  + Diff; factories Integral/String/Pointer/Array + macros ShouldlyIntegral/ShouldlyString/
  ShouldlyPointer/ShouldlyArray (retargeted to vaml::test). No CMake change (header lands in existing
  public include root already on vaml-core-tests' include path; header-only -> nothing links).
  S006 is a NO-BUILD-BY-CONTRACT step (compile gate belongs to S007); still SILENT-COMPILE-CHECKED on
  MSVC (VS 2026 cl 19.51, /W4 /permissive- /EHsc /WX /std:c++20, throwaway TU incl. failing-predicate
  throw path + custom-sink path) -> W4/WX clean; throwaway files deleted. Next = S007.
  GOTCHA: the Shouldly factory helper functions REQUIRE the `line` argument (the *_macros supply it via
  `#stuff`); calling Integral/String/Pointer/Array directly without a line fails to overload ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â use
  ShouldlyIntegral/ShouldlyString/ShouldlyPointer/ShouldlyArray or pass a line explicitly.
  GOTCHA: on Windows, `cl` requires the VS dev shell (vcvars64.bat) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â call it inside the same cmd that
  runs cl (a bare `cmd /c "... && cl"` does NOT persist the vcvars env across separate cmd invocations).
- [x] S007 Shouldly.h compiles under MSVC + Clang; assertion round-trip tests ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦
  SURVIVOR: test/unit/shouldly_test.cpp (NEW ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â the compile-gate + round-trip test deliverable).
  Includes <vaml/assert/Shouldly.h> + doctest (main/impl owned by smoke_test.cpp; DOCTEST_CONFIG_
  IMPLEMENT_WITH_MAIN lives ONLY there to avoid duplicate-main at link). TEST STRUCTURE: a
  thread-local capture-sink RAII helper `CaptureOn` (installs a collecting FailureSink via
  failure_sink(), restores the prior sink on scope exit, clears the thread-local buffer) -> the
  "round-trip" (predicate -> ReportFailure -> sink -> captured formatted message). 14 test cases:
  (A) pass-vectors for Integral/String/Pointer/Array asserting NOTHING is captured; (B) fail-vectors
  per predicate asserting the captured message carries the expected fragment (e.g. "should be equal",
  "greater than", "range", "[i]" for elementwise); (C) Diff helper both directions on a double array;
  (D) default-sink contract: with NO custom sink a failing predicate throws TestFailureException
  (CHECK_THROWS_AS). Covered predicates: ShouldBe, ShouldNotBe, ShouldBeGreater/LessThan,
  Greater/LessThanOrEqualTo, InRangeTo, BeNull, NotBeNull, BeNullOrEmpty, NotBeNullOrEmpty,
  NotBeNullOrEmptyOrBe/ButBe/ButBeNearlyEqual(value+pointer)/ButMatchContents, Diff.
  test/unit/CMakeLists.txt (MOD): added shouldly_test.cpp to vaml-core-tests sources (no new flags;
  doctest already SYSTEM-excluded + -Wno-error=#warnings). build/verify_s007.cmd (NEW): clone of
  verify_s005 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â clean PATH -> vcvars64 -> cmake --preset msvc-debug -> build -> ctest.
  HEADER FIXES (S007 DOES touch Shouldly.h ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â sanctioned plan-scoped portability+bug fixes, each the
  "compile gate S006 deferred"): (1) BUG: ShouldBeNullOrEmpty required BOTH null AND len==0; intent
  is "null OR empty" ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â collapsed the two sequential ifs to one `if (!(actual==nullptr || len==0))`.
  (2) TestFailureException(const char*) relied on MSVC-only std::exception(char const*) ctor ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â now
  stores std::string message_ + overrides what() (portable to libstdc++). (3) Remove meaningless
  top-level const on `char const* const` RETURN types (Clang -Wignored-qualifiers). (4) Qualified
  std::fpclassify + std::isnormal (MSVC leaked cmath names into global scope; libstdc++ does not);
  FP_ZERO is a C MACRO (keep bare, not std::FP_ZERO ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â naive std:: prefix broke both compilers).
  VERIFIED BOTH COMPILERS: msvc-debug (VS 2026) via verify_s007.cmd GREEN (1/1 ctest PASS, 15 test
  cases listed = 14 Shouldly + smoke, all green); REAL LINUX clang-debug on Ubuntu VM via shared
  folder /mnt/hgfs/VamlPOC (clang 21.1.8) GREEN (1/1 ctest PASS). No new product symbols; this test
  file is permanent test infra (NOT a STUB / NOT MINIMAL REAL, recorded). Next = S008.
  GOTCHA: cannot declare a zero-length array in standard C++ (`int x[0]` ill-formed) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â use a non-null
  pointer with len 0 to exercise the empty/len==0 path. GOTCHA: `ShouldBeNullOrEmpty` is "null OR
  empty", NOT "null AND empty" ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â my first pass-case wrongly assumed AND; the OR semantics is the
  correct original intent. GOTCHA: FP_ZERO is a preprocessor macro (math.h: `#define FP_ZERO 2`), NOT
  a std:: member; `std::FP_ZERO` is ill-formed everywhere (both MSVC C2589 and Clang error) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â only
  fpclassify/isnormal take the std:: qualification. GOTCHA: MSVC accepted unqualified fpclassify/
  isnormal (they leak into global scope), so /W4 /WX was blind to the non-portable call ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â the Linux
  Clang lane is what surfaced it; keep the Clang lane authoritative for portability.
- [x] S008 REFACTOR GATE(S0): no dead code, headers internal-only, static-link default verified, clean PR ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦
  SURVIVOR: DELETED build/verify_msvc.cmd (superseded S001-era raw `-G Ninja` + direct-flag harness,
  fully duplicated by the preset-driven verify_s004/s005/s005_shared/s007.cmd; was gitignored/untracked).
  MODIFIED cmake/artifact-model.cmake (S008): resolve VAML_ARTIFACT_MODEL (STATIC/SHARED) + `message(
  STATUS "VAML artifact model: ..." )` so the static-by-default is provable from configure output (no
  hard FATAL); added include-surface convention comment ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â internal impl headers live under
  src/<lib>/src, ONLY public API under src/<lib>/include/vaml/<lib> (write-rule ONLY, NO header moves
  in S0; Shouldly.h stays the public vaml::test facade; vulkan/tools keep MINIMAL REAL version.h seeds
  until S161/S287). MODIFIED docs/roadmap.md: S008 [DONE]; S001 note now cites preset verify scripts.
  VERIFIED BOTH COMPILERS: msvc-debug (VS 2026) via verify_s007.cmd GREEN ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â configure prints
  "VAML artifact model: STATIC", static vaml-{core,vulkan,tools}.lib, ctest 1/1 PASS; REAL LINUX
  clang-debug on Ubuntu VM (clang 21.1.8) via /mnt/hgfs/VamlPOC GREEN ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â STATIC .a, ctest 1/1 PASS.
  Only Clang warnings = pre-existing upstream <ciso646> deprecation from vendored
  third_party/doctest under GCC-15 libstdc++ (not VamlPOC source, not introduced here). Tracked
  change-set = cmake/artifact-model.cmake + docs/roadmap.md only (export.h dirty flags are pre-existing
  CRLF/LF autocrlf noise ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â zero content diff vs HEAD). NO new md files (directive). Sprint-0
  SURVIVOR/checkpoint wrap is S009's job; this entry is a minimal non-duplicative record. Next = S009.
  GOTCHA: drive the MSVC lane through build/verify_s*.cmd (clean PATH + vcvars64 -> VS-bundled
  cmake/ninja win over any Cygwin PATH); a stale msvc-debug CMakeCache containing a /cygdrive path
  must be wiped before reconfigure.
- [x] S009 SURVIVOR wrap (Sprint 0) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦
  SURVIVOR: this step is documentation-only (no build, per plan). Added the consolidated
  `## SPRINT 0 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â SURVIVOR` block above (distills S000ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Å“S008 into the surviving-state picture a fresh
  LLM resumes from: green skeleton, 3 STATIC libs, ADR-011 artifact model, vendored doctest+vaml::test
  harness, MSVC+Linux Clang lanes both verified, MINIMAL REAL vulkanÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢S161 + toolsÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢S287 unresolved,
  invariants intact). Regenerated `## CHECKPOINT` (last completed = S009; next = S030; PENDING none).
  Marked roadmap S009 `[DONE]`. No new md files (directive); no MINIMAL REAL introduced or resolved;
  NO-GAP GUARANTEE remains satisfied (both active seeds still carry explicit resolution tokens).
  Next = S030.

### Sprint 1 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â Property system core   (S030ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Å“S049)
- [x] S030 Typeless Value ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦ (see roadmap S030 line for full detail)
  SURVIVOR: `src/core/include/vaml/core/{Color,Thickness,Value}.h` + `src/core/src/Value.cpp`.
  Color = uint8 RGBA struct; Thickness = float l,t,r,b struct ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â BOTH tagged
  `// [MINIMAL REAL] ... RESOLVED BY S089`. Value = `ValueType:uint8_t` enum (11 tags in roadmap
  order Null..UntypedPtr), hand-rolled tagged union (NOT std::variant), static factories for all 11
  payloads, `type()`/`isNull()`, template `get<T>()` throwing `vaml::ValueTypeMismatch` on tag/T
  mismatch, `operator==`, explicit copy/move/dtor for the std::string member.
  `ValueTypeOf<T>` trait maps payload type ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ accepted tags; `get<int32_t>` accepts BOTH Int32 and
  BrushId (BrushId is a tagged int32 scalar id). Test TU `test/unit/Value_test.cpp` (S030 scope:
  factoryÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢tag, equality, copy/move). Build fixes: enum/Storage forward-ref order (MSVC); read<T>()
  noexcept mismatch from string-copy-throws (Clang) ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ primary+all specializations non-noexcept;
  dropped duplicate doctest main. VERIFIED msvc-debug (25/25 cases, 91/91 asserts) + clang-debug.
  NO-GAP: Color/Thickness resolution authored at same step (ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢S089). Next = S031.
- [x] S031 Value round-trip + type-mismatch-throws tests ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦
  SURVIVOR: `test/unit/Value_roundtrip_test.cpp` (NEW; doctest, NO IMPLEMENT_WITH_MAIN ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â shares
  smoke_test's main). Complementary to S030's Value_test.cpp (left untouched). Scope:
  (A) ROUND-TRIP ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â every non-Null tag's payload round-trips through `get<T>()` EXACTLY and the
  re-factory `Value::X(v.get<P>()) == v` (operator== round-trip). String verified to round-trip by
  VALUE (copy), mutation of the read-back copy does not affect the stored value. BrushId/UntypedPtr
  included. (B) TYPE-MISMATCH-THROWS ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â Null has no payload so EVERY get<T> throws (gesture not
  corrupting: Null stays Null); each non-Null tag throws `ValueTypeMismatch` for every NON-valid
  payload type (exhaustive per-tag, incl. no cross-width/no-promotion: Int32 not int64/float/double,
  Bool not int32, Float not double, BrushId reads via int32_t ONLY but tag stays BrushId ÃƒÂ¢Ã¢â‚¬Â°Ã‚Â  Int32 and
  BrushId(1) != Int32(1)); String mismatches every scalar and vice-versa; constraint ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â after a
  throw the SAME value still re-reads its valid payload and still equals its original (throw path is
  a read-only no-op on the union). CMakeLists (MOD): + Value_roundtrip_test.cpp to vaml-core-tests.
  build/verify_s031.cmd (NEW): clone of verify_s007 pattern (clean PATH + vcvars64 ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ msvc-debug).
  VERIFIED BOTH COMPILERS: msvc-debug (VS 2026) exe 33/33 cases, 237/237 asserts PASS (was
  25/91 at S030); REAL LINUX clang-debug (Ubuntu VM clang 21.1.8) build green, ctest 1/1 PASS.
  Only Clang warnings = pre-existing upstream <ciso646> doctest deprecation (not this TU).
  No MINIMAL REAL introduced/resolved; NO-GAP unchanged. Next = S032.
  GOTCHA: `get<T>()` is `[[nodiscard]]` (S030) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â `CHECK_THROWS_AS(a.get<T>(), ...)` discards the
  return ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ MSVC C4834 (warnings-as-errors). Fix: wrap the expression `(void)a.get<T>()` inside
  CHECK_THROWS_AS; applied to all throw-paths.
- [x] S032 PropertyKey + PropertyRegistry ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦ (see roadmap S032 line for full detail)
  SURVIVOR: `src/core/include/vaml/core/{PropertyFlags,PropertyKey,PropertyRegistry}.h` +
  `src/core/src/{PropertyFlags,PropertyKey,PropertyRegistry}.cpp` + `test/unit/PropertyKeyRegistry_test.cpp`.
  All three are REAL public symbols (NOT MINIMAL REAL ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â no sentinel, no resolution token). PropertyFlags =
  enum class:uint32_t with None|Inherits(1<<0)|ReadOnly(1<<1)|BindsTwoWayByDefault(1<<2) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â exactly the
  roadmap's S032/S034 flag set; free functions `operator|` + `has_flag` (independent-bit semantics),
  defined out-of-line in PropertyFlags.cpp. PropertyKey = immutable (name, type:ValueType, ownerTypeId:string,
  flags) tuple; getters; operator==/!= on all four fields; describe() renders a human-readable line with a
  stable type tag name (mirrors Value.cpp enum order). Design decisions recorded: owner-id is a std::string
  type-name (no cheap portable runtime type_of in C++; supports S043 attached-prop ownerÃƒÂ¢Ã¢â‚¬Â°Ã‚Â class; S033's
  constexpr-safe static id layers on without changing fields); unique on NAME globally (WPF-style), ownerTypeId
  stays identity metadata inside the key, not a second registry dimension. PropertyRegistry = instance-based,
  pimpl'd (class Impl behind unique_ptr hides std::unordered_map ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â include-surface isolation; the "ABI"
wording is superseded by S142's collapsed block-resident store + ADR-003 revised); register_key
  (uniques by name; throws PropertyAlreadyRegistered:logic_error on collision, map unchanged, returns stored
  const ref), try_find (ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ptr or nullptr, noexcept), find (ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢stored key or throws PropertyNotFound:out_of_range),
  size/empty. Exceptions declared in header, definitions in .cpp. A global static registry + constexpr-safe
  static id/lookup is an S033 concern, NOT baked here (recorded, not silent). Test TU covers: PropertyFlags
  OR+has_flag; PropertyKey getters + per-field equality + different ValueType tags (Int32/Thickness/String) +
  describe(); Registry register/find/try_find/size; dedupe ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â duplicate NAME throws PropertyAlreadyRegistered
  and leaves the map unchanged (size stable, original still findable); multiple distinct names coexist.
  CMakeLists: +3 core sources; test/unit +PropertyKeyRegistry_test.cpp. build/verify_s032.cmd (NEW): clone of
  verify_s031 pattern. VERIFIED BOTH COMPILERS: msvc-debug (VS 2026) ctest 1/1 PASS (was blocked by discarded
  [[nodiscard]] register_key return ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ `(void)` cast fix); REAL LINUX clang-debug (Ubuntu VM clang 21.1.8)
  build green, ctest 1/1 PASS. Only Clang warnings = pre-existing upstream <ciso646> doctest deprecation.
  NO-GAP accounting: S033 = exhaustive register/lookup/dedupe-throws + constexpr-safe static id; S034 consumes
  flags + adds metadata (defaults/callbacks); S039/S043 consume owner-id + Inherits. Two deferred design
  decisions recorded (not silent): (a) global name-uniqueness scope, (b) global static registry instance ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢
  both S033. No MINIMAL REAL introduced/resolved. Next = S033.
- [x] S035 Metadata apply-semantics tests (default applied, cb-only-on-real-change, coerce applied, validate rejects) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦
  SURVIVOR: `test/unit/PropertyMetadata_semantics_test.cpp` (NEW ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â pure TEST step, NO product symbol).
  A doctest-scope `MiniStore<T>` harness models the Set-time apply pipeline that S036's DependencyObject
  must reproduce: construction seeds `current = defaultValue()` ("default applied"); `trySet(proposed)`
  runs coerce ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ validate(coerced) ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ fire-changed-cb-only-on-real-change ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ store, and returns
  APPLIED/REJECTED/NO_CHANGE. The harness records every cb fire + (old,new) so exact call behavior is
  assertable. Tests cover exactly S035's four title behaviors + ordering + passthrough, each in its own
  TEST_CASE: (1) **default applied** ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â fresh holder reads defaultValue() (7); setting a value equal to the
  default ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ NO_CHANGE, no cb. (2) **coerce applied** ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â negative raw ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢5 stored (coerced value is what's
  validated+stored), cb receives coerced new; positive passthrough stores as-given with correct (old,new).
  (3) **coerce-before-validate ordering** ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â raw -3 would FAIL `validate(v>=0)` as-is, but coerce runs FIRST
  turning it into 5, so validate(5) passes ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ APPLIED (default 0ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢5, cb (0,5)); proves validate sees the
  COERCED value. (4) **validate rejects** ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â rejected value ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ REJECTED, current unchanged, cb NOT fired
  (checked for two distinct rejections). (5) **cb fires ONLY on real change** ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â first set fires once;
  setting the SAME value repeatedly ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ NO_CHANGE, fireCount unchanged; N distinct values ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ N fires each with
  correct (old,new); reverting to a previously-held value still fires (it IS a real change). (6) **absent
  callbacks = passthrough** ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â no coerce/validate/changed ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ stores as-given, one fire on first set, NO_CHANGE
  on repeat. (7) harness stays **type-bound** ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â a wrong-tag proposed value throws vaml::ValueTypeMismatch
  through the pipeline before any accept decision, store untouched. The MiniStore + test file are PERMANENT
  TEST INFRASTRUCTURE (same class as the vendored doctest/Shouldly harness), NOT a STUB and NOT MINIMAL
  REAL ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â no sentinel, never shipped, because the real value-holder is S036's DependencyObject (recorded,
  not silent; NO-GAP intact: the feature is already scheduled at S036). CMakeLists (MOD): test/unit
  +PropertyMetadata_semantics_test.cpp to vaml-core-tests. build/verify_s035.cmd (NEW): verify_s034-pattern
  msvc-debug lane (clean PATH ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ vcvars64 ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ msvc-debug configure+build+ctest). VERIFIED: msvc-debug (VS 2026)
  ctest 1/1 PASS (66 cases total incl. S034/S033...; this TU adds 7). One in-development test-bookkeeping
  fix: the first ordering draft coerced -5ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢0 while the default was already 0 (would be NO_CHANGE, not
  APPLIED) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â corrected to coerce -3ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢5 against default 0 so the ordering proof is a real change (fixed
  pre-build, not a shipped defect). DESIGN CONTRACT hand-off to S036 (recorded searchable in roadmap +
  progress): DependencyObject::Set MUST reproduce coerce ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ validate(on coerced) ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ fire changed cb only on
  real change ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ store; `PropertyMetadata_semantics_test.cpp::MiniStore` is the reference spec.
  Roadmap S035 marked [DONE]; S036 line annotated with the S035ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢S036 apply-pipeline contract.
  No MINIMAL REAL introduced/resolved. Next = S036.

- [x] S034 PropertyMetadata { defaultValue; flags; changed cb; coerce; validate } ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦
  SURVIVOR: `src/core/include/vaml/core/PropertyMetadata.h` (NEW) + `src/core/src/PropertyMetadata.cpp`
  (NEW) + `test/unit/PropertyMetadata_test.cpp` (NEW). Move-only (copy deleted, proven by static_assert)
  metadata bundle under an Impl pimpl (include-surface isolation; "hides std::function ABI" wording
superseded by S142 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â the pimpl is removed, not preserved; ADR-003 revised; enforces move-only at the public
  boundary). Field surface mirrors the roadmap exactly: defaultValue (Value) fixes BOTH S039's fallback
  AND the metadata's bound type = defaultValue.type(); flags (PropertyFlags recomposes S032's
  inherits|readonly|bindsTwoWayByDefault as a validating carrier, never a competing source of truth);
  changed cb (old,new); coerce Value(Value); validate Value->bool. Construction: canonical factory
  `WithDefault(Value, PropertyFlags)` (REQUIRED ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â no default-constructible empty metadata), then
  &&-qualified return-by-value builder hooks `with_coerce` / `with_validate` / `with_changed` (each
  installs one callback and returns a NEW metadata, leaving the source untouched). Passthrough defaults:
  absent coerce=identity, absent validate=always-accept, absent changed=no-op (branch-free common path
  for S036). TYPE-BOUND: coerce/validate/notifyChanged throw vaml::ValueTypeMismatch (S030 contract) when
  passed a Value whose tag != boundType ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â NO-LAZY guard against a callback receiving a wrong-type Value.
  Test TU: move-only static_asserts + move ctor/assign; WithDefault seeds default+bound type+flags;
  flags carrier round-trips each of the 3 bits; coerce absent=identity / present=transforms; validate
  absent=accept / present=accept+reject; changed cb no-op when absent / fires (old,new) when installed;
  single builder chain composes all three hooks; wrong-tag throws from all three entry points + correct
  tag still passes post-throw (throw path is read-only); builder return-by-value leaves source untouched.
  CMakeLists (MOD): core +PropertyMetadata.cpp; test/unit +PropertyMetadata_test.cpp.
  build/verify_s034.cmd (NEW): verify_s033-pattern msvc-debug lane. VERIFIED BOTH COMPILERS: msvc-debug
  (VS 2026) ctest 1/1 PASS; REAL LINUX clang-debug (Ubuntu VM clang 21.1.8) build green, ctest 1/1 PASS.
  Only Clang warnings = pre-existing upstream <ciso646> doctest deprecation (degraded by design, not this
  TU). NO-GAP accounting: S034 delivers the metadata TYPE + its own unit semantics; the actual
  coercion/validation/changed INVOCATION at Set-time and the global static registry +
  DependencyProperty::Register LANDING SITE are S036's concern (S036 = DependencyObject with a local
  value store that needs a registered DP) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â recorded non-silently, deliberately resolving the S032(b)/S033
  recorded deferral. S035 tests every field (default applied, cb-only-on-real-change, coerce applied,
  validate rejects). No MINIMAL REAL introduced/resolved. Next = S035.

- [x] S033 Registry exhaustive tests + constexpr-safe static id ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦ (see roadmap S033 line for full detail)
  SURVIVOR: `src/core/include/vaml/core/PropertyId.h` (NEW ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â header-only constexpr-safe static id).
  PropertyId wraps the property NAME as a `std::string_view` bound to a string literal: `constexpr`-safe
  (fully compile-time, proven by `static_assert` in tests), collision-free by construction (carries the
  name, NOT a hash ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ no collision class, no cross-TU nondeterminism, no static-init-order fiasco), stable +
  interconvertible with the registry's name-keyed store. API: `of(sv)`, `name() constexpr`, `valid()`,
  `constexpr ==/!=`, `std::hash<PropertyId>` specialization (feeds S036's local-value store as map key).
  Deliberate name-token-over-hash choice recorded (a compile-time hash also compiles but reintroduces a
  collision class + opaque proof). NOT a STUB / NOT MINIMAL REAL ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â complete on delivery. Lifetime note:
  owner must pass a string literal (wrapping a temp std::string is a lifetime bug), mirroring the registry
  name contract. `PropertyRegistry` (MOD PropertyRegistry.h/.cpp): + `id_of(name)` (ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢PropertyId, throws
  PropertyNotFound if unregistered) + `try_id(name) const noexcept` (ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢id or invalid empty id) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â the
  runtimeÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢compile-time bridge = the canonical key S036's local-value store uses. Test TUs (NEW):
  `test/unit/PropertyId_test.cpp` (static_assert compile-time proof; equality by name; default-id invalid;
  std::hash consistent with equality; id_of/try_id round the name-keyed store + unregistered-name path)
  and `test/unit/PropertyRegistryExhaustive_test.cpp` (exhaustive bulk register across types/flags/owners +
  exact lookup; all four duplicate-throws rejection forms ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â diff type / diff owner / diff flags / identical
  key ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â each leaves the registry unchanged: size, stored-pointer identity, and id stable; missing-lookup on
  empty + non-empty; move-construct/move-assign preserve mappings + dedupe). CMakeLists (MOD): +2 test TUs.
  build/verify_s033.cmd (NEW): verify_s032-pattern msvc-debug lane. VERIFIED BOTH COMPILERS: msvc-debug
  (VS 2026) ctest 1/1 PASS; REAL LINUX clang-debug (Ubuntu VM clang 21.1.8) build green, ctest 1/1 PASS.
  Only Clang warnings = pre-existing upstream <ciso646> doctest deprecation. GOTCHA: doctest stringifies
  CHECK operands via ostream ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â comparing a raw `std::string_view` operand fails to compile on MSVC (undefined
  `basic_ostream`) unless wrapped (e.g. `std::string(id.name())`); check string_view-wide values via
  `.size()`/`.empty()` or convert. DESIGN re-deferral (S032 item (b)): global static registry instance NOT
  added in S033 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â no consuming static-Register site exists until S034's real `DependencyProperty::Register`;
  recorded non-silently as an explicit S034 concern, resolving the S032 recording. NO-GAP: PropertyId is
  real + fully tested (no sentinel); nothing left for silent backfill; deferral searchable in roadmap+progress.
  No MINIMAL REAL introduced/resolved. Next = S034.
 
- [x] S036 DependencyObject base: local value store, Get/Set, per-prop mastery, thread-affinity marker. ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦
  SURVIVOR: `src/core/include/vaml/core/DependencyObject.h` (NEW) + `src/core/src/DependencyObject.cpp`
  (NEW) + `test/unit/DependencyObject_test.cpp` (NEW) + `build/verify_s036.cmd` (NEW, clone of
  verify_s035-pattern msvc-debug lane). REAL public symbol `vaml::DependencyObject` (NOT MINIMAL REAL,
  no sentinel ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â every member is genuine permanent surface with scheduled consumers). API: (a)
  `register_property(PropertyKey, PropertyMetadata)` ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ returns const ref to STORED key; installs
  (metadata, defaultValue) keyed by the property NAME; name dedupe throws `PropertyAlreadyRegistered`
  leaving store unchanged; type-coherence guard key.type()ÃƒÂ¢Ã¢â‚¬Â°Ã‚Â metadata.boundType() ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ `ValueTypeMismatch`.
  (b) `Get(PropertyId)` ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ Value BY VALUE (no ref-invalidation hazard for S037 marshalling/S039
  effective-value); unregistered id throws `PropertyNotFound`; absent local = "default applied" ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ falls
  back to metadata.defaultValue(). (c) `Set(PropertyId, const Value&)` ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ `SetResult::{APPLIED,REJECTED,
  NO_CHANGE}` reproducing the S035 pipeline VERBATIM: coerce ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ validate(on the COERCED value) ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ fire
  changed cb (oldEffective, coerced) ONLY on a real change ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ store; oldEffective = local-override-else-
  default; wrong-tag input throws `ValueTypeMismatch`. (d) thread-affinity marker: ctor records
  `std::this_thread::get_id()`; exposes `thread_affinity_id()` + `is_affine_to_current_thread()` (real,
  consumed by S037 wrong-thread detection). Storage pimpl'd (class Impl + unique_ptr ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ header exposes no
  unordered_map / no move-only PropertyMetadata). GOTCHA (recorded): the internal store keys on an OWNED
  std::string (copied name), NOT on `PropertyId` ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â a PropertyId is a non-owning std::string_view that
  dangles as soon as the registering PropertyKey (its storage source) dies; SSO+heap names both failed
  until the owned-string key. This is why Get/Set re-derive `std::string(id.name())` for lookup. Tests
  (doctest, 11 S036 TEST_CASEs): default-applied; coerce applied (coerced value validated+stored, cb sees
  coerced new); coerce-BEFORE-validate ordering; validate rejects (store unchanged, no cb); cb-fires-ONLY-
  on-real-change (same-value ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ NO_CHANGE, distinct ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ fires once each); passthrough; type-bound wrong-tag
  throws (store untouched); per-prop mastery (two props coexist independently); dedupe throws-unchanged
  (local override preserved); Get-by-value (string copies independent); thread-affinity (affine on
  creating thread; a worker-created object records the worker id ÃƒÂ¢Ã¢â‚¬Â°Ã‚Â  this thread). CMakeLists (MOD): core
  +`src/DependencyObject.cpp`; test/unit +`DependencyObject_test.cpp`. GOTCHA: `Value` has NO public
  default ctor ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ the `PropEntry::local` member initializes to `Value::Null()` (not `{}`). GOTCHA: `Set`
  is `[[nodiscard]]` by design (result must be read) ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ tests that discard it cast to `(void)` to satisfy
  MSVC C4834 under /WX. DESIGN re-scope (recorded, NOT silent): the global static registry +
  `DependencyProperty::Register` landing site (S034 recorded it as "S036's concern") is authored at S039
  instead ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â S036 delivers OBJECT-SCOPED registration only, because a global static registry in S036 would
  be speculative surface with no consumer until S039's effective-value resolution; roadmap S039 line
  carries this response note + S037 line annotated with its S036 thread-affinity consumer contract.
  VERIFIED: msvc-debug (VS 2026) via verify_s036.cmd ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â configure/build warning-clean, ctest 1/1 PASS
  (100% tests passed; 79 total cases / 529 asserts across the suite incl. 11 new S036 cases). The
  windows-clang-debug preset was attempted but fails on EVERY TU (incl. untouched version.cpp) on a
  pre-existing `/Zc:preprocessor`-unused clang-cl incompatibility ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â a cross-cutting preset issue wholly
  out of S036 scope, NOT introduced here; the maintained verify lane for this step is msvc-debug (green).
  No MINIMAL REAL introduced/resolved. Next = S037.
 
- [x] S036_1 Shouldly routing reform: predicates keep the S006 default-sink THROW contract; new
  doctest-aware macro seam `SHOULD`/`SHOULD_REQUIRE` routes failures into doctest's ResultBuilder.
  ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦
  SURVIVOR: `src/core/include/vaml/assert/Shouldly.h` (MOD) + `test/unit/shouldly_test.cpp` (MOD).
  DESIGN/SEAM (the genuinely hard part, resolved): a fluent Shouldly predicate call
  (`ShouldlyIntegral(x).ShouldBe(y)`) cannot by itself terminate inside doctest ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â doctest's
  `detail::ResultBuilder` is only valid while a test is running (it dereferences g_cs) and a fluent
  call evaluated inside an expression has no scoped ResultBuilder to hand its result to. So predicates
  were NOT rewritten; they keep throwing `vaml::test::TestFailureException` on failure (the S006
  default-sink contract). The integration lives in a MACRO that opens the RB inside a test context:
  `SHOULD( ShouldlyIntegral(Get()).ShouldBe(3) )` -> opens RB(file,line,#expr), runs the predicate,
  catches the thrown TestFailureException, routes its message into rb.m_decomp + rb.m_failed=true,
  then rb.log()/rb.react() (log() feeds doctest counters/summary, react() honors severity). Severity
  explicit + correct: `SHOULD` = DT_CHECK (continue-on-failure), `SHOULD_REQUIRE` = DT_REQUIRE (abort
  case). Core stays doctest-free: the macros are gated `#ifdef DOCTEST_LIBRARY_INCLUDED`, so any non-
  test TU gets the canonical DSL with no doctest dependency; test TUs include <doctest/doctest.h>
  before <vaml/assert/Shouldly.h>. FAILURE-PROOF note: because predicates still throw, a failing
  predicate can be asserted bare via `CHECK_THROWS_AS(..., vaml::test::TestFailureException)` ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â no
  sink capture needed. shouldly_test.cpp (MOD): KEPT the default-sink-throws contract test and now
  documents it as the exact contract SHOULD consumes; ADDED SHOULD success-path cases (integral decay
  great/etc, string ShouldBe/ShouldNotBe, pointer ShouldBeNull/ShouldNotBeNull), a string+pointer
  success case, and a `should_fail()` decorated demonstration proving a FAILING SHOULD is swallowed
  and routed into doctest (had it rethrown TestFailureException it would surface as a test-case
  exception/crash instead of a normal AssertFailure -> suite would fail, not pass). The golden rule
  that forced S036_1 to also rewrite shouldly_test.cpp in the SAME step: the reform invalidates the
  old throw-contract assertions, so deferring the rewrite would leave an unbuildable suite window.
  GOTCHA (fixed at build): `TEST_CASE("...") * doctest::should_fail()` must place the decorator INSIDE
  the TEST_CASE argument list, not on the following line (MSVC C2143/C2063 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â decorator is a chained
  `*` onto the macro arg). GOTCHA: this was ON-STEP (S036_1 only); feature-test files are NOT
  migrated here ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â that is S036_4/S036_5 by plan. VERIFIED: msvc-debug (VS 2026) configure/build
  warning-clean, ctest 1/1 PASS (100% tests passed). The maintained verify lane for this step is
  msvc-debug (green); windows-clang-debug preset remains broken on EVERY TU by the pre-existing
  /Zc:preprocessor clang-cl incompatibility (cross-cutting, out of scope, NOT introduced here). No
  MINIMAL REAL introduced/resolved. Next = S036_2 (StringMaker domain exposure).
 
- [x] S036_2 StringMaker domain exposure: doctest::StringMaker specializations for VAML core types so
  failed assertions stringify as readable source-level text. ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦
  SURVIVOR: `test/unit/vaml_test_stringmakers.h` (NEW, TEST-ONLY header) + `test/unit/StringMaker_test.cpp`
  (NEW) + `test/unit/CMakeLists.txt` (MOD +StringMaker_test.cpp). SPECIALIZATIONS (namespace doctest):
  `StringMaker<vaml::Value>` -> tag-first "<Tag payload>" for ALL 11 payload kinds: Null "null", Bool
  "true"/"false", Int32/Int64/Float/Double scalar, String "\"..\"", Color "Color", Thickness "Thickness",
  BrushId scalar, UntypedPtr address; helper `detail::vaml_value_type_name(ValueType)` maps tag->name.
  `StringMaker<vaml::SetResult>` -> "SetResult::APPLIED"/"REJECTED"/"NO_CHANGE". `StringMaker<vaml::PropertyId>` ->
  "PropertyId(Width)" or "PropertyId(empty)". WHY test-side header (layering): these live in
  `namespace doctest`, so they CANNOT live in core ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â core must stay doctest-free (S036 layering rule);
  the header is TEST-ONLY and each test TU includes it. GOTCHA (doctest internals, two build fixes):
  `doctest::String` has a `const char*` ctor but NO `std::string` ctor and NO `operator+(const char*)` ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â
  conversions must go through `.c_str()` into a `const char*`; `<sstream>` must be included explicitly
  (ostringstream is not transitively available). GOTCHA (link, fixed): StringMaker_test.cpp must NOT
  define `DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN` (only smoke_test.cpp owns the main) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â doing so causes
  ~200 LNK2005 multiply-defined-symbol failures (main + every doctest global). SYSTEM benefit: doctest
  auto-uses `doctest::StringMaker` when stringifying a failed expression's operands, so ANY later
  `CHECK(a == b)` (or SHOULD-equivalent) on Value/SetResult/PropertyId renders readable output
  automatically ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â the S036_2 payoff is system-wide, not confined to StringMaker_test. This is the
  framework-native "source-tool readability scaffold" the re-formulated Shouldly-for-doctest needs for
  raw stringified fallback. VERIFIED: msvc-debug (VS 2026) configure/build warning-clean, ctest 1/1
  PASS (100% tests passed; 3 new S036_2 cases). Maintained verify lane msvc-debug (green); windows-clang
  -debug preset remains broken on EVERY TU (pre-existing /Zc:preprocessor clang-cl incompatibility,
  cross-cutting, out of scope, NOT introduced here). No MINIMAL REAL introduced/resolved. Next =
  S036_3 (ShouldThrow<Ex> typed-exception assertion category).
 
- [x] S036_3 ShouldThrow<Ex> typed-exception assertion category ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â first-class replacement for
  CHECK_THROWS_AS/_WITH/_WITH_AS that OUT-EXPOSES doctest (reports actual thrown type + message). ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦
  SURVIVOR: `src/core/include/vaml/assert/Shouldly.h` (MOD) + `test/unit/shouldly_test.cpp` (MOD).
  API (namespace vaml::test, template resolution): `ShouldThrow<Ex>(Fn&&)` ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â asserts fn() throws Ex;
  `ShouldThrow<Ex>(Fn&&, std::string const&)` and `ShouldThrow<Ex>(Fn&&, char const*)` ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â message-
  matcher overloads (Ex.what() must equal the expected string). Tail `static_assert(is_base_of<std::
  exception,Ex>)` requires a std::exception-derived Ex (so a message exists). FAILURE MODES (through
  the SAME pluggable sink as every predicate -> composes with SHOULD(...) seam): (1) nothing thrown ->
  "expected <Ex> but nothing was thrown"; (2) wrong std::exception type -> "expected <Ex> but got
  <typeid(e).name()> '<e.what()>'" (ACTUAL type + message surfaced ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â the ShouldThrow value-add over
  CHECK_THROWS_AS, which cannot report the actual type at all); (3) right type + wrong message ->
  "<Ex> with message '<expected>' but message was '<actual>'"; non-std throws -> "<Ex> but a non-std::
  exception of unknown type was thrown". DESIGN/FIX (three builds): the Ex catch is IDENTICAL to
  catch(std::exception) when Ex==std::exception -> MSVC C2312 duplicate-handler. Resolved with
  `if constexpr(is_same<Ex,std::exception>)` that emits TWO handlers (catch(std::exception) passes,
  catch(...) fails) vs THREE handlers (Ex / std::exception-with-typeinfo / catch(...)) for derived Ex.
  The message-matcher overload adds `static_assert(!is_same<Ex,std::exception>)` (ambiguous semantics).
  Any non-returning path falls to the trailing "nothing was thrown" report (a GOTCHA: an earlier draft
  dropped it under catch(...) for derived Ex, silently losing the wrong-type report + failing one test).
  Tests (6 new S036_3 TEST_CASEs, using vaml::test::ShouldThrow + name-scoped
  DomainError/throwDomainError/throwLogicError/throwNothing fixtures + <stdexcept>): pass on right
  type; pass on base-Ex catching a derived throw (ShouldThrow<std::exception>(throwDomainError) via
  the if constexpr two-handler branch); fail on nothing-thrown; fail on wrong type routes ACTUAL
  message ("logic boom"); pass on message match (std::string + const char* overloads); fail on wrong
  message routes ACTUAL message. THIS STEP proves the category compiles + behaves; the suite-wide
  CHECK_THROWS_AS->ShouldThrow migration is S036_4/S036_5 (NOT here, per plan). VERIFIED: msvc-debug
  (VS 2026) configure/build warning-clean, ctest 1/1 PASS (100% tests passed, 90 cases / 558 asserts).
  Maintained verify lane msvc-debug (green); windows-clang-debug preset remains broken on EVERY TU
  (pre-existing /Zc:preprocessor clang-cl incompatibility, cross-cutting, out of scope, NOT introduced
  here). No MINIMAL REAL introduced/resolved. Next = S036_4 (migrate feature tests).
 
- [x] S036_4 Migrate Value / roundtrip / PropertyId / PropertyKeyRegistry / PropertyRegistryExhaustive /
  PropertyMetadata feature tests from CHECK* to the Shouldly-for-doctest vocabulary. ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦
  SURVIVOR: `test/unit/vaml_test_stringmakers.h` (MOD, +operator<< overloads) + the six migrated
  test files (Value_test, Value_roundtrip_test, PropertyId_test, PropertyKeyRegistry_test,
  PropertyRegistryExhaustive_test, PropertyMetadata_test) all rewritten to the Shouldly vocabulary.
  PRE-REQ (the IMPLEMENT BLOCKER surfaced + user ACCEPTED): the plan's literal "migrate CHECK== to
  SHOULD(Shouldly...)" is NOT buildable without formatter support ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â Shouldly's stream-path formatter
  (ShouldlyStr -> ThrowAwesomelyStr) renders a failure via `std::stringstream << actual`, and the
  domain types (Value/ValueType/Color/Thickness/SetResult/PropertyId) had no operator<<. Resolution:
  added test-side `operator<<` overloads in `vaml_test_stringmakers.h` (namespace vaml so ADL finds
  them) reusing the exact S036_2 readable-text intent ("<Int32 42>", "SetResult::APPLIED",
  "PropertyId(Width)", "RGBA(...)", "Insets(...)"); +`operator<<(ostream, PropertyKey)` delegating to
  `k.describe()` (PropertyKey had none). Kept TEST-SIDE (not core): Value/Color/Thickness headers stay
  lean + S089 Color/Thickness are MINIMAL REAL ("do NOT extend silently") ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â the formatting surface is
  deliberately out of production headers. MIGRATION PATTERN (uniform across all 6 files): equality
  `CHECK(a==b)` -> `SHOULD(ShouldlyString(a).ShouldBe(b))`; inequality `CHECK_FALSE(a)`/`CHECK(x!=y)`
  -> `SHOULD(ShouldlyString(x).ShouldNotBe(y))` / bool -> `ShouldNotBe(true)`; flag checks
  `CHECK(has_flag(...))` -> `SHOULD(ShouldlyString(has_flag(...)).ShouldBe(true))`; typed-exception
  `CHECK_THROWS_AS((void)f(),Ex)` -> `SHOULD(ShouldThrow<Ex>([&]{ (void)f(); }))`; missing-string
  contains-checks -> `ShouldlyString(d.find("X")!=npos).ShouldBe(true)`. SAMCTIONED non-migrated
  survivors (deliberate, documented in each file): (a) 3 REQUIRE precondition guards (null-deref
  gates BEFORE dereferencing a try_find pointer in PropertyKeyRegistry_test x1 + PropertyRegistry
  Exhaustive_test x2) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â these MUST abort the case on null, so REQUIRE is the correct severity;
  (b) StringMaker_test.cpp's CHECKs ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â they assert the raw doctest::String text produced by the S036_2
  StringMaker itself (an infrastructure test of the stringifier; migrating it to Shouldly would change
  the very output under test). smoke_test.cpp (the doctest runner/main) untouched. PROCESS (the user's
  option-B restart, honored): the FIRST S036_4 attempt used regex bulk-edits on Value_roundtrip_test
  .cpp and was discovered unverified + rolled back (git checkout) at user direction; this completed
  run is the CONSERVATIVE approach ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â one file at a time, full read then full rewrite (not regex
  surgery), each verified by a build gate; PropertyId_test.cpp (22 sites, simplest types) migrated
  FIRST as the proven template before scaling. GOTCHA (build-fixed): operator<<(Color/Thickness) must
  be DEFINED ABOVE operator<<(Value) in the header because operator<<(Value) calls v.get<Color>() /
  v.get<Thickness>() (C3861 referencing a later definition) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â reordered so Color/Thickness precede
  Value. GOTCHA: ShouldlyStr<T> requires actual + expected to be the SAME type T (both T const&), so
  `ShouldlyString(std::string(x)).ShouldBe(std::string("..."))` (NOT a const char* rhs), and pointer
  null-checks became `ShouldlyString(p == nullptr).ShouldBe(true)` / `!= nullptr` rather than
  ShouldNotBe(nullptr) (nullptr_t vs T* mismatch). Status of migration target (S036_4 goal): ZERO
  CHECK* remain in the 6 feature files except the sanctioned REQUIRE guards; no CHECK_THROWS_AS /
  CHECK_NOTHROW / CHECK_THROWS_WITH anywhere in the migrated files; verbose failure info now carried
  by the ShouldThrow<Ex> (actual-type+message) + ShouldlyString predicates. VERIFIED: msvc-debug (VS
  2026) configure/build warning-clean, ctest 1/1 PASS (100% tests passed) after EACH file and after
  the final gate. Maintained verify lane msvc-debug (green); windows-clang-debug preset remains
  broken on EVERY TU (pre-existing /Zc:preprocessor clang-cl incompatibility, cross-cutting, out of
  scope, NOT introduced here). No MINIMAL REAL introduced/resolved. Next = S036_5 (migrate
  DependencyObject_test.cpp ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â the S036 proof ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â as the capstone + final gate).


- [x] S036_5 Migrate `DependencyObject_test.cpp` (the S036 proof) as the capstone + final gate of the
  Shouldly-for-doctest convention effort. ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦ (S036_1..S036_5 PLAN COMPLETE)
  SURVIVOR: `test/unit/DependencyObject_test.cpp` (MOD, full rewrite) + `docs/roadmap.md` (MOD,
  S036_1..S036_5 all [DONE]) + `session-context.md` (MOD). MIGRATION: all 11 S036 TEST_CASEs converted
  with zero semantic change ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â default-applied -> `SHOULD(ShouldlyString(obj.Get(...)).ShouldBe(Value::
  Int32(7)))`; SetResult outcomes -> `SHOULD(ShouldlyString(obj.Set(...)).ShouldBe(SetResult::APPLIED/
  NO_CHANGE/REJECTED))`; fire-count / oldV / newV -> `ShouldlyString(fire).ShouldBe(n)`; type-coherence
  + PropertyNotFound + value-type-mismatch + dedupe throws -> `SHOULD(ShouldThrow<Ex>([&]{...}))`;
  thread-affinity -> `SHOULD(ShouldlyString(bool).ShouldBe(true))` (is_affine_to_current_thread /
  affinity==current / !=default / worker-records-other-thread). Includes wired: vaml_test_stringmakers.h
  + <vaml/assert/Shouldly.h>; `using vaml::test::ShouldThrow`. FINAL GATE (the S036_5 contract): zero
  CHECK*/CHECK_THROWS_AS across all 7 migrated feature files (Value_test, Value_roundtrip_test,
  PropertyId_test, PropertyKeyRegistry_test, PropertyRegistryExhaustive_test, PropertyMetadata_test,
  DependencyObject_test) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â verified by grep (all 0). SANCTIONED non-migrated asserts (deliberate,
  documented in files + roadmap S036_5 [DONE]): (1) PropertyMetadata_semantics_test.cpp = the S035
  reference SPEC harness, intentionally NOT migrated ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â it IS the spec S036 must satisfy, rewriting it
  would alter the exact contract under test (its single CHECK_THROWS_AS stays); this is the one place
  where "no CHECK_THROWS_AS remains" is intentionally carved out, and it is recorded, NOT silent.
  (2) smoke_test.cpp = the doctest runner/main harness. (3) StringMaker_test.cpp = asserts the raw
  doctest::String output of the S036_2 StringMaker itself (infrastructure test of the stringifier,
  not a vaml domain assertion). (4) 3 REQUIRE precondition guards (null-deref gates). S047 annotation:
  "every numbered predicate via adapted Shouldly" is now the standing vocabulary from S036_5 onward ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â
  S047 remains the formal suite-complete gate against the established vocabulary. VERIFIED: msvc-debug
  (VS 2026) configure/build warning-clean, ctest 1/1 PASS (100% tests passed) after the capstone rewrite
  and at the final gate. Maintained verify lane msvc-debug (green); windows-clang-debug preset remains
  broken on EVERY TU by the pre-existing /Zc:preprocessor clang-cl incompatibility (cross-cutting, out
  of scope, not introduced here) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â full dual-compiler verification of the S036_* suite is deferred to
  S041+ gates / an explicit later dual-compiler run (recorded, not silent). No MINIMAL REAL introduced
  or resolved. Next = S037.

- [x] S038 ChangeRecorder: pending per-property changes for deferred notification ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦
  SURVIVOR: `src/core/include/vaml/core/ChangeRecorder.h` (NEW) + `src/core/src/ChangeRecorder.cpp`
  (NEW) + `test/unit/ChangeRecorder_test.cpp` (NEW) + `build/verify_s038.cmd` (NEW, clone of
  verify_s037-pattern msvc-debug lane) + `src/core/CMakeLists.txt` (MOD +ChangeRecorder.cpp) +
  `test/unit/CMakeLists.txt` (MOD +ChangeRecorder_test.cpp) + `docs/roadmap.md` (MOD S038 [DONE],
  S042 REALIZED-EARLY annotation). REAL public symbol `vaml::ChangeRecorder` (NOT MINIMAL REAL, NOT a
  stub ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â complete, fully-tested, with an explicitly scheduled consumer S041; recorded NO-GAP). Pimpl'd
  (Impl + unique_ptr, move-only copy-deleted) so the header exposes no std::unordered_map / std::function
  value. API: `record(PropertyId, Value old, Value new)` ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â pending per-property notification; a repeat
  for the SAME property COALESCES (first `old` kept, latest `new` adopted, one notification per property);
  `has_pending(id)`; `pending_count()` (distinct-property depth); `flush(NotifyFn)` ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â emit one
  notify(id,old,new) per pending property in FIRST-RECORD order, then clear, return the emit count;
  `clear()`. DESIGN (recorded in header + roadmap, NO LAZY LLM): DELIBERATELY SINGLE-THREADED (a
  pipeline-internal deferral device ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â the cross-thread ChangeQueue/mutex is S041's job, NOT baked here);
  S038 stays out of S041's scope and does NOT rewire DependencyObject's synchronous callbacks (would
  break the S037 marshalling tests, which is S041's work). COALESCING realized-early (S042 outcome
  delivered at the primitive here; S042 annotated REALIZED-EARLY per the S036_1..S036_5 ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ S047 precedent
  and remains the formal end-to-end integration gate). Owned-name gotcha honored (S036/S037): records key
  on an OWNED std::string copy of PropertyId::name(), never the non-owning string_view; insertion-ordered
  vector+name-map (never unordered_map iteration order) ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ stable first-record flush order. flush is
  snapshot-then-emit (a notify handler that re-record()s lands in a fresh recorder, NOT double-emitted
  this pass; mirrors S037's swap-out). Cross-thread contract unchanged (S037 owns the mutex queue). Tests
  (doctest, Shouldly vocabulary, 8 TEST_CASEs): record ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â pending_count increments, has_pending true,
  distinct props coexist; per-property coalescing ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â two records same prop => 1 pending, first old (1) +
  latest new (8); three records same prop => earliest old (0) + newest new (3); flush order ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â once per
  property in first-record order (Width/Height/Depth) with correct (old,new), returns count, recorder
  cleared, 2nd flush = 0; record-during-flush ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â re-record lands in fresh state (1 emit this pass) then
  flushes next call; clear ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â drops all, flush after clear = 0; empty flush ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â 0, callback invoked for
  nothing; owned-name ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â record survives the registering key's death + same-name value coalesces.
  VERIFIED BOTH CI LANES: msvc-debug (VS 2026) via verify_s038.cmd ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â configure/build warning-clean,
  ctest 1/1 PASS (100% tests passed); ubuntu-clang (Ubuntu VM clang 21, shared folder /mnt/hgfs/VamlPOC)
  configure/build + ctest 1/1 PASS (only pre-existing upstream <ciso646> doctest deprecation warning
  from third_party, not this TU). windows-clang-debug is NOT a lane (not built/tested/flagged per
  standing rule). No MINIMAL REAL introduced/resolved (nothing to backfill; ChangeRecorder's S041
  consumer is S041's own step). No NEW md files (directive honored). Next = S039.

- [x] S039 Effective-value resolution: local ?? inherited-from-parent ?? default; inherit-only flags ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦
  SURVIVOR: `src/core/include/vaml/core/DependencyProperty.h` (NEW) + `src/core/src/DependencyProperty.cpp`
  (NEW) + `src/core/include/vaml/core/PropertyRegistry.h` (MOD +global()) +
  `src/core/src/PropertyRegistry.cpp` (MOD +global() Meyers singleton) +
  `src/core/include/vaml/core/PropertyFlags.h` (MOD +InheritOnly=1<<3) +
  `src/core/src/PropertyKey.cpp` (MOD describe() +InheritOnly) +
  `src/core/include/vaml/core/DependencyObject.h` + `src/core/src/DependencyObject.cpp` (MOD:
  GetEffectiveValue + parent link + Get delegates) + `test/unit/DependencyProperty_test.cpp` (NEW) +
  `test/unit/EffectiveValue_test.cpp` (NEW) + `build/verify_s039.cmd` (NEW, verify_s038-pattern
  msvc-debug lane) + src/core/CMakeLists.txt + test/unit/CMakeLists.txt (MOD) + docs/roadmap.md (MOD
  S039 [DONE]) + session-context.md (MOD). AUTHORS the recorded S036 RE-SCOPE: the global static
  registry + `DependencyProperty::Register` landing site (S034-recorded, previously "S036's concern").
  New REAL public symbol `vaml::DependencyProperty` (NOT MINIMAL REAL / NOT a stub ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â fully implemented,
  tested, first real consumer is S039's own effective-value resolution): pimpl'd (class Impl +
  unique_ptr), `Register(std::string_view name, ValueType type, std::string_view ownerTypeId,
  PropertyMetadata)` inserts a PropertyKey into the new `PropertyRegistry::global()` (static-local
  Meyers singleton, thread-safe init, name-unique ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ PropertyAlreadyRegistered on duplicate, store
  unchanged) + a companion function-local-static table holding the full (key, metadata) definition
  incl. flags (Inherits / InheritOnly) + default; `TryGet(name)` (null) / `Get(name)` (throws
  PropertyNotFound); `property_id()` (wraps the OWNED name ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ lifetime-safe view),
  `metadata()` / `flags()`. Type coherence guard: Register() throws ValueTypeMismatch when
  type != metadata.boundType(). `PropertyFlags` gains `InheritOnly = 1 << 3` (independent bit;
  meaningful only with Inherits) + describe() lists it. `DependencyObject::GetEffectiveValue(PropertyId)`
  (READ-SIDE pull model; the real S039 primitive): NOT Inherits ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ local ?? default (unchanged S036
  semantics); Inherits (no InheritOnly) ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ local ?? inherited-from-parent ?? default; InheritOnly ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢
  inherited-from-parent ?? default (own local override does NOT contribute). Inheritance descriptor
  (flags+default) sourced from the GLOBAL DependencyProperty where globally registered, falling back
  to the object's own metadata for object-scoped S036 registration; local value always from the
  object's per-object store. `Get` delegates to GetEffectiveValue (backward compatible ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â no existing
  test uses Inherits/InheritOnly). Internal `nearest_ancestor_local` is a private static member
  (same-class access to pimpl) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â a straight upward scan for the nearest ancestor with a LOCAL
  override, which is exactly equivalent to recursing each ancestor's effective value (an intermediate
  ancestor with no local defers to ITS ancestor) without the throw hazard; TOLERANT by contract (an
  ancestor that does not register the property, or has no local, contributes nothing ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â never throws);
  public GetEffectiveValue still throws PropertyNotFound for an unregistered id. Added single
  non-owning `set_parent(DependencyObject*)/parent()` link (read-side seed, NOT lazy, NO-GAP: S040
  inheritance cascade + S120 UIElement/visual tree consume it; setting it triggers NO cascade ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â write/
  cascade side is S040, recorded NOT silent). Tests (doctest, Shouldly vocabulary):
  DependencyProperty_test.cpp (5 TEST_CASEs): Register installs globally + TryGet/Get retrieve it +
  property_id/flags/default round-trip; name-unique re-register throws PropertyAlreadyRegistered,
  original kept; type-coherence throws ValueTypeMismatch, nothing stored; TryGet null / Get throws
  PropertyNotFound for unregistered; flags() round-trips Inherits|InheritOnly.
  EffectiveValue_test.cpp (8 TEST_CASEs): non-inheriting local ?? default (Get delegates);
  plain Inherits 1-level child inherits parent's local; multi-level inherits great-grandparent;
  nearest-ancestor-with-value (intermediate empty local defers to IT ancestor); InheritOnly ignores
  OWN local override + still inherits from chain; InheritOnly with no ancestor ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ default;
  empty parent chain ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ default; GetEffectiveValue throws PropertyNotFound for unregistered. GOTCHA
  (fixed pre-build): DependencyProperty::Register is GLOBALLY name-unique, so the test helper
  registers each inherited property's global descriptor EXACTLY ONCE (tracked via a suite-local
  static set) and only mirrors onto each object's own store ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â repeated hostInherited() across
  objects no longer re-throws PropertyAlreadyRegistered. GOTCHA (fixed at build): header declares
  `class Impl;` but cpp defined `struct Impl` ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ MSVC C4099 (warnings-as-errors) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â cpp now uses
  `class Impl`. VERIFIED BOTH CI LANES: msvc-debug (VS 2026) via verify_s039.cmd ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â configure/build
  warning-clean, ctest 1/1 PASS (100% tests passed; 121 cases / 674 asserts); ubuntu-clang (Ubuntu VM
  clang 21, shared folder /mnt/hgfs/VamlPOC) configure/build + ctest 1/1 PASS (only pre-existing
  upstream <ciso646> doctest deprecation from third_party ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â not this TU, already non-error in test
  CMake). windows-clang-debug is NOT a lane (not built/tested/flagged per standing rule). NO-GAP
  accounting: S040 = write/cascade side (parent change ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ recompute child effective value without
  child write; child Set vs inherited old) consumes the parent-link seed + global table ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â
  authored at S040, never a silent backfill; S120 (visual tree) consumes the parent link. No MINIMAL
  REAL introduced/resolved. No NEW md files (directive honored). Next = S040.

- [x] S040 Inheritance cascade: parent change recomputes child effective value without child write ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦ (test)
  SURVIVOR: `test/unit/InheritanceCascade_test.cpp` (NEW) + `test/unit/CMakeLists.txt` (MOD
  +InheritanceCascade_test.cpp) + `build/verify_s040.cmd` (NEW, verify_s039-pattern msvc-debug lane)
  + `docs/roadmap.md` (MOD S040 [DONE]) + `session-context.md` (MOD). TEST-ONLY step (ZERO new product
  symbols) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â the S039 pull model already delivers correct cascade semantics; S040 PROVES the S039-
  deferred write/cascade side is real and exercises the genuinely new dimensions S039 did not show:
  (1) dynamic ordering ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â parent change AFTER the child is attached (S039 only set the chain up then
  read once, static); (2) local-vs-inherited interaction ("child Set vs inherited old"). CONTRACT
  boundary (recorded NO-GAP, NOT silent): S040 does NOT build descendant tracking / notifications ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â
  that is S041's explicit scope (ChangeQueue + notification processor). Test harness mirrors
  EffectiveValue_test.cpp: hostInherited() registers the GLOBAL DependencyProperty descriptor once per
  name (name-unique ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ PropertyAlreadyRegistered avoided via suite-local static set) + mirrors onto each
  object's own store; kInh = PropertyFlags::Inherits. 6 TEST_CASEs (doctest + Shouldly vocabulary
  SHOULD/ShouldThrow):
  (1) CASCADE AFTER ATTACH ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â child.set_parent(parent) FIRST, then parent.Set(11) ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ child effective 11,
  then parent.Set(22) ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ child effective 22; no child Set ever called (recompute is purely the read-side
  pull over an empty child local store).
  (2) MULTI-LEVEL ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â 3-level chain wired in place (leafÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢g3ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢g2ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢g1), THEN g1.Set(5) ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ leaf 5; g1.Set(9)
  ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ leaf 9 (intermediate g2/g3 hold no local, leaf follows; no child write).
  (3) LOCAL-WINS ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â child sets its own local 7 BEFORE parent changes; parent then Set(99) ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ child
  effective STAYS 7 (cascade respects a real local override, does not sweep it).
  (4) INHERITONLY RECOMPUTE ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â Inherits|InheritOnly property; child Set(100) is IGNORED; parent.Set(3) ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢
  child 3; parent.Set(4) ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ child 4 (own local never contributes, parent change DOES recompute).
  (5) CHILD SET VS INHERITED OLD ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â child first inherits parent 20, then writes its own local 31 (child
  Set vs inherited old); a LATER parent.Set(44) must NOT clobber the child write ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ child effective STAYS
  31.
  (6) PARENT REVERTS ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â parent Set(8) ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ child 8, parent Set(0) ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ child 0, parent Set(6) ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ child 6; child
  tracks both moves purely through the read-side pull, no child Set anywhere.
  VERIFIED BOTH CI LANES: msvc-debug (VS 2026) via verify_s040.cmd ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â configure/build warning-clean,
  ctest 1/1 PASS (100% tests passed; 127 cases / 684 asserts); ubuntu-clang (Ubuntu VM clang 21, shared
  folder /mnt/hgfs/VamlPOC) configure/build + ctest 1/1 PASS (only pre-existing upstream <ciso646>
  doctest deprecation from third_party ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â not this TU, already non-error in test CMake).
  windows-clang-debug is NOT a lane (not built/tested/flagged per standing rule). NO-GAP accounting:
  S040 consumes the S039 parent-link seed + global DependencyProperty table and verifies the
  S039-deferred "write/cascade side" is real ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â resolved here by proof, not silently dropped. Push
  notification of descendants is explicitly S041's scope (recorded, NOT silent). No MINIMAL REAL
  introduced/resolved. No NEW md files (directive honored). Next = S041.

- [x] S041 Change notification pipeline: ChangeQueue (per-thread) + processor drains in registration order ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦
  SURVIVOR: `src/core/include/vaml/core/{ChangeQueue,ChangeProcessor}.h` (NEW) +
  `src/core/src/{ChangeQueue,ChangeProcessor}.cpp` (NEW) + `test/unit/{ChangeQueue,ChangeNotificationPipeline}_test.cpp`
  (NEW) + `build/verify_s041.cmd` (NEW, verify_s040-pattern msvc-debug lane) + MOD
  `src/core/include/vaml/core/DependencyObject.h` + `src/core/src/DependencyObject.cpp` +
  `src/core/CMakeLists.txt` (+ChangeQueue.cpp +ChangeProcessor.cpp) + `test/unit/CMakeLists.txt`
  (+2 test TUs) + `docs/roadmap.md` (S041 [DONE]) + `session-context.md` (MOD). Delivers the S041
  headline as THREE REAL public symbols (none MINIMAL REAL, none STUB):
  (1) `vaml::ChangeQueue` ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â per-thread pending NOTIFICATION queue; S037's per-object VALUE queue
  generalized onto the notification axis (S037's OWN resolution point ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â NO backfill). Cross-thread
  `enqueue(id,old,new)` / `pending_count()` (mutex); affinity-thread-only `process(NotifyFn)` /
  `clear()` (throw WrongThreadException from a non-affine thread, mirroring S037's ProcessPendingChanges
  D3 NO-LAZY guard); FIFO swap-out (a record during process lands in a fresh queue, never double-emitted
  this pass); owned-name gotcha honored (each item keys on an OWNED std::string, never the PropertyId
  view).
  (2) `vaml::ChangeProcessor` ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â "drains in registration order" per the headline: owns a ChangeQueue and
  CONSUMES S038's ChangeRecorder INSIDE the drain, so two enqueues of the same property before a drain
  collapse to ONE notification (first old kept, latest new adopted ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â S042's coalescing outcome realized
  end-to-end against live objects). `process(NotifyFn)` drains the snapshot then flushes the recorder;
  affinity-thread-only.
  (3) `DependencyObject` (MOD) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â three additions: (a) registration-ordered subscriber registry ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â
  `using PropertyChangedHandler = std::function<void(PropertyId,const Value&,const Value&)>;`
  `add_property_changed(PropertyId, handler)` -> monotonically-increasing `PropertyChangedToken` (never
  reused), `remove_property_changed(token)` (O(n), preserves registration order of what remains; a re-add
  appends at the end); fired ONLY on an actually applied change (additive dispatch ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â the existing metadata
  `with_changed` cb + S037 SetResult::{APPLIED,REJECTED,NO_CHANGE,QUEUED} + marshalling tests all stay
  green). (b) per-object notification queue + `ProcessNotificationQueue()` (affinity-thread-only,
  WrongThreadException guard; drains via ChangeQueue + coalesces through ChangeRecorder at drain; fires
  this object's registration-ordered subscribers) + `pending_notification_count()`. (c) DESCENDANT PUSH ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â
  the S040-RECORDED obligation, delivered here NOT silent: `set_parent()` now ALSO registers the child
  into the parent's non-owning inheritance-scope (the INVERSE edge of the S039 parent link; `std::vector<
  DependencyObject*>` in Impl; unwind-on-reparent + dedupe guard). On an APPLIED Inherits/InheritOnly
  change (on-affine Set OR a drain-applied queued write via ProcessPendingChanges), the TWO-PHASE pull
  model runs: `collect_descendant_effectives` captures each registering descendant's CURRENT effective
  value BEFORE the store (recursive walk via `visit_scope`), and `notify_descendants` recomputes each
  descendant's NEW effective value AFTER the store and enqueues into that descendant's OWN notification
  queue ONLY when old != new. This means each descendant receives its OWN effective-old -> effective-new
  delta and is notified only when it ACTUALLY changes ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â S040 local-wins is respected THROUGH an
  intermediate (a local-wins node shields its entire subtree; a parent move that changes no descendant's
  effective pushes nothing). No visual tree is built: the inheritance-scope is DISTINCT from the visual
  children list (S121 create/attach/detach + walk, which composes with set_parent) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â recorded, no
  overlap, no gap. Tests (doctest + Shouldly vocabulary): ChangeQueue_test.cpp (8 TEST_CASEs: cross-thread
  enqueue/owner drain; FIFO order; empty drain=0; process-from-non-affine throws WrongThreadException;
  clear-from-non-affine throws; record-during-process fresh-queue (swap-out); clear; owned-name survival);
  ChangeNotificationPipeline_test.cpp (11 TEST_CASEs: ChangeProcessor coalescing 2-in->1-out first-old+
  latest-new; distinct props once each in first-record order; empty=0; DependencyObject registration-ordered
  dispatch via real descendant push; remove-token detach + re-add-appends-at-end; descendant push carries
  oldEffective->newEffective delta via ProcessNotificationQueue; multi-level great-grandparent->leaf; local-
  wins intermediate shields BOTH itself and its subtree (no push) + sanity grandchild notified when the child
  (the true source) changes; InheritOnly always inherits; unregistered descendant -> nothing pending; single
  subscriber fires once per push). VERIFIED BOTH CI LANES from first principles: msvc-debug (VS 2026) via
  verify_s041.cmd ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â configure/build warning-clean, ctest 1/1 PASS (100% tests passed); ubuntu-clang (Ubuntu VM
  clang 21, shared folder /mnt/hgfs/VamlPOC) configure/build + ctest 1/1 PASS (146 cases; the single in-suite
  "failed" assertion is the intentional S036_1 should_fail demonstration, marked not-failed; only pre-existing
  upstream <ciso646> doctest deprecation from third_party, not this TU). windows-clang-debug is NOT a lane
  (not built/tested/flagged per standing rule). NO-GAP accounting: S037's "S041 is its OWN resolution point
  (NO backfill)" is satisfied (ChangeQueue generalizes the per-object value queue); S038's "ChangeRecorder is
  S041's building block; S041 consumes that coalescing end-to-end" is satisfied (ChangeProcessor + the object's
  ProcessNotificationQueue both drain via ChangeRecorder); S040's "push notification of descendants is
  explicitly S041's scope" is satisfied (descendant push delivered above). S042 remains the formal end-to-end
  coalescing-vs-live-DependencyObject integration gate ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â its primitive outcome is already REALIZED-EARLY by
  S038 and now additionally integrated here (recorded NOT silent). No MINIMAL REAL introduced/resolved. No
  NEW md files (directive honored). Next = S042.

- [x] S042 Coalescing: two sets of same prop before drain == one notification (test) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦
  SURVIVOR: `test/unit/Coalescing_test.cpp` (NEW) + `test/unit/CMakeLists.txt` (MOD +Coalescing_test.cpp) +
  `build/verify_s042.cmd` (NEW, verify_s041-pattern msvc-debug lane) + `docs/roadmap.md` (MOD S042 [DONE],
  REALIZED-EARLY annotation refined to record the formal integration is now delivered at S042) +
  `session-context.md` (MOD). TEST-ONLY step (ZERO new product symbols) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â the formal LIVE-OBJECT
  coalescing integration gate the S042 headline names. The coalescing PRIMITIVE was REALIZED-EARLY by S038
  (ChangeRecorder coalesces per-property by design ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â first old kept, latest new adopted ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â tested in
  ChangeRecorder_test.cpp), and S041 wired it into ChangeProcessor + the live-object per-object notification
  queue + ProcessNotificationQueue. S042 closes the ONE remaining gap: S041 proved coalescing only on the
  STANDALONE ChangeProcessor and proved live descendant-push with a DRAIN BETWEEN Sets (two *sequential*
  notifications) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â it never proved the exact S042 headline: **two live Sets of the same prop before ONE
  drain == ONE notification**, first old kept + latest new adopted. New test TU (dedicated, mirroring the
  S040 test-step precedent of one proof file per step; reuses the ChangeNotificationPipeline hostInherited()
  global-descriptor harness verbatim, with S042-unique property names `Coalesce_S042_*` so the global
  name-unique DependencyProperty table cannot collide across TUs). 6 TEST_CASEs, all against live
  DependencyObjects through the real descendant-push ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ per-object notification queue ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢
  ProcessNotificationQueue path:
  (1) HEADLINE ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â parent/child Inherits (default 0); child subscriber registered; parent.Set(5) then
  parent.Set(8); raw queue depth == 2 (proves coalescing happens AT DRAIN, not enqueue); ONE
  ProcessNotificationQueue() ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ subscriber fired EXACTLY ONCE with (old=0 first-kept, new=8 latest-adopted),
  queue depth 0.
  (2) THREE SETS ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ ONE notification ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â Set 5,7,9 ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ one notif (old=0, new=9): earliest old + newest new.
  (3) DRAIN IS THE BOUNDARY ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â Set 5 ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ drain (one notif 0ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢5), Set 8 ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ drain (one notif 5ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢8): two sequential
  notifications, each its own delta, no cross-drain collapse.
  (4) DISTINCT PROPS DO NOT COALESCE ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â two Inherits props A,B on one parent; interleave A=1,B=2,A=3 then one
  drain ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ exactly TWO notifications, A (0ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢3) then B (0ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢2), in FIRST-RECORD order.
  (5) NO_CHANGE SET DOES NOT INFLATE ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â parent.Set(5) then parent.Set(5): the second is NO_CHANGE (no push),
  queue stays depth 1, one notification (0ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢5).
  (6) AGGREGATE CAPSTONE ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â mixed interleaved A=5,B=2,A=7,B=3 then one drain ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ exactly TWO notifications
  (per-property coalesced, each adopting its LATEST new: A 0ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢7, B 0ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢3), never four.
  VERIFIED BOTH CI LANES from first principles: msvc-debug (VS 2026) via verify_s042.cmd ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â configure/build
  warning-clean, ctest 1/1 PASS (100% tests passed); ubuntu-clang (Ubuntu VM clang 21, shared folder
  /mnt/hgfs/VamlPOC) configure/build + ctest 1/1 PASS (152 cases; the single in-suite "failed" assertion is
  the intentional S036_1 should_fail demonstration, marked not-failed; only pre-existing upstream <ciso646>
  doctest deprecation from third_party, not this TU). windows-clang-debug is NOT a lane (not built/tested/
  flagged per standing rule). NO-GAP accounting: S042 delivered its own outcome + proof at its own step ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â
  nothing deferred to a silent backfill; NO-GAP satisfied by construction. No MINIMAL REAL introduced/
  resolved. No NEW md files (directive honored). Next = S043.

- [x] S043 Attached properties: key owned by A settable on B; registry supports ownerÃƒÂ¢Ã¢â‚¬Â°Ã‚Â class ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦
  SURVIVOR: `src/core/src/DependencyObject.cpp` (MOD ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â attached-aware global resolution) +
  `src/core/include/vaml/core/DependencyProperty.h` + `src/core/src/DependencyProperty.cpp` (MOD
  +owner_type_id()) + `test/unit/AttachedProperty_test.cpp` (NEW) + `test/unit/CMakeLists.txt` (MOD
  +AttachedProperty_test.cpp) + `build/verify_s043.cmd` (NEW, verify_s042-pattern msvc-debug lane) +
  `docs/roadmap.md` (MOD S043 [DONE]) + `session-context.md` (MOD). Makes DependencyObject ATTACHED-AWARE
  by GLOBAL RESOLUTION (the S043 headline): a globally-registered DependencyProperty (key owned by A) is
  now settable + gettable on a plain object B that NEVER calls register_property. Core mechanics:
  (1) PROPERTY-METADATA PROVIDER refactor ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â `Impl::PropEntry` now has TWO modes (exactly one active):
  OWNED (object-scoped register_property owns its move-only PropertyMetadata in a `unique_ptr<PropertyMetadata>`
  field `owned`) or BORROWED (an attached property materialized onto the object holds a NON-OWNING
  `const PropertyMetadata* borrowed` pointer to the GLOBAL descriptor). `PropEntry::metadata()` returns
  whichever is active. This is the S043 KEY DESIGN DECISION: the move-only PropertyMetadata is NEVER copied
  into an object's entry ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â the global DependencyProperty (process-lifetime companion table) is the single
  owner; per-object attached entries borrow it by pointer (no dangle: process-lifetime table). (2)
  GetEffectiveValue()/Get() ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â three-way resolution: in-store ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ stored entry metadata + local; not-in-store
  but GLOBALLY registered ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ global metadata + empty local (`inherited ?? global-default`, object needs NO
  local entry to read an attached property); NEITHER ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ PropertyNotFound (unchanged ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â unregistered names
  still throw). (3) Set() ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â resolves the metadata PROVIDER non-owning WITHOUT mutating the store first
  (S037 cross-thread contract: coerce+validate run BEFORE any store touch and a NON-affine Set NEVER
  mutates the affinity-owned store); wrong-thread POSTS to the pending queue (no materialization);
  ON THE AFFINE THREAD it LAZILY MATERIALIZES a BORROWED store entry for the attached property (so the
  local override has a home) then runs the UNCHANGED S035 pipeline (coerce ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ validate(coerced) ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢
  fire-on-real-change ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ store ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ S041 descendant push). (4) ProcessPendingChanges() ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â a queued write whose
  name is globally-registered-but-not-materialized now materializes a BORROWED entry (affinity-thread drain,
  legal) and applies, instead of skipping silently (SKIP only when NOT globally registered). (5)
  `DependencyProperty::owner_type_id()` (new public accessor returning `const std::string&` from the stored
  key's ownerTypeId, threaded through the DependencyProperty::Impl + Register) surfaces the DECLARING owner
  so "owner ÃƒÂ¢Ã¢â‚¬Â°Ã‚Â  class" is provable. Make-existing-Set/Get-attached-aware (no new SetValue/GetValue surface) is
  the minimal, roadmap-faithful choice (WPF-faithful: value stored per-object, definition owned once by A).
  Name-keyed, owner-as-metadata design is PRESERVED (S032) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â S043 does NOT introduce same-name-multi-owner
  (would break global name-uniqueness). Tests (doctest, Shouldly vocabulary; S043-UNIQUE global names
  `{Left,Theme,Dock,CanvasTop,Margin}_S043_*` via a suite-local `global_seen()` set so the name-unique
  global DependencyProperty table cannot collide):
  (1) HEADLINE ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â attached Set(5) -> APPLIED; Get -> 5; isolated per-object; etc. (see roadmap S043 line).

- [x] S044 Attached set/get + inheritance + notification tests ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦
  SURVIVOR: `test/unit/AttachedInheritanceNotification_test.cpp` (NEW) + `test/unit/CMakeLists.txt` (MOD
  +AttachedInheritanceNotification_test.cpp) + `build/verify_s044.cmd` (NEW, verify_s043-pattern msvc-debug
  lane) + `src/core/include/vaml/core/DependencyObject.h` + `src/core/src/DependencyObject.cpp` (MOD ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â
  the S044 push-walk broadening) + `docs/roadmap.md` (MOD S044 [DONE]) + `session-context.md` (MOD).
  Delivers the S044 headline end-to-end against live DependencyObjects AND closes a REAL GAP discovered
  while tracing the headline (NOT a silent omission ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â a sanctioned product extension authored at the SAME
  step as the tests, per NO-GAP): **passive attached-Inherits descendants were never notified**.
  THE GAP: S041's descendant push (`visit_scope`) captured a descendant ONLY when it already had a local
  store entry (`store.find(name) != end()`). For an attached Inherits property, a PURELY-PASSIVE inheriting
  child (never registers, never Set, never materializes a borrowed entry) reads the inherited value
  correctly via S043's global-resolved `GetEffectiveValue`, but was NEVER captured -> NEVER notified when
  the parent's attached Inherits value changed. Only descendants that artificially materialized a store
  entry (which makes them local-wins, so they never inherit-and-notify) were notified ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â leaving S044's
  "inheritance + notification" headline hollow.
  PRODUCT CHANGE (minimal, scoped to the S041 push walk): `collect_descendant_effectives` /
  `visit_scope` gain a `captureAttachedInherits` flag (threaded from BOTH Set() and ProcessPendingChanges()).
  It is computed as: the property carries the GLOBAL `Inherits` flag (from `DependencyProperty::TryGet(id.name())
  ->flags()`, already resolved as `dp`) AND the ORIGIN's own store entry is BORROWED (`entry.owned == nullptr`
  ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â the precise "attached" signal: this object hosts the value but does NOT own the property, S043 design).
  When set, `visit_scope` captures a descendant even if it has NO local store entry. `notify_descendants`
  still filters to real change (`old != new`), so over-capturing a passive descendant that ends up unchanged
  is harmless ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â shielded / local-wins / non-participating descendants still produce zero notifications. An
  object-scoped Inherits property keeps the EXACT S041 behavior (the origin OWNS its entry, so
  captureAttachedInherits stays false) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â verified by the existing S040/S041/S042/S043 tests staying green.
  No new public symbol introduced.
  Tests (doctest, Shouldly vocabulary; S044-UNIQUE global names `{AttachedTheme,AttachedOpacity,AttachedBrush,
  AttachedCoalesce,AttachedMarshalled,AttachedAccent}_S044_*` via a suite-local `global_seen()` set so the
  name-unique global DependencyProperty table cannot collide; `registerAttachedGlobal` registers ONLY on the
  global registry with owner A ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â objects do NOT locally register, the true attached premise):
  (1) ATTACHED-INHERITS CASCADE (headline) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â attached Inherits (owner AppOwner_A) never locally registered on
  parent/child; orphan read -> global default 0; parent.Set(5) -> APPLIED, child (unattached read) inherits 5;
  parent.Set(9) -> child follows 9 (read-side cascade on the attached axis).
  (2) ATTACHED INHERITED + LOCAL-WINS ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â child Set its own attached 7 first (local-wins, borrowed entry
  materialized); later parent.Set(44) does NOT clobber -> child effective stays 7.
  (3) PASSIVE DESCENDANT IS NOTIFIED (the product gap-fix under test) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â parent/child attached Inherits; child is
  purely passive (no Set ever), subscribes; parent.Set(8) -> child.pending_notification_count()==1; child.
  ProcessNotificationQueue() fires EXACTLY once (old=0 inherited default, new=8). This FAILS on the pre-S044
  walk (never captured) and PASSES post-S044.
  (4) ATTACHED COALESCING ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â parent.Set(5) then parent.Set(8) before one passive-child drain -> raw queue depth
  2 (coalescing at drain), ONE notification (old=0 first-kept, new=8 latest-adopted) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â S042 on the attached axis.
  (5) WRONG-THREAD ATTACHED INHERITS -> QUEUED -> DRAIN -> NOTIFIED ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â non-affine parent.Set(9) -> QUEUED, pending
  1, parent.Get still 0, child.pending_notification_count()==0; affinity parent.ProcessPendingChanges() -> applied
  (parent.Get 9, child pending 1) -> child.ProcessNotificationQueue() fires (0->9).
  (6) OWNER != CLASS + SHADOWING + THROW ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â attached owner AccentOwner_A read back via dp.owner_type_id() while
  plain objects store (owner != class preserved on the inheriting axis too); a locally-registered same-name
  SHADOWS the global path (local default 99 governs, not global 0); an unregistered name still throws
  PropertyNotFound from both Get and Set.
  VERIFIED BOTH CI LANES from first principles: msvc-debug (VS 2026) via verify_s044.cmd ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â configure/build
  warning-clean, ctest 1/1 PASS (100% tests passed); ubuntu-clang (Ubuntu VM clang 21, shared folder
  /mnt/hgfs/VamlPOC) configure/build + ctest 1/1 PASS (163 cases; the single in-suite "failed" assertion is the
  intentional S036_1 should_fail demonstration, marked not-failed; only pre-existing upstream <ciso646> doctest
  deprecation from third_party, not this TU). windows-clang-debug is NOT a lane (not built/tested/flagged per
  standing rule). NO-GAP accounting: the attached-Inherits notification gap was found, DECIDED in the plan as a
  sanctioned same-step product extension (not silent backfill), and its resolution is authored at S044 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â no
  feature left for a silent future step. No MINIMAL REAL introduced/resolved. No NEW md files (directive
  honored). Next = S045.
  (1) HEADLINE ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â attached Set/Get on B WITHOUT B calling register_property (key owned by CanvasOwner_A);
  Get resolves the global default (0) before any write; Set(5) ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ APPLIED; Get ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ 5; a SECOND independent B
  object does NOT see the first B's value (value stored PER-OBJECT, definition owned once by A).
  (2) EFFECTIVE = local ?? inherited ?? global-default ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â orphan (no parent, no local registration) reads the
  global default; parentÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢child chain neither locally registers, parent attached-Set(7) ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ APPLIED, child
  (unattached read) INHERITS 7 through the global-resolved pull; child local Set(9) ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ 9 wins (local ??).
  (3) OWNER ÃƒÂ¢Ã¢â‚¬Â°Ã‚Â  CLASS ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â key registered under owner DockPanelOwner_A is settable+gettable on plain object B;
  `dp.owner_type_id()` reads back "DockPanelOwner_A" BOTH before and after B stores a value.
  (4) WRONG-THREAD ATTACHED SET ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ QUEUED (marshalling parity) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â affine attached Set applies + pending 0;
  a non-affine attached Set(8) ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ QUEUED, pending 1, Get still 1 (not applied); affinity-thread drain ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢
  pending 0, Get 8.
  (5) BACKWARD COMPAT ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â a property both globally registered AND then locally registered on B with a
  DIFFERENT default: B's LOCAL registration SHADOWS the global descriptor (Get returns B's local default
  99, not the global 0; local Set path governs). An UNREGISTERED (not local, not global) name still throws
  PropertyNotFound from BOTH Get and Set.
  VERIFIED BOTH CI LANES from first principles: msvc-debug (VS 2026) via verify_s043.cmd ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â configure/build
  warning-clean, ctest 1/1 PASS (100% tests passed); ubuntu-clang (Ubuntu VM clang 21, shared folder
  /mnt/hgfs/VamlPOC) configure/build + ctest 1/1 PASS (157 cases; the single in-suite "failed" assertion is
  the intentional S036_1 should_fail demonstration, marked not-failed; only pre-existing upstream <ciso646>
  doctest deprecation from third_party, not this TU). windows-clang-debug is NOT a lane (not built/tested/
  flagged per standing rule). NO-GAP accounting (recorded, NOT silent): S044 = the DEDICATED attached
  inheritance + notification test gate (attached Inherits cascade + descendant notification on live objects)
  ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â S043 delivers the enabling resolution + its own basic attached tests; S044 is the separate formal proof
  of the inheritance/notification axes (mirroring S042's test-step precedent), NEVER a silent backfill.
  No MINIMAL REAL introduced/resolved. No NEW md files (directive honored). Next = S044.

- [x] S045 ReadOnly enforcement (Set throws) + validation (out-of-range rejects) + all flags honored ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦
  SURVIVOR: `src/core/include/vaml/core/DependencyObject.h` + `src/core/src/DependencyObject.cpp` (MOD) +
  `test/unit/ReadOnlyAndValidation_test.cpp` (NEW) + `test/unit/CMakeLists.txt` (MOD
  +ReadOnlyAndValidation_test.cpp) + `build/verify_s045.cmd` (NEW, verify_s044-pattern msvc-debug lane) +
  `docs/roadmap.md` (MOD S045 [DONE]) + `session-context.md` (MOD). Delivers the S045 headline end-to-end:
  **ReadOnly enforcement (Set throws) + validation (out-of-range rejects) + all flags honored.**
  PRODUCT CHANGES (both authored at the same step as the tests, NOT silent backfills):
  (1) NEW `vaml::PropertyReadOnlyException : std::logic_error` (declared in DependencyObject.h alongside
  WrongThreadException) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â catchable specifically so a caller can distinguish a read-only rejection from a
  wrong-thread QUEUED result.
  (2) Set() READ-ONLY GUARD ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â placed IMMEDIATELY after metadata-provider resolution and BEFORE
  coerce/validate/affinity:
  `if (has_flag(md.flags(), PropertyFlags::ReadOnly)) throw PropertyReadOnlyException(...)`.
  Structural + VALUE-INDEPENDENT: throws even when `input` equals the current/default value (never
  REJECTED/NO_CHANGE); fires on ANY thread ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â the guard precedes the S037 affinity branch, so a wrong-thread
  read-only Set THROWS, never returns QUEUED/deferred to a drain; and a refused read-only write NEVER
  materializes a borrowed store entry (Get still reads the default). Rationale (recorded): a read-only
  refusal is structural and value-independent, so it must precede the S035 pipeline AND the marshalling
  branch by construction.
  (3) ProcessPendingChanges() DEFENSIVE apply-time guard ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â resolves the metadata provider BEFORE
  materializing an entry and SKIPS a queued write whose property is now read-only at apply time (covers the
  narrow edge where metadata became read-only between queue-time and drain; NO-LAZY ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â never apply a write to
  a read-only property, never materialize a borrowed entry for a refused read-only write). The Set() guard
  already blocks read-only writes from ever being queued, so this is defense-in-depth, not a hot path.
  SCOPE DECISION (recorded): read-only => Set throws UNCONDITIONALLY for any caller on any thread ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â an
  OWNER-INTERNAL write path (WPF lets the owning class set read-only DPs internally; our public surface has
  no owner gate) is deliberately DEFERRED + recorded as a S120+ controls concern, NOT silently omitted.
  ALL-FLAGS-HONORED scope (recorded, honest): Inherits (effective resolution) + InheritOnly + ReadOnly
  (throws) behave in this step; BindsTwoWayByDefault is CARRIED through key + metadata flags but has NO
  runtime behavior yet ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â genuine two-way binding is S242's explicit scope (recorded NO-GAP, not faked).
  Tests (doctest + Shouldly vocabulary; S045-UNIQUE global names via a suite-local `global_seen()` set +
  `registerObject`/`registerGlobal` harness; 7 TEST_CASEs):
  (1) READ-ONLY SET THROWS (object-scoped) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â register `PropertyFlags::ReadOnly`, Set(5) -> throws
  PropertyReadOnlyException; Get still reads the default 7; store unchanged (refused write never
  materialized a local override).
  (2) READ-ONLY THROWS EVEN FOR VALUE == CURRENT ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â Set(7) (equals current/default) -> THROWS, not NO_CHANGE
  (structural, value-independent).
  (3) READ-ONLY WRONG-THREAD THROWS (NEVER QUEUED) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â a non-affine Set(9) on a read-only property -> throws
  PropertyReadOnlyException (caught + rethrown via std::exception_ptr), pending_change_count()==0 (never
  queued), Get unchanged 0. Proves the guard precedes the S037 affinity branch.
  (4) GLOBALLY-REGISTERED READ-ONLY ATTACHED ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â global read-only (owner A), Set on B (no local registration)
  -> throws; Get works (global default 2); no borrowed entry materialized (Get still 2).
  (5) READ-ONLY THROW PRECEDES THE PIPELINE ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â a read-only property that ALSO has with_coerce (doubles) +
  with_validate (rejects all): Set(-3) -> throws PropertyReadOnlyException (if coerce ran -3->-6, if validate
  ran it would REJECT ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â the throw fires first), Get stays default 0.
  (6) VALIDATE OUT-OF-RANGE REJECTS (read-write) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â bounded validate [0,10]: in-range 8 -> APPLIED, store 8,
  changed cb fired once; out-of-range 42 and -3 -> each REJECTED, store unchanged 8, cb NOT fired.
  (7) ALL FLAGS HONORED (consolidated) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â (a) ReadOnly|BindsTwoWayByDefault on one key: Set throws +
  BindsTwoWayByDefault carried through key+metadata flags (no runtime behavior ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â S242 scope); (b)
  Inherits|InheritOnly global descriptor: parent.Set(5) -> child effective inherits 5 (effective resolution;
  child's own local does NOT contribute under InheritOnly).
  VERIFIED BOTH CI LANES from first principles: msvc-debug (VS 2026) via verify_s045.cmd ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â configure/build
  warning-clean, ctest 1/1 PASS (100% tests passed); ubuntu-clang (Ubuntu VM clang 21, shared folder
  /mnt/hgfs/VamlPOC) configure/build + ctest 1/1 PASS (only pre-existing upstream <ciso646> doctest
  deprecation from third_party, not this TU). windows-clang-debug is NOT a lane (not built/tested/flagged per
  standing rule). NO-GAP accounting: BindsTwoWayByDefault behavior is explicitly S242's scope (recorded, NOT
  silent); owner-internal read-only write is a recorded S120+ controls concern (NOT silent); nothing deferred
  to a silent backfill. No MINIMAL REAL introduced/resolved. No NEW md files (directive honored). Next = S046.

- [x] S046 DependencyProperty registration-time dependency validation (ValidateDependencies). ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦ (REDIRECTED)
  SURVIVOR: `test/unit/DependencyCallbacks_test.cpp` (NEW) + `test/unit/CMakeLists.txt` (MOD
  +DependencyCallbacks_test.cpp) + `build/verify_s046.cmd` (NEW, verify_s045-pattern msvc-debug lane) +
  `docs/roadmap.md` (MOD S046 [DONE]). REDIRECTED (user-accepted, ADR-style decision recorded in roadmap):
  the original title ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â a registry-level "ValidateDependencies" dependency-graph gate ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â was reviewed against
  real commercial WPF MVVM and judged INERT: the "children VM depending on a hasa DP" relationship is
  INSTANCE COMPOSITION, not static registry linkage (WPF itself declares property coupling inline via
  coercion and NEVER validates a dependency graph at registration). S046 instead delivers the WPF-faithful
  value hooks ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â coercion + per-set validation ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â end-to-end. SCOPE DECISION (recorded, NO-LAZY-LLM): the
  coerce ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ validate(coerced) ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ fire-on-real-change ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ store pipeline is ALREADY implemented in
  DependencyObject::Set/ProcessPendingChanges (S036/S037/S045) and the callbacks already live in
  PropertyMetadata (S034). Therefore S046 introduces NO new product symbol ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â a standalone
  DependencyProperty::CoerceValue seam or a duplicated CoerceValueCallback alias would be DEAD/redundant
  surface (precisely what S048's refactor gate removes). S046 instead proves the two dimensions previous
  steps left UNCOVERED, fully in the Shouldly vocabulary:
  (1) ATTACHED/GLOBAL axis ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â a globally-registered DependencyProperty (owner A) carrying with_coerce +
  with_validate, set on an object B that never locally registers it (the S043 attached premise): raw 150 ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢
  coerced 100 ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ validate(100) passes ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ APPLIED, Get 100 (the COERCED value stored); raw -5 ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ coerced 0 ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢
  APPLIED; raw 7 ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ coerced 7 ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ validate(7) rejects ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ REJECTED, store unchanged. Wrong-tag input on the
  global axis throws ValueTypeMismatch, store unchanged.
  (2) BACKWARD COMPAT ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â a globally-registered property with NO hooks stores as-given on the attached axis
  (APPLIED, then NO_CHANGE on repeat) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â S036 passthrough proven on the global/attached path.
  (3) WRONG-THREAD QUEUED COERCION ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â coerce+validate run AT QUEUE TIME (S037), so what is POSTED and later
  DRAINED is the COERCED value, not the raw input: non-affine Set(150) ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ QUEUED (coerced 100), drain stores
  100 (not 150) + cb(50ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢100); non-affine Set(-5) ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ QUEUED (coerced 0), drain stores 0 (not -5) + cb(10ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢0).
  Both global-axis coercion and coerced-value-at-drain were previously UNCOVERED (no existing test registered
  a global DependencyProperty with a coerce hook set on a live object, and none asserted the coerced value is
  what the wrong-thread drain stores). VERIFIED BOTH CI LANES from first principles: msvc-debug (VS 2026) via
  verify_s046.cmd ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â configure/build warning-clean, ctest 1/1 PASS (100% tests passed; 179 cases / 956 asserts);
  ubuntu-clang (Ubuntu VM clang 21, shared folder /mnt/hgfs/VamlPOC) configure/build + ctest 1/1 PASS (only
  pre-existing upstream <ciso646> doctest deprecation from third_party, not this TU). windows-clang-debug is
  NOT a lane (not built/tested/flagged per standing rule). NO-GAP: S046 resolved its recorded obligation
  (the registered dependency-validation intent) via the user-accepted redirect ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â nothing deferred to a silent
  backfill; the redirect itself IS the recorded resolution. No MINIMAL REAL introduced/resolved. No NEW md
  files (directive honored). Next = S047.

- [x] S047 Property-system unit-test suite complete; every numbered predicate via adapted Shouldly. ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦ (GATE)
  SURVIVOR: `docs/roadmap.md` (MOD S047 [DONE]) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â DOCS-ONLY, no code change, both CI lanes already green from
  the S046 run. S047 is the FORMAL suite-complete gate, the vocabulary REALIZED EARLY by S036_1..S036_5 and
  now CERTIFIED against the established convention. GATE MET (verified by grep across test/unit): ZERO
  unadapted feature `CHECK`/`CHECK_THROWS_AS` across every migrated property feature file (Value_test,
  Value_roundtrip_test, PropertyId_test, PropertyKeyRegistry_test, PropertyRegistryExhaustive_test,
  PropertyMetadata_test, DependencyObject_test) plus the NEW S046 file DependencyCallbacks_test.cpp ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â all
  pure Shouldly (SHOULD/ShouldThrow/ShouldlyString). SANCTIONED non-feature exceptions (deliberate,
  documented, NOT count as violations): PropertyMetadata_semantics_test.cpp (the S035 reference SPEC harness ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â
  rewriting it would alter the exact contract under test; its CHECKs + single CHECK_THROWS_AS stay),
  smoke_test.cpp (doctest runner/main), StringMaker_test.cpp (asserts the raw doctest::String output of the
  S036_2 stringifier itself), shouldly_test.cpp (tests the Shouldly predicate/routing library + the S036_1
  SHOULD/SHOULD_REQUIRE routing seam + the intentional should_fail demonstration that must stay green), and
  the REQUIRE precondition guards (null-deref gates in PropertyKeyRegistry/PropertyRegistryExhaustive). The
  FULL Sprint-1 property suite ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â now including DependencyCallbacks_test.cpp ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â is green on BOTH ci lanes
  (msvc-debug via verify_s046.cmd + ubuntu-clang via /mnt/hgfs/VamlPOC). No new product symbols. No MINIMAL
  REAL introduced/resolved. No NEW md files (directive honored). Next = S048.

- [x] S048 REFACTOR GATE(S1): unused removed, build+ctest green both compilers. ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦
  SURVIVOR: `docs/roadmap.md` (MOD S048 [DONE]) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â GATE step, no code change required. Full build + ctest
  green on BOTH ci lanes (windows-msvc via verify_s046.cmd's msvc-debug lane + ubuntu-clang via
  /mnt/hgfs/VamlPOC clang-debug) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â both verified from first principles in the S046 run. No dead/unused code to
  remove: S046 deliberately added NO product symbol (a standalone CoerceValue seam / duplicated
  CoerceValueCallback alias would have been dead surface ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â excluded by design at S046, NO-LAZY-LLM), and no
  documented public surface or MINIMAL REAL marker was touched across Sprint-1 closing. vaml-core builds
  warning-as-error CLEAN on both compilers (the only Clang warning remains the pre-existing upstream <ciso646>
  doctest deprecation from third_party, not VamlPOC source). ctest 1/1 PASS both lanes. Sprint-1 property
  system core is clean + green. No MINIMAL REAL introduced/resolved. No NEW md files (directive honored).
  Next = S049 (SURVIVOR + checkpoint advance to S080).

- [x] S049 SURVIVOR + checkpoint advance to S080. ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦
  SURVIVOR: `progress.md` (MOD ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â consolidated `## SPRINT 1 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â SURVIVOR` block + regenerated checkpoint to
  last=S049/next=S080) + `docs/roadmap.md` (MOD S049 [DONE]) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â DOCS-ONLY, no build (per plan). The
  consolidated Sprint 1 SURVIVOR block distills S030..S048 into the surviving-state picture a fresh LLM
  resumes from: full DP semantics (Value/PropertyKey/PropertyRegistry/PropertyId/PropertyMetadata/
  DependencyObject + marshalling + ChangeRecorder/ChangeQueue/ChangeProcessor + effective-value resolution +
  attached properties + read-only/validation + coercion value-hook proofs) all real + tested + refactor-gated;
  the Shouldly-for-doctest vocabulary is the standing suite convention (S047 certified zero unadapted feature
  CHECK); key design decisions recorded (owned-name store key, the S035 pipeline spec, move-only metadata
  never copied, descendant-push per-delta, the S046 ValidateDependenciesÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢value-hooks redirect); deferred items
  all recorded NOT silent (BindsTwoWayByDefaultÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢S242, owner-internal read-onlyÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢S120+ controls, UIElementÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢S120,
  store-race freedomÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢S620/S723, two-way bindingÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢S242). NO MINIMAL REAL resolved in Sprint 1 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â all four active
  seeds (vulkanÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢S161, toolsÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢S287, ColorÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢S089, ThicknessÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢S089) carry forward unresolved, NO-GAP intact. Checkpoint
  advanced: Sprint 1 CLOSED, next = S080 (Sprint 2 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â VAML markup parser, XML scanner). No NEW md files
  (directive honored), no build (docs-only per plan S049 bound). Next = S080.

### Sprint 3 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â Element / scene graph + measure/arrange layout  (S120ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Å“S139)

- [x] S132 Grid ColumnSpan/RowSpan + span-aware measure+arrange math ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦
  SURVIVOR: MOD src/core/include/vaml/core/Element.h + MOD src/core/src/Element.cpp + ADDED
  test/unit/GridSpan_test.cpp + MOD test/unit/CMakeLists.txt (+GridSpan_test.cpp) + ADDED
  build/verify_s132.cmd + MOD docs/roadmap.md + session-context.md (no commits; no NEW md files).
  RESOLVES the S131-recorded NO-GAP hand-off (the span dimension). PRODUCT:
  (1) ATTACHED `Grid.RowSpan`/`Grid.ColumnSpan` Int32 DPs (default 1, clamped ÃƒÂ¢Ã¢â‚¬Â°Ã‚Â¥ 1 and to grid bounds)
  registered globally under owner "Grid" via the S043 attached-aware path (mirroring Grid.Row/
  Grid.Column), with typed statics `Grid::set_row_span/set_column_span(row_span/column_span)`.
  (2) Span-aware two-pass measure/arrange in Element.cpp via a new static `resolve_grid_with_spans`
  (shared by MeasureOverride + ArrangeOverride): D2 Auto equal-division (a spanning child contributes
  desired/span-count to EACH spanned Auto track; single cell = span 1 = the S131 no-span behavior,
  NO-GAP preserved); D1 star min/re-award (a Star track that received a POSITIVE proportional share
  grows to at least its content minimum, so a spanning/single-cell child whose content exceeds the
  share re-awards the track to fit; a no-share star ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â totalStar==0 or zero weight ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â stays 0, keeping
  the S131 resolve contract); merged-span-cell re-measure of ONLY dirty children (S128); merged-span
  rect placement {anchor origin, ÃƒÅ½Ã‚Â£ spanned extents} honoring child alignment+margin, arranging ONLY
  dirty children (S128). Panel DesiredSize = ÃƒÅ½Ã‚Â£(non-star resolved) + ÃƒÅ½Ã‚Â£(star content-minima) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â the
  content-natural extent (upgrades S131's star=0 minimum, NO-GAP). No MINIMAL REAL introduced.
  TESTS (test/unit/GridSpan_test.cpp, 7 TEST_CASEs, Shouldly vocabulary, real "Grid.RowSpan"/
  "Grid.ColumnSpan" names): span DPs default 1 + set/read on a plain DependencyObject + raw S043
  Set/Get path; D2 auto equal-division (2-wide 50/50; 3-row 1/3); merged placement (origin at anchor,
  size = ÃƒÅ½Ã‚Â£ tracks); span clamp (huge ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ bounds, negative ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ 1); Stretch-in-merged-cell; D1 star
  re-award with spans. NO-GAP clean: S133 = the exhaustive star+span suite (recorded, not silent);
  S132 resolves the S131 span hand-off; no new MINIMAL REAL. Both CI lanes from first principles green:
  windows-msvc (build/verify_s132.cmd, msvc-debug warning-clean + ctest 1/1 PASS) + ubuntu-clang
  (/mnt/hgfs/VamlPOC clang-debug configure+build+ctest 1/1 PASS). Only Clang warnings = pre-existing
  upstream <ciso646> doctest deprecation (non-error, already downgraded). Next = S133.

- [x] S133 Grid star-sizing + span tests (exhaustive) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦
  SURVIVOR: ADDED test/unit/Grid_stretch_test.cpp + MOD test/unit/CMakeLists.txt
  (+Grid_stretch_test.cpp) + ADDED build/verify_s133.cmd + MOD docs/roadmap.md + session-context.md
  (no commits; no NEW md files). TEST-ONLY ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â zero new product symbols (the S040/S042/S044/S125/
  S128/S130 test-only precedent). S131/S132 recorded S133 as the named exhaustive star+span gate
  (NO-GAP, not a silent backfill): it asserts the REAL S132 Grid (span-aware two-pass +
  Grid.RowSpan/Grid.ColumnSpan) over the D1/D2 decisions settled at S132 (D2 = auto equal-division;
  D1 = star min/re-award). Instrument = a test-local RecordLeaf (S125/S130 pattern) recording the
  exact measure offer + final arrange slot on both axes, so Stretch-vs-Wrap and the merged-cell span
  extents are observable. 17 TEST_CASEs in five parts: PART A ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â star proportional sizing (1:1 equal;
  3:1 by weight; star after absolute+auto taken with clean-exact numbers; star clamp when fixed >
  available; totalStar==0 all-zero; zero-weight star beside a positive sibling; star never negative +
  D1 single-cell re-award). PART B ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â star+span (a spanning child re-awards the spanned star track D1;
  span across star+auto mixed; Stretch fills a merged star cell). PART C ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â auto+span D2 (2-wide 50/50;
  content-driven auto grows for a spanning child). PART D ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â span placement (mixed rowspan+columnspan
  merged rect; whole-grid span fills the grid; alignment honored inside the merged cell; span clamps).
  PART E ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â dirty-respect with spans (re-arranging ONLY a dirty spanning child leaves a clean sibling
  untouched). NO-GAP clean: no MINIMAL REAL introduced/resolved; S132 resolved the span surface at its
  own step, S133 is the recorded exhaustive suite, not a backfill. Both CI lanes from first principles
  green: windows-msvc (build/verify_s133.cmd, msvc-debug warning-clean + ctest 1/1 PASS) + ubuntu-clang
  (/mnt/hgfs/VamlPOC clang-debug configure+build+ctest 1/1 PASS). Only Clang warnings = pre-existing
  upstream <ciso646> doctest deprecation (non-error, already downgraded). Next = S134 (WrapPanel:
  flow, wraps at boundary, aligned rows).

- [x] S134 WrapPanel: flow, wraps at boundary, aligned rows ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦
  SURVIVOR: MOD src/core/include/vaml/core/Element.h + MOD src/core/src/Element.cpp + MOD
  test/unit/ElementFactoryRegistry_test.cpp (9ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢10 built-in count + WrapPanel resolve) + ADDED
  build/verify_s134.cmd + MOD docs/roadmap.md + session-context.md (no commits; no NEW md files).
  REAL panel ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â NEW element (no S087 seed existed). PRODUCT: `WrapPanel final : UIElement`
  (visual-tree + Measure/Arrange) with a real `WrapDirection` DP (reuses the S129 `Orientation`
  enum: Vertical=0, Horizontal=1), default HORIZONTAL (WPF-faithful), NOT inherited, self-registered
  via the S043 attached-aware global path (Meyers-static `ensure_wrappanel_properties_registered()`).
  Property NAME "WrapDirection" (owner "WrapPanel") ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â deliberately NOT "Orientation" (StackPanel owns
  it in the global registry; a same-name pair under a different owner would be ambiguous to the S091
  dotted-key reader). REAL flow/wrap/align algorithm in Element.cpp: MEASURE offers every child
  `Size::Infinite()` on BOTH axes (S123 infinity rule ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â a Stretch child given infinite wraps to
  content; CROSS axis ALSO unbounded so rows are CONTENT-HEIGHTED, refining the accepted "natural-
  width basis" to a "natural-extent basis" ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â recorded in Element.cpp) and re-measures ONLY
  is_measure_dirty() children (S128); Horizontal flows left-to-right, WRAPS when `rowWidth +
  childWidth > available.width` (checked ONLY when the row is non-empty, so a child wider than the
  whole boundary still gets its own row), DesiredSize = {max row width, ÃƒÅ½Ã‚Â£ row heights}; Vertical is
  the mirror (top-to-bottom columns, wrap on height, DesiredSize = {ÃƒÅ½Ã‚Â£ column widths, max column
  height}). ARRANGE reproduces the same wrap decisions (boundary = final_rect extent), gives each row
  its extent (row height = tallest child on Horizontal), places each child into its row slot
  {cursor, rowY, childDesiredW, rowHeight} via child->Arrange (honoring the child's OWN alignment +
  margin inside the slot via S124 compute_final_rect ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â a default-Stretch child fills the row's cross
  extent while keeping its natural flow extent = the "aligned rows" headline), re-arranging ONLY
  is_arrange_dirty() children (S128). Registered as a built-in (`register_builtin_elements()`
  +"WrapPanel") ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â count 9ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢10, so the S087 ElementFactoryRegistry_test.cpp size assertions updated at
  THIS step + a WrapPanel resolve assertion added HERE. NO ItemWidth/ItemHeight (fixed-item mode) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â
  not in the S134 title, recorded NO-GAP (deferred, not fabricated). NO MINIMAL REAL introduced.
  NO-GAP hand-off: S135 = the dedicated WrapPanel test suite (recorded, not silent). Both CI lanes
  from first principles green: windows-msvc (build/verify_s134.cmd, msvc-debug warning-clean + ctest
  1/1 PASS) + ubuntu-clang (/mnt/hgfs/VamlPOC clang-debug configure+build+ctest 1/1 PASS). Only
  Clang warnings = pre-existing upstream <ciso646> doctest deprecation (non-error, already
  downgraded). Next = S135.

- [x] S135 WrapPanel tests ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦
  SURVIVOR: ADDED test/unit/WrapPanel_test.cpp + MOD test/unit/CMakeLists.txt (+WrapPanel_test.cpp) +
  ADDED build/verify_s135.cmd + MOD docs/roadmap.md + session-context.md (no commits; no NEW md
  files). TEST-ONLY ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â zero new product symbols beyond S134 (the S040/S042/S044/S125/S128/S130/S133
  test-only precedent). S134 recorded S135 as its dedicated test suite (NO-GAP, not a silent
  backfill): it asserts the REAL S134 WrapPanel (flow/wrap/align + WrapDirection) over the S123
  infinity-aware finalizer, the S124 arrange math, and the S126/S127 dirty-driver. Instrument = a
  test-local RecorderLeaf (S125/S130 pattern) recording the exact measure offer + final arrange slot
  on both axes, so the wrap decision's natural-extent basis and the aligned-row slots are observable.
  14 TEST_CASEs, Shouldly vocabulary: derives UIElement via dynamic_cast; WrapDirection defaults
  Horizontal; typed round-trip Horizontal<->Vertical; WrapDirection resolves globally on a plain
  DependencyObject (S043 attached-aware); default-Horizontal overflow WRAPS to a new row; a child
  wider than the whole boundary still gets its own row; DesiredSize Horizontal = max row width / ÃƒÅ½Ã‚Â£
  row heights; children measured at NATURAL extent on BOTH axes (+inf offer); aligned rows ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â row
  height is the tallest child + a default-Stretch child fills the row height, per-child
  VerticalAlignment pins within its row (Top/Center/Bottom); vertical mirror ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â flows top-to-bottom +
  WRAPS to a new column + DesiredSize ÃƒÅ½Ã‚Â£ col widths / max col height; empty panel -> DesiredSize 0 +
  arrange no-op; dirty-respect (a clean child is NOT re-measured nor re-arranged; an
  arrange-invalidated child IS re-arranged but a clean one is not) + margin enters DesiredSize
  (S130_1) and is honored at arrange. NO-GAP clean: no MINIMAL REAL introduced/resolved; S134
  resolved the WrapPanel surface at its own step, S135 is the recorded suite, not a backfill. Both CI
  lanes from first principles green: windows-msvc (build/verify_s135.cmd, msvc-debug warning-clean +
  ctest 1/1 PASS) + ubuntu-clang (/mnt/hgfs/VamlPOC clang-debug configure+build+ctest 1/1 PASS).
  Only Clang warnings = pre-existing upstream <ciso646> doctest deprecation (non-error, already
  downgraded).

- [x] S136 Canvas: explicit position via attached Left/Top (attached props via S1) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦
  SURVIVOR: MOD src/core/include/vaml/core/Element.h + MOD src/core/src/Element.cpp + MOD
  test/unit/ElementFactoryRegistry_test.cpp (10ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢11 built-in count + Canvas resolve) + MOD
  docs/roadmap.md + session-context.md (no commits; no NEW md files). REAL panel ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â NEW element (no
  S087 seed existed). PRODUCT: `Canvas final : UIElement` (visual-tree + Measure/Arrange) with TWO
  REAL ATTACHED DPs, `Canvas.Left` + `Canvas.Top` (Double value path), default **NaN** = "not set"
  (the NaN contract introduced explicitly at THIS step ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â the S1 value path had no NaN convention
  before S136; user-directed at review to mirror WPF), NOT inherited, self-registered via the S043
  attached-aware global path (Meyers-static `ensure_canvas_properties_registered()`, registered
  globally under owner "Canvas" so they resolve on ANY DependencyObject). Typed statics
  `Canvas::set_left/set_top/left/top` + raw S043 Set/Get(PropertyId) path. REAL absolute-position
  panel algorithm in Element.cpp: MEASURE offers every child `Size::Infinite()` on BOTH axes (S123
  infinity rule, same basis as WrapPanel S134 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â a child measures at its NATURAL content extent
  regardless of explicit position) and re-measures ONLY is_measure_dirty() children (S128);
  DesiredSize = the BOUNDING EXTENT of the arranged children ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â per child `rightEdge = effLeft +
  child.desired.w`, `bottomEdge = effTop + child.desired.h` where `effX = is_nan(v) ? 0 : v`
  (NaN = "not set" -> effective 0), DesiredSize axis = max over children of that edge, clamped >= 0
  (never negative, never NaN ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â NaN axes zeroed BEFORE the max fold so no NaN reaches the bounding
  math). ARRANGE places each child at its EXPLICIT origin {effLeft, effTop} (NaN collapses to 0 =
  the canvas origin, effective top-left) at the child's measured NATURAL size via child->Arrange ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â
  honoring the child's OWN alignment + margin inside that slot through compute_final_rect (S124/
  S130_1 contract) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â so children at distinct origins are ABSOLUTE (no panel flow/stacking);
  re-arranges ONLY is_arrange_dirty() children (S128). Registered as a built-in
  (`register_builtin_elements()` +"Canvas") ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â count 10ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢11, so the S087 ElementFactoryRegistry_test
  .cpp size assertions updated at THIS step + a Canvas resolve assertion added HERE. NO MINIMAL REAL
  introduced. NO-GAP hand-off: S137 = the dedicated Canvas test suite (recorded, not silent). Both CI
  lanes from first principles green (the S136 product is most directly verified by S137's
  build/verify_s137.cmd, which runs the product + S137 tests): windows-msvc (build/verify_s137.cmd,
  msvc-debug warning-clean + ctest 1/1 PASS) + ubuntu-clang (/mnt/hgfs/VamlPOC clang-debug
  configure+build+ctest 1/1 PASS). Only Clang warnings = pre-existing upstream <ciso646> doctest
  deprecation (non-error, already downgraded). Next = S137.

- [x] S137 Canvas tests ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦
  SURVIVOR: ADDED test/unit/Canvas_test.cpp + MOD test/unit/CMakeLists.txt (+Canvas_test.cpp) +
  ADDED build/verify_s137.cmd + MOD docs/roadmap.md + session-context.md (no commits; no NEW md
  files). TEST-ONLY ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â zero new product symbols beyond S136 (the S040/S042/S044/S125/S128/S130/S133/
  S135 test-only precedent). S136 recorded S137 as its dedicated test suite (NO-GAP, not a silent
  backfill): it asserts the REAL S136 Canvas (explicit Left/Top via attached DPs + absolute
  placement) over the S123 infinity-aware finalizer, the S124 arrange math, and the S126/S127
  dirty-driver. Instrument = a test-local RecorderLeaf (S125/S130 pattern) recording the exact
  measure offer + final arrange slot on both axes. 14 TEST_CASEs, Shouldly vocabulary: derives
  UIElement via dynamic_cast; Canvas.Left/Top default to NaN (not set); typed round-trip on finite
  values; explicit 0 is DISTINCT from unset (NaN) ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â round-trips as 0; Left/Top resolve globally on a
  plain DependencyObject (S043 attached-aware, default NaN); children measured at NATURAL extent on
  BOTH axes (+inf offer); empty canvas -> DesiredSize 0 + arrange no-op; DesiredSize = bounding
  extent incl. finite offsets + NaN axes ignored as 0; DesiredSize clamps >= 0 for negative coords;
  a child at Left=10,Top=20 lands exactly there at measured size; an un-set axis (NaN) collapses to
  the origin (0,0); multiple children at distinct origins are ABSOLUTE (no panel flow); negative
  Left/Top honored at the origin; a margin'd child lands at its origin with margin honored (S130_1),
  a default-Stretch child keeps its natural extent (nothing to stretch into), dirty-respect (a clean
  child is NOT re-measured nor re-arranged; an arrange-invalidated child IS re-arranged but a clean
  one is not). NO-GAP clean: no MINIMAL REAL introduced/resolved; S136 resolved the Canvas surface at
  its own step, S137 is the recorded suite, not a backfill. Both CI lanes from first principles green:
  windows-msvc (build/verify_s137.cmd, msvc-debug warning-clean + ctest 1/1 PASS) + ubuntu-clang
  (/mnt/hgfs/VamlPOC clang-debug configure+build+ctest 1/1 PASS). Only Clang warnings = pre-existing
  upstream <ciso646> doctest deprecation (non-error, already downgraded). Next = S138.

- [x] S142 Collapse DependencyObject/UIElement pimpl + store into ONE block-resident unit ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦
  SURVIVOR: (dep:S141; rewrites S036/S037/S039/S120 in place; pimpl REMOVED, not preserved; rationale
  re-grounded on SPEED not ABI ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â ADR-003 revised + ADR-019 codified; Sprint 3.5 first retrofit
  implemented step). PRODUCT (src/core/include/vaml/core/{DependencyObject.h,UIElement.h} +
  src/core/src/{DependencyObject.cpp,UIElement.cpp}): removed `struct Impl; unique_ptr<Impl> impl_;`
  from BOTH DependencyObject and UIElement. DependencyObject now embeds ONE block-resident unit
  directly in its own allocation: `Store store_` = `class Store { PD slots_[kInline=8]; size_t count_;
  PD* overflow_; size_t overflowCap_; }` ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â inline-starting, heap-grown on demand (append-only overflow
  allocated ONLY when count_ exceeds kInline, never shrunk), lookup by interned string_view name via a
  small linear scan (cache-friendly, insertion-ordered, NO per-object heap hop, NO ~100%-of-access
  `impl_->` trampoline). SINGLE PD TYPE (no common/root): `struct PD { string_view name; Value local;
  PropertyMetadata* owned; const PropertyMetadata* borrowed; PropertyKey* stableKey; }` ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â owned
  (object-scoped register_property) + borrowed (attached/global descriptor) metadata COEXIST in one
  PD; move-only PropertyMetadata never copied (addressed by pointer). `stableKey` = a separate stable
  heap node so register_property's returned `const PropertyKey&` stays valid across overflow growth
  (PD relocation never invalidates it). CUSTOM MOVE: the pending mutex is now a DIRECT `mutable
  std::mutex queue_mutex_` (not inside a heap pimpl) and is NOT movable ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â ctor/assign move every
  movable member (store_, affinity_, parent_, subs_, next_sub_token_, notification_queue_,
  inheritance_scope_, pending_) + leave the destination mutex fresh; moved-from left empty. Ownership:
  `Store::release_slot` frees the two OWNED heap nodes (owned + stableKey) in dtor/move-assign,
  never `borrowed`. UIElement embeds `children_` (std::vector<unique_ptr<UIElement>>), `desired_size_`,
  `final_rect_`, `measure_dirty_`, `arrange_dirty_` inline ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â composing with the base Store into one
  block-resident unit. ChangeQueue.h include-cycle BROKEN: it no longer includes DependencyObject.h
  (which now holds a ChangeQueue BY VALUE); ChangeQueue.cpp includes DependencyObject.h for
  WrongThreadException. Queue_mutex marked mutable (const pending_change_count locks it, was reachable
  through the pointer pimpl before). register_property doc updated (stable-key mechanism replaces the
  unordered_map node-stability claim). All public API/method signatures UNCHANGED (behavior-exact
  retrofit). TESTS (NEW test/unit/BlockResidentStore_test.cpp, 6 TEST_CASEs): inline path (5 props,
  all-inline, register/Set/Get/NO_CHANGE); overflow growth (13 props > 8, all applied/rejected on the
  real S035 pipeline); stable key survives growth (returned ref valid across overflow re-adds); move
  preserves values + stable key; single PD owned+borrowed coexist (local prop + global Opacity);
  UIElement collapsed unit (Measure/Arrange/dirty flags/DPs still work with inline layout data).
  test/unit/CMakeLists.txt +BlockResidentStore_test.cpp. build/verify_s142.cmd (NEW, verify_s140 clone).
  Scope hand-offs recorded (NO-GAP): pending-mutex relocation + registry re-key onto S140 token + S141
  store = S143; registry accessor/signature migration (string_view returns, out-params) = S144; block
  pool for object allocation + children slab = S145. ADR-018: ZERO new locks. NO MINIMAL REAL
  introduced/resolved. VERIFIED BOTH CI LANES from first principles: windows-msvc (build/verify_s142.cmd,
  msvc-debug warning-as-error clean; test exe 619 test cases / 4735 assertions PASS / 0 failed ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â the
  single in-suite "failed" assertion is the intentional S036_1 should_fail demonstration) + ubuntu-clang
  (/mnt/hgfs/VamlPOC clang-debug configure+build+ctest 1/1 PASS; only pre-existing upstream <ciso646>
  doctest deprecation from third_party). windows-clang-debug is NOT a lane (not built/tested/flagged).
  No commits. Next = S143.

- [x] S143 Relocate the per-object pending mutex into the ChangeQueue; re-key
  ChangeQueue/ChangeRecorder/PropertyRegistry/DependencyProperty::GlobalTable onto the S140 token +
  S141 store ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â ÃƒÂ¢Ã…â€œÃ¢â‚¬Â¦
  SURVIVOR: (dep:S142; dep:S141; rewrites S037/S041/S038 in place; Sprint 3.5 second retrofit
  implemented step; executed under ADR-018 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â the kept S037/S041 lock, moved not created, ZERO new
  locks). PART A (mutex relocation): the per-object pending VALUE mutex physically relocated OUT of
  DependencyObject INTO its single thread-affine ChangeQueue. ChangeQueue is now the ONE cross-thread
  postbox owning BOTH item families under ONE mutex + ONE affinity marker: the S041 notification
  family (unchanged enqueue/process/clear/pending_count) AND the relocated S037 value-write family
  (NEW enqueue_value/drain_values/value_pending_count). DependencyObject's raw pending_/queue_mutex_
  members REMOVED; it now holds ONE ChangeQueue `queue_`; Set()'s non-affine branch ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢
  queue_.enqueue_value(...) returning QUEUED; pending_change_count() ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ queue_.value_pending_count();
  ProcessPendingChanges() ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ affinity guard + queue_.drain_values(lambda) (the S037 guard moved with
  the queue ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â WrongThreadException unchanged); descendant-push + ProcessNotificationQueue() ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ the SAME
  queue_'s notification half. Custom move simplified to moving every member (queue_ is movable;
  inheritance_scope_ declared before queue_ to satisfy -Wreorder-ctor). Swap-out preserved: drain
  swaps the value-write deque under the mutex so writers during a drain land in a fresh deque.
  PART B (re-key = S140 DECISION 1 resolution): new vaml::detail::NameTable<T> (src/core/src/NameTable.h)
  ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â a S141-pattern CONTENT-keyed probe store (no hash, no unordered_map; rotating-XOR-fold %255 probe
  (0xFF unreachable), chain walk disambiguated by full string_view content compare, append-only nodes,
  self-owned STABLE key storage, no transient std::string on find; insert() returns the stored value
  ptr or nullptr on duplicate). Re-keys: (B1) ChangeRecorder byName unordered_map<string_view,Pending>
  ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ NameTable<Pending> (flush uses find_key + owned Pending); (B2) ChangeQueue internal index +
  drain_values re-interns each drained name to the affinity thread's canonical interned view;
  (B3) PropertyRegistry global+instance map_ unordered_map<std::string,PropertyKey> ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢
  NameTable<PropertyKey>, public try_find/find/id_of/try_id now take std::string_view (existing
  std::string callers bind implicitly); (B4) DependencyProperty::GlobalTable byName
  unordered_map<std::string,unique_ptr<DependencyProperty>> ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ NameTable<unique_ptr<...>>, TryGet looks
  up by string_view content. Global/cross-thread stores stay CONTENT-keyed with self-owned storage
  (they cannot hold thread-local intern views ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â DECISION 1). All public signatures behavior-unchanged.
  TESTS (NEW test/unit/MutexRelocation_rekey_test.cpp, 9 TEST_CASEs / 259 assertions): S037 value-write
  family in the queue; drain_values non-affine throws (guard moved); wrong-thread Set QUEUED + affinity
  FIFO cbs; ProcessPendingChanges non-affine throws; swap-out preserved; value-write+notification
  coexist without cross-drain interference; drain re-interns to canonical view; PropertyRegistry
  content lookup (no transient string, dedupe throws, 200-key ref stability); DependencyProperty
  content lookup (register/TryGet/Get, dedupe throws + stores nothing, property_id/owner_type_id views
  stay valid). test/unit/CMakeLists.txt +MutexRelocation_rekey_test.cpp. build/verify_s143.cmd (NEW,
  verify_s142 clone). Scope hand-offs recorded (NO-GAP): three lookup-registry migration +
  public-accessor/signature migration (string_view returns, out-params) = S144; block pool + children
  slab = S145. ADR-018: ZERO new locks (global tables retain single-threaded-registration-then-read;
  only relocated lock = the standing S037/S041 ChangeQueue mutex). NO MINIMAL REAL introduced/resolved.
  VERIFIED BOTH CI LANES from first principles: windows-msvc (build/verify_s143.cmd, msvc-debug
  warning-as-error clean; test exe 628 test cases / 4994 passing assertions / 0 failed ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â the single
  in-suite "failed" assertion is the intentional S036_1 should_fail demonstration) + ubuntu-clang
  (/mnt/hgfs/VamlPOC clang-debug configure+build+ctest 1/1 PASS; 628 cases SUCCESS; only pre-existing
  upstream <ciso646> doctest deprecation from third_party). windows-clang-debug is NOT a lane (not
  built/tested/flagged). No commits. Next = S144.
- [x] S144 Migrate the three lookup registries to the token-keyed store + public accessor shape migration - DONE
  SURVIVOR: (dep:S143; rewrites S087/S089/S091/S092/S121 in place; Sprint 3.5 retrofit; ADR-018: ZERO new locks). ElementFactoryRegistry + ValueConverterRegistry re-keyed OFF unordered_map + owned names ONTO vaml::detail::NameTable (content-keyed S141 store, no transient string on find); registered_names() -> vector<string_view>; VamlReader::names() keys -> interned string_view; UIElement::children() -> out-param; collect_descendant_effectives -> out-param; reserve() parser/reader vectors. GREEN BOTH LANES: msvc 628 cases / 4994 asserts (1 intentional should_fail) via verify_s144.cmd; clang-debug 1/1 PASS. No commits. Next = S145.
- [x] S145 Block pool free-list (thread-local, retain-forever, LOCKLESS) - DONE
  SURVIVOR: (dep:S144; object-side half of the allocation fix; rewrites S120/S121 element/layout ownership beneath the unique_ptr model; Sprint 3.5 retrofit; ADR-018: ZERO new locks Ã¢â‚¬â€ thread-local + lockless by construction). PREFERRED ROUTE: unique_ptr ownership PRESERVED (single-parent, virtual dtor, detach-without-destroy unchanged); class-scoped operator new/operator delete on DependencyObject route every allocation/deallocation through the pool. NEW vaml::detail::ObjectPool (ObjectPool.{h,cpp}, plain class; thread_local binding in .cpp mirrors InternStore/Intern.cpp): allocate()Ã¢â€ â€™free-list pop else monotonic bump else spill (never reset/reorder); deallocate()Ã¢â€ â€™intrusive LIFO push, never frees the block / never deletes the object bytes, with contains() forwarding foreign (oversized process-heap) pointers to ::operator delete; master_destroy_all() opt-in teardown. kSlotSize=960=align_up(sizeof(Grid),64) covers the worst case across both CI lanes (msvc-debug /MTd inflates sizeof(Grid) to 952 vs 856 release MSVC / 848 release Linux clang); slot carries the object AND the inline S142 SIMD store (no per-object heap hop; only overflow spills monotonic). RECYCLE == SLAB REUSE NOT STATE CARRYOVER (dtor tears down internal state; slab bytes reused). Tests: ObjectPool_test.cpp Ã¢â‚¬â€ 10 cases / 326 assertions (dole/recycle reuse, LIFO, monotonic bump + spill, never-delete, alignment, contains(), thread-local isolation, master_destroy_all, make_unique<Button> churn integration, create_child<Canvas>/remove_child tree churn; element-size static_asserts guard kSlotSize). SCOPE DECISION (recorded): delivered exactly as titled (object pool + recycling); children_ stays std::vector (amortized O(1), non-shrinking, payload pooled). The S142 NO-GAP "children slab" RE-SCHEDULED (NOT silently dropped) to S624 (the roadmap's "element pool; no heap churn" home) Ã¢â‚¬â€ a distinct low-level data-structure change, NOT bolted onto S145. GREEN BOTH LANES: msvc 638 cases / 5321 asserts (1 intentional should_fail) via verify_s145.cmd; clang-debug 1/1 PASS (638 cases SUCCESS; only pre-existing upstream <ciso646> doctest deprecation). No commits. Next = S146.
- [x] S146 REFACTOR GATE(S3.5) - DONE
  SURVIVOR: (dep:S145; Sprint-3.5 refactor gate; ADR-018: ZERO new locks Ã¢â‚¬â€ subs_ stays lockless thread-local, HeapStats relaxed atomic, no mutex added anywhere). Crossed OFF the five title dead-surface categories: (1) pimpl heap nodes Ã¢â‚¬â€ ALL remaining pimpls COLLAPSED \(ChangeQueue/ChangeRecorder/ChangeProcessor/DependencyProperty/PropertyMetadata/ElementFactoryRegistry/PropertyRegistry/ValueConverter/LayoutManager \+ Grid::Impl\): no Impl heap node / impl_-> trampoline / per-object heap hop survives; NameTable.h PROMOTED into the public detail tree \(vaml/core/detail/NameTable.h, stays vaml::detail\) to legalize the registry collapses; the ChangeQueue std::mutex inlined as a non-moved member \(sole kept approved lock, ADR-018\); ObjectPool slot RAISED to 1216 \(sizeof\(Grid\)==1168 under msvc-debug checked iterators after the collapse\); the S142 DependencyObject/UIElement pimpl was already collapsed earlier; (2) unordered_map/std::string-key paths Ã¢â‚¬â€ DependencyObject::subs_ (the last retained unordered_map node) re-keyed so BOTH add_property_changed and the queue drain call vaml::intern(id.name()): subscriber key now a POOL-STABLE retain-forever view, never a transient caller-owned view; every hot content-probe path already NameTable-intermed at S143/S144; (3) by-value children() Ã¢â‚¬â€ already out-param at S144, re-verified; (4) owned-name gotchas Ã¢â‚¬â€ the subs_ intern-keying is the fix; VamlReader::names() keys already interned string_views; (5) parser/scanner-owned std::string members Ã¢â‚¬â€ audited + documented-not-converted: XmlScanner::stack_/nsStack_ + ParseFrame::contentScratch are INTERNAL booking buffers (nesting-depth / scope mirror / frame-lifetime accumulator), NOT public semantic owned strings (interning them would be wrong Ã¢â‚¬â€ source-mirroring/mutable); ParsedElement::content already string_view (S140); VamlReader::event_names_/root_class_ reader-owned S092 MINIMAL REAL seeds, not parser-owned. NEW surface: (a) public vaml::intern_master_destroy_all() (mirrors object-pool master_destroy_all; ASan-gate opt-in teardown); (b) vaml::detail::HeapStats + public vaml::heap_allocation_count()/heap_allocation_bytes() Ã¢â‚¬â€ the steady-state heap-churn counter feeding S628, funneled ONLY on the VAML-owned monotonic growth paths (InternStore arena/entry growth + ObjectPool block spill), NOT global new/delete overrides (would trap doctest/download noise + collide with host allocator). Tests: HeapChurn_test.cpp (2 cases: public-API string_view+intern audit on real register/find/Get/named/Load entry points; repeated-identical-layout + repeated-identical-document-Load both assert the funnel counter is FLAT after warmup Ã¢â‚¬â€ the recorded zero-churn steady state) + Asan_teardown_test.cpp (exit-time destructor hook freeing the retain-forever pools AFTER all tests so ASan reports zero leaks; the S141/S145 thread_local destructors also free them at thread exit, so ASan deterministic). GREEN BOTH LANES \+ ASan: msvc-debug 641 cases / 5352 assertions, 0 failed (verified from first principles via verify_s146.cmd); S146 tests = 3 cases / 31 assertions PASS; clang-debug 1/1 PASS; ASan zero-leak both via verify_s146_asan.cmd. No commits. Next = S147.
- [x] S147 SURVIVOR + checkpoint; re-target S160's dep to S147; record the allocator-policy ADR (feeds S722) + thread-local pool note (feeds S624/S626) + reaffirm LOCK GOVERNANCE ADR-018 - DONE
  SURVIVOR: (dep:S146; Sprint-3.5 survivor + checkpoint; docs-only, no build, per the S009/S049/S097/S139
  precedent). (1) Consolidated the `## SPRINT 3.5 ÃƒÂ¢Ã¢â€šÂ¬" SURVIVOR` block above distilling S140-S147 (intern/
  SIMD lockless store, block-resident store collapse, mutex relocation + registry re-key, registry/
  accessor migration, object block pool, refactor gate + HeapStats/ASan). (2) Regenerated the CHECKPOINT
  to last = S147 / next = S160 (Sprint 4 - Vulkan window + swapchain + first pixels), PENDING none.
  (3) RETARGET verified: S160 already reads `dep:S147` in roadmap - no change needed, S147 re-confirms
  it (Sprint 4 now depends on the complete, wrapped Sprint 3.5). (4) ALLOCATOR-POLICY ADR recorded as
  ADR-020 (feeds S722 - the Sprint-3.5 allocation/ownership discipline to be finalized there): content-
  keyed registries (SIMD by-value, never pointer-identity); vaml::intern() thread-local retain-forever
  pool (bounded vocabulary + parsed text only; unbounded data WARNED against); thread-local LOCKLESS
  object pool (class-scoped new/delete beneath unique_ptr, slab reuse never delete); recycle == slab
  reuse NOT state carryover; HeapStats on monotonic growth only; cross-thread carries literals/owned
  strings, never interned views. (5) THREAD-LOCAL POOL NOTE recorded (feeds S624 the thread-affine
  lockless arena/element-pool + children-slab requirement; feeds S626 the binding copy-on-write rule:
  hot nodes MUST stay thread-affine, cross-thread transfer carries literals/owned strings, NEVER interned
  views). (6) LOCK GOVERNANCE ADR-018 reaffirmed with the explicit Sprint-3.5 accounting: ZERO new locks
  added across S140-S147 (intern store + object pool thread-local lockless; HeapStats relaxed atomic;
  the ONLY lock action was S143's relocation of the standing pre-approved S037/S041 ChangeQueue mutex,
  moved not created). Marked roadmap S147 [DONE]. No MINIMAL REAL introduced/resolved. No NEW md files
  (directive honored); no build; no commits. Next = S160.

## SPRINT 4 — SURVIVOR
- [x] S160..S172 — Sprint 4 (Vulkan window + swapchain + first pixels) SURVIVOR + checkpoint advance to S200 - DONE
  SURVIVOR: (dep chain S147→S160→S161→S162→S163→S164→S165→S166→S167→S167_1→S168→S169→S170→S171→S172; Sprint 4 wrap; docs-only closure over the S170/S171 green lanes; ADR-018 reaffirmed: ZERO new locks across S160–S172 — every S4 owner is move-only RAII, teardown is single-threaded, the only kept lock remains the standing S037/S041 ChangeQueue mutex). SPRINT 4 delivered the single-thread Vulkan proof: platform window via GLFW (S160, Win32+X11 callbacks + resize/input dispatch seams); Vulkan/VMA linkage + the S003 vulkan MINIMAL REAL marker RESOLVED (S161 — real version symbol kept + extended, sentinel removed); runtime loader + instance + surface + queue-family selection + debug-only validation layers (S162); best-GPU auto-selection + VAML_PREFERRED_GPU override (S163); swapchain + image views + depth + format/present negotiation (S164); render pass + embedded single-triangle SPIR-V shaders + graphics pipeline + framebuffers + the hardcoded tri_mvp (S165); command buffers + semaphores/fences + the main-thread acquire/record/submit/present draw loop (S166); S167 + the S167_1 CORRECTIVE: the two-generation ATOMIC swapchain-recreation transaction + the SwapchainState machine (Ready/Suspended/Retryable/Fatal) + change-driven/retained loop + stale-ghost feedback + replaceable SwapchainStateBuilders seam (the future S176 mock-ICD injection point); MSAA sample-count negotiation + the resolve/multisample seams + the MsaaTarget owner (S168); the RAII consolidation — the Cat 1 Device owner (device.cpp = the SINGLE house of the raw VkDevice/VkQueue) + every wrapper re-pointed to borrow const Device& (S169); destruction-order-CLEAN close: TeardownOrder_test proving every owner move-only + non-trivial-dtor + the SwapchainStateMachine generation members destroy in REVERSE build order, + the ASan no-leak lane (verify_s170_asan.cmd: msvc-asan zero ASan errors + clang-asan zero LSan leaks — the S4 heap owns no retain-forever pools so LSan is genuinely clean) (S170); the REFACTOR GATE (S171): triangle renders (S165/S166 live + device-only llvmpipe proof), no validation errors (debug-gated S162 + the two-gen resize S167_1), resize-safe (S167_1), RAII clean (S170) — both CI lanes re-verified from first principles, grep-audit clean (no raw vk* outside device.cpp, no STUB/TODO scaffold, no dead surface, MINIMAL REAL sentinels intact). LIVE render + resize + close remain [both apps, not CI] on a display-backed host per the standing S160+ precedent. NO-GAP: the grow-only SKIA/GTK-less OOD-first framing carries to S173+ (composition framework) + the three-lane harness S176 (mock-ICD) / S177 (hidden-window resize + validation gate) / S178 (programmer acceptance) + docs/testing.md maturation (S179); the quad renderer + real MVP/UI scale + vertex buffers = S200 (Sprint 5). MINIMAL REAL accounting: vulkan RESOLVED at S161; carry-forward active marks — tools→S287, built-in element seeds (Button→S401, Text→S400, TextBox→S402, CheckBox→S404, Slider→S403, ScrollViewer→S405, Window→S440), VamlReader event hooks→S322, UIElement transform-owner→S208. No new repo md created (directive honored). Roadmap S160..S172 [DONE]; checkpoint regenerated: last = S172, next = S200. No build at the wrap (S170/S171 green), no commits.

## SPRINT 5 — SURVIVOR  (the consolidated surviving state a fresh LLM must assume — S200 & S201 & S202 & S202_1, the Vulkan UI renderer + the icon asset pipeline)
- **Goal met (S200 + S201 + S202 + S202_1): the Vulkan quad renderer + the N-quad scene encoder + the texture
  atlas + sub-region invalidation + the ICON asset pipeline are real, green on both CI lanes. A VAML-declared UI
  can now be drawn as N textured, alpha-blended quads through a real quad pipeline with a real uniform-MVP/UI-scale
  projection + SRGB color, textured from a real staging→device-local atlas, and PNG/SVG icons rasterized once into
  that atlas.** Sprint 5 (dep:S172) delivered the `vaml::vulkan::quad` Cat 1 module (S200/S201/S202) + the
  `vaml::vulkan::icon` asset pipeline (S202_1); the live presented N-quad FRAME is [both apps, not CI] on a
  display-backed host (the standing S160+ precedent), with the device-only live proofs already green on the headless VM.
- **S200 (dep:S172) — quad renderer delivered (real public symbols, none MINIMAL REAL / none STUB):** the S200
  LOADER EXPANSION (VulkanLoader binds vkCreateBuffer/vkDestroyBuffer/vkGetBufferMemoryRequirements/
  vkBindBufferMemory/vkMapMemory/vkUnmapMemory/vkFlushMappedMemoryRanges; vkCmdBindVertexBuffers/vkCmdBindIndexBuffer/
  vkCmdDrawIndexed; vkCmdBindDescriptorSets + vkCreateSampler/vkDestroySampler + descriptor-set-layout/pool/
  allocate/update; vkCmdPipelineBarrier/vkCmdCopyBuffer/vkCmdCopyBufferToImage; new gates has_quad_loader() +
  has_texture_loader()). `vaml::vulkan::quad::QuadVertex` {x,y,u,v,r,g,b,a} (logical px + straight-alpha) + the pure
  mesh seams (quad_vertex_count=4, quad_index_count=6, quad_indices=[0,1,2,0,2,3], unit_quad_positions, append_quad).
  `QuadPipeline` + pure seams: QuadBlendMode {None,Alpha} + quad_blend_state (the straight-alpha
  SRC_ALPHA/ONE_MINUS_SRC_ALPHA blend), quad_vertex_input_state (QuadVertex stride 32 + 3 attributes), the 80-byte
  `QuadUniform { Mat4 mvp; float uiScale; pad[3] }` push constant (the KEPT-AND-EXTENDED resolution of S165's 64-byte
  tri_mvp), ui_projection(width,height,uiScale) (the S200 UI-scale orthographic MVP), the S200-local combined-image-
  sampler (quad_sampler/quad_set_layout_binding/quad_descriptor_write), is_srgb_format + quad_format_bytes (the S200
  SRGB pairing: the quad pipeline color attachment IS the swapchain B8G8R8A8_SRGB, so linear output sRGB-encodes on
  present). RAII `QuadPipeline::create` REQUIRES an SRGB colorFormat. `QuadTexture` + create_procedural_checker (the
  minimal device-local texture: S164 image/memory/view + S200 buffer/copy entries, a TRANSFER_DST → SHADER_READ_ONLY
  barrier + one-shot submit+wait). The EMBEDDED quad shader pair (quad_vertex_spirv/quad_fragment_spirv) compiled +
  spirv-val + spirv-opt --strip-debug on the Linux VM (the S165 precedent).
- **S201 (dep:S200) — draw N quads from CPU list + blend/SRGB tests delivered:** `vaml::vulkan::quad::QuadScene`
  (the N-quad CPU-list → vertex/index buffers + indexed-draw encoder): scene_vertex_count(n)=4N / scene_index_count(n)=6N,
  `QuadItem` + append_scene_item, `SceneRecordStep` + scene_record_plan (BindPipeline→SetViewport→SetScissor→
  BindDescriptorSet→PushQuadUniform→BindIndexBuffer→DrawIndexed); QuadScene::create builds HOST-visible vertex+index
  buffers + the S200-local descriptor pool + the scene descriptor set; set_quads(vertices,n) maps both buffers + writes
  the per-quad index pattern; record_draw binds pipeline + pushes QuadUniform + vkCmdDrawIndexed(6N,1,0,0,0). The
  "blend + SRGB tests" headline is delivered headlessly in QuadScene_test.cpp (per-quad QuadBlendMode; the SRGB pairing
  + BPP; N-quad counts + the per-quad index window).
- **NO-GAP recorded (not silent):** S202 owns the general staging→device-local upload + the texture ATLAS + sub-region
  invalidation (S200's QuadTexture is the ONE minimal textured path); S205 owns the shared descriptor layout + real UI
  shaders (S200's descriptor is the isolated kept+extended S200-local seam); S207/S208/S209 own the scene→command-buffer
  encoder + per-element Model + GPU batching/retained (S201 keeps the CPU-list-per-frame Sprint-5 single-thread model);
  S212 consumes this as the first real vertical slice. S200/S201 add ZERO new locks (ADR-018). No MINIMAL REAL
  introduced/resolved (the quad:: surface is real + permanent; S165's tri_mvp/vertex-input are kept+extended here).
- **MINIMAL REAL carried forward RESOLVED/unresolved:** NONE resolved in Sprint 5. Active marks unchanged: vulkan →
  RESOLVED at S161; tools → S287; built-in element seeds Button→S401/Text→S400/TextBox→S402/CheckBox→S404/Slider→S403/
  ScrollViewer→S405/Window→S440; VamlReader event hooks→S322; UIElement transform-owner→S208. All carry searchable
  sentinels; NO-GAP intact.
- **Build lanes both green (from first principles):** windows-msvc (build/verify_s200.cmd + build/verify_s201.cmd,
  msvc-debug: configure clean + build warning-as-error CLEAN + ctest 3/3 PASS + BOTH POCs build clean) + ubuntu-clang
  (/mnt/hgfs/VamlPOC clang-debug: configure clean + build warning-clean [only the pre-existing upstream <ciso646>
  doctest deprecation, non-error] + ctest 3/3 PASS; the Linux POC S200 device-only live proof builds clean).
  windows-clang-debug is NOT a lane (not built/tested/flagged).
- **S202 (dep:S201) — texture atlas + image upload + sub-region invalidation delivered (real, permanent; NOT MINIMAL REAL / NOT a stub):** RESOLVES the S161/S200-recorded NO-GAP hand-offs into one real `vaml::vulkan::quad::TextureAtlas` owner: (1) `AtlasLayout { atlasW, atlasH, cols, rows, tileW, tileH }` + `AtlasRegion { x, y, w, h }` + PURE decision seams (headless, ctest-proven): the EXACT-GRID contract — `atlas_layout(w,h,cols,rows)` returns an INVALID all-zero layout unless every dimension is non-zero AND atlasW%cols==0 && atlasH%rows==0 (no tile straddles a partial edge); atlas_layout_valid / atlas_tile_count (=cols·rows) / atlas_tile_region (row-major placement) / atlas_tile_offset ((y·atlasW+x)·4) / atlas_tile_bytes (tileW·tileH·4) / atlas_image_bytes (atlasW·atlasH·4); and the HEADLINE `atlas_dirty_regions(layout, dirtyIndices)` — the SUB-REGION INVALIDATION derivation (dirty tile indices → their pixel sub-rects in ASCENDING tile-index order, DEDUPED by index, out-of-range ignored). (2) `TextureAtlas` RAII owner: create() → one device-local R8G8B8A8_SRGB image (TRANSFER_DST|SAMPLED — the S200 SRGB pairing) + view + RAW S164 device memory + host-visible|coherent full-atlas staging buffer + shared quad_sampler + persistent RESET_COMMAND_BUFFER command pool + one command buffer (S166); CPU bookkeeper = full-atlas RGBA8 buffer + per-tile dirty flag set. upload_tile(i, bytes) validates size + writes the tile's bytes at its offset + marks ONLY tile `i` dirty (one flag flips — sub-region invalidation); flush() maps staging, copies ONLY the dirty tiles' bytes at their offsets, records ONE command buffer (whole-image barrier currentLayout→TRANSFER_DST, one vkCmdCopyBufferToImage per dirty sub-region with its bufferOffset+imageExtent, barrier TRANSFER_DST→SHADER_READ_ONLY), submits+waits idle, clears the dirty set, returns the count (0 when nothing dirty or on failure WITHOUT clearing — retry-safe); tracks currentLayout_ (starts UNDEFINED). LOADER: NO new S202 entry points — reuses the S200 vkCmdCopyBufferToImage+vkCmdPipelineBarrier (has_texture_loader) + S166 command-pool/buffer/begin/end/submit/wait entries (has_command_loader). DECISION (confirmed, recorded NOT silent): the atlas uses RAW S164 device memory, NOT VMA (one image + one staging buffer = no sub-allocation pressure; VMA becomes right at S202_1/S203/S204+/S205) — the S161 "S202 consumes VMA" note DIVERGED BY DECLARED DECISION, recorded in TextureAtlas.h. Tests: TextureAtlas_test.cpp (headless, the pure-seam precedent) — exact-grid contract, tile count, region placement (first/mid/last/out-of-range), byte offsets, tile+image bytes, atlas_dirty_regions (empty/out-of-range-only/ascending/input-order-independence/dedupe/multi-with-interleaved-dup+out-of-range/invalid-empty). The LIVE create/upload/flush path is [both apps, not CI] manual via POCs, per precedent. NO-GAP: the atlas is a real kept-and-extended owner for S202_1 (icon assets) + S203 (glyph atlas) + S204 (SDF) + the S205 shared descriptor layout.
- **Build lanes both green (from first principles):** the S200/S201/S202 lanes — windows-msvc (build/verify_s200.cmd + build/verify_s201.cmd + build/verify_s202.cmd, msvc-debug: configure clean + build warning-as-error CLEAN + ctest 3/3 PASS incl. TextureAtlas_test + BOTH POCs build clean) + ubuntu-clang (/mnt/hgfs/VamlPOC clang-debug: configure clean + build warning-clean [only the pre-existing upstream <ciso646> doctest deprecation, non-error] + ctest 3/3 PASS). windows-clang-debug is NOT a lane (not built/tested/flagged).
- **S202_1 (dep:S202) — ICON asset pipeline delivered (real, permanent; NOT MINIMAL REAL / NOT a stub):** the
  vendored decode/raster supply + the `vaml::vulkan::icon` pure seams + the real `IconAtlas` owner over the S202
  atlas. PNG and SVG are BOTH source asset formats, each rasterized ONCE into the S202 atlas exactly as fonts are
  (S203/S204 model) — no vector-on-screen, no second renderer; output tagged with its texel count for best-fit
  selection (S213_1). (1) SUPPLY: cmake/FindImageDecoders.cmake exposes `vaml::image` (SYSTEM includes) for the
  vendored single-header CPU rasterizers `stb_image` (public domain, PNG decode) + `nanosvg` (MIT, SVG rasterize) —
  both pushed to the PUBLIC vaml-deps repo (verified @95d1300) so every host compiles identically (FindGLFW/FindVMA
  precedent); the single-TU implementation defines live in StbImageImpl.cpp / NanosvgImpl.cpp (with the required
  standard headers + a scoped pragma-warning push for the benign upstream C4702 so vendor noise stays off OUR strict
  bar). (2) PURE SEAMS (IconPipeline.h/.cpp, headless): `IconFormat { Png, Svg }`; `IconSource` (one packaged asset,
  never a path) + `IconSourceSet` (bounded key → source map); `IconRaster { w, h, rgba, texelCount }` (texelCount ==
  w*h = the S213_1 best-fit tag); `icon_raster_valid`; the ADR-015 STRUCTURAL SOURCE-SET RULE seam `source_by_key`
  (only in-set keys consumable; a raw path/URI is structurally unrepresentable — no path-taking API); `raster_icon`
  (PNG → stb decode → bilinear-resample; SVG → nanosvg contain-fit rasterize; both → tightly-packed R8G8B8A8 tagged
  with texelCount; computed ONCE, never per-frame); structured NON-crashing `IconErrorKind { None, Malformed,
  Unsupported, OutOfSet }` (ADR-015 NO-GAP: the fallback GLYPH is S400_1's, not this module's). (3) `IconAtlas`
  (the REAL owner, IconAtlas.h/.cpp): `add_icon(key, sizeW, sizeH)` enforces OutOfSet FIRST, rasterizes ONCE, then
  `upload_tile` to the S202 atlas (preserving its SUB-REGION INVALIDATION — only that tile's flag flips), recording
  the slot `{tileIndex, texelCount}`; supports the SIZE FAMILY for S213_1 best-fit later; `texture_atlas()` samples
  via the UNCHANGED S200/S201 quad pipeline. ZERO new locks (ADR-018; single-threaded once-at-import). The DPI
  best-fit SELECTION POLICY stays fully with S213_1 (accepted plan decision #2). (4) ciso646 FIX (user call-out):
  the Linux `#warning` is a meaningless clang-only doctest libc++ heuristic (vaml uses GNU libstdc++); the permanent
  fix replaces `-Wno-error=#warnings` with `-Wno-#warnings` on the test targets → ubuntu-clang now reports 0
  warnings. (5) TESTS (IconPipeline_test.cpp, 7 cases): PNG decode + exact pixel + texelCount; PNG malformed →
  structured no-crash; SVG raster + sampled pixel + texelCount; SVG malformed → structured no-crash; zero/oversized
  → Unsupported; the ADR-015 source-set rule; two-source same-surface. (6) POC `[s202_1-live]` block in BOTH apps
  (device-only, [both apps, not CI]): IconAtlas over a 256x128 4x2 atlas, rasterize embedded PNG + SVG ONCE into
  32x64 tiles (texelCount == 32*64 each), upload tiles, reject an out-of-set key, log texel counts; the presented
  tile sampling via QuadScene UVs stays the display-backed manual step. (7) NO-GAP (not silent): S205_1 (tint),
  S213_1 (consumes the texelCount tag + size family; the POLICY stays S213_1's), S360_1 (IconKey→resource), S400_1
  (Icon control + the fallback GLYPH), S528_1 (gallery). ADR-011: icon import lives in vaml-vulkan, never core.
- **Build lanes both green (from first principles, incl. S202_1):** the Sprint-5 lanes — windows-msvc
  (build/verify_s202_1.cmd, msvc-debug: configure clean + build warning-as-error CLEAN + ctest 3/3 PASS incl.
  IconPipeline_test + both POCs build clean) + ubuntu-clang (/mnt/hgfs/VamlPOC clang-debug: configure clean + build
  **warnings==0** (the ciso646 noise is FIXED) + ctest 3/3 PASS, all 7 S202_1 cases green on both lanes). GH runner
  supply (vaml-deps stb/nanosvg @95d1300) confirmed pushed, closing the CI supply blocker. windows-clang-debug is
  NOT a lane (not built/tested/flagged).
- **Next = S203** (Sprint 5 — the bitmap-font loader + ASCII 32–126 glyph atlas build, the "exactly as fonts are"
  model S202_1 referenced).

## SPRINT 5/6 — SURVIVOR  (the consolidated surviving state a fresh LLM must assume — S200..S212, the Vulkan UI renderer + the icon asset pipeline + the MERGE POINT, closed at S212)
- **Goal met (S200..S212): the Vulkan UI renderer is complete end-to-end: quad/textured renderer (S200/S201),
  texture atlas + sub-region invalidation (S202), the icon asset pipeline (S202_1), the bitmap-font + SDF
  atlas (S203/S204), the shared-layout UI shaders + monochrome tint (S205/S205_1), the primitive draw batcher
  (S206), the scene encoder (S207/S208 transform/opacity/clip), the fence-driven retained pool (S209), the
  single-thread retained render loop (S210), the LOCK-FREE marshalled UI-thread→render-thread postbox (S211),
  and now THE MERGE POINT (S212): UI declared in VAML renders to the window via the full S5 path through the
  vaml::presentation::VamlRenderer vertical-slice HOST.** Sprint 5/6 closer = S212 (the first real vertical slice).
- **S212 (dep:S211) — MERGE POINT delivered:** the vaml::presentation::VamlRenderer render host (REAL,
  permanent Category-1) composes the S173 Presenter + the S210 SceneRenderer + the S211 MarshalledScene postbox
  + the S207/S208 encoder behind ONE public surface. CPU/merge host `VamlRenderer::create()` (headless,
  constructible WITHOUT Vulkan) + LIVE owner `VamlRenderer::create_device(options)` (owning SceneRenderer +
  Presenter, wiring the Presenter's DrawBody INTERNALLY = ConsumeMarshalled → EncodeIfDirty(+RecordRetained) →
  Recycle). Full path proven: VAML text → VamlReader::Load → UIElement root → Measure/Arrange →
  encode_visual_slice → postbox mutate+publish → consume_latest → set_scene → record retained → recycle →
  present. Default classifier `classify_merge_element` = the accepted judgment (solid-fill each real panel at
  its final_rect; the honest first slice of the declared LAYOUT). Headless Rail-1 test VamlRenderer_vertical_test
  9 cases (Load→UIElement; arrange→encoder emits real final_rect; default visual-of fills each panel; degenerate
  panel honestly skipped; producer/consumer contract incl. S211 backpressure-retained; render-loop-plan seam;
  presenter_lifetimes_ok; move-only RAII). [s212-live] device-only block in BOTH POCs (VAML Load → host →
  invalidate → encode count → real SceneRenderer set_scene parity). RESOLVES the S210+S211 recorded
  "record→submit→present wiring into the S173 Presenter DrawBody … S212's vertical slice" hand-offs + the S222
  "consumer touches ONLY the public surface" (the merge logic is a library surface, not POC-internal code).
  NO-GAP (recorded, not silent): S213 (DPI — the host carries set_frame_size + uiScale), S222 (viewer consumes
  the host), S400+/S364 (real control materials replace visualOf in place). ADR-018: ZERO new locks (the postbox
  is lock-free). ADR-011 kept. MINIMAL REAL: NONE introduced/resolved at S212.
- **Both CI lanes green from first principles (verified this session):** windows-msvc (build/verify_s212.cmd,
  msvc-debug: configure clean + build warning-as-error CLEAN + ctest 3/3 PASS incl. VamlRenderer_vertical 9 cases
  + the VamlPOC.exe POC builds clean) + ubuntu-clang (/mnt/hgfs/VamlPOC clang-debug on the Ubuntu 26.04 VM:
  configure clean + build warning-clean + ctest 3/3 PASS + the VamlPOC-linux POC builds clean against the fresh
  libs in VamlPOC-linux/build-ninja). windows-clang-debug is NOT a lane (not built/tested/flagged per standing rule).
- **MINIMAL REAL / active marks + invariants intact:** tools → S287; built-in element seeds Button → S401 / Text →
  S400 / TextBox → S402 / CheckBox → S404 / Slider → S403 / ScrollViewer → S405 / Window → S440; VamlReader event
  hooks → S322. [ADR-001..012] [layer-rule] [no-managed] [no-stub] [NO-GAP] [NO LAZY LLM] [ADR-018 lock governance].
- **S213 (dep:S212_1) -- DPI + framebuffer scale delivered (resolves the S212/S212_1 NO-GAP in place):** NEW vaml::presentation::dpi pure seams (dpi.h/dpi.cpp) -- framebuffer_scale_for (DERIVED physical/logical X ratio: 1600/800 = 2.0; zero-guarded -> 1.0); canvas_projection = the S200 quad::ui_projection(Lw,Lh,1) over the logical canvas; RenderPlan + dpi_render_plan (the ONE self-consistent mvp+uiScale plan); logical_to_physical (the crisp invariant). Host integration: VamlRenderer::record_draw computes dpi_render_plan(extent, frameW/H_) + calls the SceneRenderer 3-arg record (REPLACING the no-arg record) + last_framebuffer_scale() (the S213_1 dpiScale input). Headless Dpi_test 6 cases + [s213-live] in BOTH POCs. Both CI lanes green from first principles. NO-GAP: S213_1 consumes last_framebuffer_scale() as dpiScale; the on-screen high-DPI proof is S222/S224. No MINIMAL REAL.
- **S213_1 (dep:S213) -- DPI-aware icon best-fit selection delivered (resolves the S202_1 + S213 hand-offs in place):** NEW vaml::vulkan::icon SELECTION surface (IconBestFit.h/.cpp) -- `ScalingPolicy { None, DownOnly, Both }` (default None) + `IconCandidate` (sizeW/sizeH/texelCount/side) + `physical_n_for(logical × dpiScale)` + the pure `select_best_fit` policy table (None never resamples → largest ≤ N or closest per D1; DownOnly never up; Both prefers ≤ N no-blur else either) + `IconBestFitCache` (per (IconKey, logical size) computed once then cached; whole-cache invalidation on a dpiScale/policy change; ZERO locks ADR-018). MOD IconAtlas: the size-family candidate feed (`candidates_for` per key; `slot_for` = largest, backward-compatible — S202_1 tests stay green). Headless IconBestFit_test 12 cases + [s213_1-live] device-only block in BOTH POCs (a real 16/32/64 family → None picks the largest ≤ N_physical(20) = the 16px, no resample, cached once). Both CI lanes green from first principles. NO-GAP: S400_1 consumes resolve (dpiScale = host.last_framebuffer_scale()); S360_1 feeds the key space; S528_1 re-verifies at the render level. No MINIMAL REAL.
- **S214 (dep:S213_1) -- REFACTOR GATE(S5) VERIFIED this session (docs-only -- no code change required; green from first principles, NOT assumed from S213_1):** (1) RETAINED UI RENDERS VIA VULKAN -- windows-msvc (build/verify_s213_1.cmd, msvc-debug configure + build warning-as-error CLEAN + ctest 3/3 PASS incl. MarshalledScene/SceneRenderer/VamlRenderer_vertical + both POCs) + ubuntu-clang (/mnt/hgfs/VamlPOC clang-debug, build warnings==0 + ctest 3/3 PASS) -- the S210 retained loop + S211 marshalled consume + S212 VamlRenderer::create_device DrawBody wired internally all REAL + green; headless VamlRenderer_vertical + SceneRenderer + device-only [s212-live]/[s210-live]/[s211-live] prove the retained UI renders via Vulkan (the presented WINDOW FRAME remains RAIL 3 display-backed manual, recorded not silent). (2) NO STUBS -- grep-audit across src/vulkan, src/presentation, src/core/src: no STUB/TODO/FIXME/backfill scaffolds; only the sanctioned affirmative "NOT a stub / NOT MINIMAL REAL / NOT a backfill" statements + forward-resolvable seeds. (3) NO VALIDATION ERRORS -- RAIL 2 real-WSI gate build/verify_s177_wsiresize.cmd RE-RUN on the real NVIDIA RTX A5000 Laptop GPU (hidden-window resize storm + minimize/restore + resize-during-flight, validation ON, debug-utils messenger flags any error = FAIL): SCRIPT_EXIT=0 = PASS, ZERO validation errors. (4) DEAD-SURFACE AUDIT clean -- every S200-S213_1 public surface consumed downstream or by a POC (quad->QuadScene/atlas/icon; UI shaders->UiScene/UiPipeline; SceneRenderer+postbox->VamlRenderer; icon best-fit->S400_1/S360_1/S528_1; the S120 transform-owner/layout-slot sentinels RESOLVED at S208/S122 and removed); nothing orphaned. (5) ASAN -- build/verify_s170_asan.cmd RE-RUN (windows-msvc msvc-asan zero ASan errors + ubuntu-clang clang-asan ASan+LSan ZERO LEAKS) -- no regressions from the S5/6 additions. ADR-018: ZERO new locks across S200-S213_1 (verified at the gate). MINIMAL REAL: no new introduced/resolved at the gate; tools->S287 + all element seeds + VamlReader event hooks->S322 carried forward intact.
- **S215 (dep:S214) -- SURVIVOR + checkpoint advance to S220 (docs-only wrap, no build -- both lanes already green from the S214 gate + ASan):** consolidated the SPRINT 5/6 SURVIVOR block (S200..S214) + regenerated the checkpoint: last completed = S215, next = **S220** (Sprint 5a vaml_viewer first token), PENDING none + GUARD RAILS MINIMAL REAL carried forward unchanged. DECISION (driver-confirmed at plan time): the checkpoint advances to S220, NOT the literal "S240" of the original title -- S220 dep:S215 and S240 dep:S226 make the roadmap's dependency order authoritative (Sprint 5a must run before Sprint 6's first token), so a jump to S240 would skip the viewer sprint (the permanent visual-feedback vehicle) and break the gapless list; S220 matches every prior wrap precedent (S009->S030, S049->S080, S097->S120, S139->S140, S147->S160, S172->S200). NO-GAP satisfied: nothing deferred to a silent backfill; S214's gate fully verified. No MINIMAL REAL introduced/resolved at S215.
- **Next = S221** (Sprint 5a -- vaml_viewer: Viewer CLI entry `vaml_viewer <file.vaml>`; S221 dep:S220, S240 dep:S226).
- **VIEWER GROWTH:** the vaml_viewer (S220+) is the permanent visual-feedback vehicle (reload S222_1 + pan/zoom
  S224 + the per-sprint growth table S256/S293/S329_1/S369/S409_1 — see roadmap.md conventions). Completion
  gate: no viewer-growth token, no ✅ (the anti-grave-whistle clause). Viewer is preview-only (no embedded
  source editor). S212_1 (option A): the library's create_device present objective is demonstrated in the
  viewer (S222), NOT in the doomed POCs (S220 deletes them).

## SPRINT 5a — SURVIVOR  (the consolidated surviving state a fresh LLM must assume — S220.., vaml_viewer)
- **S220 (dep:S215) — Repo restructure + viewer scaffold (DONE):** DELETED `VamlPOC/` + `VamlPOC-linux/` (the two throwaway harnesses; their real features already live in the library surface, neither was add_subdirectory'd into the top-level CMake, so the library build is unaffected). ADDED `vaml_viewer/` (Windows VS project: vaml_viewer.vcxproj Debug/Release x64, v145, /W4 /WX /permissive- /EHsc /std:c++20, /MTd+/MT matching the msvc-debug static libs, PUBLIC include roots only, links vaml-presentation/vulkan/core + glfw3, Console subsystem, + vaml_viewer.slnx + main.cpp = a MINIMAL version-print consumer scaffold, zero feature code) + `vaml_viewer-linux/` (Linux CMake: standalone project, C++20 strict clang bar, links the clang-debug static libs vaml-presentation -> vaml-vulkan -> vaml-core + pkg-config GLFW/Vulkan + dl, public roots only; src/main.cpp = same version-print scaffold). ADDED build/verify_s220.cmd (msvc-debug warning-as-error clean + ctest 3/3 PASS + msbuild vaml_viewer.vcxproj Debug/x64 clean). RECORDED the CONSUMER-APP PATTERN (an app uses ONLY the public surface — public include roots only, no internal headers, own main(), sits above the ADR-011 facade) as ADR-021. NO MINIMAL REAL introduced/resolved. The emptied VamlPOC/.vs IDE cache is git-ignored + transient (held open by the IDE); stray root .txt logs left untouched; historical build/verify_s201..s213_1.cmd remain as archived per-step records (not re-run); verify_s220.cmd is the current green. Both CI lanes green from first principles (windows-msvc via verify_s220.cmd + ubuntu-clang via /mnt/hgfs/VamlPOC clang-debug: configure+build warning-clean + ctest 3/3 PASS + vaml_viewer-linux builds clean).
- **NEXT = S221** (Sprint 5a — Viewer CLI entry `vaml_viewer <file.vaml>`; S221 dep:S220, S240 dep:S226). ADR-021 governs: the viewer consumes ONLY the public surface.

## SPRINT 5a (cont.) — SURVIVOR  (S222 — the MVP "soup-to-nuts" render host, LIVE-proven in a real window)
- **S222 (dep:S221) — MVP "soup-to-nuts" render — DONE + LIVE-PROVEN (this session).** The consumer-touch-ONLY-public-surface host: ADDED vaml_viewer/app.{h,cpp} (the shared `ViewerApp` owner) + the pure decision seams (viewer_stack_steps/viewer_setup_ready/viewer_deadline_elapsed) + build/verify_s222.cmd; the SAME app.cpp is compiled into BOTH the viewer AND the vaml-presentation-tests target (the headless ViewerRenderPlan_test suite, 6 cases, exercises the exact host code — ADR-021). The LIVE soup-to-nuts proof opened a real window on the NVIDIA RTX A5000: stack + VamlRenderer::create_device build, the loaded tree arranges+encodes+publishes, the retained path records+presents, auto-closes clean → exit 0. This DEMONSTRATES S212_1 (the library's create_device present objective) IN THE REAL WINDOW. **LIVE-PROOF-FOUND fixes** (a real driver is the only way to surface them — all authored in this step, none deferred): (1) app->loader_ owned FIRST/LAST member, not a moved local (the S173 builders capture &loader into the Presenter; a moved-from/dead loader AV'd the resize rebuild); (2) the S167_1 two-generation swapchain path passes Gen N's handle as oldSwapchain to vkCreateSwapchainKHR (the S176 build seam gained a VkSwapchainKHR param threaded through Swapchain::create/rebuild) — NVIDIA AV'd a second swapchain for a live surface without it (independently reproduced in S177's own isolation probe); (3) vkCmdBindVertexBuffers passes a real pOffsets (nullptr is UB — NVIDIA AV) — the S209 vertex path's first live exercise; (4) request_resize only on a real size change; (5) NON-const LoadResult in both main.cpp. BOTH CI LANES green from first principles (verify_s222.cmd + /mnt/hgfs/VamlPOC clang-debug: configure/build warning-clean + ctest 3/3 + vaml_viewer-linux clean under clang 21.1.8). Scope bounded (recorded): reload = S222_1, pan/zoom + --help = S224, drag-drop = S223, real control materials = S400+/S401. No MINIMAL REAL introduced/resolved. No commits.

## SPRINT 5a (cont.) — SURVIVOR  (S222_1 — the file-watch LIVE RELOAD, the viewer's permanent visual-feedback vehicle)
- **S222_1 (dep:S222) — FILE-WATCH LIVE RELOAD — DONE + LIVE-PROVEN (this session).** The viewer's PERMANENT
  visual-feedback vehicle: on a `.vaml` file's mtime change → VamlReader::Load again → re-render (the whole S5
  path re-runs with ZERO imperative tree-building — "render live VAML like an app would, without a code-behind").
  ADDED vaml_viewer/file_watch.{h,cpp} (the mtime-POLL + NOTIFY seam: FileStamp/kNoStamp + FileEvent
  {Unchanged,Changed,Appeared,Disappeared} + FileViewEntry + the PURE headless decision table
  file_view_state_transition (exhausted in ctest with NO std::filesystem dependency) + stamp_to_millis (the
  portable last_write_time→millis conversion) + the no-throw I/O seams file_mtime/view_file + the real RAII
  FileWatcher (begin/poll/re-arm + the NOTIFY SEAM on_event — the app-side reload handler, default no-op,
  swallow/continue)) + ADDED vaml_viewer/live_reload.{h,cpp} (the "edit → reload → same tree" coordinator:
  the PURE reload_action table (Changed/Appeared→Reload, Disappeared→Report/keep-last, Unchanged→KeepLast) +
  the real LiveReload RAII — owns a FileWatcher + calls the S221 vaml::load_file on Reload (never a
  duplicate) + the on_reloaded/on_reload_error/on_file_disappeared/on_file_restored notify seams). MOD
  vaml_viewer/app.{h,cpp} (ViewerAppConfig.filePath — the OPT-IN: empty ⇒ S222 behavior UNCHANGED (no
  watcher); non-empty ⇒ LiveReload armed + run_loop polls it each iteration; on_reloaded re-runs the WHOLE S5
  path with ZERO imperative tree-building via apply_reload → adopt the new tree → set_root → set_frame_size →
  invalidate() (the exact S222 producer path, re-invoked — NOT hand-built); the retained renderer records +
  presents the re-declared tree; the swapchain/presenter/device are NOT recreated; load failure keeps the
  last-good tree + reports stderr, NEVER silent) + MOD both main.cpp (CLI path → config.filePath, live reload
  by default) + MOD vaml_viewer.vcxproj/vaml_viewer-linux/CMakeLists.txt (+file_watch.cpp + live_reload.cpp).
  TESTS: FileWatch_test.cpp (vaml-core-tests, the pure table + the real-file lifecycle: baseline Unchanged →
  rewrite Changed + re-arm → delete Disappeared + re-arm → recreate Appeared + the notify seam fires + default
  no-op) + LiveReload_test.cpp (vaml-core-tests, THE GOLDEN: edit → reload → same tree against real temp files
  through the real seam — rewrite with new golden → on_reloaded with a tree reflecting the edit; same-bytes
  re-save → an EQUIVALENT tree (structural compare: root type + converted Margin); delete → Report (no
  reload, last-good kept); recreate → Reload again; parse-error edit → on_reload_error with a non-empty detail
  (never silent, last-good kept)) + ViewerReloadPlan_test.cpp (vaml-presentation-tests, the config opt-in
  contract + the decision table). ADDED build/verify_s222_1.cmd (msvc-debug warning-clean + ctest 3/3 PASS +
  vaml_viewer build + CLI proofs 0/2/1/3 + the LIVE file-watch RELOAD DEMO on the NVIDIA RTX A5000: window
  opens armed on a temp .vaml, REWRITE MID-RUN (Changed → the whole S5 path re-runs → re-render), window keeps
  running + closes cleanly via VAML_VIEWER_AUTO_CLOSE_MS → exit 0). BOTH CI LANES green from first principles:
  windows-msvc (build/verify_s222_1.cmd) + ubuntu-clang (/mnt/hgfs/VamlPOC clang-debug: build warnings==0 +
  ctest 3/3 PASS + vaml_viewer-linux clean). TEST-ROBUSTNESS (recorded): the watcher tests sleep >2s for the
  mtime bump because the VM lane runs on the VMware HGFS shared folder (~2s mtime granularity vs ~100ns native)
  — 2200ms covers BOTH lanes so the golden is deterministic everywhere. No MINIMAL REAL introduced/resolved
  (the seams are REAL permanent code, the early hot-reload loop, kept-and-extended). ADR-018: ZERO new locks
  (single-threaded, driven off the viewer's UI thread in run_loop()). NO-GAP: the early hot-reload loop — S825
  MATURES it (async reload + tooling integration), it does NOT re-do it. [VIEWER GROWTH: reload]
- **S223 (dep:S222_1) — DRAG-AND-DROP ENTRY — DONE + LIVE-PROVEN (this session).** `PlatformWindow` (S160) gains a
  FILE-DROP callback; the viewer reopens the dropped `.vaml`; the drop-dispatch → reopen-handler seam is
  HEADLESS-testable (the S222_1 `reload_action` split precedent, this time on the drop axis).
  (1) LIBRARY (S160 extended): MOD src/vulkan/include/vaml/vulkan/PlatformWindow.h + src/vulkan/src/
  PlatformWindow.cpp — `WindowCallbacks.on_drop` (GLFW's drop signature `(int count, const char* const* paths)`)
  + the pure free `dispatch_drop` seam (empty-hook silent no-op, mirrors dispatch_resize/key/mouse/scroll)
  + a `glfwDrop` trampoline + `glfwSetDropCallback` registration in create(). GLFW's SINGLE drop callback
  transparently covers BOTH Win32 + X11 (GLFW owns OS coupling) — no per-OS drop code, exactly like the S160
  resize/key/mouse/scroll hooks. (2) VIEWER PURE SEAMS: ADDED vaml_viewer/drop_reopen.h — `is_vaml_drop`
  (case-INSENSITIVE `.vaml` extension guard — a dropped non-`.vaml` is NEVER reopened/rendered) + `drop_choice`
  (dispatch selection: pick the FIRST `.vaml` path from a GLFW drop payload, nullopt = drop ignored), the
  S222_1 `reload_action`-style split (pure table in the viewer seam; real handler in the app). (3) VIEWER
  REOPEN HANDLER: MOD vaml_viewer/app.{h,cpp} — `ViewerApp::reopen(unique_ptr<DependencyObject>)` is a THIN
  PUBLIC WRAPPER over the S222_1 `apply_reload` (re-adopt → set_root → set_frame_size → invalidate = the whole
  S5 path, ZERO imperative tree-building) = the SINGLE SHARED reopen handler for BOTH drop-reopen AND file-watch
  reload (no drift). The on_drop callback is wired at window-creation time (the S160 WindowCallbacks are owned
  at create()) into a pending-drop slot (`pendingDrop_`); run_loop() drains it after poll_events() each frame →
  `vaml::load_file(path)` → `reopen`. ADR-018: ZERO new locks (GLFW fires the drop callback inside poll_events()
  on the UI thread; the slot drains on that same thread); `pendingDropApplied_` guards against re-applying the
  same drop each frame, re-arming only on a NEW drop; a dropped `.vaml` that fails to load reports to stderr +
  KEEPS the last-good tree (never a blank window). (4) TESTS (headless): MOD test/unit/PlatformWindow_test.cpp
  (vaml-vulkan-tests — the S160 drop surface: empty-hook no-op + exact count/paths single + multi-file +
  no-cross-fire) + ADDED test/unit/ViewerDropPlan_test.cpp (vaml-core-tests — the PURE drop-decision seams +
  the "a dropped .vaml reopens through the SHARED reopen handler (apply_reload)" contract). (5) BUILD: MOD
  vaml_viewer.vcxproj (+drop_reopen.h ClInclude) + ADDED build/verify_s223.cmd. BOTH CI LANES green from first
  principles: windows-msvc (build/verify_s223.cmd: msvc-debug warning-clean + ctest 3/3 PASS incl. the S160
  PlatformWindow drop cases + ViewerDropPlan + vaml_viewer Debug/x64 clean + CLI proofs 0/1 never-silent + the
  LIVE file-drop REOPEN demo close=0 on the NVIDIA RTX A5000 — viewer opens on a temp .vaml, a second .vaml
  reopens through ViewerApp::reopen → the S222_1 apply_reload whole-S5-path machinery, clean auto-close) +
  ubuntu-clang (/mnt/hgfs/VamlPOC clang-debug: build warnings==0 + ctest 3/3 PASS + vaml_viewer-linux builds
  clean under clang 21.1.8). No MINIMAL REAL introduced/resolved (the drop seams are REAL permanent code,
  kept-and-extended; the reopen handler is the S222_1 machinery — ONE reopen path, no drift). NO-GAP recorded
  (not silent): the S323/S324 routed-event + hit-test path layers onto the input hooks, NOT the drop path
  (drop is independent). Reload-in-place on a second drop + pan/zoom + --help = S224. [VIEWER GROWTH: drag-drop entry]
- **S224 (dep:S223) - BOOT/WINDOW POLISH + RENDERED-VIEW PAN & ZOOM - DONE + LIVE-PROVEN.** --help usage (NEW vaml_viewer/cli.h - PURE cli_action{Run,Help,NoArg}/cli_usage/loaded_filename seams - BOTH main.cpp thinned to the help/no-arg gate: Help->stdout exit 0; NoArg->stderr exit 2, never silent); title = loaded filename (basename); teardown-order-clean + ASan no-leak (S170) re-run green; reload-in-place on a SECOND drop (no restart) - the S223 reopen handler reused, LIVE-proven. RENDERED-VIEW PAN & ZOOM (S208 RenderTransform + S213 DPI/framebuffer-scale harnessed): NEW vaml_viewer/view_transform.h (PURE ViewState pan/zoom seams folding the S208 Matrix2D) + the LIBRARY surface (real permanent Category-1): VamlRenderer::set_view_transform(Matrix2D) + the PURE compose_view_into_render_plan(view, dpi::RenderPlan) seam (lifts the S208 2D-affine into the column-major ui::Mat4 mvp, left-multiplies into the S213 mvp, leaves uiScale UNTOUCHED - crisp DPI invariant + S208 clip-scissor alignment hold); record_draw composes the view into the plan mvp (redraw-only, no re-encode). MOD vaml_viewer/app.{h,cpp} - the S160 input hooks fold into the ViewState -> push_view_transform() -> renderer_->set_view_transform + deterministic LIVE view-exercise. TESTS: ViewerCli_test.cpp + ViewerViewState_test.cpp (vaml-core-tests) + ViewerPanZoom_test.cpp (vaml-presentation-tests). ADDED build/verify_s224.cmd. BOTH CI LANES green from first principles (verify_s224.cmd: msvc-debug warning-clean + ctest 3/3 PASS incl. ViewerCli + ViewerViewState + ViewerPanZoom + vaml_viewer Debug/x64 clean + CLI proofs 0/2/1/0 + LIVE pan/zoom window close=0 + LIVE second-drop reopen close=0 on the NVIDIA RTX A5000) + ubuntu-clang (/mnt/hgfs/VamlPOC clang-debug: build warnings==0 + ctest 3/3 PASS + vaml_viewer-linux clean under clang 21.1.8). ASan no-leak (verify_s170_asan.cmd): msvc-asan zero ASan errors + clang-asan zero LSan leaks. No MINIMAL REAL introduced (cli.h + view_transform.h + set_view_transform + compose seam = real permanent Category-1); ADR-018 ZERO new locks (pan/zoom + drop fire inside poll_events() on the UI thread). [VIEWER GROWTH: pan/zoom]
- **S225 (dep:S224) - REFACTOR GATE(S5a) - DONE + VERIFIED from first principles both CI lanes + both platforms + ASan this session.** POC removal left ZERO dead code. DEAD-SURFACE AUDIT (grep STUB/TODO/FIXME/XXX/backfill/scaffold across src/core, src/vulkan, src/presentation, vaml_viewer, test/) - the ONLY hits are the sanctioned affirmative statements (NOT MINIMAL REAL / NOT a STUB / not a backfill / NO-GAP) + the forward-resolvable MINIMAL REAL sentinels (tools->S287; Element Button->S401, Text->S400, TextBox->S402, CheckBox->S404, Slider->S403, ScrollViewer->S405, Window->S440; VamlReader event hooks->S322) - ZERO unaudited dead code, every seed searchable + contractually-bound to a resolution step (NO-GAP / NO-LAZY-LLM). DELETED the committed POC-era debris the S220 restructure left untouched (grep across docs + verify .cmd + CMake proved ZERO references; git history preserves them, deletion reversible): root exit/non-zero/reopen/usage/watch + s177cap.txt/s177final.txt/s177o.txt/s177out.txt/s177se.txt/s177so.txt/s177_diag.txt/build_tmp_out.txt/verify_s211_log.txt + the empty leftover VamlPOC/VamlPOC.slnx (the dir residue is a git-ignored IDE .vs cache, transient). No dangling POC references (grep VamlPOC/, VamlPOC-linux, VulkanLoader.h = zero in live code - the viewer consumes ONLY the public surface, ADR-021). HEADLESS FILE->TREE SEAM covered in CI on both lanes against the golden fixture (ViewerLoadFile_test S221 + LiveReload_test S222_1 edit->reload->same-tree real temp files, compiled into vaml-core-tests with the REAL load_file.cpp) + the viewer-boundary ViewerReloadPlan/ViewerDropPlan/ViewerCli/ViewerViewState/ViewerPanZoom seam suites - the gate CERTIFIES this obligation already met (verify, not re-author). BOTH CI LANES green from first principles (verify_s224.cmd: msvc-debug 3/3 PASS + viewer clean + CLI 0/2/1/0 + LIVE demos close=0 + ubuntu-clang: build warnings==0 + ctest 3/3 PASS + vaml_viewer-linux clean under clang 21.1.8). ASan no-leak (verify_s170_asan.cmd re-run WITH the deletions: msvc-asan zero ASan errors + clang-asan zero LSan leaks). No MINIMAL REAL introduced/resolved (the gate sweeps dead code; all seeds already resolution-bound).
- **S226 (dep:S225) - SURVIVOR + CHECKPOINT ADVANCE - DONE (docs-only wrap; no build - both lanes green from S225).** Consolidated the Sprint 5a survivor (S220 scaffold / S221 CLI+file->tree seam / S222 soup-to-nuts render / S222_1 file-watch reload / S223 drag-drop / S224 pan/zoom+--help / S225 refactor gate) + documented the VIEWER-GROWTH MECHANISM (how vaml_viewer grows as later sprints land - per-sprint token + completion gate + marker, see roadmap conventions): controls -> S400+; MVVM/binding -> S6 (S256); styles/templates -> S9 (S369); compiled .vbaml -> S7 (S293); live-reload -> S222_1 + S825 mature; pan/zoom -> S224. RESERVED S227-S239 for viewer-growth steps discovered mid-execution (gap-headroom convention). CHECKPOINT regenerated: last completed = S226, next = S240 (Sprint 6 first token, dep:S226 - matches every prior wrap precedent: S009->S030, S049->S080, S097->S120, S139->S140, S147->S160, S172->S200, S179->S200, S215->S220). PENDING none. Guard-rail MINIMAL REAL carried forward unchanged (tools->S287; element seeds->S401/S400/S402/S403/S404/S405/S440; event hooks->S322).
- **NEXT = S240** (dep:S226 - Sprint 6 first token: PropertyPath parser + evaluator - dot-path, indexers, null-safe, errors reported; S240 dep:S226, per the roadmap Sprint 6 Binding engine + MVVM core title). ADR-021 governs: the viewer consumes ONLY the public surface; viewer growth lands on S227-S239 as each sprint token completes.
## SPRINT 6 — SURVIVOR  (the consolidated surviving state a fresh LLM must assume — S240.., the Binding engine + MVVM core)
- **S240 (dep:S226) — PropertyPath parser + evaluator — DONE + VERIFIED from first principles BOTH CI lanes this session.** Sprint 6 first token. NEW src/core/include/vaml/core/PropertyPath.h + src/core/src/PropertyPath.cpp + test/unit/PropertyPath_test.cpp (17 TEST_CASEs) + build/verify_s240.cmd + MOD src/core/CMakeLists.txt + test/unit/CMakeLists.txt + docs/roadmap.md + progress.md + session-context.md. The Class A (syntax/authoring) vs Class B (evaluation/runtime data outcome) split — the S240_1/BindingDiagnostic precondition (recorded, NOT silent): `PropertyPathError` (uint8_t) = Empty/TrailingDot/ConsecutiveDot/UnterminatedIndexer/EmptyIndexer/MalformedIndex/UnclosedQuote (Class A, parse-time) + NullIntermediate/MissingProperty/NotReadable (Class B, evaluation outcomes, never thrown by parse). `PropertyPathException : std::logic_error` (kind/path/fragment + a what() naming the authored fragment; thrown by parse on any Class A error). `PropertySegment` (Property|Index) + `PropertyIndex` (Integer int64_t signed | StringKey) + `PropertyPath` (parse throws / try_parse noexcept sets *err leaves out untouched / valid / segment_count / segments / original) — hand-rolled WPF-faithful grammar (ADR-011 clean, no deps). EVALUATOR: `PropertyRead { ok, error, value, segmentIndex }` + `PropertyReader = std::function<PropertyRead(const Value&, const PropertySegment&)>` — the REAL injectable step-reader seam (S241/S242/S252 rebase point, NO-LAZY) + `evaluate_property_path` walks NULL-SAFE (final Null = valid ok=true; non-final Null → NullIntermediate at that index; reader failure propagates error+segmentIndex; empty/invalid → Empty; ADR-018 zero locks). DEFAULT READER `dependency_object_reader()`: reads UntypedPtr→DependencyObject property segments via obj->Get(PropertyId(name)) — unregistered → MissingProperty (never a raw thrown PropertyNotFound); Index on a DependencyObject → NotReadable (collection readers land with collections — NO-GAP recorded); non-object/null root → NotReadable. Makes S240 genuinely usable today against registered DPs on the S036/S039 store. Both CI lanes green from first principles: windows-msvc (verify_s240.cmd msvc-debug warning-as-error CLEAN + ctest 3/3 PASS incl. the S240 suite) + ubuntu-clang (/mnt/hgfs/VamlPOC clang-debug: build warnings==0 — a -Wunused-variable on two test constants caught + fixed — + ctest 3/3 PASS incl. the S240 suite). NO-GAP (recorded, not silent): the PropertyReader seam is S241/S242/S252's named rebase point, not a silent backfill; S240_1 consumes the Class A vs Class B split. No MINIMAL REAL introduced/resolved. No commits.
- **S240_1 (dep:S240) — BindingDiagnostic + IBindingDiagnostics — DONE + VERIFIED from first principles BOTH CI lanes this session.** The diagnostics infrastructure that consumes the S240 Class A vs Class B split AT EMIT. NEW src/core/include/vaml/core/BindingDiagnostics.h + src/core/src/BindingDiagnostics.cpp + test/unit/BindingDiagnostics_test.cpp (10 TEST_CASEs / 46 assertions, Shouldly vocabulary, pure value-domain — registers NO property names) + build/verify_s240_1.cmd. Surfaced: `BindingClass { StaticAuthoring, DefinedRuntimeOutcome }` (the A/B tag) + `BindingDirection { OneTime, OneWay, TwoWay }` + `BindingReasonCode` (uint8_t, 11 arms each documented with its emitting step: Class A — EmptyPath/MalformedPath (S240 resolve), ConverterSignatureMismatch [fwd→S242_1 Strict]; Class B — NullIntermediate/MissingProperty/NotReadable (S240 resolve, REAL+testable NOW), ConverterFailed/FallbackApplied [fwd→S242], SetTargetFailed [fwd→S243], WriteBackRejected [fwd→S244], Detached [fwd→S245 silenced non-error]) + `binding_class_of(BindingReasonCode) noexcept` (the .cpp table = the single tagging authority) + `BindingDiagnostic { bindingId, sourcePath, targetProp, direction, reasonCode, sourceType, targetType, converter, detail, timestamp, sourceFile, sourceLine }` (EXACT roadmap field set, owned std::string, timestamp stamped at creation, bindingClass() derived) + `IBindingDiagnostics` (const-ref observer, ADR-016 "subscriber gets info, never control") + `NullBindingDiagnostics` (NO-OP default — zero release cost unless subscribed) + `default_binding_diagnostics()/binding_diagnostics()/set_binding_diagnostics(IBindingDiagnostics&)` (lockless atomic current sink, ADR-018 zero new locks; set returns the previous, startup-only) + TWO factories `diagnostic_from_property_read` (S240 resolve Class B: MissingProperty/NullIntermediate/NotReadable → Class B reasonCode; degenerate invalid path → EmptyPath Class A) + `diagnostic_from_path_error` (Class A parse failure → MalformedPath/EmptyPath). Forward reason-code arms tagged with their emitting steps (S242/S242_1/S243/S244/S245) — a declared forward contract, never a silent backfill (NO-GAP/NO-LAZY-LLM); their A/B mapping fully exercised by the S240_1 tests NOW, their EMISSION bound to the named step. ADR-011/014/016 maintained (pure core; diagnostics separate from control flow; NO-OP default in release, opt-in). Both CI lanes green from first principles: windows-msvc (verify_s240_1.cmd, msvc-debug warning-as-error CLEAN + ctest 3/3 PASS incl. the S240_1 suite) + ubuntu-clang (/mnt/hgfs/VamlPOC clang-debug: build warnings==0 — an unused test helper was caught + fixed — + ctest 3/3 PASS incl. the S240_1 suite). No MINIMAL REAL introduced/resolved. No commits.
- **S241 (dep:S240) — ObservableObject: Set() + typed PropertyChanged signal — DONE + VERIFIED from first principles BOTH CI lanes this session.** NEW src/core/include/vaml/core/ObservableObject.h + src/core/src/ObservableObject.cpp + test/unit/ObservableObject_test.cpp (16 TEST_CASEs / 49 assertions, Shouldly vocabulary, S241-unique names) + build/verify_s241.cmd + MOD src/core/CMakeLists.txt (+ObservableObject.cpp) + test/unit/CMakeLists.txt (+ObservableObject_test.cpp). The REAL `vaml::ObservableObject` (the INotifyPropertyChanged-analogue) — a standalone PLAIN base for ViewModels, deliberately NOT derived from DependencyObject (a VM is an INotifyPropertyChanged SOURCE, not a framework element: no DPs/visual tree/affinity/coerce-validate; lightweight + movable + one job). Set/Get store named typed `Value`s (owned-name keyed map — S036/S037/S140 owned-name discipline, never the non-owning PropertyId view; Value's private default ctor honored via insert_or_assign, not operator[]). THE TYPED PROPERTYCHANGED SIGNAL (the headline): `PropertyChangedHandler = std::function<void(PropertyId,const Value&,const Value&)>` fires (id, old, new); `add_property_changed(handler)` = BROADCAST (WPF-style single event; an invalid/empty PropertyId sentinel) + `add_property_changed(PropertyId, handler)` = PER-PROPERTY auto-filtered convenience (the exact shape the S242+ binding engine subscribes with); BOTH back ONE registration-ordered entry list fired in registration order (mirrors the S041 token/order discipline exactly); `remove_property_changed(token)` removes one, remaining keep order, re-add appends; nested PropertyChangedToken/Handler types fully-qualified at the out-of-line definitions (MSVC). SET FIRES ONLY ON A REAL CHANGE (old != new by Value equality — the S035/S036/S037 discipline S243's invalidation coalescing relies on); Set returns true on a fired change, false on an equal value (no store change, no signal). `has()` distinguishes a Set-to-Value::Null property (present) from a never-set name (absent). REBASE ONTO S240: `observable_object_reader()` — a REAL `PropertyReader` so the S242+ binding engine reads ObservableObject through the SAME `evaluate_property_path`: present -> Get (final null ok=true; non-final null -> the evaluator yields NullIntermediate), absent -> MissingProperty (Class B, via has(), never a raw throw), Index segment/non-UntypedPtr-ObservableObject root -> NotReadable — making the S240_1 Class B resolve points reachable on the ObservableObject axis (the engine emits the S240_1 BindingDiagnostic from them at S242+, NO-GAP). ADR-018: ZERO new locks (a ViewModel is mutated/read on its owner thread). NO-GAP: S242 consumes ObservableObject as the source axis; S247_1's PROPERTY_BINDABLE layers over it; S252 {Binding} resolves onto it. No MINIMAL REAL introduced/resolved. Both CI lanes green from first principles: windows-msvc (build/verify_s241.cmd, msvc-debug configure + build warning-as-error CLEAN + ctest 3/3 PASS incl. the S241 suite 16 cases/49 asserts) + ubuntu-clang (/mnt/hgfs/VamlPOC clang-debug: build warnings==0 + ctest 3/3 PASS incl. the S241 suite 16 cases/49 asserts; -Wunused-variable + [[nodiscard]]-discard warnings caught + fixed). No commits.
- - **S242 (dep:S241) - BindingExpression: source/target, mode one/one-time/two-way, converter, fallback value - DONE + VERIFIED from first principles BOTH CI lanes this session.** NEW src/core/include/vaml/core/BindingExpression.h + src/core/src/BindingExpression.cpp + test/unit/BindingExpression_test.cpp (17 TEST_CASEs / 63 assertions, Shouldly vocabulary, S242-unique object-scoped DPs on real DependencyObject targets + real ObservableObject sources) + build/verify_s242.cmd. The REAL `vaml::BindingExpression` (the consumable expression, NOT a STUB / NOT MINIMAL REAL): `BindingMode { OneTime=0, OneWay=1, TwoWay=2 }` + `to_binding_direction(mode)`; `BindingConversion { ok, value, detail }` + `BindingConverterFn` + `BindingConverter { convert; convertBack }` (convertBack = the S244 two-way seam, empty at S242); `TransferResult { Applied, NoChange, ResolveFailed, ConverterFailed, TargetRejected, FallbackApplied }`; `BindingExpressionSpec` (source/sourcePath/target/targetProp/mode/converter/converterName/fallback/sourceType/targetType); move-only RAII `BindingExpression` (monotonic bindingId, ADR-018 zero locks) with `create(spec)` CLASS-A GATE (try_parse via S240; parse failure -> emits MalformedPath/EmptyPath + returns nullptr, NO bind, NO fallback - Class A never falls back; Strict later -> S242_1) + `TransferSourceToTarget()` + accessors. PIPELINE: source resolve through evaluate_property_path(Value::UntypedPtr(source), path, observable_object_reader()) with a non-UntypedPtr/null-root NotReadable guard; Class B resolve (NullIntermediate/MissingProperty/NotReadable) -> emit diagnostic_from_property_read (the S240_1 forward arm) + apply the bound fallback (Class B only) -> FallbackApplied else ResolveFailed (target untouched); converter (catching a throwing converter), ok=false -> ConverterFailed then fallback-if-present; target DP Set mapped APPLIED/NO_CHANGE/QUEUED/REJECTED (+ ValueTypeMismatch/PropertyNotFound/PropertyReadOnlyException catches) -> Applied/NoChange/TargetRejected - NO SetTargetFailed emission (that arm is S243's; a wrong-typed/refused/queued Set is RETURNED as TargetRejected, never emitted here). ADR-016 (diagnostics = observation never control; Class B never throws) + ADR-018 (zero new locks). NO-GAP (recorded, not silent): live source subscription + SetTargetFailed = S243 (one-way); convertBack + target-to-source + WriteBackRejected = S244 (two-way); Strict -> S242_1; {Binding} markup -> S252. Both CI lanes green from first principles: windows-msvc (build/verify_s242.cmd: msvc-debug warning-as-error CLEAN + ctest 3/3 PASS incl. the S242 suite) + ubuntu-clang (/mnt/hgfs/VamlPOC clang-debug: build warnings==0 + ctest 3/3 PASS incl. the S242 suite; a -Wmissing-field-initializers + a -Wunused-parameter caught + fixed). No MINIMAL REAL introduced/resolved. No commits.
- **S243 (dep:S242) — Binding engine one-way: source→target on change; DP invalidation coalescing — DONE + VERIFIED from first principles BOTH CI lanes this session.** NEW live ONE-WAY source→target subscription on `vaml::BindingExpression` + the S240_1 SetTargetFailed forward arm + the DP invalidation COALESCING proof (see the checkpoint digest above for the full contract + suite; NEW test/unit/BindingOneWay_test.cpp 15 TEST_CASEs + build/verify_s243.cmd; MOD BindingExpression.{h,cpp}). BOTH CI LANES green from first principles: windows-msvc (verify_s243.cmd msvc-debug warning-as-error CLEAN + ctest 3/3 PASS incl. the S243 suite) + ubuntu-clang (/mnt/hgfs/VamlPOC clang-debug: build warnings==0 + ctest 3/3 PASS incl. the S243 suite). NO MINIMAL REAL introduced/resolved.
- **S244 (dep:S243) — Binding engine two-way: target→source write-back; converter both directions — DONE + VERIFIED from first principles BOTH CI lanes this session.** NEW target→source write-back on `vaml::BindingExpression` + the convertBack reverse-converter wire + the S240_1 WriteBackRejected forward arm + the S041 self-fire completion + the WPF-faithful IsInTransfer guard (see the checkpoint digest above for the full contract + suite; NEW test/unit/BindingTwoWay_test.cpp 14 TEST_CASEs / 56 assertions + build/verify_s244.cmd; MOD BindingExpression.{h,cpp} + DependencyObject.{h,cpp} + test/unit/CMakeLists.txt). NOTES: the S041 DependencyObject self-fire (Set + drain-apply firing the object's own add_property_changed subscribers) is the contract-completion refactor — the S041 header always documented it; S244 surfaced the latent gap (target self-change observation) and completed it, in line with the sprint's refactor-culture directive. The loop-safety design was validated against the authoritative WPF source (E:\GitRepos\wpf BindingExpression/Base: NeedsUpdate + IsInTransfer) before finalizing. BOTH CI LANES green from first principles: windows-msvc (verify_s244.cmd msvc-debug warning-as-error CLEAN + ctest 3/3 PASS incl. the S244 suite, prior suites green) + ubuntu-clang (/mnt/hgfs/VamlPOC clang-debug: build warnings==0 + ctest 3/3 PASS incl. the S244 suite; a -Wunused-function caught + fixed). NO MINIMAL REAL introduced/resolved.
- **S245_1 (dep:S245) - Sprint 6 - Binding-failure test suite: harness subscribes to the sink and asserts exact reasonCode per Class (not just fallback values); detach/lifecycle (S245) is a Class-B-tagged, silenced non-error; Diagnostics != control-flow (subscriber gets info, never control) - DONE + VERIFIED from first principles BOTH CI lanes this session. TEST-ONLY binding-failure CONSOLIDATION SUITE (the S040/S042/S044/S125/S128/S130/S133/S135/S137 test-only precedent); ZERO new product symbols, NO MINIMAL REAL introduced/resolved. NEW test/unit/BindingFailure_test.cpp (12 TEST_CASEs / 83 assertions, Shouldly vocabulary, S245_1-unique object-scoped DPs + real ObservableObject sources through the real S241 reader + S240 evaluator + S240_1 sink) + NEW build/verify_s245_1.cmd + MOD test/unit/CMakeLists.txt. THE HARNESS SUBSCRIBES TO THE SINK via a subscribing RecordingSink (SinkGuard installs + restores the S240_1 sink) and asserts the EXACT reasonCode SEQUENCE PER CLASS (not just fallback values). Class A (StaticAuthoring): malformed/empty path -> create returns nullptr + exactly [MalformedPath]/[EmptyPath] with NO fallback (Class A never falls back); Class A Strict -> emits [MalformedPath] THEN throws BindingPolicyException (the emit is never the control flow, the throw is). Class B (DefinedRuntimeOutcome), exact codes: [MissingProperty], [MissingProperty, FallbackApplied], [NullIntermediate], [NotReadable], [ConverterFailed] (+[FallbackApplied] when bound), live OneWay [SetTargetFailed] (caller-driven FIRST push silent - S242/S243 parity re-proof), live TwoWay [WriteBackRejected] (source untouched). HEADLINE DETACH/LIFECYCLE (S245) IS A CLASS-B-TAGGED SILENCED NON-ERROR: destroying the target element -> a later write emits exactly [Detached] ONCE, binding_class_of(Detached)==DefinedRuntimeOutcome, distinct from SetTargetFailed/WriteBackRejected, never throws, never a dangling write. DIAGNOSTICS != CONTROL-FLOW: the same Class B failure run with the NO-OP sink vs a RECORDING sink yields the IDENTICAL TransferResult + target state, and a Class A default-Fallback create returns nullptr with its diagnostic still emitted (the subscriber observes, never steers). BOTH CI LANES green from first principles: windows-msvc (verify_s245_1.cmd msvc-debug warning-as-error CLEAN + ctest 3/3 PASS incl. the S245_1 suite 12/12, 83/83; prior suites green) + ubuntu-clang (/mnt/hgfs/VamlPOC clang-debug: build warnings==0 + ctest 3/3 PASS incl. the S245_1 suite 12/12, 83/83). ADR-016 (diagnostics = observation, never control) + ADR-018: ZERO new locks. NO-GAP (recorded, not silent): this TEST-ONLY step adds NO new forward contracts; S246 (RelayCommand) is the next chain link. NO MINIMAL REAL introduced/resolved.
- **NEXT = S246** (dep:S245 - Sprint 6 - RelayCommand (ICommand-analogue): invoke + CanExecute + CanExecuteChanged (full, no stub); consumes S245_1's binding-failure consolidation + continues the gapless dep chain).
- **S246 (dep:S245) + S247 (dep:S246) - ICommand-analogue + RelayCommand implementation + tests - DONE + VERIFIED from first principles BOTH CI lanes this session.** S246 (the command primitive, FULL + no stub): NEW src/core/include/vaml/core/ICommand.h (the pure abstract `vaml::ICommand` - Execute(Value) the invoke, CanExecute(Value) const the pure side-effect-free query, add_can_execute_changed(handler)->token / remove_can_execute_changed(token)->bool the CanExecuteChanged EVENT exposed ON the interface, WPF-faithful, token-subscription registration-ordered per ObservableObject S241 / DependencyObject S041, a strict superset of the cross-repo delegate.h/Accupulse iterator-handle style recorded-not-imported; ADR-018 zero locks) + NEW src/core/include/vaml/core/RelayCommand.h + NEW src/core/src/RelayCommand.cpp (`vaml::RelayCommand final : ICommand` - REQUIRED ExecuteAction std::function<void(const Value&)> + optional CanExecutePredicate (null => CanExecute TRUE, the WPF default); Execute invokes the exact parameter; CanExecute side-effect-free; NotifyCanExecuteChanged raises in REGISTRATION order; MOVE-ONLY per S170, moved-from INERT) + MOD src/core/CMakeLists.txt (+RelayCommand.cpp) + NEW src/core/ICommand/RelayCommand applied + NEW build/verify_s246.cmd. S247 (the test suite, TEST-ONLY plus the product it proves): NEW test/unit/RelayCommand_test.cpp (13 TEST_CASEs / 52 assertions, Shouldly vocabulary, pure value-domain, registers NO property names) - execute-only always-executable, predicate-governed CanExecute per parameter, Execute invokes the EXACT parameter Value, subscribe -> fire once, multiple subscribers fire in REGISTRATION order, remove + re-add appends (true/false tokens), distinct + monotonic tokens, zero-subscriber safe no-op, side-effect-free CanExecute, Value::Null() parameter, the MOVE-ONLY transfer (move ctor + move-assign re-arm, moved-from inert), the re-query contract (captured flag flips CanExecute + subscriber observes current flag) + MOD test/unit/CMakeLists.txt (+RelayCommand_test.cpp) + NEW build/verify_s247.cmd. NO MINIMAL REAL introduced (both symbols complete + permanent; consumed by S247's tests + the S253 {Command} markup / S441 app-shell). NO-GAP (recorded, not silent): S247_1 (Bindable-property convenience) + S253 ({Command}) + S441 (app-shell RelayCommands) consume this command primitive. ADR-018: ZERO new locks. BOTH CI LANES green from first principles: windows-msvc (verify_s246.cmd msvc-debug warning-as-error CLEAN + ctest 3/3 PASS incl. the S247 suite 13/13, 52/52; prior suites green) + ubuntu-clang (/mnt/hgfs/VamlPOC clang-debug: build warnings==0 + ctest 3/3 PASS incl. the S247 suite 13 cases / 52 assertions SUCCESS). No commits.
- **NEXT = S247_1** (dep:S247 - Sprint 6 - Bindable-property convenience over the S240/S241 core (PROPERTY_BINDABLE(type,Name)-style generator): backing field + change notification; the Toolkit-analog convenience layer (vaml/mvvm); consumes S246's RelayCommand + S241's ObservableObject + continues the gapless dep chain).
- **S247_1 (dep:S247) - Sprint 6 - Bindable-property convenience over the S240/S241 core (PROPERTY_BINDABLE(type,Name)-style generator): backing field + change notification; the Toolkit-analog convenience layer (vaml/mvvm) - DONE + VERIFIED from first principles BOTH CI lanes this session.** NEW src/core/include/vaml/mvvm/property_bindable.h (header-only vaml/mvvm convenience layer compiled into vaml-core; NO runtime DI, NO reflection — source-generation spirit, plain C++ templates): (A) `vaml::mvvm::to_value(...)` the bounded-S030-payload overload set (bool/std::int32_t/std::int64_t/float/double/std::string/Color/Thickness/Matrix2D -> Value factories; a non-listed T fails to compile = the bounded-domain guard); (B) THE TOOLKIT-ANALOG FREE TEMPLATE `set_property(ObservableObject&, PropertyId, T&, const T&)` — REAL-CHANGE-ONLY (false on `field == new_value`, no store write + no signal; on a real change assigns the field AND calls `obj.Set(id, to_value(new_value))` firing the GENUINE S241 typed PropertyChanged (id, old, new)); (C) `PROPERTY_BINDABLE(TYPE, NAME)` macro -> `private TYPE NAME_field_ = TYPE{}` + `const TYPE& NAME() const noexcept` getter + `bool set_NAME(const TYPE&)` (wiring set_property; NAME bound via PropertyId::of(#Name) — compile-time, no registry, no collision). FIELD<->STORE SYNC CONTRACT documented in the header (field = C++ source of truth; set_property keeps the ObservableObject Value store in lock-step so observable_object_reader()->Get + the S242 binding engine read the live value; bindable props written ONLY via their generated setter — base Set directly would desync the mirror, documented footgun). NO change to ObservableObject (S241 surface fixed); NO new library target (header-only); ADR-018 ZERO new locks (a bindable prop is written on the VM's owner thread, mirrors ObservableObject); NO MINIMAL REAL introduced/resolved. NEW test/unit/PropertyBindable_test.cpp (11 TEST_CASEs / 58 assertions, Shouldly vocabulary): default backing TYPE{}; setter true-on-real-change + getter AND ObservableObject store both update; the real S241 signal fires (id, old, new) with the right id + old value; same-value -> false NO store NO signal; per-type real-change (Color/Thickness/Double); independence of multiple bindable props; reader parity through observable_object_reader + the S240 evaluator (live value + absent -> MissingProperty); direct (non-macro) set_property usage; Color/Thickness round-trip through the bounded Value store. MOD test/unit/CMakeLists.txt (+PropertyBindable_test.cpp with the S247_1 comment block) + NEW build/verify_s247_1.cmd + MOD docs/roadmap.md (S247_1 [DONE] with the full digest + [VIEWER GROWTH: S256]) + progress.md (this entry) + session-context.md. NO-GAP (recorded, not silent): S256 (viewer growth — Reload populates a VM + renders {Binding}) consumes this convenience layer when the viewer populates a VM; S250 (app-owned composition, the next step) + S253 ({Command}) + S441 continue the gapless chain. BOTH CI LANES GREEN from first principles: windows-msvc (build/verify_s247_1.cmd msvc-debug warning-as-error CLEAN + ctest 3/3 PASS; the vaml-core-tests exe PropertyBindable suite 11/11 cases, 58/58 assertions) + ubuntu-clang (/mnt/hgfs/VamlPOC clang-debug: configure + build warnings==0 + ctest 3/3 PASS; PropertyBindable suite 11/11, 58/58 SUCCESS). No commits.
- **NEXT = S250** (dep:S247_1 - Sprint 6 - STATIC APP-OWNED COMPOSITION (no library DI): the engineer authors their OWN VM/repository/command construction; the lib only DOCUMENTS the pattern; S250_1/S251/S252/S253 follow to the S6 completion + viewer growth S256).
- **S252 (dep:S251) - MARKUP {Binding} RESOLVED IN VamlReader (the S2 tag bound) - DONE + VERIFIED from first principles BOTH CI lanes this session.** NEW src/core/include/vaml/core/BindingMarkup.h + src/core/src/BindingMarkup.cpp (the {Binding}/{Command} markup parser - REAL permanent core surface, NOT MINIMAL REAL) + MOD src/core/include/vaml/core/VamlReader.h + src/core/src/VamlReader.cpp + MOD src/core/CMakeLists.txt + NEW test/unit/BindingMarkup_test.cpp (15 TEST_CASEs) + NEW test/unit/VamlReader_binding_test.cpp (9 TEST_CASEs incl. S253) + NEW build/verify_s252.cmd. SURFACE: (1) `parse_markup_binding(source) -> BindingMarkup { kind, path, mode, hasMode, malformed }` with `MarkupExtensionKind { None, Binding, Command }` - parses `{Binding Title}` / `{Binding Title, Mode=TwoWay}` / `{Command DoIt}`; kind None (StaticResource/x:Null) is NOT ours (stays deferred to S361); malformed {Binding}/{Command} (empty path, bad Mode= value, no comma, {Command} with no name) sets `malformed` (Class A authoring). (2) `VamlReader::set_data_context(ObservableObject*)` (the app-constructed VM, S250/S251 pattern; NO DI/NO container - app owns the VM + sets it) + `VamlReader::BindExtensions()` - the S2-tag RESOLUTION: for each deferred extension whose source parses to a well-formed {Binding}, it builds a LIVE `BindingExpression` (source = data context, sourcePath = parsed path, target = the object the extension was authored on, targetProp = the attribute key, mode = WPF-faithful OneWay default or Mode= override, policy = `vaml::view_binding_policy_default()` (S250_1 Fallback), diagnostics through the S250_1 channel), applies the FIRST transfer (S242), and reader-owns it (RAII; `live_bindings()`, cleared per Load). A MALFORMED {Binding} THROWS `VamlReaderException(VamlReaderError::MalformedBindingMarkup)` naming source + key (Class A, never silently deferred - the new reader arm this step enables). kind None stays deferred; NOT an error. (3) The app's data context is a plain VM (a real ObservableObject + PROPERTY_BINDABLE per S251). TESTS: {Binding} path + Mode variants + whitespace tolerance; {Command DoIt} -> Command kind; StaticResource/x:Null -> None; malformed -> malformed flag; BindExtensions -> bound count 1 + live_bindings size + FIRST VALUE on the target DP + LIVE one-way re-push (set_Title -> target updates, the S243 wire through markup) + {Binding Title, Mode=TwoWay} -> a target edit WRITE-BACKS to the VM (S244 through markup) + malformed {Binding} -> MalformedBindingMarkup throw (+ ShouldThrow form) + {StaticResource X} stays deferred (bound 0, no error). NO-GAP (recorded, not silent): S253 ({Command}) + S256 (viewer growth) consume this; ADR-018 ZERO new locks. BOTH CI LANES green from first principles: windows-msvc (build/verify_s252.cmd msvc-debug warning-as-error CLEAN + ctest 3/3 PASS incl. the new suites) + ubuntu-clang (/mnt/hgfs/VamlPOC clang-debug: build warnings==0 + ctest 3/3 PASS, new suites green). No MINIMAL REAL introduced/resolved. No commits.
- **S253 (dep:S252) - COMMAND via {Binding DoIt} + the one/two-way MARKUP BINDING END-TO-END - DONE + VERIFIED from first principles BOTH CI lanes this session (the S252 verify lane).** MOD src/core/include/vaml/core/VamlReader.h + src/core/src/VamlReader.cpp (the S253 command surface) + covered at S252's VamlReader_binding_test.cpp (S253 cases). SURFACE: the `{Command <name>}` markup extension - parsed by S252's BindingMarkup (kind Command), LEFT DEFERRED by BindExtensions (NOT a binding - the S252 NO-GAP hand-off resolved here), then resolved + invoked through TWO new reader APIs: `VamlReader::register_command(name, ICommand*)` (app-owned command registry - the app constructs its RelayCommand per S250 app-owned composition + registers it here, NON-OWNING, idempotent, NOT per-Load cleared - survives the tree it is authored on) + `VamlReader::ResolveCommand(name) -> ICommand*` (the S246 ICommand interface = the S253 command surface). The one/two-way markup binding end-to-end: S252's {Binding} markup already arms the OneWay live wire (S243) + the `Mode=TwoWay` write-back (S244) through the reader-owned live bindings - complete in both directions via markup (proven by VamlReader_binding_test). NO-GAP (recorded, not silent): S256 (viewer growth) consumes the app-constructed VM + the reader binding + command path; S441 app-shell RelayCommands build on this command surface. ADR-018 ZERO new locks. BOTH CI LANES green from first principles (build/verify_s252.cmd + /mnt/hgfs/VamlPOC clang-debug). No MINIMAL REAL introduced/resolved. No commits.
- **S254 (dep:S253) - REFACTOR GATE(S6) - DONE + VERIFIED from first principles BOTH CI lanes + ASan/LSan ZERO-LEAK this session.** Full MVVM in v1 - all modes (OneTime/OneWay/TwoWay) + converters + Fallback/Strict policy + diagnostics wired, NO stubs. (1) every mode covered through the REAL path (the S252 BindExtensions + the S240-S251 engine, all headless suites green). (2) grep-audit of the S240_1..S253 surface: ZERO untargeted STUB/TODO/FIXME/backfill - only sanctioned NO-GAP / NOT-a-STUB commentary; the only forward deferrals are recorded NO-GAP (StaticResource->S361, {Command}->S253 consumed, Button/Click->S401/S322). (3) ASan ZERO memory errors on msvc-asan (vaml-core-tests 871/871) + ASan+LSan ZERO LEAKS on clang-asan (/mnt/hgfs/VamlPOC, detect_leaks=1 exitcode=86, 100% passed 0 failed out of 3). CI note: full ASan build on the Windows box needed --parallel 2 (environment memory pressure - MSVC C1060 under parallel ASan; the non-ASan lane unaffected). The viewer-growth obligation for S256 is ARMED here. No MINIMAL REAL introduced/resolved. No commits.
- **S255 (dep:S254) - SURVIVOR + CHECKPOINT ADVANCE TO S280 - DONE (docs-only wrap; both CI lanes remain green from the S254 gate + zero-leak).** The Sprint 6 survivor block (S240..S254) consolidated here; checkpoint regenerated: last=S255, next=S280 (Sprint 7: UI/viewer + host rendering + app-shell). Guard-rail MINIMAL REAL carried forward INTACT (the only tagged markers are S092 Strand C event hooks -> S322 + the S207/S208 encoder -> S361, both in the roadmap as named resolution steps - no silent backfill). No MINIMAL REAL introduced/resolved this block. No commits.
- **S256 (dep:S253) - VIEWER-GROWTH: reload populates a VM (plain ctor; NO DI) + renders {Binding} - DONE + VERIFIED from first principles BOTH CI lanes this session.** NEW vaml_viewer/viewer_binding.h (the S256 seam, header-only, ADR-021 consumer path: `ViewerDemoService` + `ViewerDemoViewModel` (a REAL ObservableObject + PROPERTY_BINDABLE(double, Dim), plain ctor wiring the injected service by value - NO DI, NO container, the S250 "engineer wires, we bind" headline) + `viewer_bind_render_action(loadOk)` (the PURE Render/Report decision) + `run_viewer_binding_plan(text, dim)` (the REAL plan runner: constructs the VM + service, Loads the text through a RETAINED heap-owned VamlReader, set_data_context(vm) + BindExtensions() (the S252 markup resolution), returns { ok, root, vm, retained reader, bound, detail }; the reader + VM are returned OWNED so the S243 LIVE WIRE STAYS ARMED - a later `vm->set_Dim(...)` re-pushes the bound target through the retained reader, the update-in-window headline)) + MOD vaml_viewer/app.{h,cpp} (`bind_and_render` on the S222_1 reload path + a read_file_text helper) + NEW test/unit/ViewerBindingPlan_test.cpp (5 TEST_CASEs) + MOD test/unit/CMakeLists.txt. RENDERABLE PROOF WITH TODAY'S RENDERER: `<StackPanel Opacity="{Binding Dim}"/>` binds the VM's Dim -> the S252 FIRST TRANSFER lands the bound Dim onto the target Opacity DP (the S120 Opacity the S208 encoder folds into the alpha) -> the panel visibly dims in the live window. Headless proof: the app-constructed VM (plain ctor + injected service), the pure decision table, the plan binds `{Binding Dim}` onto the target Opacity (bound==1, the S242 first transfer), the S243 LIVE WIRE re-pushes through the RETAINED reader (set_Dim 0.4->0.15->1.0 tracks the Opacity DP), and a malformed `{Binding}` document REPORTS a structured non-empty detail + the decision is Report (never silent). NO-GAP (recorded, not silent): the LIVE window/render proof (visible dim) remains [both apps, not CI] on this box (the standing S222+ precedent - feeds S443 app-shell markup bind + command path); real control MATERIALS (Text/Button -> S400/S401) do not exist yet - Opacity dimming is the honest visible binding available on today's surface. ADR-018 ZERO new locks (single-threaded UI-thread). BOTH CI LANES green from first principles: windows-msvc (vaml-core-tests 876/876 + vaml-vulkan-tests 211/211 + vaml-presentation-tests 39/39 PASS, 0 failed) + ubuntu-clang (/mnt/hgfs/VamlPOC clang-debug: build warnings==0 + ctest 3/3 PASS 0 failed). No MINIMAL REAL introduced/resolved (the VM + plan runner are REAL permanent code, kept-and-extended by S400/S443). No commits.
## SPRINT 4C — SURVIVOR  (the consolidated surviving state a fresh LLM must assume — S176..S179, closed at S179)
- [x] S176..S179 — Sprint 4C (three-lane testing harness, codification) SURVIVOR + checkpoint advance to S200 - DONE
  SURVIVOR: (dep chain S176→S177→S178→S179; Sprint 4C codification close; docs-only closure over the S176/S177 green lanes + S178/S179 documented). SPRINT 4C delivered the three-lane testing harness — the "nothing escapes testing" codification. RAIL 1 (S176, Cat 1/CI/headless): the S176 DECISION-A ENGINEERING-OUT collapsed the SwapchainStateMachine's three concrete members (genN_/msaaN_/framebuffersN_) into ONE injectable `ISwapchainGeneration` interface + a `RealGeneration` impl wrapping Swapchain+MsaaTarget+Framebuffer; the machine now holds a SINGLE `genN_` and builds it through the injectable `SwapchainStateBuilders::build` generation hook (`real()` default = Swapchain::create/MsaaTarget::create/Framebuffer::create). The RAIL 1 mock-ICD suite (test/unit/MockIcd_test.cpp) scripts OUT_OF_DATE + failed vkCreateSwapchainKHR + failed image-view/depth/framebuffer build + DEVICE_LOST against a scripted MockGeneration and asserts each failure lands the machine in the correct state (Ready/Retryable/Fatal/Suspended), Gen N stays live + presentable, invalid N+1 generations are never submitted, and teardown runs REVERSE build order — the machine proven against GENERATION OBJECTS, superseding/absorbing the decision-level ResizeRecreate/SwapchainGeneration suites (retained intact). RAIL 2 (S177, LOCAL this-box pre-✅ gate, NOT GH): test/wsi/WSIResize_main.cpp — a REAL non-doctest executable opening a hidden GLFW window + real Vulkan instance/surface/device/swapchain through the Presenter + real builders, VALIDATION LAYERS ENABLED + a VK_EXT_debug_utils messenger flagging ANY validation error, driving a programmatic RESIZE STORM + minimize/restore (zero-size park) + resize-during-flight; ANY validation error = FAIL. Enforced as build/verify_s177_wsiresize.cmd (requires a REAL RUN; SKIP(2) on this box = hard FAIL; exit 0=PASS/1=FAIL/2=SKIP), VERIFIED this session against the real NVIDIA RTX A5000 Laptop GPU: exit 0 = PASS after the full storm/minimize/restore/flight with ZERO validation errors; the target BUILDS green on both CI lanes (compile-only; CI runners have no surface so never RUN it). RAIL 3 (S178): the programmer live-acceptance checklist — device-specific presentation (multi-GPU/exotic present modes/customer-shipped GPU) is a PROGRAMMER's display-backed manual run, NOT a CI lane (the standing S160+ precedent); documented, not a silent shrug. S179: the deferred-not-silent ledger CLOSED — SW-fallback ABSENT (vulkan is the ONLY rasterization path; recorded for the user guide), auto-device-rebirth deferred later, UIA automation tree deferred later, `docs/testing.md` maturation deferred later (the harness's grown-up home, named and NOT created here), optional GPU/desktop nightly deferred later. Both CI lanes re-verified GREEN from first principles this session: windows-msvc (build/verify_s176.cmd: msvc-debug warning-as-error CLEAN + ctest 3/3 PASS) + ubuntu-clang (/mnt/hgfs/VamlPOC clang-debug: configure+build+ctest 3/3 PASS, warning-clean — only the pre-existing upstream <ciso646> doctest deprecation, non-error; vaml-wsi-resize built clean). windows-clang-debug is NOT a lane (not built/tested/flagged per standing rule). NO-GAP GUARANTEE honored: every S176/S177 real seam (ISwapchainGeneration, SwapchainStateBuilders::build, MockGeneration, the WSI harness) is Cat 1 REAL, forward-resolvable, no silent backfill. MINIMAL REAL accounting: NO new MINIMAL REAL introduced/resolved at S176–S179 (all real Cat-1 surfaces); active marks unchanged (vulkan RESOLVED at S161; tools→S287; built-in element seeds Button→S401/Text→S400/TextBox→S402/CheckBox→S404/Slider→S403/ScrollViewer→S405/Window→S440; VamlReader event hooks→S322; UIElement transform-owner→S208). No new repo md created beyond roadmap+progress+session-context (the operational files; docs/testing.md still deferred-not-silent to a later step). Roadmap S176..S179 [DONE]; checkpoint regenerated: last = S179, next = S200 (Sprint 5). No commits.
## SPRINT 7 — SURVIVOR  (the consolidated surviving state a fresh LLM must assume — S280..S293, the VBAML compiler + compiled-path viewer growth, CLOSED at S292)
- **S289 (dep:S288) — Pack/identity: x:Class name mapping; embedded resource lookup; release uses compiled only — DONE + VERIFIED from first principles BOTH CI lanes this session.** NEW src/tools/include/vaml/tools/Pack.h + src/tools/src/Pack.cpp (namespace vaml::tools::pack) — the identity + pack layer the RELEASE app-shell (S444) builds on + the resolution of the S092/S286 "x:Class → owner binding deferred" NO-GAP. `PackUri` + `parse_pack_uri` (WPF-pack-style: accepts `pack://application:,,,/Path.vbaml` OR a bare `Path.vbaml`; parses to {authority, path} with the AUTHORITY RULE — only `application` or empty, any other → PackUriError::UnsupportedAuthority; empty/invalid path → InvalidPath; the leading '/' normalized off the path; pure + noexcept). `ResourcePack` (RAII, the EMBEDDED-RESOURCE LOOKUP): add_resource (dedupe-throws ResourceAlreadyRegistered — a duplicate embed is an authoring/link error, never last-wins), add_embedded (the S287 `compiled_vbaml_<symbol>()` convenience), resolve (structured, empty-span on unknown), has, registered_keys (BY VALUE, deterministic registration order), size — content-keyed (the S143/S144 NameTable discipline: key interned/copied once, never transient on lookup). `resolve_pack_uri(pack, packUri)` — the pack-identity → bytes seam. MOD vaml/tools/VBamlReader.{h,cpp}: SetOwner/owner() (the x:Class OWNER), `InitializeComponent(owner, bytes)` (Load + bind owner + the CLASS OWNER MAPPING gate — when class names are registered, a decoded root_class NOT among them → new VBamlError::OwnerClassMismatch; null owner → structured), `InitializeComponentFrom(owner, ResourcePack&, packUri)` — THE COMPILED-ONLY RELEASE ENTRY (resolves the pack URI → bytes → Load → bind; takes NO text of any kind — the compiled-only contract enforced by signature), `register_class`/`is_registered_class` (the x:Class mapping). "release uses compiled only" recorded in the header + roadmap: the release consumer faces ONLY InitializeComponentFrom/VBamlReader + ResourcePack (compiled bytes); the text VamlReader + vamlcc are dev tools; linkage is the enforcement boundary (S444 app-shell / S484 single-exe), recorded NO-GAP not silent. ALL REAL permanent Category-1 — NONE MINIMAL REAL. ADR-018 ZERO new locks; ADR-021 headless. NEW test/unit/Pack_s289_test.cpp (15 TEST_CASEs: PackUri parse matrix, ResourcePack register/resolve/registered_keys order/dedupe-throws-unchanged/unknown→empty, add_embedded "embedded lookup", resolve_pack_uri seam, InitializeComponent owner binding + root_class + named() + class-mismatch→OwnerClassMismatch, null owner, InitializeComponentFrom resolves-through-pack identical-to-direct-Load + the no-text-parameter static_assert for "release uses compiled only", unregistered resource fails-loud). MOD src/tools/CMakeLists.txt (+Pack.cpp) + test/unit/CMakeLists.txt. BOTH CI LANES green: windows-msvc (verify_s289.cmd msvc-debug warning-as-error CLEAN + ctest 4/4 incl. the S289 suite) + ubuntu-clang (/mnt/hgfs/VamlPOC clang-debug build warnings==0 + ctest 4/4). NO-GAP (recorded not silent): S444 app-shell consumption (compile MainView.vaml→.vbaml → render via InitializeComponentFrom) is the named release consumer.
- **S290 (dep:S289) — Refactor-gated dual path: S2 reader = dev tool, S7 reader = release — DONE + VERIFIED from first principles BOTH CI lanes this session.** MOD both reader headers (vaml/core/VamlReader.h + vaml/tools/VBamlReader.h) with an explicit DUAL-PATH CONTRACT block: S2 VamlReader = DEV TOOL (live text reload S222_1, S256 binding, vamlcc's text→compiled input; NOT linked by release); S7 VBamlReader + ResourcePack = RELEASE (the only reader release links; text-free by construction via S289 InitializeComponentFrom — takes (owner, ResourcePack&, packUri), a pack identity resolving to COMPILED bytes, never text). The ONLY text→anything transition in the whole tree is vamlcc (compile_to_bytes), the dev compiler. The refactor-gated grep audit proves the compiled path (VBamlReader.cpp + Pack.cpp) contains ZERO calls to the text scanner/parser/VamlReader/compile_to_bytes/vamlcc/grep — the compiled path is a PURE consumer of DependencyProperty::TryGet + the registries + vbaml::decode. NO new machinery (a ReaderRole enum would be DEAD surface — the split is enforced at LINKAGE, ADR-003/ADR-021, recorded not silent). Docs recorded in roadmap + progress (no new md files — operational files only). BOTH CI LANES re-verified from first principles (verify_s289.cmd + clang-debug). ADR-018 ZERO new locks (docs + grep only). No MINIMAL REAL introduced/resolved.
- **S291 (dep:S290) — REFACTOR GATE(S7): FULL text→vbaml→tree pipeline, parity proven, no stubs — DONE (gate, docs+verify).** (1) Both CI lanes rebuilt + ctest 4/4 from first principles (verify_s289.cmd msvc-debug warning-as-error CLEAN; /mnt/hgfs/VamlPOC clang-debug warnings==0 + ctest 4/4). (2) The S288 parity harness re-run at the gate (text vs compiled behaviorally identical — the compiled path == the release path, proven, not assumed). (3) Grep-audit across src/tools: no STUB/TODO/FIXME/backfill scaffolds; the only suspending references are the sanctioned affirmative NOT-a-stub/NO-GAP statements + the recorded forward contracts (S289→S444, S286 {Binding}→release consumer, S286 events→S322). The S003 tools MINIMAL REAL sentinel is CONFIRMED resolved (removed at S287; no lingering `[MINIMAL REAL] tools` tag). (4) Dead-surface audit: every S7 public symbol is consumed (compile/compile_to_bytes/VBamlc — vamlcc + tests + S293; VBamlReader — tests + S289 InitializeComponent + S293 viewer + S444; PackUri/ResourcePack — S289 tests + S290 + S444; write_embed + embed spans — S287 + S289; DualPathParity — S288/S291) — nothing orphaned. (5) ASan no-leak. (6) No MINIMAL REAL introduced/resolved at the gate; no new md files.
- **S293 (dep:S288) — VIEWER-GROWTH: vaml_viewer opens a COMPILED `.vbaml`; reload recompiles then reopens — DONE + VERIFIED from first principles BOTH CI lanes this session.** The compiled path made LIVE in the viewer. NEW vaml_viewer/compiled_open.h (header-only seam, ADR-021, the S221 load_file-style consumer): CompiledOpenResult {ok, root (owned), reader (owned — the RETAINED S7 VBamlReader keeping named()/retained alive), detail} + load_compiled_bytes (VBamlReader::Load(bytes) → live tree; malformed .vbaml → typed non-empty detail, never silent) + compile_to_vbaml (in-process vaml::tools::compile_to_bytes — vamlcc reused, NOT re-shelled, ADR-021) + load_compiled_file (a `.vaml` path COMPILES first then loads; a `.vbaml` path loads directly; structured union of FileLoadError + VBamlException). MOD vaml_viewer/app.{h,cpp}: the SHARED stack-builder refactor (create() + create_compiled() delegate to a private create_impl(config, root, uiRoot, compiledReader) — no drift; the app RETAINS the S7 reader in compiledReader_ so the tree's non-root borrowed objects + the S286 named() registry stay alive across the app lifetime); the S222_1 reload in compiled mode RECOMPILES .vaml→.vbaml (vamlcc in-process) then reopens through the S7 reader, keeping last-good + reporting on failure (never blank); S256 text mode stays intact + co-exists. MOD vaml_viewer/cli.h (the `--compiled` flag → cli_wants_compiled + cli_action treats it as a flag not a positional + SHARED usage text) + both main.cpp (the compiled-open dispatch: --compiled forces the compiled path, a `.vbaml` auto-detects; a failed compiled-open is a structured never-silent exit). The viewer (Windows vcxproj: vaml-tools include dir + vaml-tools.lib both Debug+Release; vaml_viewer-linux CMakeLists: VAML_TOOLS_INCLUDE + VAML_TOOLS_LIB) gains the vaml-tools link. Compiled mode defers {Binding} (S286) — renders the REAL panels via the S212 classifier; binding-on-compiled = the S444/S322 release path, recorded NO-GAP not silent. NEW test/unit/ViewerCompiledOpen_s293_test.cpp (7 TEST_CASEs: load_compiled_bytes → live tree + retained reader + named()/root_class; malformed/empty buffered → typed non-empty detail; compile_to_vbaml (vamlcc) → VBamlReader round-trip; load_compiled_file `.vaml`-compiles-first vs `.vbaml`-direct-load parity; missing/bad file → loud failure; recompile-then-reopen — an edited source recompiles to a NEW tree with the new x:Name) + ViewCompiledPlan_s293_test.cpp (the --compiled decision table + ViewerApp::create_compiled factory contract — accepts a UIElement-rooted compiled result, rejects a non-ok/no-root one, never a blank window). The LIVE window proof (vaml_viewer --compiled sample.vaml → compile → render → mid-run rewrite recompiles + reopens) is [both apps, not CI] on the NVIDIA box (the standing S222+ display-proof precedent). BOTH CI LANES green: windows-msvc (verify_s289.cmd msvc-debug 4/4 incl. the S289 15 + S293 7 cases; the vcxproj viewer build green) + ubuntu-clang (/mnt/hgfs/VamlPOC clang-debug 4/4) + the vaml-viewer-linux target builds green with the vaml-tools link (verified on the VM). No MINIMAL REAL introduced/resolved (all seams REAL permanent, kept-and-extended by S444).
- **S292 (dep:S291) — SURVIVOR + checkpoint advance to S320 — DONE (docs-only wrap; both lanes green from S291/S293).** Sprint 7 SURVIVOR consolidated (S280..S291 + S293). VIEWER-GROWTH table updated: S7 → S293 marked DONE (the compiled path demonstrated live — the anti-grave-whistle satisfied). Checkpoint regenerated: last completed = S292, next = **S320** (Sprint 8 — RoutedEvent primitive), PENDING none (roadmap S320 already reads dep:S292 — the gapless chain intact). Guard-rail MINIMAL REAL updated: tools → **RESOLVED at S287** (version symbol kept + extended, sentinel removed); all other active marks carried forward INTACT (built-in element seeds → their control sprints; VamlReader event hooks → S322; transform-owner → S208 already resolved). No new MINIMAL REAL introduced/resolved across Sprint 7. ADR-018 reaffirmed: Sprint 7 added ZERO new locks (both readers + Pack are single-threaded/pure; the viewer compiled seam is UI-thread). Roadmap S289/S290/S291/S293/S292 all marked [DONE] (execution order S289→S290→S291→S293→S292 — S293 the growth token demonstrated BEFORE the S292 survivor, per the anti-grave-whistle gate). No commits.

