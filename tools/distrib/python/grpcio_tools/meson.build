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
        meson.project_source_root() / 'grpc_tools' / 'grpc_version.py'
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
grpc = dependency('gRPC', method: 'cmake', modules: ['gRPC::grpc', 'gRPC::grpc++', 'gRPC::grpc_plugin_support']) # TODO+ Run-time dependency grpc (modules: gRPC::gpr, gRPC::grpc) found: YES 8.11.0-DEV
protobuf = dependency('protobuf', method: 'cmake', modules: ['protobuf::libprotobuf', 'protobuf::libprotoc', 'protobuf::protoc', 'protobuf::libupb', 'protobuf::protoc-gen-upb', 'protobuf::protoc-gen-upbdefs', 'protobuf::protoc-gen-upb_minitable'])
utf8_range = dependency('protobuf', method: 'cmake', modules: ['utf8_range::utf8_range'])
# NOTE: these are the modules listed in protoc_lib_deps.py: `base`, `container`, `crc`, `debugging`, `hash`, `log`, `numeric`, `profiling`, `status`, `strings`, `synchronization`, `time`
absl = dependency('absl', method: 'cmake', modules: [
    'absl::base',
    'absl::container_common',
    'absl::container_memory',
    'absl::hash_container_defaults',
    'absl::crc_internal',
    'absl::crc_cpu_detect',
    'absl::crc_cord_state',
    'absl::debugging',
    'absl::hash',
    'absl::log',
    'absl::numeric',
    'absl::profile_builder',
    'absl::hashtable_profiler',
    'absl::status',
    'absl::strings',
    'absl::synchronization',
    'absl::time',
])
# === @end: Dependencies ===




# ['ZLIB::ZLIB', 'Threads::Threads', 'absl::atomic_hook', 'absl::errno_saver', 'absl::log_severity', 'absl::no_destructor', 'absl::nullability', 'absl::raw_logging_internal', 'absl::spinlock_wait', 'absl::config', 'absl::dynamic_annotations', 'absl::core_headers', 'absl::malloc_internal', 'absl::base_internal', 'absl::base', 'absl::throw_delegate', 'absl::pretty_function', 'absl::endian', 'absl::scoped_set_env', 'absl::strerror', 'absl::fast_type_id', 'absl::prefetch', 'absl::poison', 'absl::tracing_internal', 'absl::iterator_traits_internal', 'absl::iterator_traits_test_helper_internal', 'absl::algorithm', 'absl::algorithm_container', 'absl::cleanup_internal', 'absl::cleanup', 'absl::btree', 'absl::compressed_tuple', 'absl::fixed_array', 'absl::inlined_vector_internal', 'absl::inlined_vector', 'absl::flat_hash_map', 'absl::flat_hash_set', 'absl::node_hash_map', 'absl::node_hash_set', 'absl::hash_container_defaults', 'absl::container_memory', 'absl::hash_function_defaults', 'absl::hash_policy_traits', 'absl::common_policy_traits', 'absl::hashtablez_sampler', 'absl::hashtable_debug', 'absl::hashtable_debug_hooks', 'absl::node_slot_policy', 'absl::raw_hash_map', 'absl::container_common', 'absl::hashtable_control_bytes', 'absl::raw_hash_set', 'absl::raw_hash_set_resize_impl', 'absl::layout', 'absl::crc_cpu_detect', 'absl::crc_internal', 'absl::crc32c', 'absl::non_temporal_arm_intrinsics', 'absl::non_temporal_memcpy', 'absl::crc_cord_state', 'absl::stacktrace', 'absl::symbolize', 'absl::examine_stack', 'absl::failure_signal_handler', 'absl::debugging_internal', 'absl::demangle_internal', 'absl::bounded_utf8_length_sequence', 'absl::decode_rust_punycode', 'absl::demangle_rust', 'absl::utf8_for_code_point', 'absl::leak_check', 'absl::debugging', 'absl::flags_path_util', 'absl::flags_program_name', 'absl::flags_config', 'absl::flags_marshalling', 'absl::flags_commandlineflag_internal', 'absl::flags_commandlineflag', 'absl::flags_private_handle_accessor', 'absl::flags_reflection', 'absl::flags_internal', 'absl::flags', 'absl::flags_usage_internal', 'absl::flags_usage', 'absl::flags_parse', 'absl::any_invocable', 'absl::bind_front', 'absl::function_ref', 'absl::overload', 'absl::hash', 'absl::city', 'absl::weakly_mixed_integer', 'absl::log_internal_check_impl', 'absl::log_internal_check_op', 'absl::log_internal_conditions', 'absl::log_internal_config', 'absl::log_internal_flags', 'absl::log_internal_format', 'absl::log_internal_globals', 'absl::log_internal_log_impl', 'absl::log_internal_proto', 'absl::log_internal_message', 'absl::log_internal_log_sink_set', 'absl::log_internal_nullguard', 'absl::log_internal_nullstream', 'absl::log_internal_strip', 'absl::log_internal_voidify', 'absl::log_internal_append_truncated', 'absl::absl_check', 'absl::absl_log', 'absl::check', 'absl::die_if_null', 'absl::log_flags', 'absl::log_globals', 'absl::log_initialize', 'absl::log', 'absl::log_entry', 'absl::log_sink', 'absl::log_sink_registry', 'absl::log_streamer', 'absl::log_internal_structured', 'absl::log_internal_structured_proto', 'absl::log_structured', 'absl::vlog_config_internal', 'absl::absl_vlog_is_on', 'absl::vlog_is_on', 'absl::log_internal_fnmatch', 'absl::memory', 'absl::type_traits', 'absl::meta', 'absl::bits', 'absl::int128', 'absl::numeric', 'absl::numeric_representation', 'absl::sample_recorder', 'absl::exponential_biased', 'absl::periodic_sampler', 'absl::profile_builder', 'absl::hashtable_profiler', 'absl::random_random', 'absl::random_bit_gen_ref', 'absl::random_internal_mock_helpers', 'absl::random_distributions', 'absl::random_seed_gen_exception', 'absl::random_seed_sequences', 'absl::random_internal_traits', 'absl::random_internal_distribution_caller', 'absl::random_internal_fast_uniform_bits', 'absl::random_internal_seed_material', 'absl::random_internal_entropy_pool', 'absl::random_internal_salted_seed_seq', 'absl::random_internal_iostream_state_saver', 'absl::random_internal_generate_real', 'absl::random_internal_wide_multiply', 'absl::random_internal_fastmath', 'absl::random_internal_nonsecure_base', 'absl::random_internal_pcg_engine', 'absl::random_internal_randen_engine', 'absl::random_internal_platform', 'absl::random_internal_randen', 'absl::random_internal_randen_slow', 'absl::random_internal_randen_hwaes', 'absl::random_internal_randen_hwaes_impl', 'absl::random_internal_distribution_test_util', 'absl::random_internal_uniform_helper', 'absl::status', 'absl::statusor', 'absl::string_view', 'absl::strings', 'absl::charset', 'absl::has_ostream_operator', 'absl::strings_internal', 'absl::str_format', 'absl::str_format_internal', 'absl::cord_internal', 'absl::cordz_update_tracker', 'absl::cordz_functions', 'absl::cordz_statistics', 'absl::cordz_handle', 'absl::cordz_info', 'absl::cordz_sample_token', 'absl::cordz_update_scope', 'absl::cord', 'absl::graphcycles_internal', 'absl::kernel_timeout_internal', 'absl::synchronization', 'absl::time', 'absl::civil_time', 'absl::time_zone', 'absl::any', 'absl::span', 'absl::optional', 'absl::variant', 'absl::compare', 'absl::bad_any_cast', 'absl::bad_optional_access', 'absl::bad_variant_access', 'absl::utility', 'utf8_range::utf8_validity', 'utf8_range::utf8_range',
# 'protobuf::libprotobuf-lite', 'protobuf::libprotobuf', 'protobuf::libprotoc', 'protobuf::libupb', 'protobuf::protoc', 'protobuf::protoc-gen-upb', 'protobuf::protoc-gen-upbdefs', 'protobuf::protoc-gen-upb_minitable']


# TODO+ discuss with slava about absl warning that upstream is working on
    # https://github.com/google-ai-edge/mediapipe/issues/5371
    # https://github.com/grpc/grpc/issues/38703
    # https://github.com/abseil/abseil-cpp/issues/1656
    # https://github.com/grpc/proposal/blob/master/L117-core-replace-gpr-logging-with-abseil-logging.md
# TODO+ discuss with slava about --experimental_editions flag needed (editions_2023 in grpc_protoc vs editions_2024 in protobuf)
    # https://github.com/grpc/grpc/issues/15675
    # https://github.com/grpc-ecosystem/grpc-gateway/pull/5723
    # https://stackoverflow.com/questions/79193781/grpc-tools-protoc-compiling-grpc-with-edition-2023-fails
    # https://stackoverflow.com/questions/34713861/python-grpc-protobuf-stubs-generation-issue-grpc-out-protoc-gen-grpc-plugin
    # https://www.google.com/search?q=Built-in+generator+--grpc_python_out+specifies+a+maximum+edition+2023+which+is+not+the+protoc+maximum+2024.+experimental_editions&sca_esv=e70896b8d61e9a78&sxsrf=AE3TifO8PjHHJURHWYZ0JubwqOyeuVAtZA%3A1755987305474&ei=aT2qaJjlHKeyptQPme7niAI&ved=0ahUKEwjYvcq8-qGPAxUnmYkEHRn3GSEQ4dUDCBA&uact=5&oq=Built-in+generator+--grpc_python_out+specifies+a+maximum+edition+2023+which+is+not+the+protoc+maximum+2024.+experimental_editions&gs_lp=Egxnd3Mtd2l6LXNlcnAigQFCdWlsdC1pbiBnZW5lcmF0b3IgLS1ncnBjX3B5dGhvbl9vdXQgc3BlY2lmaWVzIGEgbWF4aW11bSBlZGl0aW9uIDIwMjMgd2hpY2ggaXMgbm90IHRoZSBwcm90b2MgbWF4aW11bSAyMDI0LiBleHBlcmltZW50YWxfZWRpdGlvbnNIAFAAWABwAHgBkAEAmAEAoAEAqgEAuAEDyAEA-AEB-AECmAIAoAIAmAMAkgcAoAcAsgcAuAcAwgcAyAcA&sclient=gws-wiz-serp
    # https://github.com/grpc/grpc/blob/master/src/compiler/python_generator.h
    # https://protobuf.dev/reference/cpp/api-docs/google.protobuf.compiler.plugin/
    # https://grpc.io/docs/languages/python/basics/#generating-client-and-server-code
    # https://stackoverflow.com/questions/62649353/difference-between-protoc-and-python-m-grpc-tools-protoc
    # https://github.com/protocolbuffers/protobuf/blob/main/python/README.md
    # https://github.com/google/protobuf-gradle-plugin/pull/768

# TODO+ also ben reminder to give ryan example test files i created for grpcio_tools and grpcio


# TODO+ delete after discussing with slava. they copy sources to this directory in setup.py but it isnt necessary
# # === @begin: Copy gRPC sources ===
# copy_grpc_sources = custom_target(
#     'copy_grpc_sources',
#     output: ['grpc_root_copied.stamp'],
#     command: [
#         py.full_path(),
#         meson.current_source_dir() / 'eugo_copy_sources.py',
#     ],
#     build_by_default: true,
#     console: true,
# )
# # === @end: Copy gRPC sources ===


# # === @begin: _protoc_compiler.so ===

# # Create variables for the copied source files that depend on the custom target
# no_op_grpc_compiler_sources = custom_target(
#     'no_op_grpc_compiler_sources',
#     input: copy_grpc_sources,
#     output: ['no_op'],
#     command: [
#         'echo', 'Copy completed successfully', '>', '@OUTPUT@'
#     ],
#     depends: copy_grpc_sources,
#     build_by_default: true,
# )

root = '../../../..' # TODO+

py.extension_module(
    '_protoc_compiler',
    [
        # Cython source
        'grpc_tools/_protoc_compiler.pyx',

        # C / C++ sources - # NOTE: we need to use original files directly because meson evaluates all paths at configuration time
        '../../../../src/compiler/python_generator.cc',
        '../../../../src/compiler/proto_parser_helper.cc',
        # '../../../../src/compiler/python_plugin.cc',
        'grpc_tools/main.cc',
    ],
    # c_args: [], if you also need macros for C, copy them here OR (better) - just define them as a list before this invocation and pass this list to both `c_args` and `cpp_args`.
    cpp_args: [
        # '-DHAVE_PTHREAD',
        # This macro is only relevant for `_protoc_compiler.cpp` and we use cython sources
    ],
    include_directories: [
        # System
        '/usr/local/include/',

        # Cython
        'grpc_tools/',

        # C / C++ sources - # NOTE: these are needed because meson evaluates include directories at configuration time
        '.',  # This is included automatically as a folder of the current `meson.build` file but we want to be explicit!
        '../../../../',                   # gRPC root directory
        '../../../../include/',           # Original gRPC headers
        '../../../../src/',               # Original src sources
        '../../../../src/compiler/',      # Original compiler sources
    ],
    dependencies: [
        # Package-managed
        grpc,
        protobuf,
        utf8_range,
        absl,
    ],
    override_options : ['cython_language=cpp'], # See: https://github.com/mesonbuild/meson/blob/master/mesonbuild/compilers/cython.py and https://github.com/mesonbuild/meson/blob/master/mesonbuild/build.py#L995-L1003
    cython_args: ['--3str'], # NOTE: we need '3str' otherwise we error out, See: https://github.com/mesonbuild/meson/blob/master/test%20cases/cython/3%20cython_args/meson.build

    install: true,
)
# === @end: _protoc_compiler.so ===


# === @begin: Pure Python ===
# `py.get_install_dir()` -> site_packages!
install_data(
    [
        'grpc_tools/__init__.py',
        'grpc_tools/command.py',
        'grpc_tools/grpc_version.py',
        'grpc_tools/protoc.py',
        'grpc_tools/python_version.py',
    ],

    install_dir: py.get_install_dir() / 'grpc_tools',
    install_tag: 'python-runtime',
)
# -> py.get_install_dir() / 'grpc_tools' / '__init__.py'
# -> py.get_install_dir() / 'grpc_tools' / 'command.py'
# -> py.get_install_dir() / 'grpc_tools' / 'grpc_version.py'
# -> py.get_install_dir() / 'grpc_tools' / 'protoc.py'
# -> py.get_install_dir() / 'grpc_tools' / 'python_version.py'

# -> py.get_install_dir() / 'grpc_tools' /
install_subdir(
    'grpc_tools/_proto/',

    install_dir: py.get_install_dir() / 'grpc_tools',
    install_tag: 'python-runtime',

    exclude_files: [
        # other buildtime/auxtime stuff
        'README.rst',

        # **/BUILD.bazel
        'BUILD.bazel',
    ]
)
# === @end: Pure Python ===
