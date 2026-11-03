#pragma once
// VAML BindingBase family — the binding RECIPE objects (WPF
// System.Windows.Data.BindingBase : MarkupExtension). 11n-4.
//
// WPF-faithful (verified from E:\GitRepos\wpf:
//   BindingBase.cs:57 `public abstract class BindingBase: MarkupExtension`,
//   BindingBase.cs:181 `public sealed override object ProvideValue(...)`):
//   - MarkupExtension is the base every XAML value-maker implements (DAG
//     anchor `MarkupExtension`). A Binding recipe IS a MarkupExtension, so
//     `{Binding Path=X}` resolves the recipe via ProvideValue.
//   - BindingBase is the abstract shared base of the three concrete recipes:
//       Binding          — ONE source property -> ONE target property;
//       MultiBinding     — SEVERAL source properties -> ONE target value via
//                          an IMultiValueConverter;
//       PriorityBinding  — an ORDERED candidate list; the FIRST that yields a
//                          valid value wins (the fallback ladder).
//   - Each recipe "provide_value"s into a BindingExpressionSpec that the
//     existing S240-S245 BindingExpression engine consumes — 11n-4 REUSES the
//     engine (no duplicate expression path).
//
// ADR-018: ZERO new locks (a recipe is a plain configured object, read on the
// affinity thread).

#include "vaml/core/BindingExpression.h" // BindingExpressionSpec + BindingMode/BindingPolicy
#include "vaml/core/IValueConverter.h"   // the S501 single-value converter
#include "vaml/core/export.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace vaml {

class DependencyObject;   // forward decl (Binding target/host)

// The abstract markup-extension base (WPF System.Windows.Markup
// MarkupExtension) — the DAG anchor `MarkupExtension`. A concrete extension
// (Binding, StaticResource, ...) provides the VALUE it represents when the
// XAML reader resolves the `{...}` reference. In VAML the reader-parsed markup
// (`{Binding Path=X}`) is classified by the S084/S092 scanner; the recipe
// object is the typed value-maker reachable by a binding author.
class VAML_CORE_EXPORT MarkupExtension
{
public:
    virtual ~MarkupExtension() = default;

    // Returns the value this extension represents (WPF ProvideValue). For a
    // Binding recipe this yields a configured BindingExpression-ready spec
    // (the reader + engine consume it). A null return means "no value"
    // (fallback/Class B).
    [[nodiscard]] virtual Value provide_value() const = 0;
};

// The abstract shared base of the three binding recipes (WPF BindingBase).
// Carries the common configuration: mode, converter+name, fallback value,
// policy. Concrete recipes add their structure + provide the spec.
class VAML_CORE_EXPORT BindingBase : public MarkupExtension
{
public:
    BindingBase() = default;
    ~BindingBase() override = default;

    BindingBase(const BindingBase&) = delete;
    BindingBase& operator=(const BindingBase&) = delete;

    // --- WPF-faithful common settings --------------------------------------
    void set_mode(BindingMode mode) { mode_ = mode; }
    [[nodiscard]] BindingMode mode() const noexcept { return mode_; }

    void set_converter(BindingConverter converter) { converter_ = std::move(converter); }
    [[nodiscard]] const BindingConverter& converter() const noexcept { return converter_; }
    void set_converter_name(std::string name) { converterName_ = std::move(name); }
    [[nodiscard]] const std::string& converter_name() const noexcept { return converterName_; }

    void set_fallback(Value v) { fallback_ = std::move(v); }
    [[nodiscard]] const std::optional<Value>& fallback() const noexcept { return fallback_; }

    void set_policy(BindingPolicy policy) { policy_ = policy; }
    [[nodiscard]] BindingPolicy policy() const noexcept { return policy_; }

    // Composes the common BindingExpressionSpec fields onto `spec` (mode,
    // converter, converterName, fallback, policy). Concrete recipes fill the
    // source/target/path + call this.
    void fill_spec(BindingExpressionSpec& spec) const;

protected:
    BindingMode          mode_ = BindingMode::OneTime;
    BindingPolicy        policy_ = BindingPolicy::Fallback;
    BindingConverter     converter_;
    std::string          converterName_;
    std::optional<Value> fallback_;
};

// Binding — ONE source property -> ONE target property (WPF Binding).
class VAML_CORE_EXPORT Binding : public BindingBase
{
public:
    Binding();
    explicit Binding(std::string path);

    void set_path(std::string path) { path_ = std::move(path); }
    [[nodiscard]] const std::string& path() const noexcept { return path_; }

    void set_source(ObservableObject* source) { source_ = source; }
    [[nodiscard]] ObservableObject* source() const noexcept { return source_; }

    void set_target(DependencyObject* target, std::string targetProp)
    {
        target_ = target;
        targetProp_ = std::move(targetProp);
    }

    // Builds a BindingExpressionSpec for the existing engine.
    [[nodiscard]] BindingExpressionSpec make_spec() const;

    [[nodiscard]] Value provide_value() const override;

private:
    std::string      path_;
    ObservableObject* source_ = nullptr;
    DependencyObject* target_ = nullptr;
    std::string      targetProp_;
};

// MultiBinding — SEVERAL source properties -> ONE target value via an
// IMultiValueConverter (WPF MultiBinding).
class VAML_CORE_EXPORT IMultiValueConverter
{
public:
    virtual ~IMultiValueConverter() = default;
    // Combines `values` (one per child Binding) into a single target value.
    [[nodiscard]] virtual Value convert(const std::vector<Value>& values) const = 0;
};

class VAML_CORE_EXPORT MultiBinding : public BindingBase
{
public:
    MultiBinding() = default;

    void add_binding(std::unique_ptr<Binding> b) { bindings_.push_back(std::move(b)); }
    [[nodiscard]] std::size_t binding_count() const noexcept { return bindings_.size(); }
    [[nodiscard]] const Binding* binding_at(std::size_t i) const { return bindings_[i].get(); }

    void set_multi_converter(IMultiValueConverter* c) { multiConverter_ = c; }
    [[nodiscard]] IMultiValueConverter* multi_converter() const noexcept { return multiConverter_; }

    [[nodiscard]] Value provide_value() const override;

private:
    std::vector<std::unique_ptr<Binding>> bindings_;
    IMultiValueConverter*                 multiConverter_ = nullptr;
};

// PriorityBinding — an ORDERED candidate list; the first valid value wins
// (WPF PriorityBinding). Each candidate targets the SAME target DP.
class VAML_CORE_EXPORT PriorityBinding : public BindingBase
{
public:
    PriorityBinding() = default;

    void add_binding(std::unique_ptr<Binding> b) { bindings_.push_back(std::move(b)); }
    [[nodiscard]] std::size_t binding_count() const noexcept { return bindings_.size(); }
    [[nodiscard]] const Binding* binding_at(std::size_t i) const { return bindings_[i].get(); }

    [[nodiscard]] Value provide_value() const override;

private:
    std::vector<std::unique_ptr<Binding>> bindings_;
};

} // namespace vaml
