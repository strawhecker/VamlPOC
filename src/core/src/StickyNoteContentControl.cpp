// S-kA-3 — the StickyNote content-host abstraction (11k-A, core).
//
// WPF ground truth (StickyNoteContentControl.cs): the abstract content host
// behind Save/Load/Clear/IsEmpty/Type, with StickyNoteRichTextBox (Text) and
// StickyNoteInkCanvas (Ink). The VAML mapping keeps the value-domain body
// string (NO System.Xml/base64 — the honest value-domain analogue), bounded by
// the MaxBufferSize guard.
//
// ADR-011 held (core data + measure). ZERO MINIMAL REAL.

#include "vaml/core/StickyNoteContentControl.h"

#include "vaml/core/DependencyProperty.h"
#include "vaml/core/Geometry.h"
#include "vaml/core/PropertyFlags.h"
#include "vaml/core/PropertyMetadata.h"
#include "vaml/core/TextLayout.h" // S400 — the wrap measure basis (the host sizes to content)

#include <algorithm>
#include <utility>

namespace vaml {

// The abstract host base: keep the destructor out-of-line for PCH completeness
// (clang requires the abstract virtual's dtor to be out-of-line in some TUs).
StickyNoteContentControl::~StickyNoteContentControl() = default;

// ---------------------------------------------------------------------------
// StickyNoteRichTextBox — the TEXT note body
// ---------------------------------------------------------------------------
namespace {

void register_stickynote_richbox_properties()
{
    (void)DependencyProperty::Register(
        "StickyNoteRichTextBox.Text", ValueType::String, "StickyNoteRichTextBox",
        PropertyMetadata::WithDefault(Value::String(""), PropertyFlags::None));
}

void ensure_stickynote_richbox_properties_registered()
{
    static bool registered = false;
    if (!registered)
    {
        registered = true;
        register_stickynote_richbox_properties();
    }
}

} // namespace

StickyNoteRichTextBox::StickyNoteRichTextBox()
{
    ensure_stickynote_richbox_properties_registered();
}

SetResult StickyNoteRichTextBox::set_text(const std::string& value)
{
    const SetResult r = Set(PropertyId::of("StickyNoteRichTextBox.Text"), Value::String(value));
    if (r == SetResult::APPLIED)
    {
        text_ = value;   // keep the host's owned body in lock-step
        InvalidateMeasure();
        InvalidateArrange();
    }
    return r;
}

std::string StickyNoteRichTextBox::text() const
{
    return text_;
}

void StickyNoteRichTextBox::set_document(std::unique_ptr<FlowDocument> doc)
{
    doc_ = std::move(doc);
    docObserver_ = doc_.get();
    InvalidateMeasure();
    InvalidateArrange();
}

FlowDocument* StickyNoteRichTextBox::document() const noexcept
{
    return docObserver_;
}

bool StickyNoteRichTextBox::Save(std::string& out, std::size_t maxBufferSize) const
{
    const std::string body = body_text_cargo();
    if (body.size() > maxBufferSize)
    {
        return false;   // the MaxBufferSize guard — never explodes the store
    }
    out = body;
    return true;
}

bool StickyNoteRichTextBox::Load(const std::string& body)
{
    text_ = body;
    (void)Set(PropertyId::of("StickyNoteRichTextBox.Text"), Value::String(body));
    InvalidateMeasure();
    InvalidateArrange();
    return true;
}

vaml::Size StickyNoteRichTextBox::MeasureOverride(const vaml::Size& available) const
{
    // The S400 wrap basis: the text body wraps at `available` and the host
    // sizes to its wrapped line extent (the TextBox/TextBlock precedent).
    float lineH = 1.2f * 12.0f;   // the default-font line height (FontSize 12)
    float contentH = lineH;
    if (!text_.empty())
    {
        // A simple line-count estimate from the wrapped body.
        float w = available.width > 0.0f ? available.width : 200.0f;
        std::size_t lines = 1 + std::count(text_.begin(), text_.end(), '\n');
        // A rough chars-per-line from the measure width.
        if (w > 0.0f)
        {
            lines = std::max<std::size_t>(lines, text_.size() / std::max<std::size_t>(1, static_cast<std::size_t>(w / 7.0f)));
        }
        contentH = static_cast<float>(lines) * lineH;
    }
    float w = available.width > 0.0f ? available.width : 200.0f;
    return vaml::Size{std::max(0.0f, w), std::max(0.0f, contentH)};
}

// The body cargo the Save surface produces (the value-domain text body).
std::string StickyNoteRichTextBox::body_text_cargo() const
{
    return text_;
}

// ---------------------------------------------------------------------------
// StickyNoteInkCanvas — the INK note body
// ---------------------------------------------------------------------------
namespace {

void register_stickynote_ink_properties()
{
    (void)DependencyProperty::Register(
        "StickyNoteInkCanvas.Strokes", ValueType::String, "StickyNoteInkCanvas",
        PropertyMetadata::WithDefault(Value::String(""), PropertyFlags::None));
}

void ensure_stickynote_ink_properties_registered()
{
    static bool registered = false;
    if (!registered)
    {
        registered = true;
        register_stickynote_ink_properties();
    }
}

} // namespace

StickyNoteInkCanvas::StickyNoteInkCanvas()
{
    ensure_stickynote_ink_properties_registered();
}

SetResult StickyNoteInkCanvas::set_strokes(const std::string& value)
{
    const SetResult r = Set(PropertyId::of("StickyNoteInkCanvas.Strokes"), Value::String(value));
    if (r == SetResult::APPLIED)
    {
        strokes_ = value;
        InvalidateMeasure();
        InvalidateArrange();
    }
    return r;
}

std::string StickyNoteInkCanvas::strokes() const
{
    return strokes_;
}

bool StickyNoteInkCanvas::Save(std::string& out, std::size_t maxBufferSize) const
{
    if (strokes_.size() > maxBufferSize)
    {
        return false;   // the MaxBufferSize guard
    }
    out = strokes_;
    return true;
}

bool StickyNoteInkCanvas::Load(const std::string& body)
{
    strokes_ = body;
    (void)Set(PropertyId::of("StickyNoteInkCanvas.Strokes"), Value::String(body));
    InvalidateMeasure();
    InvalidateArrange();
    return true;
}

vaml::Size StickyNoteInkCanvas::MeasureOverride(const vaml::Size& available) const
{
    // The S554 InkCanvas basis: a fixed note-body box (the ink surface).
    float w = available.width > 0.0f ? available.width : 200.0f;
    float h = available.height > 0.0f ? available.height : 160.0f;
    return vaml::Size{std::max(0.0f, w), std::max(0.0f, h)};
}

} // namespace vaml
