---
name: eugo-grpcio-tools-migration
description: Why the Eugo gRPC fork eliminates grpcio_tools and how downstream projects migrate to native protoc + grpc_python_plugin - the upstream vs Eugo call chains, the CMake targets that make it work with zero source changes, the canonical protoc invocation, the grpc_2_0 codegen option, and the downstream Meson integration pattern. Activates on "grpcio_tools", "grpc_tools.protoc", "grpc_python_plugin", "migrate protoc invocation", "python -m grpc_tools.protoc", "protoc plugin for grpc python".
---

# Eugo: grpcio_tools is eliminated

Upstream ships a separate `grpcio_tools` package that bundles `protoc` +
`grpc_python_plugin` as a Python extension. Our fork does not need this.
Eliminating it required **zero modifications** to upstream source code - it is
purely an architectural choice: we rely on standalone binaries that upstream's
CMake already builds and installs, instead of bundling those same binaries
into a Python extension. Never build or ship `grpcio_tools` as a Python
package.

## Canonical invocation

```bash
protoc \
  --plugin=protoc-gen-grpc_python="$(which grpc_python_plugin)" \
  --proto_path="${SOURCE_ROOT}" \
  --python_out="${BUILD_ROOT}" \
  --grpc_python_out="${BUILD_ROOT}" \
  --grpc_python_opt="grpc_2_0" \
  "${INPUT}"
```

This works because we build `native/grpc` (via CMake), which installs
`protoc`, `grpc_python_plugin`, headers, and libraries system-wide.

## What upstream `grpcio_tools` does

Upstream `grpcio_tools` compiles the entire protobuf compiler + gRPC Python
code generator into a single Python extension (`_protoc_compiler.so`),
callable from Python:

```
Python: grpc_tools.protoc.main([...])
  -> _protoc_compiler.pyx: run_main()           (Cython wrapper)
    -> main.cc: protoc_main()                    (in-process protoc CLI)
      -> python_generator.cc                     (generates *_pb2_grpc.py)
```

Key upstream files:

| File | Purpose |
|---|---|
| `tools/distrib/python/grpcio_tools/grpc_tools/_protoc_compiler.pyx` | Cython extension wrapping `protoc_main()` as `run_main()` |
| `tools/distrib/python/grpcio_tools/grpc_tools/main.cc` | Creates `CommandLineInterface`, registers `--python_out`, `--pyi_out`, `--grpc_python_out` generators, calls `cli.Run()` |
| `tools/distrib/python/grpcio_tools/grpc_tools/protoc.py` | Python API: `main()` encodes args to bytes, calls `_protoc_compiler.run_main()` |
| `src/compiler/python_generator.cc` | The actual code generator - produces `*_pb2_grpc.py` with stubs, servicers, etc. |
| `src/compiler/python_plugin.cc` | Standalone `main()` wrapping `PythonGrpcGenerator` in the protobuf plugin protocol |

The setup.py extension links **all of protoc/libprotobuf** C++ sources
(hundreds of files from `CC_FILES`) into `_protoc_compiler.so`.

## What Eugo does instead

The **exact same `python_generator.cc`** code is used both ways. Upstream
wraps it in `main.cc` -> `_protoc_compiler.so` (in-process). We use the
standalone `grpc_python_plugin` binary that CMake already builds:

```
Shell: protoc --plugin=protoc-gen-grpc_python=$(which grpc_python_plugin) ...
  -> protoc (standalone binary, from protobuf)
    -> grpc_python_plugin (standalone binary, from gRPC CMake)
      -> python_generator.cc    (same code as grpcio_tools uses)
```

## CMake targets that make this work

All from upstream `CMakeLists.txt` - no Eugo modifications:

**`grpc_plugin_support`** (static library, `CMakeLists.txt:6445`):
```cmake
add_library(grpc_plugin_support
  src/compiler/cpp_generator.cc
  src/compiler/csharp_generator.cc
  src/compiler/node_generator.cc
  src/compiler/objective_c_generator.cc
  src/compiler/php_generator.cc
  src/compiler/proto_parser_helper.cc
  src/compiler/python_generator.cc      # <- the Python codegen logic
  src/compiler/ruby_generator.cc
)
```

**`grpc_python_plugin`** (executable, `CMakeLists.txt:19606`):
```cmake
add_executable(grpc_python_plugin
  src/compiler/python_plugin.cc         # <- minimal main() calling PythonGrpcGenerator
)
target_link_libraries(grpc_python_plugin
  grpc_plugin_support                   # <- gets python_generator.cc
)
```

**Install rule** (`CMakeLists.txt:19635`):
```cmake
install(TARGETS grpc_python_plugin EXPORT gRPCPluginTargets
  RUNTIME DESTINATION ${gRPC_INSTALL_BINDIR}   # -> /usr/local/bin/grpc_python_plugin
)
```

`protoc` itself comes from the protobuf dependency - when gRPC's CMake builds
or finds protobuf, `protoc` ends up in `/usr/local/bin/` as well.

## Why no source changes were needed

1. **Same generator code**: Both paths use `python_generator.cc` - identical
   output.
2. **CMake already installs the binary**: `grpc_python_plugin` is an upstream
   build target, enabled by default (`gRPC_BUILD_GRPC_PYTHON_PLUGIN=ON`).
3. **protoc's plugin protocol**: `protoc` natively supports
   `--plugin=protoc-gen-X=<path>` for external code generators - this is the
   standard protobuf extension mechanism.
4. **No Python-specific behavior lost**: The only extra thing `grpcio_tools`
   provides over native protoc is (a) bundled `.proto` files (we get them from
   `/usr/local/include/` via CMake install) and (b) a dynamic proto finder for
   import-time compilation (not needed in our build pipeline).

## Eugo-created reference files (experimental, not used in production)

| File | Purpose |
|---|---|
| `tools/distrib/python/grpcio_tools/meson.build` | Experimental Meson build that builds `_protoc_compiler.so` linking against system protobuf/gRPC - exists as a reference, **not the intended approach** |
| `tools/distrib/python/grpcio_tools/pyproject.toml` | `meson-python` config for the experimental build |
| `tools/distrib/python/grpcio_tools/eugo_copy_sources.py` | Helper to copy `include/` and `src/compiler/` (unused - Meson build references sources via relative paths) |
| `tools/distrib/python/grpcio_tools/.gitignore` | Ignores build artifacts |

Do **not** build `grpcio_tools` separately. Use native `protoc` +
`grpc_python_plugin` instead.

## Protoc invocation notes for downstream projects

Downstream projects that previously used `grpc_tools.protoc.main([...])` or
`python -m grpc_tools.protoc` must switch to native `protoc`. Key differences:

1. **Plugin must be explicit**:
   `--plugin=protoc-gen-grpc_python="$(which grpc_python_plugin)"`
2. **No bundled `.proto` files**: `grpc_tools` ships `google/protobuf/*.proto`
   inside the package. With native protoc, these are at `/usr/local/include/`
   (or wherever CMake installed them). Add `--proto_path=/usr/local/include`
   if your protos import `google/protobuf/empty.proto` etc.
3. **`grpc_2_0` opt for modern codegen**: `--grpc_python_opt="grpc_2_0"`
   produces code that uses `grpc.method_handlers_generic_handler()` instead of
   the deprecated `grpc.method_service_handler()`.
4. **Import path fixups**: Generated code uses bare module imports (e.g.,
   `import foo_pb2`). If your project needs relative imports, post-process
   with `sed`:
   ```bash
   sed -i -E 's/from src.ray.protobuf/from ./' "${OUTPUT}"
   ```

## Downstream Meson integration pattern

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

## Related

- `eugo-build-and-test` - building native/grpc, which installs the binaries.
- `eugo-wheel-validation` - smoke test 6c exercises this codegen path.
