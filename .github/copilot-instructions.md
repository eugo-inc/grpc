# Eugo gRPC Fork — Copilot Instructions

You are an expert maintainer of Eugo's fork of [grpc/grpc](https://github.com/grpc/grpc). This fork (`eugo-inc/grpc`) modifies upstream gRPC to support Eugo's build and deployment pipeline on **Linux only**, targeting **Python >= 3.12** with **C++23** (`gnu++23`).

## Key context

| Item | Value |
|---|---|
| Upstream repo | `https://github.com/grpc/grpc.git` (remote: `upstream`) |
| Fork repo | `https://github.com/eugo-inc/grpc.git` (remote: `origin`) |
| Fork base commit | `d231d3cd473cf0719eaca79a1a2ff8fd774c0561` |
| Fork author emails | `benjamin.w.leff@gmail.com`, `17219379+BwL1289@users.noreply.github.com` |
| Target platform | Linux only |
| Python | >= 3.12 |
| C/C++ standards | C: `gnu17`, C++: `gnu++23` |

### What this fork changes

1. **`python/grpcio` build system**: Replaced `setuptools`/`setup.py` with **Meson** (`meson.build` + `pyproject.toml` at repo root). The extension `cygrpc.so` is built via Meson linking against system-installed `libgrpc`/`libgpr` (found via CMake config).

2. **`python/grpcio_tools` is eliminated**: Upstream ships a separate `grpcio_tools` package that bundles `protoc` + `grpc_python_plugin` as a Python extension. Our fork **does not need this**. Instead, users invoke the native `protoc` binary with `grpc_python_plugin` directly:
   ```bash
   protoc \
     --plugin=protoc-gen-grpc_python="$(which grpc_python_plugin)" \
     --proto_path="${SOURCE_ROOT}" \
     --python_out="${BUILD_ROOT}" \
     --grpc_python_out="${BUILD_ROOT}" \
     --grpc_python_opt="grpc_2_0" \
     "${INPUT}"
   ```
   This works because we build `native/grpc` (via CMake) which installs `protoc`, `grpc_python_plugin`, headers, and libraries system-wide.

3. **`native/grpc` (C/C++)**: Built with **CMake** (upstream's supported build). One source-level fix applied to `src/core/util/log.cc`.

4. **`src/core/util/log.cc`**: Added missing `#include "absl/log/vlog_is_on.h"` for newer abseil versions that no longer transitively include it.

5. **`setup.py` modifications**: When `BUILD_WITH_SYSTEM_GRPC=true`, exclude `src/core`, `third_party/upb`, `third_party/address_sorting`, `third_party/utf8_range` from compilation to avoid duplicating symbols already in `libgrpc`.

## Build commands

### native/grpc (C/C++ libraries)
```bash
# Standard upstream CMake build — installs to /usr/local
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
cmake --install build
```

### python/grpcio (Python extension)
```bash
# Meson-based build (our build system)
pip install meson-python meson cmake cython
pip install . --no-build-isolation  # or: pip wheel .
```
The Meson build file is at `./meson.build`. It:
- Finds gRPC via `dependency('gRPC', method: 'cmake', modules: ['gRPC::gpr', 'gRPC::grpc'])`
- Builds `cygrpc.so` from `src/python/grpcio/grpc/_cython/cygrpc.pyx`
- Installs pure-Python `grpc/` package files

### python/grpcio_tools (NOT USED)
Do **not** build `grpcio_tools` separately. The Meson build at `tools/distrib/python/grpcio_tools/meson.build` exists as a reference/experiment but is not the intended approach. Use native `protoc` + `grpc_python_plugin` instead.

## Project structure — Eugo-modified files

### New files (entirely Eugo-created)
| File | Purpose |
|---|---|
| `meson.build` (root) | Meson build for `python/grpcio` — builds `cygrpc.so` and installs pure-Python files |
| `pyproject.toml` (root, Eugo side) | `meson-python` build backend config for `grpcio` |
| `tools/distrib/python/grpcio_tools/meson.build` | Meson build for `grpcio_tools` (experimental/reference) |
| `tools/distrib/python/grpcio_tools/pyproject.toml` (Eugo side) | `meson-python` config for `grpcio_tools` |
| `tools/distrib/python/grpcio_tools/eugo_copy_sources.py` | Helper to copy `include/` and `src/compiler/` for `grpcio_tools` build |
| `tools/distrib/python/grpcio_tools/.gitignore` | Ignore build artifacts for `grpcio_tools` |
| `tools/distrib/python/.gitignore` | Ignore `distrib_virtualenv/`, `build/`, `*.stamp` |

### Modified upstream files
| File | Change | Marker |
|---|---|---|
| `src/core/util/log.cc` | Added `#include "absl/log/vlog_is_on.h"` after `#include "absl/log/log.h"` | `@EUGO_CHANGE` |
| `setup.py:332-333` | Exclude `upb`, `address_sorting`, `utf8_range` from `CORE_C_FILES` when `BUILD_WITH_SYSTEM_GRPC` | `@EUGO_CHANGE` |
| `setup.py:526-544` | Debug logging of collection contents during `cython_extensions_and_necessity()` | `@EUGO_CHANGE` |
| `setup.py:458` | `GRPC_ENABLE_FORK_SUPPORT` macro — kept from upstream but annotated `@HELP(now)` | **UNMARKED** — needs `@EUGO_CHANGE` |
| `setup.py:478` | Changed `GRPC_DO_NOT_INSTANTIATE_POSIX_POLLER` macro (upstream changed to `GRPC_PYTHON_BUILD`) | **UNMARKED** — needs `@EUGO_CHANGE` |
| `setup.py:490` | `__STDC_FORMAT_MACROS` annotated `@HELP(now)` | **UNMARKED** — needs `@EUGO_CHANGE` |
| `setup.py:495-497` | `pymodinit` f-string style (cosmetic, upstream uses `.format()`) | **UNMARKED** — needs `@EUGO_CHANGE` |
| `setup.py:549` | `sources=` line: upstream unconditionally adds `absl/log/initialize.cc` (for bundled-abseil case); we guard it as `[] if BUILD_WITH_SYSTEM_ABSL else [...]` to avoid duplicate symbols when linking system abseil. `InitializeLog()` is called from `absl.pyx.pxi` and provided by `libabsl_log_initialize.so` in system installs. | `@EUGO_CHANGE` |
| `setup.py:79-84` | Removed duplicate `import grpc_core_dependencies` | **UNMARKED** — needs `@EUGO_CHANGE` |
| `setup.py:95-106` | `CLASSIFIERS` as tuple-of-list vs upstream's plain list (cosmetic) | **UNMARKED** — needs `@EUGO_CHANGE` |
| `src/python/grpcio/commands.py:96,124,311` | Simplified string formatting, `GRPC_STEM`/`GRPC_ROOT` path differences | **UNMARKED** — needs `@EUGO_CHANGE` |
| `src/python/grpcio/grpc/_grpcio_metadata.py` | Hardcoded version `1.75.0.dev0` (upstream uses auto-generated template) | **UNMARKED** — needs `@EUGO_CHANGE` |
| `.gitignore:8` | Added `eugo_build/` | **UNMARKED** — needs `@EUGO_CHANGE` |
| `tools/distrib/python/grpcio_tools/setup.py:301` | `PROTO_INCLUDE = "/usr/local/include/"` hardcoded path | `@TODO+:Eugo` |

### Resolved conflicts — accepted upstream

| Location | Eugo side | Upstream side | Decision | Reason |
|---|---|---|---|---|
| `setup.py` `pymodinit_type` / `pymodinit` (was line ~495) | f-string + `PY3` ternary | `.format()` style, only `PyObject*` | **Accept upstream** | Python 2 (`PY3` guard, `void` return) is dead. Upstream already dropped it. No Eugo-specific divergence. |

## Change marking convention

**All** Eugo modifications to upstream files MUST be wrapped with markers:
```python
# @EUGO_CHANGE: @begin: <brief description of why>
<changed code>
# @EUGO_CHANGE: @end
```

For C/C++:
```cpp
// @EUGO_CHANGE: @begin - <brief description>
<changed code>
// @EUGO_CHANGE: @end
```

For TOML/config files where comments use `#`:
```toml
# @EUGO_CHANGE: @begin: <brief description>
<changed lines>
# @EUGO_CHANGE: @end
```

### Rules
- Every modification to a file that exists upstream **must** have these markers.
- New files that don't exist upstream do not need markers (the entire file is an Eugo addition).
- The `@begin` marker should include a brief explanation of **why** the change was made.
- `@HELP(now)` and `@TODO+` annotations are internal notes — they should be replaced or supplemented with proper `@EUGO_CHANGE` markers.

## Upstream merge workflow

### Git configuration
```
origin    → https://github.com/eugo-inc/grpc.git  (our fork)
upstream  → https://github.com/grpc/grpc.git      (official gRPC)
```

### Merge procedure
```bash
git fetch upstream
git checkout master
git checkout -b <username>/chore/merge-upstream
git merge upstream/master
# Resolve conflicts — always preserve @EUGO_CHANGE blocks
# Test build: cmake for native, meson for python
git push origin <username>/chore/merge-upstream
# Create PR to master
```

### Conflict resolution rules
1. **Files in the "Modified upstream files" table above**: Keep the Eugo side (`HEAD`/ours) for code within `@EUGO_CHANGE` markers. Accept upstream changes for everything else.
2. **`pyproject.toml` (root)**: Always keep the Eugo (Meson-based) version. Discard the upstream setuptools version entirely.
3. **`tools/distrib/python/grpcio_tools/pyproject.toml`**: Always keep the Eugo version.
4. **`src/python/grpcio/grpc/_grpcio_metadata.py`**: Update the version number to match upstream's new version, but keep our simplified format (no copyright header needed — it's auto-generated).
5. **`setup.py`**: This file has the most conflicts. For sections within `@EUGO_CHANGE` markers, keep ours. For other sections, prefer upstream. Pay special attention to `DEFINE_MACROS` — upstream may add new macros we should evaluate.
6. **`src/core/util/log.cc`**: Check if upstream has added the `vlog_is_on.h` include. If so, our change can be dropped. If not, keep it.
7. **New upstream files**: Accept them. They don't conflict with our changes.
8. **`meson.build` files**: These are Eugo-only so they won't conflict, but may need updates if upstream changes source file locations or dependencies.

### After merge — validation checklist
- [ ] All `@EUGO_CHANGE` markers are intact
- [ ] No leftover `<<<<<<< HEAD` / `=======` / `>>>>>>>` conflict markers
- [ ] `meson.build` (root) still references correct source paths
- [ ] Native gRPC builds: `cmake -B build && cmake --build build`
- [ ] Python grpcio builds: `pip install .` (Meson)
- [ ] `protoc --grpc_python_out` works with installed `grpc_python_plugin`

## Keeping meson.build in sync with upstream

This is the most maintenance-intensive part of the fork. Every upstream merge can introduce new macros, new source files, or changed experiment flags that affect `cygrpc.so`. Because our Meson build compiles **only** `cygrpc.pyx` and links against system-installed `libgrpc`, most upstream `setup.py` macro changes are irrelevant — but you must consciously verify each one.

### Decision framework for new macros in setup.py

When upstream adds a new macro to `DEFINE_MACROS` in `setup.py`, ask:

1. **Is it only checked in `src/core/` C++ files?** → We don't compile `src/core/`. **Skip it.**
2. **Is it checked in any Cython (`.pyx`/`.pxi`/`.pxd`) file?** → Check `src/python/grpcio/grpc/_cython/`. If yes, **add it to `meson.build`**.
3. **Is it checked in any public header under `include/`?** → Those headers are included by our Cython build. If yes, **add it to `meson.build`**.
4. **Is it an ABI-affecting macro that changes struct layouts or vtable shapes in installed `libgrpc` headers?** → Must match what `libgrpc` was compiled with. Check `cmake/` and `CMakeLists.txt` to see if CMake sets it. If CMake doesn't set it, upstream's installed `libgrpc` won't have it either — **skip it**.

### How to check where a macro is used
```bash
# Find all uses of a macro in the gRPC source tree
grep -rn 'MACRO_NAME' --include='*.h' --include='*.cc' --include='*.pyx' --include='*.pxi' --include='*.pxd' .

# Check if CMake sets it (meaning libgrpc was compiled with it)
grep -rn 'MACRO_NAME' CMakeLists.txt cmake/
```

### Known macro decisions for meson.build

| Macro | Upstream sets in setup.py | In meson.build | Reason |
|---|---|---|---|
| `PyMODINIT_FUNC` | Yes | **Yes — ACTIVE** | Needed to mark Cython init function with correct visibility. Only place it's checked is in Cython-generated C++. |
| `GRPC_ENABLE_FORK_SUPPORT` | Yes | **No** | Only affects `src/core/` (fork support logic). Not in any Cython code. CMake doesn't set it either. |
| `GRPC_DO_NOT_INSTANTIATE_POSIX_POLLER` | Removed upstream | **No** | Was removed and replaced by `GRPC_PYTHON_BUILD` (see below). |
| `GRPC_PYTHON_BUILD` | Yes (new in ~v1.79) | **No** | Guards behavior in `posix_engine.cc`, `dns_resolver_plugin.cc`, `shim.cc`, `backup_poller.cc` — all `src/core/` files we don't compile. System `libgrpc` was built without it (CMake doesn't set it). |
| `GRPC_ENABLE_FORK_SUPPORT_DEFAULT` | Yes (new in ~v1.79) | **No** | Affects `src/core/config_vars.cc`. We don't compile `src/core/`. |
| `GRPC_POSIX_FORK_ALLOW_PTHREAD_ATFORK` | Yes | **No** | Only affects `src/core/` (`posix_engine.cc`, `fork_posix.cc`) and only when `GRPC_ENABLE_FORK_SUPPORT` is also set. Confirmed not used in any Cython file. |
| `HAVE_CONFIG_H` | Yes | **No** | Legacy `./configure` artifact. Not used in CMake or Meson builds of `native/` or `python/`. |
| `__STDC_FORMAT_MACROS` | Yes | **No** | Pre-C++11 no-op on modern compilers. gRPC enables it unconditionally in `port_platform.h` anyway. |
| `GRPC_XDS_USER_AGENT_NAME_SUFFIX` | Yes | **No** | Cosmetic xDS client disambiguation. Not used in Cython code. |
| `GRPC_XDS_USER_AGENT_VERSION_SUFFIX` | Yes | **No** | Same as above. |
| `OPENSSL_NO_ASM` | Yes | **No** | Only relevant when BoringSSL/OpenSSL is bundled into `libgrpc`. We use system OpenSSL. |

### Known compiler flag decisions for meson.build

| Flag | setup.py sets (linux/darwin) | In meson.build | Reason |
|---|---|---|---|
| `-fno-exceptions` | Yes | **Yes — ACTIVE** | Python extensions don't use C++ exceptions. gRPC's public API uses `absl::Status`. Disabling removes exception table overhead and matches upstream's intent. |
| `-fno-wrapv` | Yes | **Yes — ACTIVE** | Disables signed-integer-overflow wrapping (UB in C/C++). Enables optimizer transformations; matches setup.py for ABI consistency. |
| `-fvisibility=hidden` | Yes | **Implicit — handled by Meson** | Meson's `extension_module()` automatically sets `gnu_symbol_visibility='hidden'`. Our explicit `-DPyMODINIT_FUNC=...` override ensures the init function is still exported with default visibility. No manual flag needed. |
| `-static-libgcc` | Yes (linux) | **No** | Useful for portable wheel deployment. Not critical for our non-wheel server deployments. Re-evaluate if we start distributing wheels. |
| `-lpthread` | Yes (linux/darwin) | **Implicit via CMake dep** | gRPC's CMake config propagates `-lpthread` transitively. No explicit flag needed unless linking fails. |

### Runtime Python dependencies

`setup.py` declares `INSTALL_REQUIRES = ("typing-extensions~=4.12",)`. This must be mirrored in `pyproject.toml` under `[project] dependencies`. The pure-Python `grpc` package uses `typing_extensions.override` (in `_server.py`) and `typing_extensions.Self` (in `aio/_metadata.py`). If upstream adds new runtime Python dependencies to `INSTALL_REQUIRES`, add them to `pyproject.toml` as well.

### New source files added by upstream to grpc_core_dependencies

When upstream adds new `.cc` files to `grpc_core_dependencies.py` (which feeds `CORE_C_FILES` in `setup.py`), those files are compiled into `cygrpc.so` in the upstream bundled build. **In our Meson build they are compiled into system `libgrpc` instead** — so no action needed in `meson.build` unless the new file also adds a public header or new Cython-visible interface.

However, if upstream adds a new file to the Cython sources (`CYTHON_HELPER_C_FILES` or adds a new `.pyx`), that **does** require updating `meson.build`.

### New Cython/Python extension files

If upstream adds a new `.pyx` file to `CYTHON_EXTENSION_MODULE_NAMES` in `setup.py`, a corresponding entry must be added to `meson.build`'s `py.extension_module()` call. Check `setup.py` diff carefully during each merge.

### experiment flags in libgrpc

Upstream uses gRPC experiment flags (e.g. `IsEventEnginePollerForPythonEnabled()`) gated by `GRPC_PYTHON_BUILD`. These experiments are compiled into `libgrpc` at CMake build time. Since our `libgrpc` is built without `GRPC_PYTHON_BUILD`, these experiments default to their non-Python paths — which is correct for our use case (we want the same high-performance paths as the native library).

## Code style

### Python
- Python >= 3.12, use f-strings freely
- Follow upstream gRPC Python style (they use Ruff as of recent versions)
- `pyproject.toml` uses `meson-python` as build backend — NOT setuptools

### C/C++
- Follow upstream gRPC style (see `GEMINI.md` at repo root for details)
- C17 (`gnu17`), C++23 (`gnu++23`)
- Prefer `absl` types, then `std` types
- Include `absl` headers before gRPC headers

### Meson build files
- Use section markers: `# === @begin: Section Name ===` / `# === @end: Section Name ===`
- Include detailed comments explaining **why** macros/flags are included or excluded (see existing `meson.build` for reference)
- Every commented-out macro in `meson.build` must have an explanation of why it's excluded
- When referencing upstream decisions, include links to relevant CMake files, PRs, or issues

## Boundaries

### Always do
- Wrap ALL modifications to upstream files with `@EUGO_CHANGE` markers
- Test both native (CMake) and Python (Meson) builds after changes
- Update this file when adding new Eugo modifications
- Keep `meson.build` comments explaining macro decisions (they're invaluable for future merges)

### Ask first
- Adding new dependencies to `meson.build`
- Modifying `src/core/` C++ files (high merge-conflict risk)
- Changing the protoc/grpc_python_plugin invocation interface
- Removing any `@EUGO_CHANGE` block

### Never do
- Modify files under `third_party/` (we use system packages)
- Remove `@EUGO_CHANGE` markers without replacing them
- Add Eugo-specific CI/CD workflows to `.github/workflows/` (upstream has extensive CI that would break)
- Build or ship `grpcio_tools` as a Python package — use native `protoc` + `grpc_python_plugin`
- Commit with upstream author identities
