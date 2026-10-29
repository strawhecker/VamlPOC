# Session Context — Sprint 11k-2 (the missing WPF controls the corpus consumes)

last_command: implementit (COMPLETE) — 11k-2 implemented + both CI lanes green
plan_digest: 11k-2 (dep: ScrollViewer S405 + the S401/DockPanel/TextBlock real
  re-base precedent) authors the five WPF controls the corpus consumes that the
  roadmap never authored:
    RepeatButton — the toolkit NumericUpDown/NumUpDown SPIN-button base
      (ButtonBase-faithful Click + Delay/Interval + IsPressed).
    ScrollBar    — the scrollbar chrome ScrollViewer S405 drives (RangeBase-
      faithful Value/Min/Max + Orientation + ViewportSize + the bubbling
      "Scroll" routed event).
    Viewbox      — the scale-contain single-child container (ColorPickerSwatch)
      (Decorator-faithful Stretch/StretchDirection + content child); classifies
      to NONE — "Viewbox -> its content" (the content child renders its own
      material).
    ToolBar      — the app's ToolbarControl basis (ItemsControl-faithful
      Orientation/Band/BandIndex/IsOverflowOpen/HasOverflowItems surface).
    ToolBarTray  — the ToolBar host (Orientation/IsLocked).
  Each is a REAL UIElement re-base registered as a built-in + classified
  (RepeatButton/ScrollBar/ToolBar/ToolBarTray -> Solid; Viewbox -> None, "its
  content"). Built-in count 36 -> 41. NO MINIMAL REAL introduced (all five are
  real re-based UIElements resolved at this step). WPF-faithful branch mapping:
  RepeatButton:ButtonBase / ScrollBar:RangeBase / ToolBar:ItemsControl /
  ToolBarTray:Panel / Viewbox:Decorator(FrameworkElement). The full
  ButtonBase/RangeBase/ItemsControl/Control abstract-base re-bind is 11k-11's
  S649 step (recorded NO-GAP not silent); the RepeatButton auto-repeat TIMER
  loop + the full scrollbar thumb/page chrome are the S649/11k-11 continuation
  (recorded NO-GAP not silent).

files_touched:
  core:
  - src/core/include/vaml/core/Element.h — NEW classes (before
    register_builtin_elements): ScrollBar / RepeatButton / Viewbox / ToolBar /
    ToolBarTray, each `final : public UIElement` with the WPF-faithful surface +
    MeasureOverride + single-content-slot (Viewbox). No ContentControl base
    (11k-11's S649).
  - src/core/src/Element.cpp — per-control Meyers-static ensure_/register_ DP
    functions + the ctors + the typed accessors + MeasureOverride/ArrangeOverride
    + the bubbling Scroll (ScrollBar) / Click (RepeatButton) raises +
    register_builtin_elements 36 -> 41 (+ScrollBar +RepeatButton +Viewbox
    +ToolBar +ToolBarTray). DP NAMES: the new non-colliding DPs are registered
    BARE (Value/Minimum/Maximum/ViewportSize/Delay/Interval/IsPressed/
    Band/BandIndex/IsOverflowOpen/HasOverflowItems/Stretch/StretchDirection/
    IsLocked) so corpus-shaped `<ToolBar Orientation=...>` etc. resolve (the
    reader resolves bare attribute keys by name). Orientation reuses the SHARED
    GLOBAL bare "Orientation" (StackPanel owner, S129 — NAME-UNIQUE registry;
    ToolBar/ToolBarTray self-set Horizontal in their ctors because the shared
    default is Vertical; ScrollBar keeps Vertical).
  - src/vulkan/src/ui/UiSceneEncoder.cpp — classify_element arms:
    RepeatButton/ScrollBar/ToolBar/ToolBarTray -> MaterialKind::Solid; Viewbox
    -> MaterialKind::None ("its content"). (Viewbox placed before the fallback.)
  - src/presentation/src/VamlRenderer.cpp — classify_merge_element arms: Viewbox
    -> MaterialKind::None (the explicit arm needed so the non-Text fallback does
    NOT emit a Solid box for the Viewbox); RepeatButton (spin slot) / ScrollBar
    (track) / ToolBar (strip) / ToolBarTray (host) -> Solid fills at final_rect.
  tests:
  - NEW test/unit/11k2_addedControls_test.cpp (presentation target) — each
    control is a REAL UIElement + registered built-in; DP round-trip; ScrollBar
    value-clamp + the bubbling "Scroll" routed event; RepeatButton Click + the
    DP round-trip; Viewbox measures its content child + classifies None; the
    ToolBar/ToolBarTray strip layout; the corpus-shaped doc authoring all five
    controls LOADS (no unknown-type/prop) + classifies (after
    finalize_visual_tree). Uses child_at(0) for the Viewbox content child (the
    reader finalizes declared children into the S121 visual list; Viewbox's
    content_ pointer is the programmatic slot).
  - test/unit/CMakeLists.txt — +11k2_addedControls_test.cpp (presentation
    target).
  - test/unit/ElementFactoryRegistry_test.cpp — is_registered + assertConcrete
    for the five (using RepeatButton/ScrollBar/ToolBar/ToolBarTray) + count
    36 -> 41 (both the startup gate + the idempotence case).
  - test/unit/BrushAuthoring_s460_test.cpp, test/unit/Milestone407_test.cpp —
    the built-in count gate 36 -> 41 (the 11k-2 additions).
  infra:
  - build/verify_s11k2.cmd (consolidated two-lane gate; verify_s11k1.cmd shape).
  - session-context.md (this file).

build_status: 11k-2 DONE — BOTH CI LANES green:
  windows-msvc (msvc-debug) — build warning-as-error CLEAN + ctest 4/4 100%
    (Start 1 vaml-core concurrency? all 4 suite names pass 100%).
  ubuntu-clang (clang-debug, /mnt/hgfs/VamlPOC) — -Werror CLEAN + ctest 4/4 100%.
  Official gate verify_s11k2.cmd ends "S11K2 BOTH CI LANES OK". No warnings.
  (windows-clang-debug is NOT a CI lane — not built/tested/flagged.)

next_step: 11k-2 is complete. The roadmap sprint block for 11k-2 is authored;
  next implement 11k-11 (GENERIC CONTENT-OWNER ROUTING + the S649
  ContentControl-base / control-material RE-BIND — the new in-11k step that owns
  the general content→DP routing + mixed/interleaved ordering + the S085
  conscience note; ALSO the declarative single-content routing for Viewbox/
  Window/Border). After 11k, Sprint 12± stabilizes/documents/performs (adds NO
  features). commitit outstanding (driver's seat is yours).

notes: 11k-2 delivered the FIVE missing corpus controls the roadmap never
  authored. DP registrations are BARE (reader resolves bare attribute keys), and
  Orientation rides the single SHARED bare "Orientation" (StackPanel owner) —
  per the 11j-5 shared-control-base precedent, since the global DP registry is
  NAME-UNIQUE (a second owner could not re-register it; the S043/S131 WrapPanel
  precedent). The auto-repeat TIMER loop for RepeatButton + the full
  scrollbar thumb/page-up-down chrome + the ContentControl/ButtonBase/RangeBase/
  ItemsControl abstract bases are recorded NO-GAP (resolved at 11k-11's S649
  re-bind), never silent. ZERO MINIMAL REAL introduced. No commits (standing
  directive — commitit outstanding).
