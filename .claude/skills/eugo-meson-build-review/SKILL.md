---
name: eugo-meson-build-review
description: Pre-commit review checklist for build-file changes in the Eugo gRPC fork - the root meson.build/pyproject.toml, @EUGO_CHANGE discipline in setup.py/commands.py/log.cc, the macro decision framework, the Cython-vs-src/core link-dep rule, and the gates that must pass before pushing. Activates on "review meson.build", "review grpc build change", "check EUGO_CHANGE", "new macro in setup.py", "pyproject dependencies", "validate grpcio build change".
---

# Eugo gRPC build-file review (pre-commit)

## Scope: what is Eugo-owned here

- Eugo-created, no upstream counterpart (no markers needed): root
  `meson.build` + `pyproject.toml`, `src/python/grpcio_observability/`
  meson files, `tools/distrib/python/grpcio_tools/meson.build` (experimental
  reference - never built).
- Upstream files with `@EUGO_CHANGE` blocks: `setup.py`,
  `src/python/grpcio/commands.py`, `src/core/util/log.cc`. Several older
  `setup.py`/`commands.py` edits are UNMARKED debt - see the "Modified
  upstream files" table in `.github/copilot-instructions.md`; add markers
  when touching those lines, never silently revert them.
- Upstream CMake (`CMakeLists.txt`, `cmake/`) is intentionally UNMODIFIED.
  Eugo's CMake configuration lives in the consumer
  (`protomolecule/dependencies/native/grpc/setup` - e.g. the
  `gRPC_INSTALL_LIBDIR` pinning). A diff adding Eugo logic to the repo's
  CMake files is almost always wrong - move it to the consumer setup.

## Annotation discipline

Every modification to a file that exists upstream gets:

```python
# @EUGO_CHANGE: @begin: <why, not what>
...
# @EUGO_CHANGE: @end
```

(`//` form for C/C++.) In `meson.build`, use the section markers
`# === @begin: Name ===` / `# === @end: Name ===`, and every excluded or
commented-out macro MUST carry a comment explaining why (these comments are
the fork's institutional memory - see the existing file).

## The checklist

1. **New macro in setup.py's DEFINE_MACROS?** Run the 4-question decision
   framework (copilot-instructions, "Decision framework for new macros"):
   only checked in `src/core/`? skip. Checked in Cython or `include/`
   headers? add to `meson.build`. ABI-affecting? must match what CMake built
   `libgrpc` with. Record the verdict in the known-macro-decisions table.
2. **New link dependency?** If the undefined symbol is called from Cython
   (`.pyx`/`.pxi`), add it to `meson.build` (it will not come from libgrpc -
   precedent: `absl::log_initialize` for `absl::InitializeLog()`). If called
   from `src/core/`, it belongs in libgrpc - investigate, do not paper over.
3. **`setup.py:NNN` citations fresh?** `meson.build` comments cite setup.py
   line numbers; any setup.py change shifts them. Scan and update.
4. **New `.pyx` in CYTHON_EXTENSION_MODULE_NAMES or new CYTHON_HELPER_C
   file?** Needs a matching `meson.build` entry. New `grpc_core_dependencies`
   `.cc` files do NOT (they compile into system libgrpc, not cygrpc).
5. **INSTALL_REQUIRES changed?** Mirror into `pyproject.toml`
   `[project] dependencies` (currently `typing-extensions~=4.12`).
6. **Install rules touched?** New/moved pure-Python files or data
   (`roots.pem`) must survive the `install_subdir`/`install_data` rules -
   verify with the wheel file-list diff, not by eye.
7. **No leftover `<<<<<<<`/`>>>>>>>` markers; all `@begin` have `@end`.**

## Gates before pushing

- `pip install . --no-build-isolation` succeeds against system libgrpc.
- Wheel-vs-upstream comparison + smoke tests 6a-6e
  (`.github/copilot-instructions.md`, "Validating the Eugo wheel against
  upstream"): identical file list, `PyInit_cygrpc` is the only `T` symbol,
  `NEEDED` lists `libgrpc.so`/`libgpr.so`.
- If CMake-adjacent files changed: native configure + build still pass.

## Related

- `eugo-upstream-merge` - the merge-time version of these checks.
- `eugo-build-and-test` / `eugo-rebuild` - build commands and scope.
- `meson` skill - general Meson reference (wraps, options, cross files).
