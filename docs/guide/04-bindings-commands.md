# Chapter 4 — Bindings & Commands: Making the UI and the Model Date

> ### In This Chapter
> - PropertyPath from the `DataContext` to your ViewModel's lapel
> - The four binding modes and which one you actually mean
> - **Converters are callables now** — the `IValueConverter` farewell tour
> - The two-way tango: a `TextBox` that talks *and* listens
> - Binding commands, and how a binding knows when to let go

**Evidence:** S246 (binding engine), S142 (property system), S242 (converters), S252 (commands),
S520–S529 (binding stress/property tests), S324/S4xx (lifecycle/detach). Snippets compiled in CI (S951).

---

A binding is a marriage counselor between two objects that don't want to talk directly: the UI and the
ViewModel. In C# you set them up with a markup extension and a bushel of trust. In VAML it's the same dance
with a new song — and converters have changed instruments.

## 4.1 The C# You Knew

```xml
<TextBlock Text="{Binding Greeting}"/>
<TextBox Text="{Binding Name, Mode=TwoWay}"/>
```

A binding watches a source property and pushes/pulls values per `Mode`. A converter implements
`IValueConverter` and gets instantiated as a resource. The source of all truth? The `DataContext`.

## 4.2 Meet the `DataContext`: Your Binding's Home Address

The `DataContext` is whatever object you set in the code-behind constructor (`set_data_context(vm)`, Ch.3).
Every binding resolves its `PropertyPath` against it, and nested contexts **inherit down the tree** — kind
of like XAML's "find the nearest context" rule, which you already know and love.

## 4.3 PropertyPath and the Four Modes

```vaml
<TextBlock Text="{Binding Greeting}"/>                              <!-- OneWay  (default) -->
<TextBlock Text="{Binding Order.Subtotal, Mode=OneTime}"/>          <!-- read once -->
<TextBox  Text="{Binding Name, Mode=TwoWay}"/>                       <!-- source AND target -->
<TextBlock Text="{Binding FallbackText, FallbackValue=—}"/>          <!-- source missing/unset -->
```

| Mode | C#/WPF | VAML | Meaning |
|---|---|---|---|
| `OneWay` | default | default | target updates when the source changes |
| `OneTime` | explicit | explicit | evaluated once at load, then I don't care |
| `TwoWay` | explicit | explicit | edits flow target→source (with validation) |
| fallback | `FallbackValue` | `FallbackValue` | shown when the source value is absent |

Dot paths traverse nested objects/property snapshots. If any hop is missing at load, the binding drops to
fallback — and it'll re-evaluate if the missing hop *later* appears (`S246` coalescing: rapid invalidations
merge into one correct final update, `S525`). So an empty text isn't always a null crime; sometimes it's a
"the path wasn't there yet" that self-heals.

## 4.4 Converters: The `IValueConverter` Farewell Tour

The C# way — a *class*, referenced as a resource:

```xml
<TextBlock Text="{Binding Price, Converter={StaticResource PriceConverter}}"/>
```

The VAML way — a **callable**, registered by name. There is no `IValueConverter` interface to implement.
You hand the binding a lambda:

```cpp
#include <vaml/binding.h>

// Convert: (value, target_type) -> target value
vm.set_converter("price", [](vaml::value const& v, vaml::type_id /*target*/) -> vaml::value {
    double cents = v.as<double>();
    return vaml::value(std::format("${:.2f}", cents / 100.0));
});
```

```vaml
<TextBlock Text="{Binding PriceCents, Converter={StaticConverter price}}"/>
```

> ⚠ **Warning — mental-model change:** a C# converter is a **named class** discovered by reflection and
> instantiated as a resource. A VAML converter is a **plain C++ callable** you register by name on the
> owner/scope. Two practical consequences:
> 1. Register it once, in the code-behind (see Ch.9 for where).
> 2. **Keep converters pure** — no captured mutable state. The binding can re-run a converter (it's
>    fuzz-tested, `S522`, and stressed, `S525`, on the assumption of purity), and a stateful converter will
>    quietly misbehave. Pure in, deterministic out.

> 💡 **Tip:** A two-way binding may offer a second function (ConvertBack, the reverse trip). Check the
> public header (`S920`) for the exact callable shape the release exposes for Convert/ConvertBack — the
> guide gate verifies it (S951), so you never have to guess.

## 4.5 The Two-Way Tango: Binding a `TextBox`

```vaml
<TextBox Text="{Binding Name, Mode=TwoWay}"/>
```

Wired up as in Chapter 3: `name` is a `vaml::property<std::string>`. When the user edits the box, the edit
flows through `name.set(...)` → `PropertyChanged` → any `OneWay` neighbors refresh. Update triggering,
coalescing, and validation are `S246`/`S524` territory.

And the rule that never changes, repeated on purpose:

> ### Remember
> **Go through `set` to notify.** If the TextBox's edit isn't reaching your ViewModel, your property isn't
> using `set`. One-line check, ninety-percent fix rate.

## 4.6 Binding Commands

```vaml
<Button Content="Save" Command="{Binding save}"/>
```

`save` is the `relay_command` from Chapter 3. The button queries `CanExecute`; when the command raises
`CanExecuteChanged`, the button re-queries and flips its enabled state. Bindability, invalidation, and
focus-after-invoke follow `S252` and the command machinery. If you've bound a WPF command, you've done this.
No new verbs.

## 4.7 The Awkward Talk About Letting Go (Detach and Lifetime)

Every binding holds a **subscription** to its source. Like every relationship, it needs to know when it's
over. Two rules, straight from the Chapter 3 ownership playbook:

- **The source must outlive the binding** — or the binding must be detached when the source dies first.
  The runtime's thread-affinity + teardown order (Ch.8, S724/S725) catches a wrong order, but *you* own the
  decision upfront.
- **Detach is explicit when you need it.** For a one-shot or long-lived-but-sometimes-null source, bind then
  detach in the code-behind owner's teardown — rather than leaking a subscription into a dead object.

> ⚠ **Warning:** a binding pointing at a freed source is the silent killer of porting. C# never made you
> think about it (GC kept the source alive — remember the GC, RIP). VAML does. When a binding misbehaves at
> shutdown, 9 times out of 10 the source died before the binding let go.

## 4.8 Troubleshooting, the "Why Is My Binding Empty" Table

| You see | Cause | Fix |
|---|---|---|
| Empty text, no error | source property never notified | use `property<T>::set` (Ch.3) |
| Fallback value shown | path hop missing at load | set `DataContext` before load; or set `FallbackValue` |
| Two-way edits don't persist | editing didn't call `set` | VM property must go through `set` |
| Binding leaks at teardown | no detach for a short-lived source | detach in owner teardown (4.7) |

## 4.9 In This Chapter, You Learned…

- `DataContext` + PropertyPath + modes map 1:1 to what you knew, minus legacy baggage.
- **Converters are callables**, not `IValueConverter` classes — register by name, keep them pure.
- Two-way goes through `property<T>::set`; commands bind via `relay_command` — no new verbs.
- **Lifetime:** the source outlives the binding, or you detach explicitly. Ownership is a Chapter 3 habit.
