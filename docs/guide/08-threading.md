# Chapter 8 — Threading: There's More Than One Thread, and They Don't Talk Much

> ### In This Chapter
> - Why there's no single "UI thread" to hide behind anymore
> - UI thread vs render thread vs worker threads — the cast of characters
> - Marshalling onto the UI thread without losing your lunch
> - Why thread-affinity is *enforced* (and why that's a gift, not a tax)
> - Threading + ownership: the double act that ends C#'s "it just works"

**Evidence:** ADR-002 (threading model), S620–S629 (async layout/worker threads, TSan-clean), S720
(cancellation), S724 (thread-affinity enforcement), S725 (teardown ordering), S626 (UI threads × render
ring scale-out). Snippets compiled in CI (S951).

---

Here's the news that rearranges your furniture: **you've been spared thinking about threads your whole C#
career, and VAML gently, firmly, stops sparing you.** Not because it's mean — because it can afford to run
a renderer that's actually fast. Once you know the cast of characters, it's not scary. It's just *more
explicit*.

## 8.1 The C# You Knew: One Thread, Kept Secret

One UI thread. `Dispatcher` marshalled onto it. `async`/`await` resumed on it. WPF enforced access on the
creation thread. You rarely even had to *think* "which thread am I on?" — the answer was always "the UI one."

VAML says: "that single-threaded renderer hits a wall on a modern GPU. We're splitting the work." And so:

## 8.2 Meet the Cast of Characters

VAML is **threaded for performance by design** (ADR-002), and the split is explicit — that's the whole plot:

- **UI thread(s)** — own the element tree, bindings, commands, input, and **your code-behind handlers**.
  This is the thread that runs the events loop (`vaml::run`, Ch.1).
- **Render thread** — owns the GPU device, swapchain, texture uploads, and draw submission. **Not** the UI
  thread. This is the one doing the heavy lifting ADR-002 exists for.
- **Layout** — measure/arrange runs on **worker threads** (`S620` async layout) and is *committed* to the
  UI tree in a single transactional commit. Correctness must equal single-threaded layout (`S621`,
  diff-tested) and be **TSan-clean** (`S623`, `S628`).

Three consequences you'll *feel* as an author:

1. **You can't touch the visual tree from a continuation unless it's on the UI context.** (Chapter 5's
   "run it on the UI context" — now you know why.)
2. **Writes from the wrong thread are blocked — loudly.** VAML *actively enforces* thread-affinity (`S724`):
   a wrong-thread write is a clear error, not silent corruption. Annoying? Slightly. Better than shipping
   a data race? Infinitely.
3. **The render thread dictates some lifetimes.** GPU resources (textures, glyph caches) are owned by the
   render ring — you hand them over and **never mutate them after upload** (`S625`, `S626`).

> ### Remember
> **One question, one thread.** Before *any* mutation, ask "am I on the thread that owns this?" The UI tree
> answers "the UI thread." GPU resources answer "the render thread." If you're not sure you're on the right
> one, you're about to meet `S724`'s error message — which is the platform being kind enough to tell you
> before the data race does.

## 8.3 Marshalling Onto the UI Thread (The Polite Way Across Threads)

As in Chapter 5, async work returns via `Task` continuations — and **you choose the context**:

```cpp
repo().fetch().then_on(ui_context(), [this](vaml::result<rows> r) {
    // safe: on the UI thread, may touch the tree
    bind_rows(*r);
});
```

Use `then_on(ui_context(), ...)` (or the owner-context form) whenever the continuation touches the visual
tree or bindings.

> ⚠ **Warning — the difference between `.then` and `.then_on`:** a plain `.then(...)` runs on *whatever
> thread completed the task* — fine for pure compute that doesn't touch UI. The moment the continuation
> calls `bind_rows(*r)` or sets a property that updates a control, it **must** be `then_on(ui_context(), ...)`.
> Using the wrong one is the #1 cause of "my UI froze after a fetch."

> 💡 **Tip — the mental litmus test:** "does this continuation touch a control, a binding, or my code-behind
> owner?" Yes → `then_on(ui_context())`. No → plain `.then` is fine. One question. Every time.

## 8.4 Thread-Affinity, Ownership, and Why Lifetime Differs From C#

This is where threading and Chapter 3's ownership come together to deliver the knockout punch.

In C#: GC + dispatcher meant objects lived as long as references, and only the UI thread held the tree.
Easy. Unconscious. Done for you.

In VAML:

- Objects live as long as **you** say (composition root owns them, Ch.3.4).
- The **render thread may still reference resources** after your code returns. A GPU resource handed to the
  renderer must outlive the frames that reference it.
- Worker-thread layout snapshots parts of the tree; a mutation from the UI thread mid-measure is safe only
  because layout is **copy-on-write over hot nodes** (`S626`) **and** policy (your code) keeps the UI-thread
  rule.

The practical rules — your four-point survival kit:

1. **Own VMs in the composition root** (`unique_ptr`); views get **raw non-owning** pointers.
2. **Run UI-touching continuations on the UI context** (`then_on(ui_context())`).
3. **Don't mutate GPU resources** after handing them to the renderer.
4. **On shutdown, tear down in reverse order of construction** — children → parents, views → VMs, UI →
   render. Thread-affinity + teardown ordering (`S724`/`S725`) surface a wrong order *deterministically*,
   so you get told, not haunted.

> ⚠ **Warning — the double act:** ownership (Ch.3) and threading are not two separate chapters — they're
> the two halves of the same C++ truth. C# let you ignore both (GC handled liveness, dispatcher handled
> threading). VAML makes you state both. A "dangling observer" and a "wrong-thread write" are the same bug
> wearing two hats: *something that owned or touched this is no longer valid here.*

## 8.5 Scaling: More UI Threads (The Optional Upgrade)

`S626` supports multiple UI threads × a render ring for genuinely parallel UI surfaces. For ordinary
desktop apps, **one UI thread is correct** — the model *allows* more without you restructuring, but you
shouldn't reach for it. Prefer one unless a profiled need (`S627`) demands otherwise.

> 💡 **Tip:** "multi-UI-thread" sounds impressive at parties and is a liability in practice. The model
> *permits* it; v1 *recommends* one. Prove you need two (profiler), then go. Until then, one.

## 8.6 Troubleshooting, the "Why Am I Seeing Thread Errors" Table

| Symptom | Cause | Fix |
|---|---|---|
| Wrong-thread write error | touched the tree off the UI thread | marshal via `then_on(ui_context())` |
| Frozen UI on fetch | continuation ran on a worker | `then_on(ui_context())` (8.3) |
| Renderer error on exit | GPU resource freed too soon | don't mutate; teardown order `S725` |
| Data race in tests | worker layout vs UI mutation | rely on copy-on-write + `S621` diff invariant |

## 8.7 In This Chapter, You Learned…

- VAML is threaded by design: **UI threads** (tree/bindings/code-behind), **render thread** (GPU), and
  **worker threads** (layout) — with a single transactional commit (`S620`).
- Wrong-thread writes are **actively blocked** (`S724`) — get the context right up front.
- Continuations touch UI **only on the UI context**; GPU resources are owned by the render ring.
- **Lifetime is your decision**, and threading + ownership together are exactly why the C# mental model
  doesn't transfer directly. Own it; say which thread; tear down in reverse.

_This is the other chapter — alongside Chapter 3 — that makes porting either a joy or a vigil. Between them,
they ARE the C#→C++ gap._
