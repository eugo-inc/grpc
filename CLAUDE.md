# CLAUDE.md

Eugo fork of grpc/grpc (`eugo-inc/grpc`, branch `eugo-main`): Linux-only,
Python >= 3.12, gnu++23. grpcio is built with Meson (root `meson.build` +
mesonpy `pyproject.toml`) linking system `libgrpc`; `grpcio_tools` is
eliminated -> use native `protoc` + `grpc_python_plugin` instead.
Full guide: [.github/copilot-instructions.md](.github/copilot-instructions.md)
(sections are named, not numbered). Route by what you are about to do:

- Editing any file that exists upstream -> wrap it per "Change marking
  convention" (`@EUGO_CHANGE: @begin ... @end`). NEVER modify
  `third_party/` -> use system packages (see "Boundaries").
- Building -> "Build commands": CMake for native/grpc; grpcio via
  `pip install . --no-build-isolation`. NEVER build/ship grpcio_tools ->
  native protoc path ("How `grpcio_tools` was eliminated").
- About to merge upstream -> "Upstream merge workflow" (merge, not rebase;
  history is merge commits), resolve per "Conflict resolution rules", then
  run every item in "After merge — validation checklist", incl. re-verifying
  each `setup.py:NNN` citation inside `meson.build`.
- Upstream `setup.py` adds a macro / source file / `.pyx` -> "Keeping
  meson.build in sync with upstream" decision framework; grep where the
  macro is checked before adding it to `meson.build`.
- Wheel built or merge finished -> "Validating the Eugo wheel against
  upstream": file-listing diff clean, `PyInit_cygrpc` exported as `T`
  symbol, smoke tests 6a-6e all print "passed".
