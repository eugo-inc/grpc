---
name: eugo-build-and-test
description: Use when building, installing, or smoke-testing the Eugo gRPC fork - the two protomolecule consumers (native/grpc via CMake, grpcio via meson-python), the local build commands, what a healthy install looks like (libgrpc/libgpr, protoc + grpc_python_plugin, cygrpc.so), and the wheel-vs-upstream validation gate. Activates on "build grpc", "build grpcio", "install grpcio", "smoke test grpc", "cygrpc fails to load", "grpc wheel comparison", "protoc grpc_python_plugin".
---

# Build & test the Eugo gRPC fork

Two protomolecule packages consume this fork (eugo-inc/grpc, branch eugo-main),
both pinned by git_commit in their meta.json - keep the two pins IDENTICAL
(cygrpc.so links the libgrpc that native/grpc installed; skew is an ABI trap):

1. `protomolecule/dependencies/native/grpc` - CMake build of the C/C++ libs.
2. `protomolecule/dependencies/python/wave_4/grpcio` - meson-python build of
   `cygrpc.so` via the Eugo-added root `meson.build` + `pyproject.toml`.

Build order is always native/grpc first, grpcio second. `grpcio_tools` is NOT
built - native `protoc` + `grpc_python_plugin` replace it (see
`eugo-grpcio-tools-migration`).

## How protomolecule builds native/grpc (setup distilled)

`native/grpc/setup` clones the fork at the pin (no submodule init - the
vendored deps upb/utf8_range/address_sorting/xxhash are copy-pasted in-tree),
then configures from `cmake/<build-dir>` NESTED TWO LEVELS below the source
root (`cmake ../..` - rare layout, gRPC requires it). Key args beyond
`${EUGO_CMAKE_COMMON_OPTIONS}`:

- `-DgRPC_INSTALL_LIBDIR=${EUGO_CANONICAL_INSTALL_LIBDIR}` and
  `-DgRPC_INSTALL_CMAKEDIR=.../cmake/grpc` - gRPC never includes
  GNUInstallDirs, so it IGNORES `CMAKE_INSTALL_LIBDIR`; unset, everything
  lands in `lib` and trips the validate_libdir_is_lib gate.
- `-DBUILD_SHARED_LIBS=ON -DgRPC_INSTALL=ON -DgRPC_BUILD_CODEGEN=ON`
- Plugins: CPP + PYTHON `ON` (python plugin = codegen only, not bindings);
  csharp/node/objc/php/ruby `OFF`; `gRPC_BUILD_GRPCPP_OTEL_PLUGIN=OFF`
  (otel-cpp is built WITH grpc support, enabling it here recurses).
- Every dependency provider is `package` (protobuf, absl, cares, re2, ssl,
  zlib, opencensus); `gRPC_USE_SYSTEMD=OFF`; `gRPC_BUILD_TESTS=OFF`.

Then `ninja && ninja install`.

## How protomolecule builds grpcio

`python/wave_4/grpcio/setup` is one pip call:
`pip3 install "grpcio @ <fork-at-pin>" ${EUGO_PIP_COMPILABLE_PACKAGE_OPTIONS}
${EUGO_PIP_TARGET_FLAG} ${EUGO_MESONPY_COMMON_OPTIONS}`. The root
`meson.build` finds gRPC via `dependency('gRPC', method: 'cmake', modules:
['gRPC::gpr', 'gRPC::grpc'])`, builds only `cygrpc.pyx`, links system
`libgrpc`/`libgpr` plus `absl::log_initialize` (required: false - unity absl
builds provide the symbol transitively), and installs the pure-Python `grpc/`
tree verbatim.

## Local builds (in the eugo container)

```bash
# native (requires system absl/protobuf/c-ares/zlib-ng/openssl/re2/opencensus)
# installs to /usr/local
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc) && cmake --install build

# grpcio (requires the native install above)
pip install meson-python meson cmake cython   # build prerequisites
pip install . --no-build-isolation            # or: pip wheel .
```

## What healthy looks like

- Native install: `libgrpc.so` + `libgpr.so` (and the grpc++/plugin-support
  libs) under the canonical libdir; `protoc` and `grpc_python_plugin` on PATH.
- Wheel: `cygrpc*.so` whose only `T` export is `PyInit_cygrpc` (everything
  else `U`), with `libgrpc.so` and `libgpr.so` in `readelf -d ... NEEDED`.
- Quick smoke: `python3 -c "import grpc; grpc.insecure_channel('localhost:0').close(); print(grpc.__version__)"`.

## Full validation - DEFER to eugo-wheel-validation

The primary correctness gate is the wheel-vs-upstream comparison plus runtime
tests 6a-6e (import, generic RPC, protoc codegen, sync + asyncio roundtrips)
in the `eugo-wheel-validation` skill. Run it after every upstream merge,
every `meson.build` change, and before bumping the protomolecule pins. Do
not duplicate it here.

## Related

- `eugo-rebuild` - cheapest-correct-rebuild decision guide.
- `eugo-meson-build-review` - pre-commit checklist for meson.build/setup.py.
- `eugo-upstream-merge` - merge recipe + the meta.json pin bump (push to
  eugo-main BEFORE editing pins; local-only SHAs break the protomolecule
  build).
- `eugo-wheel-validation` - the full validation gate (steps 1-5 + tests 6a-6e).
- `eugo-grpcio-tools-migration` - the native protoc + grpc_python_plugin path.
- `meson` / `bazel` skills - general build-system reference.
