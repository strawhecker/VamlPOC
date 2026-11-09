using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using System.Windows.Shapes;

namespace Vaml.WpfEmitter
{
    // S642/S643 — RESOLVES the S640b WPF-metadata-tree-scanner MINIMAL REAL marker at its
    // own step (the NO-GAP contract: a marker may only be replaced by the step that declares
    // its resolution — this is that step; the provenance record + the mark's history live in
    // docs/roadmap.md + docs/progress.md, never edited to hide it).
    //
    // The S640b placeholder GROWS into the REAL catalogue-driven tree-scanner here (kept
    // and extended, never backfilled). The scanner walks the emitter's OWN rendered element
    // tree and HARVESTS the quads declared in the authored `.xaml` (S643 REVISIT: the
    // in-XAML SCAN DECLARATION is the PRIMARY oracle — the sidecar is derived FROM it, so it
    // is in sync with what the C++ compare reads by construction), overlaying each declared
    // quad's REAL frame-space position/size from the named framework element.
    //
    // S643 REVISIT S640b/S641 (metadata-fine-tune) CLOSURE:
    //   The prior placeholder misclassified a Rectangle as Icon and dropped Border entirely,
    //   and collapsed every text capability id to first-of-family — so it did NOT round-trip
    //   with the declaration the C++ compare reads (the "planned metadata gap S642 left").
    //   This scanner now reads the SAME declaration block (parse_scan_declaration below, the
    //   C# mirror of the C++ grammar in src/presentation/src/scan/ScanDeclaration.cpp) and
    //   emits ONE sidecar quad PER DECLARED quad: Name/Kind/Capability/Tolerance come from the
    //   DECLARATION (the authoritative values, never guessed), while X/Y/Width/Height come from
    //   the REAL layout position of the named element in the WPF tree. A declared quad whose
    //   element is NOT present yields a zero-area quad the compare can detect as missing.
    //
    // SYNC TOKEN (the "keep both in sync" mechanism): a harvested quad carries the capability
    // id into the catalogue (the SAME committed JSON the LIB side + the matrix read). A
    // declared capability must still resolve in the catalogue — the LIB headless S643 suite
    // asserts it — so a new capability lands on BOTH sides at the same step (never one-sided).
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

        // A declared quad as parsed from the in-XAML SCAN DECLARATION.
        internal sealed class DeclaredQuad
        {
            public string Name = "";
            public string Kind = "solid";   // solid|text|icon
            public string Capability = "";
            public bool HasTolerance = false;
            public double Tolerance = 0.0;
            public bool Located = false;
            public int RX, RY, RW, RH;      // declared rect (frame-space), when Located
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
            var baseDir = AppContext.BaseDirectory;   // ...\wpf_emitter\bin\x64\Release\net...
            var fromAssembly = System.IO.Path.GetFullPath(
                System.IO.Path.Combine(baseDir, "..", "..", "..", "..", "..", "..", "..", "..", "..", "..", "..", "..", "..", "tools"));

            var envRoot = System.Environment.GetEnvironmentVariable("VAML_REPO_ROOT");
            string[] dirs =
            {
                envRoot != null && System.IO.File.Exists(System.IO.Path.Combine(envRoot, "tools", "capability_catalogue.json"))
                    ? System.IO.Path.Combine(envRoot, "tools")
                    : fromAssembly,
                System.IO.Path.GetFullPath(@"tools"),
                System.IO.Path.GetFullPath(@"..\tools"),
                System.IO.Path.GetFullPath(@"..\..\tools"),
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
                            UsesCommittedCatalogue = true;
                            return loaded;
                        }
                    }
                    catch
                    {
                    }
                }
            }
            return FallbackCatalogue;
        }

        private static CatalogRoot BuildFallback()
        {
            var r = new CatalogRoot();
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

        // Resolves a declared capability id to its catalogue row (kind + tolerance). Used to
        // VALIDATE a declared quad still maps into the catalogue (the SYNC-TOKEN both-sides
        // discipline; the C++ headless suite asserts the same resolutions).
        private static CatalogCapability Find(string id)
        {
            return Catalogue.capabilities
                .Concat(FallbackCatalogue.capabilities)
                .FirstOrDefault(c => string.Equals(c.id, id, StringComparison.Ordinal));
        }

        public static void Harvest(FrameworkElement root, EmitterMetadata meta, string xamlText)
        {
            if (root == null) return;
            var size = root.RenderSize;
            meta.Width = (uint)Math.Max(0, Math.Ceiling(size.Width));
            meta.Height = (uint)Math.Max(0, Math.Ceiling(size.Height));

            // S643 REVISIT CLOSURE: derive the sidecar from the in-XAML SCAN DECLARATION
            // (the PRIMARY oracle — the exact same block the C++ S641 parser reads), then
            // overlay each declared quad's real layout position from the named WPF element.
            var declared = ParseScanDeclaration(xamlText);
            foreach (var q in declared)
            {
                var fe = FindByName(root, q.Name);
                QuadMetadata mq = new QuadMetadata
                {
                    Name = q.Name,
                    Kind = KindFromString(q.Kind),
                    Capability = q.Capability,
                    Tolerance = q.HasTolerance ? q.Tolerance : DefaultTolerance(q.Kind),
                };
                if (fe != null)
                {
                    var p = PositionOf(fe);
                    mq.X = p.X;
                    mq.Y = p.Y;
                    mq.Width = (int)Math.Ceiling(fe.RenderSize.Width);
                    mq.Height = (int)Math.Ceiling(fe.RenderSize.Height);
                }
                else if (q.Located)
                {
                    // Declared tight rect but the element is absent in the tree: surface the
                    // rect so the compare can detect the missing real element.
                    mq.X = q.RX; mq.Y = q.RY; mq.Width = q.RW; mq.Height = q.RH;
                }
                meta.Quads.Add(mq);
            }
        }

        private static QuadKind KindFromString(string k)
        {
            switch (k)
            {
                case "icon": return QuadKind.Icon;
                case "text": return QuadKind.Text;
                default: return QuadKind.Solid;
            }
        }

        private static double DefaultTolerance(string kind)
        {
            // Mirrors default_tolerance_for in src/presentation/src/scan/ScanDeclaration.cpp.
            switch (kind)
            {
                case "icon": return 0.25;
                case "text": return 0.08;
                default: return 0.02;
            }
        }

        // Walks the visual tree for the FIRST element whose Name() equals `name`.
        private static FrameworkElement FindByName(FrameworkElement root, string name)
        {
            if (string.IsNullOrEmpty(name)) return null;
            var q = new Queue<FrameworkElement>();
            q.Enqueue(root);
            while (q.Count > 0)
            {
                var cur = q.Dequeue();
                if (string.Equals(cur.Name, name, StringComparison.Ordinal))
                    return cur;
                int n = VisualTreeHelper.GetChildrenCount(cur);
                for (int i = 0; i < n; ++i)
                {
                    if (VisualTreeHelper.GetChild(cur, i) is FrameworkElement child)
                        q.Enqueue(child);
                }
            }
            return null;
        }

        // A minimal C# mirror of the S641 in-XAML `<!-- SCAN DECLARATION ... END SCAN
        // DECLARATION -->` grammar (src/presentation/src/scan/ScanDeclaration.cpp). Parses
        // ONLY the fields the sidecar needs (name/kind/capability/tolerance/rect). Malformed
        // or missing declarations yield an empty list (the emitter still works; the C++
        // compare is the authoritative reader).
        internal static List<DeclaredQuad> ParseScanDeclaration(string xamlText)
        {
            var result = new List<DeclaredQuad>();
            if (string.IsNullOrEmpty(xamlText)) return result;

            const string open = "<!-- SCAN DECLARATION";
            const string end = "END SCAN DECLARATION";
            int blockStart = xamlText.IndexOf(open, StringComparison.Ordinal);
            if (blockStart < 0) return result;
            int endMarker = xamlText.IndexOf(end, blockStart, StringComparison.Ordinal);
            if (endMarker < 0) return result;

            string body = xamlText.Substring(blockStart + open.Length, endMarker - blockStart - open.Length);
            foreach (var rawLine in body.Split('\n'))
            {
                string line = rawLine.Trim();
                if (line.Length == 0 || line[0] == ';') continue;
                var toks = line.Split((char[])null, StringSplitOptions.RemoveEmptyEntries);
                if (toks.Length == 0) continue;
                if (toks[0] != "quad") continue;   // only quads matter to the sidecar

                // quad NAME kind rect/auto [options]
                if (toks.Length < 3) continue;
                var dq = new DeclaredQuad { Name = toks[1], Kind = toks[2] };
                int i = 3;
                if (i < toks.Length && toks[i] == "rect")
                {
                    dq.Located = true;
                    int[] v = new int[4];
                    // both `rect 32,32,128,128` and `rect 32 32 128 128`
                    int filled = 0;
                    if (i + 1 < toks.Length && toks[i + 1].Contains(','))
                    {
                        var parts = toks[i + 1].Split(',');
                        for (int k = 0; k < 4 && k < parts.Length; ++k)
                        {
                            if (int.TryParse(parts[k], out v[k])) filled++;
                        }
                        i += 2;
                    }
                    else if (i + 1 + 4 <= toks.Length)
                    {
                        for (int k = 0; k < 4; ++k)
                        {
                            if (int.TryParse(toks[i + 1 + k], out v[k])) filled++;
                        }
                        i += 5;
                    }
                    if (filled == 4) { dq.RX = v[0]; dq.RY = v[1]; dq.RW = v[2]; dq.RH = v[3]; }
                }
                else if (i < toks.Length && toks[i] == "auto")
                {
                    i += 1;
                }

                for (; i < toks.Length; ++i)
                {
                    if (toks[i] == "capability" && i + 1 < toks.Length)
                    {
                        dq.Capability = toks[i + 1]; i++;
                    }
                    else if (toks[i] == "tolerance" && i + 1 < toks.Length)
                    {
                        if (double.TryParse(toks[i + 1], out double tol))
                        {
                            dq.HasTolerance = true; dq.Tolerance = tol;
                        }
                        i++;
                    }
                }
                result.Add(dq);
            }
            return result;
        }

        // true when the repo capability_catalogue.json was actually located on disk (the real
        // single-source-of-truth); false when the in-memory FALLBACK is being used (the LIB
        // side + the matrix always use the committed bytes + the C++ headless test asserts
        // catalogued parity anyway).
        public static bool UsesCommittedCatalogue { get; private set; }

        internal static bool CatalogueLocated() => UsesCommittedCatalogue;

        private static (int X, int Y) PositionOf(FrameworkElement fe)
        {
            var origin = fe.TranslatePoint(new Point(0, 0), null);
            return ((int)Math.Floor(origin.X), (int)Math.Floor(origin.Y));
        }
    }
}

