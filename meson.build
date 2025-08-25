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
# === @end: Dependencies ===


# === @begin: cygrpc.so ===
py.extension_module(
    'cygrpc',
    [
        'src/python/grpcio/grpc/_cython/cygrpc.pyx',
    ],
    # c_args: [], if you also need macros for C, copy them here OR (better) - just define them as a list before this invocation and pass this list to both `c_args` and `cpp_args`.
    cpp_args: [
        '-DPyMODINIT_FUNC=extern "C" __attribute__((visibility ("default"))) PyObject*',

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
        # This macro, in a similar manner, isn't set by CMake and seems to be only used by package maintaners when building multiple Python wheels for PyPi to work around some bugs related to macOS (at the price of performance reduced on other platforms).
        # It's only set in `setup.py` and in Bazel files, but in the latter it's also only used when building compilable Python extension.
        # In contrast to `GRPC_ENABLE_FORK_SUPPORT`, there is no corresponding environment variable, so we have even less reasons to worry about that.

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
        # Cython
        'src/python/grpcio/grpc/_cython/',
        'src/python/grpcio/',

        # C, C++
        '.',  # This is included automatically as a folder of the current `meson.build` file but we want to be explicit!
        'src/core/ext/upb-gen/',
        'src/core/ext/upbdefs-gen/',
        # Starting v6 or so, gRPC guys decided to spoil everything. Again.
        # Previously, `python/grpcio` was consuming `native/grpc` interfaces correctly - only the public ones and not using its private symbols.
        # But now, it uses 5 private `src/core/` headers directly and about another hundred directly.
        # It's really dangerou and fragile as it means that `python/grpcio` relies on internal implementation details, which may not be preserved in its public configuration.
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
    ],
    override_options : ['cython_language=cpp'], # See: https://github.com/mesonbuild/meson/blob/master/mesonbuild/compilers/cython.py and https://github.com/mesonbuild/meson/blob/master/mesonbuild/build.py#L995-L1003
    cython_args: ['--3str'], # NOTE: we need '3str' otherwise we error out, See: https://github.com/mesonbuild/meson/blob/master/test%20cases/cython/3%20cython_args/meson.build

    install: true,
    subdir: 'grpc/_cython/',
)
# === @end: cygrpc.so ===


# === @begin: Pure Python ===
# `py.get_install_dir()` -> site_packages!

install_data(
    [
        'src/python/grpcio/grpc/_cython/_cygrpc/__init__.py',
    ],

    install_dir: py.get_install_dir() / 'grpc' / '_cython' / '_cygrpc',
    install_tag: 'python-runtime',
)
# -> py.get_install_dir() / 'grpc' / '_cython' / '_cygrpc' / '__init__.py'

# -> py.get_install_dir() / 'grpc'
install_subdir(
    'src/python/grpcio/grpc/',

    install_dir: py.get_install_dir(),
    install_tag: 'python-runtime',

    exclude_directories: [
        # Cython sources
        '_cython/_cygrpc/',
    ],

    exclude_files: [
        # _cython/ sources and other buildtime/auxtime stuff
        '_cython/README.rst',
        '_cython/cygrpc.pxd',
        '_cython/cygrpc.pyx',
        '_cython/.gitignore',

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
