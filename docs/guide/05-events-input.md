# Chapter 5 — Events & Input: When the User Pokes the Screen

> ### In This Chapter
> - How `Click="on_save_click"` becomes a handler on your `x:Class` owner
> - Routed events: bubbling, tunneling, and the almighty `mark_handled()`
> - Focus and keyboard (the polite table manners of input)
> - **The big one: async without `await`** — a world where you write continuations instead

**Evidence:** S322 (markup event wiring → owner handler), S323/S324 (routed events: bubbling/tunnel/handled),
S326 (Task/Executor async), S720 (cancellation), S527 (event-routing tests), S824 (keyboard/a11y).
Snippets compiled in CI (S951).

---

People click. People type. People click *while* networking. This chapter is how you hear them, how you
respond, and how you do it without freezing the screen — in a language where `await` packs its bags.

## 5.1 The C# You Knew

```xml
<Button Click="OnSave_Click" Content="Save"/>
```

```csharp
private void OnSave_Click(object sender, RoutedEventArgs e){ ... }
```

Handlers are methods connected by name; `async void` lets you `await` inside; routed events bubble and
tunnel with `e.Handled`. Comfortable, familiar, and three of those four things have a new accent now.

## 5.2 The Handshake: `Click="on_save_click"` → Your Code-Behind

The handler must be a **member of the `x:Class` owner class** (from Chapter 2/3), declared with the
routed-event signature:

```cpp
// MainWindow.h
class my_main_window : public vaml::user_control
{
public:
    void on_save_click(vaml::routed_event_args const& e);
};
```

```vaml
<Button Click="on_save_click" Content="Save"/>
```

The markup reference is resolved **by name against the owner** when markup loads (`S322`).

> 💡 **Tip — your typo is now a compile error:** If the handler doesn't exist, `vamlcc` refuses to build —
> a compile-time diagnostic (`S826`), not a runtime `XamlParseException` in front of a customer. Missing
> method? Caught at the door. (Chapter 2 promised you'd like this. Told you.)

## 5.3 Routed Events: Bubbling, Tunneling, and the Almightier `mark_handled()`

VAML implements the same three-phase model you know:

- **Bubbling** (default): child → parent, until a handler sets `e.handled`.
- **Tunneling** (preview): parent → child, before the bubble pass.
- **Handled**: `routed_event_args::mark_handled()` stops further propagation. (`S527` fuzz-tests this
  across random trees, so the edge cases are caught by CI, not shipped.)

```cpp
void my_main_window::on_save_click(vaml::routed_event_args const& e)
{
    if (e.handled()) return;      // someone upstream already got it — stay out
    // ... do the work ...
    e.mark_handled();             // stop the bubble here (Q: "should it keep going?")
}
```

> ⚠ **Warning:** Forgetting `mark_handled()` is the "leaky tap" of event handling — the event keeps bubbling
> and some parent handler you forgot about runs and steps on your work. If "why did *that* also fire?" is
> your Monday, you forgot the `mark_handled()`.

For programmatic wiring (non-markup), check the public facade (`S920`) for the `add_handler` overload set —
the guide gate verifies the names (S951).

## 5.4 Focus and Keyboard: Table Manners

- Tab order and focus follow the same model you expect from containers and controls (a11y completeness is
  `S824`).
- Key handling is a routed event (a tunneling preview + a bubbling pass). A control reports
  `IsKeyboardFocused`; bind focus-sensitive visuals to that instead of guessing.

Nothing wild here. Keyboard works the way you'd hope, and a11y is being graded as a real deliverable, not
an afterthought.

## 5.5 The Big One: Async Without `await`

Alright. The chapter-title promise. Deep breath.

> ⚠ **Warning — the sharpest mental-model change in the whole book:**
> **There is no `async`/`await` in VAML's UI model.**
>
> C++ doesn't give you a magical `await` that turns "paused here, resume later" into a keyword you sprinkle
> on a method. VAML models async work as a **Task** with **continuations** (`S326`, `S720`). You don't
> *pause and resume* a method — you say **"when this finishes, run this."**

The C# you knew:

```csharp
private async void Load_Click(object sender, RoutedEventArgs e)
{
    var data = await _repo.FetchAsync();
    Bind(data);
}
```

The VAML C++ you write:

```cpp
void my_main_window::on_load_click(vaml::routed_event_args const& /*e*/)
{
    vaml::task<std::vector<row>> t = repo().fetch();      // starts the async fetch
    t.then([this](vaml::result<std::vector<row>> r) {     // "when it finishes..."
        if (r) bind_rows(*r);                             // ...do the UI update
        else   show_error(r.error());
    });
}
```

Break it down — four things to internalize:

> ### Remember — the What's-What of Tasks
> 1. **Continuations, not awaiting.** `t.then(callback)` says "when `t` completes, run `callback`." No
>    pausing, no resuming, no stack-unwinding gymnastics.
> 2. **Context matters.** The continuation runs on the context *you ask for* — the UI/owner context (Ch.8),
>    so the callback can safely touch the visual tree. Pick the wrong context and you're off-thread.
> 3. **Cancellation is real.** `t.cancel()` is cooperative (`S720`) — it propagates, never blocks, and
>    teardown is deterministic. Prefer cancelling long tasks on owner teardown over leaking a callback into
>    a dead window.
> 4. **Chaining builds pipelines.** `t.then(a).then(b).on_error(...)` replaces the flat C# `await`-chain
>    with a pipeline. Same flow, new plumbing.

> 💡 **Tip — the one-sentence translation:** every place you used to write `await foo()`, you now write
> `foo().then([captures](auto r){ ... })` and put the "after" code inside the brackets. That's 90% of what
> `async`/`await`'d code translates to. The other 10% is remembering to run it on the right context (Ch.8)
> and to cancel on teardown.

## 5.6 Troubleshooting, the "Why Won't My Handler Fire / Why Is It Frozen" Table

| You see | Cause | Fix |
|---|---|---|
| `Click="foo"` won't compile | handler missing on the owner | declare `foo(vaml::routed_event_args const&)` on the `x:Class` |
| Bubbling continues past you | never marked handled | call `e.mark_handled()` |
| UI frozen after fetch | continuation ran on a worker | `.then` on the UI/owner context (Ch.8) |
| Callback into a dead window | task outlived owner, no cancel | cancel on teardown (`S720`) |

## 5.7 In This Chapter, You Learned…

- `Click="on_save_click"` resolves a **member of the `x:Class` owner**; missing handlers are **compile-time**.
- Routed events: bubbling + tunneling + `mark_handled()` — tested across random trees (`S527`).
- **Async is continuations, not `async`/`await`**: `Task` + `.then(...)`, on the right context, with
  cooperative cancellation. When in doubt: `foo().then(...)`, run it on the UI context, cancel on teardown.
