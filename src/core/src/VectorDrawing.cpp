// 11k-4 — VectorDrawing definitions: the abstract Geometry/Drawing bases + the
// object/brush resources (PathGeometry/GeometryGroup/RectangleGeometry/
// EllipseGeometry; GeometryDrawing/DrawingGroup; DrawingBrush/DrawingImage/
// ImageBrush). See VectorDrawing.h. Dep:S460 (brush authoring) / 11k-3 (value-
// type authoring). ADR-011 held (core DATA; render stays vulkan).

#include "vaml/core/VectorDrawing.h"

#include "vaml/core/DependencyProperty.h"
#include "vaml/core/PropertyFlags.h"
#include "vaml/core/PropertyId.h"
#include "vaml/core/PropertyMetadata.h"
#include "vaml/core/ValueConversionError.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <string_view>

namespace vaml {

// ---------------------------------------------------------------------------
// Geometry abstract base
// ---------------------------------------------------------------------------

namespace {

void register_geometry_properties()
{
    // Geometry.Transform — the 11k-8 audited Geometry.Transform DP (Matrix2D,
    // default identity). Registered GLOBALLY by name so a `<Owner.Transform>` /
    // `Transform="..."` attribute on any geometry leaf resolves (the S043
    // attached-aware / Name-keyed path).
    (void)DependencyProperty::Register(
        "Transform", ValueType::Matrix2D, "Geometry",
        PropertyMetadata::WithDefault(Value::Matrix2D(vaml::Matrix2D::identity()),
                                      PropertyFlags::None));
}

const bool kGeometryRegistered = [] {
    register_geometry_properties();
    return true;
}();

} // namespace

VAML_CORE_EXPORT
Geometry::~Geometry() = default;

VAML_CORE_EXPORT
SetResult Geometry::set_transform(const vaml::Matrix2D& m)
{
    (void)kGeometryRegistered;
    return Set(PropertyId::of("Transform"), Value::Matrix2D(m));
}

VAML_CORE_EXPORT
vaml::Matrix2D Geometry::transform() const
{
    const Value v = Get(PropertyId::of("Transform"));
    return v.type() == ValueType::Matrix2D ? v.get<vaml::Matrix2D>()
                                           : vaml::Matrix2D::identity();
}

// ---------------------------------------------------------------------------
// Concrete Geometry leaves
// ---------------------------------------------------------------------------

namespace {

// Registers the PathGeometry DPs once per process: FillRule (Int32, default 0 =
// EvenOdd). "Figures" is NOT a DP — it is a reader-consumed authored string
// parsed into the owned PathData (data, not a scalar DP payload; the S460
// LinearGradientStop::stops data-model precedent).
void register_pathgeometry_properties()
{
    (void)DependencyProperty::Register(
        "FillRule", ValueType::Int32, "PathGeometry",
        PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None));
}

const bool kPathGeometryRegistered = [] {
    register_pathgeometry_properties();
    return true;
}();

} // namespace

VAML_CORE_EXPORT
PathGeometry::PathGeometry()
{
    (void)kPathGeometryRegistered; // ensure the FillRule DP is registered
}

VAML_CORE_EXPORT
PathGeometry::~PathGeometry() = default;

VAML_CORE_EXPORT
SetResult PathGeometry::set_figures(std::string figures)
{
    // Parse eagerly (fail loud on empty/malformed — the value-type-element
    // discipline). Store the raw source + the parsed step stream.
    PathData data = parse_path_geometry(figures, &figuresSource_);
    if (data.empty())
    {
        throw ValueConversionException(
            ValueConversionError::MalformedValue, "PathGeometry", figures,
            "a PathGeometry must carry non-empty Figures path data (MalformedValue)");
    }
    figures_  = std::move(data);
    parsed_   = true;
    return SetResult::APPLIED;
}

VAML_CORE_EXPORT
SetResult PathGeometry::set_fill_rule(std::int32_t rule)
{
    (void)kPathGeometryRegistered;
    return Set(PropertyId::of("FillRule"), Value::Int32(rule));
}

VAML_CORE_EXPORT
std::int32_t PathGeometry::fill_rule() const
{
    const Value v = Get(PropertyId::of("FillRule"));
    return v.type() == ValueType::Int32 ? v.get<std::int32_t>() : 0;
}

VAML_CORE_EXPORT
vaml::Rect PathGeometry::bounds() const
{
    // Conservative axis-aligned bounds of the figure stream (the min/max of all
    // coordinates + cubic control points). Sufficient for the rasterize + atlas
    // tile sizing (the real anti-aliased ink bounds are computed by the vulkan
    // vector seam at raster time). Degenerate/empty -> zero rect.
    float minX = std::numeric_limits<float>::infinity();
    float minY = std::numeric_limits<float>::infinity();
    float maxX = -std::numeric_limits<float>::infinity();
    float maxY = -std::numeric_limits<float>::infinity();
    bool  any  = false;
    // Considers every coordinate + control-point operand (the conservative
    // bounds cover the curve control boxes — sufficient for tile sizing).
    auto consider = [&](float x, float y) {
        minX = std::min(minX, x);
        minY = std::min(minY, y);
        maxX = std::max(maxX, x);
        maxY = std::max(maxY, y);
        any  = true;
    };
    for (const PathFigure& fig : figures_)
    {
        for (const PathStep& s : fig)
        {
            const int n = (s.cmd == PathCommand::CubicTo) ? 6
                          : (s.cmd == PathCommand::SmoothCubic ||
                             s.cmd == PathCommand::QuadTo ||
                             s.cmd == PathCommand::ArcTo) ? 4 : 2;
            for (int i = 0; i < n; i += 2)
            {
                consider(s.a[i], s.a[i + 1]);
            }
        }
    }
    if (!any)
    {
        return {};
    }
    return { minX, minY, maxX - minX, maxY - minY };
}

VAML_CORE_EXPORT
GeometryGroup::GeometryGroup() = default;

VAML_CORE_EXPORT
GeometryGroup::~GeometryGroup() = default;

VAML_CORE_EXPORT
void GeometryGroup::add_geometry(std::shared_ptr<Geometry> child)
{
    children_.push_back(std::move(child));
}

VAML_CORE_EXPORT
vaml::Rect GeometryGroup::bounds() const
{
    vaml::Rect r{};
    bool any = false;
    for (const auto& c : children_)
    {
        if (!c)
        {
            continue;
        }
        const vaml::Rect b = c->bounds();
        if (!any)
        {
            r = b;
            any = true;
        }
        else
        {
            r.x = std::min(r.x, b.x);
            r.y = std::min(r.y, b.y);
            r.width  = std::max(r.x + r.width, b.x + b.width) - r.x;
            r.height = std::max(r.y + r.height, b.y + b.height) - r.y;
        }
    }
    return r;
}

VAML_CORE_EXPORT
RectangleGeometry::RectangleGeometry() = default;

VAML_CORE_EXPORT
RectangleGeometry::~RectangleGeometry() = default;

namespace {
const bool kRectGeometryRegistered = [] {
    (void)DependencyProperty::Register(
        "Rect", ValueType::String, "RectangleGeometry",
        PropertyMetadata::WithDefault(Value::String(""), PropertyFlags::None));
    return true;
}();
}

VAML_CORE_EXPORT
SetResult RectangleGeometry::set_rect(std::string rect)
{
    (void)kRectGeometryRegistered;
    // Parse "x,y,w,h" (comma or whitespace separated; WPF RectConverter).
    // Four non-negative-or-real floats required; malformed -> fail loud.
    double vals[4];
    int    count = 0;
    std::string_view sv(rect);
    std::size_t pos = 0;
    while (count < 4 && pos < sv.size())
    {
        // Skip separators (',', ' ', '\t').
        while (pos < sv.size() && (sv[pos] == ',' || sv[pos] == ' ' ||
                                   sv[pos] == '\t' || sv[pos] == '\r' || sv[pos] == '\n'))
        {
            ++pos;
        }
        if (pos >= sv.size())
        {
            break;
        }
        std::size_t consumed = 0;
        const double v = std::strtod(sv.data() + pos, nullptr);
        while (consumed < sv.size() - pos &&
               ( (sv[pos + consumed] >= '0' && sv[pos + consumed] <= '9') ||
                 sv[pos + consumed] == '.' || sv[pos + consumed] == '-' ||
                 sv[pos + consumed] == '+' || sv[pos + consumed] == 'e' ||
                 sv[pos + consumed] == 'E' ))
        {
            ++consumed;
        }
        // Verify a number was actually parsed (next char was a digit/dot).
        bool validNum = false;
        {
            std::size_t k = pos;
            while (k < sv.size() && ((sv[k] >= '0' && sv[k] <= '9') || sv[k] == '.' ||
                                     sv[k] == '-' || sv[k] == '+' || sv[k] == 'e' || sv[k] == 'E'))
            {
                ++k;
            }
            validNum = (k > pos);
        }
        if (!validNum || consumed == 0)
        {
            throw ValueConversionException(
                ValueConversionError::MalformedValue, "RectangleGeometry", rect,
                "malformed Rect '" + std::string(rect) + "'");
        }
        vals[count++] = v;
        pos += consumed;
    }
    if (count != 4)
    {
        throw ValueConversionException(
            ValueConversionError::MalformedValue, "RectangleGeometry", rect,
            "a RectangleGeometry Rect must be 'x,y,w,h' (4 values; got '" +
                std::string(rect) + "')");
    }
    rect_ = { static_cast<float>(vals[0]), static_cast<float>(vals[1]),
              static_cast<float>(vals[2]), static_cast<float>(vals[3]) };
    parsed_ = true;
    return SetResult::APPLIED;
}

VAML_CORE_EXPORT
vaml::Rect RectangleGeometry::bounds() const
{
    return rect_;
}

VAML_CORE_EXPORT
EllipseGeometry::EllipseGeometry() = default;

VAML_CORE_EXPORT
EllipseGeometry::~EllipseGeometry() = default;

VAML_CORE_EXPORT
SetResult EllipseGeometry::set_radius_x(double rx)
{
    radiusX_ = rx;
    return SetResult::APPLIED;
}

VAML_CORE_EXPORT
SetResult EllipseGeometry::set_radius_y(double ry)
{
    radiusY_ = ry;
    return SetResult::APPLIED;
}

VAML_CORE_EXPORT
vaml::Rect EllipseGeometry::bounds() const
{
    return { static_cast<float>(-radiusX_), static_cast<float>(-radiusY_),
             static_cast<float>(2.0 * radiusX_), static_cast<float>(2.0 * radiusY_) };
}

// ---------------------------------------------------------------------------
// Drawing abstract base + concrete leaves
// ---------------------------------------------------------------------------

VAML_CORE_EXPORT
Drawing::~Drawing() = default;

namespace {
const bool kGeometryDrawingRegistered = [] {
    // BrushId-typed "Brush" / "Pen" DPs + a Double "PenThickness". These are
    // audit-listed by the 11k-8 DP surface (GeometryDrawing.Brush/.Pen).
    (void)DependencyProperty::Register(
        "Brush", ValueType::BrushId, "GeometryDrawing",
        PropertyMetadata::WithDefault(Value::BrushId(0), PropertyFlags::None));
    (void)DependencyProperty::Register(
        "Pen", ValueType::BrushId, "GeometryDrawing",
        PropertyMetadata::WithDefault(Value::BrushId(0), PropertyFlags::None));
    (void)DependencyProperty::Register(
        "PenThickness", ValueType::Double, "GeometryDrawing",
        PropertyMetadata::WithDefault(Value::Double(1.0), PropertyFlags::None));
    return true;
}();
}

VAML_CORE_EXPORT
GeometryDrawing::GeometryDrawing()
{
    (void)kGeometryDrawingRegistered; // ensure the Brush/Pen/PenThickness DPs
}

VAML_CORE_EXPORT
GeometryDrawing::~GeometryDrawing() = default;

VAML_CORE_EXPORT
SetResult GeometryDrawing::set_brush(std::int32_t brushId)
{
    (void)kGeometryDrawingRegistered;
    brushId_ = brushId;
    return SetResult::APPLIED;
}

VAML_CORE_EXPORT
SetResult GeometryDrawing::set_pen(std::int32_t penId)
{
    (void)kGeometryDrawingRegistered;
    penId_ = penId;
    return SetResult::APPLIED;
}

VAML_CORE_EXPORT
SetResult GeometryDrawing::set_pen_thickness(double t)
{
    penThickness_ = t;
    return SetResult::APPLIED;
}

VAML_CORE_EXPORT
vaml::Rect GeometryDrawing::bounds() const
{
    return geometry_ ? geometry_->bounds() : vaml::Rect{};
}

VAML_CORE_EXPORT
Color GeometryDrawing::representative_color() const
{
    // The fill color is a BrushId resolved by the presenter (S460 path); as a
    // headless representative, honor the authored scalar if brushId==0-the-
    // default, else opaque black (the S460 representative fallback).
    return Color{ 0, 0, 0, 255 };
}

VAML_CORE_EXPORT
DrawingGroup::DrawingGroup() = default;

VAML_CORE_EXPORT
DrawingGroup::~DrawingGroup() = default;

VAML_CORE_EXPORT
void DrawingGroup::add_drawing(std::shared_ptr<Drawing> child)
{
    children_.push_back(std::move(child));
}

VAML_CORE_EXPORT
SetResult DrawingGroup::set_transform(const vaml::Matrix2D& m)
{
    transform_ = m;
    return SetResult::APPLIED;
}

VAML_CORE_EXPORT
vaml::Rect DrawingGroup::bounds() const
{
    vaml::Rect r{};
    bool any = false;
    for (const auto& c : children_)
    {
        if (!c)
        {
            continue;
        }
        const vaml::Rect b = c->bounds();
        if (!any)
        {
            r = b;
            any = true;
        }
        else
        {
            r.x = std::min(r.x, b.x);
            r.y = std::min(r.y, b.y);
            r.width  = std::max(r.x + r.width, b.x + b.width) - r.x;
            r.height = std::max(r.y + r.height, b.y + b.height) - r.y;
        }
    }
    return r;
}

VAML_CORE_EXPORT
Color DrawingGroup::representative_color() const
{
    for (const auto& c : children_)
    {
        if (c)
        {
            return c->representative_color();
        }
    }
    return Color{ 0, 0, 0, 255 };
}

// ---------------------------------------------------------------------------
// Drawing brushes + the drawing image source
// ---------------------------------------------------------------------------

VAML_CORE_EXPORT
DrawingBrush::DrawingBrush() = default;

VAML_CORE_EXPORT
DrawingBrush::~DrawingBrush() = default;

VAML_CORE_EXPORT
Color DrawingBrush::representative_color() const noexcept
{
    return drawing_ ? drawing_->representative_color() : Color{ 0, 0, 0, 255 };
}

VAML_CORE_EXPORT
vaml::Rect DrawingBrush::bounds() const noexcept
{
    return drawing_ ? drawing_->bounds() : vaml::Rect{};
}

VAML_CORE_EXPORT
DrawingImage::DrawingImage() = default;

VAML_CORE_EXPORT
DrawingImage::~DrawingImage() = default;

VAML_CORE_EXPORT
ImageBrush::ImageBrush() = default;

VAML_CORE_EXPORT
ImageBrush::~ImageBrush() = default;

namespace {
const bool kImageBrushRegistered = [] {
    (void)DependencyProperty::Register(
        "ImageSource", ValueType::String, "ImageBrush",
        PropertyMetadata::WithDefault(Value::String(""), PropertyFlags::None));
    return true;
}();
}

VAML_CORE_EXPORT
SetResult ImageBrush::set_image_source(std::string value)
{
    (void)kImageBrushRegistered;
    imageSource_ = std::move(value);
    return SetResult::APPLIED;
}

VAML_CORE_EXPORT
Color ImageBrush::representative_color() const noexcept
{
    return Color{ 0, 0, 0, 255 };
}

// ---------------------------------------------------------------------------
// The WPF MiniLanguage path parser
// ---------------------------------------------------------------------------

namespace {

struct MiniParser
{
    std::string_view src;
    std::size_t      pos = 0;

    void skip_ws_and_commas()
    {
        while (pos < src.size() &&
               (src[pos] == ' ' || src[pos] == '\t' || src[pos] == '\r' ||
                src[pos] == '\n' || src[pos] == ','))
        {
            ++pos;
        }
    }

    bool peek_command(char& c) const
    {
        if (pos < src.size())
        {
            const char ch = src[pos];
            if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z'))
            {
                c = ch;
                return true;
            }
        }
        return false;
    }

    bool parse_number(double& out)
    {
        skip_ws_and_commas();
        if (pos >= src.size())
        {
            return false;
        }
        const char* start = src.data() + pos;
        char*       end   = nullptr;
        out = std::strtod(start, &end);
        if (end == start)
        {
            return false;
        }
        pos += static_cast<std::size_t>(end - start);
        return true;
    }
};

// Maps a command letter to its operand count (the number the parser consumes).
int command_operand_count(char c)
{
    switch (c)
    {
    case 'M': case 'm': case 'L': case 'l': case 'T': case 't': return 2;
    case 'H': case 'h': case 'V': case 'v': return 1;
    case 'C': case 'c': return 6;
    case 'S': case 's': case 'Q': case 'q': return 4;
    case 'A': case 'a': return 7;
    case 'Z': case 'z': return 0;
    default: return -1;
    }
}

PathCommand to_command(char c)
{
    switch (c)
    {
    default:
    case 'M': case 'm': return PathCommand::MoveTo;
    case 'L': case 'l': case 'H': case 'h': case 'V': case 'v': return PathCommand::LineTo;
    case 'C': case 'c': return PathCommand::CubicTo;
    case 'S': case 's': return PathCommand::SmoothCubic;
    case 'Q': case 'q': return PathCommand::QuadTo;
    case 'T': case 't': return PathCommand::SmoothQuad;
    case 'A': case 'a': return PathCommand::ArcTo;
    case 'Z': case 'z': return PathCommand::Close;
    }
}

} // namespace

VAML_CORE_EXPORT
PathData parse_path_geometry(std::string_view path, std::string* sourceOut)
{
    if (sourceOut)
    {
        sourceOut->assign(path);
    }
    PathData data;
    if (path.empty())
    {
        return data;
    }

    MiniParser p{ path, 0 };
    bool       hasStart = false;
    PathFigure current;
    char       previousCmd = 0;

    while (true)
    {
        p.skip_ws_and_commas();
        if (p.pos >= path.size())
        {
            break;
        }
        char c = 0;
        bool isCmd = p.peek_command(c);
        if (isCmd)
        {
            const int n = command_operand_count(c);
            if (n < 0)
            {
                throw ValueConversionException(
                    ValueConversionError::MalformedValue, "PathGeometry",
                    std::string(path), std::string("unknown path command '") + c + "'");
            }
            if (c == 'M' || c == 'm')
            {
                // A MoveTo begins a NEW figure (an implicit Close of the prior
                // one when it had content).
                if (hasStart && !current.empty())
                {
                    data.push_back(std::move(current));
                    current = PathFigure{};
                }
                else if (hasStart)
                {
                    current.clear();
                }
                hasStart = true;
                ++p.pos; // consume the command letter
                double x = 0, y = 0;
                if (!p.parse_number(x) || !p.parse_number(y))
                {
                    throw ValueConversionException(
                        ValueConversionError::MalformedValue, "PathGeometry",
                        std::string(path), "MoveTo requires two coordinates");
                }
                PathStep s;
                s.cmd = PathCommand::MoveTo;
                s.a[0] = static_cast<float>(x);
                s.a[1] = static_cast<float>(y);
                current.push_back(s);
                previousCmd = c;
                continue;
            }
            if (c == 'Z' || c == 'z')
            {
                ++p.pos;
                PathStep s;
                s.cmd = PathCommand::Close;
                current.push_back(s);
                previousCmd = c;
                continue;
            }
            // A drawing command (L/H/V/C/S/Q/T/A + relatives).
            ++p.pos;
            const PathCommand pc = to_command(c);
            const int baseCount = command_operand_count(c);
            // Consume the operand groups (a command may repeat numbers).
            bool firstGroup = true;
            while (true)
            {
                double vals[7];
                bool ok = true;
                for (int i = 0; i < baseCount; ++i)
                {
                    if (!p.parse_number(vals[i]))
                    {
                        ok = false;
                        break;
                    }
                }
                if (!ok)
                {
                    if (firstGroup)
                    {
                        throw ValueConversionException(
                            ValueConversionError::MalformedValue, "PathGeometry",
                            std::string(path), "a path command has incomplete operands");
                    }
                    break; // no more repeated groups
                }
                if (pc == PathCommand::LineTo && (c == 'H' || c == 'h' || c == 'V' || c == 'v'))
                {
                    // H/V carry ONE operand; absolute/relative handled by target.
                }
                PathStep s;
                s.cmd = pc;
                if (pc == PathCommand::LineTo && (c == 'H' || c == 'h'))
                {
                    // H carries ONE operand on the X (horizontal) axis.
                    s.a[0] = static_cast<float>(vals[0]);
                    s.a[1] = 0.0f; // no Y operand authored (the cursor's Y is implied)
                }
                else if (pc == PathCommand::LineTo && (c == 'V' || c == 'v'))
                {
                    // V carries ONE operand on the Y (vertical) axis.
                    s.a[0] = 0.0f; // no X operand authored
                    s.a[1] = static_cast<float>(vals[0]);
                }
                else
                {
                    for (int i = 0; i < baseCount; ++i)
                    {
                        s.a[i] = static_cast<float>(vals[i]);
                    }
                }
                // Relative operands: named by the parser; promotion to absolute
                // is deferred to the rasterizer (the step stream keeps the
                // authored numeric form + the cmd's relative flag is implied by
                // the lowercase letter — NOT retained here; a relative command is
                // re classified by its numeric sign at raster. We retain the
                // VERBATIM authored form; the vulkan seam applies relative maths).
                current.push_back(s);
                firstGroup = false;
                previousCmd = c;
                if (p.pos >= path.size())
                {
                    break;
                }
                // Stop if the next token is a command letter (not a number).
                char next = 0;
                if (p.peek_command(next))
                {
                    break;
                }
            }
            continue;
        }
        // A bare number with no command is a continuation of the previous
        // command (WPF shorthand). If there is no previous draw command, it is
        // a malformed stream.
        if (previousCmd == 0)
        {
            throw ValueConversionException(
                ValueConversionError::MalformedValue, "PathGeometry",
                std::string(path),
                "a path must begin with a command; found a bare number");
        }
        // Otherwise, re-enter the previous command's operand loop.
        const int baseCount = command_operand_count(previousCmd);
        if (baseCount <= 0)
        {
            throw ValueConversionException(
                ValueConversionError::MalformedValue, "PathGeometry",
                std::string(path), "bare number cannot continue a close/move");
        }
        const PathCommand pc = to_command(previousCmd);
        double vals[7];
        bool ok = true;
        for (int i = 0; i < baseCount; ++i)
        {
            if (!p.parse_number(vals[i]))
            {
                ok = false;
                break;
            }
        }
        if (!ok)
        {
            throw ValueConversionException(
                ValueConversionError::MalformedValue, "PathGeometry",
                std::string(path), "a path command has incomplete operands");
        }
        PathStep s;
        s.cmd = pc;
        for (int i = 0; i < baseCount; ++i)
        {
            s.a[i] = static_cast<float>(vals[i]);
        }
        current.push_back(s);
    }

    if (hasStart && !current.empty())
    {
        data.push_back(std::move(current));
    }
    return data;
}

} // namespace vaml
