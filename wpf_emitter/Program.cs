using System;
using System.IO;
using System.Text;
using System.Text.Json;
using System.Windows;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Xml;

namespace Vaml.WpfEmitter
{
    // S640b/S642 — WPF REFERENCE EMITTER (Windows-authoring-only; PRECEDES S641, refined S642).
    // A small C#/WPF app that renders an authored .xaml OFFSCREEN via RenderTargetBitmap,
    // blits to the SAME ImageBuffer BGRA shape (the convergence contract), and DEPOSITS
    // the diffable reference material on disk:
    //     out.bgra = tight w*h*4 raw BGRA (row 0 = top)
    //     out.json = EmitterMetadata sidecar (dims + per-quad kinds + the S642
    //                catalogue-driven Capability/Tolerance the S641/S642 compare READS)
    //
    // Usage:  WpfEmitter.exe <file.xaml> <out.bgra> [out.json]
    // The emitter is real WPF: XamlReader loads the authored doc into a genuine WPF tree
    // (zero fidelity drift — it IS WPF). NO compare, NO SCAN DECLARATION parsing HERE — those
    // are S641. S642 (the catalogue): the emitter's tree-scanner (ElementTreeScanner) reads the
    // SAME SINGLE-SOURCE-OF-TRUTH capability catalogue (tools/capability_catalogue.json) that
    // the LIB side (embedded at compile time) and the PARITY MATRIX generator read, and tags
    // every harvested text/icon quad with its catalogue capability id + kind + tolerance — the
    // SYNC-TOKEN "keep both in sync" mechanism. This Main harness surfaces that the catalogue
    // was reachable at authoring time (a FALLBACK catalogue is used only if the repo JSON is
    // not locatable, so the emitter never crashes; the LIB side + the matrix always use the
    // committed bytes).
    public static class Program
    {
        [STAThread]
        public static int Main(string[] args)
        {
            if (args.Length < 2)
            {
                Console.Error.WriteLine("Usage: WpfEmitter.exe <file.xaml> <out.bgra> [out.json]");
                return 2;
            }

            string xamlPath = Path.GetFullPath(args[0]);
            string outBgra = Path.GetFullPath(args[1]);
            string outJson = args.Length >= 3 ? Path.GetFullPath(args[2]) : Path.ChangeExtension(outBgra, ".json");

            if (!File.Exists(xamlPath))
            {
                Console.Error.WriteLine("XAML not found: " + xamlPath);
                return 2;
            }

            try
            {
                // 1. Load the authored .xaml into a TRUE WPF object tree.
                object parsed;
                using (var reader = XmlReader.Create(xamlPath, new XmlReaderSettings { DtdProcessing = DtdProcessing.Prohibit }))
                {
                    parsed = System.Windows.Markup.XamlReader.Load(reader);
                }

                if (!(parsed is FrameworkElement root))
                {
                    Console.Error.WriteLine("XAML root is not a FrameworkElement.");
                    return 2;
                }

                // 2. Size to the authored Width/Height; Measure + Arrange + UpdateLayout.
                var metadata = new EmitterMetadata { Source = Path.GetFileName(xamlPath) };

                // Render size comes from the authored root's Width/Height (NOT pre-layout
                // RenderSize, which is 0 before Measure/Arrange). Fall back to 1x1.
                double rootW = root.Width > 0 ? root.Width : (root.ActualWidth > 0 ? root.ActualWidth : 1);
                double rootH = root.Height > 0 ? root.Height : (root.ActualHeight > 0 ? root.ActualHeight : 1);
                uint w = (uint)Math.Max(1, Math.Ceiling(rootW));
                uint h = (uint)Math.Max(1, Math.Ceiling(rootH));
                root.Measure(new Size(w, h));
                root.Arrange(new Rect(0, 0, w, h));
                root.UpdateLayout();

                // Tree-scanner placeholder harvests text/icon quads AFTER layout (so the
                // quads carry real frame-space positions). Sets meta.Width/Height from the
                // root's post-layout RenderSize.
                ElementTreeScanner.Harvest(root, metadata);
                w = metadata.Width >= 1 ? metadata.Width : w;
                h = metadata.Height >= 1 ? metadata.Height : h;

                // S642 (the SYNC TOKEN, authoring half): the catalogue-driven scanner must
                // have read the COMMITTED single-source-of-truth catalogue (tools/
                // capability_catalogue.json), not the in-memory fallback — otherwise the
                // WPF side could silently drift from the LIB side + the matrix (which always
                // use the committed JSON). The authored sample's sidecar main must carry a
                // catalogued capability id; a fallback-only run FAILS LOUDLY (never silent).
                if (!ElementTreeScanner.CatalogueLocated())
                {
                    Console.Error.WriteLine("WpfEmitter S642-FAIL: capability_catalogue.json was not located — " +
                                            "fallback in use, the WPF side is NOT in sync with the LIB side + matrix. " +
                                            "Run me from the repo root so tools/capability_catalogue.json resolves.");
                    return 2;
                }

                // 3. RenderTargetBitmap at pixel size -> raw BGRA, row 0 = top.
                var rtb = new RenderTargetBitmap((int)w, (int)h, 96, 96, PixelFormats.Pbgra32);
                rtb.Render(root);

                var stride = (int)w * 4;
                var buffer = new byte[stride * (int)h];
                rtb.CopyPixels(buffer, stride, 0);

                // 4. Blit into the ImageBuffer (the C# twin). Pbgra32 IS BGRA byte order
                //    (premultiplied BGRA), matching our BGRA row-0-top contract.
                var imageBuffer = new ImageBuffer
                {
                    Width = w,
                    Height = h,
                    Bgra = buffer,
                };

                // 5. Deposit: tight w*h*4 .bgra + .json sidecar.
                File.WriteAllBytes(outBgra, imageBuffer.Bgra);
                var jsonOptions = new JsonSerializerOptions
                {
                    WriteIndented = true,
                    Converters = { new System.Text.Json.Serialization.JsonStringEnumConverter() },
                };
                File.WriteAllText(outJson, JsonSerializer.Serialize(metadata, jsonOptions), new UTF8Encoding(false));

                Console.WriteLine($"WpfEmitter: wrote {outBgra} ({w}x{h}, {imageBuffer.Bgra.Length} bytes, {imageBuffer.Bgra.Length / 4} pxX4) + {outJson} ({metadata.Quads.Count} quads)");
                return 0;
            }
            catch (Exception ex)
            {
                Console.Error.WriteLine("WpfEmitter FAILED: " + ex);
                return 1;
            }
        }
    }
}
