# Chapter 9 — The Order Tracker: A Whole App, Start to Finish

> ### In This Chapter
> - Porting one complete, real app — C# on the left, VAML C++ on the right
> - Seeing Chapter 1–8's advice all wired together in one place
> - The four "real change" moments, called out so you never miss them
> - A debugging cheat that turns "why is this empty/broken" into a five-second guess

**Evidence:** S440–S445 (POC app), S923 (sample gallery/tutorial), plus the chapter evidence from Ch.1–8.
Snippets compiled in CI (S951).

---

Enough theory — let's build something. This is the payoff chapter: one small-but-real app, ported section by
section, **C# on the left, VAML C++ on the right**, so you can *see* what each VAML C++ line replaces. This
mirrors the project's own POC/sample app (`S440`–`S445`).

## 9.0 The App: "Order Tracker"

A window with a header, a list of orders, a two-way text box to filter, and a "Refresh" button that
asynchronously loads orders. Selecting an order shows its detail with a formatted price. It's the shape of a
first WPF MVVM app — small enough to hold in your head, complete enough to use everything from this book.

Throughout, sections marked **"➜ real change"** are the ones that are *more than syntax* — they're the four
mental-model shifts (Ch.3, Ch.5, Ch.8). Everything else is translation.

## 9.1 Startup — Who Owns What

**C#**
```csharp
public partial class App : Application
{
    protected override void OnStartup(StartupEventArgs e)
    {
        var vm = new OrderListViewModel();
        var w  = new MainWindow { DataContext = vm };
        w.Show();
    }
}
```

**VAML C++** (Ch.1, Ch.3.4)
```cpp
int main(int argc, char** argv)
{
    vaml::App app(argc, argv);
    auto vm  = std::make_unique<order_list_view_model>();
    auto win = std::make_unique<main_window>(vm.get());   // raw, non-owning
    return vaml::run(app, std::move(win));                // runtime owns the window
}
```
**➜ real change — ownership, not just syntax:** you *explicitly* hand the window's ownership to the runtime,
and the composition root owns the VM. There is no container — **you** are the container (ADR-006). In C#
this was a `var` and a nap.

## 9.2 The ViewModel — Observable + Command (Ch.3)

**C#**
```csharp
public class OrderListViewModel : ViewModelBase
{
    private ObservableCollection<Order> _orders;
    public ObservableCollection<Order> Orders { get => _orders; set { _orders = value; OnPropertyChanged(); } }
    private string _filter;
    public string Filter { get => _filter; set { _filter = value; OnPropertyChanged(); } }
    public ICommand Refresh { get; }
}
```

**VAML C++**
```cpp
class order_list_view_model : public vaml::observable_object
{
public:
    order_list_view_model() = default;
    // move-only: wired into the tree (Ch.3.4)
    order_list_view_model(order_list_view_model const&) = delete;
    order_list_view_model& operator=(order_list_view_model const&) = delete;
    order_list_view_model(order_list_view_model&&) = delete;
    order_list_view_model& operator=(order_list_view_model&&) = delete;

    vaml::property<std::vector<order>> orders { "orders", this };
    vaml::property<std::string>        filter { "filter", this };

    vaml::relay_command refresh { [this]{ load(); }, [this]{ return !loading; } };
    // ...
};
```
**➜ real change — value semantics:** `orders` holds a `std::vector` *value*. You **replace it wholesale and
notify** rather than mutating a collection in place and raising a collection-changed event. And you notify
via `set` (Ch.3.2). An `ObservableCollection` with a mutation event becomes "here's a fresh vector, look."

## 9.3 The Markup (Ch.2, Ch.6)

**C# WPF**
```xml
<Window x:Class="OrderTracker.MainWindow" Title="Order Tracker" Width="520" Height="360">
    <DockPanel>
        <StackPanel DockPanel.Dock="Top" Orientation="Horizontal" Margin="8">
            <TextBox Text="{Binding Filter, Mode=TwoWay}" Width="180"/>
            <Button Content="Refresh" Command="{Binding Refresh}" Margin="8,0,0,0"/>
        </StackPanel>
        <ListBox ItemsSource="{Binding Orders}">
            <ListBox.ItemTemplate>
                <DataTemplate><TextBlock Text="{Binding Name}"/></DataTemplate>
            </ListBox.ItemTemplate>
        </ListBox>
    </DockPanel>
</Window>
```

**VAML**
```vaml
<UserControl x:Class="main_window" Width="520" Height="360">
    <StackPanel Margin="8">
        <StackPanel Orientation="Horizontal" Margin="0,0,0,8">
            <TextBox Text="{Binding filter, Mode=TwoWay}" Width="180"/>
            <Button Content="Refresh" Command="{Binding refresh}" Margin="8,0,0,0"/>
        </StackPanel>
        <ListBox ItemsSource="{Binding orders}">
            <ListBox.ItemTemplate>
                <DataTemplate><TextBlock Text="{Binding Name}"/></DataTemplate>
            </ListBox.ItemTemplate>
        </ListBox>
    </StackPanel>
</UserControl>
```
**Not a real change — but two habits:** `using:` namespaces vs `clr-namespace` (Ch.2.2), and the property
names in markup match the **C++ property names** you declared (`save` vs `Save`, `orders` vs `Orders`).
Snippets must pass `vamlcc`.

## 9.4 Code-Behind Owner — DataContext + Async Refresh (Ch.3.5, Ch.5, Ch.8)

**C#**
```csharp
public partial class MainWindow : Window
{
    private readonly OrderListViewModel _vm;
    public MainWindow(OrderListViewModel vm) { InitializeComponent(); DataContext = _vm = vm; }
    private async void Refresh_Click(object sender, RoutedEventArgs e)
    {
        _vm.Orders = await _repo.FetchAsync(_vm.Filter);
    }
}
```

**VAML C++**
```cpp
class main_window : public vaml::user_control
{
public:
    explicit main_window(order_list_view_model* vm)
    {
        vaml::markup::load(this, "MainWindow.vaml");  // InitializeComponent analog
        set_data_context(vm);
    }

    void on_refresh(vaml::routed_event_args const& e)
    {
        if (e.handled()) return;
        auto* vm = data_context<order_list_view_model>();
        repo().fetch(vm->filter.get()).then_on(
            vaml::ui_context(),                        // Ch.8: touch tree only here
            this,
            [this](vaml::result<std::vector<order>> r) {
                if (r) data_context<order_list_view_model>()->orders.set(*r);
                else   show_error(r.error());
            });
        e.mark_handled();
    }
};
```
**➜ real change — async is continuations:** `async void` + `await` becomes a **Task continuation** on the
UI context (Ch.5.5, Ch.8.3). Notice the `then_on(vaml::ui_context(), ...)` — that's the Ch.8 marshalling in
the flesh. And `orders.set(*r)` is Ch.3's set-to-notify. All four chapters, one snippet.

## 9.5 Selecting an Item and Showing Detail (Ch.4 Converters)

**C#**
```csharp
public class PriceConverter : IValueConverter { public object Convert(...) => $"${(double)value/100:0.00}"; }
```
```xml
<TextBlock Text="{Binding PriceCents, Converter={StaticResource PriceConverter}}"/>
```

**VAML C++** (Ch.4.4)
```cpp
main_window::main_window(order_list_view_model* vm) {
    vaml::markup::load(this, "MainWindow.vaml");
    set_data_context(vm);
    set_converter("price", [](vaml::value const& v, vaml::type_id) -> vaml::value {
        return vaml::value(std::format("${:.2f}", v.as<double>() / 100.0));
    });
}
```
```vaml
<TextBlock Text="{Binding PriceCents, Converter={StaticConverter price}}"/>
```
**➜ real change — converters are callables:** the converter is a **lambda you register by name**, not an
`IValueConverter` class resource (Ch.4.4). Same job, no interface, no reflection, no "resource instantiation"
waltz.

## 9.6 The Five-Second Debugging Cheat

When something's empty or broken, run this mental table before you lose the afternoon:

| Symptom | First guess |
|---|---|
| Something's empty/binding shows nothing | didn't `set` (Ch.3.2) — check `property<T>::set` |
| "Unknown property/handler" | typo vs the facade → **compile-time** (Ch.7.2, `S826`) |
| Wrong-thread write error | touched the tree off the UI thread (Ch.8.3) |
| (Almost) anything with the fetch | continuation off the UI context (Ch.8.3) |
| Crash at shutdown | teardown order / dangling observer (Ch.3.4, Ch.8.4) |

> 💡 **Tip — the ordering hunch:** the runtime's thread-affinity + teardown ordering (`S724`/`S725`) means a
> wrong *order* reports itself loudly instead of corrupting silently. When you see a shutdown crash, your
> first instinct should be "reverse order of construction" — it usually is.

## 9.7 In This Chapter, You Learned…

Porting is **1:1 at the markup and MVVM level**, with four deliberate changes you now recognize at a glance:

1. **Ownership** — `main`/composition root owns VMs (Ch.3.4).
2. **Value semantics + `set`** — replace the value, notify via `set` (Ch.3.2).
3. **Continuations for async** — `Task` + `.then_on(ui_context())` (Ch.5, Ch.8).
4. **Overt threading** — say which thread, tear down in reverse (Ch.8).

If you've internalized Chapters 3, 5, and 8, the rest of this book is translation — not redesign. You're
not porting a language; you're porting four habits. And now you know exactly which four.

**Next stop:** Appendix A (the glossary — "what changed and what didn't") and Appendix B (the cheat sheet +
the ten ways tourists hurt themselves). You've earned the cheat sheet.
