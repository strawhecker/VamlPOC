# session-context.md

last_command: implementit (11k-A — STICKYNOTE + CAF ANNOTATION ENGINE, S-kA-1..S-kA-5; self-contained dependency-group closure)
plan_digest: 11k-A (dep:11n-6 adorner + 11k_12 RichTextBox/FlowDocument + S554 InkCanvas) — make the WPF
PresentationFramework\MS\Internal\Controls\StickyNote\ annotation component REAL in VAML, correctly understood
as an ANNOTATION-ANCHOR component (not an authored control). **S-kA-1** CAF ENGINE (Annotation / AnnotationResource
MetaData|TextData|InkData / AnnotationService AddAttached|RemoveAttached|ModifyAttached + the reentrancy guard +
duplicate-refused / IAttachedAnnotation / StickyNoteTokens — the WPF SNCAnnotation XmlToken set); **S-kA-2**
StickyNote : UIElement,IAttachedAnnotation (IsExpanded/Author READ-ONLY/StickyNoteType {Text,Ink}/note Width-Height/
ZOrder/PositionTransform S208 + the SNC<->annotation bridge; built-in factory #52, count 108->109); **S-kA-3**
StickyNoteContentControl abstract base + StickyNoteRichTextBox (Text) + StickyNoteInkCanvas (Ink) behind
Save/Load/Clear/IsEmpty/Type + MaxBufferSize; **S-kA-4** StickyNoteAdorner : Adorner (GetDesiredTransform composes
the PositionTransform) + AdornerHitTest RESOLVES the 11n-6 forward refinement + StickyKeepOnPage (WPF GetOffsets);
**S-kA-5** viewer-growth seam viewer_growth_s11ka.h (CAF-store + anchored-preview + render-plan, headless Rail 1;
live window = [both-apps, not CI] gate, in-viewer editing out — preview-only). Both CI lanes green per step from first
principles (build/verify_s11ka.cmd msvc-debug + ubuntu-clang clang-debug on /mnt/hgfs/VamlPOC). Roadmap marked [DONE].
ZERO MINIMAL REAL (grep-audit) + ZERO new locks (ADR-018) + ADR-011 held.
**CHECKPOINT: last completed = 11k-A, next = S11h/S520 (per the standing 11n-7 chain; 11k-A is a self-contained closure).**

## files_touched — 11k-A (this incarnation)
- src/core/include/vaml/core/StickyNote.h (NEW) + src/core/src/StickyNote.cpp (NEW): the CAF ANNOTATION ENGINE
  (Annotation/AnnotationResource/AnnotationService/IAttachedAnnotation/StickyNoteTokens) + the StickyNote control
  (the WPF StickyNoteControl DP surface + the SNC<->annotation bridge + the framework-element MeasureOverride).
  S-kA-1/2.
- src/core/include/vaml/core/StickyNoteContentControl.h (NEW) + src/core/src/StickyNoteContentControl.cpp (NEW):
  the content-host abstraction (StickyNoteContentControl abstract base + StickyNoteRichTextBox (Text) +
  StickyNoteInkCanvas (Ink) behind Save/Load/Clear/IsEmpty/Type + MaxBufferSize). S-kA-3.
- src/core/include/vaml/core/StickyNoteAdorner.h (NEW) + src/core/src/StickyNoteAdorner.cpp (NEW): the S-kA-4
  adorner integration (StickyNoteAdorner : Adorner + GetDesiredTransform + AdornerHitTest RESOLVES the 11n-6
  forward refinement + StickyKeepOnPage page-boundary clamp).
- src/core/include/vaml/core/Element.h (MOD): + #include StickyNote.h.
- src/core/src/Element.cpp (MOD): register_element("StickyNote", ...) — built-in #52; count 108->109.
- src/core/CMakeLists.txt (MOD): + StickyNote.cpp / StickyNoteContentControl.cpp / StickyNoteAdorner.cpp.
- vaml_viewer/viewer_growth_s11ka.h (NEW): the ADR-021 viewer-growth seam (run_viewer_growth_s11ka_plan +
  viewer_growth_s11ka_render_plan). S-kA-5.
- test/unit/CMakeLists.txt (MOD): + StickyNote_s11ka_test.cpp (core-tests) + ViewerGrowthS11ka_test.cpp
  (presentation-tests).
- test/unit/StickyNote_s11ka_test.cpp (NEW): 9 TEST_CASEs / 82 assertions (CAF engine + token authority +
  service add/remove/modify + reentrancy + duplicate-refused; DP surface + SNC bridge; content-host Text/Ink +
  MaxBufferSize; GetDesiredTransform + AdornerHitTest + StickyKeepOnPage).
- test/unit/ViewerGrowthS11ka_test.cpp (NEW): 2 TEST_CASEs / 12 assertions (CAF-store + anchored preview +
  render-plan).
- test/unit/ElementFactoryRegistry_test.cpp / BrushAuthoring_s460_test.cpp / Milestone407_test.cpp (MOD):
  built-in element count 108->109.
- build/verify_s11ka.cmd (NEW): the two-lane CI gate (windows-msvc + ubuntu-clang) + the MINIMAL REAL grep-audit.
- docs/roadmap.md (MOD): 11k-A marked [DONE] with its full digest + the return note (honest WHY).
- progress.md (MOD): the new 11k-A ACTIVE-CURRENT checkpoint line + the `SPRINT 11k-A — SURVIVOR` block.
- session-context.md (MOD): this entry.

## build_status (11k-A — re-verified from first principles)
- windows-msvc: build/verify_s11ka.cmd -> BUILD OK (warning-as-error clean) + ctest 100% pass + the 11k-A
  tests run green (9+2 cases, 94 assertions) + the MINIMAL REAL grep-audit PASS (zero [MINIMAL REAL]).
- ubuntu-clang: /mnt/hgfs/VamlPOC clang-debug -> build clean (warnings==0) + ctest 100% pass (UBUNTU-GREEN).
- 11k-A: ALL GREEN on BOTH CI lanes, verified from first principles (verify_s11ka.cmd on windows;
  clang-debug on the Linux VM).

## next_step — 11k-A CLOSED / next sprint
- **11k-A CLOSED (S-kA-1..S-kA-5 all DONE; the WPF StickyNote annotation component delivered, both CI lanes green).**
  Next per the roadmap chain = **S11h (S520 IMPORT/INJECT seam)** / Sprint 11q (S620) / S540+ (Sprint 11z) — the
  standing 11n-7 checkpoint. 11k-A is a self-contained dependency-group closure, not the chain's next step.

## notes
- 11k-A authored MINIMAL REAL-free, real permanent surface (grep-audit in verify_s11ka.cmd): the CAF annotation
  engine + the StickyNote control + the content-host abstraction + the adorner integration + the viewer-growth seam.
  CONFORMANCE-PINNED to the three real WPF StickyNote sources (StickyNoteAnnotations.cs + StickyNoteContentControl.cs
  + StickyNoteHelper.cs, read at plan time — S11f/11k rule). The StickyNote is honestly NOT an authored control: it is
  an ANNOTATION-ANCHOR component (CAF store + adorner-anchored overlay), so it earns the CAF engine + a preview
  (headless Rail 1), with the note chrome composed over the existing adorner/ink/flow-doc/editor controls.
- ZERO new locks (ADR-018): the SNC<->annotation reentrancy guard is a bool RAII (the WPF LockHelper Flag), never an
  OS lock. ADR-011 held (all new core surface is DATA + measure; the note render itself is the 11n-6 adorner second-pass,
  not a new classifier arm). The 11n-6 AdornerHitTest forward refinement is RESOLVED at S-kA-4 (its own step, NO-GAP).
- NO silent backfill (NO-GAP): the S-kA-5 LIVE window display of the anchored StickyNote preview + in-viewer note
  editing stay recorded ([both-apps, not CI] + preview-only), never silent; the base64-XAML/ISF round-trip arm is
  recorded as the bounded S-kA-3 content-serialization surface, never an invented Xml parser.
- No commit made (standing directive — commitit outstanding; remote build green with the 11k-A change set uncommitted,
  the driver's seat is yours).
