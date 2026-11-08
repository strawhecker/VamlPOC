using System;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Shapes;

namespace Vaml.WpfEmitter
{
    // [MINIMAL REAL] WPF metadata tree-scanner — RESOLVED BY S642 (typed-slot/icon/text catalogue).
    // Do NOT extend silently; do NOT backfill.
    //
    // WHY it exists (recorded, not silent — NO-GAP): the emitter cannot prognosticate
    // perfectly which quads each diff kind needs. Text and Icon DIFF points are already
    // known (even before the metadata shape is finalized), so this placeholder walks the
    // emitter's OWN rendered element tree and HARVESTS the text-bearing and icon-bearing
    // nodes NOW, emitting their quads as declared scanner metadata. It is a deliberate,
    // forward-resolvable seed (the S642 typed-slot/icon/text catalogue is where it grows
    // into the real implementation). S641 READS whatever this deposits; S642/S643 fine-tune.
    public static class ElementTreeScanner
    {
        // Walk the visual tree of the rendered root; harvest quad + kind for every
        // text-bearing and icon-bearing leaf reached today. Provisional by design —
        // the SAME step's roadmap REVISIT tokens (S642/S643) refine it.
        public static void Harvest(FrameworkElement root, EmitterMetadata meta)
        {
            if (root == null) return;
            var size = root.RenderSize;
            meta.Width = (uint)Math.Max(0, Math.Ceiling(size.Width));
            meta.Height = (uint)Math.Max(0, Math.Ceiling(size.Height));

            HarvestInto(root, meta);
        }

        private static void HarvestInto(DependencyObject node, EmitterMetadata meta)
        {
            foreach (object child in LogicalTreeHelper.GetChildren(node))
            {
                if (child is FrameworkElement fe)
                {
                    HarvestElement(fe, meta);
                    HarvestInto(fe, meta);
                }
            }
        }

        private static void HarvestElement(FrameworkElement fe, EmitterMetadata meta)
        {
            // Text-bearing nodes: TextBlock (and, later, TextBox/ContentControl text —
            // the catalogue S642 widens this). Declared kind = Text.
            if (fe is TextBlock text)
            {
                if (!string.IsNullOrEmpty(text.Text))
                {
                    var p = PositionOf(fe);
                    meta.Quads.Add(new QuadMetadata
                    {
                        Name = fe.Name ?? "text",
                        X = p.X,
                        Y = p.Y,
                        Width = (int)Math.Ceiling(fe.RenderSize.Width),
                        Height = (int)Math.Ceiling(fe.RenderSize.Height),
                        Kind = QuadKind.Text,
                    });
                }
                return;
            }

            // Icon-bearing nodes: an <Image> that points at an icon asset (or a Shape
            // that is a concrete icon glyph). Declared kind = Icon. The S642 catalogue
            // refines icon type detection; today a named Image/Shape is the seed.
            if (fe is Image img)
            {
                var p = PositionOf(fe);
                meta.Quads.Add(new QuadMetadata
                {
                    Name = fe.Name ?? "icon",
                    X = p.X,
                    Y = p.Y,
                    Width = (int)Math.Ceiling(fe.RenderSize.Width),
                    Height = (int)Math.Ceiling(fe.RenderSize.Height),
                    Kind = QuadKind.Icon,
                });
                return;
            }

            if (fe is Shape shape)
            {
                var p = PositionOf(fe);
                meta.Quads.Add(new QuadMetadata
                {
                    Name = fe.Name ?? "shape",
                    X = p.X,
                    Y = p.Y,
                    Width = (int)Math.Ceiling(fe.RenderSize.Width),
                    Height = (int)Math.Ceiling(fe.RenderSize.Height),
                    Kind = QuadKind.Icon,
                });
                return;
            }

            // Solid geometry / panels / panels-of-leaves: NOT harvested as a declared
            // kind today (they are the "solid -> tight" default in the scanner). Only
            // text/icon get explicit metadata now, matching the known diff points.
        }

        private static (int X, int Y) PositionOf(FrameworkElement fe)
        {
            // Translate to the ROOT's coordinate space so the quad is in frame-space.
            var origin = fe.TranslatePoint(new Point(0, 0), null);
            return ((int)Math.Floor(origin.X), (int)Math.Floor(origin.Y));
        }
    }
}
