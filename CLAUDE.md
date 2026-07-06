# CLAUDE.md

Eugo fork of grpc/grpc (`eugo-inc/grpc`, branch `eugo-main`): Linux-only,
Python >= 3.12, C gnu17 / C++ gnu++23. Two products: native libs via
upstream CMake, and grpcio built with Meson (Eugo root `meson.build` +
mesonpy `pyproject.toml`) linking system `libgrpc` - it compiles ONLY
`cygrpc.pyx`; everything else lives in system libgrpc. `grpcio_tools` is
eliminated -> native `protoc` + `grpc_python_plugin` replace it. Playbooks
live in `.claude/skills/` - route by event:

- Building or smoke-testing -> `eugo-build-and-test`: CMake for native/grpc,
  `pip install . --no-build-isolation` for grpcio, native ALWAYS first (the
  wheel links it). NEVER build or ship `grpcio_tools` -> use native protoc
  (`eugo-grpcio-tools-migration` has the invocation + downstream migration).
- Unsure how much validation a diff needs -> `eugo-rebuild` (files -> action).
- Editing any file that exists upstream -> wrap the edit in
  `@EUGO_CHANGE: @begin ... @end` markers, and update the divergence
  inventory in `eugo-upstream-merge` in the same change. NEVER modify
  `third_party/` -> we use system packages; NEVER remove an `@EUGO_CHANGE`
  marker -> replace it or ask first.
- Touched `meson.build` / `setup.py` / `pyproject.toml` ->
  `eugo-meson-build-review` before committing: macro decision framework
  (most setup.py macros are src/core-only -> skip), known-decision tables,
  Cython-vs-src/core link-dep rule, code style.
- About to merge upstream -> `eugo-upstream-merge` (merge, NOT rebase;
  merge-commit-only PR to `eugo-main` - squash destroys upstream ancestry).
  Upstream branch is `master`; conflict rules + divergence tables are in
  the skill. After merging, re-verify every `setup.py:NNN` citation inside
  `meson.build` - upstream churn shifts the cited lines.
- Wheel built or merge finished -> `eugo-wheel-validation`: file-listing
  diff clean, `PyInit_cygrpc` exported as the only `T` symbol
  (`nm -DC .../cygrpc*.so | grep ' T '`), `libgrpc.so`/`libgpr.so` in
  NEEDED, smoke tests 6a-6e all print "passed".
- Changing grpc CMake behavior -> the repo's `CMakeLists.txt`/`cmake/` are
  intentionally UNMODIFIED; Eugo CMake config lives in
  `protomolecule/dependencies/native/grpc/setup`.

NEVER add Eugo CI to `.github/workflows/` -> upstream CI would break; NEVER
commit with upstream author identities -> use your own. Ask first before:
new `meson.build` dependencies, `src/core/` edits, or changing the
protoc/grpc_python_plugin invocation interface.
