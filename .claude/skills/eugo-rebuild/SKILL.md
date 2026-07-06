---
name: eugo-rebuild
description: Decide what level of rebuild a change to the Eugo gRPC fork actually requires - nothing, grpcio wheel only, or native CMake + grpcio in ABI order - and when the protomolecule pins must be bumped. Activates on "rebuild grpc", "do I need to rebuild grpcio", "does this need a native rebuild", "grpc pin bump", "is this a python-only grpc change".
---

# eugo-rebuild (gRPC fork)

Given a diff, pick the first matching row. Two build products exist: the
native libs (CMake, `native/grpc`) and the grpcio wheel (meson-python,
`wave_4/grpcio`). The wheel links the native libs, so native changes imply
BOTH rebuilds, in that order.

| Files changed | Action |
|---|---|
| Docs, `.claude/`, `.github/`, comments only | Nothing. |
| Pure Python under `src/python/grpcio/grpc/` (no `.pyx`/`.pxi`/`.pxd`) | grpcio wheel rebuild only (`pip install . --no-build-isolation`) - meson installs `.py` verbatim, no native compile. |
| Cython (`.pyx`/`.pxi`/`.pxd`), root `meson.build`, `pyproject.toml` | grpcio rebuild + the wheel-vs-upstream file-list check (`eugo-wheel-validation`). |
| `src/core/**`, `include/**`, `CMakeLists.txt`, `cmake/**`, `third_party` pointer | Native CMake rebuild + install, THEN grpcio rebuild (cygrpc must relink against the new libgrpc ABI). |
| `setup.py` / `commands.py` | No build runs from them in Eugo (meson replaced setuptools) - but run the macro decision framework (`eugo-meson-build-review`) to see if `meson.build` must mirror the change; if it does, previous row applies. |
| `tools/distrib/python/grpcio_tools/**` | Nothing - experimental reference only, never built. |

## Cheap pre-build sanity

- meson-touching change: `meson setup eugo_build` (configure only) catches
  bad `dependency()` lookups and install-rule syntax before a wheel build.
- CMake-touching change: `cmake -B build -S . -DCMAKE_BUILD_TYPE=Release`
  configure catches missing system packages and dangling targets. Note the
  upstream CMake is intentionally UNMODIFIED in this fork - a diff there is
  usually an upstream merge, not an Eugo change.

## When protomolecule must be rebuilt / pin-bumped

Merging to fork `eugo-main` changes NOTHING downstream until the meta.json
pins move. After a merge lands: push `eugo-main`, verify the SHA is on
GitHub, then
bump BOTH `native/grpc/meta.json` and `wave_4/grpcio/meta.json` to the SAME
commit, and rebuild native/grpc before grpcio (runtime dep order). Never pin
a local-only or squashed-away SHA. Details: `eugo-upstream-merge`.

## Related

- `eugo-build-and-test` - full build commands and healthy-state checks.
- `eugo-meson-build-review` - review the diff before committing it.
