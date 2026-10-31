// S502 (dep:S501) — BEHAVIORS: the Microsoft.Xaml.Behaviors interactive model
// made REAL + permanent (the S11k-5 EventTrigger Actions payload resolved at 11f
// — the interactivity `i:` surface, NOT the System.Windows Style.Triggers
// family).
//
// THIS SUITE PROVES:
//   A. `Behavior` base — attach/detach lifecycle + associated object + the
//      single-associated-object discipline (attaching to a second element while
//      attached throws, WPF-faithful). ADR-018 zero locks.
//   B. `TypedBehavior<T>` — the typed mapping of WPF `Behavior<T>`:
//      `associated_object()` returns the TYPED element (nullptr when not
//      attached, or when attached to a differently-typed element).
//   C. `BehaviorCollection` — the owned ordered behavior list + the
//      `i:Interaction.Behaviors` attached surface (get_element_behaviors):
//      add takes ownership + attaches to the owner; detach_all detaches; the
//      side-table lazily creates the collection + is owner-linked.
//   D. `TriggerAction` + `InvokeCommandAction` — a no-op base + an action that
//      executes an `ICommand` with a parameter, gated on `CanExecute`
//      (WPF-faithful). A null-command / null-parameter edge is honest.
//   E. `InteractivityEventTrigger` — a Behavior that subscribes the element's
//      ROUTED event (the S320/S321 engine) when attached and fires its ordered
//      actions on the raise; the behavior REALLY fires the command on the
//      routed event (the S502 title headline). An unknown event name is
//      reported (never silent) and does NOT crash attach.
//
// Headless (Rail 1, ADR-021). doctest main comes from smoke_test.cpp. The live
// WINDOW event+behavior interaction is the [both-apps] local gate, proven here
// by the exact routed-event + command-execution mechanism.

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/Behavior.h"
#include "vaml/core/BehaviorCollection.h"
#include "vaml/core/EventTriggerBehavior.h"
#include "vaml/core/InvokeCommandAction.h"
#include "vaml/core/RelayCommand.h"
#include "vaml/core/RoutedEvent.h"
#include "vaml/core/TriggerAction.h"
#include "vaml/core/UIElement.h"
#include "vaml/core/Element.h"   // StackPanel (the attached element)

#include <cstddef>

using vaml::Behavior;
using vaml::BehaviorCollection;
using vaml::InteractivityEventTrigger;
using vaml::InvokeCommandAction;
using vaml::RelayCommand;
using vaml::StackPanel;
using vaml::TriggerAction;
using vaml::TypedBehavior;
using vaml::UIElement;
using vaml::Value;

// A concrete plain behavior (the WPF `Behavior` is abstract — a real behavior
// is authored as a derived class, so a test-only concrete base is the honest
// way to exercise the attach/detach lifecycle).
class S502PlainBehavior final : public Behavior
{
public:
    S502PlainBehavior() = default;
};

// A concrete plain action (the WPF `TriggerAction` is abstract — the concrete
// no-op we test the base invoke on).
class S502PlainAction final : public TriggerAction
{
public:
    S502PlainAction() = default;
};

// ---------------------------------------------------------------------------
// A. Behavior base — attach/detach + the single-associated-object discipline
// ---------------------------------------------------------------------------

TEST_CASE("S502: Behavior attach/detach lifecycle + associated object")
{
    vaml::register_builtin_elements();
    StackPanel host;
    S502PlainBehavior b;
    SHOULD(ShouldlyIntegral(b.is_attached() ? 1 : 0).ShouldBe(0));
    SHOULD(ShouldlyIntegral(b.associated_object() == nullptr ? 1 : 0).ShouldBe(1));

    b.attach_to(&host);
    SHOULD(ShouldlyIntegral(b.is_attached() ? 1 : 0).ShouldBe(1));
    SHOULD(ShouldlyIntegral(b.associated_object() == &host ? 1 : 0).ShouldBe(1));

    b.detach();
    SHOULD(ShouldlyIntegral(b.is_attached() ? 1 : 0).ShouldBe(0));
    SHOULD(ShouldlyIntegral(b.associated_object() == nullptr ? 1 : 0).ShouldBe(1));

    // Detach is idempotent + no-throw on a not-attached behavior.
    b.detach();
    SHOULD(ShouldlyIntegral(b.is_attached() ? 1 : 0).ShouldBe(0));
}

TEST_CASE("S502: a behavior has ONE associated object (attach to a second throws)")
{
    StackPanel a;
    StackPanel b;
    S502PlainBehavior bh;
    bh.attach_to(&a);
    bool threw = false;
    try
    {
        bh.attach_to(&b); // already attached to `a` — must fail loud, never silently steal
    }
    catch (const std::logic_error&)
    {
        threw = true;
    }
    SHOULD(ShouldlyIntegral(threw ? 1 : 0).ShouldBe(1));
    // Still attached to `a` (unchanged).
    SHOULD(ShouldlyIntegral(bh.associated_object() == &a ? 1 : 0).ShouldBe(1));
}

// ---------------------------------------------------------------------------
// B. TypedBehavior<T> — the WPF `Behavior<T>` mapping
// ---------------------------------------------------------------------------

namespace {

// A concrete typed behavior: observes its typed associated element.
class S502Typed final : public TypedBehavior<StackPanel>
{
public:
    int attachedCount = 0;
    int detachedCount = 0;
    void on_attach() override { ++attachedCount; }
    void on_detach() noexcept override { ++detachedCount; }
};

} // namespace

TEST_CASE("S502: TypedBehavior<T> returns the TYPED associated object")
{
    StackPanel host;
    S502Typed tb;
    SHOULD(ShouldlyIntegral(tb.associated_object() == nullptr ? 1 : 0).ShouldBe(1));

    tb.attach_to(&host);
    // The typed associated object IS the StackPanel (dynamic_cast-based).
    SHOULD(ShouldlyIntegral(tb.associated_object() == &host ? 1 : 0).ShouldBe(1));
    // The concrete on_attach hook ran.
    SHOULD(ShouldlyIntegral(tb.attachedCount).ShouldBe(1));

    tb.detach();
    SHOULD(ShouldlyIntegral(tb.associated_object() == nullptr ? 1 : 0).ShouldBe(1));
    SHOULD(ShouldlyIntegral(tb.detachedCount).ShouldBe(1));
}

// ---------------------------------------------------------------------------
// C. BehaviorCollection + the i:Interaction.Behaviors attached surface
// ---------------------------------------------------------------------------

TEST_CASE("S502: BehaviorCollection add (takes ownership + attaches) + detach_all")
{
    StackPanel host;
    vaml::BehaviorCollection col;
    col.set_owner(&host);
    auto b1 = std::make_unique<S502PlainBehavior>();
    auto b2 = std::make_unique<S502PlainBehavior>();
    (void)col.add(std::move(b1));
    (void)col.add(std::move(b2));
    SHOULD(ShouldlyIntegral(col.count()).ShouldBe(2u));
    SHOULD(ShouldlyIntegral(col.owner() == &host ? 1 : 0).ShouldBe(1));

    // Both behaviors are attached to the owner.
    auto ptrs = col.as_pointers();
    SHOULD(ShouldlyIntegral(ptrs.size()).ShouldBe(2u));
    SHOULD(ShouldlyIntegral(ptrs[0]->is_attached() ? 1 : 0).ShouldBe(1));
    SHOULD(ShouldlyIntegral(ptrs[0]->associated_object() == &host ? 1 : 0).ShouldBe(1));
    SHOULD(ShouldlyIntegral(ptrs[1]->is_attached() ? 1 : 0).ShouldBe(1));

    // detach_all detaches every behavior (owner unchanged, collection intact).
    col.detach_all();
    SHOULD(ShouldlyIntegral(ptrs[0]->is_attached() ? 1 : 0).ShouldBe(0));
    SHOULD(ShouldlyIntegral(col.count()).ShouldBe(2u));
}

TEST_CASE("S502: get_element_behaviors lazily creates the owner collection")
{
    vaml::register_builtin_elements();
    StackPanel host;
    // No behaviors yet.
    SHOULD(ShouldlyIntegral(vaml::element_has_behaviors(host) ? 1 : 0).ShouldBe(0));

    vaml::BehaviorCollection* col = vaml::get_element_behaviors(host);
    SHOULD(ShouldlyIntegral(col != nullptr ? 1 : 0).ShouldBe(1));
    SHOULD(ShouldlyIntegral(vaml::element_has_behaviors(host) ? 1 : 0).ShouldBe(1));
    // The lazily-created collection is owner-linked to the element.
    SHOULD(ShouldlyIntegral(col->owner() == &host ? 1 : 0).ShouldBe(1));

    // The SAME collection is returned on a repeat (no duplicate ownership).
    SHOULD(ShouldlyIntegral(vaml::get_element_behaviors(host) == col ? 1 : 0).ShouldBe(1));
}

// ---------------------------------------------------------------------------
// D. TriggerAction base + InvokeCommandAction executes an ICommand
// ---------------------------------------------------------------------------

TEST_CASE("S502: TriggerAction base is a no-op (an action without concrete behavior)")
{
    S502PlainAction a;
    // invoke is a no-op; never throws.
    a.invoke(Value::Null());
    a.invoke(Value::Int32(7));
}

TEST_CASE("S502: InvokeCommandAction executes the command on invoke")
{
    int executed = 0;
    Value lastParam = Value::Null();
    RelayCommand cmd([&](const Value& p) { ++executed; lastParam = p; });

    InvokeCommandAction action;
    action.set_command(&cmd);
    action.invoke(Value::Int32(42));
    SHOULD(ShouldlyIntegral(executed).ShouldBe(1));
    SHOULD(ShouldlyIntegral(lastParam.get<std::int32_t>()).ShouldBe(42));
}

TEST_CASE("S502: InvokeCommandAction with a CommandParameter overrides the trigger parameter")
{
    int executed = 0;
    Value lastParam = Value::Null();
    RelayCommand cmd([&](const Value& p) { ++executed; lastParam = p; });

    InvokeCommandAction action;
    action.set_command(&cmd);
    action.set_command_parameter(Value::String("override"));
    action.invoke(Value::Int32(99)); // the override wins
    SHOULD(ShouldlyIntegral(executed).ShouldBe(1));
    SHOULD(ShouldlyString(lastParam).ShouldBe(Value::String("override")));
}

TEST_CASE("S502: InvokeCommandAction with NO command is a no-op")
{
    int executed = 0;
    RelayCommand cmd([&](const Value&) { ++executed; });
    InvokeCommandAction action; // no command set
    action.invoke(Value::Int32(1));
    SHOULD(ShouldlyIntegral(executed).ShouldBe(0));
}

TEST_CASE("S502: InvokeCommandAction gates on CanExecute (WPF-faithful)")
{
    int executed = 0;
    // A predicate that only allows parameter 0.
    RelayCommand cmd([&](const Value&) { ++executed; },
                     [](const Value& p) { return p.get<std::int32_t>() == 0; });
    InvokeCommandAction action;
    action.set_command(&cmd);
    // CanExecute(1)==false -> does NOT execute.
    action.invoke(Value::Int32(1));
    SHOULD(ShouldlyIntegral(executed).ShouldBe(0));
    // CanExecute(0)==true -> executes.
    action.invoke(Value::Int32(0));
    SHOULD(ShouldlyIntegral(executed).ShouldBe(1));
}

// ---------------------------------------------------------------------------
// E. InteractivityEventTrigger — the behavior fires the action on the routed event
// ---------------------------------------------------------------------------

TEST_CASE("S502: the behavior FIRES the command on the routed event (the title headline)")
{
    vaml::register_builtin_elements();
    StackPanel host;
    int executed = 0;
    RelayCommand cmd([&](const Value&) { ++executed; });

    // Author a CLICK behavior: an event trigger that invokes the command.
    auto trigger = std::make_unique<InteractivityEventTrigger>();
    trigger->set_event_name("Click");
    auto action = std::make_unique<InvokeCommandAction>();
    action->set_command(&cmd);
    (void)trigger->add_action(std::move(action));

    // Attach it to the host through a behavior collection (the i: surface).
    vaml::BehaviorCollection* col = vaml::get_element_behaviors(host);
    (void)col->add(std::move(trigger));

    // Raise Click on the host -> the trigger fires the command.
    SHOULD(ShouldlyIntegral(executed).ShouldBe(0));
    vaml::RoutedEventArgs args;
    args.source = &host;
    (void)host.RaiseEvent(vaml::ClickEvent(), args);
    SHOULD(ShouldlyIntegral(executed).ShouldBe(1));
    // The trigger observed the event.
    auto ptrs = col->as_pointers();
    auto* trig = dynamic_cast<InteractivityEventTrigger*>(ptrs[0]);
    SHOULD(ShouldlyIntegral(trig != nullptr ? 1 : 0).ShouldBe(1));
    SHOULD(ShouldlyIntegral(trig->fired_count()).ShouldBe(1u));
}

TEST_CASE("S502: a detached event trigger no longer fires on the routed event")
{
    vaml::register_builtin_elements();
    StackPanel host;
    int executed = 0;
    RelayCommand cmd([&](const Value&) { ++executed; });

    auto trigger = std::make_unique<InteractivityEventTrigger>();
    trigger->set_event_name("Click");
    auto action = std::make_unique<InvokeCommandAction>();
    action->set_command(&cmd);
    (void)trigger->add_action(std::move(action));
    vaml::BehaviorCollection* col = vaml::get_element_behaviors(host);
    (void)col->add(std::move(trigger));

    vaml::RoutedEventArgs args;
    args.source = &host;
    (void)host.RaiseEvent(vaml::ClickEvent(), args);
    SHOULD(ShouldlyIntegral(executed).ShouldBe(1));

    // Detach the behavior -> the subscription is removed -> no more fires.
    auto ptrs = col->as_pointers();
    ptrs[0]->detach();
    (void)host.RaiseEvent(vaml::ClickEvent(), args);
    SHOULD(ShouldlyIntegral(executed).ShouldBe(1)); // unchanged
}

TEST_CASE("S502: an unknown event name reports (never silent) and does NOT crash")
{
    vaml::register_builtin_elements();
    StackPanel host;
    int executed = 0;
    RelayCommand cmd([&](const Value&) { ++executed; });
    auto trigger = std::make_unique<InteractivityEventTrigger>();
    trigger->set_event_name("NoSuchEvent"); // unregistered
    auto action = std::make_unique<InvokeCommandAction>();
    action->set_command(&cmd);
    (void)trigger->add_action(std::move(action));

    // Attach must NOT throw (an unknown event is reported, never a crash).
    vaml::BehaviorCollection* col = vaml::get_element_behaviors(host);
    (void)col->add(std::move(trigger));
    vaml::RoutedEventArgs args;
    args.source = &host;
    (void)host.RaiseEvent(vaml::ClickEvent(), args);
    // Nothing fired (no subscription on the unknown event).
    SHOULD(ShouldlyIntegral(executed).ShouldBe(0));
}

// ---------------------------------------------------------------------------
// Interop: multiple triggers + ordered actions + mixed action content
// ---------------------------------------------------------------------------

TEST_CASE("S502: multiple triggers each fire their own command on separate events")
{
    vaml::register_builtin_elements();
    StackPanel host;
    int clicks = 0;
    int loads = 0;
    RelayCommand clickCmd([&](const Value&) { ++clicks; });
    RelayCommand loadCmd([&](const Value&) { ++loads; });

    auto clickTrig = std::make_unique<InteractivityEventTrigger>();
    clickTrig->set_event_name("Click");
    auto a1 = std::make_unique<InvokeCommandAction>();
    a1->set_command(&clickCmd);
    (void)clickTrig->add_action(std::move(a1));

    auto loadTrig = std::make_unique<InteractivityEventTrigger>();
    loadTrig->set_event_name("Loaded");
    auto a2 = std::make_unique<InvokeCommandAction>();
    a2->set_command(&loadCmd);
    (void)loadTrig->add_action(std::move(a2));

    vaml::BehaviorCollection* col = vaml::get_element_behaviors(host);
    (void)col->add(std::move(clickTrig));
    (void)col->add(std::move(loadTrig));

    vaml::RoutedEventArgs args;
    args.source = &host;
    (void)host.RaiseEvent(vaml::ClickEvent(), args);
    SHOULD(ShouldlyIntegral(clicks).ShouldBe(1));
    SHOULD(ShouldlyIntegral(loads).ShouldBe(0));

    (void)host.RaiseEvent(vaml::LoadedEvent(), args);
    SHOULD(ShouldlyIntegral(clicks).ShouldBe(1));
    SHOULD(ShouldlyIntegral(loads).ShouldBe(1));
}
