## Summary (REQUIRED)
<!-- One-line summary of the change — what and why. -->

## Related DREADNOT / ECO
<!-- e.g., Fixes: #<issue> (if applicable), ECO / design doc: link -->

## Motivation & Context (REQUIRED)
<!-- Why this change is needed; user-visible impact; short rationale. -->

## Type of change
- [ ] Bug fix
- [ ] New feature
- [ ] Refactor
- [ ] Performance
- [ ] Tests
- [ ] Docs
- [ ] Chore
- [ ] Breaking change

## How was this tested? (REQUIRED)
<!--
- Environment: OS, Visual Studio version (Help → About), MSVC toolset (v143), commit: `git rev-parse --short HEAD`
- Build commands:
  - `msbuild <Solution>.sln /p:Configuration=Release /p:Platform=x64`
  - or `cmake --build build --config Release`
- Tests run (list): e.g., `MyModuleTests`, `Integration:HardwareTests`
  - Commands: `vstest.console.exe path\to\MyTests.dll` or `.\MyGoogleTests.exe --gtest_filter=...`
- Manual verification steps (if any).
-->

## CI / build impact
<!--
- Affected projects/modules:
- New warnings introduced? Yes / No
- CI jobs to run (names): e.g., `build-windows`, `unit-tests`, `perf-nightly`
- How to reproduce CI failures locally (commands / env vars):
-->

## Files changed (high level)
<!--
- `path/to/ChangedFile.h` — brief note
- `path/to/ChangedFile.cpp` — brief note
-->

## Documentation impact
- [ ] README updated
- [ ] Public API docs updated
- [ ] Examples updated
- Docs to update (files): `...`

## Performance impact
<!-- Summary (e.g., “no impact” or “~3% faster for X”); attach raw benchmark output or link to artifact. -->

## Security considerations
<!-- Does this touch secrets, input parsing, or crypto? Yes / No. If Yes, describe mitigations and tests. -->

## Migration / Compatibility
<!-- Backwards compatible? Yes / No. If breaking: migration steps and targeted removal release. -->

## Rollout & feature flags
<!-- Feature flag? Yes / No — how to enable. Rollout plan (if applicable) -->

## Artifacts & logs
<!-- Attach `build.log`, `test-results.trx`, crash dumps (.dmp), and PDBs if relevant. Links to CI artifacts: -->

## Checklist (REQUIRED)
- [ ] I followed the [coding standards](../CONTRIBUTING.md) and `.editorconfig`
- [ ] I ran formatting (`clang-format`/`dotnet format`) and fixed issues
- [ ] I added/updated tests and they pass locally
- [ ] I updated documentation where applicable
- [ ] PR title follows `<type>(<scope>): <summary>` convention
- [ ] I included reproduction steps and logs for failures (if any)

## Notes for reviewers
<!-- Scope summary, complexity, risk areas, suggested reviewer(s) / CODEOWNERS paths -->

## Release notes
<!-- Write a short summary for inclusion in release notes or changelog. -->

## Breaking change confirmation
<!--
- [ ] I confirm this PR introduces a breaking change and have provided migration steps above.
---
<!--
Guidance:
- Keep PRs small and focused.
- Attach CI artifacts or logs for failing pipelines.
- For large design changes include a linked ADR or design doc.
- Mark breaking changes with the `breaking-change` label and follow the deprecation policy in CONTRIBUTING.md
-->

## Reviewer instructions
<!-- Call out specific files, logic, or areas for reviewers to focus on. Tag suggested reviewers if needed. -->
