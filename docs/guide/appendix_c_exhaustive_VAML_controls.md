# Appendix C — The Exhaustive VAML Controls & Authoring Surface

> **The Part of Tens, part three.** Where `DAG_controls.md` is the **engine contract** (what the object
> model *supports*), this appendix is the **authoring encyclopedia** — everything a VAML author can *type*:
> controls, styles, templates, triggers, resource keys, value elements, attached properties. The two
> documents are the **spine of the roadmap**: a step is done when its referenced rows flip `DONE`.
>
> **Evidence/prereq:** symbols must resolve in the public facade (`S921`); verified by the guide CI gate
> (`S951`). This appendix states only *implemented* behavior, cross-referenced to the DAG row-IDs
> (`DAG.Button`, …). A row that is `PLANNED`/`OUT-OF-SCOPE` here is *declared, not hidden* — never silently
> omitted.

> **How this appendix is kept exhaustive.** The control/type inventory (and each control's full ancestor
> chain + property surface) is the **generated** `docs/DAG_controls.md` (from the real WPF source). This
> appendix adds the **authoring syntax** — every type gets a section here with its XAML-usage form.
> **Closure-under-ancestry** (from the DAG doc) applies: a control inherits every ancestor's surface.

---

## 1. The value/reference model

| Kind | WPF | VAML | tri-state |
|------|-----|------|-----------|
| Value types (authorable elements) | `Color`, `Double`, `Thickness`, `GridLength`, `FontFamily` | authored elements typed like WPF (`Value::Color`, `Value::Double`, `Value::Thickness`, `Value::GridLength`, `Value::String`) | **DONE (11k-3)** |
| `Geometry` (abstract) | NOT authorable as a bare element | **OUT-OF-SCOPE(11k-4)** — resolves via concrete `<PathGeometry>`/`<GeometryGroup>`-style elements | OUT-OF-SCOPE(11k-4) |

### Authoring value-type resources (11k-3)

Value-type resources are authored **by `x:Key`** inside a `ResourceDictionary` and stored as **Plain**
values (so `{StaticResource}` on a Color/Thickness/Double/GridLength DP applies them — Object statics are
rejected on a value DP):

```xml
<Window.Resources>
  <ResourceDictionary>
    <Color        x:Key="Accent">#FF2E7AF2</Color>
    <FontFamily   x:Key="AppFont">Segoe UI</FontFamily>
    <Double       x:Key="IconSize">24</Double>
    <Thickness    x:Key="CardPadding">12</Thickness>
    <GridLength   x:Key="SplitWidth">2*</GridLength>
    <!-- <Geometry x:Key="..."> deferred to 11k-4 (WPF-abstract) -->
  </ResourceDictionary>
</Window.Resources>
```

Rules:
- Each must carry `x:Key` (no-key → structured authoring error, never silent).
- Duplicate key → `ResourceAlreadyRegistered` (dedupe-throws-unchanged).
- Malformed inline content → fail-loud `ValueConversionException{MalformedValue}`.
- `Color` parses `#RRGGBB` / `#AARRGGBB` (parse_color); `Thickness` parses `1/2/4` comma forms
  (parse_thickness); `Double` via the Double converter; `GridLength` parses `Auto`/`N*`/numeric
  (parse_grid_length, hoisted public at 11k-3).
- Consumption: `{StaticResource Key}` applies the Plain value directly to a matching DP.

---

## 2. The base-branch surface (PLANNED — 11k-11 S649)

These abstract bases carry the DPs most controls inherit. `PLANNED` = declared, scheduled at 11k-11.

| Type | Key DPs / events | tri-state |
|------|------------------|-----------|
| `UIElement` | `Visibility, Opacity, Margin, RenderTransform, Width, Height, HorizontalAlignment, VerticalAlignment, Focusable, Tag` + input/event surface | PLANNED |
| `FrameworkElement` | `DataContext, Style, ToolTip, Cursor, Loaded/Unloaded, Width/Height/Min/Max, ContextMenu` | PLANNED |
| `Control` | `Padding, HorizontalContentAlignment, VerticalContentAlignment, Template, Background, BorderBrush, BorderThickness, Foreground, FontFamily/FontSize/FontWeight/FontStyle, IsTabStop, TabIndex` | PLANNED |
| `ContentControl` | `Content, ContentTemplate, ContentStringFormat, ContentTemplateSelector` | PLANNED |
| `ButtonBase` | `Command, CommandParameter, CommandTarget, ClickMode` + `Click` | PLANNED |
| `RangeBase` | `Minimum, Maximum, SmallChange, LargeChange, Value` | PLANNED |
| `ItemsControl` | `Items, ItemsSource, ItemTemplate, DisplayMemberPath` | PLANNED |
| `TextElement` | `FontFamily/FontSize/FontStyle/FontWeight/Foreground/Background` (attached, for inline text spans) | PLANNED |

---

## 3. Controls (DONE in Vaml)

For each: the DAG ancestor chain, the Vaml-modeled DPs, and the authoring form.

### `Button` — `Button → ButtonBase → ContentControl → Control`
```xml
<Button Content="Save" Click="OnSave" IsDefault="True" Width="80" Height="28"/>
```
Vaml DPs: `Content, IsEnabled` (+ 11k-2 Click). Events: `Click`.

### `StackPanel` — `StackPanel → Panel → FrameworkElement`
```xml
<StackPanel Orientation="Vertical" HorizontalAlignment="Center">
  <TextBlock Text="Hi"/> <Button Content="OK"/>
</StackPanel>
```
Vaml DPs: `Orientation`.

### `Grid` — `Grid → Panel → FrameworkElement`
```xml
<Grid>
  <Grid.ColumnDefinitions><ColumnDefinition Width="2*"/><ColumnDefinition Width="Auto"/></Grid.ColumnDefinitions>
  <Grid.RowDefinitions><RowDefinition Height="Auto"/></Grid.RowDefinitions>
  <TextBlock Grid.Column="0" Grid.Row="0"/>
</Grid>
```
Attached: `Grid.Row/Column/RowSpan/ColumnSpan`. Track lengths via `parse_grid_length` (11k-3-hoisted public).

### `TextBlock` — `TextBlock → FrameworkElement` (NOT a Control)
```xml
<TextBlock Text="label" FontFamily="Segoe UI" FontSize="14" FontWeight="Bold" Foreground="Blue"/>
```
Vaml DPs: `Text, FontSize, FontFamily, FontWeight, Foreground` (+ Control-base FontSize/FontFamily
inherited — re-based at 11k-1; NOT inheriting Control's content-alignment). Inline content: `<TextBlock>label</TextBlock>`.

### `TextBox` — `TextBox → TextBoxBase → Control`
```xml
<TextBox Text="edit me" AcceptsReturn="False"/>
```
Vaml DPs: `Text, AcceptsReturn`.

### `CheckBox` — `CheckBox → ToggleButton → ButtonBase → ContentControl`
```xml
<CheckBox Content="Agree" IsChecked="True" IsThreeState="False"/>
```
Vaml DPs: `IsChecked, IsThreeState`.

### `RadioButton` — `RadioButton → ToggleButton → ButtonBase → ContentControl`
```xml
<RadioButton Content="Option A" IsChecked="True" GroupName="G"/>
```
Vaml DPs: `IsChecked`.

### `Slider` — `Slider → RangeBase → Control`
```xml
<Slider Minimum="0" Maximum="100" Value="50"/>
```
Vaml DPs: `Value, Minimum, Maximum`.

### `ProgressBar` — `ProgressBar → RangeBase → Control`
```xml
<ProgressBar Minimum="0" Maximum="100" Value="40"/>
```
Vaml DPs: `Value, Minimum, Maximum`.

### `ComboBox` — `ComboBox → Selector → ItemsControl`
```xml
<ComboBox SelectedIndex="0"><ComboBoxItem Content="A"/><ComboBoxItem Content="B"/></ComboBox>
```
Vaml DPs: `Items, ItemsSource, SelectedIndex, SelectedItem, IsOpen`.

### `ListBox` — `ListBox → Selector → ItemsControl`
```xml
<ListBox ItemsSource="{Binding Data}" SelectedItem="{Binding Sel}"/>
```
Vaml DPs: `Items, ItemsSource, SelectedIndex, SelectedItem, ItemTemplate`.

### `ListView` — declared PLANNED (DAG row exists; Vaml models no own DPs yet).

### `ScrollViewer` — `ScrollViewer → ContentControl → Control`
```xml
<ScrollViewer><StackPanel>…</StackPanel></ScrollViewer>
```
Vaml DPs: `ViewportSize` (+ ScrollBar chrome from 11k-2).

### `ScrollBar` — `ScrollBar → RangeBase → Control`
```xml
<ScrollBar Orientation="Vertical" Minimum="0" Maximum="100" Value="30" ViewportSize="10"/>
```
Vaml DPs: `Value, Minimum, Maximum, ViewportSize, Orientation` + bubbling `Scroll` event.

### `RepeatButton` — `RepeatButton → ButtonBase → ContentControl`
```xml
<RepeatButton Content="▲" Delay="400" Interval="75"/>
```
Vaml DPs: `Delay, Interval, IsPressed` + `Click`.

### `Window` — `Window → ContentControl → Control`
```xml
<Window AllowsTransparency="False" WindowStartupLocation="CenterScreen" Topmost="False"/>
```
Vaml DPs: `AllowsTransparency, WindowStartupLocation, WindowStyle, Topmost, ResizeMode`.

### `UserControl` — `UserControl → ContentControl → Control`
```xml
<UserControl>…single content…</UserControl>
```
Vaml DP: `Content`.

### `WrapPanel` — `WrapPanel → Panel → FrameworkElement`
```xml
<WrapPanel WrapDirection="LeftToRight"><Button/><Button/></WrapPanel>
```
Vaml DP: `WrapDirection`.

### `Canvas` — `Canvas → Panel → FrameworkElement`
```xml
<Canvas><Rectangle Canvas.Left="4" Canvas.Top="6"/></Canvas>
```
Attached: `Canvas.Left/Top` (NaN default).

### `DockPanel` — `DockPanel → Panel → FrameworkElement`
```xml
<DockPanel LastChildFill="True"><StatusBar DockPanel.Dock="Bottom">…</StatusBar></DockPanel>
```
Attached: `DockPanel.Dock`.

### `Label` — `Label → ContentControl → Control`
```xml
<Label Content="Field:" Foreground="Gray"/>
```
Vaml DPs: `Content, Foreground`.

### `Border` — `Border → Decorator → FrameworkElement`
```xml
<Border BorderBrush="Gray" BorderThickness="1" CornerRadius="4" Padding="8">
  <TextBlock Text="card"/>
</Border>
```
Vaml DPs: `BorderBrush, BorderThickness, CornerRadius`.

### Shapes — `Rectangle/Ellipse/Line → Shape → FrameworkElement`
```xml
<Rectangle Fill="LightGray" Stroke="Black" StrokeThickness="1"/>
<Ellipse Fill="Red"/>
<Line X1="0" Y1="0" X2="10" Y2="10" Stroke="Blue" StrokeThickness="2"/>
```
Vaml DPs: `Fill, Stroke, StrokeThickness`. (Full WPF shape surface incl. `StrokeDashArray`,
`RadiusX/Y`, `Data` → PLANNED, 11k-8/11k-9 closure.)

### `Image` — `Image → FrameworkElement`
```xml
<Image Source="res/icon.png" Stretch="Uniform"/>
```
Vaml DPs: `Source, Stretch` (+ Vaml `IconKey`/`TintMode` — see §6).

### `PasswordBox` — `PasswordBox → Control`
```xml
<PasswordBox Password="…" MaxLength="20" PasswordChar="●"/>
```
Vaml DPs: `Password, MaxLength, PasswordChar`.

### `GroupBox` — `GroupBox → HeaderedContentControl → ContentControl`
```xml
<GroupBox Header="Details">…</GroupBox>
```
Vaml DP: `Header`.

### `Expander` — `Expander → HeaderedContentControl → ContentControl`
```xml
<Expander Header="More" IsExpanded="True">…</Expander>
```
Vaml DPs: `Header, IsExpanded`.

### `Separator` — `Separator → Control`
```xml
<Separator/>
```
Vaml DP: `Thickness`.

### `StatusBar` — `StatusBar → ItemsControl`
```xml
<StatusBar DockPanel.Dock="Bottom"><TextBlock Text="Ready"/></StatusBar>
```
Vaml DP: `Dock` (as a docked item).

### `Viewbox` — `Viewbox → Decorator → FrameworkElement`
```xml
<Viewbox Stretch="Uniform" StretchDirection="Both">…single content…</Viewbox>
```
Vaml DPs: `Stretch, StretchDirection`; single-content child; classifies to NONE ("its content").

### `ToolBar` — `ToolBar → HeaderedItemsControl → ItemsControl`
```xml
<ToolBar Orientation="Horizontal" Band="0" BandIndex="0"><Button/></ToolBar>
```
Vaml DPs: `Orientation, Band, BandIndex, IsOverflowOpen, HasOverflowItems`.

### `ToolBarTray` — `ToolBarTray → FrameworkElement`
```xml
<ToolBarTray IsLocked="False"><ToolBar/></ToolBarTray>
```
Vaml DP: `IsLocked`.

### `Application` — `Application → DispatcherObject`
```xml
<Application StartupUri="MainWindow.xaml"/>
```
Vaml DP: `StartupUri`.

---

## 4. Resources & value types (authoring the `...Themes.Colors` / `Theme.FontFamily` / `Themes.Layouts` / `AddPhysicianWindow` patterns)

```xml
<Window.Resources>
  <ResourceDictionary>
    <!-- value elements (11k-3) -->
    <Color      x:Key="Theme.Accent">#FF2E7AF2</Color>
    <FontFamily x:Key="Theme.Font">Segoe UI</FontFamily>
    <Thickness  x:Key="Theme.Padding">12</Thickness>
    <Double     x:Key="Theme.IconSize">24</Double>
    <GridLength x:Key="Theme.Split">2*</GridLength>
    <!-- object resources (11k-4 brushes) -->
    <SolidColorBrush  x:Key="Brush.Accent" Color="#FF2E7AF2"/>
    <LinearGradientBrush x:Key="Brush.Gradient">
      <LinearGradientBrush.GradientStops>
        <GradientStop Color="#FFFFFFFF" Offset="0"/>
        <GradientStop Color="#FF2E7AF2" Offset="1"/>
      </LinearGradientBrush.GradientStops>
    </LinearGradientBrush>
  </ResourceDictionary>
</Window.Resources>
```

Consumption: `Background="{StaticResource Brush.Accent}"`, `Foreground="{StaticResource Theme.Accent}"`.

---

## 5. Styles, setters, triggers (S454 — authored from .vaml)

```xml
<Window.Resources>
  <ResourceDictionary>
    <Style x:Key="PrimaryButton" TargetType="Button">
      <Setter Property="Width" Value="80"/>
      <Setter Property="Background" Value="{StaticResource Brush.Accent}"/>
      <Style.Triggers>
        <Trigger Property="IsMouseOver" Value="True"><Setter Property="Width" Value="90"/></Trigger>
      </Style.Triggers>
    </Style>
    <!-- implicit style: no x:Key -->
    <Style TargetType="Button">…implicit…</Style>
  </ResourceDictionary>
</Window.Resources>
```
- Implicit style (no `x:Key`) must declare `TargetType` (keyed `Style:{TargetType}`).
- `Style BasedOn`, `Setter`, `PropertyTrigger`, `DataTrigger` (S454), `EventTrigger` (11k-5).

### ALL trigger forms for a control (element-level `<Owner.Triggers>` — the DAG trigger family)

DAG rows: `TriggerBase`, `Trigger` (Property), `DataTrigger`, `EventTrigger`, `SetterBase`/`Setter`,
`TriggerAction`, `BeginStoryboard`, `Storyboard` (see `docs/DAG_controls.md`).

```xml
<Button.Triggers>
  <!-- 1) Property trigger (WPF `Trigger`) — fires on a DP == Value -->
  <Trigger Property="IsMouseOver" Value="True">
    <Setter TargetName="Host" Property="Opacity" Value="0.5"/>
  </Trigger>

  <!-- 2) Data trigger — fires on a binding == Value -->
  <DataTrigger Binding="{Binding Ready}" Value="True">
    <Setter TargetName="Host" Property="Opacity" Value="1.0"/>
  </DataTrigger>

  <!-- 3) Event trigger — ACTIONS-based, NOT setter-based (WPF-faithful). Fires on a RoutedEvent.
       The Actions payload (a TriggerAction — e.g. BeginStoryboard → Storyboard) is authored by the
       11e/11f/11g forward tokens (NO-GAP), not silently deferred. -->
  <EventTrigger RoutedEvent="Button.Click">
    <!-- Actions collection: concrete TriggerAction/Storyboard authored by 11f (TriggerAction) + 11g
         (BeginStoryboard → Storyboard/DoubleAnimation) tokens; 11e covers container/menu/tooltip/popup
         RoutedEvents. -->
  </EventTrigger>
</Button.Triggers>
```
- `EventTrigger` requires `RoutedEvent` (`SourceName` optional). At 11k-5 the container + `RoutedEvent`/
  `SourceName` parse into the S362 trigger model (`TriggerKind::Event`); the `Actions` content is the
  `TriggerAction` family owned by the 11e/11f/11g tokens + S510's `TriggerAction::Invoke` evaluation.

### Behaviors — the Microsoft.Xaml.Behaviors interactive surface (S502, 11f)

DAG rows: `Behavior`/`Behavior<T>`, `BehaviorCollection`, interactivity `TriggerAction`,
`InvokeCommandAction`, interactivity `EventTrigger` (the `i:`/`behaviors:`/`dd:` interaction surface;
see `docs/DAG_controls.md` + the EventTrigger DISAMBIGUATION).

```xml
<Button xmlns:i="http://schemas.microsoft.com/expression/2010/interactivity">
  <i:Interaction.Behaviors>
    <!-- An interactivity EventTrigger (EventName, NOT the System.Windows Style.Triggers one) -->
    <i:EventTrigger EventName="Click">
      <i:InvokeCommandAction Command="SaveCommand" CommandParameter="doc1"/>
    </i:EventTrigger>
  </i:Interaction.Behaviors>
</Button>
```
- VAML maps the WPF `Behavior<T>` base → `TypedBehavior<T>` (the C++ namespace cannot name a
  non-template `Behavior` and a `Behavior<T>` template together).
- `Behavior` (abstract) holds the attach/detach lifecycle + associated object.
- `BehaviorCollection` is the `i:Interaction.Behaviors` attached list (owned, ordered, attach-on-add).
- `TriggerAction` (interactivity, abstract) is the action base; `InvokeCommandAction : TriggerAction`
  executes a bound `ICommand` with a `CommandParameter`, gated on `CanExecute` (WPF-faithful).
- `InteractivityEventTrigger : Behavior` subscribes the element's ROUTED event named by `EventName`
  (the S320/S321 engine) and fires its ordered `TriggerAction`s when the event raises — the
  S502 headline "the behavior fires the command on the routed event."
- The interactivity `EventTrigger` is DISTINCT from the `System.Windows` `Style.Triggers` `EventTrigger`
  (D3); the System.Windows one stays PLANNED (S559 full re-apply).

---

## 6. Vaml-specific surface (declared, NOT hidden)

| Element | Purpose | Fidelity note |
|---------|---------|---------------|
| `Icon` | Raster icon drawn as an atlas quad, tinted at sample in the shader | **Vaml-invented — no WPF `Icon`.** Declared `OUT-OF-SCOPE(WPF)` / Vaml-only. |
| `Image.IconKey` | Key-based icon indirection | **Vaml-invented** — WPF `Image` uses `Source` (`ImageSource`), not a key. |

These are categorically different from the WPF gap: they are *additions*, not omissions, and are
documented as such so nobody mistakes them for WPF fidelity.

---

## 7. Attached properties (index)

| Attached property | Owner | Applies to |
|-------------------|-------|-----------|
| `Grid.Row/Column/RowSpan/ColumnSpan` | Grid | Grid children |
| `Canvas.Left/Top` | Canvas | Canvas children |
| `DockPanel.Dock` | DockPanel | DockPanel children |
| `ToolBar.Band/BandIndex` | ToolBar | ToolBar items (DAG: ToolBar) |

---

## 8. Events (implemented surface)

| Event | Owner | Kind |
|-------|-------|------|
| `Click` | Button / RepeatButton | routed |
| `Scroll` (bubbling) | ScrollBar | routed |
| TextChanged, selection, input events | TextBox-family | PLANNED (input surface, 11k-11) |
| `Loaded/Unloaded`, mouse/keyboard | FrameworkElement / UIElement | PLANNED (11k-11) |

---

## 9. Sprint-11n — DAG-anchored binding / input / themes / observables / 3D / visuals additions

> **Status: the DAG rows are `PLANNED` (added at **11n-0**, the DAG update delivered before the roadmap
> commit).** The underlying IMPLEMENTATION steps **11n-1…11n-6** flip each row `PLANNED → DONE`. Sections
> below are the authoring encyclopedia for the Sprint-11n additions; they are **declared now, not hidden**,
> exactly as the appendix's own rule requires. Cross-referenced to the `DAG_controls.md` 11n-0 cluster.

### 9.1 Automation (11n-1) — `AutomationProperties` (static attached props)

`AutomationProperties` is a static attached-property class (reuses the DP/PropertyId machinery; no new base).
Attached DPs a corpus doc can author on any `UIElement`/`FrameworkElement`:

```xml
<Button AutomationProperties.Name="Save"
        AutomationProperties.AutomationId="btnSave"
        AutomationProperties.HelpText="Persists the document"/>
```
Attached props: `Name`, `AutomationId`, `HelpText`, `ItemStatus`, `ItemType`, `LabeledBy`,
`AcceleratorKey`, `AccessKey`, `IsColumnHeader`, `IsRowHeader` (all `PLANNED`, 11n-1).

### 9.2 Input / commands (11n-2) — the `ICommand` spine

WPF truth: `ICommand` (Executor), `CommandManager` (static routed-command table + requery),
`InputGesture` → `KeyGesture`/`MouseGesture`, `InputBinding`(ICommandSource) → `KeyBinding`/`MouseBinding`.
VAML upgrades `ButtonBase` (Button/RepeatButton/ToggleButton/MenuItem) to the `ICommandSource` surface
(`Command`/`CommandParameter`/`CommandTarget` become the real command object, not a string DP):

```xml
<Button Command="App.SaveCommand" CommandParameter="doc1"/>
<Button.CommandBindings>
  <CommandBinding Command="App.SaveCommand" Executed="OnSave" CanExecute="OnCanSave"/>
</Button.CommandBindings>
<Window.InputBindings>
  <KeyBinding  Command="App.SaveCommand" Key="S" Modifiers="Control"/>
  <MouseBinding Command="App.OpenCommand" MouseAction="LeftClick"/>
</Window.InputBindings>
```
Types: `ICommand` (interface), `RoutedCommand : ICommand`, `CommandBinding`, `CommandManager`,
`ICommandSource`, `InputGesture` (abstract) → `KeyGesture`/`MouseGesture`, `InputBinding` → `KeyBinding`/
`MouseBinding` (all `PLANNED`, 11n-2).

### 9.3 Templates (11n-3) — `FrameworkTemplate` + `ItemsPanelTemplate`

`FrameworkTemplate` becomes the explicit parent of `ControlTemplate`/`DataTemplate` and gains
`ItemsPanelTemplate` (the `ItemsControl.ItemsPanel` recipe):

```xml
<ListBox>
  <ListBox.ItemsPanel>
    <ItemsPanelTemplate><VirtualizingStackPanel/></ItemsPanelTemplate>
  </ListBox.ItemsPanel>
</ListBox>
```
Types: `FrameworkTemplate → DispatcherObject, INameScope`, `ItemsPanelTemplate → FrameworkTemplate`
(`PLANNED`, 11n-3).

### 9.4 Binding / observables (11n-4 — OPTION A: true WPF event-subscription fidelity)

> **Roadmap note (per directive, recorded verbatim):** this step works CLOSELY WITH A HUMAN for the
> locking/threading reasoning. **Locks can be engineered out, but not always.** Single-threaded UI paths
> avoid locks; the subscription-lifetime weak-handler re-entrancy (a collection mutated mid-enumeration in
> a callback) is where a lock may be unavoidable — that decision is made WITH A HUMAN, never silently.
> ADR-018 is deliberately renegotiated here with human sign-off.

The reactive spine VAML lacked (observable-collection infrastructure) lands here. `{Binding}` becomes
**reactive** — a bound `ItemsControl` re-renders when its source collection changes:

```xml
<Window.Resources>
  <ResourceDictionary>
    <ObservableCollection x:Key="Logs" ItemType="String"/>
  </ResourceDictionary>
</Window.Resources>
<ListBox ItemsSource="{Binding Logs}" ItemTemplate="{Binding}"/>
<!-- mutating the source (a PropertyChanged/CollectionChanged event) re-runs the bind + re-renders -->
```
Types: `INotifyPropertyChanged`/`INotifyCollectionChanged` (interfaces), `NotifyCollectionChangedEventArgs`
(+ `CollectionChangedAction`), `ObservableCollection<T>` (VAML-native over `Value::Collection`), `Freezable`,
`DispatcherObject`, `IWeakEventListener` (weak subscription lifetime), `BindingBase`(+`Binding`/`MultiBinding`/
`PriorityBinding`), `BindingExpressionBase`/`BindingExpression`, `IMultiValueConverter`,
`ICollectionView`/`CollectionView` (all `PLANNED`, 11n-4).

### 9.5 3D (11n-5) — `Viewport3D` / `Model3D` / `Visual3D` / `Geometry3D`

`Viewport3D : FrameworkElement, IAddChild` hosts modeled content; `Model3D : Animatable`, `Geometry3D :
Animatable`, `Visual3D : DependencyObject, IVisual3DContainer`; camera/projection via `Camera`/
`PerspectiveCamera`. Headless software rasterization runs BOTH lanes; GPU is the [both-apps, not CI] path:

```xml
<Viewport3D>
  <Viewport3D.Camera>
    <PerspectiveCamera Position="0,0,5" LookDirection="0,0,-1" FieldOfView="60"/>
  </Viewport3D.Camera>
  <!-- Model3D/Geometry3D content -->
</Viewport3D>
```
Types: `Visual3D`, `IVisual3DContainer`, `Viewport3D`, `Model3D`, `Geometry3D`, `Animatable`, `Camera`,
`PerspectiveCamera` (all `PLANNED`, 11n-5; also corrects the pre-existing `ViewPort3D` row spelling in the DAG).

### 9.6 Visuals / adorners (11n-6 — done LAST) — `Adorner` / `VisualTreeHelper`

`VisualTreeHelper` forces a real visual-tree model that collides with the ADR-011 classifier/encode
architecture — deliberately its own step. Testing split: CPU-side (tree walk, `GetParent`/
`GetChildrenCount`/`GetChild`, `AdornerLayer` attach/detach, hit-test data) unit-tested WITHOUT GPU in the
both-lane suite; the render side unit-tested WITH GPU as the [both-apps, not CI] lane:

```xml
<AdornerLayer>
  <!-- adorner content hosted over an adorned element -->
</AdornerLayer>
```
Types: `Adorner : FrameworkElement`, `AdornerLayer`, `VisualTreeHelper` (static walk) (all `PLANNED`, 11n-6).

> **After 11n-6 + the 11n-7 wrap**, the DAG + this appendix are the EXHAUSTIVE, WPF-true survived surface,
> with the observable spine (9.4), the ICommand spine (9.2), 3D (9.5), and the visual-tree/adorners (9.6)
> no longer "conspicuously missing."

