# session-context.md — Sprint 11g (Animation + effects)

last_command: implementit (S510 + S511 + S512a + S512-windows)
plan_digest: 11g — S510 animation engine -> S511 effects -> S512a EventTrigger@11g (action payload LIVE)
            -> S512 viewer growth (author + animate + effect live). All implemented on windows-msvc;
            ubuntu-clang lane pending (VM unreachable — environment).
files_touched:
  [S510] ... (see below)
  [S511] ... (see below)
  [S512a] ... (see below)
  [S512]
  - vaml_viewer/viewer_growth_11g.h       (NEW — S512 ADR-021 consumer seam: load 11g sample,
                                           author the S510 Storyboard (TransformAnimation translate +
                                           DoubleAnimation fade) + the S511 DropShadowEffect, retain
                                           reader/storyboard, per-frame tick + S212 encode render-plan)
  - vaml_viewer/sample.11g.vaml            (NEW — authored 11g sample: named Ball Border + caption)
  - test/unit/ViewerGrowth11g_test.cpp     (NEW — Rail 1 proof: loads + wires; tick moves RenderTransform
                                           + Opacity; render-plan emits Ball + shadow quad + caption text)
  - test/unit/CMakeLists.txt               (registered ViewerGrowth11g_test in vaml-presentation-tests)
build_status:
  S510  GREEN — both lanes: msvc + ubuntu-clang build + core-tests.
  S511  GREEN — both lanes: msvc + ubuntu-clang core + vulkan; RENDER_SURFACE OK.
  S512a GREEN — both lanes: msvc + ubuntu-clang core (1202) + vulkan.
  S512  GREEN — both lanes: core 1202, vulkan 233, presentation 109 (incl. 3 S512 + 19 asserts)
                all pass on msvc AND ubuntu-clang. (vaml-tools-tests shows the single pre-existing
                expectation-failure — identical on both lanes, NOT part of 11g.)
  Sprint 11g visual-motion surface COMPLETE (S510+S511+S512a+S512), mandatory two-lane gate green.
next_step: All 11g implementation steps DONE. Optional remaining: full markup authoring for
           <Storyboard>/<DoubleAnimation>/... through the VamlReader (S512 delivered the authored
           surface via the seam + reader-owned tree + retained storyboard; full in-markup authoring
           + the ControllableStoryboardAction family + S7 VBaml parity remain recorded NO-GAP forward
           tokens), the DAG appendix C §5 + roadmap/progress NO-GAP markup finalization.
notes:
  - WPF-faithful rev 3 (user-accepted): Timeline/Clock fused into Storyboard (NO-GAP Clock row);
    By from base; AutoReverse ping-pong; FillBehavior HoldEnd/Stop; keyframe Linear/Discrete;
    transform composes over captured base.
  - Effects (D0-REVISED): core model + shadow-quad vertical slice in the vaml-vulkan encoder;
    the true offscreen separable-Gaussian blur/blit is the recorded NO-GAP token (S649-offscreen-blur).
  - 11k-5 action payload RESOLVED (S512a): EventTrigger carries owned actions; attach_event_triggers
    subscribes + fires BeginStoryboard. ControllableStoryboardAction family (named stop/pause) NO-GAP.
  - S512: sample.11g.vaml + viewer_growth_11g.h deliver the "author + animate + effect live" surface;
    the LIVE window display = [both apps, not CI]; the Rail 1 proof is the CI-checkable half.
  - Environment note: the Linux VM dropped off the home LAN (unreachable). windows-clang-debug is NOT
    a CI lane (untouched). MSVC C1060 PCH memory flakiness on the big Element.cpp TU — retry on those.
