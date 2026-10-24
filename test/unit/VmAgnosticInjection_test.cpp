// S251 — APP-CONSTRUCTED VM-WITH-INJECTED-SERVICE (dep:S250). Sprint 6.
//
// Proves the S250 "engineer wires, we bind" headline at the TEST level: the app
// constructs its OWN VM and passes its OWN service in via PLAIN CONSTRUCTOR
// wiring (no library container, no CompositionRoot) — and VAML then binds
// one-way and two-way to that VM, and a command invoked from the VM reaches the
// injected service.
//
// The S251 VM is a REAL vaml::ObservableObject with PROPERTY_BINDABLE members
// (the S247_1 convenience layer). The S251 service is an app-authored struct.
// The S251 demo VM takes the service by reference in its constructor:
//
//     S251ViewModel vm(service);   // plain-constructor injection, NO container
//
// There is NO library composition, NO resolve()/register(), NO ViewModelLocator.
// The wiring is literally: construct the service, construct the VM with a
// reference to it, build a BindingExpression against the VM + a real
// DependencyObject target. "Engineer wires, we bind."
//
// Suite map:
//   1  ONE-WAY app-constructed: service drives the VM via set_Name -> the real
//      DP target auto-updates (the S243 live one-way wire).
//   2  TWO-WAY app-constructed: a target edit writes back to the VM (the S244
//      target->source wire) onto the same injected-service VM.
//   3  COMMAND->SERVICE: a RelayCommand owned by the VM, when Execute()d,
//      calls the INJECTED service (the service observes the call) — and a
//      one-way binding still pushes the VM's command-updated property to the
//      target (VM command -> service -> property -> binding -> target).
//   4  NO-LIBRARY-COMPOSITION: the VM's construction needs ONLY the service
//      (plain ctor); nothing is registered/resolved/singleton'd by a library.
//
// Registers S251-unique object-scoped DPs on real DependencyObject targets.
// ADR-018: ZERO new locks.

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"       // StringMaker + operator<< for Value/enums
#include <vaml/assert/Shouldly.h>         // ShouldlyString / ShouldlyIntegral (SHOULD seam)

#include "vaml/core/BindingExpression.h"
#include "vaml/core/DependencyObject.h"
#include "vaml/core/ObservableObject.h"
#include "vaml/core/PropertyKey.h"
#include "vaml/core/PropertyMetadata.h"
#include "vaml/core/RelayCommand.h"
#include "vaml/core/Value.h"
#include "vaml/mvvm/property_bindable.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

using vaml::BindingExpression;
using vaml::BindingExpressionSpec;
using vaml::BindingMode;
using vaml::DependencyObject;
using vaml::ObservableObject;
using vaml::PropertyFlags;
using vaml::PropertyId;
using vaml::PropertyKey;
using vaml::PropertyMetadata;
using vaml::RelayCommand;
using vaml::TransferResult;
using vaml::Value;
using vaml::ValueType;

namespace {

// ---- S251-unique names ----
// The target DP name must MATCH the PROPERTY_BINDABLE name (each is registered
// under its own literal name) so the binding source path and the target
// property line up: a bindable `Name` registers `PropertyId::of("Name")`.
const char* kName = "Name";   // VM bindable String / target String

PropertyKey key(const std::string& name, ValueType t)
{
    return PropertyKey(name, t, "S251Test", PropertyFlags::None);
}

// ---- The APP-AUTHORED SERVICE (S251) --------------------------------------
// A plain app-domain struct. The app constructs it; the VM takes it by
// reference via a plain constructor. It is NEVER registered with any library,
// never resolved, never singleton'd. It counts how many times the VM's command
// asks it to do work.
struct S251DemoService
{
    int greetings = 0;
    std::string lastMessage; // the accumulated greeting transcript

    // The work the VM's command delegates to.
    std::string greet(std::string who)
    {
        ++greetings;
        lastMessage = "Hello, " + who + "!";
        return lastMessage;
    }
};

// ---- The APP-AUTHORED VIEW MODEL (S251) -----------------------------------
// A REAL vaml::ObservableObject built with the S247_1 PROPERTY_BINDABLE
// convenience. Its constructor takes the service by reference — the plain
// constructor wiring of S250 (no container). It exposes a bindable "Name"
// (String) that the one/two-way binding targets, and a RelayCommand that, when
// executed, calls the INJECTED service and pushes the result into the bindable
// Name (so a binding observes the command-driven change).
//
// Move-only: an ObservableObject is wired into the binding store, so
// copy/move are deleted (S247_1/guide §3.4 rule).
class S251ViewModel final : public ObservableObject
{
public:
    explicit S251ViewModel(S251DemoService& svc) : service_(svc)
    {
        // The command delegates to the injected service and then updates the
        // bindable `Name` so any one-way binding to it observes the change.
        greetCommand_ = std::make_unique<RelayCommand>([this](const Value& param) {
            // A String parameter names the "who"; otherwise fall back to "world".
            const std::string who =
                (param.type() == vaml::ValueType::String)
                    ? param.get<std::string>()
                    : std::string("world");
            (void)set_Name(service_.greet(who)); // calls service + notifies Name
        });
    }

    S251ViewModel(const S251ViewModel&) = delete;
    S251ViewModel& operator=(const S251ViewModel&) = delete;

    PROPERTY_BINDABLE(std::string, Name); // bindable String, registered as "Name"

    RelayCommand& greet_command() { return *greetCommand_; }

private:
    S251DemoService& service_;          // the injected service (non-owning ref)
    std::unique_ptr<RelayCommand> greetCommand_;
};

// A target whose String DP's metadata `with_changed` callback counts real
// notifications (the "target changed-callback" counter for the apply proofs).
struct S251Target
{
    DependencyObject obj;
    int stringWrites = 0;

    S251Target()
    {
        obj.register_property(key(kName, ValueType::String),
            PropertyMetadata::WithDefault(Value::String(""), PropertyFlags::None)
                .with_changed([this](const Value&, const Value&) { ++stringWrites; }));
    }
};

int tr_int(TransferResult r) { return static_cast<int>(r); }

} // namespace

// ===========================================================================
// 1 - ONE-WAY: service-driven VM -> target (app-constructed, live S243 wire)
// ===========================================================================

TEST_CASE("S251 one-way: the service-driven VM's set_Name pushes to the real DP target live")
{
    S251DemoService service;                 // app authors the service
    S251ViewModel vm(service);               // plain ctor: the injected service
    (void)vm.set_Name("Hello");              // seed the bindable store (registers "Name")
    S251Target target;

    // The app wires the one-way binding against the VM (engineer wires).
    BindingExpressionSpec spec;
    spec.source = &vm;
    spec.sourcePath = kName;
    spec.target = &target.obj;
    spec.targetProp = kName;
    spec.mode = BindingMode::OneWay;
    auto expr = BindingExpression::create(spec);
    SHOULD(ShouldlyIntegral(expr != nullptr).ShouldBe(true));
    SHOULD(ShouldlyIntegral(tr_int(expr->TransferSourceToTarget())).ShouldBe(tr_int(TransferResult::Applied)));

    // The initial push applies the VM's current Name ("Hello") to the target.
    SHOULD(ShouldlyString(target.obj.Get(PropertyId::of(kName))).ShouldBe(Value::String("Hello")));

    // A service-driven set on the VM (as a real app's service/async would) →
    // the live one-way subscription auto-updates the target.
    (void)vm.set_Name("Alice");
    SHOULD(ShouldlyString(target.obj.Get(PropertyId::of(kName))).ShouldBe(Value::String("Alice")));

    (void)vm.set_Name("Bob");
    SHOULD(ShouldlyString(target.obj.Get(PropertyId::of(kName))).ShouldBe(Value::String("Bob")));
}

// ===========================================================================
// 2 - TWO-WAY: a target edit writes back to the injected-service VM (S244)
// ===========================================================================

TEST_CASE("S251 two-way: a target edit writes back to the app-constructed VM's bindable property")
{
    S251DemoService service;
    S251ViewModel vm(service);
    (void)vm.set_Name("start");
    S251Target target;

    BindingExpressionSpec spec;
    spec.source = &vm;
    spec.sourcePath = kName;
    spec.target = &target.obj;
    spec.targetProp = kName;
    spec.mode = BindingMode::TwoWay;
    auto expr = BindingExpression::create(spec);
    SHOULD(ShouldlyIntegral(expr != nullptr).ShouldBe(true));
    (void)expr->TransferSourceToTarget();
    SHOULD(ShouldlyIntegral(expr->live_subscribed() ? 1 : 0).ShouldBe(1));
    SHOULD(ShouldlyIntegral(expr->live_writeback() ? 1 : 0).ShouldBe(1));

    // A target edit (a TextBox typing on the target DP) → write-back to the VM.
    // The write-back writes the ObservableObject base store (S244 wire reads
    // vm.Get(...)); the typed `Name()` field getter is the VM-read side kept in
    // lock-step by set_Name, so the write-back is verified through the store.
    (void)target.obj.Set(PropertyId::of(kName), Value::String("edited"));
    SHOULD(ShouldlyString(vm.Get(PropertyId::of(kName))).ShouldBe(Value::String("edited")));
}

// ===========================================================================
// 3 - COMMAND → SERVICE: the VM's command reaches the INJECTED service, and a
//     one-way binding observes the resulting property change
// ===========================================================================

TEST_CASE("S251 command->service: executing the VM's command calls the INJECTED service and the binding observes the prop")
{
    S251DemoService service;
    S251ViewModel vm(service);
    (void)vm.set_Name("ignored-for-now");
    S251Target target;

    BindingExpressionSpec spec;
    spec.source = &vm;
    spec.sourcePath = kName;
    spec.target = &target.obj;
    spec.targetProp = kName;
    spec.mode = BindingMode::OneWay;
    auto expr = BindingExpression::create(spec);
    (void)expr->TransferSourceToTarget();

    // The command executes with a parameter; it must reach the injected service.
    vm.greet_command().Execute(Value::String("Ada"));
    SHOULD(ShouldlyIntegral(service.greetings).ShouldBe(1));
    SHOULD(ShouldlyString(service.lastMessage).ShouldBe(std::string("Hello, Ada!")));

    // The command's set_Name() updated the VM bindable → the live one-way
    // binding auto-pushes to the target.
    SHOULD(ShouldlyString(vm.Name()).ShouldBe(std::string("Hello, Ada!")));
    SHOULD(ShouldlyString(target.obj.Get(PropertyId::of(kName))).ShouldBe(Value::String("Hello, Ada!")));
}

// ===========================================================================
// 4 - NO-LIBRARY-COMPOSITION: the VM is built by PLAIN APP WIRING, nothing else
// ===========================================================================

TEST_CASE("S251 no-library-composition: the VM is constructed by plain app wiring, nothing registered/resolved")
{
    // The whole "composition" is two lines of plain-constructor app wiring —
    // no container, no resolve(), no ViewModelLocator, no library CompositionRoot.
    S251DemoService service;
    S251ViewModel vm(service); // <-- the ONLY construction path

    // Sanity: the VM is live, its bindable reads the service-independent
    // default, and the command can execute (always-executable default).
    SHOULD(ShouldlyString(vm.Name()).ShouldBe(std::string()));
    SHOULD(ShouldlyIntegral(vm.greet_command().CanExecute(Value::Null()) ? 1 : 0).ShouldBe(1));
}
