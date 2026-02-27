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

### How `grpcio_tools` was eliminated — no source changes required

Eliminating `grpcio_tools` required **zero modifications** to upstream source code. It's purely an architectural choice: we rely on standalone binaries that upstream's CMake already builds and installs, instead of bundling those same binaries into a Python extension.

#### What upstream `grpcio_tools` does

Upstream `grpcio_tools` compiles the entire protobuf compiler + gRPC Python code generator into a single Python extension (`_protoc_compiler.so`), callable from Python:

```
Python: grpc_tools.protoc.main([...])
  → _protoc_compiler.pyx: run_main()           (Cython wrapper)
    → main.cc: protoc_main()                    (in-process protoc CLI)
      → python_generator.cc                     (generates *_pb2_grpc.py)
```

Key upstream files:
| File | Purpose |
|---|---|
| `tools/distrib/python/grpcio_tools/grpc_tools/_protoc_compiler.pyx` | Cython extension wrapping `protoc_main()` as `run_main()` |
| `tools/distrib/python/grpcio_tools/grpc_tools/main.cc` | Creates `CommandLineInterface`, registers `--python_out`, `--pyi_out`, `--grpc_python_out` generators, calls `cli.Run()` |
| `tools/distrib/python/grpcio_tools/grpc_tools/protoc.py` | Python API: `main()` encodes args to bytes, calls `_protoc_compiler.run_main()` |
| `src/compiler/python_generator.cc` | The actual code generator — produces `*_pb2_grpc.py` with stubs, servicers, etc. |
| `src/compiler/python_plugin.cc` | Standalone `main()` wrapping `PythonGrpcGenerator` in the protobuf plugin protocol |

The setup.py extension links **all of protoc/libprotobuf** C++ sources (hundreds of files from `CC_FILES`) into `_protoc_compiler.so`.

#### What Eugo does instead

The **exact same `python_generator.cc`** code is used both ways. Upstream wraps it in `main.cc` → `_protoc_compiler.so` (in-process). We use the standalone `grpc_python_plugin` binary that CMake already builds:

```
Shell: protoc --plugin=protoc-gen-grpc_python=$(which grpc_python_plugin) ...
  → protoc (standalone binary, from protobuf)
    → grpc_python_plugin (standalone binary, from gRPC CMake)
      → python_generator.cc    (same code as grpcio_tools uses)
```

#### CMake targets that make this work

All from upstream `CMakeLists.txt` — no Eugo modifications:

**`grpc_plugin_support`** (static library, `CMakeLists.txt:6445`):
```cmake
add_library(grpc_plugin_support
  src/compiler/cpp_generator.cc
  src/compiler/csharp_generator.cc
  src/compiler/node_generator.cc
  src/compiler/objective_c_generator.cc
  src/compiler/php_generator.cc
  src/compiler/proto_parser_helper.cc
  src/compiler/python_generator.cc      # ← the Python codegen logic
  src/compiler/ruby_generator.cc
)
```

**`grpc_python_plugin`** (executable, `CMakeLists.txt:19606`):
```cmake
add_executable(grpc_python_plugin
  src/compiler/python_plugin.cc         # ← minimal main() calling PythonGrpcGenerator
)
target_link_libraries(grpc_python_plugin
  grpc_plugin_support                   # ← gets python_generator.cc
)
```

**Install rule** (`CMakeLists.txt:19635`):
```cmake
install(TARGETS grpc_python_plugin EXPORT gRPCPluginTargets
  RUNTIME DESTINATION ${gRPC_INSTALL_BINDIR}   # → /usr/local/bin/grpc_python_plugin
)
```

`protoc` itself comes from the protobuf dependency — when gRPC's CMake builds or finds protobuf, `protoc` ends up in `/usr/local/bin/` as well.

#### Why no source changes were needed

1. **Same generator code**: Both paths use `python_generator.cc` — identical output
2. **CMake already installs the binary**: `grpc_python_plugin` is an upstream build target, enabled by default (`gRPC_BUILD_GRPC_PYTHON_PLUGIN=ON`)
3. **protoc's plugin protocol**: `protoc` natively supports `--plugin=protoc-gen-X=<path>` for external code generators — this is the standard protobuf extension mechanism
4. **No Python-specific behavior lost**: The only extra thing `grpcio_tools` provides over native protoc is (a) bundled `.proto` files (we get them from `/usr/local/include/` via CMake install) and (b) a dynamic proto finder for import-time compilation (not needed in our build pipeline)

#### Eugo-created reference files (experimental, not used in production)

| File | Purpose |
|---|---|
| `tools/distrib/python/grpcio_tools/meson.build` | Experimental Meson build that builds `_protoc_compiler.so` linking against system protobuf/gRPC — exists as a reference, **not the intended approach** |
| `tools/distrib/python/grpcio_tools/pyproject.toml` | `meson-python` config for the experimental build |
| `tools/distrib/python/grpcio_tools/eugo_copy_sources.py` | Helper to copy `include/` and `src/compiler/` (unused — Meson build references sources via relative paths) |
| `tools/distrib/python/grpcio_tools/.gitignore` | Ignores build artifacts |

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
- [ ] **Inline comments in `meson.build` are up to date** — the comments reference specific `setup.py` line numbers (e.g. `setup.py:280`, `setup.py:346`, `setup.py:472-473`) and explain *why* each value was chosen. After merging upstream, `setup.py` line numbers shift and the referenced logic may move or change. Scan `meson.build` for every `setup.py:NNN` citation and verify it still points at the correct line and that the described behavior matches the current upstream code. Update citations and prose as needed.
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
| `-fno-exceptions` | No | **No — INACTIVE** | Dangerous |
| `-fno-wrapv` | No | **No — INACTIVE** | Dangerous |
| `-fvisibility=hidden` | Yes | **Implicit — handled by Meson** | Meson's `extension_module()` automatically sets `gnu_symbol_visibility='hidden'`. Our explicit `-DPyMODINIT_FUNC=...` override ensures the init function is still exported with default visibility. No manual flag needed. |
| `-static-libgcc` | Yes (linux) | **No** | Useful for portable wheel deployment. Not critical for our non-wheel server deployments. Re-evaluate if we start distributing wheels. |
| `-lpthread` | Yes (linux/darwin) | **Implicit via CMake dep** | gRPC's CMake config propagates `-lpthread` transitively. No explicit flag needed unless linking fails. |

### Known extra link dependencies for meson.build

Some symbols used by `cygrpc.pyx` (Cython code) are NOT provided by `libgrpc` or `libgpr`. These must be added as explicit dependencies in `meson.build`'s `extension_module()`.

| Library | CMake target | Symbol | Called from | Why not in libgrpc | Meson dep var |
|---|---|---|---|---|---|
| `libabsl_log_initialize.so` | `absl::log_initialize` | `absl::InitializeLog()` | `_cygrpc/absl.pyx.pxi:23` | `libgrpc` never calls `InitializeLog()` itself (CMakeLists.txt:3205-3232 — not in `grpc`'s `target_link_libraries`) | `absl_log_initialize` |

When adding new dependencies, follow this pattern:
1. Check if the undefined symbol is called from Cython code (`.pyx`/`.pxi`) or from `src/core/` C++
2. If from Cython: add to `meson.build` dependencies — it won't come from `libgrpc`
3. If from `src/core/`: should already be in `libgrpc` — investigate why it's missing

#### Deep-dive: `absl::InitializeLog()` and `setup.py` vendoring

Upstream's `setup.py` compiles `third_party/abseil-cpp/absl/log/initialize.cc` directly into `cygrpc.so` as a vendored source file — it does NOT link against `libabsl_log_initialize.so`. This is the line:

```python
sources=(
    [module_file] + ... + ["third_party/abseil-cpp/absl/log/initialize.cc"]
)
```

This means `absl::InitializeLog()` appears as a `T` (text/defined) symbol in the upstream wheel's `cygrpc.so`. Upstream PR [grpc#39779](https://github.com/grpc/grpc/pull/39779) is the change that added this — it introduced the Cython call to `InitializeLog()` to suppress the "WARNING: All log messages before absl::InitializeLog() is called are written to STDERR" message. The same PR added the `GRPC_PYTHON_DISABLE_ABSL_INIT_LOG` env var opt-out.

Our Eugo `setup.py` change (lines 521-526, `@EUGO_CHANGE`) guards this:
```python
+ ([] if BUILD_WITH_SYSTEM_ABSL else ["third_party/abseil-cpp/absl/log/initialize.cc"])
```
When `BUILD_WITH_SYSTEM_ABSL=true`, the vendored source is skipped to avoid duplicate symbol errors, and `libabsl_log_initialize.so` provides the symbol at runtime instead.

Our Meson build does the equivalent: instead of compiling the vendored source, we link against the system `libabsl_log_initialize.so` via the `absl::log_initialize` CMake target.

**Note on `libgrpc`'s abseil dependencies:** `libgrpc.so` does link against *some* abseil libraries (`libabsl_log_internal_*`, `libabsl_strings`, `libabsl_synchronization`, etc.) — but NOT `libabsl_log_initialize.so` (verify with `readelf -d libgrpc.so | grep NEEDED`). `InitializeLog()` is an application-level function meant to be called once by `main()`. `libgrpc` is a library — it uses abseil's logging internals but never initializes the log system itself.

#### Unity/monolithic abseil builds

The dependency is declared `required: false` in `meson.build` to support unity abseil builds (built with `-DABSL_BUILD_MONOLITHIC_SHARED_LIBS=ON`). With a monolithic `libabseil_dll.so`, all abseil symbols (including `InitializeLog()`) are in a single shared library. In that configuration:
- `absl::log_initialize` **still exists as a CMake target** in `abslTargets.cmake` — but it's an `INTERFACE IMPORTED` library whose only link dependency is `absl::abseil_dll` (the monolithic `.so`). It contributes no additional library of its own.
- The symbol comes transitively through `libgrpc.so` → `libabseil_dll.so` — no explicit dep needed.
- Our `dependency('absl', modules: ['absl::log_initialize'], required: false)` will `found()` as true (the target exists), but linking it is harmless — it just adds another reference to `abseil_dll.so` which is already linked transitively via `libgrpc.so`.

**TODO+**: After switching to the abseil unity build in production, verify whether the explicit `absl_log_initialize` dependency in `meson.build` is still needed. With `libabseil_dll.so` providing all symbols transitively through `libgrpc.so`, it may be redundant. Check with `readelf --needed-libs cygrpc.so` — if `libabseil_dll.so` appears transitively (via `libgrpc.so`), the explicit dep can potentially be removed. However, keeping it is harmless and self-documenting.

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

## Validating the Eugo wheel against upstream

Comparing our Meson-built wheel against the upstream `grpcio` wheel from PyPI is the primary correctness gate. If the two wheels contain the same pure-Python files, export the same native symbols, and pass the same runtime smoke tests, then our build is correct.

All validation steps are documented inline below — run them manually from the repo root.

### Obtaining the two wheels

```bash
# Shared directories — all under /tmp/eugo/ to keep things organised
EUGO_INSTALLED_DIR=/tmp/eugo/grpcio-installed
EUGO_EUGO_WHEEL_DIR=/tmp/eugo/grpcio-eugo-wheel
EUGO_UPSTREAM_WHEEL_DIR=/tmp/eugo/grpcio-upstream-wheel
EUGO_UPSTREAM_DIR=/tmp/eugo/grpcio-upstream
mkdir -p "${EUGO_INSTALLED_DIR}" "${EUGO_EUGO_WHEEL_DIR}" "${EUGO_UPSTREAM_WHEEL_DIR}" "${EUGO_UPSTREAM_DIR}"

# 1. Build the Eugo wheel and install it to a known directory (requires system-installed libgrpc)
pip3 install . ${EUGO_PIP_COMPILABLE_PACKAGE_OPTIONS} ${EUGO_MESONPY_COMMON_OPTIONS}
pip3 install "${EUGO_EUGO_WHEEL_DIR}"/grpcio-*.whl --target "${EUGO_INSTALLED_DIR}" --force-reinstall

# 2. Download the matching upstream wheel from PyPI
#    Match the version exactly (from grpc_version.py) and target the same platform tag.
#    Auto-detect architecture: aarch64 → manylinux_2_17_aarch64, x86_64 → manylinux_2_17_x86_64.
GRPC_VERSION=$(python3 -c "exec(open('src/python/grpcio/grpc_version.py').read()); print(VERSION)")
ARCH=$(uname -m)
if [[ "${ARCH}" == "aarch64" ]]; then
    PLATFORM_TAG="manylinux_2_17_aarch64"
else
    PLATFORM_TAG="manylinux_2_17_x86_64"
fi
pip3 download grpcio==${GRPC_VERSION} \
    --no-deps \
    --only-binary=:all: \
    --platform "${PLATFORM_TAG}" \
    --python-version 3.12 \
    -d "${EUGO_UPSTREAM_WHEEL_DIR}/"
```

### Step 1: Unpack and diff file listings

```bash
unzip -o "${EUGO_EUGO_WHEEL_DIR}"/grpcio-*.whl -d "${EUGO_EUGO_WHEEL_DIR}"
unzip -o "${EUGO_UPSTREAM_WHEEL_DIR}"/grpcio-*.whl -d "${EUGO_UPSTREAM_DIR}"

# cd into grpc/ subdirectory so paths are comparable (both start with ./aio/... etc.)
# Exclude __pycache__ / .pyc — wheels don't ship them; they appear only in --target installs.
(cd "${EUGO_EUGO_WHEEL_DIR}/grpc" && find . -type f | grep -v '__pycache__' | sort) > /tmp/eugo/eugo_files.txt
(cd "${EUGO_UPSTREAM_DIR}/grpc"   && find . -type f | grep -v '__pycache__' | sort) > /tmp/eugo/upstream_files.txt

diff /tmp/eugo/eugo_files.txt /tmp/eugo/upstream_files.txt
```

**Expected result**: The file listings should be identical. Every `.py` file, `roots.pem`, `__init__.py`, and `cygrpc` shared object present in the upstream wheel must also be present in ours. Any missing file indicates a gap in `meson.build`'s `install_subdir` / `install_data` rules.

### Step 2: Diff pure-Python file contents

```bash
# Compare every .py file byte-for-byte (excluding _grpcio_metadata.py which we hardcode)
diff -rq "${EUGO_EUGO_WHEEL_DIR}/grpc/" "${EUGO_UPSTREAM_DIR}/grpc/" \
    --exclude='*.so' \
    --exclude='*.dylib' \
    --exclude='_grpcio_metadata.py' \
    --exclude='__pycache__'
```

**Expected result**: No differences. Our Meson build installs pure-Python files directly from the source tree, which should be identical to upstream. If upstream has added, removed, or modified `.py` files since our fork point, those changes must be merged first.

`_grpcio_metadata.py` is excluded because upstream auto-generates it while we hardcode the version string — but the version value itself must match.

### Step 3: Compare native extension exported symbols

The `cygrpc` shared object will differ at the binary level (different compiler flags, linking strategy), but the **exported symbol set** must be equivalent. The only required export is the CPython module init function.

```bash
# Use -DC for demangled output
nm -DC "${EUGO_EUGO_WHEEL_DIR}"/grpc/_cython/cygrpc*.so  | grep ' T ' | awk '{print $3}' | sort > /tmp/eugo/eugo_symbols.txt
nm -DC "${EUGO_UPSTREAM_DIR}"/grpc/_cython/cygrpc*.so     | grep ' T ' | awk '{print $3}' | sort > /tmp/eugo/upstream_symbols.txt

diff /tmp/eugo/eugo_symbols.txt /tmp/eugo/upstream_symbols.txt
```

**Expected result**: The symbol sets will differ dramatically — this is expected and correct.

**Eugo** exports only `PyInit_cygrpc` (and possibly `_fini`/`_init`). Everything else appears as `U` (undefined — resolved from shared libs at runtime). This is the correct behaviour for dynamic linking against system `libgrpc`.

**Upstream** exports 300+ symbols as `T` (text/defined): all of c-ares (`ares_*`), `std::` exception constructors, abseil internals, and more. This is because upstream's `setup.py` statically bundles the entire gRPC C core, c-ares, abseil, BoringSSL, and parts of libstdc++ into `cygrpc.so` — making it a self-contained binary with no external C++ dependencies.

The key check is that **`PyInit_cygrpc` is present as a `T` symbol** in our wheel. Its absence would mean the module init function isn't exported (broken `-fvisibility` or missing `PyMODINIT_FUNC` override). Everything else being `U` is expected.

If upstream adds *new* `T` symbols beyond the usual bundled set, investigate whether `setup.py` added new extension modules or changed visibility flags.

### Step 4: Compare dynamic library dependencies

```bash
# Check what shared libraries each cygrpc links against
readelf -d "${EUGO_EUGO_WHEEL_DIR}"/grpc/_cython/cygrpc*.so  | grep NEEDED | awk '{print $5}' | sort > /tmp/eugo/eugo_needed.txt
readelf -d "${EUGO_UPSTREAM_DIR}"/grpc/_cython/cygrpc*.so     | grep NEEDED | awk '{print $5}' | sort > /tmp/eugo/upstream_needed.txt

diff /tmp/eugo/eugo_needed.txt /tmp/eugo/upstream_needed.txt
```

**Expected result**: These will intentionally differ. Upstream bundles all C/C++ dependencies statically into `cygrpc.so` (BoringSSL, abseil, c-ares, etc.), so it typically has very few `NEEDED` entries (just `libc`, `libpthread`, `libm`, etc.). Our wheel dynamically links against system `libgrpc.so`, `libgpr.so`, `libabsl_*.so`, etc. — this is by design. The key check is that our wheel **does** list `libgrpc.so` and `libgpr.so` as `NEEDED` dependencies.

### Step 5: Compare package metadata

```bash
diff "${EUGO_EUGO_WHEEL_DIR}"/grpcio-*.dist-info/METADATA \
     "${EUGO_UPSTREAM_DIR}"/grpcio-*.dist-info/METADATA
```

**Key fields to verify**:
- `Name`: must be `grpcio` in both
- `Version`: must match
- `Requires-Dist`: our `typing-extensions~=4.12` must match upstream's runtime dependencies. If upstream adds new dependencies, update `pyproject.toml`.

### Step 6: Runtime smoke tests

Run smoke tests against the files already installed by the setup step above — no separate venv needed.

```bash
# Point Python at the installed files from the comparison setup step
export PYTHONPATH="${EUGO_INSTALLED_DIR}"
```

#### Test 6a: Import and basic API surface

Verifies the native extension loads, the version is correct, and fundamental gRPC objects can be created.

```bash
python3 -c "
import grpc
assert grpc.__version__, 'no version'
from grpc._cython import cygrpc
channel = grpc.insecure_channel('localhost:0')
channel.close()
creds = grpc.ssl_channel_credentials()
server = grpc.aio.server()
print(f'grpc version: {grpc.__version__}')
print('Test 6a passed.')
"
```

#### Test 6b: Generic (non-proto) unary RPC roundtrip

End-to-end test that creates a server, sends a request, and validates the response — all without protobuf. This exercises `grpc.server()`, `grpc.method_handlers_generic_handler()`, `grpc.unary_unary_rpc_method_handler()`, `grpc.insecure_channel()`, and `channel.unary_unary()`.

```bash
python3 -c "
import json
from concurrent import futures
import grpc

def say_method(request_bytes, context):
    request = json.loads(request_bytes.decode('utf-8'))
    return json.dumps({'message': f\"Echo: {request.get('message', '')}\"}).encode('utf-8')

server = grpc.server(futures.ThreadPoolExecutor(max_workers=2))
generic_handler = grpc.method_handlers_generic_handler(
    'echo.Echo',
    {'Say': grpc.unary_unary_rpc_method_handler(
        say_method,
        request_deserializer=lambda x: x,
        response_serializer=lambda x: x,
    )},
)
server.add_generic_rpc_handlers((generic_handler,))
port = server.add_insecure_port('[::]:0')
server.start()

with grpc.insecure_channel(f'localhost:{port}') as channel:
    stub = channel.unary_unary(
        '/echo.Echo/Say',
        request_serializer=lambda d: json.dumps(d).encode('utf-8'),
        response_deserializer=lambda b: json.loads(b.decode('utf-8')),
    )
    response = stub({'message': 'Hello gRPC'})
    assert response['message'] == 'Echo: Hello gRPC', f'Unexpected: {response}'

server.stop(0)
print('Test 6b passed.')
"
```

#### Test 6c: Protoc + grpc_python_plugin code generation

Verifies that the native `protoc` binary and `grpc_python_plugin` (installed by CMake) can compile a `.proto` file and produce working Python stubs. This is the Eugo replacement for `grpcio_tools`.

**Prerequisites**: `protoc` and `grpc_python_plugin` must be on `$PATH` (installed by `cmake --install build`).

```bash
PROTO_TEST_DIR=$(mktemp -d)

cat > "${PROTO_TEST_DIR}/test.proto" << 'EOF'
syntax = "proto3";
package test;
message TestMessage { string name = 1; int32 id = 2; }
service TestService { rpc SayHello(TestMessage) returns (TestMessage); }
EOF

protoc \
  --plugin=protoc-gen-grpc_python="$(which grpc_python_plugin)" \
  --proto_path="${PROTO_TEST_DIR}" \
  --python_out="${PROTO_TEST_DIR}" \
  --grpc_python_out="${PROTO_TEST_DIR}" \
  --grpc_python_opt="grpc_2_0" \
  "${PROTO_TEST_DIR}/test.proto"

python3 -c "
import sys
sys.path.insert(0, '${PROTO_TEST_DIR}')
import test_pb2, test_pb2_grpc
msg = test_pb2.TestMessage(name='hello', id=42)
assert msg.name == 'hello' and msg.id == 42
assert hasattr(test_pb2_grpc, 'TestServiceStub')
assert hasattr(test_pb2_grpc, 'add_TestServiceServicer_to_server')
print('Test 6c passed.')
"

rm -rf "${PROTO_TEST_DIR}"
```

#### Test 6d: Full client-server roundtrip with generated stubs

End-to-end test using proto-generated stubs. Combines code generation (Test 6c) with a real RPC call.

```bash
PROTO_TEST_DIR=$(mktemp -d)

cat > "${PROTO_TEST_DIR}/helloworld.proto" << 'EOF'
syntax = "proto3";
package helloworld;
service Greeter { rpc SayHello (HelloRequest) returns (HelloReply) {} }
message HelloRequest { string name = 1; }
message HelloReply { string message = 1; }
EOF

protoc \
  --plugin=protoc-gen-grpc_python="$(which grpc_python_plugin)" \
  --proto_path="${PROTO_TEST_DIR}" \
  --python_out="${PROTO_TEST_DIR}" \
  --grpc_python_out="${PROTO_TEST_DIR}" \
  --grpc_python_opt="grpc_2_0" \
  "${PROTO_TEST_DIR}/helloworld.proto"

python3 -c "
import sys
sys.path.insert(0, '${PROTO_TEST_DIR}')
from concurrent import futures
import grpc, helloworld_pb2, helloworld_pb2_grpc

class Greeter(helloworld_pb2_grpc.GreeterServicer):
    def SayHello(self, request, context):
        return helloworld_pb2.HelloReply(message=f'Hello, {request.name}!')

server = grpc.server(futures.ThreadPoolExecutor(max_workers=2))
helloworld_pb2_grpc.add_GreeterServicer_to_server(Greeter(), server)
port = server.add_insecure_port('[::]:0')
server.start()

with grpc.insecure_channel(f'localhost:{port}') as channel:
    stub = helloworld_pb2_grpc.GreeterStub(channel)
    response = stub.SayHello(helloworld_pb2.HelloRequest(name='World'))
    assert response.message == 'Hello, World!', f'Unexpected: {response.message}'

server.stop(0)
print('Test 6d passed.')
"

rm -rf "${PROTO_TEST_DIR}"
```

#### Test 6e: AsyncIO client-server roundtrip

Same as Test 6d but using `grpc.aio` — validates that the async API works end-to-end.

```bash
PROTO_TEST_DIR=$(mktemp -d)

cat > "${PROTO_TEST_DIR}/helloworld.proto" << 'EOF'
syntax = "proto3";
package helloworld;
service Greeter { rpc SayHello (HelloRequest) returns (HelloReply) {} }
message HelloRequest { string name = 1; }
message HelloReply { string message = 1; }
EOF

protoc \
  --plugin=protoc-gen-grpc_python="$(which grpc_python_plugin)" \
  --proto_path="${PROTO_TEST_DIR}" \
  --python_out="${PROTO_TEST_DIR}" \
  --grpc_python_out="${PROTO_TEST_DIR}" \
  --grpc_python_opt="grpc_2_0" \
  "${PROTO_TEST_DIR}/helloworld.proto"

python3 -c "
import sys, asyncio
sys.path.insert(0, '${PROTO_TEST_DIR}')
import grpc, helloworld_pb2, helloworld_pb2_grpc

class Greeter(helloworld_pb2_grpc.GreeterServicer):
    async def SayHello(self, request, context):
        return helloworld_pb2.HelloReply(message=f'Hello, {request.name}!')

async def main():
    server = grpc.aio.server()
    helloworld_pb2_grpc.add_GreeterServicer_to_server(Greeter(), server)
    port = server.add_insecure_port('[::]:0')
    await server.start()
    async with grpc.aio.insecure_channel(f'localhost:{port}') as channel:
        stub = helloworld_pb2_grpc.GreeterStub(channel)
        response = await stub.SayHello(helloworld_pb2.HelloRequest(name='Async'))
        assert response.message == 'Hello, Async!', f'Unexpected: {response.message}'
    await server.stop(0)
    print('Test 6e passed.')

asyncio.run(main())
"

rm -rf "${PROTO_TEST_DIR}"
```

#### Test summary

```bash
unset PYTHONPATH
```

All five tests must pass:
| Test | What it validates |
|---|---|
| 6a | Native extension loads, version correct, `roots.pem` present, `grpc.aio` available |
| 6b | Generic (non-proto) unary RPC roundtrip — exercises core gRPC Python API |
| 6c | `protoc` + `grpc_python_plugin` code generation produces valid Python modules |
| 6d | Synchronous client-server roundtrip with proto-generated stubs |
| 6e | AsyncIO client-server roundtrip with proto-generated stubs |

### Protoc invocation notes for downstream projects

Our fork eliminates `grpcio_tools`. Downstream projects that previously used `grpc_tools.protoc.main([...])` or `python -m grpc_tools.protoc` must switch to native `protoc`. Key differences:

1. **Plugin must be explicit**: `--plugin=protoc-gen-grpc_python="$(which grpc_python_plugin)"`
2. **No bundled `.proto` files**: `grpc_tools` ships `google/protobuf/*.proto` inside the package. With native protoc, these are at `/usr/local/include/` (or wherever CMake installed them). Add `--proto_path=/usr/local/include` if your protos import `google/protobuf/empty.proto` etc.
3. **`grpc_2_0` opt for modern codegen**: `--grpc_python_opt="grpc_2_0"` produces code that uses `grpc.method_handlers_generic_handler()` instead of the deprecated `grpc.method_service_handler()`.
4. **Import path fixups**: Generated code uses bare module imports (e.g., `import foo_pb2`). If your project needs relative imports, post-process with `sed`:
   ```bash
   sed -i -E 's/from src.ray.protobuf/from ./' "${OUTPUT}"
   ```

#### Downstream Meson integration pattern

For projects using Meson that need to compile `.proto` files at build time:

```meson
# Find grpc_python_plugin (must be on PATH from CMake install)
py = import('python').find_installation(pure: false)

protoc_script = files('protoc_py.sh')
protoc_with_args = [protoc_script, '@INPUT@', '@SOURCE_ROOT@', '@BUILD_ROOT@', '@OUTPUT0@', '@OUTPUT1@']
proto_py_kwargs = {
    'command': protoc_with_args,
    'output': ['@BASENAME@_pb2.py', '@BASENAME@_pb2_grpc.py'],
    'install': true,
    'install_dir': py.get_install_dir() / 'mypackage/generated/',
}

my_proto_py = custom_target(
    input: 'my_service.proto',
    kwargs: proto_py_kwargs,
)
```

Where `protoc_py.sh` wraps the native protoc invocation:
```bash
#!/usr/bin/env bash
set -euo pipefail
INPUT="${1}"
SOURCE_ROOT="${2}"
BUILD_ROOT="${3}"

protoc \
  --plugin=protoc-gen-grpc_python="$(which grpc_python_plugin)" \
  --proto_path="${SOURCE_ROOT}" \
  --python_out="${BUILD_ROOT}" \
  --grpc_python_out="${BUILD_ROOT}" \
  --grpc_python_opt="grpc_2_0" \
  "${INPUT}"
```

### What to do when differences are found

| Difference | Likely cause | Action |
|---|---|---|
| Missing `.py` files in Eugo wheel | Upstream added new Python modules | Update `install_subdir` / `install_data` in `meson.build`, or merge upstream changes |
| Extra files in Eugo wheel | Stale files or build artifacts included | Add exclusions to `meson.build`'s `exclude_files` / `exclude_directories` |
| Missing `roots.pem` | `install_data` for `etc/roots.pem` broken | Fix the `install_data()` call in `meson.build` |
| `.py` file content differs | Upstream modified Python source since fork point | Merge upstream (`git merge upstream/master`) |
| Different exported symbols | Visibility flags or new extension modules | Review `meson.build` cpp_args and check if upstream added new `.pyx` files |
| `cygrpc` fails to load | Linking issue — missing `libgrpc.so` at runtime | Ensure `libgrpc` and `libgpr` are installed; check `LD_LIBRARY_PATH` or `ldconfig` |
| Version mismatch | `_grpcio_metadata.py` or `grpc_version.py` out of date | Update version to match upstream after merge |
| New `Requires-Dist` in upstream | Upstream added a runtime Python dependency | Add it to `pyproject.toml` `[project] dependencies` |

### When to run this comparison

- **After every upstream merge** — before pushing the merge branch
- **After any `meson.build` change** — to catch install rule regressions
- **Before tagging a release** — final validation gate

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
