# VAML session-context (per-run state)

## last_command
implementit — S252 -> S253 -> S254 -> S255 -> S256 (Sprint 6 close: markup {Binding} → Command → Refactor Gate → SURVIVOR/checkpoint → viewer growth). COMPLETED + VERIFIED from first principles BOTH CI lanes (windows-msvc + ubuntu-clang) + ASan/LSan zero-leak (S254).
- S252 MARKUP {Binding} RESOLVED IN VamlReader (the S2 tag bound): NEW src/core/include/vaml/core/BindingMarkup.h + src/core/src/BindingMarkup.cpp (parse_markup_binding -> BindingMarkup { kind, path, mode, hasMode, malformed }; MarkupExtensionKind { None, Binding, Command } — parses {Binding Title} / {Binding Title, Mode=TwoWay} / {Command DoIt}; kind None for StaticResource/x:Null stays deferred S361; malformed sets malformed flag) + MOD src/core/include/vaml/core/VamlReader.h + src/core/src/VamlReader.cpp (VamlReaderError::MalformedBindingMarkup arm; VamlReader::set_data_context(ObservableObject*) + BindExtensions() — the S2-tag resolution building a live BindingExpression per {Binding} row against the data context + target DP, applying the S242 first transfer, reader-owned live_bindings() cleared per Load; policy from view_binding_policy_default, diagnostics through the S250_1 channel; malformed -> throws VamlReaderException(MalformedBindingMarkup)) + MOD src/core/CMakeLists.txt + NEW test/unit/BindingMarkup_test.cpp (15) + NEW test/unit/VamlReader_binding_test.cpp (9 incl. S253) + NEW build/verify_s252.cmd.
- S253 COMMAND via {Command}: MOD VamlReader.{h,cpp} — register_command(name, ICommand*) (app-owned, non-owning, idempotent, NOT per-Load cleared) + ResolveCommand(name)->ICommand* (the S246 command surface); {Command} left deferred by BindExtensions then resolved; one/two-way markup end-to-end proven via VamlReader_binding_test.
- S254 REFACTOR GATE (Sprint 6): full MVVM in v1 (all modes + converters + Fallback/Strict + diagnostics, no stubs); grep-audit clean; ASan clean on msvc-asan (871/871) + ASan+LSan zero-leak on clang-asan.
- S255 SURVIVOR + checkpoint advance to S280 (docs-only; this file + roadmap).
- S256 VIEWER-GROWTH: NEW vaml_viewer/viewer_binding.h (ViewerDemoService + ViewerDemoViewModel (ObservableObject + PROPERTY_BINDABLE(double, Dim), plain ctor, NO DI) + viewer_bind_render_action + run_viewer_binding_plan (retained heap reader so the S243 live wire stays armed)) + MOD vaml_viewer/app.{h,cpp} (bind_and_render on the S222_1 reload path) + NEW test/unit/ViewerBindingPlan_test.cpp (5) + MOD test/unit/CMakeLists.txt. Visible proof: <StackPanel Opacity="{Binding Dim}"/> binds the VM Dim onto the Opacity DP (S208 alpha fold) — the honest S400-absent binding demo.
New + modified: BindingMarkup.{h,cpp}, VamlReader.{h,cpp}, CMakeLists (core+test), BindingMarkup_test.cpp, VamlReader_binding_test.cpp, ViewerBindingPlan_test.cpp, viewer_binding.h, app.{h,cpp}, verify_s252.cmd, roadmap, session-context. Checkpoint advanced: last=S255, next=S280.

## plan_digest
S252->S256 (Sprint 6, markup binding end-to-end + viewer growth, dep:S251). S252: parse_markup_binding (BindingMarkup) + VamlReader::set_data_context + BindExtensions -> the S092 deferred {Binding} becomes a live binding (first transfer + live one-way; Mode=TwoWay arms write-back; malformed -> MalformedBindingMarkup throw; kind None stays deferred). S253: {Command} left deferred, resolved via register_command + ResolveCommand(ICommand), one/two-way markup end-to-end. S254: Refactor Gate — all modes + converters + policy + diagnostics, no stubs; ASan/LSan zero-leak. S255: SURVIVOR + checkpoint to S280. S256: viewer reload populates a VM (plain ctor, NO DI) + renders {Binding} via viewer_binding.h + bind_and_render (the visible dim). Both CI lanes green from first principles.

## files_touched
- ADDED src/core/include/vaml/core/BindingMarkup.h + src/core/src/BindingMarkup.cpp (S252 markup parser; REAL permanent Cat-1)
- MOD src/core/include/vaml/core/VamlReader.h (MalformedBindingMarkup arm; set_data_context/data_context/BindExtensions/live_bindings/register_command/ResolveCommand; members dataContext_/live_bindings_/commands_)
- MOD src/core/src/VamlReader.cpp (BindExtensions impl; Load clears live_bindings_; register_command/ResolveCommand impl)
- MOD src/core/CMakeLists.txt (+BindingMarkup.cpp)
- ADDED test/unit/BindingMarkup_test.cpp (15 TEST_CASEs)
- ADDED test/unit/VamlReader_binding_test.cpp (9 TEST_CASEs incl. S253)
- ADDED test/unit/ViewerBindingPlan_test.cpp (5 TEST_CASEs, S256)
- MOD test/unit/CMakeLists.txt (+the 3 new suites; S252/S256 comment blocks)
- ADDED build/verify_s252.cmd (S252..S253 CI-lane verification, msvc-debug)
- ADDED vaml_viewer/viewer_binding.h (S256 seam; header-only ADR-021 consumer path)
- MOD vaml_viewer/app.h + vaml_viewer/app.cpp (bind_and_render; S256 integration on the S222_1 reload path; read_file_text helper)
- MOD docs/roadmap.md (S252/S253/S254/S255/S256 marked [DONE] with digests)
- MOD session-context.md (this file)
- No NEW md files (operational files only, per protocol scope).

## build_status
GREEN both CI lanes from first principles (S252..S256 gate):
- windows-msvc: build/verify_s252.cmd (msvc-debug) — build warning-as-error (/WX) CLEAN + ctest 3/3 PASS. vaml-core-tests 876/876 (incl. BindingMarkup 15 + VamlReader_binding 9 + ViewerBindingPlan 5, all prior green) + vaml-vulkan-tests 211/211 + vaml-presentation-tests 39/39, 0 failed. CI note: full ASan build required --parallel 2 (environment memory pressure — MSVC C1060 under parallel ASan; the non-ASan lane unaffected).
- ubuntu-clang: /mnt/hgfs/VamlPOC clang-debug (shared folder re-mounted via `sudo vmhgfs-fuse .host:/ /mnt/hgfs`) — configure clean + build warnings==0 + ctest 3/3 PASS (100% passed, 0 failed out of 3).
- S254 ASan/LSan: msvc-asan vaml-core-tests 871/871 ASan-clean + clang-asan 100% passed zero-leak (detect_leaks=1).
windows-clang-debug is NOT a lane (pre-existing failure; not built/tested).

## next_step
S280 (dep:S255; Sprint 7: UI/viewer + host rendering + app-shell) — per the S255 checkpoint advance. The Sprint 6 binding chain is complete + the viewer-growth obligation (S256) is delivered.

## notes
- S252 is the S2-tag resolution point (VamlReader_binding_test proves the S092-recorded {Binding} becomes a LIVE binding: first value lands on the target DP + a live one-way re-push + Mode=TwoWay write-back end-to-end through markup).
- The reader data context is the S250/S251 app-constructed VM (NO DI); BindExtensions uses view_binding_policy_default (S250_1 Fallback) + the S250_1 diagnostics channel; ADR-018 zero locks.
- S253 command registry is app-owned + non-owning; NOT per-Load cleared (survives the tree it is authored on, like register_event) — resolved via ResolveCommand (the S246 ICommand surface).
- S256 reuses the REAL S120 Opacity DP as the visible binding target (the S208 encoder folds effective opacity into alpha) — the honest "you SEE data binding" proof absent real control materials (Text/Button -> S400/S401, recorded NO-GAP); the LIVE window proof remains [both apps, not CI] on this box + feeds S443.
- Environment note: the ubuntu-clang shared folder (/mnt/hgfs) came un-mounted between S252 and S254 on the VM and was restored via `sudo vmhgfs-fuse .host:/ /mnt/hgfs -o allow_other -o uid=1000,gid=1000` (vmhgfs kernel module unavailable; FUSE path works).
- NO MINIMAL REAL introduced/resolved (BindingMarkup + reader binding + command seam + viewer VM/plan are all REAL permanent Cat-1, forward-consumed by S400/S401/S443).
- No commits made (per standing directive unless 'commitit' is commanded).
- No NEW md files created.
