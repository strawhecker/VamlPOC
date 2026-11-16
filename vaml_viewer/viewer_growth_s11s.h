// vaml_viewer — Sprint 11s VIEWER-GROWTH seam (dep:S803, the composite
// (name, owner) DP key). THE COEXISTENCE LIVE PROOF.
//
// The SPIRIT of this seam: author a sample that writers THREE same-name/
// different-owner DP pairs and prove each RESOLVES + RENDERS independently
// through the generic renderer (the 11k-6/ViewerGrowth11k RELOAD re-demonstrated
// on the composite key):
//   1. Slider.Delay  vs  RepeatButton.Delay   (WPF Slider.cs:295 AddOwner)
//   2. Popup.AllowsTransparency  vs  Window.AllowsTransparency (Popup.cs:794)
//   3. TextBlock.FontSize (FrameworkElement TextElement branch)  vs
//      Control.FontSize (Control branch) (TextBlock.cs:776)
// Plus the re-addition (S805) pairs: Image/Viewbox StretchDirection (Image.cs:
// 135), Border/Control Padding (Border.cs:142).
//
// This is the on-screen proof that the name-only DP hosing is ABOLISHED: before
// the composite key these same-name DPs could not coexist; with it, each
// resolves to its OWN owner's per-object store and renders its authored value.
//
// Headless Rail 1 (the [both-apps, not CI] window display stays the on-machine
// local gate — this is the CI-checkable half of the exact code path, the S222_1
// RELOAD re-demonstration).
//
// The real plan runner (`run_viewer_growth_s11s_plan`) wires the sample through
// a RETAINED VamlReader, Loads + finalizes (so the declared children render),
// resolves each coexistence pair through the reader's named() registry, and
// asserts each control's typed accessor reads its OWN owner's value (the
// coexistence resolution proof).
//
// The render-plan seam (`viewer_growth_s11s_render_plan`) wraps the S212
// encode_visual_slice over the default classifier so a test asserts the tree
// RENDERS (non-degenerate items) — the generic-renderer proof (S222_1 RELOAD).
//
// ADR-021 consumer path (the S503/S512/S11q precedent). NO MINIMAL REAL.
// ADR-018: ZERO new locks (single-threaded on the viewer's UI thread).

#pragma once

#include "vaml/core/DependencyObject.h"
#include "vaml/core/DependencyProperty.h"
#include "vaml/core/Element.h"        // Slider/RepeatButton/Popup/Window/TextBlock/Button/Image/Viewbox/Border
#include "vaml/core/UIElement.h"
#include "vaml/core/Value.h"
#include "vaml/core/VamlReader.h"
#include <vaml/presentation/VamlRenderer.h> // encode_visual_slice + default_visual_of
#include <vaml/vulkan/ui/UiScene.h>          // DrawItem + ui_item_degenerate

#include <memory>
#include <string>
#include <type_traits>
#include <vector>

namespace vaml::viewer {

// The outcome of the 11s viewer growth (coexistence) plan run.
struct ViewerGrowthS11sPlanResult
{
    bool ok = false;                    // loaded + finalized cleanly
    std::unique_ptr<vaml::DependencyObject> root;  // the loaded live tree
    std::unique_ptr<vaml::VamlReader> reader;     // RETAINED — keeps named() armed
    std::string detail;                 // the structured failure reason (never silent)

    // The coexistence resolution proofs — each pair resolves + reads its OWN
    // owner's value (the value the sample authored). Read through the same
    // generic `DependencyObject::Get(PropertyId::of(name, owner))` path the
    // typed accessors use (the reader authored each value under the owner its
    // composite resolution chose; reading under BOTH owners proves the two
    // same-name DPs are physically separate stores).
    bool sliderDelay = false;           // Slider.Delay == 500 (own owner)
    bool repeatDelay = false;           // RepeatButton.Delay == 120 (own owner)
    bool popupAT      = false;          // Popup.AllowsTransparency == true  (own owner)
    bool windowAT     = false;          // Window.AllowsTransparency == false (own owner)
    bool textFeFont   = false;          // TextBlock.FontSize == 24 (own owner)
    bool controlFont  = false;          // Button Control.FontSize == 14 (Control owner)
    bool imageSD = false;               // Image.StretchDirection == 1 (own owner)
    bool viewboxSD = false;             // Viewbox.StretchDirection == 2 (own owner)
    bool borderPad = false;             // Border.Padding left == 4 (own owner)
    bool controlPad = false;            // Control default Padding == 0 (Control owner)
};

// Reads an authored DP under an EXPLICIT (name, owner) composite id. This is the
// same generic path a typed accessor (Set/Get via PropertyId::of) uses — the
// reader stored the authored value under the owner its composite resolution
// chose, and reading under that owner returns it.
template <typename T>
inline bool reads_as(const vaml::DependencyObject& o, const char* name,
                     const char* owner, T want)
{
    const PropertyId id = PropertyId::of(name, owner);
    try
    {
        const vaml::Value v = o.Get(id);
        if constexpr (std::is_same_v<T, bool>)
        {
            return v.type() == vaml::ValueType::Bool && v.get<bool>() == want;
        }
        else
        {
            return v.get<T>() == want;
        }
    }
    catch (...)
    {
        return false;
    }
}

// THE REAL PLAN RUNNER (headless — the exact code the live viewer runs on the
// coexistence sample): Load `text` through a RETAINED VamlReader,
// finalize_visual_tree (the S120/S121 hand-off that RENDERS the declared
// children), then resolve each coexistence pair through the reader's named()
// registry and assert each control's typed accessor reads its OWN owner's
// authored value. Returns { ok, root, reader, …proofs, detail }.
[[nodiscard]] inline ViewerGrowthS11sPlanResult
run_viewer_growth_s11s_plan(const std::string& text)
{
    ViewerGrowthS11sPlanResult r;
    try
    {
        r.reader = std::make_unique<vaml::VamlReader>();
        r.root = r.reader->Load(text);
        if (r.root == nullptr)
            throw std::runtime_error("sample.s11s.vaml did not produce a root");
        // The S120/S121 hand-off: transfer the declared children into their
        // parents' S121 visual-children lists so the tree RENDERS them.
        (void)r.reader->finalize_visual_tree();

        // --- PAIR 1: Slider.Delay vs RepeatButton.Delay -------------------
        auto* slider = dynamic_cast<vaml::Slider*>(r.reader->named("SliderDelay"));
        auto* repeat = dynamic_cast<vaml::RepeatButton*>(r.reader->named("RepeatDelay"));
        if (slider == nullptr || repeat == nullptr)
            throw std::runtime_error("11s sample: Slider/RepeatButton not reachable by x:Name");
        r.sliderDelay = reads_as<int>(*slider, "Delay", "Slider", 500);          // Slider's OWN owner
        r.repeatDelay = reads_as<int>(*repeat, "Delay", "RepeatButton", 120);    // RepeatButton's OWN owner

        // --- PAIR 2: Popup.AllowsTransparency vs Window.AllowsTransparency -
        auto* popup = dynamic_cast<vaml::Popup*>(r.reader->named("PopupAT"));
        auto* window = dynamic_cast<vaml::Window*>(r.reader->named("WindowAT"));
        if (popup == nullptr || window == nullptr)
            throw std::runtime_error("11s sample: Popup/Window not reachable by x:Name");
        r.popupAT  = reads_as<bool>(*popup, "AllowsTransparency", "Popup", true);   // Popup's OWN owner
        r.windowAT = reads_as<bool>(*window, "AllowsTransparency", "Window", false);// Window's OWN owner

        // --- PAIR 3: TextBlock.FontSize vs Control.FontSize ---------------
        auto* tb = dynamic_cast<vaml::TextBlock*>(r.reader->named("TbFe"));
        auto* btn = dynamic_cast<vaml::Button*>(r.reader->named("BtnCtrl"));
        if (tb == nullptr || btn == nullptr)
            throw std::runtime_error("11s sample: TextBlock/Button not reachable by x:Name");
        r.textFeFont  = reads_as<double>(*tb, "FontSize", "TextBlock", 24.0); // TextBlock's OWN owner
        r.controlFont = reads_as<double>(*btn, "FontSize", "Control", 14.0);  // Control owner (Button : Control)

        // --- re-addition surface: Image/Viewbox StretchDirection + --------
        //     Border/Control Padding (S805).
        auto* img = dynamic_cast<vaml::Image*>(r.reader->named("ImgSD"));
        auto* vb  = dynamic_cast<vaml::Viewbox*>(r.reader->named("VbSD"));
        auto* brd = dynamic_cast<vaml::Border*>(r.reader->named("BrdPad"));
        if (img == nullptr || vb == nullptr || brd == nullptr)
            throw std::runtime_error("11s sample: Image/Viewbox/Border not reachable by x:Name");
        r.imageSD   = reads_as<int>(*img, "StretchDirection", "Image", 1);   // Image's OWN owner
        r.viewboxSD = reads_as<int>(*vb,  "StretchDirection", "Viewbox", 2); // Viewbox's OWN owner
        r.borderPad = reads_as<vaml::Thickness>(*brd, "Padding", "Border", vaml::Thickness{4, 4, 4, 4});
        r.controlPad = reads_as<vaml::Thickness>(*brd, "Padding", "Control", vaml::Thickness{});

        r.ok = true;
    }
    catch (const std::exception& e)
    {
        r.ok = false;
        r.detail = e.what();               // never silent
    }
    return r;
}

// The RENDER-PLAN seam: wraps the S212 `encode_visual_slice` over the default
// classifier. Arranges the root at `width`x`height` (so the S122/S124 layout
// rects are real) then encodes into `out`. Returns the number of DrawItems
// emitted. Headless + pure — the RENDER-SURFACE proof a test asserts draw counts
// against (the tree renders non-degenerate items via the generic classifier —
// the S222_1 RELOAD re-demonstration on the composite key).
[[nodiscard]] inline std::size_t
viewer_growth_s11s_render_plan(vaml::DependencyObject* root,
                               std::vector<vaml::vulkan::ui::DrawItem>& out,
                               float width = 760.0f, float height = 560.0f)
{
    if (root == nullptr)
    {
        return 0;
    }
    vaml::UIElement* ui = dynamic_cast<vaml::UIElement*>(root);
    if (ui == nullptr)
    {
        return 0;
    }
    ui->Measure(vaml::Size{width, height});
    ui->Arrange(vaml::Rect{0, 0, width, height});
    vaml::presentation::encode_visual_slice(
        *ui, vaml::presentation::default_visual_of(), out);
    return out.size();
}

} // namespace vaml::viewer
