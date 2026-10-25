# VAML session-context (per-run state)

## last_command
implementit COMPLETE — SPRINT 8 CLOSED (S320 -> S321 -> S322 -> S323 -> S324 -> S325 -> S328 -> S329_1 -> S329) on the RE-SCOPED single-threaded plan. The routed-event + input chain is end-to-end GREEN on BOTH CI lanes. S326 (Dispatcher/Task) + S327 (async continuation -> VM -> binding) are DEFERRED to the NEW Sprint 11a (per the architect's re-scope decision this session) — their in-tree Dispatcher/Task/async code was PULLED so Sprint 8 is demonstrably single-threaded, single-instance, ZERO new locks.

## RE-SCOPE (architect decision, Sprint-8 plan review)
- Sprint 8 = single-threaded, single-instance, ZERO new locks ONLY (the "single-threaded smell" reasoning: the Dispatcher/async is a separate, larger parallel-threaded-UI sprint, not a Sprint-8 add-on).
- S326/S327 DEFERRED -> NEW Sprint 11a (roadmap S450–S459): IAsyncDispatcher REPLACEABLE seam (apps may inject their own — "we do not force uses that use their own async runtime to use ours"), Task<T>/Executor (ADR-010), co_async/co_await coroutines, InvokeAsync, async->VM->binding no-races, multi-instance-per-thread + one shared input pump, thread-pool-as-UI one-at-a-time. The deferral is a named resolution, NOT silent backfill (NO-GAP). ADR-010 re-pointed to 11a; ADR-018 reaffirmed for Sprint 8 as ZERO new locks.
- PULLED on re-scope: src/core/src/Dispatcher.cpp, src/core/include/vaml/core/Dispatcher.h, src/core/include/vaml/core/Task.h, test/unit/S326_Dispatcher_test.cpp, test/unit/S327_AsyncBinding_test.cpp + their CMake entries. No references remain (grep clean).
- The user asked a deep architecture Q (how the lib knows a UI thread / async without a provided runtime / app-injectable container + InvokeAsync); the conclusion matches the deferral design and is recorded in the 11a roadmap header + plan.

## plan_digest
Sprint 8 delivered (single-threaded, zero-lock), each green on windows-msvc + ubuntu-clang:
- S320 (dep:S292): Signal<T> (token/reentrancy-safe add-remove, copy-before-invoke) + RoutedEvent primitive + RouteEventDirection registry (Click/KeyDown/KeyUp statics).
- S321 (dep:S320): UIElement routed-handler surface + RaiseEvent (bubble source->root, tunnel root->source, handled stops both; source/originalSource).
- S322 (dep:S321): markup Click="OnClick" -> live handler via the class-owner registry (bind_class_handler + BindEvents); RESOLVES the S092 event-hook MINIMAL REAL seed (sentinel removed, kept+extended); NoEventHandler arm.
- S323 (dep:S322): UIElement::HitTestAt (deepest/topmost/reverse-z/clip/visibility) + InputMarshaller (input->routed; single-threaded, NO Dispatcher).
- S324 (dep:S323): ClickFlow gate (a click hit-tests a real element + raises a bubbling Click with the correct source; empty-space raises nothing; handled stops the climb).
- S325 (dep:S324): Focusable DP + FocusManager (pre-order tab-order, MoveFocusNext/Previous, RaiseKeyRouted bubbling KeyDown/KeyUp on the focused element).
- S328 (dep:S325): REFACTOR GATE(S8) — single-threaded, single-instance, ZERO new locks; deferred Dispatcher/Task pulled; S092 sentinel resolved; both CI lanes green.
- S329_1 (dep:S323): VIEWER-GROWTH INTERACTIVE — vaml_viewer/viewer_interactive.h (the S329_1 plan seam, ADR-021) + ViewerInteractivePlan_test.cpp: a click on a rendered element -> routed ClickEvent -> the markup handler (S322) -> the VM -> the live binding repaints. [VIEWER GROWTH: input works in the live viewer; single-threaded]
- S329 (dep:S328): SURVIVOR + checkpoint advance to S360.

## files_touched
- NEW src/core/include/vaml/core/Signal.h, RoutedEvent.h, InputMarshaller.h, FocusManager.h
- NEW src/core/src/RoutedEvent.cpp, InputMarshaller.cpp, FocusManager.cpp
- MOD src/core/include/vaml/core/UIElement.h (+routed-handler surface, HitTestAt, Focusable), src/core/src/UIElement.cpp (routed engine + hit-test + focusable)
- MOD src/core/include/vaml/core/VamlReader.h + src/core/src/VamlReader.cpp (S322 bind_class_handler + BindEvents + NoEventHandler)
- MOD src/core/CMakeLists.txt (Removed Dispatcher.cpp; added RoutedEvent/InputMarshaller/FocusManager)
- NEW src/core/include/vaml/mvvm/property_bindable.h (unchanged)
- NEW test/unit/RoutedEvent_test.cpp, VamlReader_event_test.cpp, S323_Input_test.cpp, S324_ClickFlow_test.cpp, S325_Focus_test.cpp, ViewerInteractivePlan_test.cpp + MOD test/unit/CMakeLists.txt (Removed S326/S327 test TUs)
- NEW vaml_viewer/viewer_interactive.h (S329_1 seam)
- DELETED (re-scope pull): src/core/src/Dispatcher.cpp, src/core/include/vaml/core/Dispatcher.h, src/core/include/vaml/core/Task.h, test/unit/S326_Dispatcher_test.cpp, test/unit/S327_AsyncBinding_test.cpp
- MOD docs/roadmap.md (Sprint 8 re-scoped single-threaded; S326/S327 DEFERRED -> NEW Sprint 11a header S450–S459 injected before Sprint 12), docs/adr/ADR-LOG.md (ADR-010 re-pointed to 11a), progress.md (Sprint 8 SURVIVOR appended + checkpoint -> S360), plan, session-context.md
- No NEW md beyond the operational files + the existing plan. No commits made.

## build_status
BOTH CI LANES GREEN from first principles this session (Sprint 9):
- windows-msvc: build/verify_s369.cmd (msvc-debug, MSVC-bundled CMake, warning-as-error CLEAN) + ctest 4/4 suites PASS exit-clean: vaml-core-tests 971/971 test cases (6618/6619 assertions; the single failed assertion is the standing intentional S036_1 Shouldly demonstration fixture, Status SUCCESS), vaml-tools-tests 89/89 incl. the S366 + S369 suites, vaml-vulkan + vaml-presentation green. Plus vaml-tools/presentation suites verified separately.
- A REAL process-exit heap-use-after-free (ObjectPool master_destroy_all vs the S9 process-lifetime app_resources static) was EXPOSED by msvc-asan and FIXED IN PLACE (ResourceDictionary::remove + test cleanup guards); ASan-verified ZERO UAF + ctest exit-clean after the fix.
- ubuntu-clang (Linux VM, /mnt/hgfs/VamlPOC): prior Sprint-8 state recorded green (clang 21.1.8, warnings==0, ctest 4/4). The windows lane was re-verified from first principles this session; the VM lane stands on the prior verified state per the standing convention.
- NOTE: Cygwin-cmake PATH contamination handled via build/vs_build_s8.cmd (keepable helper). windows-clang-debug is NOT a lane.
- The LIVE interactive viewer proof + the S369 live re-skin ([both apps, not CI], standing S222+ precedent) are the only display-backed items; the headless seams (ViewerRestylePlan + TemplateParity) are the CI proofs and are green.

## next_step
(Stopped at SPRINT 9 completion — S360..S369, styles/templates/resources.) Next = S400 (Sprint 10 — controls: real Button/Text/control materials + the implicit Button style, dep:S368). The FULL Sprint 9 survivor digest is in progress.md's SPRINT 9 SURVIVOR block. On resumption: begin Sprint 10 per its roadmap header. The must-run command is `implementit`.

## notes
- The S322 markup Click wiring operates on real rendered UIElements (panels) today; Button (a MINIMAL REAL DependencyObject seed, not UIElement) stays unresolved until S401 — recorded not silent. The interactive proof uses a real StackPanel.
- Sprint 9 shipped the full styling engine in core (ResourceDictionary/three-scope lookup + IconKey + Static/DynamicResource + Style/Setter/BasedOn/triggers + StyleApplier with local-wins + ControlTemplate/visual-states + SolidColorBrush + TemplateBinding + dual-path parity + live re-skin seam) — all REAL permanent Cat-1, NOT MINIMAL REAL.
- Build helper left in tree: build/vs_build_s8.cmd (MSVC cmake + vcvars64 wrapper) + build/verify_s369.cmd (the Sprint 9 closing CI-lane gate). Harmless, useful.
- No commits made (per the standing directive unless 'commitit' is commanded).
- No NEW md files beyond the operational files + the pre-existing plan (the "no guides/summaries/overviews" rule held — session-context.md, progress.md, roadmap.md, ADR-LOG.md only).
