#pragma once
// VAML IWeakEventListener — the weak subscription-lifetime contract
// (WPF System.Windows.IWeakEventListener). 11n-4 (DAG anchor `IWeakEventListener`).
//
// WPF-faithful (verified from E:\GitRepos\wpf:
//   src\...\WindowsBase\System\Windows\IWeakEventListener.cs):
//     interface IWeakEventListener
//     {
//         // receives an event on behalf of the owner; managerType identifies
//         // the event manager that raised it, args the event payload.
//         bool ReceiveWeakEvent(Type managerType, object sender, EventArgs e);
//     }
//
// The weak-handler contract is HOW a binding listens to its source without
// keeping it alive (the roadmap 11n-4 item: "BindingExpression subscribes via
// IWeakEventListener — the weak-handler subscription-lifetime contract and
// RE-RUNS on PropertyChanged / CollectionChanged, true event-subscription,
// NOT polled"). A listener that implements IWeakEventListener receives the
// event; the SUBSCRIBER holds a WEAK reference to the listener (the binding
// expression/collection view), so the source can be collected even while the
// binding lives.
//
// The existing S243 binding engine subscribes STRONGLY today (liveToken_).
// This interface is the weak-LIFETIME seam: a binding/collection-view that
// implements IWeakEventListener can be reached weakly, and the event manager
// (11n-4's weak-bridge) invokes ReceiveWeakEvent through a weak handle instead
// of a strong std::function. Concrete event managers:
//   - PropertyChangedEventManager: fires ReceiveWeakEvent with
//     "PropertyChanged" as the managerTag when a bound source property changes;
//   - CollectionChangedEventManager: fires it with "CollectionChanged" on a
//     collection mutation.
//
// ADR-018: ZERO new locks (single-threaded affinity; the weak-bridge is
// processed on the affinity thread).

#include "vaml/core/export.h"
#include "vaml/core/Value.h"          // the VAML event payload domain

#include <string>
#include <typeinfo>

namespace vaml {

// A tag identifying the event manager that raised a weak event (the WPF
// managerType analogue). A string keeps it value-equatable + reflection-free.
using WeakEventManagerTag = std::string;

// Constants for the two concrete managers (WPF-faithful names).
inline constexpr const char* kPropertyChangedManager  = "PropertyChanged";
inline constexpr const char* kCollectionChangedManager = "CollectionChanged";

// The weak-handler contract (WPF System.Windows.IWeakEventListener). A binding
// / collection-view implements this to receive weak notifications; the event
// bridge holds a weak handle to it.
class VAML_CORE_EXPORT IWeakEventListener
{
public:
    virtual ~IWeakEventListener() = default;

    // Receives a weak event on behalf of the owner. `managerTag` identifies
    // the firing manager (PropertyChanged / CollectionChanged); `sender` is the
    // notifying object; `args` the payload (a PropertyChanged / CollectionChanged
    // args value). Returns true when the eager/sink handled it.
    virtual bool ReceiveWeakEvent(const WeakEventManagerTag& managerTag,
                                  const void* sender,
                                  const Value& args) = 0;
};

} // namespace vaml
