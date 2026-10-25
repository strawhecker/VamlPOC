# VAML session-context (per-run state)

## last_command
implementit COMPLETE — SPRINT 10 CLOSED. The full S400–S409_1 control set is delivered, each token verified GREEN on BOTH CI lanes (windows-msvc msvc-debug + ubuntu-clang clang-debug) from first principles (build/verify_s400..s402.cmd + /mnt/hgfs/VamlPOC clang-debug). The user requested the one-by-one run through S409_1; all S10 tokens are now complete.

## plan_digest
Sprint 10 (Core controls) — the S087 control seeds RESOLVED at their own steps (NO-GAP, MINIMAL REAL/NO-LAZY-LLM honored):
- S400 Text RESOLVED: vaml::core TextLayout (pure CPU text-layout + wrap: text_layout_plan / default_text_metric / resolve_foreground_color — Foreground = BrushId the WPF-faithful brush read) + the real re-based Text (Text/FontSize/FontWeight/Foreground DPs, MeasureOverride wrapped extent) + the vulkan TextGlyphPlan render seam + the Text material in the classifiers.
- S400_1 Icon (per ADR-015) DONE: vaml::core Icon (Source/IconKey/TintMode{Preserve|Monochrome}/Foreground/Width/Height/Stretch) + the vulkan IconGlyphPlan (S213_1 best-fit quad + the DELIBERATE fallback glyph + a STRUCTURED diagnostic through the S250_1 channel). Built-in 12th.
- S401 Button RESOLVED: re-based onto UIElement (IsEnabled DP gating Click; the Click routed event; the template/visual-state surface Normal/Hover/Pressed/Disabled; measure) + the Solid material honoring the visual-state background + the IsEnabled fade.
- S402 TextBox RESOLVED: re-based (Text/AcceptsReturn DPs + caret/selection; key-driven editing via handle_key; the S244 TWO-WAY binding on edit; the multi-line S400 wrap Measure) + the Solid edit-field box material.
- S403 Slider RESOLVED: Value/Min/Max DPs (Value clamps) + value-changed Signal + routed ValueChanged + thumb-drag set_value_from_ratio.
- S404 CheckBox RESOLVED: nullable IsChecked (WPF-faithful third state) + IsThreeState + the WPF on-toggle cycle.
- S405 ScrollViewer RESOLVED: offset clamps to scrollable + S124 clipping + wheel scroll + ScrollChanged.
- S406 Focus RESOLVED/EXTENDED: the S325 FocusManager gains a default-Button (Enter-activates-default, WPF-faithful) + the routed key completeness.
- S407 Milestone: every S10 control is a real UIElement re-base + registered concrete + tested (no DependencyObject-only seeds).
- S408 REFACTOR GATE(S10): both lanes green + grep-audit (no stubs/Minimal-Real in the set) + the Sprint-10 SURVIVOR + checkpoint advance.
- S409_1 VIEWER-GROWTH: the viewer hosts the REAL S10 control set render + interactive via the generic renderer (classify_merge_element + classify_element emit a REAL material for each control); the dedicated sample.vaml exercises every control.
- Tests added across the three suites: Text_test/TextGlyphPlan_test/ViewerTextMaterial_test/IconGlyphPlan_test/ViewerIconMaterial_test/Button_test/ViewerButtonMaterial_test/TextBox_test/ViewerTextBoxMaterial_test/Controls_test/ViewerControlsMaterial_test/Focus406_test/Milestone407_test/ViewerControlsPlan_s409_1_test. WPF fidelity confirmed from E:\GitRepos\wpf (TextBlock.Foreground=Brush -> BrushId; CheckBox nullable bool? + IsThreeState).

## files_touched
- MOD src/core/include/vaml/core/Element.h (Text/Button/TextBox/Slider/CheckBox/ScrollViewer re-bases + Icon + the control DPs + KeyCode fwd decl)
- MOD src/core/src/Element.cpp (the real control implementations: TextLayout-based Text, Button, TextBox, Slider, CheckBox, ScrollViewer + registrations)
- NEW src/core/include/vaml/core/TextLayout.h + src/core/src/TextLayout.cpp; MOD src/core/CMakeLists.txt
- MOD src/core/include/vaml/core/FocusManager.h + src/core/src/FocusManager.cpp (default-Button/Enter-activation; KeyCode additions)
- NEW src/vulkan/include/vaml/vulkan/ui/TextGlyphPlan.{h,cpp}; NEW src/vulkan/include/vaml/vulkan/icon/IconGlyphPlan.{h,cpp}; MOD src/vulkan/CMakeLists.txt
- MOD src/vulkan/src/ui/UiSceneEncoder.cpp (classify_element -> Text/Icon/Button/TextBox/Slider/CheckBox/ScrollViewer)
- MOD src/presentation/src/VamlRenderer.cpp (classify_merge_element -> the real control materials)
- NEW test/unit/{Text,Button,TextBox,Controls,Focus406,Milestone407,IconGlyphPlan,TextGlyphPlan,ViewerTextMaterial,ViewerIconMaterial,ViewerButtonMaterial,ViewerTextBoxMaterial,ViewerControlsMaterial,ViewerControlsPlan_s409_1}_test.cpp + MOD test/unit/CMakeLists.txt
- MOD test/unit/ElementFactoryRegistry_test.cpp (Icon = 12th built-in; count 11->12)
- NEW build/verify_s400.cmd, verify_s400_1.cmd, verify_s401.cmd, verify_s402.cmd
- NEW vaml_viewer/vaml_viewer/sample.vaml (S409_1 dedicated sample)
- MOD docs/roadmap.md + progress.md (S400-S409_1 [DONE] + the Sprint-10 SURVIVOR + checkpoint advance to S440). No commits made.

## build_status
BOTH CI LANES GREEN from first principles at EVERY S10 token (S400..S409_1):
- windows-msvc: build/verify_s400.cmd / s400_1 / s401 / s402 — each msvc-debug warning-as-error CLEAN + ctest 4/4 PASS (vaml-core/vaml-tools/vaml-vulkan/vaml-presentation).
- ubuntu-clang: /mnt/hgfs/VamlPOC clang-debug — build warnings==0 + ctest 4/4 PASS at each step.
- windows-clang-debug is NOT a lane (not built/tested/flagged).

## next_step
Sprint 11 (S440+): the vaml_viewer app-shell (MainView.vaml with Grid+StackPanel+Button+Text+TextBox+CheckBox+Slider+Icon+ScrollViewer through the generic renderer). The S409_1 viewer-growth token has already landed the live viewer hosting the S10 controls, so S440 completes the authored app-shell on top of it. On resumption: `implementit S440`.

## notes
- WPF source-fidelity (confirmed from E:\GitRepos\wpf at plan review): TextBlock.Foreground = Brush (BrushId); CheckBox nullable bool? + IsThreeState.
- NO-GAP recorded, not silent: the FULL multi-glyph Text render through the REAL SDF atlas + the live icon-atlas draw are the [both-apps/live] + S409_1/S440 viewer path — the S400-405 classifiers deliver the real per-control materials headlessly; the live high-DPI/atlas draw is the display-backed continuation.
- Each S10 token keeps MINIMAL REAL/NO-GAP: every control seed resolved at its OWN step; no silent backfills.
- No commits made (per the standing directive unless 'commitit' is commanded).
