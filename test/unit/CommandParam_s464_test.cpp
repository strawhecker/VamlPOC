// S464 (dep:S463) — COMMAND + CommandParameter attributes.
//
// The authored Command surface rides the S253 command registry + the S246
// ICommand::Execute(Value) interface as a PARAMETERIZED execute:
//   * `Command="{Binding SaveCommand}"` — the Binding path IS the command NAME
//       (resolved via the app-registered S253 command registry).
//   * `CommandParameter="{Binding SelectedItem}"` — a parameter VALUE (a bound
//       path / static value / literal) passed to ICommand::Execute(parameter).
//   * `Command="SaveCommand"` (bare name) + a literal CommandParameter also work.
//   * MenuItem.Header / Command / CommandParameter attributes ride the SAME
//       attribute surface — the element-resolution (a real <MenuItem>) is
//       S491/11e, recorded forward; this step delivers the attribute CONTRACT.
//
// The plan title "S253 {Command} extended to a parameterized execute" is
// realized as VamlReader::ExecuteCommand(name, parameter): resolve + gate on
// CanExecute + Execute(parameter) — the reader seam a Button click (or a
// MenuItem command) drives.
//
// Registers no S464-unique DPs. Uses the Shouldly vocabulary. ADR-018 zero locks.

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/ICommand.h"       // ICommand (the command primitive)
#include "vaml/core/RelayCommand.h"   // RelayCommand (the S246 concrete command)
#include "vaml/core/Value.h"
#include "vaml/core/VamlReader.h"

#include <memory>
#include <string>

using vaml::RelayCommand;
using vaml::Value;
using vaml::VamlReader;

// ---------------------------------------------------------------------------
// 1. Command="{Binding SaveCommand}" + CommandParameter="{Binding SelectedItem}"
//    execute the command with the parameter through ICommand::Execute(Value)
// ---------------------------------------------------------------------------

TEST_CASE("S464: Command + CommandParameter execute a command with the parameter")
{
    VamlReader reader;
    Value received = Value::Null();
    // The app registers its command (the S250/S253 pattern — no DI). RelayCommand
    // is the S246 concrete; register one that records its parameter.
    auto* command = new RelayCommand([&received](const Value& p) { received = p; }, nullptr);
    reader.register_command("SaveCommand", command);
    // The COMMAND+PARAMETER surface: Command names the command, CommandParameter
    // an item to pass. We exercise ExecuteCommand with the parameter VALUE.
    const Value item = Value::String("item-42");
    const bool executed = reader.ExecuteCommand("SaveCommand", item);
    SHOULD(ShouldlyIntegral(static_cast<int>(executed)).ShouldBe(1));
    SHOULD(ShouldlyString(std::string(received.get<std::string>()))
               .ShouldBe(std::string("item-42")));
    delete command;
}

// ---------------------------------------------------------------------------
// 2. A command that CanExecute()==false does NOT execute (WPF-faithful gate)
// ---------------------------------------------------------------------------

TEST_CASE("S464: a CanExecute-driven command gates Execution (never a silent blind execute)")
{
    VamlReader reader;
    int executed = 0;
    // A command that says it CANNOT execute (predicate false) must not invoke.
    auto* command = new RelayCommand(
        [&](const Value&) { ++executed; },
        [](const Value&) { return false; }); // CanExecute always false
    reader.register_command("Blocked", command);

    const bool r = reader.ExecuteCommand("Blocked", Value::Null());
    SHOULD(ShouldlyIntegral(static_cast<int>(r)).ShouldBe(0)); // gate: no execute
    SHOULD(ShouldlyIntegral(executed).ShouldBe(0));
    delete command;
}

// ---------------------------------------------------------------------------
// 3. An unregistered command resolves as false (never throws)
// ---------------------------------------------------------------------------

TEST_CASE("S464: an unregistered Command returns false (never a throw)")
{
    VamlReader reader;
    const bool r = reader.ExecuteCommand("NoSuchCommand", Value::Null());
    SHOULD(ShouldlyIntegral(static_cast<int>(r)).ShouldBe(0));
}

// ---------------------------------------------------------------------------
// 4. CommandParameter as a literal (plain Value) — the same attribute surface
// ---------------------------------------------------------------------------

TEST_CASE("S464: a literal CommandParameter passes through as the parameter Value")
{
    VamlReader reader;
    Value received = Value::Null();
    auto* command = new RelayCommand(
        [&received](const Value& p) { received = p; }, nullptr);
    reader.register_command("SaveCommand", command);

    // A literal parameter (e.g. CommandParameter="5") — execute with the Value.
    const Value literal = Value::Int32(5);
    const bool executed = reader.ExecuteCommand("SaveCommand", literal);
    SHOULD(ShouldlyIntegral(static_cast<int>(executed)).ShouldBe(1));
    SHOULD(ShouldlyIntegral(received.get<int>()).ShouldBe(5));
    delete command;
}
