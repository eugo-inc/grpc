---
name: eugo-meson-build-review
description: Pre-commit review checklist for build-file changes in the Eugo gRPC fork - the root meson.build/pyproject.toml, @EUGO_CHANGE discipline in setup.py/commands.py/log.cc, the macro decision framework with the known-decision tables, the Cython-vs-src/core link-dep rule, code style, and the gates that must pass before pushing. Activates on "review meson.build", "review grpc build change", "check EUGO_CHANGE", "new macro in setup.py", "pyproject dependencies", "validate grpcio build change", "known macro decisions".
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
  upstream files" table in `eugo-upstream-merge`; add markers
  when touching those lines, never silently revert them.
- Upstream CMake (`CMakeLists.txt`, `cmake/`) is intentionally UNMODIFIED.
  Eugo's CMake configuration lives in the consumer
  (`protomolecule/dependencies/native/grpc/setup` - e.g. the
  `gRPC_INSTALL_LIBDIR` pinning). A diff adding Eugo logic to the repo's
  CMake files is almost always wrong - move it to the consumer setup.
- NEVER modify files under `third_party/` (we use system packages). Ask
  first before: adding new dependencies to `meson.build`, modifying
  `src/core/` C++ files (high merge-conflict risk), changing the
  protoc/grpc_python_plugin invocation interface, or removing any
  `@EUGO_CHANGE` block.

## Annotation discipline

Every modification to a file that exists upstream gets:

```python
# @EUGO_CHANGE: @begin: <why, not what>
...
# @EUGO_CHANGE: @end
```

(`//` form for C/C++; the `#` form also serves TOML/config files.) New files
that don't exist upstream do not need markers - the entire file is an Eugo
addition. `@HELP(now)` and `@TODO+` annotations are internal notes - replace
or supplement them with proper `@EUGO_CHANGE` markers. Never remove
`@EUGO_CHANGE` markers without replacing them.

In `meson.build`, use the section markers `# === @begin: Name ===` /
`# === @end: Name ===`, and every excluded or commented-out macro MUST carry
a comment explaining why (these comments are the fork's institutional memory
- see the existing file). When referencing upstream decisions, include links
to relevant CMake files, PRs, or issues.

## The checklist

1. **New macro in setup.py's DEFINE_MACROS?** Run the 4-question decision
   framework below. Record the verdict in the known-macro-decisions table.
2. **New link dependency?** If the undefined symbol is called from Cython
   (`.pyx`/`.pxi`), add it to `meson.build` (it will not come from libgrpc -
   precedent: `absl::log_initialize` for `absl::InitializeLog()`). If called
   from `src/core/`, it belongs in libgrpc - investigate, do not paper over.
3. **`setup.py:NNN` citations fresh?** `meson.build` comments cite setup.py
   line numbers; any setup.py change shifts them. Scan and update.
4. **New `.pyx` in CYTHON_EXTENSION_MODULE_NAMES or new CYTHON_HELPER_C
   file?** Needs a matching `meson.build` entry (a new `.pyx` needs a
   corresponding `py.extension_module()` entry). New `grpc_core_dependencies`
   `.cc` files do NOT (they compile into system libgrpc, not cygrpc) - unless
   the new file also adds a public header or new Cython-visible interface.
5. **INSTALL_REQUIRES changed?** Mirror into `pyproject.toml`
   `[project] dependencies` (currently `typing-extensions~=4.12`; the
   pure-Python `grpc` package uses `typing_extensions.override` in
   `_server.py` and `typing_extensions.Self` in `aio/_metadata.py`).
6. **Install rules touched?** New/moved pure-Python files or data
   (`roots.pem`) must survive the `install_subdir`/`install_data` rules -
   verify with the wheel file-list diff, not by eye.
7. **No leftover `<<<<<<<`/`>>>>>>>` markers; all `@begin` have `@end`.**

## Decision framework for new macros in setup.py

Because our Meson build compiles **only** `cygrpc.pyx` and links against
system-installed `libgrpc`, most upstream `setup.py` macro changes are
irrelevant - but you must consciously verify each one. When upstream adds a
new macro to `DEFINE_MACROS`, ask:

1. **Is it only checked in `src/core/` C++ files?** -> We don't compile
   `src/core/`. **Skip it.**
2. **Is it checked in any Cython (`.pyx`/`.pxi`/`.pxd`) file?** -> Check
   `src/python/grpcio/grpc/_cython/`. If yes, **add it to `meson.build`**.
3. **Is it checked in any public header under `include/`?** -> Those headers
   are included by our Cython build. If yes, **add it to `meson.build`**.
4. **Is it an ABI-affecting macro that changes struct layouts or vtable
   shapes in installed `libgrpc` headers?** -> Must match what `libgrpc` was
   compiled with. Check `cmake/` and `CMakeLists.txt` to see if CMake sets
   it. If CMake doesn't set it, upstream's installed `libgrpc` won't have it
   either - **skip it**.

How to check where a macro is used:

```bash
# Find all uses of a macro in the gRPC source tree
grep -rn 'MACRO_NAME' --include='*.h' --include='*.cc' --include='*.pyx' --include='*.pxi' --include='*.pxd' .

# Check if CMake sets it (meaning libgrpc was compiled with it)
grep -rn 'MACRO_NAME' CMakeLists.txt cmake/
```

## Known macro decisions for meson.build

| Macro | Upstream sets in setup.py | In meson.build | Reason |
|---|---|---|---|
| `PyMODINIT_FUNC` | Yes | **Yes - ACTIVE** | Needed to mark Cython init function with correct visibility. Only place it's checked is in Cython-generated C++. |
| `GRPC_ENABLE_FORK_SUPPORT` | Yes | **No** | Only affects `src/core/` (fork support logic). Not in any Cython code. CMake doesn't set it either. |
| `GRPC_DO_NOT_INSTANTIATE_POSIX_POLLER` | Removed upstream | **No** | Was removed and replaced by `GRPC_PYTHON_BUILD` (see below). |
| `GRPC_PYTHON_BUILD` | Yes (new in ~v1.79) | **No** | Guards behavior in `posix_engine.cc`, `dns_resolver_plugin.cc`, `shim.cc`, `backup_poller.cc` - all `src/core/` files we don't compile. System `libgrpc` was built without it (CMake doesn't set it). |
| `GRPC_ENABLE_FORK_SUPPORT_DEFAULT` | Yes (new in ~v1.79) | **No** | Affects `src/core/config_vars.cc`. We don't compile `src/core/`. |
| `GRPC_POSIX_FORK_ALLOW_PTHREAD_ATFORK` | Yes | **No** | Only affects `src/core/` (`posix_engine.cc`, `fork_posix.cc`) and only when `GRPC_ENABLE_FORK_SUPPORT` is also set. Confirmed not used in any Cython file. |
| `HAVE_CONFIG_H` | Yes | **No** | Legacy `./configure` artifact. Not used in CMake or Meson builds of `native/` or `python/`. |
| `__STDC_FORMAT_MACROS` | Yes | **No** | Pre-C++11 no-op on modern compilers. gRPC enables it unconditionally in `port_platform.h` anyway. |
| `GRPC_XDS_USER_AGENT_NAME_SUFFIX` | Yes | **No** | Cosmetic xDS client disambiguation. Not used in Cython code. |
| `GRPC_XDS_USER_AGENT_VERSION_SUFFIX` | Yes | **No** | Same as above. |
| `OPENSSL_NO_ASM` | Yes | **No** | Only relevant when BoringSSL/OpenSSL is bundled into `libgrpc`. We use system OpenSSL. |

Experiment flags: upstream uses gRPC experiment flags (e.g.
`IsEventEnginePollerForPythonEnabled()`) gated by `GRPC_PYTHON_BUILD`. These
experiments are compiled into `libgrpc` at CMake build time. Since our
`libgrpc` is built without `GRPC_PYTHON_BUILD`, they default to their
non-Python paths - which is correct for our use case (we want the same
high-performance paths as the native library).

## Known compiler flag decisions for meson.build

| Flag | setup.py sets (linux/darwin) | In meson.build | Reason |
|---|---|---|---|
| `-fno-exceptions` | No | **No - INACTIVE** | Dangerous |
| `-fno-wrapv` | No | **No - INACTIVE** | Dangerous |
| `-fvisibility=hidden` | Yes | **Implicit - handled by Meson** | Meson's `extension_module()` automatically sets `gnu_symbol_visibility='hidden'`. Our explicit `-DPyMODINIT_FUNC=...` override ensures the init function is still exported with default visibility. No manual flag needed. |
| `-static-libgcc` | Yes (linux) | **No** | Useful for portable wheel deployment. Not critical for our non-wheel server deployments. Re-evaluate if we start distributing wheels. |
| `-lpthread` | Yes (linux/darwin) | **Implicit via CMake dep** | gRPC's CMake config propagates `-lpthread` transitively. No explicit flag needed unless linking fails. |

## Known extra link dependencies for meson.build

Some symbols used by `cygrpc.pyx` (Cython code) are NOT provided by `libgrpc`
or `libgpr`. These must be added as explicit dependencies in `meson.build`'s
`extension_module()`.

| Library | CMake target | Symbol | Called from | Why not in libgrpc | Meson dep var |
|---|---|---|---|---|---|
| `libabsl_log_initialize.so` | `absl::log_initialize` | `absl::InitializeLog()` | `_cygrpc/absl.pyx.pxi:23` | `libgrpc` never calls `InitializeLog()` itself (CMakeLists.txt:3205-3232 - not in `grpc`'s `target_link_libraries`) | `absl_log_initialize` |

When adding new dependencies, follow this pattern:
1. Check if the undefined symbol is called from Cython code (`.pyx`/`.pxi`)
   or from `src/core/` C++
2. If from Cython: add to `meson.build` dependencies - it won't come from
   `libgrpc`
3. If from `src/core/`: should already be in `libgrpc` - investigate why it's
   missing

### Deep-dive: `absl::InitializeLog()` and `setup.py` vendoring

Upstream's `setup.py` compiles `third_party/abseil-cpp/absl/log/initialize.cc`
directly into `cygrpc.so` as a vendored source file - it does NOT link
against `libabsl_log_initialize.so`. This is the line:

```python
sources=(
    [module_file] + ... + ["third_party/abseil-cpp/absl/log/initialize.cc"]
)
```

This means `absl::InitializeLog()` appears as a `T` (text/defined) symbol in
the upstream wheel's `cygrpc.so`. Upstream PR
[grpc#39779](https://github.com/grpc/grpc/pull/39779) is the change that
added this - it introduced the Cython call to `InitializeLog()` to suppress
the "WARNING: All log messages before absl::InitializeLog() is called are
written to STDERR" message. The same PR added the
`GRPC_PYTHON_DISABLE_ABSL_INIT_LOG` env var opt-out.

Our Eugo `setup.py` change (`@EUGO_CHANGE`) guards this:

```python
+ ([] if BUILD_WITH_SYSTEM_ABSL else ["third_party/abseil-cpp/absl/log/initialize.cc"])
```

When `BUILD_WITH_SYSTEM_ABSL=true`, the vendored source is skipped to avoid
duplicate symbol errors, and `libabsl_log_initialize.so` provides the symbol
at runtime instead.

Our Meson build does the equivalent: instead of compiling the vendored
source, we link against the system `libabsl_log_initialize.so` via the
`absl::log_initialize` CMake target.

**Note on `libgrpc`'s abseil dependencies:** `libgrpc.so` does link against
*some* abseil libraries (`libabsl_log_internal_*`, `libabsl_strings`,
`libabsl_synchronization`, etc.) - but NOT `libabsl_log_initialize.so`
(verify with `readelf -d libgrpc.so | grep NEEDED`). `InitializeLog()` is an
application-level function meant to be called once by `main()`. `libgrpc` is
a library - it uses abseil's logging internals but never initializes the log
system itself.

### Unity/monolithic abseil builds

The dependency is declared `required: false` in `meson.build` to support
unity abseil builds (built with `-DABSL_BUILD_MONOLITHIC_SHARED_LIBS=ON`).
With a monolithic `libabseil_dll.so`, all abseil symbols (including
`InitializeLog()`) are in a single shared library. In that configuration:

- `absl::log_initialize` **still exists as a CMake target** in
  `abslTargets.cmake` - but it's an `INTERFACE IMPORTED` library whose only
  link dependency is `absl::abseil_dll` (the monolithic `.so`). It
  contributes no additional library of its own.
- The symbol comes transitively through `libgrpc.so` -> `libabseil_dll.so` -
  no explicit dep needed.
- Our `dependency('absl', modules: ['absl::log_initialize'], required:
  false)` will `found()` as true (the target exists), but linking it is
  harmless - it just adds another reference to `abseil_dll.so` which is
  already linked transitively via `libgrpc.so`.

**TODO+**: After switching to the abseil unity build in production, verify
whether the explicit `absl_log_initialize` dependency in `meson.build` is
still needed. With `libabseil_dll.so` providing all symbols transitively
through `libgrpc.so`, it may be redundant. Check with
`readelf --needed-libs cygrpc.so` - if `libabseil_dll.so` appears
transitively (via `libgrpc.so`), the explicit dep can potentially be removed.
However, keeping it is harmless and self-documenting.

## Code style

- **Python**: Python >= 3.12, use f-strings freely. Follow upstream gRPC
  Python style (they use Ruff as of recent versions). `pyproject.toml` uses
  `meson-python` as build backend - NOT setuptools.
- **C/C++**: Follow upstream gRPC style (see `GEMINI.md` at repo root for
  details). C17 (`gnu17`), C++23 (`gnu++23`). Prefer `absl` types, then
  `std` types. Include `absl` headers before gRPC headers.
- **Meson build files**: see "Annotation discipline" above.

## Gates before pushing

- `pip install . --no-build-isolation` succeeds against system libgrpc.
- Wheel-vs-upstream comparison + smoke tests 6a-6e (`eugo-wheel-validation`):
  identical file list, `PyInit_cygrpc` is the only `T` symbol, `NEEDED`
  lists `libgrpc.so`/`libgpr.so`.
- If CMake-adjacent files changed: native configure + build still pass.

## Related

- `eugo-upstream-merge` - the merge-time version of these checks + the
  divergence inventory tables.
- `eugo-build-and-test` / `eugo-rebuild` - build commands and scope.
- `eugo-wheel-validation` - the full validation gate.
- `meson` skill - general Meson reference (wraps, options, cross files).
