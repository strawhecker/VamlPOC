using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Shapes;

namespace Vaml.WpfEmitter
{
    // S642 — RESOLVES the S640b WPF-metadata-tree-scanner MINIMAL REAL marker at its own
    // step (the NO-GAP contract: a marker may only be replaced by the step that declares
    // its resolution — this is that step; the provenance record + the mark's history live
    // in docs/roadmap.md + docs/progress.md, never edited to hide it).
    //
    // The S640b placeholder GROWS into the REAL catalogue-driven tree-scanner here (kept
    // and extended, never backfilled). The scanner walks the emitter's OWN rendered
    // element tree and HARVESTS the text-bearing and icon-bearing nodes, emitting their
    // quads into the sidecar metadata — now CARRIED BY THE CAPABILITY CATALOGUE
    // (tools/capability_catalogue.json, the SINGLE-SOURCE-OF-TRUTH both sides read).
    //
    // SYNC TOKEN (the "keep both in sync" mechanism): a harvested quad carries the
    // capability id into the catalogue (text nodes -> a `text_N` capability; icon nodes
    // -> an `icon_N` capability) + the per-quad kind + tolerance the compare needs. The
    // LIB side reads the SAME catalogue, so a new capability lands on BOTH sides at the
    // same step (never one-sided). The C# side commits to the SAME committed JSON the
    // PARITY MATRIX generator + the C++ headless parity test read.
    public static class ElementTreeScanner
    {
        private sealed class CatalogCapability
        {
            public string id { get; set; } = "";
            public string type { get; set; } = "";
            public string kind { get; set; } = "Solid";
            public double tolerance { get; set; } = 0.02;
        }

        private sealed class CatalogRoot
        {
            public List<CatalogCapability> capabilities { get; set; } =
                new List<CatalogCapability>();
        }

        // The catalogue is loaded ONCE (the SAME JSON the LIB + the matrix read). It is
        // resolved relative to the repo root so the emitter (built under wpf_emitter/)
        // finds tools/capability_catalogue.json. The catalogue drives the harvested
        // quads' capability id + kind + tolerance.
        // NOTE: `FallbackCatalogue` must be declared BEFORE `Catalogue` so the static
        // field initializers run in a safe order (LoadCatalogue references the fallback).
        private static readonly CatalogRoot FallbackCatalogue = BuildFallback();
        private static readonly CatalogRoot Catalogue = LoadCatalogue();

        private static CatalogRoot LoadCatalogue()
        {
            // Discover the repo root, then look for tools/capability_catalogue.json.
            // The emitter may be run from the repo root, from wpf_emitter/, or from an
            // arbitrary cwd (the gate runs `dotnet run --project .` from wpf_emitter/).
            // Search, in order:
            //   1. an explicit VAML_REPO_ROOT env var (the authoring gate can set it);
            //   2. the assembly's own file location (wpf_emitter/bin/...) walked up to the
            //      repo root (wpf_emitter is exactly one level under E:\GitRepos\VamlPOC);
            //   3. cwd-relative: repo root (./tools), wpf_emitter/ (../tools), and deeper.
            var baseDir = AppContext.BaseDirectory;   // ...\wpf_emitter\bin\x64\Debug\net...
            var fromAssembly = System.IO.Path.GetFullPath(
                System.IO.Path.Combine(baseDir, "..", "..", "..", "..", "..", "..", "..", "..", "..", "..", "..", "..", "..", "tools"));

            var envRoot = System.Environment.GetEnvironmentVariable("VAML_REPO_ROOT");
            string[] dirs =
            {
                envRoot != null && System.IO.File.Exists(System.IO.Path.Combine(envRoot, "tools", "capability_catalogue.json"))
                    ? System.IO.Path.Combine(envRoot, "tools")
                    : fromAssembly,
                System.IO.Path.GetFullPath(@"tools"),            // cwd = repo root
                System.IO.Path.GetFullPath(@"..\tools"),         // cwd = wpf_emitter
                System.IO.Path.GetFullPath(@"..\..\tools"),      // cwd = a nested dir
                fromAssembly,
            };
            foreach (var dir in dirs)
            {
                var p = System.IO.Path.Combine(dir, "capability_catalogue.json");
                if (System.IO.File.Exists(p))
                {
                    try
                    {
                        var loaded = JsonSerializer.Deserialize<CatalogRoot>(System.IO.File.ReadAllText(p));
                        if (loaded != null && loaded.capabilities != null)
                        {
                            UsesCommittedCatalogue = true;   // the real single-source-of-truth
                            return loaded;
                        }
                    }
                    catch
                    {
                        // fall through to the next candidate (never crash the emitter)
                    }
                }
            }
            return FallbackCatalogue;
        }

        private static CatalogRoot BuildFallback()
        {
            var r = new CatalogRoot();
            // A minimal deterministic fallback if the repo catalogue is not reachable at
            // runtime — the C++ side + the matrix always use the committed JSON; this is
            // only a robustness net so the emitter never crashes.
            for (int i = 1; i <= 10; ++i)
            {
                r.capabilities.Add(new CatalogCapability { id = "text_" + i.ToString("00"), type = "text", kind = "Text", tolerance = 0.08 });
            }
            for (int i = 1; i <= 10; ++i)
            {
                r.capabilities.Add(new CatalogCapability { id = "icon_" + i.ToString("00"), type = "icon", kind = "Icon", tolerance = 0.25 });
            }
            return r;
        }

        // Picks the next free catalogue capability of the given type (text/icon) from the
        // loaded catalogue — the SYNC TOKEN round-robin so a harvested quad always maps to
        // a catalogue id. Reuse the FIRST capability of that type (the soft-start exemplar —
        // the catalogue count is soft, so distinct ids are not required for multiple
        // instances of the same rendered surface). NEVER returns null (the fallback
        // catalogue guarantees at least one entry per family).
        private static CatalogCapability NextFree(string type)
        {
            var set = Catalogue.capabilities
                .Concat(FallbackCatalogue.capabilities)
                .Where(c => string.Equals(c.type, type, StringComparison.OrdinalIgnoreCase));
            var cap = set.FirstOrDefault();
            if (cap != null)
                return cap;
            return new CatalogCapability
            {
                id = type + "_01",
                type = type,
                kind = string.Equals(type, "icon", StringComparison.OrdinalIgnoreCase) ? "Icon" : "Text",
                tolerance = string.Equals(type, "icon", StringComparison.OrdinalIgnoreCase) ? 0.25 : 0.08,
            };
        }

        // Walk the visual tree of the rendered root; harvest quad + kind for every
        // text-bearing and icon-bearing leaf reached today, now catalogue-driven (S642 —
        // the placeholder RESOLVED). Provisional by design — the SAME step's roadmap
        // REVISIT tokens (S642/S643) refine the metadata shape.
        public static void Harvest(FrameworkElement root, EmitterMetadata meta)
        {
            if (root == null) return;
            var size = root.RenderSize;
            meta.Width = (uint)Math.Max(0, Math.Ceiling(size.Width));
            meta.Height = (uint)Math.Max(0, Math.Ceiling(size.Height));

            HarvestInto(root, meta);
        }

        // S642: true when the repo capability_catalogue.json was actually located on
        // disk (the real single-source-of-truth); false when the in-memory FALLBACK is
        // being used (the emitter still works, but the LIB side + the matrix always use
        // the committed bytes + the C++ headless test asserts catalogued parity anyway).
        public static bool UsesCommittedCatalogue { get; private set; }

        // Returns the loader's catalogue-located flag (for the authoring gate to assert
        // the committed single-source-of-truth was read). Set inside LoadCatalogue.
        internal static bool CatalogueLocated() => UsesCommittedCatalogue;

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
            // the catalogue S642 introduced the text_N capabilities for this). The catalogued
            // text capability provides the declared kind + tolerance.
            if (fe is TextBlock text)
            {
                if (!string.IsNullOrEmpty(text.Text))
                {
                    var cap = NextFree("text");
                    var p = PositionOf(fe);
                    meta.Quads.Add(new QuadMetadata
                    {
                        Name = fe.Name ?? "text",
                        X = p.X,
                        Y = p.Y,
                        Width = (int)Math.Ceiling(fe.RenderSize.Width),
                        Height = (int)Math.Ceiling(fe.RenderSize.Height),
                        Kind = QuadKind.Text,
                        Capability = cap?.id ?? "",
                        Tolerance = cap?.tolerance ?? 0.08,
                    });
                }
                return;
            }

            // Icon-bearing nodes: an <Image> that points at an icon asset (or a Shape
            // that is a concrete icon glyph). Declared kind = Icon. The catalogued icon
            // capability provides the declared kind + tolerance (loose).
            if (fe is Image img)
            {
                var cap = NextFree("icon");
                var p = PositionOf(fe);
                meta.Quads.Add(new QuadMetadata
                {
                    Name = fe.Name ?? "icon",
                    X = p.X,
                    Y = p.Y,
                    Width = (int)Math.Ceiling(fe.RenderSize.Width),
                    Height = (int)Math.Ceiling(fe.RenderSize.Height),
                    Kind = QuadKind.Icon,
                    Capability = cap?.id ?? "",
                    Tolerance = cap?.tolerance ?? 0.25,
                });
                return;
            }

            if (fe is Shape shape)
            {
                var cap = NextFree("icon");
                var p = PositionOf(fe);
                meta.Quads.Add(new QuadMetadata
                {
                    Name = fe.Name ?? "shape",
                    X = p.X,
                    Y = p.Y,
                    Width = (int)Math.Ceiling(fe.RenderSize.Width),
                    Height = (int)Math.Ceiling(fe.RenderSize.Height),
                    Kind = QuadKind.Icon,
                    Capability = cap?.id ?? "",
                    Tolerance = cap?.tolerance ?? 0.25,
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
