// 11k-3 (dep:11j-9 / S131) — GridLength string parsing. PROMOTED PUBLIC from the
// 11j-9 file-local copy in VamlReader.cpp: parse_grid_length gives the Grid
// track-length mini-language ("Auto"/numeric/"N*"/*) a public, authorable,
// reusable home (a value-type resource element + the Grid-track consumer).
// WPF-faithful (GridLengthConverter.FromString: Auto / value±unit / Star).

#include "vaml/core/GridLength.h"

#include <charconv>
#include <cmath>

namespace vaml {

namespace {

[[nodiscard]] char ascii_lower(char c) noexcept
{
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

[[nodiscard]] bool ieq(std::string_view a, const char* b) noexcept
{
    std::size_t i = 0;
    for (; i < a.size(); ++i)
    {
        if (b[i] == '\0' || ascii_lower(a[i]) != ascii_lower(b[i]))
        {
            return false;
        }
    }
    return b[i] == '\0';
}

} // namespace

VAML_CORE_EXPORT
GridLength parse_grid_length(std::string_view s)
{
    if (s.empty())
    {
        throw ValueConversionException(
            ValueConversionError::MalformedValue, std::string("GridLength"),
            std::string(s), "malformed grid track length (empty)");
    }

    // Auto / auto / AUTO.
    if (ieq(s, "Auto"))
    {
        return GridLength::Auto();
    }

    // Star: "*" (weight 1) or "N*" (weight N).
    if (s.back() == '*')
    {
        if (s.size() == 1)
        {
            return GridLength::Star(1.0f);
        }
        const std::string_view num = s.substr(0, s.size() - 1);
        double w = 0.0;
        auto [ptr, ec] = std::from_chars(num.data(), num.data() + num.size(), w);
        if (ec != std::errc() || ptr != (num.data() + num.size()) || !std::isfinite(w) || w < 0.0)
        {
            throw ValueConversionException(
                ValueConversionError::MalformedValue, std::string("GridLength"),
                std::string(s), "malformed star grid track length '" + std::string(s) + "'");
        }
        return GridLength::Star(static_cast<float>(w));
    }

    // Numeric -> Pixel (an absolute length).
    double v = 0.0;
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), v);
    if (ec != std::errc() || ptr != (s.data() + s.size()) || !std::isfinite(v) || v < 0.0)
    {
        throw ValueConversionException(
            ValueConversionError::MalformedValue, std::string("GridLength"),
            std::string(s), "malformed grid track length '" + std::string(s) + "'");
    }
    return GridLength::Pixel(static_cast<float>(v));
}

} // namespace vaml
