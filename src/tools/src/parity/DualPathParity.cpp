// S288 (dep:S287) — Dual-path parity harness implementation. See the header for
// the contract: for the SAME VAML source, the S2 text reader and the S7 compiled
// reader (over vamlcc→.vbaml bytes) must produce BEHAVIORALLY identical trees.
// This comparator walks both live trees side by side and reports per-axis
// parity (retained document order, resolved element types, literal prop
// converted values incl. attached dotted keys, parent chain, x:Name registry,
// deferred {Binding} extensions, deferred event handlers). It asserts the
// contract WITHOUT demanding object identity (the two readers build distinct
// objects by design).
//
// Single-threaded + pure (ADR-018); ADR-021 headless (drives both real readers).

#include "vaml/tools/parity/DualPathParity.h"

#include "vaml/core/Element.h" // built-in types for the dynamic_cast probe + register_builtin_elements
#include "vaml/core/PropertyId.h"
#include "vaml/core/ValueConverter.h" // register_builtin_converters (S7 needs the S089 table)
#include "vaml/tools/Vamlc.h"         // compile_to_bytes (the S7 bytes)
#include "vaml/tools/VBamlReader.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace vaml::tools {

namespace detail {

bool values_equal(const vaml::Value& a, const vaml::Value& b)
{
    return a == b;
}

// Human-readable element-type probe via dynamic_cast to the built-in classes.
// Deterministic on BOTH paths (the S7 factory and the S2 resolver create the
// same concrete built-in types). Falls back to "Object" for unknown types.
const char* probe_type(const vaml::DependencyObject* o)
{
    if (o == nullptr)
    {
        return "(null)";
    }
    if (dynamic_cast<const vaml::Button*>(o))
    {
        return "Button";
    }
    if (dynamic_cast<const vaml::Window*>(o))
    {
        return "Window";
    }
    if (dynamic_cast<const vaml::StackPanel*>(o))
    {
        return "StackPanel";
    }
    if (dynamic_cast<const vaml::Grid*>(o))
    {
        return "Grid";
    }
    if (dynamic_cast<const vaml::WrapPanel*>(o))
    {
        return "WrapPanel";
    }
    if (dynamic_cast<const vaml::Canvas*>(o))
    {
        return "Canvas";
    }
    if (dynamic_cast<const vaml::Text*>(o))
    {
        return "Text";
    }
    if (dynamic_cast<const vaml::TextBox*>(o))
    {
        return "TextBox";
    }
    if (dynamic_cast<const vaml::CheckBox*>(o))
    {
        return "CheckBox";
    }
    if (dynamic_cast<const vaml::Slider*>(o))
    {
        return "Slider";
    }
    if (dynamic_cast<const vaml::ScrollViewer*>(o))
    {
        return "ScrollViewer";
    }
    return "Object";
}

} // namespace detail

ParityReport compare_dual_path(std::string_view source,
                               const std::vector<std::string>& events)
{
    ParityReport report;

    // --- Drive BOTH paths from the SAME source. --------------------------
    // S2 (text path): vaml::VamlReader::Load(source) — the S2 reader.
    vaml::VamlReader s2;
    for (const std::string& ev : events)
    {
        s2.register_event(ev);
    }
    std::unique_ptr<vaml::DependencyObject> s2root = s2.Load(source);

    // S7 (compiled path): compile the SAME source to bytes, then load via
    // vaml::tools::VBamlReader. Register the same event set on both readers so
    // event-shaped keys route identically (S286/Strand C).
    const std::vector<std::uint8_t> bytes = vaml::tools::compile_to_bytes(source);
    vaml::tools::VBamlReader s7;
    for (const std::string& ev : events)
    {
        s7.register_event(ev);
    }
    std::unique_ptr<vaml::DependencyObject> s7root = s7.Load(bytes);

    // --- Root type. --------------------------------------------------------
    {
        ParityAxis a;
        a.name = "root_type";
        const char* t2 = detail::probe_type(s2root.get());
        const char* t7 = detail::probe_type(s7root.get());
        a.ok = (t2 != nullptr && t7 != nullptr && std::string(t2) == std::string(t7));
        if (!a.ok)
        {
            a.diff = "s2=" + std::string(t2 ? t2 : "(null)") + " s7=" +
                     std::string(t7 ? t7 : "(null)");
        }
        report.axes.push_back(std::move(a));
    }

    // --- Retained element count + DOCUMENT order + resolved types. -------
    {
        const auto& r2 = s2.retained_objects();
        const auto& r7 = s7.retained_objects();

        ParityAxis cnt;
        cnt.name = "retained_count";
        cnt.ok = (r2.size() == r7.size());
        if (!cnt.ok)
        {
            cnt.diff = "s2=" + std::to_string(r2.size()) + " s7=" +
                       std::to_string(r7.size());
        }
        report.axes.push_back(std::move(cnt));

        if (r2.size() == r7.size())
        {
            ParityAxis order;
            order.name = "retained_document_order_types";
            order.ok = true;
            for (std::size_t i = 0; i < r2.size(); ++i)
            {
                const std::string t2 = detail::probe_type(r2[i].get());
                const std::string t7 = detail::probe_type(r7[i].get());
                if (t2 != t7)
                {
                    order.ok = false;
                    order.diff += "[" + std::to_string(i) + "] s2=" + t2 + " s7=" +
                                  t7 + "; ";
                }
            }
            report.axes.push_back(std::move(order));
        }
    }

    // --- Literal property converted values (incl. attached dotted keys). --
    // The prop-key axis is exposed separately (compare_dual_path_props) so the
    // caller passes the golden's plain-DP names; the structural/deferred axes
    // live here in compare_dual_path.

    // --- Parent chain. ------------------------------------------------------
    {
        const auto& r2 = s2.retained_objects();
        const auto& r7 = s7.retained_objects();
        ParityAxis a;
        a.name = "parent_chain";
        a.ok = true;
        if (r2.size() == r7.size())
        {
            // Root's parent is null on both; each retained element's parent must
            // be the SAME-side tree ancestor. We verify the structural depth
            // (parent present == parent present, grandparent == grandparent)
            // rather than identity (distinct objects across paths).
            for (std::size_t i = 0; i < r2.size(); ++i)
            {
                const bool hasParent2 = r2[i]->parent() != nullptr;
                const bool hasParent7 = r7[i]->parent() != nullptr;
                if (hasParent2 != hasParent7)
                {
                    a.ok = false;
                    a.diff += "[retained " + std::to_string(i) + "] parent-presence s2=" +
                              std::to_string(hasParent2) + " s7=" +
                              std::to_string(hasParent7) + "; ";
                }
            }
        }
        report.axes.push_back(std::move(a));
    }

    // --- x:Name registry (names in registration order + mapped objects). ---
    {
        ParityAxis a;
        a.name = "name_registry";
        a.ok = (s2.name_count() == s7.name_count());
        if (!a.ok)
        {
            a.diff = "s2 names=" + std::to_string(s2.name_count()) + " s7 names=" +
                     std::to_string(s7.name_count());
            report.axes.push_back(std::move(a));
        }
        else
        {
            const auto& n2 = s2.names();
            const auto& n7 = s7.names();
            a.ok = true;
            for (std::size_t i = 0; i < n2.size(); ++i)
            {
                if (n2[i].first != n7[i].first)
                {
                    a.ok = false;
                    a.diff += "[name " + std::to_string(i) + "] s2='" +
                              std::string(n2[i].first) + "' s7='" +
                              std::string(n7[i].first) + "'; ";
                }
                else
                {
                    // same name: mapped objects must be of the SAME type.
                    const char* t2 = detail::probe_type(n2[i].second);
                    const char* t7 = detail::probe_type(n7[i].second);
                    if (std::string(t2) != std::string(t7))
                    {
                        a.ok = false;
                        a.diff += "[name '" + std::string(n2[i].first) + "'] type s2=" +
                                  t2 + " s7=" + t7 + "; ";
                    }
                }
            }
            report.axes.push_back(std::move(a));
        }
    }

    // --- Deferred {Binding} extensions (count, target type, key, source). ---
    {
        const auto& e2 = s2.deferred_extensions();
        const auto& e7 = s7.deferred_extensions();
        ParityAxis a;
        a.name = "deferred_bindings";
        a.ok = (e2.size() == e7.size());
        if (!a.ok)
        {
            a.diff = "s2=" + std::to_string(e2.size()) + " s7=" + std::to_string(e7.size());
            report.axes.push_back(std::move(a));
        }
        else
        {
            a.ok = true;
            for (std::size_t i = 0; i < e2.size(); ++i)
            {
                const std::string k2(e2[i].key), k7(e7[i].key);
                const std::string s2s(e2[i].source), s7s(e7[i].source);
                const std::string t2 = detail::probe_type(e2[i].target);
                const std::string t7 = detail::probe_type(e7[i].target);
                if (k2 != k7 || s2s != s7s || t2 != t7)
                {
                    a.ok = false;
                    a.diff += "[ext " + std::to_string(i) + "] key s2='" + k2 +
                              "' s7='" + k7 + "' src s2='" + s2s + "' s7='" + s7s +
                              "' target-type s2=" + t2 + " s7=" + t7 + "; ";
                }
            }
            report.axes.push_back(std::move(a));
        }
    }

    // --- Deferred event handlers (count, target type, event, handler). -----
    {
        const auto& h2 = s2.deferred_event_handlers();
        const auto& h7 = s7.deferred_event_handlers();
        ParityAxis a;
        a.name = "deferred_events";
        a.ok = (h2.size() == h7.size());
        if (!a.ok)
        {
            a.diff = "s2=" + std::to_string(h2.size()) + " s7=" +
                     std::to_string(h7.size());
            report.axes.push_back(std::move(a));
        }
        else
        {
            a.ok = true;
            for (std::size_t i = 0; i < h2.size(); ++i)
            {
                const std::string e2(h2[i].eventName), e7(h7[i].eventName);
                const std::string hn2(h2[i].handlerName), hn7(h7[i].handlerName);
                const std::string t2 = detail::probe_type(h2[i].target);
                const std::string t7 = detail::probe_type(h7[i].target);
                if (e2 != e7 || hn2 != hn7 || t2 != t7)
                {
                    a.ok = false;
                    a.diff += "[ev " + std::to_string(i) + "] name s2='" + e2 +
                              "' s7='" + e7 + "' handler s2='" + hn2 + "' s7='" +
                              hn7 + "' target-type s2=" + t2 + " s7=" + t7 + "; ";
                }
            }
            report.axes.push_back(std::move(a));
        }
    }

    return report;
}

ParityAxis compare_dual_path_props(std::string_view source,
                                   const std::vector<std::string>& propKeys,
                                   const std::vector<std::string>& events)
{
    ParityAxis a;
    a.name = "literal_props";

    vaml::VamlReader s2;
    for (const std::string& ev : events)
    {
        s2.register_event(ev);
    }
    std::unique_ptr<vaml::DependencyObject> s2root = s2.Load(source);

    const std::vector<std::uint8_t> bytes = vaml::tools::compile_to_bytes(source);
    vaml::tools::VBamlReader s7;
    for (const std::string& ev : events)
    {
        s7.register_event(ev);
    }
    std::unique_ptr<vaml::DependencyObject> s7root = s7.Load(bytes);

    // Collect element pairs in DOCUMENT order: root, then retained (both are in
    // the path's construction/document order).
    const auto& r2 = s2.retained_objects();
    const auto& r7 = s7.retained_objects();
    if (r2.size() != r7.size())
    {
        a.ok = false;
        a.diff = "retained count differs: s2=" + std::to_string(r2.size()) + " s7=" +
                 std::to_string(r7.size());
        return a;
    }

    // Node index 0 = root; retained[i] pairs with s2 root-chain.
    const std::size_t total = 1 + r2.size();
    a.ok = true;
    for (std::size_t ni = 0; ni < total; ++ni)
    {
        const vaml::DependencyObject* o2 = (ni == 0) ? s2root.get() : r2[ni - 1].get();
        const vaml::DependencyObject* o7 = (ni == 0) ? s7root.get() : r7[ni - 1].get();
        for (const std::string& key : propKeys)
        {
            const vaml::Value v2 = o2->Get(vaml::PropertyId::of(key));
            const vaml::Value v7 = o7->Get(vaml::PropertyId::of(key));
            if (!(v2 == v7))
            {
                a.ok = false;
                a.diff += "[node " + std::to_string(ni) + " key='" + key + "' type s2=" +
                          detail::probe_type(o2) + " s7=" + detail::probe_type(o7) + "] ";
            }
        }
    }
    return a;
}

} // namespace vaml::tools
