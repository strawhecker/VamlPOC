# Appendix B — The Cheat Sheet & Ten Common Mistakes

> **The Part of Tens, part two.** One screen to pin next to your monitor (the cheat sheet), and the honest
> list of the ten ways tourists hurt themselves on day one — so you don't have to. Plus, where each piece
> lives in the roadmap, in case the cheat sheet makes you hungry for the receipts.

**Evidence/prereq:** symbols must resolve in the public facade (`S921`); verified by the guide CI gate
(`S951`). This sheet states only *implemented* behavior.

---

## The Cheat Sheet — Pin This Next to Your Monitor

### The Four Rules (the whole book in one breath)

| # | Rule | See |
|---|---|---|
| 1 | **Own it.** Composition root owns VMs (`unique_ptr`); views get raw non-owning pointers; wired objects are move-only. | Ch.3.4 / Ch.8.4 |
| 2 | **Set to notify.** Change notification only happens through `property<T>::set(...)` (or whole-value replace). | Ch.3.2 |
| 3 | **Continuations, not await.** `Task` + `.then(...)`; touch UI only via `then_on(ui_context(), ...)`. | Ch.5.5 / Ch.8.3 |
| 4 | **Errors at compile time.** Markup/types/handlers/bindings that don't resolve are `vamlcc` errors. | Ch.7.2 |

### The ViewModel Skeleton

```cpp
class my_vm : public vaml::observable_object
{
public:
    my_vm() = default;                       // move-only once wired
    my_vm(my_vm const&) = delete;
    my_vm& operator=(my_vm const&) = delete;

    vaml::property<std::string> name { "name", this };
    vaml::relay_command         save { [this]{ save_impl(); }, [this]{ return can_save(); } };
};
```

### The Code-Behind Owner Skeleton

```cpp
class main_window : public vaml::user_control
{
public:
    explicit main_window(my_vm* vm)
    {
        vaml::markup::load(this, "MainWindow.vaml");
        set_data_context(vm);
    }
    void on_save(vaml::routed_event_args const& e);
};
```

### Startup

```cpp
int main(int argc, char** argv)
{
    vaml::App app(argc, argv);
    auto vm  = std::make_unique<my_vm>();
    auto win = std::make_unique<main_window>(vm.get());
    return vaml::run(app, std::move(win));   // runtime owns the window
}
```

### Markup Cheat-Sheet

```vaml
<UserControl x:Class="main_window" xmlns:local="using:myapp">
  <Button Click="on_save" Command="{Binding save}"
          Style="{StaticResource Primary}"/>
  <TextBox Text="{Binding name, Mode=TwoWay}"/>
  <TextBlock Text="{Binding price, Converter={StaticConverter price}}"/>
</UserControl>
```

### Async Cheat-Sheet

```cpp
repo().fetch().then_on(vaml::ui_context(), this,
    [this](vaml::result<rows> r) { if (r) bind(*r); else err(r.error()); });
```

---

## The Ten Common Mistakes (Read These, Save a Tuesday)

1. **Not using `set` to notify.** Bound text stays empty because you assigned a member directly. Rule 2.
2. **Keeping a raw copy of a window/VM you passed to the runtime.** It dangles at shutdown. Own-or-look.
3. **Freeing the VM before the view that observes it.** The Chapter 3 warning, in the flesh.
4. **Running a UI-touching continuation on a worker thread.** Frozen UI, wrong context. `then_on(ui_context())`.
5. **Forgetting `mark_handled()`.** The event keeps bubbling and a parent handler you forgot steps on you.
6. **`*` vs `Auto`.** A row collapses to zero and you blame layout. Star fills, Auto hugs.
7. **An implicit style that "doesn't work."** Implicit only matches exact `TargetType`. Give it a `x:Key`.
8. **A converter with mutable captured state.** The binding re-runs it; stateful = nondeterministic. Pure.
9. **Choosing shared linking "just in case."** Now you ship a haunted house of runtimes. Static unless you
   chose shared for a reason.
10. **Fighting the v1 control set.** The control you "need" may be post-1.0 (`S927`). Compose; don't reinvent.

> ⚠ **The meta-mistake:** all ten are one habit wearing ten coats — **"C# habits assumed to hold in VAML."**
> Run the four rules and you sidestep the whole list.

---

## Where Each Piece Lives in the Roadmap

| Concern | Chapter | Roadmap |
|---|---|---|
| Ownership / DI | 3, 8 | ADR-006, S920/S921 |
| Properties / commands | 3 | S142, S241, S252 |
| Binding / converters | 4 | S242, S246 |
| Events / async | 5 | S322–S326, S720 |
| Threads | 8 | ADR-002, S620–S628, S724 |
| Compiled path | 7 | S2, S826, S827, S947 |
| Guide gate / ship | — | S951, S952, S953 |

> And if you only remember one sentence from all twelve files, let it be this:
>
> **"Set to notify. Own what you create. Continuations, not await. Errors at compile time."**
>
> Welcome to VAML. You were never really a dummy — you were just missing the map.
