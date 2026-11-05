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
| `Label` (S480) | `dynamic_cast<Label>` | `Solid` (**neutral alpha-1.0 backdrop box** — S593: the invented `0.85f` is GONE, and the box is NOT tinted by `Foreground` per WPF; the content/glyph uses the resolved `Foreground` alpha via the S400 path; `Opacity` folds once) | **VERTICAL-SLICE** | `S409_1` (real glyphs) |

### Image / icon
| XAML concept | Classifier arm | Vulkan material | Status | NO-GAP token |
|---|---|---|---|---|
| `Icon` (S400_1) | `dynamic_cast<Icon>` | `Monochrome` (fallback "broken icon" glyph) | **PLACEHOLDER** (no device IconAtlas headlessly) | `S209`/`S213_1`/`S409_1` (live atlas + best-fit quad) |
| `Image` (S483) | `dynamic_cast<Image>` | `Monochrome` (fallback glyph) | **PLACEHOLDER** (no device IconAtlas headlessly) | **S640** (`ImageGlyphPlan` full-color best-fit quad; live device atlas + S209 texture-binding + REAL framebuffer-scale = [s640-live] + the S641 scanner confirms rect + non-default pixels) |

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
| `StackPanel` | (base `UIElement` fallback) | `Solid` (fill from the resolved `Background` color alpha when authored, else the neutral fill at alpha 1.0; **`Opacity` folds EXACTLY ONCE** via the encoder — the S593 single-fold fix removes the opacity² double-count) | **VERTICAL-SLICE** | `S409_1`-style render |
| `Grid` | (base `UIElement` fallback) | `Solid` (same S593 alpha single-fold; authored `Background` alpha carried through) | **VERTICAL-SLICE** | `S409_1`-style render |
| `WrapPanel` | (base `UIElement` fallback) | `Solid` (same S593 alpha single-fold) | **VERTICAL-SLICE** | `S409_1`-style render |
| `Canvas` | (base `UIElement` fallback) | `Solid` (same S593 alpha single-fold) | **VERTICAL-SLICE** | `S409_1`-style render |
| `Window` (S440) | (base `UIElement` fallback) | `Solid` (fill from the resolved `Background` color alpha when authored — the S593 single-fold; a `Window{AllowsTransparency=True}` composites clear alpha 0 via S592 `decide_clear`) | **VERTICAL-SLICE** | `S409_1`-style render |

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

### Corpus tail + shape/channel/tree/date/document family (Sprint 11m, S550–S558)
| XAML concept | Classifier arm | Vulkan material | Status | NO-GAP token |
|---|---|---|---|---|
| `Path` (S551) | `dynamic_cast<Path>` | `Solid` (fill box via `Fill`/`Stroke`) | **VERTICAL-SLICE** (curved/segmented outline) | `S649`-style real geometry render |
| `Polygon` (S551) | `dynamic_cast<Polygon>` | `Solid` (fill box via `Fill`) | **VERTICAL-SLICE** | `S649`-style render |
| `Polyline` (S551) | `dynamic_cast<Polyline>` | `Solid` (fill box via `Stroke`) | **VERTICAL-SLICE** | `S649`-style render |
| `Track` (S551) | `dynamic_cast<Track>` | `Solid` (channel box) | **VERTICAL-SLICE** (thumb repeat) | `S649` |
| `TickBar` (S551) | `dynamic_cast<TickBar>` | `Solid` (tick slot) | **VERTICAL-SLICE** (tick marks) | `S649` |
| `TreeView` (S552) | `dynamic_cast<TreeView>` | `Solid` (tree fill) | **VERTICAL-SLICE** (indent/expand chrome) | `S649`-style render |
| `TreeViewItem` (S552) | `dynamic_cast<TreeViewItem>` | `Solid` (item-row fill; selected tint) | **VERTICAL-SLICE** | `S649`-style render |
| `DatePicker` (S553) | `dynamic_cast<DatePicker>` | `Solid` (field box) | **VERTICAL-SLICE** (dropdown calendar) | `S649`-style render |
| `DatePickerTextBox` (S553) | `dynamic_cast<DatePickerTextBox>` | `Solid` (edit-field box) | **VERTICAL-SLICE** | `S649`-style render |
| `Calendar` (S553) | `dynamic_cast<Calendar>` | `Solid` (calendar grid) | **VERTICAL-SLICE** (day-cell chrome) | `S649`-style render |
| `CalendarItem` (S553) | `dynamic_cast<CalendarItem>` | `Solid` (calendar panel) | **VERTICAL-SLICE** | `S649`-style render |
| `CalendarDayButton` (S553) | `dynamic_cast<CalendarDayButton>` | `Solid` (day-button slot) | **VERTICAL-SLICE** | `S649`-style render |
| `CalendarButton` (S553) | `dynamic_cast<CalendarButton>` | `Solid` (button slot) | **VERTICAL-SLICE** | `S649`-style render |
| `InkCanvas` (S554) | `dynamic_cast<InkCanvas>` | `Solid` (ink surface fill) | **VERTICAL-SLICE** (stroke rendering) | `S649`-style render |
| `InkPresenter` (S554) | `dynamic_cast<InkPresenter>` | `Solid` (stroke host fill) | **VERTICAL-SLICE** | `S649`-style render |
| `MediaElement` (S554) | `dynamic_cast<MediaElement>` | `Monochrome` (media frame texture) | **VERTICAL-SLICE** (buffered/playback chrome) | `S649` |
| `Page` (S555) | `dynamic_cast<Page>` | `Solid` (page fill) | **VERTICAL-SLICE** | `S649`-style render |
| `Frame` (S555) | `dynamic_cast<Frame>` | `Solid` (frame fill) | **VERTICAL-SLICE** | `S649`-style render |
| `DocumentViewer` (S555) | `dynamic_cast<DocumentViewer>` | `Solid` (document view fill) | **VERTICAL-SLICE** | `S649`-style render |
| `FlowDocumentReader` (S555) | `dynamic_cast<FlowDocumentReader>` | `Solid` (reader fill) | **VERTICAL-SLICE** | `S649`-style render |
| `FlowDocumentScrollViewer` (S555) | `dynamic_cast<FlowDocumentScrollViewer>` | `Solid` (scroll reader fill) | **VERTICAL-SLICE** | `S649`-style render |
| `SinglePageViewer` (S555) | `dynamic_cast<SinglePageViewer>` | `Solid` (single-page fill) | **VERTICAL-SLICE** | `S649`-style render |

> **Sprint 11m conformance (recorded, not silent):** the S550–S558 tail controls are all
> REAL classified surfaces (mostly `Solid`, `MediaElement` → `Monochrome`) delivered at their
> own 11m steps. These rows close the RENDER_SURFACE gap that the 11m SURVIVOR recorded —
> the catalog had not yet been extended for the tail controls (a coverage-matrix drift, now
> consolidated at the 11n-7 wrap per the coverage-matrix obligation). No new classifier arm /
> material / matrix row introduced by 11n itself (see the 11n surface note below).

### Non-UIElement / structural
| XAML concept | Classifier arm | Vulkan material | Status | NO-GAP token |
|---|---|---|---|---|
| **`UIElement` base fallback** | (non-Text base) | `Solid` (fill, alpha by `effective_opacity`) | **VERTICAL-SLICE** | `S400`/`S364` (real control materials in place) |
| **non-`UIElement` (a `DependencyObject`-only control seed, not yet re-based)** | (no arm; falls to nothing) | — | **GAP** (visible-silent: contributes nothing until its own sprint re-bases it) | the control's own sprint step |
| **`Window`** | (base `UIElement` fallback) | `Solid` (window fill) | **VERTICAL-SLICE** | `S409_1`-style render |

> **Note on `Window`.** `Window` has no dedicated arm — it renders via the base
> `UIElement` Solid fallback. **Known drift (recorded, not silent — the anti-lazy
> posture):** WPF itself handles **100% of transparency** natively on every visual
> that participates in the render tree — Window chrome, panel `Background`, each
> element's `Opacity`, alpha brushes, everything. The ONLY WPF conditions that are
> not handled by WPF's own render path are a narrow set that rely on a **secondary
> COM object** to complete the transparency — and we do NOT defer, special-case, or
> sidestep that. Those conditions are not individually important; what is important
> is the contract: **VAML transparently handles 100% of the transparency WPF exposes,
> composited through the SAME pipeline to Vulkan, exactly like any other XAML → Vulkan
> render surface.** The currently-rendered solid-box behavior below is therefore a
> transient vertical-slice artifact of the WIP renderer, NOT a designed limitation —
> it is closed by **Sprint 11o (S590–S596, token S595 viewer-growth)**, which makes
> `AllowsTransparency` + per-element alpha real, end-to-end, on every platform the
> swapchain/compositor can support (fail-fast probe at S590; premultiplied final
> surface at S591; per-window clear + background composition at S592; element alpha
> at S593; OS-window transparency at S594; live-consumed `AllowsTransparency` +
> viewer proof at S595). Any remaining per-machine constraint (e.g. a swapchain that
> rejects composite-alpha) is documented at the S590 probe as an honest capability
> result with a clean opaque fallback — never silent, never a deferral. See the
> blank-splash diagnosis and the RENDER_SURFACE adoption (governance commit) context.

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

**S503 CONFORMANCE (recorded, not silent):** the 11f viewer produces its templated
items + converter output by COMPOSING the EXISTING classifier arms — a `ListBox`
(→ `Solid`), its per-item `ContentPresenter`s (→ the shared `UIElement` Solid
fallback), and `TextBlock`s (→ `Text`) — plus the converter-driven `Visibility`
on a `TextBlock` (reusing the S120 Visibility DP + the `S501 BoolVis` converter,
which map `Collapsed` → a zero-area skip via `ui_item_degenerate`). **No new
classifier arm / material / matrix row is introduced by 11f.** The RENDER_SURFACE
conformance validator passes at S503 (`OK`; the S212 `encode_visual_slice` render-
plan in `ViewerGrowth11f_test` asserts the emitted Solid + Text draw counts). If a
later step adds a genuinely NEW draw (a dedicated datatemplate material), the arm +
row must be added at that step (NO-GAP).

---

## The EFFECTS surface (S511 — WPF-truthful, dep:S510)

> **Purpose.** This section catalogues how the WPF `UIElement.Effect` visual effects
> (`DropShadowEffect`, `BlurEffect`) map onto the VAML render surface. It exists so
> that the effect surface is **visible and searchable** and so the two-GPU-blit
> caveat (the sharp-shadow vertical slice vs. the true offscreen blur) is never
> silent. The core effect MODEL + the pure shadow seams live in `vaml/core`
> (`Effect.h`); the RENDER arm lives in the vaml-vulkan `UiSceneEncoder`
> (`append_effect_draws`), NOT in `classify_merge_element` (the encoder is the
> authoritative effect emitter, so the validator's classifier-arm walk does not
> cover it — its rows are exercised by `UiSceneEncoder_effect_test.cpp`).

| XAML concept | Model + seams (vaml/core) | Vulkan render arm | Status | NO-GAP token |
|---|---|---|---|---|
| `DropShadowEffect` (S511) | `DropShadowEffect {Color,BlurRadius,ShadowDepth,Direction,Opacity}` + `drop_shadow_offset`/`drop_shadow_color` | `UiSceneEncoder::append_effect_draws` — an OFFSET translucent shadow quad emitted BEHIND the element's draw, tinted at Effect.Opacity | **VERTICAL-SLICE** (sharp, unblurred shadow quad; the true offscreen alpha-mask Gaussian blur is the NO-GAP token) | `S649`-style offscreen blur/blit shader pass (forward token) |
| `BlurEffect` (S511) | `BlurEffect {Radius}` | pass-through (renders the element as-is) | **VERTICAL-SLICE** (the true offscreen Gaussian blur of the subtree + blit is the NO-GAP token) | `S649`-style offscreen blur/blit shader pass (forward token) |

> **CONFORMANCE (recorded, not silent):** the S511 drop-shadow and blur are the
> honest headless vertical slices: DropShadow emits a real offset translucent
> shadow quad BEHIND the element (proven by `UiSceneEncoder_effect_test`), Blur
> passes through. The full WPF-faithful offscreen separable-Gaussian blur + blit
> (which makes both DropShadow's blurred mask and BlurEffect's blurred subtree
> true GPU blits) is the named forward token. Effects introduce NO change to any
> existing `classify_merge_element` arm, so the RENDER_SURFACE conformance
> validator stays `OK` at S511 (re-run below).

---

## The CONVERTER SURFACE (S501 — WPF-truthful)
> **Purpose.** This section is the authoritative catalog of the VAML **value-converter**
> surface. Converters convert a bound *source* value into a *target* value; per
> ADR-011 they are **logic, not rendering** — a converter never draws, it only
> produces a Value the renderer consumes. This catalog lives here (alongside the
> render map) so the converter surface is *visible and searchable* and so no author
> silently invents a converter that WPF does not have.

### The seam — `IValueConverter` (NOT a `ValueConverter` base)

**WPF ground truth (verified against `E:\GitRepos\wpf`):** the binding value-converter
surface is the **`IValueConverter` INTERFACE** (`System\Windows\Data\IValueConverter.cs`).
There is **no `ValueConverter` base class anywhere in the WPF tree**. (The roadmap's
earlier "`ValueConverter` base" wording was make-believe; S501 corrected it to the
WPF-truthful interface and renamed the S089 literal/type-converter alias
`ValueConverter` → `LiteralConverter` so the two surfaces never collide.)

| Concept | WPF | VAML (S501) | Status |
|---|---|---|---|
| The binding converter interface | `IValueConverter` | `IValueConverter` (`core/IValueConverter.h`) | **REAL** |
| The concrete bool→Visibility converter | `BooleanToVisibilityConverter` | `BooleanToVisibilityConverter : IValueConverter` | **REAL** (the SOLE framework-owned) |
| Convert / ConvertBack | both required in WPF | `Convert` + `ConvertBack`; **ConvertBack default = not-supported** | **REAL** |

### Which converters are FRAMEWORK-owned

The framework ships **exactly one** concrete converter: **`BooleanToVisibilityConverter`**
(bool → the S120 Visibility Int32 DP: `true`→`Visible(0)`, `false`→`Collapsed(2)`).
Everything else is **app-authored**.

The remaining WPF framework converters are **NOT built at S501** — they are
**S556-consumed** (recorded NO-GAP, not silent):
`AlternationConverter`, `BooleanToSelectiveScrollingOrientationConverter`,
`BorderGapMaskConverter`, `MenuScrollingVisibilityConverter`,
`DataGridHeadersVisibilityToVisibilityConverter`.

### `NullTo*` is APP-AUTHORED, NOT a framework type

WPF has **no framework `NullTo*` converter** (the roadmap's "`NullTo*`" spelling
mislead). `NullToVisibility`, `NullToCollapsed`, `NullToPlaceholder`, etc. are all
**app-authored types** the app writes by implementing `IValueConverter`:

```cpp
class MyNullToPlaceholder : public vaml::IValueConverter {
public:
    vaml::BindingConversion Convert(const vaml::Value& v) const override {
        if (v.isNull()) return {true, vaml::Value::String("[none]"), {}};
        return {true, v, {}};
    }
};
```

### Authoring a converter in markup

Register the converter by key on the reader (app-owned, S250 pattern) before binding,
then reference it with `Converter=`:

```cpp
reader.register_converter("BoolVis", std::make_shared<vaml::BooleanToVisibilityConverter>());
```

```xml
<TextBlock Visibility="{Binding IsAvailable, Converter={StaticResource BoolVis}}"/>
<!-- or the bare-key form: -->
<TextBlock Visibility="{Binding IsAvailable, Converter=BoolVis}"/>
```

The reader resolves the key through its converter registry and wires the resolved
`IValueConverter` into the S242 `BindingExpressionSpec.converter` via `ToBindingConverter`.
An **UNRESOLVED `Converter=` key is a Class A reader error** (fail loud, never silent).

### Converter failure

A converter returns `BindingConversion{ok=false, detail}` (or throws) on failure. The
S242 pipeline emits `ConverterFailed` and, when a fallback is bound, applies it
(`FallbackApplied`). `ConvertBack` failure on a TwoWay binding emits `WriteBackRejected`
(S244) and skips the write-back — never a throw (ADR-016, Class B observation).

---

## The 11n SURFACE (3D + Adorner — non-`classify_merge_element` drawable surfaces)

> **Purpose.** The 11n sprint (11n-5 3D, 11n-6 Adorner/VisualTreeHelper) delivers two
> drawable surfaces that are **NOT `classify_merge_element` classifier arms** — they
> render via dedicated seams OUTSIDE the base visual-slice walk, so the base-vs-
> encoder split keeps ADR-011 clean (model/logic in core; "how to draw" in the
> vulkan lane). They are cataloged here so they are **visible and searchable**
> (the coverage-matrix obligation), with the honest vertical slice / live-lane /
> forward-token split — never silent.

| XAML concept | Model + seams (vaml/core) | Vulkan render arm | Status | NO-GAP token |
|---|---|---|---|---|
| `Viewport3D`/`Model3D`/`Geometry3D`/`Visual3D` (11n-5) | `Viewport3D` (FrameworkElement, IAddChild) hosts `Model3D`/`Geometry3D` (Animatable) content + camera/projection; `Visual3D : DependencyObject` (NOT Visual) | `vaml::vulkan::media3d` — the headless SOFTWARE rasterizer (pure geometry→tile; CPU, no GPU) | **VERTICAL-SLICE** (headless geometry→tile on BOTH lanes; the live viewport-presented Model3D on a real device is the [both apps, not CI] live lane) | live **GPU path** ([both-apps, not CI] live lane — viewport-presented Model3D), recorded not silent |
| `Adorner` layer (11n-6) | `Adorner : UIElement` (measure-to-adorned-size + GetDesiredTransform + IsClipEnabled), `AdornerLayer` (Add/Remove/z-order/Update), `AdornerDecorator` host | `encode_adorner_tree` in the vaml-vulkan `UiSceneEncoder` — a SECOND-PASS adorner emission feeding the existing S206 batcher, rendered ABOVE at the adorned element's transform | **VERTICAL-SLICE** (CPU side on BOTH lanes; the second-pass adorner emission into the encoder is the [both apps, not CI] live lane) | **AdornerHitTest** + adorner input routing (overlaying S323 hit-test) = the named forward refinement, recorded not silent |

> **CONFORMANCE (recorded, not silent):** neither the 11n-5 3D surface nor the 11n-6
> adorner surface introduces a NEW `classify_merge_element` arm (they are non-classifier
> drawable surfaces rendered via dedicated seams), so the RENDER_SURFACE conformance
> validator stays `OK` at the 11n-7 wrap. The rows above make both surfaces first-class
> catalog entries with an honest status + forward token — no silent gap. The 11n-5 GPU
> path (viewport-presented Model3D on a real device) and the 11n-6 adorner render +
> AdornerHitTest lanes are [both apps, not CI], exercised live on the NVIDIA box, never
> CI gates (recorded not silent).

 
---  
## Alpha compositing (S593)  
  
> **S593 alpha model (recorded, not silent):** every classifier arm's alpha flows (1) the straight-alpha blend for its own material (Solid/Image already emit alpha; Border/Label/Text pass alpha through), (2) content-alpha * Opacity coverage multiplier (exactly ONCE, folded by the S208 encoder via `visual.item.a *= effective_opacity` - the S593 single-fold fix removed the base-arm opacity^2 double-count), (3) into the premultiplied final surface (S591) / transparent clear (S592). The base `UIElement` fallback reads the resolved `Background` Color alpha (authored) or neutral alpha 1.0; `Label` keeps a neutral alpha-1.0 backdrop with `Foreground` on the glyph/content (WPF-faithful - no invented constant, no foreground-tinted box). RENDER_SURFACE validator stays `OK` (no new classifier arm introduced by S593; semantics-only edits). 
