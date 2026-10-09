# VAML Design Seeds

> Planning-placeholder for detailed design notes. Each becomes a real design doc written from
> POC evidence during Sprint 12 (S480–S482). Seeds below capture the intended shape now.
>
> GOVERNING PRINCIPLE (read first): docs/design/GPU-DRIVEN-PRINCIPLE.md — all renderer work
> (Sprints 4, 5, 14) MUST adhere to it. Cross-ref: ADR-012.

## Design: Property System (see ROADMAP Sprint 1)
Typeless `Value` (variant) → `PropertyMetadata` (default/flags/coerce/validate/changed) → `DependencyObject`
local store + effective-value resolution (local ?? inherited ?? default) + per-thread `ChangeQueue`
(deferred, coalescing) notification + attached properties (owner≠class) + read-only enforcement.
Key invariants: type-safe get, inheritance only for `inherits`-flagged props, wrong-thread write queued.

## Design: Measure/Arrange (Sprint 3)
`Measure(available)->DesiredSize` (infinity-aware: stretch vs wrap) then `Arrange(finalRect)`; panels override
`MeasureOverride`/`ArrangeOverride`; `LayoutManager` re-lays-out dirty subgraph only. Panels: StackPanel
(last-child-stretch), Grid (Absolute/Auto/Star + spans), WrapPanel (flow), Canvas (attached Left/Top).

## Design: Renderer (Sprint 5)
Quad renderer + texture atlas + SDF text atlas + shaders (SDF text/solid/border) → scene→command-buffer
encoder walks tree → emits draw batches by material → z-order via tree order → GPU batch per texture/material.
Resource lifetime via retire-after-fence. DPI via logical-px layout + framebufferScale.

## Design: VBAML Schema (Sprint 7)
Versioned binary records: element-id, type-token, prop-token, event-token, resource-token, string-table.
Serializer must round-trip byte-identical; runtime VBamlReader instantiates via ElementFactoryRegistry,
sets props, attaches deferred events; dual-path parity harness vs runtime VamlReader.

## Design: Routed Events + MVVM (Sprints 6 & 8)
`Signal<T>` + token → routed events (bubble/tunnel + handled) → markup `Click="OnClick"` wiring. MVVM layer:
ObservableObject, RelayCommand, Messenger, static CompositionRoot. DI: static root for app deps; element-factory
registry for UI only (no string setter injection).

## Design: Rendering threading (Sprint 14)
Single-thread in POC → async layout on workers + GPU-driven batching (indirect/draw-list) + per-frame arena +
copy-on-write hot nodes + multi UI threads → final commit to render thread. TSan-verified.
