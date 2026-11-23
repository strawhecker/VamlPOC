# Session Context — S835 (dep:S834) ARRANGE-ENTRY FIDELITY

last_command: implementit — S835, the WPF `Arrange` guard spectrum. Mirrors the S829
measure-entry sibling (11ac). All four WPF legs landed in `UIElement::Arrange`, plus the
`_finalRect` mirror (`arrange_slot_`) and the `ensureClip` analogue (`ensure_clip`).
plan_digest: WPF `UIElement.Arrange` (UIElement.cs:786-906) — (a) Inf/NaN slot throws
(798-809); (b) `Visibility.Collapsed` stores `_finalRect`, keeps dirty, returns without
arranging (811-828); (c) `MeasureDirty || NeverMeasured` → backfill `Measure` (833-862);
(d) `ensureClip(finalRect.Size)` at the end (884→3056). VAML did none.
- (a) `std::invalid_argument` naming the slot + offending value (S810_1 form) for a +Inf/NaN
  extent (WPF-exact: −Inf is NOT tested and is accepted; a −Inf extent degenerates to a
  0 client in compute_final_rect — no −Inf escapes).
- (b) `visibility()==Collapsed` → store `arrange_slot_`, return; no final_rect_/clip change,
  arrange_dirty_ stays set.
- (c) `measure_dirty_||never_measured_` → `Measure(slot size)` when never measured, else
  `Measure(previous_available_)`.
- (d) `ensure_clip(compute_final_rect().clip_rect)` AFTER ArrangeOverride (WPF order).
files_touched:
  - src/core/src/UIElement.cpp — Arrange guard spectrum + `ensure_clip` def + `arrange_slot()`
    def; `<string>` include.
  - src/core/include/vaml/core/UIElement.h — `arrange_slot()` accessor + `ensure_clip` private
    decl + `arrange_slot_` member (already present from prior partial work; verified).
  - src/core/src/ObjectPool.h — kLargestElement 1360→1376 (kSlotSize 1408 unchanged).
  - src/core/src/Element.cpp — LATENT-BUG fix surfaced by the guard: ~35 controls echoed an
    INFINITE `available` into `DesiredSize`. Faithful fix: a finite offer fills, an
    infinite/unbounded offer falls back to the control's natural box
    (`std::isfinite(available.X) && available.X > 0.0f ? … : K`). NOTE: `std::min(available.X,K)`
    forms already clamp ∞→K and were LEFT unguarded (reverting the over-broad first pass).
  - src/core/src/StickyNoteContentControl.cpp — same finite-offer guard + `<cmath>`.
  - test/unit/Arrange_test.cpp — 5 S835 closure-audit cases (Inf/NaN throws; Collapsed stores
    slot + stays dirty + re-arranges on reveal; never-measured Measure backfill at the slot;
    measure-dirty backfill at PreviousConstraint; end-of-arrange clip refresh observable) +
    `AvailLeaf` helper + `<limits>`/`<stdexcept>`.
  - build/verify_s835.cmd — grep-audit + two-lane gate.
  - docs/roadmap.md — S835 → [DONE]* (checkpoint).
build_status: BOTH CI LANES GREEN from first principles (build\verify_s835.cmd → "S835 GREEN
(both CI lanes)"). windows-msvc msvc-debug warnings-as-errors CLEAN + ctest 4/4 100%.
ubuntu-clang clang-debug (VM /mnt/hgfs/VamlPOC) build warnings==0 (-Werror) + ctest 4/4 100%.
windows-clang-debug is NOT a CI lane.
next_step: S836 (11ad) — the render-slot clip gate + viewer growth (sample.s11ad.vaml).
notes: ZERO new locks (single-threaded arrange, S126 affinity). S829 measure-ENTRY untouched
(11ac) — the Element.cpp edits are MeasureOverride BODY finite-offer fixes (required so the
Arrive guard does not surface latent infinite DesiredSize), not the Measure entry contract.
The Infinity guard is WPF-exact (+Inf/NaN only). No commits (standing directive — commitit
outstanding, driver's seat is yours).
