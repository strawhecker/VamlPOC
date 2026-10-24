// S252 (dep:S251) + S253 (dep:S252) — VamlReader markup {Binding} resolution.
//
// S252: the reader's deferred-extensions (the S2 tag) are resolved by
// VamlReader::BindExtensions() into LIVE BindingExpressions against a
// data context (the app-constructed VM, S250/S251 pattern) + target DP:
//   <StackPanel S252Text="{Binding Title}"/>
//   -> set_data_context(vm) -> BindExtensions() -> the bound value lands on the
//   target (first transfer) + a live one-way re-push on set_Title.
//   Mode=TwoWay arms the S244 write-back end-to-end through markup.
//   A malformed {Binding ...} throws VamlReaderException(MalformedBindingMarkup).
// S253: {Command DoIt} markup — left deferred by S252's BindExtensions, then
//   resolved via VamlReader::ResolveCommand(name) against the reader's command
//   registry (register_command), and invocable via the S246 ICommand surface.
//
// Registers S252/S253-unique global DPs (S039 discipline). ADR-018: zero locks.

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"       // StringMaker + operator<< for Value/enums
#include <vaml/assert/Shouldly.h>         // ShouldlyString / ShouldlyIntegral / ShouldThrow

#include "vaml/core/BindingExpression.h"  // BindingMode (mode introspection)
#include "vaml/core/DependencyObject.h"
#include "vaml/core/DependencyProperty.h"
#include "vaml/core/Element.h"            // register_builtin_elements (reader Load)
#include "vaml/core/ObservableObject.h"
#include "vaml/core/PropertyFlags.h"
#include "vaml/core/PropertyId.h"
#include "vaml/core/PropertyMetadata.h"
#include "vaml/core/RelayCommand.h"
#include "vaml/core/Value.h"
#include "vaml/core/VamlReader.h"
#include "vaml/mvvm/property_bindable.h"

#include <memory>
#include <string>
#include <unordered_set>
#include <utility>

using vaml::BindingMode;
using vaml::DependencyObject;
using vaml::DependencyProperty;
using vaml::ObservableObject;
using vaml::PropertyFlags;
using vaml::PropertyId;
using vaml::PropertyKey;
using vaml::PropertyMetadata;
using vaml::RelayCommand;
using vaml::Value;
using vaml::ValueType;
using vaml::VamlReader;
using vaml::VamlReaderError;
using vaml::VamlReaderException;
using vaml::test::ShouldThrow;

namespace {

std::unordered_set<std::string>& seen_names()
{
    static std::unordered_set<std::string> seen;
    return seen;
}

const DependencyProperty& registerGlobal(const std::string& name, ValueType type,
                                         PropertyFlags flags, Value def)
{
    if (seen_names().insert(name).second)
    {
        return DependencyProperty::Register(name, type, "S252_Binding_Owner",
                                            PropertyMetadata::WithDefault(def, flags));
    }
    return DependencyProperty::Get(name);
}

void registerS252Properties()
{
    (void)registerGlobal("S252Text",  ValueType::String, PropertyFlags::None, Value::String(""));
    (void)registerGlobal("S253Text",  ValueType::Int32,  PropertyFlags::None, Value::Int32(0));
}

// The app-constructed VM (S250/S251 pattern): a real ObservableObject with
// PROPERTY_BINDABLE members — the "Title" the {Binding Title} resolves against.
class S252ViewModel final : public ObservableObject
{
public:
    PROPERTY_BINDABLE(std::string, Title);

    // The S253 {Command} surface — a RelayCommand the markup can resolve.
    RelayCommand& doIt() { return doIt_; }

private:
    vaml::RelayCommand doIt_{[](const vaml::Value&) { /* the action */ }, /*can*/ {}};
};

} // namespace

// ---------------------------------------------------------------------------
// S252 — {Binding Title} resolved: first value + live one-way re-push
// ---------------------------------------------------------------------------

TEST_CASE("S252: BindExtensions resolves {Binding Title} onto a real VM + target DP")
{
    registerS252Properties();
    VamlReader reader;
    S252ViewModel vm;
    (void)vm.set_Title("Hello");   // seed the bindable store (registers "Title")
    reader.set_data_context(&vm);

    // Load a doc whose extension is recorded (deferred), then bind it.
    std::unique_ptr<DependencyObject> root = reader.Load("<StackPanel S252Text=\"{Binding Title}\"/>");
    SHOULD(ShouldlyIntegral(reader.deferred_extensions().size()).ShouldBe(1u));

    const std::size_t bound = reader.BindExtensions();
    SHOULD(ShouldlyIntegral(bound).ShouldBe(1u));
    SHOULD(ShouldlyIntegral(reader.live_bindings().size()).ShouldBe(1u));

    // The bound value landed on the target (the S242 first transfer).
    const DependencyObject* target = reader.deferred_extensions()[0].target;
    SHOULD(ShouldlyString(target->Get(PropertyId::of("S252Text")))
               .ShouldBe(Value::String("Hello")));

    // A live VM change auto-pushes through the S243 one-way wire.
    (void)vm.set_Title("World");
    SHOULD(ShouldlyString(target->Get(PropertyId::of("S252Text")))
               .ShouldBe(Value::String("World")));
}

// ---------------------------------------------------------------------------
// S252 — {Binding Title, Mode=TwoWay} arms the write-back end-to-end
// ---------------------------------------------------------------------------

TEST_CASE("S252: {Binding Title, Mode=TwoWay} binds two-way through markup")
{
    registerS252Properties();
    VamlReader reader;
    S252ViewModel vm;
    (void)vm.set_Title("start");
    reader.set_data_context(&vm);

    std::unique_ptr<DependencyObject> root = reader.Load(
        "<StackPanel S252Text=\"{Binding Title, Mode=TwoWay}\"/>");
    const std::size_t bound = reader.BindExtensions();
    SHOULD(ShouldlyIntegral(bound).ShouldBe(1u));

    // The S252 live binding is TwoWay (mode carried through markup).
    const vaml::BindingExpression& expr = *reader.live_bindings()[0];
    SHOULD(ShouldlyIntegral(vaml::to_binding_direction(expr.mode()) == vaml::BindingDirection::TwoWay ? 1 : 0)
               .ShouldBe(1));

    const DependencyObject* target = reader.deferred_extensions()[0].target;
    // A target edit -> the S244 write-back reaches the VM store.
    (void)const_cast<DependencyObject*>(target)->Set(PropertyId::of("S252Text"), Value::String("edited"));
    SHOULD(ShouldlyString(vm.Get(PropertyId::of("Title"))).ShouldBe(Value::String("edited")));
}

// ---------------------------------------------------------------------------
// S252 — a malformed {Binding} is a Class A reader error (never silent)
// ---------------------------------------------------------------------------

TEST_CASE("S252: a malformed {Binding} throws VamlReaderException(MalformedBindingMarkup)")
{
    registerS252Properties();
    VamlReader reader;
    S252ViewModel vm;
    reader.set_data_context(&vm);

    std::unique_ptr<DependencyObject> root = reader.Load(
        "<StackPanel S252Text=\"{Binding}\"/>"); // no path
    SHOULD(ShouldlyIntegral(reader.deferred_extensions().size()).ShouldBe(1u));

    bool threw = false;
    try
    {
        (void)reader.BindExtensions();
    }
    catch (const VamlReaderException& e)
    {
        threw = true;
        SHOULD(ShouldlyIntegral(static_cast<int>(e.kind()))
                   .ShouldBe(static_cast<int>(VamlReaderError::MalformedBindingMarkup)));
    }
    SHOULD(ShouldlyIntegral(static_cast<int>(threw)).ShouldBe(1));
}

TEST_CASE("S252: malformed {Binding} — ShouldThrow form")
{
    registerS252Properties();
    VamlReader reader;
    (void)reader.Load("<StackPanel S252Text=\"{Binding , Mode=TwoWay}\"/>");
    SHOULD(ShouldThrow<VamlReaderException>([&] { (void)reader.BindExtensions(); }));
}

// ---------------------------------------------------------------------------
// S252 — kind None (StaticResource) stays deferred; not bound, not an error
// ---------------------------------------------------------------------------

TEST_CASE("S252: {StaticResource X} stays deferred (kind None), not bound")
{
    registerS252Properties();
    VamlReader reader;
    S252ViewModel vm;
    reader.set_data_context(&vm);

    std::unique_ptr<DependencyObject> root = reader.Load(
        "<StackPanel S252Text=\"{StaticResource SomeBrush}\"/>");
    const std::size_t bound = reader.BindExtensions();
    SHOULD(ShouldlyIntegral(bound).ShouldBe(0u));
    SHOULD(ShouldlyIntegral(reader.live_bindings().size()).ShouldBe(0u));
}

// ---------------------------------------------------------------------------
// S253 — {Command DoIt} markup: deferred by S252, resolved + invocable
// ---------------------------------------------------------------------------

TEST_CASE("S253: {Command DoIt} markup is resolved via ResolveCommand + invocable")
{
    registerS252Properties();
    VamlReader reader;
    S252ViewModel vm;
    reader.set_data_context(&vm);

    // The app registers its own command (S250 app-owned composition).
    reader.register_command("DoIt", &vm.doIt());

    std::unique_ptr<DependencyObject> root = reader.Load(
        "<StackPanel S253_DoIt=\"{Command DoIt}\"/>");
    SHOULD(ShouldlyIntegral(reader.deferred_extensions().size()).ShouldBe(1u));

    // S252's BindExtensions leaves {Command ...} deferred (not bound).
    const std::size_t bound = reader.BindExtensions();
    SHOULD(ShouldlyIntegral(bound).ShouldBe(0u));

    // S253 resolves the command by name against the reader's registry.
    vaml::ICommand* cmd = reader.ResolveCommand("DoIt");
    SHOULD(ShouldlyIntegral(cmd != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(cmd->CanExecute(vaml::Value::Null()) ? 1 : 0).ShouldBe(1));
    // The resolved command is the app's RelayCommand (invocable via ICommand).
    cmd->Execute(vaml::Value::Null());
    SHOULD(ShouldlyIntegral(reader.ResolveCommand("Nope") == nullptr).ShouldBe(1));
}

TEST_CASE("S253: register_command is idempotent + ResolveCommand works for registered only")
{
    VamlReader reader;
    S252ViewModel vm;
    reader.register_command("A", &vm.doIt());
    reader.register_command("A", &vm.doIt()); // repeat -> first wins (no crash)
    SHOULD(ShouldlyIntegral(reader.ResolveCommand("A") != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(reader.ResolveCommand("Missing") == nullptr).ShouldBe(1));
}
