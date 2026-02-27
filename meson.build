# === @begin: General ===
project(
    'grpcio',
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
        meson.project_source_root() / 'src' / 'python' / 'grpcio' / 'grpc_version.py'
    ], check: true).stdout().strip()
)
# === @end: General ===


# === @begin: Meson modules imports ===
py = import('python').find_installation(pure: false)
# === @end: Meson modules imports ===


# === @begin: Dependencies ===
# If we'd omit `grpc` as a dependency, the extension below will build fine as it still has access to gRPC headers (via our CFLAGS, CXXFLAGS, and similar variables).
# However, it won't have `libgrpc` and `libgpr` linked so it will crash at runtime. Why does it build without error in a first place? It's because Python extensions
# are always built to exclude dependency on `libpython.so` while still using its symbols (as it's `libpython.so` who loads these extensions at runtime, so related
# symbols are guaranteed to be resolved by this point and other way round - making extensions load libpython.so will almost certainly cause conflict by loading multiple
# libpython.so simultaneously). As a side effect of this, Python extensions don't need any of its dynamic library dependencies to be present at buildtime, only the headers.
#
# Even so, we always link all dependencies explicitely. It's not only makes the build more reliable but also allows to perform LTO on resultant object and benefit from LLVM bitcode embedded into linked libraries.
grpc = dependency('gRPC', method: 'cmake', modules: ['gRPC::gpr', 'gRPC::grpc']) # TODO+ Run-time dependency grpc (modules: gRPC::gpr, gRPC::grpc) found: YES 8.11.0-DEV

# See https://github.com/grpc/grpc/issues/41696 for details on why we need to link against absl::log_initialize.
# absl::log_initialize provides absl::InitializeLog(), called from _cygrpc/absl.pyx.pxi
# during module init. libgrpc does NOT link against this (CMakeLists.txt:3205-3232 shows
# grpc's target_link_libraries — no absl::log_initialize). Upstream setup.py compiles
# absl/log/initialize.cc directly into cygrpc.so (bundled-abseil case) or expects
# libabsl_log_initialize.so at runtime (system-abseil case, see setup.py:522-526).
# We must link explicitly since cygrpc.so is the caller, not libgrpc.
#
# required: false — with a unity/monolithic abseil build (-DABSL_BUILD_MONOLITHIC_SHARED_LIBS=ON),
# absl::log_initialize still exists as a CMake INTERFACE target in abslTargets.cmake but just
# points to absl::abseil_dll (the single .so). The symbol comes transitively via libgrpc → abseil_dll,
# so this dep is redundant but harmless in that case.
#
# NOTE: The CMake package name is 'absl' (not 'absl_log_initialize'). Abseil installs a
# single package config (abslConfig.cmake) that exports all targets. Using the wrong
# package name causes find_package() to fail silently with required: false.
#
# TODO+: After switching to abseil unity build, recheck whether this explicit dep is still
# needed or if InitializeLog() comes transitively through libgrpc → libabseil_dll.
absl_log_initialize = dependency('absl', method: 'cmake', modules: ['absl::log_initialize'], required: false)
# === @end: Dependencies ===


# === @begin: cygrpc.so ===
#
# Module name and source file:
#   Upstream setup.py:280 defines `CYTHON_EXTENSION_MODULE_NAMES = ("grpc._cython.cygrpc",)`.
#   The bare module name passed to Meson is `cygrpc` (the last dotted component).
#   The install `subdir` (see below) places it at `grpc/_cython/cygrpc.so` in site-packages,
#   matching what setuptools derives from the full dotted name.
#
#   The `.pyx` source path is computed by setup.py:480-482 as:
#     os.path.join(PYTHON_STEM, name.replace('.', '/') + '.pyx')
#   where PYTHON_STEM = 'src/python/grpcio' (setup.py:45).
#   Result: 'src/python/grpcio/grpc/_cython/cygrpc.pyx'.
#
py.extension_module(
    'cygrpc',
    [
        'src/python/grpcio/grpc/_cython/cygrpc.pyx',
    ],
    # c_args: [], if you also need macros for C, copy them here OR (better) - just define them as a list before this invocation and pass this list to both `c_args` and `cpp_args`.
    cpp_args: [
        # Source: setup.py:472-473 (linux/darwin branch of DEFINE_MACROS).
        # Upstream builds with `-fvisibility=hidden` (setup.py:253), which hides all symbols by default.
        # This macro overrides Python's `PyMODINIT_FUNC` to restore default visibility on the module
        # init function (`PyInit_cygrpc`), so the Python loader can find it via `dlsym()`.
        # In Meson, `extension_module()` implicitly sets `gnu_symbol_visibility='hidden'`, so the
        # situation is identical and this macro is equally necessary.
        #
        # The exact value comes from setup.py:472-473:
        #   pymodinit_type = "PyObject*"
        #   pymodinit = f'extern "C" __attribute__((visibility ("default"))) {pymodinit_type}'
        #   DEFINE_MACROS += (("PyMODINIT_FUNC", pymodinit),)
        '-DPyMODINIT_FUNC=extern "C" __attribute__((visibility ("default"))) PyObject*',

        # `-fno-exceptions`
        # setup.py sets this for linux/darwin via EXTRA_ENV_COMPILE_ARGS but we explicitly do not set it as we need exceptions to propagate from C++ code to Cython code.

        # `-fno-wrapv`
        # setup.py sets this for linux/darwin. Prevents the compiler from assuming signed integer overflow wraps (which is UB in C/C++).
        # We do not enable this as it can be potentionally dangerous even though this enables certain optimizer transformations that are only valid when overflow cannot happen.

        # '-DGRPC_ENABLE_FORK_SUPPORT',
        # This macro isn't used in `python/grpcio`, but only in `native/grpc`. But even there it's not set by any CMake option.
        #
        # If it's set at buildtime, this still can be disabled at runtime by using environment variable of the same name (`GRPC_ENABLE_FORK_SUPPORT`,
        # really bad practice, BTW). To make things more complicated, `python/grpcio` and `native/grpc` check this environment variable separately - so if it's
        # enforced in native portion, the Python portion still may decide to not use that and vice versa.
        #
        # If `GRPC_ENABLE_FORK_SUPPORT` isn't set at buildtime, enabling it at runtime in native library is a no-op. Python `grpc`, as said above, checks
        # environment variable separately, and doesn't consult with `native/grpc` either it has the required support or not, which may lead to unpredictable behavior at runtime.
        #
        # We don't enable this flag and environment variable neither in python nor in native variants anyway, so we aren't affected by this possible descripence.

        # '-DGRPC_DO_NOT_INSTANTIATE_POSIX_POLLER',
        # This macro was removed upstream and replaced by `-DGRPC_PYTHON_BUILD` (see below). It was only set in `setup.py` and Bazel files when bundling `src/core/` directly into `cygrpc.so`.
        # In contrast to `GRPC_ENABLE_FORK_SUPPORT`, there is no corresponding environment variable, so we have even less reasons to worry about that.

        # '-DGRPC_PYTHON_BUILD',
        # This is the upstream replacement for `GRPC_DO_NOT_INSTANTIATE_POSIX_POLLER`. It is set in `setup.py` and Bazel builds when `src/core/` C++ files are compiled DIRECTLY into `cygrpc.so`.
        # It guards Python-specific behavior in 4 `src/core/` files:
        #   1. `posix_engine.cc`     — `ShouldUsePosixPoller()` calls `IsEventEnginePollerForPythonEnabled()` instead of unconditionally returning `true`.
        #   2. `dns_resolver_plugin.cc` — EventEngine DNS resolver is gated behind the same experiment flag.
        #   3. `shim.cc`             — `EventEngineExperimentDisabledForPython()` returns `!IsEventEnginePollerForPythonEnabled()` rather than always `false`.
        #   4. `backup_poller.cc`    — Two uses: re-enables backup polling if the poller experiment is off; `g_can_poll_in_background()` always returns `false` for Python builds.
        #
        # WHY WE DO NOT SET IT HERE:
        # Our Meson build links `cygrpc.so` against system-installed `libgrpc` (compiled via CMake WITHOUT this macro).
        # We do NOT compile any `src/core/` files ourselves. The macro is only relevant for code compiled into `libgrpc` — which we don't touch.
        # Therefore, this macro has no effect on our Cython-only compilation and must NOT be set here.
        # If we ever switch to bundling `src/core/` into `cygrpc.so`, revisit this decision.

        # `-DGRPC_POSIX_FORK_ALLOW_PTHREAD_ATFORK`
        # This is not set in any place except PHP extension build and a few related some examples.
        # There is no explanation of that it does but it only affects the `native/grpc` build if `GRPC_ENABLE_FORK_SUPPORT` is also set.
        # This is not used in `python/grpcio`, neither at runtime (via env var) nor at buildtime (via macro), so due to this and above facts we don't touch it at all. Meh.

        # `-DHAVE_CONFIG_H`
        # This is an artifact of old days when gRPC was built using ./configure. This is not used in `native/` and `python/` variants anymore, only in old `Make` files.
        # It also appears in compilable PHP extension, yet it's also built via CMake modern days, so it will be unser here either.
        #
        # References:
        # 1. https://www.gnu.org/software/autoconf/manual/autoconf-2.72/html_node/Configuration-Headers.html#:~:text=generatedfiles%20with%20%2D-,DHAVE_CONFIG_H,-instead%20of%20the

        # `-D__STDC_FORMAT_MACROS`,
        # This is also quite old macro, originating from C and C++ standard libraries. It was used prior C++11 and C11 to force-enable special formatting macros for numeric types[1].
        # In modern compilers and standard libraries, the associated macros are brought automatically by relevant headers without using `__STDC_FORMAT_MACROS`.
        #
        # `gRPC` checks during its CMake configuration stage[2], either we are using quite modern system and forces this macro or omits it depending on the result.
        # However, in quite messy "portability" header[3], they enable this macro unconditionally. Lol. It doesn't harm us anyhow - it's just a no-op.
        #
        # The craziest part is that gRPC requires C++14 support at minimum, but it looks like this strange trash has been introduced by [4-5] PRs to make gRPC compatible with really-really old platforms.
        # As always, it's not documented in the .git itself and confuses everyone else who actually doesn't need this macro.
        #
        # @Important:
        # As it's not gRPC-specific macro, it's hard to say either it affects `python/grpcio` or not. It may be included from `native/grpc` headers, from `src/core` upb headers, and such, but it's really time-consuming and complicated to research that.
        # However, as it's a no-op anyway - we don't bother and just omit it.
        #
        # References:
        # 1. https://en.cppreference.com/w/cpp/types/integer.html#:~:text=Notes
        # 2. `@/CMakeLists.txt`
        # 3. `@/support/port_platform.h`
        # 4. https://github.com/grpc/grpc/pull/32159
        # 5. https://github.com/grpc/grpc/pull/14487

        # `-DGRPC_XDS_USER_AGENT_NAME_SUFFIX={}`
        # `-DGRPC_XDS_USER_AGENT_VERSION_SUFFIX={}`
        #
        # These are only set to better disambiguate the Envoy API (xDS) clients across various gRPC bindings (native, C#, Python, PHP, ...).
        # If it's not set, the generic values will be provided, but still quite informative (native gRPC strings).
        #
        # These macros aren't used in Python code directly anyway, and cause we don't set them manually in native one as well, we omit it in our tiny little fork.

        # `-DOPENSSL_NO_ASM`
        #
        # This is only used if BoringSSL/OpenSSL is built as part of `native/grpc` build (which we don't do as making `grpc` use OpenSSL built by us).
        # It's not used in building `python/grpcio` and is actually unused in native code either.
        # However, it's preserved in resultant `native/grpc` configuration if used, probably for downstream consumption.
        #
        # References:
        # 1. https://github.com/eugo-inc/grpc/blob/master/cmake/ssl.cmake#L24-L42
    ],
    include_directories: [
        # Cython include path:
        #  - commands.py:237 adds `CYTHON_STEM` to Cython.Build.cythonize(include_path=...).
        #  - CYTHON_STEM = os.path.join(PYTHON_REL_PATH, 'grpc', '_cython') → 'src/python/grpcio/grpc/_cython'.
        #  - This is how Cython resolves `include "_cygrpc/grpc.pxi"` in cygrpc.pxd.
        #  - NOTE: Cython also resolves `include` directives relative to the source file's directory,
        #  - so this path is technically redundant for files included from cygrpc.pxd/cygrpc.pyx
        #  - (since they live in the same directory). But it's needed for any `cimport` that
        #  - Cython resolves via the include_path rather than relative to the source.
        'src/python/grpcio/grpc/_cython/',

        # PYTHON_STEM — setup.py:45 defines `PYTHON_STEM = os.path.join('src', 'python', 'grpcio')`.
        # This appears as the first element of EXTENSION_INCLUDE_DIRECTORIES (setup.py:346).
        # Needed because Cython-generated C++ code may `#include` headers relative to the grpcio root,
        # and some `.pxi` files reference Python-level modules whose stubs live here.
        'src/python/grpcio/',

        # CORE_INCLUDE — setup.py:46 defines `CORE_INCLUDE = ('.',)`.
        # The repo root is needed because private `src/core/` headers (used by 5 `cdef extern from`
        # declarations in grpc.pxi and grpc_aio.pxd.pxi) use repo-root-relative `#include` paths like:
        #   #include "src/core/call/message.h"
        #   #include "src/core/lib/channel/channel_args.h"
        # Without `.` as an include dir, the C++ compiler can't resolve these.
        '.',

        # `UPB_GRPC_GENERATED_INCLUDE` / `UPBDEFS_GRPC_GENERATED_INCLUDE` — setup.py:68-69.
        # These contain generated protobuf headers (`.upb.h`) used by `src/core/` files in upstream's
        # bundled build. However, even though Cython code doesn't directly include upb-gen headers, private `src/core/`
        # headers DO include them transitively. For example, `src/core/channelz/channel_trace.h` includes
        # `src/proto/grpc/channelz/v2/channelz.upb.h`, which lives at
        # `src/core/ext/upb-gen/src/proto/grpc/channelz/v2/channelz.upb.h` in the source tree.
        # Without this include dir, the compiler cannot resolve those upb-generated headers.
        'src/core/ext/upb-gen/',
        'src/core/ext/upbdefs-gen/',
        #
        # Starting v6 or so, gRPC guys decided to spoil everything. Again.
        # Previously, `python/grpcio` was consuming `native/grpc` interfaces correctly - only the public ones and not using its private symbols.
        # But now, it uses 5 private `src/core/` headers directly and about another hundred directly.
        # It's really dangerous and fragile as it means that `python/grpcio` relies on internal implementation details, which may not be preserved in its public configuration.
        # However, private headers used by `python/grpcio` include public headers using angle brackets `<>` and use the public search paths and not the ones relative/pointing to the repo itself.
        # E.g.:
        # 1. #include <grpc/support/port_platform.h> - this will find our, `/usr/local` headers, so our `grpc` configuration is used and not the template/default one from `grpc` sources.
        # 2. #include "src/core/call/message.h" - this prefers searching in `grpc` sources itself and not in the system (`/usr/local` and friends). However, as these are private headers, we don't have them installed anyway.
        #
        # Generally speaking, this approach couldn't be always safe:
        # 1. If package build system passes configuration via `-D` macros and not via `.h` headers, the private `-D` macros are usually excluded from its public `-config.cmake`, `.pc`, and similar configuration providers files.
        # 2. Good news for us is that as the moment of writing (2025-08-22), gRPC stores most of its macros in `.h` files. The only one it uses during the build `-DgRPC_EXPORTS` is not used in private headers from `src/core/` (in fact, it doesn't look like to be used anywhere at all).
        #
        # However, other packages (or even `python/grpcio`) may use the different approach and rely on `-D` macros to preserve configuration and in this case, it's possible that we won't have macros propagated correctly to private headers when linking `python/` package part to the `native/`
        # as they will likely be missing from configuration `-config.cmake`, `.pc`, and similar files (as you shouldn't include private configuration details into public configuration - if you do, why to call it __private__?).
        # In such case, different macros may affect API, ABI, and/or binary compatability, making the package crash at buildtime in the best case or exhibit UB at runtime. Really hard-to-debug UB. I hate UB.
    ],
    dependencies: [
        # Package-managed
        grpc,
    ] + (absl_log_initialize.found() ? [absl_log_initialize] : []),  # absl::InitializeLog() — see Dependencies section
    override_options : ['cython_language=cpp'], # See: https://github.com/mesonbuild/meson/blob/master/mesonbuild/compilers/cython.py and https://github.com/mesonbuild/meson/blob/master/mesonbuild/build.py#L995-L1003
    cython_args: ['--3str'], # NOTE: we need '3str' otherwise we error out, See: https://github.com/mesonbuild/meson/blob/master/test%20cases/cython/3%20cython_args/meson.build

    install: true,
    # Install subdir: setuptools derives this from the dotted extension name `grpc._cython.cygrpc`.
    # The last component (`cygrpc`) becomes the .so filename; everything before it (`grpc._cython`)
    # becomes the subdirectory path `grpc/_cython/` under site-packages.
    # In Meson, `subdir` explicitly provides this path since we only pass the bare name `cygrpc`.
    subdir: 'grpc/_cython/',
)
# === @end: cygrpc.so ===


# === @begin: Pure Python ===
# `py.get_install_dir()` -> site_packages!

# roots.pem — SSL root certificates.
#
# Upstream setup.py:586-591 copies `etc/roots.pem` into `src/python/grpcio/grpc/_cython/_credentials/`
# at module load time (before setup() runs). The `_credentials/` directory does NOT exist in the source
# tree — it's created on the fly. The `PYTHON-MANIFEST.in` line `recursive-include ... *.pem` then
# picks it up for inclusion in the sdist/wheel.
#
# At runtime, `security.pyx.pxi:26` loads it via:
#   pkgutil.get_data(pkg, '_credentials/roots.pem')
# where `pkg` resolves to `grpc._cython`. This means the file must be installed at:
#   site-packages/grpc/_cython/_credentials/roots.pem
#
# Meson doesn't run setup.py's copy step, so we install `etc/roots.pem` directly to the correct
# location. Without this, the grpc package silently falls back to certifi or system-provided certs,
# which may not include all the CAs that gRPC users depend on.
install_data(
    'etc/roots.pem',
    install_dir: py.get_install_dir() / 'grpc' / '_cython' / '_credentials',
    install_tag: 'python-runtime',
)

# `_cygrpc/__init__.py` — package marker for 'grpc._cython._cygrpc'.
#
# This file is license-header-only (no code). It exists upstream to make `_cygrpc` a proper Python
# package. The upstream wheel includes it, and while we exclude the `_cygrpc/` *directory* from
# `install_subdir` below (since it contains only Cython sources: .pxd.pxi, .pyx.pxi, grpc.pxi,
# plus the aio/ subdir), we must install this one file separately to preserve the package structure.
#
# Without it, `_cygrpc` wouldn't be importable as a package. While no code currently imports it
# directly, its presence matches upstream's wheel layout (verified via Section "Validating the Eugo
# wheel against upstream" in copilot-instructions.md).
install_data(
    [
        'src/python/grpcio/grpc/_cython/_cygrpc/__init__.py',
    ],

    # The install path is derived from the file's source path relative to the `grpc/` package root.
    # The last two components (`_cython/_cygrpc`) become the subdirectory path under site-packages, and the filename (`__init__.py`) is preserved.
    # `py.get_install_dir()` points to site-packages, so this installs the file at `site-packages/grpc/_cython/_cygrpc/__init__.py`.
    install_dir: py.get_install_dir() / 'grpc' / '_cython' / '_cygrpc',
    install_tag: 'python-runtime',
)

# Pure Python package tree — installs everything under `grpc/` to site-packages.
#
# Upstream setup.py uses setuptools auto-discovery from the `grpc/` package root within PYTHON_STEM.
# `PYTHON-MANIFEST.in` line 1 (`recursive-include src/python/grpcio/grpc *.py`) controls which files
# are included in the sdist. In Meson, we replicate this with `install_subdir`, which copies the
# directory tree recursively, minus explicit exclusions.
#
# The source path `src/python/grpcio/grpc/` installs *as* `grpc/` under `py.get_install_dir()`
# (i.e., site-packages), because Meson's `install_subdir` preserves the last path component.
# Result: site-packages/grpc/__init__.py, site-packages/grpc/aio/__init__.py, etc.
install_subdir(
    'src/python/grpcio/grpc/',

    # `py.get_install_dir()` returns the path to site-packages, so this installs the `grpc/` package directly into site-packages.
    install_dir: py.get_install_dir(),
    install_tag: 'python-runtime',

    exclude_directories: [
        # Cython sources
        # The _cygrpc/ directory contains ONLY Cython source files used at build time:
        #   - ~30 pairs of .pxd.pxi and .pyx.pxi files (arguments, call, channel, credentials, ...)
        #   - grpc.pxi (834 lines — the main C/C++ extern declarations)
        #   - aio/ subdirectory (15 async support files)
        #   - __init__.py (installed separately via install_data above)
        # None of these are needed at runtime — the compiled cygrpc.so contains all their logic.
        # Upstream's wheel also excludes raw Cython sources.
        '_cython/_cygrpc/',
    ],

    exclude_files: [
        # Cython source/build files — not needed at runtime (compiled into cygrpc.so)
        '_cython/README.rst',
        '_cython/cygrpc.pxd',   # Cython declaration file for cygrpc
        '_cython/cygrpc.pyx',   # Cython implementation file for cygrpc
        '_cython/.gitignore',

        # Bazel BUILD files — upstream uses Bazel for CI but these aren't part of the installed package.
        # Every subdirectory under grpc/ that contains a BUILD.bazel must be listed here because
        # Meson's exclude_files paths are relative to the install_subdir source root ('grpc/').
        # **/BUILD.bazel
        'BUILD.bazel',
        '_cython/BUILD.bazel',
        'experimental/BUILD.bazel',
        'framework/BUILD.bazel',
        'framework/common/BUILD.bazel',
        'framework/foundation/BUILD.bazel',
        'framework/interfaces/base/BUILD.bazel',
        'framework/interfaces/BUILD.bazel',
        'framework/interfaces/face/BUILD.bazel',
    ]
)
# === @end: Pure Python ===
