# Chapter 6 — Layout, Styles, and Templates: Dressing Up the Same Old Controls

> ### In This Chapter
> - The v1 control wardrobe (small, but everything fits)
> - Laying things out with `Grid` and friends — same measure, new muscle memory
> - Making it pretty: explicit vs implicit styles
> - Re-shaping a button from the inside with a `ControlTemplate`
> - Why "compose, don't reinvent" is the v1 love language

**Evidence:** S520–S529 (layout/property tests), S142 (property system), S2xx (layout: measure/arrange),
S3xx (styles/resources), S3xx (ControlTemplate), S920 (documented v1 control set). Snippets pass `vamlcc`
(S951).

---

You don't need a lecture on layout — you need the *specific* wardrobe VAML shipped, and the few layout
hairstyles that behave differently. Consider this the "what's in the box" chapter.

## 6.1 The C# You Knew

A WPF `Control` is a template plus a `Template` property; layout is `Measure`/`Arrange` through the
`FrameworkElement` contract; styling is `Style` + `Setter`; resources are `StaticResource`/`DynamicResource`.
All familiar. Now: the VAML set.

## 6.2 The v1 Control Wardrobe

The v1 control set is **small and deliberately so** — production-quality rather than sprawling, and it's
*documented and stabilized* at `S920`. The authoritative list lives in that doc; these classes are
representative, and the public header facade (`S921`) is the source of truth for exact names:

- **Containers:** `StackPanel`, `Grid`, `Canvas`, `Border`, `ScrollViewer`
- **Content/presentation:** `TextBlock`, `Image`, `ToolTip`
- **Input:** `Button`, `TextBox`, `CheckBox`, `RadioButton`, `ComboBox`, `ListBox`
- **Structure:** `UserControl` (your `x:Class` owner base), `ItemsControl` (what `ListBox` uses to draw items)

> ⚠ **Warning — the v1 temptation:** if a control you need isn't in the set, the roadmap reserves advanced
> controls for post-1.0 (`S927`). **Compose with what exists; don't fight the set.** A `UserControl` built
> from these is faster to build, stays in the tested surface (`S520–S529`), and avoids deep template edge
> cases. Reinvention is not a v1 virtue.

## 6.2a Icons: the `Icon` control

Icons are deliberately simple in VAML (see the Vulkan-vs-DirectX note in the README). An icon is a **raster
slotted into the texture atlas** — `PNG`, or `SVG` pre-rasterized once — drawn as a quad. You pick it by
**key**, not by path (your ViewModel never holds a file path or a render object; it exposes an `IconKey`).

```vaml
<Icon IconKey="Add" Width="16" Height="16" Foreground="{DynamicResource TextBrush}" TintMode="Monochrome"/>
```

- `IconKey` names a packaged icon (resolution is restricted to the known resource set; no arbitrary paths).
- `TintMode="Monochrome"` tints a single-color alpha mask with `Foreground` in the shader — crisp at any
  size, clean transparency, no bitmap-effect mud.
- `TintMode="Preserve"` samples the multi-color raster as-authored.
- Scaling policy defaults to **none**: the largest authored size ≤ your slot (in physical pixels, DPI-aware)
  is used, never resampled — so a crystal-clear icon stays crystal-clear; request additional sizes when you
  need a different look at a different slot.

Missing or invalid keys do **not** crash the view — you get a deliberate fallback glyph plus a diagnostic
(Chapter 7's compile-time/diagnostics story). See ADR-015; tokens `S202_1`/`S213_1`/`S400_1`.

## 6.3 Laying It Out: Same Measure, New Muscle Memory

```vaml
<Grid>
    <Grid.RowDefinitions>
        <RowDefinition Height="Auto"/>
        <RowDefinition Height="*"/>
        <RowDefinition Height="32"/>
    </Grid.RowDefinitions>
    <TextBlock Grid.Row="0" Text="Header"/>
    <ListBox Grid.Row="1" ItemsSource="{Binding Rows}"/>
    <Button Grid.Row="2" Content="OK" Command="{Binding ok}"/>
</Grid>
```

- Measure/arrange is the same two-phase contract you already know. And the good news: correctness is
  **guaranteed by property-based layout tests** (`S524`: no negative size, always terminates) and stress
  (`S528` offscreen render tests). Layout bugs are CI's problem before they're yours.
- The renderer batches and keeps draw-call count near-constant (`S623`), but **measure cost still scales
  with tree size**. So: avoid deep, multiply-nested layouts. Your future self, on a laptop, will thank you.

> 💡 **Tip — star vs Auto, the eternal confusion:** `Height="*"` fills leftover space; `Height="Auto"`
> hugs the content. If a row "collapses to zero," you probably wanted `*` where you said `Auto`. One rule,
> endless Tuesday rescue.

## 6.4 Making It Pretty: Styles

Chapter 2 covered the dialect; here's the working recipe:

```vaml
<UserControl.Resources>
    <Style TargetType="Button" x:Key="Primary">
        <Setter Property="Foreground" Value="#FFFFFF"/>
        <Setter Property="Background" Value="#2B5B9B"/>
        <Setter Property="Padding" Value="12,6"/>
    </Style>
</UserControl.Resources>

<Button Style="{StaticResource Primary}" Content="Go"/>
```

- **Explicit** styles via `x:Key` + `StaticResource` — apply on demand.
- **Implicit** styles apply by *exact* `TargetType` automatically (Chapter 2's "make every button look like
  this" button).
- Style/resource lookup precedence is **fuzz-tested** (`S527`), so the shadowing and cross-scope edge cases
  are caught by CI, not shipped on a Tuesday.

## 6.5 Re-Shaping a Control: The `ControlTemplate`

A template restates a control's visual tree, and you author it exactly as you did in XAML:

```vaml
<Style TargetType="Button" x:Key="Pill">
    <Setter Property="Template">
        <Setter.Value>
            <ControlTemplate TargetType="Button">
                <Border Background="{TemplateBinding Background}"
                        CornerRadius="16"
                        Padding="{TemplateBinding Padding}">
                    <ContentPresenter/>
                </Border>
            </ControlTemplate>
        </Setter.Value>
    </Setter>
</Style>
```

- `TemplateBinding` binds a template element to a property of the control the template is applied to
  (`Background`, `Padding` here).
- `ContentPresenter` renders whatever the control's content is — `Content` for a `Button`, or an item for
  an `ItemsControl`.
- Template resources are scoped *to the template* (Ch.2.7), so a template is self-contained and won't leak.

> ◇ **Remember:** `TemplateBinding` = "this part of my template follows the control's property."
> `ContentPresenter` = "and this part shows whatever I was given." Together, they're the whole of template
> re-skinning.

## 6.6 Custom Controls vs Composition: The v1 Love Language

For v1, prefer **composition** (a `UserControl` `x:Class` built from shipped controls) over a custom
`Control` with a bespoke template + rendering. Composition:

- stays in the tested surface (`S520–S529`),
- avoids deep template/measure edge cases,
- and ships faster than a bespoke visual.

> ⚙ **Technical Stuff:** If you *must* have a truly custom control, you're out of the v1 comfort zone —
> and that's fine, it's just not what this chapter or v1 optimizes for. Revisit when a concrete need
> emerges post-1.0. Until then: compose.

## 6.7 Troubleshooting, the "Why Does It Look Wrong" Table

| You see | Cause | Fix |
|---|---|---|
| Row collapses to zero | `*` vs `Auto` confusion | star to fill, `Auto` to hug |
| Style didn't apply | implicit only matches exact `TargetType` | `x:Key` + `StaticResource`, or match exactly |
| Template binding "no value" | `TemplateBinding` to an unset prop | ensure the control property has a default |
| Items show but look flat | no item template/`Template` | wrap via `ContentPresenter` template |

## 6.8 In This Chapter, You Learned…

- The v1 control set is **small and stabilized at S920** — compose from it, don't fight it.
- Layout is the familiar measure/arrange contract; **correctness is CI's job** (`S524`, `S528`), and star
  vs Auto is the one to remember.
- Styles (`x:Key` + implicit) and `ControlTemplate` (`TemplateBinding`, `ContentPresenter`) author the way
  you already know — no new verbs, just new spellings.
