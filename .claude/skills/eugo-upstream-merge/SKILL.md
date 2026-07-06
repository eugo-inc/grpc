---
name: eugo-upstream-merge
description: Merge upstream grpc/grpc:master into the Eugo fork (eugo-inc/grpc, branch eugo-main). Covers the fork's authoritative divergence inventory (new files, modified upstream files, unmarked debt), the per-file conflict-resolution rules, the merge-commit-only recipe, the post-merge validation checklist, and the protomolecule commit-pin bump. Activates on "merge upstream", "upstream sync", "catch up to grpc master", "grpc fork merge", "bump grpc pin", "conflict resolution rules".
---

# Eugo gRPC fork: upstream merge

## What this fork is / who consumes it

Fork of https://github.com/grpc/grpc. Canonical branch is `eugo-main`.

| Item | Value |
|---|---|
| Upstream repo | `https://github.com/grpc/grpc.git` (remote: `upstream`, branch: `master`) |
| Fork repo | `https://github.com/eugo-inc/grpc.git` (remote: `origin`, branch: `eugo-main`) |
| Fork base commit | `d231d3cd473cf0719eaca79a1a2ff8fd774c0561` |
| Fork author emails | `benjamin.w.leff@gmail.com`, `17219379+BwL1289@users.noreply.github.com` |
| Target platform | Linux only |
| Python | >= 3.12 |
| C/C++ standards | C: `gnu17`, C++: `gnu++23` |

Never commit with upstream author identities.

Two protomolecule packages consume the fork, both pinned by **git_commit** in
`meta.json` (`version.kind: git_commit`, `branch: eugo-main`,
`should_auto_update: true`):

- `protomolecule/dependencies/native/grpc` - CMake build of the C/C++ libs against
  system deps (absl, protobuf, c-ares, zlib-ng, openssl, re2, opencensus-cpp;
  boringssl/systemd/otel deliberately off). See its `setup` for the exact cmake args.
- `protomolecule/dependencies/python/wave_4/grpcio` - Meson build of `cygrpc.so`
  linking system libgrpc (the Eugo-added root `meson.build` + `pyproject.toml`).

Commit pins decouple merge from adoption: merging upstream into fork `eugo-main`
does NOT change what protomolecule builds until the meta.json pins are bumped.

## Eugo divergence inventory (authoritative)

This is the authoritative catalog. Update it here when adding new Eugo
modifications.

### New files (entirely Eugo-created, never conflict, no markers needed)

| File | Purpose |
|---|---|
| `meson.build` (root) | Meson build for `python/grpcio` - builds `cygrpc.so` and installs pure-Python files |
| `pyproject.toml` (root, Eugo side) | `meson-python` build backend config for `grpcio` |
| `tools/distrib/python/grpcio_tools/meson.build` | Meson build for `grpcio_tools` (experimental/reference - grpcio_tools is NOT built; native protoc + grpc_python_plugin replace it, see `eugo-grpcio-tools-migration`) |
| `tools/distrib/python/grpcio_tools/pyproject.toml` (Eugo side) | `meson-python` config for `grpcio_tools` |
| `tools/distrib/python/grpcio_tools/eugo_copy_sources.py` | Helper to copy `include/` and `src/compiler/` for `grpcio_tools` build |
| `tools/distrib/python/grpcio_tools/.gitignore` | Ignore build artifacts for `grpcio_tools` |
| `tools/distrib/python/.gitignore` | Ignore `distrib_virtualenv/`, `build/`, `*.stamp` |
| `src/python/grpcio_observability/` meson files | Meson build for grpcio_observability |
| `.claude/skills/`, `.devcontainer/`, `CLAUDE.md` | Eugo tooling |

### Modified upstream files

Rows marked **UNMARKED** are known annotation debt: the edit exists but lacks
`@EUGO_CHANGE` markers. Add markers when touching those lines; never silently
revert them when taking upstream hunks. Line numbers drift with upstream churn.

| File | Change | Marker |
|---|---|---|
| `src/core/util/log.cc` | Added `#include "absl/log/vlog_is_on.h"` after `#include "absl/log/log.h"` (newer abseil no longer transitively includes it) | `@EUGO_CHANGE` |
| `setup.py:332-333` | Exclude `src/core`, `third_party/upb`, `third_party/address_sorting`, `third_party/utf8_range` from `CORE_C_FILES` when `BUILD_WITH_SYSTEM_GRPC=true` (avoid duplicating symbols already in `libgrpc`) | `@EUGO_CHANGE` |
| `setup.py:526-544` | Debug logging of collection contents during `cython_extensions_and_necessity()` | `@EUGO_CHANGE` |
| `setup.py:458` | `GRPC_ENABLE_FORK_SUPPORT` macro - kept from upstream but annotated `@HELP(now)` | **UNMARKED** - needs `@EUGO_CHANGE` |
| `setup.py:478` | Changed `GRPC_DO_NOT_INSTANTIATE_POSIX_POLLER` macro (upstream changed to `GRPC_PYTHON_BUILD`) | **UNMARKED** - needs `@EUGO_CHANGE` |
| `setup.py:490` | `__STDC_FORMAT_MACROS` annotated `@HELP(now)` | **UNMARKED** - needs `@EUGO_CHANGE` |
| `setup.py:495-497` | `pymodinit` f-string style (cosmetic, upstream uses `.format()`) | **UNMARKED** - needs `@EUGO_CHANGE` |
| `setup.py:549` | `sources=` line: upstream unconditionally adds `absl/log/initialize.cc` (bundled-abseil case); we guard it as `[] if BUILD_WITH_SYSTEM_ABSL else [...]` to avoid duplicate symbols when linking system abseil. `InitializeLog()` is called from `absl.pyx.pxi` and provided by `libabsl_log_initialize.so` in system installs. | `@EUGO_CHANGE` |
| `setup.py:79-84` | Removed duplicate `import grpc_core_dependencies` | **UNMARKED** - needs `@EUGO_CHANGE` |
| `setup.py:95-106` | `CLASSIFIERS` as tuple-of-list vs upstream's plain list (cosmetic) | **UNMARKED** - needs `@EUGO_CHANGE` |
| `src/python/grpcio/commands.py:96,124,311` | Simplified string formatting, `GRPC_STEM`/`GRPC_ROOT` path differences | **UNMARKED** - needs `@EUGO_CHANGE` |
| `src/python/grpcio/grpc/_grpcio_metadata.py` | Hardcoded version `1.75.0.dev0` (upstream uses auto-generated template) | **UNMARKED** - needs `@EUGO_CHANGE` |
| `.gitignore:8` | Added `eugo_build/` | **UNMARKED** - needs `@EUGO_CHANGE` |
| `tools/distrib/python/grpcio_tools/setup.py:301` | `PROTO_INCLUDE = "/usr/local/include/"` hardcoded path | `@TODO+:Eugo` |

### Resolved conflicts - accepted upstream (precedent log)

| Location | Eugo side | Upstream side | Decision | Reason |
|---|---|---|---|---|
| `setup.py` `pymodinit_type` / `pymodinit` (was line ~495) | f-string + `PY3` ternary | `.format()` style, only `PyObject*` | **Accept upstream** | Python 2 (`PY3` guard, `void` return) is dead. Upstream already dropped it. No Eugo-specific divergence. |

## Merge recipe

```bash
git clone git@github.com:eugo-inc/grpc.git && cd grpc
git remote add upstream https://github.com/grpc/grpc.git
git fetch upstream
git checkout -b <user>/feat/MM-DD-YY-merge-upstream eugo-main   # prior art: bwl1289/chore/merge-upstream
git merge upstream/master        # merge, NOT rebase - preserve fork history
```

### Conflict resolution rules

1. **Files in the "Modified upstream files" table above**: Keep the Eugo side
   (`HEAD`/ours) for code within `@EUGO_CHANGE` markers. Accept upstream
   changes for everything else.
2. **`pyproject.toml` (root)**: Always keep the Eugo (Meson-based) version.
   Discard the upstream setuptools version entirely.
3. **`tools/distrib/python/grpcio_tools/pyproject.toml`**: Always keep the
   Eugo version.
4. **`src/python/grpcio/grpc/_grpcio_metadata.py`**: Update the version number
   to match upstream's new version, but keep our simplified format (no
   copyright header needed - it's auto-generated).
5. **`setup.py`**: This file has the most conflicts. For sections within
   `@EUGO_CHANGE` markers, keep ours. For other sections, prefer upstream. Pay
   special attention to `DEFINE_MACROS` - upstream may add new macros we
   should evaluate (run the decision framework in `eugo-meson-build-review`).
6. **`src/core/util/log.cc`**: Check if upstream has added the `vlog_is_on.h`
   include. If so, our change can be dropped. If not, keep it.
7. **New upstream files**: Accept them. They don't conflict with our changes.
8. **`meson.build` files**: These are Eugo-only so they won't conflict, but
   may need updates if upstream changes source file locations or dependencies.

## After merge - validation checklist

- [ ] All `@EUGO_CHANGE` markers are intact
- [ ] No leftover `<<<<<<< HEAD` / `=======` / `>>>>>>>` conflict markers
- [ ] `meson.build` (root) still references correct source paths
- [ ] **Inline comments in `meson.build` are up to date** - the comments
      reference specific `setup.py` line numbers (e.g. `setup.py:280`,
      `setup.py:346`, `setup.py:472-473`) and explain *why* each value was
      chosen. After merging upstream, `setup.py` line numbers shift and the
      referenced logic may move or change. Scan `meson.build` for every
      `setup.py:NNN` citation and verify it still points at the correct line
      and that the described behavior matches the current upstream code.
      Update citations and prose as needed.
- [ ] Native gRPC builds: `cmake -B build && cmake --build build`
- [ ] Python grpcio builds: `pip install .` (Meson)
- [ ] `protoc --grpc_python_out` works with installed `grpc_python_plugin`
- [ ] Wheel-vs-upstream comparison + smoke tests 6a-6e pass
      (`eugo-wheel-validation`)

Then:

```bash
git push origin <branch>
# Open PR to eugo-main. Merge with a MERGE COMMIT only - never squash:
# squashing destroys upstream ancestry and makes every future sync re-conflict.
```

## Post-merge adoption

After the PR lands on `eugo-main`, bump BOTH pins to the new merge commit:

- `protomolecule/dependencies/native/grpc/meta.json` -> `version.commit`
- `protomolecule/dependencies/python/wave_4/grpcio/meta.json` -> `version.commit`

Keep the two pins identical - grpcio's meson build links the libgrpc that
native/grpc installed; skew between them is an ABI trap. Then rebuild native/grpc
first, grpcio second (runtime dep order).

## Push-before-pin warning

Both meta.json pins are raw commit SHAs fetched by `git fetch origin <commit>` from
eugo-inc/grpc. A SHA that exists only locally (or only on an unmerged branch that
later gets squashed/deleted) breaks the protomolecule build. Push to `eugo-main`
first, verify the SHA is reachable on GitHub, then edit meta.json.
