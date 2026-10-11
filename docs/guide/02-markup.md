# Chapter 2 — Markup: The Dialect You Actually Speak

> ### In This Chapter
> - Why VAML markup feels like XAML but isn't XAML
> - Taking the pulse of elements, properties, and attached properties
> - `x:Name`, `x:Key`, and `x:Class` — the `x:` family reunion
> - Styles, resources, and the markup extension `{ Binding }` you've met before
> - Why your typos now fail at **compile time** instead of Thursday afternoon

**Evidence:** S122/S123 (parser), S142 (properties), S242 (converters), S246 (binding), S252 (commands),
S3xx (styles/templates/resources), S942. Every snippet validated by `vamlcc` (S826) + the guide CI gate (S951).

---

You don't need to be *taught* markup. You need to know the **specific flavor** VAML uses and the few places
the dialect waves at XAML and then does its own thing. This chapter is that tour. No homework, just:
"here's the dialect, here's what's fresh, here's what'll bite."

## 2.1 The Curious Case of the Loyal Clone

> ### Remember
> **VAML is a loyal clone of XAML — not a re-skin, a clone.** Its whole job is to let you write markup the
> way you already know how, minus a decade of legacy baggage (no BAML tilde-twiddling, no ancient corners).

The one structural difference that actually *matters* to you as an author: **element names resolve to C++
types, not CLR types.** When you read `<Button>`, the machinery is asking "which C++ type is named `Button`
and is it in this namespace?" — you barely feel it while reading markup, but it changes two things:

- **How types are spelled** (they follow C++ naming).
- **Where you find them** (C++ namespaces/headers via the `using:` scheme — see 2.2), because C++ has no
  reflection assembly to go fishing through.

That's the whole "worlds apart in *path*" you heard someone mumble. Not hard — just *where things live*.

## 2.2 The Root, the Namespaces, and `using:`

```vaml
<UserControl x:Class="MyMainWindow"
             xmlns="http://schemas.vaml.dev/ui"
             xmlns:local="using:myapp.controls">
```

- The default `xmlns` maps unprefixed names (`Button`, `Grid`) to the VAML UI schema.
- `xmlns:local="using:myapp.controls"` maps the `local:` prefix to C++ types in `myapp::controls`.
  This is your `clr-namespace` analog — and the `using:` scheme is how C++ does it, because C++ types live
  in namespaces + headers rather than a metadata assembly you can reflect over.

> ⚙ **Technical Stuff:** That "worlds apart in path" idea, in one line: C# finds a type by scanning an
> assembly's metadata; C++ finds a type by **taking the header's word for it during compilation**. The
> `using:` prefix tells the compiler which namespace/header to trust. Everything after that is identical.

## 2.3 Elements, Properties, and Attached Properties

```vaml
<StackPanel Orientation="Horizontal">
    <Button Content="Save" Command="{Binding SaveCommand}"/>
    <TextBox Text="{Binding Name, Mode=TwoWay}"/>
    <Grid.RowDefinitions>...</Grid.RowDefinitions>   <!-- attached property on the owner -->
</StackPanel>
```

- **Simple property:** `Orientation="Horizontal"` → sets `StackPanel::orientation`.
- **Attached property:** `Grid.RowDefinitions=...` → the *owner* (`Grid`) declares a property any child may
  set; `Grid` reads it when arranging.
- **Content (nested element):** a child inside `StackPanel` becomes content — here, the children collection.
- **Element-property syntax** (a property given a child element) works for complex values:
  ```vaml
  <Button>
      <Button.ToolTip><ToolTip Text="Save the file"/></Button.ToolTip>
  </Button>
  ```

If you've written XAML, you just read this in two seconds and nodded. Good — that's the whole point.

## 2.4 The `x:` Family Reunion

| `x:` name | What it does | Analogy |
|---|---|---|
| `x:Name` | Names an element you can grab from code-behind | `FindName` field in WPF |
| `x:Key` | Names a **resource** (2.6) | WPF/`ResourceDictionary` key |
| `x:Class` | Names the C++ code-behind *owner* type for this markup | the `partial class` in WPF |

```vaml
<TextBox x:Name="InputBox" Text="{Binding Name, Mode=TwoWay}"/>
```

From code-behind you fetch it with `Window::find<vaml::text_box>("InputBox")` — the same "grab by name"
trick you used in WPF, only with a template type instead of a reflection result.

## 2.5 Markup Extensions: The `{ Curly Brace }` People

```vaml
<TextBlock Text="{Binding Greeting}"/>
<Button Background="{StaticResource AccentBrush}" Content="Go"/>
```

- `{Binding ...}` → a *live* link to a property on the current `DataContext` (Chapter 4 owns all this).
- `{StaticResource Key}` → resolves a resource defined higher in the tree, once, at load time.

That's it for the everyday two. (Converters are Chapter 4's business; templates are Chapter 6's.)

## 2.6 Styles: Explicit and Implicit

```vaml
<FrameworkElement.Resources>
    <Style TargetType="Button" x:Key="PrimaryButton">
        <Setter Property="Foreground" Value="#FFFFFF"/>
        <Setter Property="Margin" Value="4"/>
    </Style>
    <Style TargetType="TextBlock">
        <Setter Property="FontSize" Value="14"/>   <!-- NO x:Key → implicit style -->
    </Style>
</FrameworkElement.Resources>

<Button Style="{StaticResource PrimaryButton}" Content="OK"/>
```

- An **explicit** style has `x:Key` and is applied on demand via `{StaticResource PrimaryButton}`.
- An **implicit** style has *no* `x:Key` and quietly applies to **every** element of its `TargetType` below
  it in the tree. It's the "make all buttons look like this" button.

> ⚠ **Warning:** implicit styles only match *exact* `TargetType` — a subclass won't catch it. If your
> implicit style "isn't working," nine times out of ten it's this. Give it a `x:Key` and use `StaticResource`,
> or make the type exact.

## 2.7 Resources: The Family Hand-Me-Down

Resources are name→value maps scoped to an element and **inherited by its descendants**. Key resolution
walks **nearest first**, then up the tree — the same shadow-the-parent semantics you know from WPF. A
template's resources are scoped *to that template* (Chapter 6), so templates stay self-contained.

## 2.8 Why Your Typos Now Fail at Compile Time (The Good Kind of Failure)

> ### Remember
> In WPF you met `XamlParseException` at **runtime** — your app shipped, ran, and then exploded on a
> customer's machine because a `{Binding}` was misspelled.
>
> VAML compiles markup with `vamlcc`. A typo — unknown element, unknown property, missing handler — is a
> **compile-time error with line, column, and a suggested fix** (S826).

If you've felt the low-grade dread of "I hope I didn't typo an attribute in markup," feel it dissolve. Your
Friday is now free.

## 2.9 The "You Write XAML, I Wrote VAML" Trap Table

| You wrote | The reality | The fix |
|---|---|---|
| `Image Source="/assets/x.png"` | raw filesystem paths aren't a thing | use a pack-style URI or a resource |
| `{Binding Path=Name}` vs `{Binding Name}` | both are fine | breathe; no fix needed |
| Implicit style didn't apply | implicit only matches exact `TargetType` | `x:Key` + `StaticResource`, or match exactly |
| Event in markup "unknown" | handler must **exist on the owner** (Ch.5) | declare `void on_click(...)` on the `x:Class` |

## 2.10 In This Chapter, You Learned…

- VAML is a **fresh, loyal clone** of XAML's authoring surface — comfortable by design.
- `using:` namespaces map prefixes to **C++ types** (the `clr-namespace` analog, and the "path" difference).
- `x:Name`/`x:Key`/`x:Class` are your friends; attached properties and element-property syntax work as you'd hope.
- Styles: **explicit** (`x:Key`) vs **implicit** (automatic, exact-type); resources inherit nearest-first.
- Markup typos are **compile-time** errors now, not Thursday-afternoon runtime surprises.

_All markup in this chapter is validated by `vamlcc` in the guide CI gate (S951)._
