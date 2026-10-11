# Appendix A — The Glossary: C#/XAML → VAML C++, What Changed and What Didn't

> **The Part of "Ten Things That Changed (and Ten That Didn't)."** A fast mapping table for a C#/WPF/MVVM
> developer. This is not a dictionary of VAML or of C++ generally — it maps the terms you *already use* to
> the VAML C++ term, and — the part really worth your attention — it flags the true mental-model changes.

**Evidence:** consolidated from Ch.1–9; each entry cites the chapter + ROADMAP token where its behavior is
defined. Symbols named here must resolve in the public header facade (`S921`) — verified by the guide CI
gate (`S951`).

---

## The Ten That (Mostly) Didn't Change — 1:1, Just New Spellings

| In C# / WPF you knew | In VAML C++ you write | See |
|---|---|---|
| XAML markup | `.vaml` markup (the VAML dialect) | Ch.2 / S122–S123 |
| `x:Name` / `x:Key` / `x:Class` | the same `x:` names | Ch.2.4 |
| `{Binding ...}` | `{Binding ...}` (VAML extension) | Ch.4 / S246 |
| `{StaticResource ...}` | `{StaticResource ...}` | Ch.2.6–2.7 / S3xx |
| `Style` + `Setter` (explicit + implicit) | the same | Ch.2.6, Ch.6.4 / S3xx |
| `ControlTemplate` + `TemplateBinding` + `ContentPresenter` | the same | Ch.6.5 / S3xx |
| `RoutedEventArgs.Handled` | `vaml::routed_event_args::mark_handled()` / `handled()` | Ch.5.3 / S323–S324 |
| `InitializeComponent()` | `vaml::markup::load(this, "X.vaml")` | Ch.3.5, Ch.7.4 / S947 |
| layout `Measure`/`Arrange` | the same two-phase contract | Ch.6.3 / S52x |
| `DataContext` | `set_data_context(...)` / `data_context<T>()` | Ch.3.5, Ch.4.2 / S246 |

> ◇ **Remember:** the markup layer and the MVVM *ideas* transfer almost 1:1. If it felt like "just XAML with
> a new accent," you're reading the right table. Now the other table.

## The Ten That Changed — the Mental-Model Hall of Fame

| In C# / WPF | In VAML C++ | What changed | See |
|---|---|---|---|
| GC keeps objects alive | you own lifetime (composition root, `unique_ptr`/raw/`shared_ptr`) | ownership is explicit | Ch.3.4, Ch.8.4 / ADR-006 |
| class = reference; property is an instance member | value semantics: binding holds a value; `set` stores + notifies | value vs reference | Ch.3.2 |
| `ObservableCollection` mutation + collection-changed | replace a `std::vector` value wholesale + notify | value vs reference | Ch.9.2 |
| DI container | static CompositionRoot in `main` | no runtime container | Ch.3.4 / ADR-006 |
| `async void` + `await` | `Task` + `.then(...)`/`.then_on(...)` continuations | no `await` keyword | Ch.5.5 / S326, S720 |
| one hidden UI thread | UI thread(s) + render thread + worker-thread layout | threading is overt | Ch.8 / ADR-002 |
| dispatcher marshals for you | you pick the context (`then_on(ui_context())`) | marshalling is explicit | Ch.8.3 / S724 |
| runtime `XamlParseException` | **compile-time** `vamlcc` diagnostic | errors move earlier | Ch.7.2 / S826 |
| `ICommand` / `IValueConverter` interfaces | `relay_command` object + converter *callable* | interfaces → callables | Ch.3.3, Ch.4.4 / S252, S242 |
| WPF ships it for you | two paths (interpreted/compiled) + you choose static vs shared | distribution is your call | Ch.7 / S2, S827 |

> 💡 **Tip — the mnemonic for the ten that changed:** **Own → Value → Container → Continuations → Threads**.
> Own it (ownership), treat it as a value (set-to-notify), you are the container (composition root), async
> is continuations (no await), and threading is overt (say which thread). Chapter 3, 5, and 8 *are* those
> five words.

## Did You Not Find Your Term?

Prefer the **public facade (`S921`)** and the **v1 control-set doc (`S920`)** over guessing. VAML reserves
the right to name things its own way, and the guide only states what is *implemented* — never a
would-be-nice feature. If it's not here and not in `S920`/`S921`, treat it as "check before you commit,"
not "must be missing."

**Next up:** Appendix B — one screen to pin next to your monitor, and the ten ways new arrivals hurt
themselves. You've earned it.
