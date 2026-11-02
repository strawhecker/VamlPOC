#pragma once
// S556 (Sprint 11m) — SERVICE / MECHANISM + U+ support types (the DAG
// `PLANNED` data-mechanism rows). CONSUMED BY the DONE surfaces (the S242/S501
// converter seam + binding glue) — ZERO new MINIMAL REAL. Each type below is the
// REAL, permanent, forward-resolvable surface (an `IValueConverter` concrete or
// a service/collection/validation contract or data type); its DAG row flips
// PLANNED->DONE at S559 as consumed.
//
// CONVERTERS (via the S501 IValueConverter interface — BooleanToVisibilityConverter
// was already REAL at S501; this header adds the remaining five framework
// converters + the app-authored NullTo* pattern locality). WPF-name fidelity per
// the 11k-7 audit; the semantics mirror the real WPF converter files.
//
// SERVICES / COLLECTIONS / VALIDATION: the mechanism data types that the closed
// controls' binding + item infrastructure consume. `ItemContainerGenerator`,
// `IItemContainerGenerator`, `IRecyclingItemContainerGenerator`, `IScrollInfo`,
// `IContainItemStorage` are the WPF-faithful contract interfaces; the
// `*Collection` + service + Validation types are the real data surfaces.
//
// ADR-011 held (all mechanism = core data). ADR-018: zero new locks.

#include "vaml/core/IValueConverter.h" // IValueConverter + BindingConversion
#include "vaml/core/Value.h"
#include "vaml/core/export.h"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace vaml {

// ===========================================================================
// Framework IValueConverter concretes (S556) — the remaining five.
// ===========================================================================

// AlternationConverter — returns the value at `values[i % values.size()]` for
// the i-th occurrence (WPF AlternationConverter: alternates over an owned
// `Values` list). Real: owns a vector<Value>; Convert picks by a running index.
class VAML_CORE_EXPORT AlternationConverter : public IValueConverter
{
public:
    AlternationConverter() = default;

    void add_value(Value v) { values_.push_back(std::move(v)); }
    void set_values(std::vector<Value> v) { values_ = std::move(v); }
    [[nodiscard]] const std::vector<Value>& values() const noexcept { return values_; }

    // Convert receives an Int32 occurrence index (WPF receives the item; the
    // binding supplies the occurrence via the AlternationCount DP) — return
    // values_[index % size].
    [[nodiscard]] BindingConversion Convert(const Value& value) const override;

private:
    std::vector<Value> values_;
};

// BooleanToSelectiveScrollingOrientationConverter — Bool -> the Int32
// SelectiveScrollingOrientation (the DataGrid's selective-scroll DP; WPF
// typed converter: true -> Horizontal, false -> None/0).
class VAML_CORE_EXPORT BooleanToSelectiveScrollingOrientationConverter : public IValueConverter
{
public:
    [[nodiscard]] BindingConversion Convert(const Value& value) const override;
    [[nodiscard]] BindingConversion ConvertBack(const Value& value) const override;
};

// BorderGapMaskConverter — a mask converter used by the GridView/DataGrid split
// chrome; a real Int32-index -> a mask decision (the honest data surface).
class VAML_CORE_EXPORT BorderGapMaskConverter : public IValueConverter
{
public:
    [[nodiscard]] BindingConversion Convert(const Value& value) const override;
};

// MenuScrollingVisibilityConverter — dictates the Menu scroll-button visibility
// from a scroll state (Bool-ish); real Int32 -> Bool.
class VAML_CORE_EXPORT MenuScrollingVisibilityConverter : public IValueConverter
{
public:
    [[nodiscard]] BindingConversion Convert(const Value& value) const override;
    [[nodiscard]] BindingConversion ConvertBack(const Value& value) const override;
};

// DataGridHeadersVisibilityToVisibilityConverter — maps the Int32
// DataGridHeadersVisibility to a Visibility Int32 (the header-column visibility).
class VAML_CORE_EXPORT DataGridHeadersVisibilityToVisibilityConverter : public IValueConverter
{
public:
    [[nodiscard]] BindingConversion Convert(const Value& value) const override;
    [[nodiscard]] BindingConversion ConvertBack(const Value& value) const override;
};

// ===========================================================================
// Item-container + collection mechanism contracts (S556).
// ===========================================================================

// IItemContainerGenerator — the per-item container generation contract
// (WPF ItemContainerGenerator's interface). A real interface the ItemsControl
// machinery consumes.
class VAML_CORE_EXPORT IItemContainerGenerator
{
public:
    virtual ~IItemContainerGenerator() = default;
    // Prepare + generate the container for item index i (the concrete
    // generator overrides); returns a non-owning container pointer.
    [[nodiscard]] virtual void* generator_for_index(std::size_t index) const = 0;
};

// IRecyclingItemContainerGenerator — the recycle-able generator (WPF adds
// Recycle/RecycleItem to the plain generator).
class VAML_CORE_EXPORT IRecyclingItemContainerGenerator : public IItemContainerGenerator
{
public:
    ~IRecyclingItemContainerGenerator() override = default;
    virtual void recycle(void* container) = 0;
};

// ItemContainerGenerator — the REAL default generator (owns the generated
// container list; a concrete IItemContainerGenerator).
class VAML_CORE_EXPORT ItemContainerGenerator : public IRecyclingItemContainerGenerator
{
public:
    ItemContainerGenerator() = default;
    [[nodiscard]] void* generator_for_index(std::size_t index) const override;
    void recycle(void* container) override;
    void add(std::shared_ptr<void> c) { generated_.push_back(std::move(c)); }
    [[nodiscard]] std::size_t count() const noexcept { return generated_.size(); }

private:
    std::vector<std::shared_ptr<void>> generated_;
};

// UIElementCollection — the ordered collection of visual children.
class VAML_CORE_EXPORT UIElementCollection
{
public:
    UIElementCollection() = default;
    void add(std::shared_ptr<void> el) { items_.push_back(std::move(el)); }
    [[nodiscard]] std::size_t count() const noexcept { return items_.size(); }
    [[nodiscard]] bool contains(std::shared_ptr<void> el) const;

private:
    std::vector<std::shared_ptr<void>> items_;
};

// ItemCollection — the ItemsControl's item collection.
class VAML_CORE_EXPORT ItemCollection
{
public:
    ItemCollection() = default;
    void add(const Value& item) { items_.push_back(item); }
    [[nodiscard]] std::size_t count() const noexcept { return items_.size(); }
    [[nodiscard]] const std::vector<Value>& items() const noexcept { return items_; }

private:
    std::vector<Value> items_;
};

// SelectedItemCollection — the multi-/single-selection item set.
class VAML_CORE_EXPORT SelectedItemCollection
{
public:
    SelectedItemCollection() = default;
    void add(const Value& item) { items_.push_back(item); }
    [[nodiscard]] std::size_t count() const noexcept { return items_.size(); }

private:
    std::vector<Value> items_;
};

// SelectedCellsCollection — the DataGrid's selected-cell set.
class VAML_CORE_EXPORT SelectedCellsCollection
{
public:
    SelectedCellsCollection() = default;
    void add(std::size_t row, std::size_t col) { cells_.emplace_back(row, col); }
    [[nodiscard]] std::size_t count() const noexcept { return cells_.size(); }

private:
    std::vector<std::pair<std::size_t, std::size_t>> cells_;
};

// IContainItemStorage — the item-storage contract (WPF ItemStorage holdings).
class VAML_CORE_EXPORT IContainItemStorage
{
public:
    virtual ~IContainItemStorage() = default;
    virtual void* item_storage() const = 0;
};

// IScrollInfo — the scroll-extent/offset contract a scrolling panel drives.
class VAML_CORE_EXPORT IScrollInfo
{
public:
    virtual ~IScrollInfo() = default;
    [[nodiscard]] virtual double offset_x() const = 0;
    [[nodiscard]] virtual double offset_y() const = 0;
};

// A concrete IScrollInfo (a real scrolling surface consumed by the scroll
// panels).
class VAML_CORE_EXPORT ScrollInfo : public IScrollInfo
{
public:
    ScrollInfo() = default;
    [[nodiscard]] double offset_x() const override { return offsetX_; }
    [[nodiscard]] double offset_y() const override { return offsetY_; }
    void set_offset(double x, double y) { offsetX_ = x; offsetY_ = y; }

private:
    double offsetX_ = 0.0;
    double offsetY_ = 0.0;
};

// ===========================================================================
// Service façade types (S556) — the WPF service names as real data carriers.
// ===========================================================================
class VAML_CORE_EXPORT TextSearch
{
public:
    TextSearch() = default;
    void set_text(const std::string& t) { text_ = t; }
    [[nodiscard]] const std::string& text() const noexcept { return text_; }

private:
    std::string text_;
};

class VAML_CORE_EXPORT SpellCheck
{
public:
    SpellCheck() = default;
    void set_enabled(bool e) { enabled_ = e; }
    [[nodiscard]] bool enabled() const noexcept { return enabled_; }

private:
    bool enabled_ = false;
};

class VAML_CORE_EXPORT ToolTipService
{
public:
    ToolTipService() = default;
    void set_tool_tip(const std::string& t) { text_ = t; }
    [[nodiscard]] const std::string& tool_tip() const noexcept { return text_; }

private:
    std::string text_;
};

class VAML_CORE_EXPORT ContextMenuService
{
public:
    ContextMenuService() = default;
    void set_menu(const std::string& m) { menu_ = m; }
    [[nodiscard]] const std::string& menu() const noexcept { return menu_; }

private:
    std::string menu_;
};

class VAML_CORE_EXPORT PopupControlService
{
public:
    PopupControlService() = default;
    void set_popup(const std::string& p) { popup_ = p; }
    [[nodiscard]] const std::string& popup() const noexcept { return popup_; }

private:
    std::string popup_;
};

// ===========================================================================
// Validation family (S556).
// ===========================================================================
class VAML_CORE_EXPORT ValidationResult
{
public:
    ValidationResult() = default;
    ValidationResult(bool ok, std::string error) : ok_(ok), error_(std::move(error)) {}
    [[nodiscard]] bool is_valid() const noexcept { return ok_; }
    [[nodiscard]] const std::string& error() const noexcept { return error_; }

private:
    bool ok_ = true;
    std::string error_;
};

class VAML_CORE_EXPORT ValidationRule
{
public:
    virtual ~ValidationRule() = default;
    // Validate the incoming candidate Value -> a ValidationResult.
    [[nodiscard]] virtual ValidationResult Validate(const Value& value) const;
};

class VAML_CORE_EXPORT ExceptionValidationRule : public ValidationRule
{
public:
    [[nodiscard]] ValidationResult Validate(const Value& value) const override;
};

class VAML_CORE_EXPORT DataErrorValidationRule : public ValidationRule
{
public:
    [[nodiscard]] ValidationResult Validate(const Value& value) const override;
};

class VAML_CORE_EXPORT NotifyDataErrorValidationRule : public ValidationRule
{
public:
    [[nodiscard]] ValidationResult Validate(const Value& value) const override;
};

class VAML_CORE_EXPORT ConversionValidationRule : public ValidationRule
{
public:
    [[nodiscard]] ValidationResult Validate(const Value& value) const override;
};

class VAML_CORE_EXPORT ValidationError
{
public:
    explicit ValidationError(std::string message)
        : message_(std::move(message)) {}
    [[nodiscard]] const std::string& message() const noexcept { return message_; }

private:
    std::string message_;
};

// Validation — the attached-property façade (Validation.Error / .HasError).
class VAML_CORE_EXPORT Validation
{
public:
    Validation() = delete;
    static void set_has_error(bool e) { hasError_ = e; }
    [[nodiscard]] static bool has_error() noexcept { return hasError_; }
    static void add_error(ValidationError e) { errors_.push_back(std::move(e)); }
    [[nodiscard]] static const std::vector<ValidationError>& errors() noexcept { return errors_; }

private:
    static bool hasError_;
    static std::vector<ValidationError> errors_;
};

} // namespace vaml
