# session-context.md

last_command: implementit (Sprint 11m — S550..S559)
plan_digest: WPF-corpus TAIL closure — every DAG PLANNED row with no roadmap home scheduled REAL (S550-S557) + LIVE viewer tail (S558) + DAG re-state + SURVIVOR (S559).

## files_touched (Sprint 11m)
- src/core/include/vaml/core/Scaffolding.h/.cpp — S550 (Panel/Decorator, item containers, GridView family, Virtualizing panels, ItemsPanelTemplate/ItemContainerTemplate)
- src/core/include/vaml/core/Element.h/.cpp — S550-S555 classes (item containers, Shape/Path/Polygon/Polyline/Track/TickBar, TreeView/TreeViewItem, DatePicker/Calendar family, InkCanvas/InkPresenter/MediaElement, Page/Frame/DocumentViewer family) + registrations (count 100)
- src/core/include/vaml/core/RoutedEvent.h/.cpp — DpiChanged/ImageFailed, Expanded/Collapsed/Selected/Unselected/SelectedItemChanged, SelectedDates/DateChanged, MediaOpened/Ended/Failed
- src/core/include/vaml/core/TailMechanism.h/.cpp — S556 (5 framework converters + Validation family + item-container/collection/svc contracts)
- src/core/include/vaml/core/TailValueTypes.h — S557 (enums/event-arg/data value types; Orientation + DataGridColumn/Length NOT duplicated — consume core)
- src/core/include/vaml/core/BindingExpression.h — BindingConversion::detail default-init (clang)
- src/presentation/src/VamlRenderer.cpp — S550-S555 classifier arms
- src/core/CMakeLists.txt — Scaffolding.cpp + TailMechanism.cpp
- vaml_viewer/sample.tail.xaml + viewer_growth_11m.h — S558 (LIVE tail seam)
- test/unit — TailScaffolding_s550, TailShape_s551, TreeView_s552, DateCalendar_s553, TailMedia_s554, TailDocument_s555, TailMechanism_s556, TailValueTypes_s557, ViewerGrowth11m_s558 + count-gates (100)
- gap_dp_daggen.py + docs/DAG_controls.md — S559 DAG re-state (VAML_STATUS DONE tail + stale 11c/11d/11e + Viewbox spelling + WebBrowser/ViewPort3D OUT-OF-SCOPE(11m)); generator re-ran cleanly (282 rows, exactly-one-state); the manual-add note-blocks (12k-5 trigger family + 11f/EventTrigger disambiguation + 11n-0 DAG-EXT cluster) re-applied verbatim via _dag_s559.py (the S503a GEN-LOSSY escape); S556 mechanism + S557 value rows flipped DONE
- build/verify_s559.cmd — the missing S559 deliverable: both CI lanes + the DAG-completeness grep (manual rows survived) + the MINIMAL-REAL/STUB grep-audit
- progress.md — S11m SURVIVOR + CHECKPOINT ADVANCE (last = S11m, next = S540+/Sprint 11z)
- build/verify_s550.cmd, verify_s551.cmd, _build_win*.cmd

## build_status
- windows-msvc (msvc-debug): GREEN — presentation 163/163, core 1220 (1 pre-existing intentional shouldly-only failure)
- ubuntu-clang (VM): GREEN — 4/4 suites (S550-S558)

## next_step
- Sprint 11z (S540+): the next living lane. DAG-RE-GEN NORM recorded — every future control-supplier SURVIVOR re-runs gap_dp_daggen.py (base-regen + manual re-apply).

## notes
- ZERO MINIMAL REAL + ZERO stubs + ZERO invented names across the sprint (grep-audit PASS). ADR-018 zero locks; ADR-011 (surface = core data; render stays in the vulkan classifier).
- WebBrowser/ViewPort3D re-affirmed OUT-OF-SCOPE(11m) (S520 import/inject seam) — reclassified in the DAG at S559.
- CalendarDateRange/BlackoutDates + DataGridColumn/Length are pure data/surfaces CONSUMED from core — DAG flips at S559.
- No commit made (standalone directive — commitit outstanding, driver's seat is yours).
