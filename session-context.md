# Session Context — Sprint 11k-1 (TextBlock: retire the invented `Text`)

last_command: implementit (COMPLETE) — 11k-1 implemented + both CI lanes green
plan_digest: 11k-1 (dep:S400) = the WPF-faithful CORRECTION. `Text` is NOT a WPF
  control (WPF has TextBlock/TextBox). 11k-1 authors the REAL `TextBlock`
  (TextBlock : FrameworkElement; in VAML the framework-element branch IS
  UIElement, so TextBlock : UIElement is the faithful mapping) and RETIRES the
  S087-invented `Text` factory name (built-in count NET-NEUTRAL +TextBlock −Text
  = 36). The S400 semantic content (TextLayout wrap engine, TextGlyphPlan,
  MaterialKind::Text) carries over unchanged under the correct name. DP re-owner:
  Text/FontWeight/Foreground now owner "TextBlock"; FontSize stays the bare
  shared Control DP (11j-5), unchanged (TextBlock's own font/padding DP-owner
  split is 11k-8's named scope, not pre-empted). The loader accepts `<TextBlock>`
  and classifies it MaterialKind::Text. NEW inline-content routing: the reader
  now routes `<TextBlock>label</TextBlock>` inline content onto the Text DP
  (WPF IAddChild behavior — the corpus's dominant usage) — SCOPED to TextBlock;
  the GENERAL content→DP + mixed/interleaved ordering resolves at the NEW
  in-11k step 11k-11 (roadmap.md already authored; alpha feature end-of-line,
  NOT Sprint 12+). S167_1 corrective convention: the S400 `Text` history is
  preserved (never edited to hide the error), SUPERSEDED BY 11k-1. TextBox (S402)
  is already WPF-true AND a genuine Control — untouched.

files_touched:
  core:
  - src/core/include/vaml/core/Element.h — `class Text` -> `class TextBlock final
    : public UIElement` (WPF-faithful doc block + [MINIMAL REAL] TextBlock
    — RESOLVED BY 11k-1 sentinel); class body surface unchanged.
  - src/core/src/Element.cpp — register_text_properties ->
    register_textblock_properties (owner "Text" -> "TextBlock"); accessors +
    MeasureOverride Text:: -> TextBlock::; ListBox ItemTemplate/render_items
    Text -> TextBlock; register_builtin_elements "Text" -> "TextBlock"
    (count stays 36, NET-NEUTRAL).
  - src/core/src/VamlReader.cpp — NEW TextBlock inline-content routing: after
    attributes, a resolved TextBlock's trimmed element.content is convertLiteral'd
    and Set on the "Text" DP (11k-1; general routing = 11k-11).
  - src/vulkan/src/ui/UiSceneEncoder.cpp — classify_element dynamic_cast:
    vaml::Text -> vaml::TextBlock (MaterialKind::Text unchanged).
  - src/presentation/src/VamlRenderer.cpp — classify_merge_element dynamic_cast
    vaml::Text -> vaml::TextBlock.
  - src/tools/src/parity/DualPathParity.cpp — dynamic_cast vaml::Text ->
    vaml::TextBlock (returns "TextBlock").
  viewer/infra:
  - vaml_viewer/MainView.vaml, vaml_viewer/sample.vaml,
    vaml_viewer/vaml_viewer/sample.vaml — <Text .../> -> <TextBlock .../>.
  - vaml_viewer/app_shell.h — inline kAppShellMainViewVaml <Text> -> <TextBlock>
    (kept byte-identical to the on-disk MainView.vaml for the S444 parity test).
  tests (re-pointed consumers + the new proof):
  - NEW test/unit/TextBlock_s11k1_test.cpp (presentation target) — corpus-shaped
    `<TextBlock FontSize="24">label</TextBlock>` LOADS (no unknown-type throw),
    inline content lands on the Text DP, FontSize round-trips, Measure wraps,
    classify_merge_element -> MaterialKind::Text.
  - test/unit/CMakeLists.txt — +TextBlock_s11k1_test.cpp (presentation target).
  - re-pointed Text -> TextBlock: Text_test.cpp, TextGlyphPlan_test.cpp,
    ViewerTextMaterial_test.cpp, ElementFactoryRegistry_test.cpp (is_registered
    TextBlock true + Text FALSE, count 36), ElementResolver_test.cpp,
    ControlDp_s11j5_test.cpp, Milestone407_test.cpp (TextBlock registered/Text
    not), GoldenDocs_test.cpp, ListBoxListView_test.cpp, MARKupStaticType_s461,
    VamlReader_defer/s093/s094/test, VBamlReader_s285/s286/s288_parity,
    ViewerLoadFile_test, ViewerControlsPlan_s409_1, ObjectPool_test,
    Border_test, PasswordGroupStatus_test, AppShell* (named_as<TextBlock> /
    dynamic_cast<TextBlock*> + authored <TextBlock>), PropertyElement_s452/
    PropertyElementS7_s452, TemplateParity_test, Vamlc_s280_1 (binding hop Text.->
    TextBlock.), Vamlc_s283/s284 (typeToken "Text"}, "TextBlock"),
    ViewerGrowth_s475/s486.
  infra: build/verify_s11k1.cmd (consolidated two-lane gate)
  roadmap (prior turn, roadmap.md ONLY): added the 11k-11 step (GENERIC
    CONTENT-OWNER ROUTING + S649 ContentControl-base / control-material RE-BIND)
    + re-pointed the S649 forward refs (11j-10, 11k-10, 11j-1, ground-truth) into
    11k-11; the S085 mixed-content conscience note is decided/resolved in 11k.

build_status: 11k-1 DONE — BOTH CI LANES green:
  windows-msvc (msvc-debug) — build warning-as-error CLEAN + ctest 4/4 100%
  ubuntu-clang (clang-debug, /mnt/hgfs/VamlPOC) — -Werror CLEAN + ctest 4/4 100%
  Official gate verify_s11k1.cmd ends "S11K1 BOTH CI LANES OK". No warnings.
  (windows-clang-debug is NOT a CI lane — not built/tested/flagged.)

next_step: mark 11k-1 DONE + SUPERSEDED-BY note on the S400 `Text` history in the
  roadmap sprint block; then implement 11k-11 (GENERIC CONTENT-OWNER ROUTING +
  the S649 ContentControl-base / control-material RE-BIND — the new in-11k step
  that owns the general content→DP routing + mixed/interleaved ordering + the
  S085 conscience note). After 11k, Sprint 12± stabilizes/documents/performances
  (adds NO features). commitit outstanding (driver's seat is yours).

notes: 11k-1 delivered the corpus dump's #1 blocker (`unknown element type
  '<TextBlock>'`) via the WPF-true TextBlock (NOT a stub — the S400 content is
  kept-and-extended under the correct name). DP re-owner "TextBlock" is
  declaration-label only (S043 resolves by bare name globally), so no behavior
  change: the Text/FontWeight/Foreground DPs still resolve by name on any DO.
  BrushId has NO literal converter (a `Foreground="..."` attribute is not
  literal-authorable — that surface is the S364 brush-resource path), so the
  corpus test proves Foreground via the programmatic brush round-trip and keeps
  the authored leaf to the authorable FontSize + inline content. ZERO MINIMAL
  REAL beyond the declared, resolution-bound TextBlock marker (11k-1). The
  general content-routing deferral is NO-GAP LOCKED to 11k-11 (not Sprint 12+).
  No commits (standing directive — commitit outstanding).

