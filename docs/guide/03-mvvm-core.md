# Chapter 3 — MVVM Core in C++: The Part That Bites

> ### In This Chapter
> - `ObservableObject`: your `INotifyPropertyChanged`, minus the boilerplate, plus a plot twist
> - Why value semantics change how you think about a "property"
> - `RelayCommand`: the `ICommand` you already love, wearing a C++ coat
> - **Ownership:** the garbage collector left, and *you're* the new lifeguard
> - The `x:Class` code-behind owner: a view and an `InitializeComponent` in one trench coat

**Evidence:** S142 (properties/PropertyChanged), S241, S246 (binding), S252 (commands/RelayCommand),
S242 (converters), S322 (event wiring), S250 (static app-owned composition), S250_1 (binding channel
accessors), S251 (app-constructed VM with injected service). Every class snippet is a full TU
compiled in CI (S951).

---

This is the chapter we told you about in the intro — the one where the *mental models* change, not just the
syntax. Grab a coffee. This is where the porting either goes beautifully or eats a week.

## 3.1 The C# You Knew (We'll Miss It, But It Had to Go)

```csharp
public class ProductViewModel : ViewModelBase
{
    private string _name;
    public string Name
    {
        get => _name;
        set { if (_name != value) { _name = value; OnPropertyChanged(); } }
    }
    public ICommand Save { get; }
}
```

In C#: a property is an **instance member** of an object held by **reference**, the **GC** keeps everything
alive, **DI** is usually a container, and change notification is a little dance you do on every single setter.

Now watch what VAML does to each of those pillars.

## 3.2 `ObservableObject`: Notification with a Plot Twist

```cpp
#include <vaml/object.h>

class product_view_model : public vaml::observable_object
{
public:
    // The "view model value" is Product; the object that watches for changes
    // is managed separately — that's ownership (§3.4).
    struct value_t {
        std::string name;
        int quantity = 0;
    };

    vaml::property<std::string> name { "name", this };
    vaml::property<int> quantity   { "quantity", this };

    void update(value_t next)   // replace the whole value; notify once (C# notified per-field)
    {
        name.set(std::move(next.name));
        quantity.set(next.quantity);
    }
};
```

> ⚠ **Warning — the plot twist: value vs reference semantics.**
>
> In C#, a bound label *holds a reference* to the same `Name` string the ViewModel mutated — they were
> literally the same object, forever, for free.
>
> In C++, `std::string` is a **value**. The binding reads a *copy* whenever it needs one, and only refreshes
> when the property **raises change**. So:
>
> - **`set` does two things at once**: it stores the new value *and* raises `PropertyChanged`. That's the
>   whole trick.
> - Plainly assigning to a raw member does **nothing** — no notification, no refresh. (This is the twin of
>   C#'s "you forgot `OnPropertyChanged()`," except here there's no warning, just silence.)

**Setting and notifying:**

```cpp
vm.name.set("Widget");               // stores + raises PropertyChanged("name")
vm.update({ "Widget", 3 });          // replace whole value_t; notifies both fields
```

> 💡 **Tip:** You'll meet two rhythms in real code — **per-field** (`name.set(...)` each time) and
> **whole-value replace** (`update({...})`). Both are valid; they just notify with different granularity.
> Pick per-field for cheap, frequent edits; whole-value for "here's a fresh snapshot."

## 3.3 `RelayCommand`: the `ICommand` You Know, in a Trench Coat

The C# you know:

```csharp
Save = new RelayCommand(_ => SaveImpl(), _ => CanSave);
```

The VAML C++ you write:

```cpp
#include <vaml/command.h>

class product_view_model : public vaml::observable_object
{
public:
    // relay_command = Execute callable + optional CanExecute predicate.
    // Mirrors ICommand: Execute / CanExecute / CanExecuteChanged.
    vaml::relay_command save {
        [this] { save_impl(); },                 // Execute
        [this] { return can_save(); }            // CanExecute
    };
};
```

Bind it exactly the way you bound an `ICommand`:

```vaml
<Button Content="Save" Command="{Binding save}"/>
```

When `CanExecute`'s answer might have changed, the command raises `CanExecuteChanged` and the button
re-queries and re-enables itself. For the exact invalidation-hook name this release exposes, check the
public facade (`S920`) — the guide CI gate verifies it resolves (S951).

> ◇ **Remember:** a command is just *two lambdas and a notification*. Don't overthink it. When confused,
> re-read this snippet and it all comes back.

## 3.4 Ownership: The Part That Has No C# Analog (The Real Gap)

Everything above was syntax-with-flair. **This** is where C++ genuinely differs from C# — the part that has
no C# analog, because C# has a garbage collector and you don't anymore.

> ⚠ **Warning — the big one: no GC.** A C# ViewModel is freed when nothing references it. Done. The runtime
> waters the plants.
>
> In VAML, **you** decide the lifetime. You own it, and you free it — in the right order — or it bites you.
> The reward is deterministic teardown and zero GC pauses in the render loop. The cost is that a dangling
> raw observer is *your* bug, not the platform's.

Two questions you now answer explicitly:

**Q1 — Who owns this object, and for how long?**

VAML ships **no composition type** (S250): no library `CompositionRoot`, no runtime container, no
ViewModelLocator, no pub/sub. Composition is **static and app-owned** — *you* author the construction
(plain constructors + your wiring) and hand objects around once, at startup. VAML only *documents* the
pattern; it never injects for you:

```cpp
// your composition, authored in main (S250 app-owned, plain constructors)
auto svc = std::make_unique<product_service>();            // the injected service
auto vm  = std::make_unique<product_view_model>(*svc);     // its VM takes the service
auto win = std::make_unique<my_main_window>(vm.get());     // window OBSERVES vm
```

This is the "**engineer wires, we bind**" contract (S251): you build the VM with its service via a plain
constructor — no container, no magic — and VAML binds one-way/two-way to that VM (S242/S243/S244).

The rules, short and brutal:

- The **composition root owns** the ViewModels — `unique_ptr`. It's the only owner. Own it once, own it well.
- Views and bindings get **raw, non-owning pointers** (`vm.get()`). They look, they never own.
- Never let a view live longer than the VM it observes. The runtime's thread-affinity + teardown ordering
  (Ch.8, S725) will catch a wrong order with a clear error — but design it right up front instead.
- `shared_ptr` is for **genuinely shared** ownership (a resource referenced by several independent owners,
  like a style/template). It's the exception, not the default. If you reach for it because you're unsure,
  stop and make the owner explicit.

**Q2 — Can this object be copied or moved?**

> ⚠ **Warning — move-only by default.** Once an `observable_object` is wired into the tree, the runtime
> holds *addresses* into it. Copying or moving it would invalidate those addresses. So delete them all:

```cpp
class product_view_model : public vaml::observable_object
{
public:
    product_view_model() = default;
    product_view_model(product_view_model const&) = delete;
    product_view_model& operator=(product_view_model const&) = delete;
    product_view_model(product_view_model&&) = delete;
    product_view_model& operator=(product_view_model&&) = delete;
    // ... properties / commands ...
};
```

> 💡 **Tip:** This is not punishment; it's the compiler enforcing the "you are wired in, don't move" truth.
> If the compiler yells at you for copying a VM, it's almost certainly right, and you have better things to
> do than argue with it. The decision *"who owns the VM"* is made exactly once, in `main`, and never
> re-litigated.

## 3.5 The `x:Class` Code-Behind Owner

Remember the `x:Class="MyMainWindow"` from Chapter 1? Here's the hand that shakes it. The view's C++ class
*is* the `x:Class` — WPF's `MainWindow` + `InitializeComponent` rolled into one trench coat.

```cpp
#include <vaml/ui.h>

class my_main_window : public vaml::user_control
{
public:
    explicit my_main_window(product_view_model* vm);

    void on_click(vaml::routed_event_args const&);   // wired from markup, Ch.5

private:
    vaml::ui_element* _root = nullptr;               // from x:Name, Ch.2.4
};
```

And the constructor does the two things WPF used to split across a generated file and your code:

```cpp
my_main_window::my_main_window(product_view_model* vm)
{
    vaml::markup::load(this, "MainWindow.vaml");   // = InitializeComponent
    set_data_context(vm);                          // bindings now resolve against vm
}
```

> ◇ **Remember:** `InitializeComponent` is now `vaml::markup::load(this, "X.vaml")`, and `DataContext = vm`
> is now `set_data_context(vm)`. Same ideas, two new spellings, zero new concepts.

## 3.6 The Converters (A Sneak Peek, Chapter 4's Full Tango)

`IValueConverter.Convert` has no interface analog. VAML uses a **type-erased callable** (S242). You'll meet
it properly in Chapter 4 — just know the "converter *class* + resource" habit is about to become "converter
*lambdas* + registration."

## 3.7 In This Chapter, You Learned…

- `observable_object` + `property<T>::set` gives you `INotifyPropertyChanged` — but with **value semantics**:
  **set stores and notifies in one step; plain assignment is silent.**
- `relay_command` gives you `ICommand` — Execute + CanExecute + `CanExecuteChanged`, bound the same way.
- **Ownership is the real gap**: the composition root owns VMs (`unique_ptr`), views get raw non-owning
  pointers, wired objects are **move-only**. This is the single biggest mental-model change in the whole book.
- The `x:Class` C++ class is view + `InitializeComponent` + `DataContext` wiring in one.

_If you take only one chapter with you into the wilderness, it's this one._ Chapters 4 and 5 lean on it
constantly. And when in doubt, return to the three-word oath: **Set to notify. Own what you create.**
