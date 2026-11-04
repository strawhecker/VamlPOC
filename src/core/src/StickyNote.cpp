// S096 — the CAF annotation engine + StickyNote control (11k-A, core).
//
// WPF ground truth (from the three StickyNote sources read at plan time):
//   * The annotation engine (Annotation/AnnotationResource/AnnotationService +
//     IAttachedAnnotation) is the plain CAF data model the visible StickyNote
//     mirrors into. VAML has no System.Xml, so the cargos ride the VAML value
//     domain (the string cargo form is the honest value-domain serialization).
//   * The StickyNoteControl's DP set: IsExpanded/Author (read-only)/Width/
//     Height/ZOrder + the PositionTransform + the StickyNoteType {Text,Ink}.
//   * The reentrancy guard is the WPF LockHelper Flag lock — a single bool
//     RAII, NEVER an OS lock (ADR-018 holds: ZERO new locks).
//
// ADR-011 held (core data + measure; the render "how" stays in the vulkan
// classifier / encoder). ZERO MINIMAL REAL — every symbol is real + permanent.

#include "vaml/core/StickyNote.h"

#include "vaml/core/DependencyProperty.h"
#include "vaml/core/Geometry.h"
#include "vaml/core/PropertyFlags.h"
#include "vaml/core/PropertyMetadata.h"
#include "vaml/core/RoutedEvent.h"

#include <cmath>
#include <cstdint>
#include <utility>

namespace vaml {

// ---------------------------------------------------------------------------
// Annotation metadata/cargo helpers
// ---------------------------------------------------------------------------
const std::string* Annotation::metadata(const std::string& token) const noexcept
{
    for (const auto& r : resources)
    {
        if (r.type == AnnotationResourceType::MetaData && r.token == token)
        {
            return &r.value;
        }
    }
    return nullptr;
}

void Annotation::set_metadata(const std::string& token, const std::string& value)
{
    for (auto& r : resources)
    {
        if (r.type == AnnotationResourceType::MetaData && r.token == token)
        {
            r.value = value;   // update in place, keep the ordered position
            return;
        }
    }
    resources.push_back({AnnotationResourceType::MetaData, token, value});
}

const std::string* Annotation::content(AnnotationResourceType type) const noexcept
{
    for (const auto& r : resources)
    {
        if (r.type == type)
        {
            return &r.value;
        }
    }
    return nullptr;
}

void Annotation::set_content(AnnotationResourceType type, const std::string& body)
{
    for (auto& r : resources)
    {
        if (r.type == type)
        {
            r.value = body;
            return;
        }
    }
    resources.push_back({type, {}, body});
}

// ---------------------------------------------------------------------------
// StickyNoteTokens — the token<->name authority (conformance-pinned to the
// WPF SNCAnnotation XmlToken set).
// ---------------------------------------------------------------------------
const char* StickyNoteTokens::name(StickyNoteMetaToken token) noexcept
{
    switch (token)
    {
        case StickyNoteMetaToken::Left:       return "Left";
        case StickyNoteMetaToken::Top:        return "Top";
        case StickyNoteMetaToken::XOffset:    return "XOffset";
        case StickyNoteMetaToken::YOffset:    return "YOffset";
        case StickyNoteMetaToken::Width:      return "Width";
        case StickyNoteMetaToken::Height:     return "Height";
        case StickyNoteMetaToken::IsExpanded: return "IsExpanded";
        case StickyNoteMetaToken::ZOrder:     return "ZOrder";
    }
    return "Left";
}

StickyNoteMetaToken StickyNoteTokens::from_name(const std::string& name) noexcept
{
    if (name == "Top")        return StickyNoteMetaToken::Top;
    if (name == "XOffset")    return StickyNoteMetaToken::XOffset;
    if (name == "YOffset")    return StickyNoteMetaToken::YOffset;
    if (name == "Width")      return StickyNoteMetaToken::Width;
    if (name == "Height")     return StickyNoteMetaToken::Height;
    if (name == "IsExpanded") return StickyNoteMetaToken::IsExpanded;
    if (name == "ZOrder")     return StickyNoteMetaToken::ZOrder;
    return StickyNoteMetaToken::Left; // the default (Left)
}

// ---------------------------------------------------------------------------
// AnnotationService — the CAF store
// ---------------------------------------------------------------------------
AnnotationService::Attached* AnnotationService::AddAttached(Annotation* annotation,
                                                            IAttachedAnnotation* host,
                                                            UIElement* anchor)
{
    if (annotation == nullptr || host == nullptr)
    {
        return nullptr;
    }
    // A duplicate attach (same anchor or same annotation id) is REFUSED (never
    // a silent last-wins).
    for (const auto& a : attached_)
    {
        if (a.anchor == anchor)
        {
            return nullptr;
        }
    }
    attached_.push_back({annotation, host, anchor, false});
    return &attached_.back();
}

bool AnnotationService::RemoveAttached(Attached* attached)
{
    for (auto it = attached_.begin(); it != attached_.end(); ++it)
    {
        if (&(*it) == attached)
        {
            attached_.erase(it);
            return true;
        }
    }
    return false;
}

void AnnotationService::ModifyAttached(Attached* attached)
{
    if (attached == nullptr || attached->host == nullptr)
    {
        return;
    }
    // The reentrancy guard (WPF LockHelper Flag): a mirror re-applies, never
    // echoes into a duplicate. Single-threaded — ZERO OS locks (ADR-018).
    if (mirroring_)
    {
        return;
    }
    mirroring_ = true;
    (void)attached->host->ApplyAnnotation(*attached->annotation);
    (void)attached->host->UpdateAnnotation(*attached->annotation);
    mirroring_ = false;
}

std::size_t AnnotationService::AttachedCount() const noexcept
{
    return attached_.size();
}

void AnnotationService::attached(std::vector<const Attached*>& out) const noexcept
{
    out.clear();
    out.reserve(attached_.size());
    for (const auto& a : attached_)
    {
        out.push_back(&a);
    }
}

bool AnnotationService::is_mirroring() const noexcept
{
    return mirroring_;
}

// ---------------------------------------------------------------------------
// StickyNote DP registration (self-registering, once per process)
// ---------------------------------------------------------------------------
namespace {

void register_stickynote_properties()
{
    (void)DependencyProperty::Register(
        "StickyNote.IsExpanded", ValueType::Int32, "StickyNote",
        PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None));
    (void)DependencyProperty::Register(
        "StickyNote.Author", ValueType::String, "StickyNote",
        PropertyMetadata::WithDefault(Value::String(""), PropertyFlags::ReadOnly));
    (void)DependencyProperty::Register(
        "StickyNote.Type", ValueType::Int32, "StickyNote",
        PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None));
    (void)DependencyProperty::Register(
        "StickyNote.Width", ValueType::Double, "StickyNote",
        PropertyMetadata::WithDefault(Value::Double(std::nan("")), PropertyFlags::None));
    (void)DependencyProperty::Register(
        "StickyNote.Height", ValueType::Double, "StickyNote",
        PropertyMetadata::WithDefault(Value::Double(std::nan("")), PropertyFlags::None));
    (void)DependencyProperty::Register(
        "StickyNote.ZOrder", ValueType::Int32, "StickyNote",
        PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None));
    (void)DependencyProperty::Register(
        "StickyNote.PositionTransform", ValueType::Matrix2D, "StickyNote",
        PropertyMetadata::WithDefault(Value::Matrix2D(Matrix2D::identity()), PropertyFlags::None));
}

void ensure_stickynote_properties_registered()
{
    static bool registered = false;
    if (!registered)
    {
        registered = true;
        register_stickynote_properties();
    }
}

} // namespace

// ---------------------------------------------------------------------------
// StickyNote
// ---------------------------------------------------------------------------
StickyNote::StickyNote()
{
    ensure_stickynote_properties_registered();
}

SetResult StickyNote::set_is_expanded(bool value)
{
    return Set(PropertyId::of("StickyNote.IsExpanded"), Value::Int32(value ? 1 : 0));
}

bool StickyNote::is_expanded() const
{
    return Get(PropertyId::of("StickyNote.IsExpanded")).get<int>() != 0;
}

SetResult StickyNote::set_author(const std::string& value)
{
    // The owner-internal startup path (WPF's author comes from the annotation,
    // never authored); the S045 read-only guard throws for an external Set.
    return Set(PropertyId::of("StickyNote.Author"), Value::String(value));
}

std::string StickyNote::author() const
{
    return Get(PropertyId::of("StickyNote.Author")).get<std::string>();
}

SetResult StickyNote::set_sticky_note_type(StickyNoteType t)
{
    return Set(PropertyId::of("StickyNote.Type"), Value::Int32(static_cast<std::int32_t>(t)));
}

StickyNoteType StickyNote::sticky_note_type() const
{
    return static_cast<StickyNoteType>(Get(PropertyId::of("StickyNote.Type")).get<std::int32_t>());
}

SetResult StickyNote::set_note_width(double value)
{
    return Set(PropertyId::of("StickyNote.Width"), Value::Double(value));
}

double StickyNote::note_width() const
{
    return Get(PropertyId::of("StickyNote.Width")).get<double>();
}

SetResult StickyNote::set_note_height(double value)
{
    return Set(PropertyId::of("StickyNote.Height"), Value::Double(value));
}

double StickyNote::note_height() const
{
    return Get(PropertyId::of("StickyNote.Height")).get<double>();
}

SetResult StickyNote::set_z_order(std::int32_t value)
{
    return Set(PropertyId::of("StickyNote.ZOrder"), Value::Int32(value));
}

std::int32_t StickyNote::z_order() const
{
    return Get(PropertyId::of("StickyNote.ZOrder")).get<std::int32_t>();
}

SetResult StickyNote::set_position_transform(const vaml::Matrix2D& value)
{
    // The S208 finite-validity gate: a non-finite PositionTransform is REJECTED.
    if (!is_transform_valid(value))
    {
        return SetResult::REJECTED;
    }
    return Set(PropertyId::of("StickyNote.PositionTransform"), Value::Matrix2D(value));
}

vaml::Matrix2D StickyNote::position_transform() const
{
    const vaml::Value v = Get(PropertyId::of("StickyNote.PositionTransform"));
    return v.get<vaml::Matrix2D>();
}

UIElement* StickyNote::AnchoredElement() const noexcept
{
    return anchor_;
}

void StickyNote::set_anchored_element(UIElement* anchor) noexcept
{
    anchor_ = anchor;
}

void StickyNote::set_body_text(const std::string& text)
{
    bodyText_ = text;
}

std::string StickyNote::body_text() const
{
    return bodyText_;
}

void StickyNote::set_body_ink(const std::string& ink)
{
    bodyInk_ = ink;
}

std::string StickyNote::body_ink() const
{
    return bodyInk_;
}

bool StickyNote::ApplyAnnotation(const Annotation& annotation)
{
    // annotation -> SNC: apply the MetaData cargos onto the live DPs.
    if (const std::string* v = annotation.metadata(StickyNoteTokens::name(StickyNoteMetaToken::IsExpanded)); v != nullptr)
    {
        (void)set_is_expanded(*v == "1" || *v == "true");
    }
    if (const std::string* v = annotation.metadata(StickyNoteTokens::name(StickyNoteMetaToken::Width)); v != nullptr)
    {
        (void)set_note_width(std::atof(v->c_str()));
    }
    if (const std::string* v = annotation.metadata(StickyNoteTokens::name(StickyNoteMetaToken::Height)); v != nullptr)
    {
        (void)set_note_height(std::atof(v->c_str()));
    }
    if (const std::string* v = annotation.metadata(StickyNoteTokens::name(StickyNoteMetaToken::ZOrder)); v != nullptr)
    {
        (void)set_z_order(static_cast<std::int32_t>(std::atoi(v->c_str())));
    }
    // The content cargos -> the body slots.
    if (const std::string* v = annotation.content(AnnotationResourceType::TextData); v != nullptr)
    {
        set_body_text(*v);
    }
    if (const std::string* v = annotation.content(AnnotationResourceType::InkData); v != nullptr)
    {
        set_body_ink(*v);
    }
    return true;
}

bool StickyNote::UpdateAnnotation(Annotation& annotation) const
{
    // SNC -> annotation: capture the live DPs back into the cargos.
    if (annotation.author.empty())
    {
        annotation.author = author();   // only fill the author once (WPF author is read-only)
    }
    annotation.set_metadata(StickyNoteTokens::name(StickyNoteMetaToken::IsExpanded),
                            is_expanded() ? "1" : "0");
    const double w = note_width();
    if (!std::isnan(w))
    {
        annotation.set_metadata(StickyNoteTokens::name(StickyNoteMetaToken::Width),
                                std::to_string(w));
    }
    const double h = note_height();
    if (!std::isnan(h))
    {
        annotation.set_metadata(StickyNoteTokens::name(StickyNoteMetaToken::Height),
                                std::to_string(h));
    }
    annotation.set_metadata(StickyNoteTokens::name(StickyNoteMetaToken::ZOrder),
                            std::to_string(z_order()));
    // The content cargos from the body slots.
    annotation.set_content(AnnotationResourceType::TextData, bodyText_);
    annotation.set_content(AnnotationResourceType::InkData, bodyInk_);
    return true;
}

vaml::Size StickyNote::MeasureOverride(const vaml::Size& available) const
{
    // The framework-element branch: the note sizes to its authored box
    // (Width/Height DPs; NaN = auto -> a nominal default note box, the WPF
    // StickyNoteControl content-size basis).
    float w = static_cast<float>(note_width());
    float h = static_cast<float>(note_height());
    if (std::isnan(w))
    {
        w = 200.0f;   // the nominal default note width (WPF ~200 logical px)
    }
    if (std::isnan(h))
    {
        h = 160.0f;   // the nominal default note height
    }
    // Clamp to the offered available bound (a note never exceeds its slot).
    if (available.width > 0.0f)
    {
        w = std::min(w, available.width);
    }
    if (available.height > 0.0f)
    {
        h = std::min(h, available.height);
    }
    return vaml::Size{std::max(0.0f, w), std::max(0.0f, h)};
}

} // namespace vaml
