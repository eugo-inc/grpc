# === @begin: General ===
# IMPORTANT: i have not tested this....just started it
# Meson build for grpcio_observability — Eugo fork
#
# Upstream (setup.py) bundles ~150+ C/C++ files from src/core/, third_party/abseil-cpp/,
# third_party/protobuf/ directly into _cyobservability.so. In Eugo's model, all those
# symbols are already in system-installed libgrpc/libabsl/libprotobuf — so we link
# against them instead. We only compile:
#   1. _cyobservability.pyx (Cython bridge)
#   2. 7 observability-specific C++ files in grpc_observability/ (NOT part of libgrpc)
#
# The observability C++ code uses private gRPC headers from src/core/ (e.g.
# call_tracer.h, channel_stack.h, metadata_batch.h). These are NOT installed
# by CMake, so we point include paths to the repo source tree.
project(
    'grpcio_observability',
    'c', 'cpp', 'cython',
    default_options: [
        'c_std=gnu17',
        'cpp_std=gnu++23',
        'buildtype=release',
    ],
    meson_version: '>=1.3.0',
    version: run_command([
        'sed', '-n',
        's/VERSION\\s*=\\s*[\'"]\\([^\'\"]*\\)[\'\"]/\\1/p',
        meson.project_source_root() / 'grpc_version.py'
    ], check: true).stdout().strip()
)
# === @end: General ===


# === @begin: Path constants ===
# The repo root is 3 levels up from src/python/grpcio_observability/
grpc_repo_root = meson.project_source_root() / '..' / '..' / '..'
# === @end: Path constants ===


# === @begin: Meson modules imports ===
py = import('python').find_installation(pure: false)
# === @end: Meson modules imports ===


# === @begin: Dependencies ===
# System-installed gRPC provides libgrpc (which contains all src/core/ symbols),
# libgpr, and transitively brings abseil, protobuf, and other dependencies.
# This replaces the ~150+ CC_FILES from observability_lib_deps.py plus the
# abseil/protobuf/upb files that upstream bundles into the extension.
grpc = dependency('gRPC', method: 'cmake', modules: ['gRPC::gpr', 'gRPC::grpc'])
# === @end: Dependencies ===


# === @begin: _cyobservability.so ===
py.extension_module(
    '_cyobservability',
    [
        # Cython source
        'grpc_observability/_cyobservability.pyx',

        # Observability-specific C++ files — these implement Python-specific
        # observability call tracers, metadata exchange, sampling, etc.
        # These are NOT part of libgrpc and MUST be compiled here.
        'grpc_observability/client_call_tracer.cc',
        'grpc_observability/metadata_exchange.cc',
        'grpc_observability/observability_util.cc',
        'grpc_observability/python_observability_context.cc',
        'grpc_observability/rpc_encoding.cc',
        'grpc_observability/sampler.cc',
        'grpc_observability/server_call_tracer.cc',
    ],
    cpp_args: [
        # Needed to export Cython module init function with default visibility.
        # Meson's extension_module() sets -fvisibility=hidden, which hides the
        # PyInit__cyobservability symbol. This macro overrides it for the init function.
        '-DPyMODINIT_FUNC=extern "C" __attribute__((visibility ("default"))) PyObject*',

        # Set on Linux by setup.py. Declares POSIX threading support.
        '-DHAVE_PTHREAD=1',

        # `-D__STDC_FORMAT_MACROS`
        # setup.py sets this for aarch64 compatibility with <inttypes.h> used in
        # src/core/telemetry/call_tracer.h. It's a no-op on modern compilers
        # (gRPC's port_platform.h also defines it unconditionally). Omitted here
        # for same reasons as root meson.build — see copilot-instructions.md.

        # `-frtti`
        # setup.py explicitly sets this. RTTI is enabled by default in GCC/Clang,
        # so no explicit flag is needed unless we've set -fno-rtti (we haven't).
        # The observability C++ code uses dynamic dispatch via virtual base classes
        # from src/core/telemetry/call_tracer.h — RTTI must remain enabled.

        # `-fno-wrapv`
        # setup.py sets this. Prevents compiler from assuming signed integer overflow
        # wraps. We don't set it — see root meson.build for rationale.

        # `-flto`
        # setup.py enables LTO for dead code elimination. We omit it here —
        # it's a build optimization, not a correctness requirement. Can be enabled
        # via `meson configure -Db_lto=true` if desired.
    ],
    include_directories: [
        # Local observability headers (constants.h, sampler.h, python_observability_context.h, etc.)
        # The .pxd file uses `cdef extern from "python_observability_context.h"` etc.,
        # so this directory must be in the include path.
        'grpc_observability/',

        # Repo root — needed for two kinds of includes:
        # 1. Private gRPC headers: `#include "src/core/telemetry/call_tracer.h"`,
        #    `#include "src/core/lib/channel/channel_stack.h"`, etc.
        #    These are NOT installed by CMake — they only exist in the source tree.
        # 2. Although public <grpc/...> headers come from system install, some private
        #    headers use `#include "include/grpc/..."` style which needs repo root.
        grpc_repo_root,

        # Generated protobuf/upb headers used by some src/core/ private headers
        grpc_repo_root / 'src' / 'core' / 'ext' / 'upb-gen',
        grpc_repo_root / 'src' / 'core' / 'ext' / 'upbdefs-gen',
    ],
    dependencies: [
        grpc,
    ],
    override_options: ['cython_language=cpp'],
    cython_args: ['--3str'],

    install: true,
    subdir: 'grpc_observability/',
)
# === @end: _cyobservability.so ===


# === @begin: Pure Python ===
install_subdir(
    'grpc_observability/',

    install_dir: py.get_install_dir(),
    install_tag: 'python-runtime',

    # Exclude C/C++/Cython sources — only install .py files
    exclude_files: [
        # Cython sources
        '_cyobservability.pyx',
        '_cyobservability.pxd',

        # C++ sources and headers
        'client_call_tracer.cc',
        'client_call_tracer.h',
        'constants.h',
        'metadata_exchange.cc',
        'metadata_exchange.h',
        'observability_util.cc',
        'observability_util.h',
        'python_observability_context.cc',
        'python_observability_context.h',
        'rpc_encoding.cc',
        'rpc_encoding.h',
        'sampler.cc',
        'sampler.h',
        'server_call_tracer.cc',
        'server_call_tracer.h',

        # Build files
        'BUILD.bazel',
    ],
)
# === @end: Pure Python ===
