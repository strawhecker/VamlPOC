// S089 (dep:S088) — Color string parsing. RESOLVES the S030 Color MINIMAL REAL
// marker: parse_color gives Color its full "#RRGGBB"/"#AARRGGBB" parsing
// semantics. The sentinel is removed (resolution is part of this feature step,
// not a backfill — NO-GAP).
//
// 11j-3 (GAP CLOSE) — the WPF named-color table (dep:S089/S090). S089 explicitly
// scoped named colors OUT ("named-color tables are a later brush/resource step");
// 11j-3 RESOLVES that recorded NO-GAP: parse_color now accepts the full WPF
// named-color set (case-insensitive) -> Color, closing the `MalformedValue`
// color genre across the external WPF corpus + unblocking `Transparent`
// overlays. The hex path is UNCHANGED (a bare identifier that is NOT a valid
// hex form falls through to the named-color lookup; an unknown identifier ->
// MalformedValue exactly as before).

#include "vaml/core/Color.h"

namespace vaml {

namespace {

// Maps one ASCII hex nibble to its value, or -1 if not a hex digit.
// Case-insensitive per the Color.h contract.
int hexNibble(char c) noexcept
{
    if (c >= '0' && c <= '9')
    {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f')
    {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F')
    {
        return c - 'A' + 10;
    }
    return -1;
}

[[noreturn]] void throw_bad_color(std::string_view literal)
{
    throw ValueConversionException(
        ValueConversionError::MalformedValue, std::string("Color"), std::string(literal),
        "cannot convert '" + std::string(literal) + "' to Color (MalformedValue)");
}

// ---------------------------------------------------------------------------
// 11j-3 — the WPF named-color table (the S089 recorded NO-GAP RESOLVED here).
//
// A constexpr-friendly array of {name, r, g, b, a}. `Transparent` is
// fully-transparent black (0,0,0,0) — the `Transparent` overlay the sprint
// names. The set is the WPF standard named colors (the ~140 System.Windows.Media
// Colors names). Lookup is case-insensitive via an ASCII fold (NOT `iequals` —
// the S090 Bool hex fold is deliberately left untouched; this table folds the
// FULL A-Z alphabet because color names like "AliceBlue" span all letters).
//
// SCOPE HONESTY (recorded, NO-GAP not silent): only the WPF named-color NAMES
// (plus Transparent) are in scope here. The WPF color *scRGB* (ContextColor/
// #sc#...) and named *brush* forms (SystemColors.… as a color) stay out — not
// part of the corpus color loads. A named color is an 8-bit RGBA exactly like
// a hex color; the S364 SolidColorBrush / Foreground paths consume the same
// Color via the ONE Color converter (single path).
// ---------------------------------------------------------------------------
struct NamedColor
{
    const char*  name;
    std::uint8_t r, g, b, a;
};

constexpr NamedColor kNamedColors[] = {
    {"Transparent", 0x00, 0x00, 0x00, 0x00}, // fully-transparent BLACK (WPF-faithful)
    {"AliceBlue", 0xF0, 0xF8, 0xFF, 0xFF},
    {"AntiqueWhite", 0xFA, 0xEB, 0xD7, 0xFF},
    {"Aqua", 0x00, 0xFF, 0xFF, 0xFF},
    {"Aquamarine", 0x7F, 0xFF, 0xD4, 0xFF},
    {"Azure", 0xF0, 0xFF, 0xFF, 0xFF},
    {"Beige", 0xF5, 0xF5, 0xDC, 0xFF},
    {"Bisque", 0xFF, 0xE4, 0xC4, 0xFF},
    {"Black", 0x00, 0x00, 0x00, 0xFF},
    {"BlanchedAlmond", 0xFF, 0xEB, 0xCD, 0xFF},
    {"Blue", 0x00, 0x00, 0xFF, 0xFF},
    {"BlueViolet", 0x8A, 0x2B, 0xE2, 0xFF},
    {"Brown", 0xA5, 0x2A, 0x2A, 0xFF},
    {"BurlyWood", 0xDE, 0xB8, 0x87, 0xFF},
    {"CadetBlue", 0x5F, 0x9E, 0xA0, 0xFF},
    {"Chartreuse", 0x7F, 0xFF, 0x00, 0xFF},
    {"Chocolate", 0xD2, 0x69, 0x1E, 0xFF},
    {"Coral", 0xFF, 0x7F, 0x50, 0xFF},
    {"CornflowerBlue", 0x64, 0x95, 0xED, 0xFF},
    {"Cornsilk", 0xFF, 0xF8, 0xDC, 0xFF},
    {"Crimson", 0xDC, 0x14, 0x3C, 0xFF},
    {"Cyan", 0x00, 0xFF, 0xFF, 0xFF},
    {"DarkBlue", 0x00, 0x00, 0x8B, 0xFF},
    {"DarkCyan", 0x00, 0x8B, 0x8B, 0xFF},
    {"DarkGoldenrod", 0xB8, 0x86, 0x0B, 0xFF},
    {"DarkGray", 0xA9, 0xA9, 0xA9, 0xFF},
    {"DarkGreen", 0x00, 0x64, 0x00, 0xFF},
    {"DarkKhaki", 0xBD, 0xB7, 0x6B, 0xFF},
    {"DarkMagenta", 0x8B, 0x00, 0x8B, 0xFF},
    {"DarkOliveGreen", 0x55, 0x6B, 0x2F, 0xFF},
    {"DarkOrange", 0xFF, 0x8C, 0x00, 0xFF},
    {"DarkOrchid", 0x99, 0x32, 0xCC, 0xFF},
    {"DarkRed", 0x8B, 0x00, 0x00, 0xFF},
    {"DarkSalmon", 0xE9, 0x96, 0x7A, 0xFF},
    {"DarkSeaGreen", 0x8F, 0xBC, 0x8F, 0xFF},
    {"DarkSlateBlue", 0x48, 0x3D, 0x8B, 0xFF},
    {"DarkSlateGray", 0x2F, 0x4F, 0x4F, 0xFF},
    {"DarkTurquoise", 0x00, 0xCE, 0xD1, 0xFF},
    {"DarkViolet", 0x94, 0x00, 0xD3, 0xFF},
    {"DeepPink", 0xFF, 0x14, 0x93, 0xFF},
    {"DeepSkyBlue", 0x00, 0xBF, 0xFF, 0xFF},
    {"DimGray", 0x69, 0x69, 0x69, 0xFF},
    {"DodgerBlue", 0x1E, 0x90, 0xFF, 0xFF},
    {"Firebrick", 0xB2, 0x22, 0x22, 0xFF},
    {"FloralWhite", 0xFF, 0xFA, 0xF0, 0xFF},
    {"ForestGreen", 0x22, 0x8B, 0x22, 0xFF},
    {"Fuchsia", 0xFF, 0x00, 0xFF, 0xFF},
    {"Gainsboro", 0xDC, 0xDC, 0xDC, 0xFF},
    {"GhostWhite", 0xF8, 0xF8, 0xFF, 0xFF},
    {"Gold", 0xFF, 0xD7, 0x00, 0xFF},
    {"Goldenrod", 0xDA, 0xA5, 0x20, 0xFF},
    {"Gray", 0x80, 0x80, 0x80, 0xFF},
    {"Green", 0x00, 0x80, 0x00, 0xFF},
    {"GreenYellow", 0xAD, 0xFF, 0x2F, 0xFF},
    {"Honeydew", 0xF0, 0xFF, 0xF0, 0xFF},
    {"HotPink", 0xFF, 0x69, 0xB4, 0xFF},
    {"IndianRed", 0xCD, 0x5C, 0x5C, 0xFF},
    {"Indigo", 0x4B, 0x00, 0x82, 0xFF},
    {"Ivory", 0xFF, 0xFF, 0xF0, 0xFF},
    {"Khaki", 0xF0, 0xE6, 0x8C, 0xFF},
    {"Lavender", 0xE6, 0xE6, 0xFA, 0xFF},
    {"LavenderBlush", 0xFF, 0xF0, 0xF5, 0xFF},
    {"LawnGreen", 0x7C, 0xFC, 0x00, 0xFF},
    {"LemonChiffon", 0xFF, 0xFA, 0xCD, 0xFF},
    {"LightBlue", 0xAD, 0xD8, 0xE6, 0xFF},
    {"LightCoral", 0xF0, 0x80, 0x80, 0xFF},
    {"LightCyan", 0xE0, 0xFF, 0xFF, 0xFF},
    {"LightGoldenrodYellow", 0xFA, 0xFA, 0xD2, 0xFF},
    {"LightGray", 0xD3, 0xD3, 0xD3, 0xFF},
    {"LightGreen", 0x90, 0xEE, 0x90, 0xFF},
    {"LightPink", 0xFF, 0xB6, 0xC1, 0xFF},
    {"LightSalmon", 0xFF, 0xA0, 0x7A, 0xFF},
    {"LightSeaGreen", 0x20, 0xB2, 0xAA, 0xFF},
    {"LightSkyBlue", 0x87, 0xCE, 0xFA, 0xFF},
    {"LightSlateGray", 0x77, 0x88, 0x99, 0xFF},
    {"LightSteelBlue", 0xB0, 0xC4, 0xDE, 0xFF},
    {"LightYellow", 0xFF, 0xFF, 0xE0, 0xFF},
    {"Lime", 0x00, 0xFF, 0x00, 0xFF},
    {"LimeGreen", 0x32, 0xCD, 0x32, 0xFF},
    {"Linen", 0xFA, 0xF0, 0xE6, 0xFF},
    {"Magenta", 0xFF, 0x00, 0xFF, 0xFF},
    {"Maroon", 0x80, 0x00, 0x00, 0xFF},
    {"MediumAquamarine", 0x66, 0xCD, 0xAA, 0xFF},
    {"MediumBlue", 0x00, 0x00, 0xCD, 0xFF},
    {"MediumOrchid", 0xBA, 0x55, 0xD3, 0xFF},
    {"MediumPurple", 0x93, 0x70, 0xDB, 0xFF},
    {"MediumSeaGreen", 0x3C, 0xB3, 0x71, 0xFF},
    {"MediumSlateBlue", 0x7B, 0x68, 0xEE, 0xFF},
    {"MediumSpringGreen", 0x00, 0xFA, 0x9A, 0xFF},
    {"MediumTurquoise", 0x48, 0xD1, 0xCC, 0xFF},
    {"MediumVioletRed", 0xC7, 0x15, 0x85, 0xFF},
    {"MidnightBlue", 0x19, 0x19, 0x70, 0xFF},
    {"MintCream", 0xF5, 0xFF, 0xFA, 0xFF},
    {"MistyRose", 0xFF, 0xE4, 0xE1, 0xFF},
    {"Moccasin", 0xFF, 0xE4, 0xB5, 0xFF},
    {"NavajoWhite", 0xFF, 0xDE, 0xAD, 0xFF},
    {"Navy", 0x00, 0x00, 0x80, 0xFF},
    {"OldLace", 0xFD, 0xF5, 0xE6, 0xFF},
    {"Olive", 0x80, 0x80, 0x00, 0xFF},
    {"OliveDrab", 0x6B, 0x8E, 0x23, 0xFF},
    {"Orange", 0xFF, 0xA5, 0x00, 0xFF},
    {"OrangeRed", 0xFF, 0x45, 0x00, 0xFF},
    {"Orchid", 0xDA, 0x70, 0xD6, 0xFF},
    {"PaleGoldenrod", 0xEE, 0xE8, 0xAA, 0xFF},
    {"PaleGreen", 0x98, 0xFB, 0x98, 0xFF},
    {"PaleTurquoise", 0xAF, 0xEE, 0xEE, 0xFF},
    {"PaleVioletRed", 0xDB, 0x70, 0x93, 0xFF},
    {"PapayaWhip", 0xFF, 0xEF, 0xD5, 0xFF},
    {"PeachPuff", 0xFF, 0xDA, 0xB9, 0xFF},
    {"Peru", 0xCD, 0x85, 0x3F, 0xFF},
    {"Pink", 0xFF, 0xC0, 0xCB, 0xFF},
    {"Plum", 0xDD, 0xA0, 0xDD, 0xFF},
    {"PowderBlue", 0xB0, 0xE0, 0xE6, 0xFF},
    {"Purple", 0x80, 0x00, 0x80, 0xFF},
    {"RebeccaPurple", 0x66, 0x33, 0x99, 0xFF},
    {"Red", 0xFF, 0x00, 0x00, 0xFF},
    {"RosyBrown", 0xBC, 0x8F, 0x8F, 0xFF},
    {"RoyalBlue", 0x41, 0x69, 0xE1, 0xFF},
    {"SaddleBrown", 0x8B, 0x45, 0x13, 0xFF},
    {"Salmon", 0xFA, 0x80, 0x72, 0xFF},
    {"SandyBrown", 0xF4, 0xA4, 0x60, 0xFF},
    {"SeaGreen", 0x2E, 0x8B, 0x57, 0xFF},
    {"SeaShell", 0xFF, 0xF5, 0xEE, 0xFF},
    {"Sienna", 0xA0, 0x52, 0x2D, 0xFF},
    {"Silver", 0xC0, 0xC0, 0xC0, 0xFF},
    {"SkyBlue", 0x87, 0xCE, 0xEB, 0xFF},
    {"SlateBlue", 0x6A, 0x5A, 0xCD, 0xFF},
    {"SlateGray", 0x70, 0x80, 0x90, 0xFF},
    {"Snow", 0xFF, 0xFA, 0xFA, 0xFF},
    {"SpringGreen", 0x00, 0xFF, 0x7F, 0xFF},
    {"SteelBlue", 0x46, 0x82, 0xB4, 0xFF},
    {"Tan", 0xD2, 0xB4, 0x8C, 0xFF},
    {"Teal", 0x00, 0x80, 0x80, 0xFF},
    {"Thistle", 0xD8, 0xBF, 0xD8, 0xFF},
    {"Tomato", 0xFF, 0x63, 0x47, 0xFF},
    {"Turquoise", 0x40, 0xE0, 0xD0, 0xFF},
    {"Violet", 0xEE, 0x82, 0xEE, 0xFF},
    {"Wheat", 0xF5, 0xDE, 0xB3, 0xFF},
    {"White", 0xFF, 0xFF, 0xFF, 0xFF},
    {"WhiteSmoke", 0xF5, 0xF5, 0xF5, 0xFF},
    {"Yellow", 0xFF, 0xFF, 0x00, 0xFF},
    {"YellowGreen", 0x9A, 0xCD, 0x32, 0xFF},
};

constexpr std::size_t kNamedColorCount = sizeof(kNamedColors) / sizeof(kNamedColors[0]);

// ASCII case fold (full A-Z — distinct from the S090 `iequals` A..F-only fold;
// see the note above). Returns the lowercase of an upper-case ASCII letter,
// else the char unchanged.
char asciiLower(char c) noexcept
{
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

// Case-insensitive lookup of `literal` in the named-color table. Returns a
// pointer to the entry, or nullptr when `literal` is not a known WPF name.
const NamedColor* lookup_named_color(std::string_view literal) noexcept
{
    for (std::size_t i = 0; i < kNamedColorCount; ++i)
    {
        const char* n = kNamedColors[i].name;
        std::size_t j = 0;
        for (; j < literal.size(); ++j)
        {
            if (n[j] == '\0' || asciiLower(n[j]) != asciiLower(literal[j]))
            {
                break;
            }
        }
        // Match iff every literal char matched AND the table name ends here.
        if (j == literal.size() && n[j] == '\0')
        {
            return &kNamedColors[i];
        }
    }
    return nullptr;
}

} // namespace

VAML_CORE_EXPORT
Color parse_color(std::string_view literal)
{
    // Optional leading '#'. With it: 6 digits (RRGGBB, opaque alpha) or 8
    // digits (AARRGGBB); without it the same digit counts.
    std::size_t start = 0;
    if (!literal.empty() && literal[0] == '#')
    {
        start = 1;
    }

    const std::size_t digits = literal.size() - start;
    // A hex color is ALWAYS 6 or 8 digits (with or without '#'); ONLY a
    // non-hex-form literal can be a named color. 11j-3: if the length is not a
    // hex form, fall through to the named-color table BEFORE rejecting —
    // `Transparent`, `AliceBlue`, `#FF0000` all reach a Color, while an
    // unknown identifier still throws MalformedValue (the S089 contract, never
    // a silent color). A "#"-prefixed literal is NEVER a name (names never
    // start with '#').
    if (digits != 6 && digits != 8)
    {
        if (start == 0)
        {
            if (const NamedColor* named = lookup_named_color(literal))
            {
                return Color{ named->r, named->g, named->b, named->a };
            }
        }
        throw_bad_color(literal);
    }

    // Accumulate each byte as exactly two hex nibbles (first nibble high).
    std::uint8_t rgba[4] = {0, 0, 0, 0};
    for (std::size_t i = 0; i < digits; ++i)
    {
        const int hi = hexNibble(literal[start + i]);
        if (hi < 0)
        {
            // A 6/8-length literal that is NOT a valid hex digit is neither a
            // valid hex color nor a hex form — 11j-3: try the named-color
            // table (a bare identifier like "Red") before rejecting. A
            // "#"-prefixed literal is never a name (reject).
            if (start == 0)
            {
                if (const NamedColor* named = lookup_named_color(literal))
                {
                    return Color{ named->r, named->g, named->b, named->a };
                }
            }
            throw_bad_color(literal);
        }
        rgba[i / 2] = static_cast<std::uint8_t>((rgba[i / 2] << 4) | static_cast<std::uint8_t>(hi));
    }

    if (digits == 6)
    {
        // "#RRGGBB" -> RGB; alpha defaults to fully opaque.
        return Color{rgba[0], rgba[1], rgba[2], 255};
    }
    // "#AARRGGBB" -> A,R,G,B in that order (WPF-compatible argb).
    return Color{rgba[1], rgba[2], rgba[3], rgba[0]};
}

} // namespace vaml
