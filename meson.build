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
    version: '1.75.0.dev0' # Must match pyproject.toml#project
)
# === @end: General ===


# === @begin: Meson modules imports ===
py = import('python').find_installation(pure: false)
# === @end: Meson modules imports ===


# === @begin: Dependencies ===
# grpc = dependency('gRPC', method: 'cmake', modules: ['gRPC::gpr', 'gRPC::grpc']) # TODO+ Run-time dependency grpc (modules: gRPC::gpr, gRPC::grpc) found: YES 8.11.0-DEV
# openssl = dependency('openssl')
# zlib = dependency('zlib-ng', method: 'cmake', modules: ['zlib-ng::zlib'])
# cares = dependency('c-ares', method: 'cmake', modules: ['c-ares::cares'])
# absl = dependency('absl', method: 'cmake', modules: ['absl::core_headers'])
# re2 = dependency('re2', method: 'cmake')
# === @end: Dependencies ===



# === @begin: cygrpc.so ===
py.extension_module(
    'cygrpc',
    [
        'src/python/grpcio/grpc/_cython/cygrpc.pyx',
    ],
    cpp_args: [
        '-DPyMODINIT_FUNC=extern "C" __attribute__((visibility ("default"))) PyObject*',
        # '-DPyMODINIT_FUNC=extern \\"C\\" __attribute__((visibility (\\"default\\"))) PyObject*',
        #'-D__STDC_FORMAT_MACROS',
    ],
    include_directories: [
        'src/python/grpcio/grpc/_cython/',
        'src/python/grpcio/',
        'src/core/ext/upb-gen/',
        'src/core/ext/upbdefs-gen/',
    ],
    install: true,
    subdir: 'grpc/_cython/',
    dependencies: [
        # Package-managed
        # grpc,
        # openssl,
        # zlib,
        # cares,
        # absl,
        # re2
    ],
    override_options : ['cython_language=cpp'], # See: https://github.com/mesonbuild/meson/blob/master/mesonbuild/compilers/cython.py and https://github.com/mesonbuild/meson/blob/master/mesonbuild/build.py#L995-L1003
    cython_args: ['--3str'], # See: https://github.com/mesonbuild/meson/blob/master/test%20cases/cython/3%20cython_args/meson.build
)
# === @end: cygrpc.so ===


# === @begin: Pure Python ===
install_subdir(
    'src/python/grpcio/grpc/',
    install_dir: py.get_install_dir(),
    install_tag: 'python-runtime',

    exclude_directories: [
        'protobuf/internal/import_test_package/',
        'protobuf/internal/numpy/',
    ],

    exclude_files: [
        '__init__.py',

        'protobuf/link_error_fast_cpp.cc',
        'protobuf/link_error_pure_python.cc',
        'protobuf/link_error_upb.cc',
        'protobuf/proto_api.h',
        'protobuf/python_protobuf.h',
        'protobuf/use_fast_cpp_protos.cc',
        'protobuf/use_pure_python.cc',
        'protobuf/use_upb_protos.cc',

        'protobuf/any.py',
        'protobuf/duration.py',
        'protobuf/timestamp.py',

        # C++ bindings (fast cpp)
        'protobuf/pyext/descriptor.cc',
        'protobuf/pyext/descriptor.h',
        'protobuf/pyext/descriptor_containers.cc',
        'protobuf/pyext/descriptor_containers.h',
        'protobuf/pyext/descriptor_database.cc',
        'protobuf/pyext/descriptor_database.h',
        'protobuf/pyext/descriptor_pool.cc',
        'protobuf/pyext/descriptor_pool.h',
        'protobuf/pyext/extension_dict.cc',
        'protobuf/pyext/extension_dict.h',
        'protobuf/pyext/field.cc',
        'protobuf/pyext/field.h',
        'protobuf/pyext/map_container.cc',
        'protobuf/pyext/map_container.h',
        'protobuf/pyext/message.cc',
        'protobuf/pyext/message.h',
        'protobuf/pyext/message_factory.cc',
        'protobuf/pyext/message_factory.h',
        'protobuf/pyext/message_module.cc',
        'protobuf/pyext/python.proto',
        'protobuf/pyext/README',
        'protobuf/pyext/repeated_composite_container.cc',
        'protobuf/pyext/repeated_composite_container.h',
        'protobuf/pyext/repeated_scalar_container.cc',
        'protobuf/pyext/repeated_scalar_container.h',
        'protobuf/pyext/safe_numerics.h',
        'protobuf/pyext/scoped_pyobject_ptr.h',
        'protobuf/pyext/unknown_field_set.cc',
        'protobuf/pyext/unknown_field_set.h',

        'protobuf/internal/any_test.py',
        'protobuf/internal/api_implementation.cc',
        'protobuf/internal/decoder_test.py',
        'protobuf/internal/descriptor_database_test.py',
        'protobuf/internal/descriptor_pool_test.py',
        'protobuf/internal/descriptor_pool_test1.proto',
        'protobuf/internal/descriptor_pool_test2.proto',
        'protobuf/internal/descriptor_test.py',
        'protobuf/internal/duration_test.py',
        'protobuf/internal/factory_test1.proto',
        'protobuf/internal/factory_test2.proto',
        'protobuf/internal/field_mask_test.py',
        'protobuf/internal/file_options_test.proto',
        'protobuf/internal/generator_test.py',
        'protobuf/internal/import_test.py',
        'protobuf/internal/json_format_test.py',
        'protobuf/internal/keywords_test.py',
        'protobuf/internal/message_factory_test.py',
        'protobuf/internal/message_set_extensions.proto',
        'protobuf/internal/message_test.py',
        'protobuf/internal/missing_enum_values.proto',
        'protobuf/internal/more_extensions.proto',
        'protobuf/internal/more_extensions_dynamic.proto',
        'protobuf/internal/more_messages.proto',
        'protobuf/internal/no_package.proto',
        'protobuf/internal/packed_field_test.proto',
        'protobuf/internal/proto_builder_test.py',
        'protobuf/internal/proto_json_test.py',
        'protobuf/internal/proto_test.py',
        'protobuf/internal/pybind11_test_module.cc',
        'protobuf/internal/python_edition_defaults.py.template',
        'protobuf/internal/python_protobuf.cc',
        'protobuf/internal/recursive_message_pybind11_test.py',
        'protobuf/internal/reflection_cpp_test.py',
        'protobuf/internal/reflection_test.py',
        'protobuf/internal/runtime_version_test.py',
        'protobuf/internal/self_recursive.proto',
        'protobuf/internal/self_recursive_from_py.proto',
        'protobuf/internal/service_reflection_test.py',
        'protobuf/internal/symbol_database_test.py',
        'protobuf/internal/test_bad_identifiers.proto',
        'protobuf/internal/test_proto2.proto',
        'protobuf/internal/test_proto3_optional.proto',
        'protobuf/internal/test_util.py',
        'protobuf/internal/text_encoding_test.py',
        'protobuf/internal/text_format_test.py',
        'protobuf/internal/thread_safe_test.py',
        'protobuf/internal/timestamp_test.py',
        'protobuf/internal/unknown_fields_test.py',
        'protobuf/internal/well_known_types_test.proto',
        'protobuf/internal/well_known_types_test.py',
        'protobuf/internal/wire_format_test.py',
    ]
)
# === @end: Pure Python ===
