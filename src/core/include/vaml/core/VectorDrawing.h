#pragma once
// VAML VectorDrawing — the 11k-4 vector + drawing-brush resource model
// (vaml::core CORE DATA, ADR-011 held: all of it is DATA, the render stays
// vulkan).
//
// This is the corpus's LARGEST omitted volume — the entire `Icons/*.xaml`
// + `Icons.xaml`/`NewIcons.xaml` family: `ImageBrush`, `DrawingBrush`,
// `DrawingGroup`, `DrawingImage` and the `Geometry` resources they draw,
// authored + resolved through the S360/S453 resource path and rasterized via
// the S202 atlas mesh (the vulkan vector seam — see vaml::vulkan::vector).
//
// WPF-FIDELITY (validated against E:\GitRepos\wpf, the real product source):
//   * `Geometry` is an ABSTRACT class (WPF: `public abstract partial class
//     Geometry : Animatable`) with an INTERNAL ctor — non-instantiable, NOT
//     authorable as a bare element. The 11k-3 recorded deferral (`<Geometry>`
//     OUT-OF-SCOPE(11k-4)) is RESOLVED here: the abstract base is not authored;
//     the concrete DO leaves beneath it are.
//   * `Drawing` is likewise an ABSTRACT base (WPF: `public abstract partial
//     class Drawing : Animatable`) with an INTERNAL ctor — non-instantiable.
//     The concrete leaves are authored.
//   * Concrete leaves are DependencyObjects (valid S360 Object resources,
//     captured by x:Key) mirroring WPF's `sealed class X : Geometry/Drawing`.
//
// The DO leaves carry real DPs + owned data exactly as the S453/S460 brush
// resources do (SolidColorBrush / LinearGradientBrush precedent).

#include "vaml/core/Brush.h"            // BrushId convention + brush object types
#include "vaml/core/Color.h"
#include "vaml/core/DependencyObject.h" // SetResult / DependencyProperty surface
#include "vaml/core/Geometry.h"         // vaml::Rect (the layout rect value type)
#include "vaml/core/Transform.h"        // Matrix2D

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace vaml {

// ---------------------------------------------------------------------------
// Path data — the parsed WPF MiniLanguage step stream.
// ---------------------------------------------------------------------------

// A parsed MiniLanguage path STEP (M/L/H/V/C/S/Q/T/A/Z). Tagged command + up to
// six float operands (the WPF PathFigureCollection/PathSegmentCollection model
// flattened to a flat command stream — the value-type-element precedent from
// 11k-3, where Geometry content is authored from its inline path data).
enum class PathCommand : std::uint8_t
{
    MoveTo = 0,   // M / m  (x, y)
    LineTo = 1,   // L / l / H / h / V / v  (x, y)
    CubicTo = 2,  // C / c  (x1, y1, x2, y2, x, y)
    SmoothCubic = 3, // S / s  (x2, y2, x, y)
    QuadTo = 4,   // Q / q  (x1, y1, x, y)
    SmoothQuad = 5, // T / t  (x, y)
    ArcTo = 6,    // A / a  (rx, ry, rot, largeArc, sweep, x, y)
    Close = 7,    // Z / z  (no operands)
};

struct VAML_CORE_EXPORT PathStep
{
    PathCommand cmd = PathCommand::MoveTo;
    // ArcTo needs SEVEN operands (rx, ry, rotation, largeArc, sweep, x, y); all
    // other commands use <=6. Sized 7 for that full set.
    float a[7] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };

    [[nodiscard]] bool is_draw() const noexcept { return cmd != PathCommand::MoveTo; }
    [[nodiscard]] bool is_close() const noexcept { return cmd == PathCommand::Close; }
};

using PathFigure = std::vector<PathStep>;   // ONE figure (starts MoveTo, may Close)
using PathData   = std::vector<PathFigure>; // a geometry's full figure set

// The WPF MiniLanguage path parser. Produces a PathGeometry's figure stream
// from authored inline path data (M/L/H/V/C/S/Q/T/A/Z, relative + absolute,
// repeated numbers infer the previous command — the WPF PathGeometry TypeParse
// contract). Throws ValueConversionException{MalformedValue} on a degenerate /
// malformed stream (fail loud, never silent). Pure + headless-testable.
VAML_CORE_EXPORT PathData
parse_path_geometry(std::string_view path, std::string* sourceOut = nullptr);

// ---------------------------------------------------------------------------
// Geometry — the ABSTRACT authoring base (WPF-faithful). NOT authorable.
// ---------------------------------------------------------------------------

class VAML_CORE_EXPORT Geometry : public DependencyObject
{
public:
    ~Geometry() override;

    // `Transform` — the 11k-8 audited Geometry.Transform DP (Matrix2D; default
    // identity). Lives on the abstract base, exactly like WPF.
    [[nodiscard]] SetResult set_transform(const vaml::Matrix2D& m);
    [[nodiscard]] vaml::Matrix2D transform() const;

    // Axis-aligned bounds of the geometry in its local space (derived by the
    // concrete leaf). Pure.
    [[nodiscard]] virtual vaml::Rect bounds() const = 0;

protected:
    // Non-instantiable contract (mirrors WPF's internal ctor). Only the concrete
    // DO leaves may be constructed.
    Geometry() = default;

private:
    friend class VamlReader;
    Geometry(const Geometry&) = delete;
    Geometry& operator=(const Geometry&) = delete;
};

// ---------------------------------------------------------------------------
// Concrete Geometry leaves (AUTHORABLE DO resources, captured by x:Key).
// ---------------------------------------------------------------------------

// PathGeometry — `Figures` (WPF MiniLanguage inline path data, parsed + fail-
// loud on an empty/degenerate stream) + `FillRule`. The corpus's dominant
// `Icons/*.xaml` vector channel.
class VAML_CORE_EXPORT PathGeometry : public Geometry
{
public:
    PathGeometry();
    ~PathGeometry() override;

    // `Figures` — the authored WPF MiniLanguage path data (e.g. "M0,0 L16,0
    // L16,16 Z"). PARSED into the step stream; an EMPTY/degenerate stream is an
    // AUTHORING ERROR (fail loud, never silent — NO lazy backfill).
    [[nodiscard]] SetResult set_figures(std::string figures);
    [[nodiscard]] const std::string& figures_source() const noexcept { return figuresSource_; }
    [[nodiscard]] const PathData& figures() const noexcept { return figures_; }
    [[nodiscard]] bool parsed() const noexcept { return parsed_; }

    // `FillRule` (Int32; EvenOdd=0 | Nonzero=1; default EvenOdd) — the WPF
    // Geometry.FillRule convention (a path is filled by this rule when drawn).
    [[nodiscard]] SetResult set_fill_rule(std::int32_t rule);
    [[nodiscard]] std::int32_t fill_rule() const;

    [[nodiscard]] vaml::Rect bounds() const override;

private:
    std::string figuresSource_;
    PathData    figures_;
    bool        parsed_ = false;
};

// GeometryGroup — an owned, ordered collection of child geometries (WPF
// GeometryGroup.Children). DO leaf, captured by x:Key.
class VAML_CORE_EXPORT GeometryGroup : public Geometry
{
public:
    GeometryGroup();
    ~GeometryGroup() override;

    void add_geometry(std::shared_ptr<Geometry> child);
    [[nodiscard]] const std::vector<std::shared_ptr<Geometry>>& children() const noexcept { return children_; }
    [[nodiscard]] std::size_t child_count() const noexcept { return children_.size(); }

    [[nodiscard]] vaml::Rect bounds() const override;

private:
    std::vector<std::shared_ptr<Geometry>> children_;
};

// RectangleGeometry — an axis-aligned rectangle `Rect="x,y,w,h"`. DO leaf.
class VAML_CORE_EXPORT RectangleGeometry : public Geometry
{
public:
    RectangleGeometry();
    ~RectangleGeometry() override;

    // `Rect` — authored "x,y,w,h" (WPF RectConverter; comma or space separated).
    // Parsed eagerly; malformed/empty -> fail-loud authoring error.
    [[nodiscard]] SetResult set_rect(std::string rect);
    [[nodiscard]] vaml::Rect rect() const noexcept { return rect_; }
    [[nodiscard]] bool parsed() const noexcept { return parsed_; }

    [[nodiscard]] vaml::Rect bounds() const override;

private:
    vaml::Rect rect_{};
    bool       parsed_ = false;
};

// EllipseGeometry — an axis-aligned ellipse (WPF EllipseGeometry.RadiusX/
// RadiusY). DO leaf.
class VAML_CORE_EXPORT EllipseGeometry : public Geometry
{
public:
    EllipseGeometry();
    ~EllipseGeometry() override;

    [[nodiscard]] SetResult set_radius_x(double rx);
    [[nodiscard]] double radius_x() const noexcept { return radiusX_; }
    [[nodiscard]] SetResult set_radius_y(double ry);
    [[nodiscard]] double radius_y() const noexcept { return radiusY_; }

    [[nodiscard]] vaml::Rect bounds() const override;

private:
    double radiusX_ = 0.0;
    double radiusY_ = 0.0;
};

// ---------------------------------------------------------------------------
// Drawing — the ABSTRACT drawing base (WPF-faithful). NOT authorable.
// ---------------------------------------------------------------------------

class VAML_CORE_EXPORT Drawing : public DependencyObject
{
public:
    ~Drawing() override;

    // Axis-aligned bounds of the drawing in its local space.
    [[nodiscard]] virtual vaml::Rect bounds() const = 0;

    // The representative opaque color for the scalar-foreground fallback (the
    // S460 representative_color precedent): the first GeometryDrawing's fill
    // color when resolvable, opaque black otherwise. Headless only.
    [[nodiscard]] virtual Color representative_color() const { return Color{ 0, 0, 0, 255 }; }

protected:
    Drawing() = default;

private:
    friend class VamlReader;
    Drawing(const Drawing&) = delete;
    Drawing& operator=(const Drawing&) = delete;
};

// GeometryDrawing — one GEOMETRY+FILL drawing (WPF GeometryDrawing.Brush/.Pen/
// .Geometry). The actual ink channel a DrawingBrush rasterizes.
class VAML_CORE_EXPORT GeometryDrawing : public Drawing
{
public:
    GeometryDrawing();
    ~GeometryDrawing() override;

    // `Brush`/`Pen` — scalar BrushIds authorable as attributes (the S364 BrushId
    // path). The fill/outline color resolves via the three-scope walk.
    [[nodiscard]] SetResult set_brush(std::int32_t brushId);
    [[nodiscard]] std::int32_t brush() const noexcept { return brushId_; }
    [[nodiscard]] SetResult set_pen(std::int32_t penId);
    [[nodiscard]] std::int32_t pen() const noexcept { return penId_; }
    [[nodiscard]] SetResult set_pen_thickness(double t);
    [[nodiscard]] double pen_thickness() const noexcept { return penThickness_; }

    // `Geometry` — the owned authored Geometry (PathGeometry/GeometryGroup/...);
    // set by the reader via the `<GeometryDrawing.Geometry>` property-element.
    void set_geometry(std::shared_ptr<Geometry> g) { geometry_ = std::move(g); }
    [[nodiscard]] const std::shared_ptr<Geometry>& geometry() const noexcept { return geometry_; }

    [[nodiscard]] vaml::Rect bounds() const override;
    [[nodiscard]] Color representative_color() const override;

private:
    std::int32_t brushId_ = 0;
    std::int32_t penId_   = 0;
    double       penThickness_ = 1.0;
    std::shared_ptr<Geometry> geometry_;
};

// DrawingGroup — an owned, ordered collection of child drawings + an optional
// transform (WPF DrawingGroup.Children/.ClipGeometry/.Transform). DO leaf.
class VAML_CORE_EXPORT DrawingGroup : public Drawing
{
public:
    DrawingGroup();
    ~DrawingGroup() override;

    void add_drawing(std::shared_ptr<Drawing> child);
    [[nodiscard]] const std::vector<std::shared_ptr<Drawing>>& children() const noexcept { return children_; }
    [[nodiscard]] std::size_t child_count() const noexcept { return children_.size(); }

    [[nodiscard]] SetResult set_transform(const vaml::Matrix2D& m);
    [[nodiscard]] vaml::Matrix2D transform() const noexcept { return transform_; }

    [[nodiscard]] vaml::Rect bounds() const override;
    [[nodiscard]] Color representative_color() const override;

private:
    std::vector<std::shared_ptr<Drawing>> children_;
    vaml::Matrix2D transform_{};
};

// ---------------------------------------------------------------------------
// Drawing brushes + the drawing image source (AUTHORABLE DO resources).
// ---------------------------------------------------------------------------

// DrawingBrush — a Brush whose content is an authored Drawing (WPF
// DrawingBrush.Drawing). DO leaf, captured by x:Key (a "Brush:<id>" resource).
class VAML_CORE_EXPORT DrawingBrush : public DependencyObject
{
public:
    DrawingBrush();
    ~DrawingBrush() override;

    // `Drawing` — the owned authored Drawing (DrawingGroup/GeometryDrawing);
    // set by the reader via the `<DrawingBrush.Drawing>` property-element.
    void set_drawing(std::shared_ptr<Drawing> d) { drawing_ = std::move(d); }
    [[nodiscard]] const std::shared_ptr<Drawing>& drawing() const noexcept { return drawing_; }

    [[nodiscard]] Color representative_color() const noexcept;
    [[nodiscard]] vaml::Rect bounds() const noexcept;

private:
    std::shared_ptr<Drawing> drawing_;
};

// DrawingImage — an ImageSource carrying an authored Drawing (WPF
// DrawingImage.Drawing). DO leaf, captured by x:Key.
class VAML_CORE_EXPORT DrawingImage : public DependencyObject
{
public:
    DrawingImage();
    ~DrawingImage() override;

    void set_drawing(std::shared_ptr<Drawing> d) { drawing_ = std::move(d); }
    [[nodiscard]] const std::shared_ptr<Drawing>& drawing() const noexcept { return drawing_; }

private:
    std::shared_ptr<Drawing> drawing_;
};

// ImageBrush — a Brush whose content is an ImageSource/bitmap asset (WPF
// ImageBrush.ImageSource — the TagConfigurationUserControl channel). DO leaf,
// captured by x:Key.
class VAML_CORE_EXPORT ImageBrush : public DependencyObject
{
public:
    ImageBrush();
    ~ImageBrush() override;

    // `ImageSource` — a String (an icon/resource key or a packaged asset
    // reference) authorable as an attribute (the WPF ImageBrush.ImageSource).
    [[nodiscard]] SetResult set_image_source(std::string value);
    [[nodiscard]] const std::string& image_source() const noexcept { return imageSource_; }

    [[nodiscard]] Color representative_color() const noexcept;

private:
    std::string imageSource_;
};

} // namespace vaml
