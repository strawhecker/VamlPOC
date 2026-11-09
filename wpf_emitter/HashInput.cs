using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Text;

namespace Vaml.WpfEmitter
{
    // S643 — the DRIVER's input-hash scheme (approved; roadmap S643 DRIVER DESIGN token).
    //
    // The metadata `.json` sidecar carries `InputHash` — a SHA-256 over the PRINCIPLE
    // FILES that determine the emitted output, READ IN + APPENDED. The principle set is:
    //    1. the test's own `.xaml` (content + SCAN DECLARATION)
    //    2. tools/capability_catalogue.json (the capability ids/kinds/tolerances the
    //       declaration resolves against)
    //    3. wpf_emitter/*.cs            (the WPF composer source — determines the expected frame+meta)
    //    4. src/presentation/src/scan/*.{cpp,h}                       (the scanner/parse source)
    //    5. src/presentation/include/vaml/presentation/scan/*.h       (the scanner public headers)
    // The file DIRECTORY (paths) is NOT part of the hash — only the concatenated CONTENT
    // bytes, in the deterministic order above (each sub-list sorted by name). This deliberately
    // excludes the rebuilt executable (WPF build always churns binary bytes) and any
    // hand-bumped version constant (a forgotten bump = a silent stale frame). At WPF's
    // current stage CHANGING THE SOURCE is what matters: any edit to the emitter/scanner
    // source re-renders; a no-op rebuild reuses the cached frame.
    //
    // The DRIVER (build/run_compare.cmd + tools/compare_hash.py) recomputes this hash at
    // run time and compares it to the value stored in the metadata: equal => A (the composer)
    // SKIPS and only B (the compare) runs; mismatch => A re-renders (expected + actual) and
    // rewrites the metadata. `--rebuild` + deleting an artifact file remain manual overrides,
    // NEVER the correctness mechanism. `tools/compare_hash.py` is the single run-time
    // authority; this C# compute is for standalone/manual emitter runs (self-describing JSON).
    public static class HashInput
    {
        // Returns the lower-case hex SHA-256 over the principle source files for `xamlPath`.
        // Never throws for a missing catalogue/scanner source: a missing PRINCIPLE file is
        // a real gap and must FAIL LOUDLY (an empty hash would silently mask it).
        public static string Compute(string xamlPath)
        {
            string xaml = Path.GetFullPath(xamlPath);
            string root = RepoRoot(xaml);
            var files = new List<string>();

            // 1. the test's own .xaml (must exist — the caller already validated it).
            files.Add(xaml);

            // 2. the committed capability catalogue (single source of truth).
            string cat = Path.Combine(root, "tools", "capability_catalogue.json");
            if (!File.Exists(cat))
                throw new FileNotFoundException("principle file missing: " + cat);
            files.Add(cat);

            // 3. the WPF composer sources.
            foreach (string f in Sorted(Path.Combine(root, "wpf_emitter"), "*.cs"))
                files.Add(f);

            // 4. the scanner implementation sources (src side).
            foreach (string f in Sorted(Path.Combine(root, "src", "presentation", "src", "scan"), new[] { "*.cpp", "*.h" }))
                files.Add(f);

            // 5. the scanner public headers (include side).
            foreach (string f in Sorted(Path.Combine(root, "src", "presentation", "include", "vaml", "presentation", "scan"), "*.h"))
                files.Add(f);

            if (files.Count < 4)
                throw new InvalidOperationException("input-hash principle set is under-populated — cannot produce a trustworthy hash");

            using (var sha = SHA256.Create())
            {
                foreach (string f in files)
                {
                    byte[] content = File.ReadAllBytes(f);
                    sha.TransformBlock(content, 0, content.Length, null, 0);
                }
                sha.TransformFinalBlock(Array.Empty<byte>(), 0, 0);
                return ToHex(sha.Hash);
            }
        }

        private static string RepoRoot(string xamlFull)
        {
            // xaml lives at <root>/vaml_ui_tester/<file>.
            string dir = Path.GetDirectoryName(xamlFull) ?? "";
            string gp = Path.GetDirectoryName(dir) ?? "";
            return Path.GetFullPath(gp);
        }

        private static List<string> Sorted(string dir, string pattern)
        {
            var list = new List<string>();
            if (Directory.Exists(dir))
                list.AddRange(Directory.GetFiles(dir, pattern, SearchOption.TopDirectoryOnly));
            list.Sort(StringComparer.Ordinal);
            return list;
        }

        private static List<string> Sorted(string dir, string[] patterns)
        {
            var list = new List<string>();
            if (Directory.Exists(dir))
            {
                foreach (string pat in patterns)
                    list.AddRange(Directory.GetFiles(dir, pat, SearchOption.TopDirectoryOnly));
            }
            list = list.Distinct(StringComparer.Ordinal).ToList();
            list.Sort(StringComparer.Ordinal);
            return list;
        }

        private static string ToHex(byte[] bytes)
        {
            var sb = new StringBuilder(bytes.Length * 2);
            foreach (byte b in bytes)
                sb.Append(b.ToString("x2"));
            return sb.ToString();
        }
    }
}
