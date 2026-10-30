# RENDER_SURFACE — the XAML → Vulkan render-surface map

> **Purpose.** This is the authoritative catalog of how every XAML visual concept
> ("the app's render model") maps onto the VAML **Vulkan** render surface ("what
> actually draws"). It exists so that **no visual feature silently renders nothing
> or renders a wrong single box**. It is **not** a change to runtime behavior and it
> is **not** a plan to rewrite the renderer — it is the inventory of the *current*
> `classify_merge_element` reality, with every vertical slice / placeholder /
> recorded-NO-GAP made **visible and searchable**.
>
> **Scope.** This is the XAML/WPF render-model → VAML-Vulkan material+primitive
> boundary. It is deliberately **not** labeled "DirectX → Vulkan": the VAML viewer
> is a **native Vulkan renderer** (GLFW window + Vulkan swapchain; there is **no
> DirectX** anywhere in the repo). Naming it for the real subject (XAML → Vulkan)
> keeps the catalog true to the codebase.
>
> **Ground truth.** The classifier in
> `src/presentation/src/VamlRenderer.cpp` (`classify_merge_element`) is the single
> function that owns the XAML → Vulkan mapping. This catalog is transcribed from
> it and is kept in conformance by the validator
> (`src/presentation/tools/render_surface_check.py`).

---

## Status legend

| Status | Meaning |
|---|---|
| **REAL** | A genuine, kept, production-quality Vulkan material + primitive for this concept. Delivered and stays. |
| **VERTICAL-SLICE** | A deliberate, honest minimal draw that proves the control/feature end-to-end now; the full-fidelity rendering is recorded as a NO-GAP token elsewhere. |
| **PLACEHOLDER** | A deliberate fallback/synthetic marker (e.g. a "flag is here" glyph) where the real asset/material is a recorded NO-GAP token. |
| **NO-GAP** | The named resolution token that replaces the vertical-slice/placeholder. Recorded, never silent. |
| **GAP** | The concept has no render path and contributes nothing. The catalog makes these **visible** (never silent). |

---

## The material/primitive surface

The VAML Vulkan UI renderer emits **draw items** through a small, fixed taxonomy:
- `MaterialKind::Solid` — a filled quad at an arranged rect (a base `UiPipeline::Solid` primitive).
- `MaterialKind::Text` — an SDF glyph-quad (`DrawItemKind::TextGlyph`) built through the wrapped text layout + resolved foreground + a glyph cell.
- `MaterialKind::Monochrome` — a single-channel/tinted quad (the image/icon texture path; headless default uses a fallback glyph).
- `MaterialKind::Border` — a border rect (separate `UiPipeline` border primitive) with thickness/radius/AA.
- `MaterialKind::None` — contributes **no draw** (the encoder skips it).

The encoder skips degenerate items (`ui_item_degenerate` skip rule) — zero-area rects
and empty glyphs contribute nothing. Every control that must "hide when closed"
relies on this.

---

## The matrix — XAML visual concept → Vulkan material + status

Each row names the classifier arm (all in `classify_merge_element`) and its current
Vulkan material + status. **NO-GAP tokens** column records the exact token that
resolves the vertical-slice/placeholder to full fidelity.

### Text
| XAML concept | Classifier arm | Vulkan material | Status | NO-GAP token |
|---|---|---|---|---|
| `TextBlock` (S400) | `dynamic_cast<TextBlock>` | `Text` (SDF glyph-quad, one representative glyph) | **VERTICAL-SLICE** (single glyph/headless; no live device atlas) | `S409_1` (full multi-glyph SDF atlas render + generic renderer) |
| `Label` (S480) | `dynamic_cast<Label>` | `Solid` (fill box tinted by `Foreground`; glyphs defactor to the S400 Text path) | **VERTICAL-SLICE** | `S409_1` (real glyphs) |

### Image / icon
| XAML concept | Classifier arm | Vulkan material | Status | NO-GAP token |
|---|---|---|---|---|
| `Icon` (S400_1) | `dynamic_cast<Icon>` | `Monochrome` (fallback "broken icon" glyph) | **PLACEHOLDER** (no device IconAtlas headlessly) | `S209`/`S213_1`/`S409_1` (live atlas + best-fit quad) |
| `Image` (S483) | `dynamic_cast<Image>` | `Monochrome` (fallback glyph) | **PLACEHOLDER** (no real texture headlessly) | `S209`/`S213_1`/`S409_1` (real texture quad) |

### Interactive controls
| XAML concept | Classifier arm | Vulkan material | Status | NO-GAP token |
|---|---|---|---|---|
| `Button` (S401) | `dynamic_cast<Button>` | `Solid` (fill tinted by current visual-state `Background`, honoring `IsEnabled` fade) | **VERTICAL-SLICE** (default template fill) | `S409_1` / `[s401-live]` (full template chrome; shader-level disabled fade = `S205`) |
| `TextBox` (S402) | `dynamic_cast<TextBox>` | `Solid` (edit-field fill box) | **VERTICAL-SLICE** (typed glyphs = SDF/atlas path) | `S409_1` |
| `Slider` (S403) | `dynamic_cast<Slider>` | `Solid` (track box) | **VERTICAL-SLICE** (track+thumb visuals) | `S409_1` |
| `CheckBox` (S404) | `dynamic_cast<CheckBox>` | `Solid` (checked fill / unchecked outlined slot) | **VERTICAL-SLICE** (check marker) | `S409_1` |
| `ScrollViewer` (S405) | `dynamic_cast<ScrollViewer>` | `Solid` (viewport-clip box, zero-opacity) | **VERTICAL-SLICE** (scrollbar + beyond-viewport content) | `S409_1` |
| `PasswordBox` (S485) | `dynamic_cast<PasswordBox>` | `Solid` (masked edit-field box) | **VERTICAL-SLICE** (masked bullets via Text path) | `S409_1` |

### Selection family (Sprint 11c, S470–S473)
| XAML concept | Classifier arm | Vulkan material | Status | NO-GAP token |
|---|---|---|---|---|
| `RadioButton` (S471) | `dynamic_cast<RadioButton>` | `Solid` (check-glyph fill) | **VERTICAL-SLICE** | `S409_1` |
| `ComboBox` (S472) | `dynamic_cast<ComboBox>` | `Solid` (EditField-style field box + `→` hint) | **VERTICAL-SLICE** (full styled dropdown chrome) | `S649` |
| `ListBox` (S473) | `dynamic_cast<ListBox>` | `Solid` (list-fill box) | **VERTICAL-SLICE** | `S409_1` |
| `ListView` (S473) | `dynamic_cast<ListView>` | `Solid` (list-fill box) | **VERTICAL-SLICE** | `S409_1` |
| `DockPanel` (S470) | `dynamic_cast<DockPanel>` | `Solid` (neutral dock container fill) | **VERTICAL-SLICE** | `S409_1` |

### Display + shapes (Sprint 11d, S480–S486)
| XAML concept | Classifier arm | Vulkan material | Status | NO-GAP token |
|---|---|---|---|---|
| `Border` (S481) | `dynamic_cast<Border>` | `Border` (**first real use of the S205 border pipeline**: border_rect + thickness/radius/AA) | **REAL** | — (auto-closed) |
| `Rectangle` (S482) | `dynamic_cast<Rectangle>` | `Solid` (fill box via `Fill` brush read) | **VERTICAL-SLICE** | `S409_1`-style render |
| `Ellipse` (S482) | `dynamic_cast<Ellipse>` | `Solid` (fill box via `Fill`) | **VERTICAL-SLICE** | `S409_1`-style render |
| `Line` (S482) | `dynamic_cast<Line>` | `Solid` (fill box via `Stroke`) | **VERTICAL-SLICE** | `S409_1`-style render |
| `ProgressBar` (S484) | `dynamic_cast<ProgressBar>` | `Solid` (track+fill, fill fraction from Value/range) | **VERTICAL-SLICE** (narrow-fill geometry, marquee) | `S409_1` / `S649` |
| `GroupBox` (S485) | `dynamic_cast<GroupBox>` | `Solid` (bordered group fill + header band) | **VERTICAL-SLICE** | `S409_1`-style render |
| `Expander` (S485) | `dynamic_cast<Expander>` | `Solid` (header+content container) | **VERTICAL-SLICE** (chevron) | `S649` |
| `Separator` (S485) | `dynamic_cast<Separator>` | `Solid` (thin divider rule) | **VERTICAL-SLICE** | `S409_1`-style render |
| `StatusBar` (S485) | `dynamic_cast<StatusBar>` | `Solid` (docking item host fill) | **VERTICAL-SLICE** | `S409_1`-style render |

### Ordered-panel family
| XAML concept | Classifier arm | Vulkan material | Status | NO-GAP token |
|---|---|---|---|---|
| `StackPanel` | (base `UIElement` fallback) | `Solid` (fill, alpha by `effective_opacity`) | **VERTICAL-SLICE** | `S409_1`-style render |
| `Grid` | (base `UIElement` fallback) | `Solid` | **VERTICAL-SLICE** | `S409_1`-style render |
| `WrapPanel` | (base `UIElement` fallback) | `Solid` | **VERTICAL-SLICE** | `S409_1`-style render |
| `Canvas` | (base `UIElement` fallback) | `Solid` | **VERTICAL-SLICE** | `S409_1`-style render |

### Containers, menu, popup (Sprint 11e, S490–S495)
| XAML concept | Classifier arm | Vulkan material | Status | NO-GAP token |
|---|---|---|---|---|
| `TabControl` (S490) | `dynamic_cast<TabControl>` | `Solid` (content track fill) | **VERTICAL-SLICE** (tab strip + swap live) | `S409_1`-style render |
| `TabItem` (S490) | `dynamic_cast<TabItem>` | `Solid` (selected → prominent header fill) | **VERTICAL-SLICE** | `S409_1`-style render |
| `Menu` (S492) | `dynamic_cast<Menu>` | `Solid` (menu bar strip) | **VERTICAL-SLICE** | `S409_1`-style render |
| `MenuItem` (S492) | `dynamic_cast<MenuItem>` | `Solid` (item-row fill; checked tint) | **VERTICAL-SLICE** | `S409_1`-style render |
| `ContextMenu` (S493) | `dynamic_cast<ContextMenu>` | `Solid` (popup panel; zero-area when closed) | **VERTICAL-SLICE** | `S409_1`-style render |
| `ToolTip` (S494) | `dynamic_cast<ToolTip>` | `Solid` (popup panel; zero-area when hidden) | **VERTICAL-SLICE** | `S409_1`-style render |
| `Popup` (S494) | `dynamic_cast<Popup>` | `Solid` (popup panel; zero-area when hidden) | **VERTICAL-SLICE** | `S409_1`-style render |
| `GridSplitter` (S495) | `dynamic_cast<GridSplitter>` | `Solid` (splitter bar) | **VERTICAL-SLICE** | `S409_1`-style render |
| `DataGrid` (S495) | `dynamic_cast<DataGrid>` | `Solid` (list/grid fill; header band tinted) | **VERTICAL-SLICE** (per-cell glyphs + focus chrome) | `S649` |

### Toolkit / decorator family (11k-2)
| XAML concept | Classifier arm | Vulkan material | Status | NO-GAP token |
|---|---|---|---|---|
| `Viewbox` (11k-2) | `dynamic_cast<Viewbox>` | `None` (scale-contain decorator renders its **content child's** material, not a box of its own) | **REAL** | — (decorator-transparent) |
| `RepeatButton` (11k-2) | `dynamic_cast<RepeatButton>` | `Solid` (spin-button slot) | **VERTICAL-SLICE** | `S649`-style chrome |
| `ScrollBar` (11k-2) | `dynamic_cast<ScrollBar>` | `Solid` (track box) | **VERTICAL-SLICE** (thumb + page-up/down) | `S649` |
| `ToolBar` (11k-2) | `dynamic_cast<ToolBar>` | `Solid` (toolbar strip) | **VERTICAL-SLICE** | `S649`-style chrome |
| `ToolBarTray` (11k-2) | `dynamic_cast<ToolBarTray>` | `Solid` (toolbar-tray host) | **VERTICAL-SLICE** | `S649`-style chrome |

### Non-UIElement / structural
| XAML concept | Classifier arm | Vulkan material | Status | NO-GAP token |
|---|---|---|---|---|
| **`UIElement` base fallback** | (non-Text base) | `Solid` (fill, alpha by `effective_opacity`) | **VERTICAL-SLICE** | `S400`/`S364` (real control materials in place) |
| **non-`UIElement` (a `DependencyObject`-only control seed, not yet re-based)** | (no arm; falls to nothing) | — | **GAP** (visible-silent: contributes nothing until its own sprint re-bases it) | the control's own sprint step |
| **`Window`** | (base `UIElement` fallback) | `Solid` (window fill) | **VERTICAL-SLICE** | `S409_1`-style render |

> **Note on `Window`.** `Window` has no dedicated arm — it renders via the base
> `UIElement` Solid fallback. `Window.AllowsTransparency` is **not** rendered
> (unsupported). This is a **GAP-adjacent / VERTICAL-SLICE** entry: a splash whose
> whole design relies on transparency will render its panels/Labels/Image as solid
> boxes, not as the intended transparent composition. See the blank-splash
> diagnosis and the RENDER_SURFACE adoption (governance commit) context.

---

## Conformance validator

`src/presentation/tools/render_surface_check.py` walks **every** classifier arm in
`classify_merge_element` and cross-checks each against a row in this matrix.

Contract:
1. **Every classifier arm MUST have a row** in the matrix (by control type). A new
   control can no longer land a render path that is invisible / un-cataloged.
2. **Every row's NO-GAP token must name a real, scheduled token** (not blank unless
   the row is `REAL`).
3. The validator reports `OK` when every arm has a row and every row's status/NO-GAP
   is legal.

> Keeping it true: when S500–S503a / templates / behaviors / real rendering land,
> the matrix row is updated **at the same sprint step** that delivers it (per the
> NO-GAP and NO-LAZY-LLM conventions), then the validator is re-run. The catalog is
> never left to be backfilled.

---

## How this catalog feeds the 11f work

The 11f sprints are **render-inert** (ADR-011 model/logic): S500 (DataTemplate/
ItemTemplate), S501 (converters), S502 (behaviors) change **zero** classifier arms.
Therefore this catalog, written from the current renderer reality **first**, is
**stable across 11f** — S500–S502 are already conformant to it. The catalog is the
**global-impact-first** artifact: roadmap tokens are authored to reference it from
birth, and the final debug of the blank splash is grepped against these rows rather
than started cold.

**S503 (Viewer-Growth 11f)** adds templated items + converter output on the *live*
path; any new draw it produces must be added to this matrix at the **same step**.

