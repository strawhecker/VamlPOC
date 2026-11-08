# WPF Reference Emitter (S640b)

**Windows-authoring-only; PRECEDES S641.** A small C#/WPF app that renders an authored
`.xaml` OFFSCREEN via `RenderTargetBitmap`, blits to the SAME `ImageBuffer` BGRA shape
used on the C++ side (the convergence contract), and DEPOSITS the diffable reference
material on disk for S641's `compliance_compare`.

This step is **C#/WPF only**. NO compare, NO `SCAN DECLARATION` parsing, NO capability
matrix here — those are S641/S642 (see `docs/roadmap.md`).

## Build & run (Windows only — Ubuntu does not author WPF)

```bat
cd E:\GitRepos\VamlPOC\wpf_emitter
dotnet build -c Release
dotnet run -c Release -- samples\WpfEmitterSample.xaml out.bgra out.json
```

## CLI

```
WpfEmitter.exe <file.xaml> <out.bgra> [out.json]
```

Write-only deposit. Produces:

| File      | Content                                                          |
|-----------|------------------------------------------------------------------|
| `out.bgra`| tight `w*h*4` raw **BGRA**, row 0 = **top** (the ImageBuffer shape) |
| `out.json`| `EmitterMetadata` sidecar: dims + per-quad **text/icon/solid** kind |

## Convergence contract (the SAME ImageBuffer)

C# `ImageBuffer { Width, Height, byte[] Bgra }` is the twin of
`vaml::presentation::Impl::ImageBuffer { width, height, std::vector<uint8_t> bgra }`
(`src/presentation/include/vaml/presentation/RenderTarget.h`). Both sides MUST deposit
byte-identical BGRA so S641's compare sees identical raw frames. Any drift is a **named**
PARITY-MATRIX divergence, never silent.

- `bgra` holds EXACTLY `width*height*4` bytes after a successful render.
- BGRA order, **row 0 = the image's top row** (NOT bottom-up).
- The emitter uses `PixelFormats.Pbgra32` from `RenderTargetBitmap`, which IS BGRA byte
  order (premultiplied BGRA) — matches the contract.

## Scanner metadata (output the scanner READS)

The emitter cannot perfectly prognosticate what each diff kind needs (text → auto-locate +
tolerance band; icon → presence/centroid + loose; solid → tight). So the emitter DEPOSITS
machine-readable per-quad metadata into `out.json`:

- `Quads[]`: name, x/y (frame-space), w/h, and a declared `kind` (`Solid`/`Text`/`Icon`).
- Harvested by the **tree-scanner placeholder** (`ElementTreeScanner.cs`) that walks the
  emitter's OWN rendered element tree and emits text-bearing (TextBlock) and icon-bearing
  (Image/Shape) nodes now, as a placeholder for the S642 typed-slot/icon/text catalogue.
  Tagged `// [MINIMAL REAL] WPF metadata tree-scanner — RESOLVED BY S642`.
- S641 READS whatever this deposits TODAY; the reader is forward-compatible. S642/S643
  fine-tune the metadata shape (roadmap REVISIT S640b/S641 tokens).

## Samples

- `samples/WpfEmitterSample.xaml` — a `StackPanel` element tree with a `TextBlock`
  (text quad) + an `Image` (icon quad) → proves offscreen render → ArrayBuffer → deposit.

## Not in CI

The emitter is a standalone `dotnet` project, **excluded from the CMake/slnx build and BOTH
CI lanes** (`windows-msvc`, `ubuntu-clang`). Ubuntu builds but does not author WPF. It is
exercised by the Windows-authoring gate only: `build/verify_s640b.cmd`.
