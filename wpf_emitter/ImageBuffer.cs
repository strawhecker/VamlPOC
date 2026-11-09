using System;
using System.Collections.Generic;

namespace Vaml.WpfEmitter
{
    // The C# twin of vaml::presentation::Impl::ImageBuffer (the S640b CONVERGENCE
    // CONTRACT). Mirrors the C++ header exactly:
    //     struct ImageBuffer { uint32_t width; uint32_t height; vector<uint8_t> bgra; }
    //   - bgra holds EXACTLY width*height*4 bytes after a successful render.
    //   - bgra is BGRA, row 0 = the image's TOP row (NOT bottom-up).
    // Cross-reference: src/presentation/include/vaml/presentation/RenderTarget.h.
    // Both sides MUST deposit byte-identical BGRA so S641's compliance_compare sees
    // identical raw frames. Any drift here is a NAMED parity divergence (PARITY MATRIX),
    // never silent.
    public sealed class ImageBuffer
    {
        public uint Width { get; set; }
        public uint Height { get; set; }
        public byte[] Bgra { get; set; } = Array.Empty<byte>();

        // The bytes a fully-filled ImageBuffer needs: width*height*4 (BGRA).
        // Mirrors image_buffer_bytes(w, h).
        public static long BytesFor(uint width, uint height) => (long)width * height * 4;
    }

    // Per-quad SCANNER METADATA that the emitter deposits alongside the raw frame.
    // The emitter cannot perfectly prognosticate what each diff kind needs (text ->
    // auto-locate + tolerance band; icon -> presence/centroid + loose; solid geometry
    // -> tight), so the emitter writes DECLARED per-quad kind/shape metadata that the
    // S641 compare READS instead of guessing. S642 makes this CATALOGUE-DRIVEN: every
    // quad carries its capability id (the SYNC-TOKEN id into tools/capability_catalogue.json,
    // the SAME file the LIB side + the PARITY MATRIX read) + the per-quad tolerance +
    // kind the S642 compare needs. The metadata shape stays PROVISIONAL until S643's
    // screens prove it (REVISIT S640b/S641 metadata-fine-tune token). Harvested by the
    // real catalogue-driven tree-scanner (RESOLVED AT S642).
    public enum QuadKind
    {
        Solid,   // solid geometry -> tight tolerance
        Text,    // -> auto-locate + tolerance band
        Icon,    // -> presence / centroid / loose
    }

    public sealed class QuadMetadata
    {
        public string Name { get; set; } = "";
        public int X { get; set; }
        public int Y { get; set; }
        public int Width { get; set; }
        public int Height { get; set; }
        public QuadKind Kind { get; set; } = QuadKind.Solid;
        // S642 — the SYNC-TOKEN capability id (e.g. "text_01", "icon_02") this quad
        // instantiates; "" when the quad is not catalogue-backed. Carries the
        // per-quad tolerance/kind the S641/S642 compare needs.
        public string Capability { get; set; } = "";
        public double Tolerance { get; set; } = 0.02;
    }

    // The sidecar written next to the .bgra frame. Gives the scanner the declared
    // quads (with kinds) it needs to apply per-kind tolerance handling, plus the
    // frame dims.
    public sealed class EmitterMetadata
    {
        public uint Width { get; set; }
        public uint Height { get; set; }
        public string Source { get; set; } = "";
        // S643 — the DRIVER's input-hash (approved scheme): SHA-256 over the PRINCIPLE
        // SOURCE FILES (the test .xaml + capability_catalogue.json + wpf_emitter/*.cs +
        // the scanner sources/headers). The driver recomputes it and compares to this value:
        // equal => A (the composer) is SKIPPED (only B compares); mismatch => A re-renders.
        // Rebuilding WPF does NOT change it (no exe bytes, no version constant — source only).
        public string InputHash { get; set; } = "";
        public List<QuadMetadata> Quads { get; set; } = new List<QuadMetadata>();
    }
}
