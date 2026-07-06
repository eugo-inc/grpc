---
name: eugo-wheel-validation
description: Validate the Eugo Meson-built grpcio wheel against the upstream PyPI wheel - the primary correctness gate. Covers the comparison setup, file-listing diff, pure-Python content diff, exported-symbol diff (PyInit_cygrpc), NEEDED-library diff, metadata diff, the five runtime smoke tests (6a-6e), the expected-vs-suspicious differences table, and when to run. Activates on "validate the grpcio wheel", "wheel comparison", "compare against upstream wheel", "grpc smoke tests", "symbol diff", "PyInit_cygrpc", "wheel validation gate".
---

# Eugo gRPC: wheel validation against upstream

Comparing our Meson-built wheel against the upstream `grpcio` wheel from PyPI
is the primary correctness gate. If the two wheels contain the same
pure-Python files, export the same native symbols, and pass the same runtime
smoke tests, then our build is correct. Run all steps manually from the repo
root.

## When to run

- **After every upstream merge** - before pushing the merge branch
- **After any `meson.build` change** - to catch install rule regressions
- **Before tagging a release** - final validation gate

## Obtaining the two wheels

```bash
# Shared directories - all under /tmp/eugo/ to keep things organised
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
#    Auto-detect architecture: aarch64 -> manylinux_2_17_aarch64, x86_64 -> manylinux_2_17_x86_64.
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

## Step 1: Unpack and diff file listings

```bash
unzip -o "${EUGO_EUGO_WHEEL_DIR}"/grpcio-*.whl -d "${EUGO_EUGO_WHEEL_DIR}"
unzip -o "${EUGO_UPSTREAM_WHEEL_DIR}"/grpcio-*.whl -d "${EUGO_UPSTREAM_DIR}"

# cd into grpc/ subdirectory so paths are comparable (both start with ./aio/... etc.)
# Exclude __pycache__ / .pyc - wheels don't ship them; they appear only in --target installs.
(cd "${EUGO_EUGO_WHEEL_DIR}/grpc" && find . -type f | grep -v '__pycache__' | sort) > /tmp/eugo/eugo_files.txt
(cd "${EUGO_UPSTREAM_DIR}/grpc"   && find . -type f | grep -v '__pycache__' | sort) > /tmp/eugo/upstream_files.txt

diff /tmp/eugo/eugo_files.txt /tmp/eugo/upstream_files.txt
```

**Expected result**: The file listings should be identical. Every `.py` file,
`roots.pem`, `__init__.py`, and `cygrpc` shared object present in the upstream
wheel must also be present in ours. Any missing file indicates a gap in
`meson.build`'s `install_subdir` / `install_data` rules.

## Step 2: Diff pure-Python file contents

```bash
# Compare every .py file byte-for-byte (excluding _grpcio_metadata.py which we hardcode)
diff -rq "${EUGO_EUGO_WHEEL_DIR}/grpc/" "${EUGO_UPSTREAM_DIR}/grpc/" \
    --exclude='*.so' \
    --exclude='*.dylib' \
    --exclude='_grpcio_metadata.py' \
    --exclude='__pycache__'
```

**Expected result**: No differences. Our Meson build installs pure-Python
files directly from the source tree, which should be identical to upstream.
If upstream has added, removed, or modified `.py` files since our fork point,
those changes must be merged first.

`_grpcio_metadata.py` is excluded because upstream auto-generates it while we
hardcode the version string - but the version value itself must match.

## Step 3: Compare native extension exported symbols

The `cygrpc` shared object will differ at the binary level (different compiler
flags, linking strategy), but the **exported symbol set** must be equivalent.
The only required export is the CPython module init function.

```bash
# Use -DC for demangled output
nm -DC "${EUGO_EUGO_WHEEL_DIR}"/grpc/_cython/cygrpc*.so  | grep ' T ' | awk '{print $3}' | sort > /tmp/eugo/eugo_symbols.txt
nm -DC "${EUGO_UPSTREAM_DIR}"/grpc/_cython/cygrpc*.so     | grep ' T ' | awk '{print $3}' | sort > /tmp/eugo/upstream_symbols.txt

diff /tmp/eugo/eugo_symbols.txt /tmp/eugo/upstream_symbols.txt
```

**Expected result**: The symbol sets will differ dramatically - this is
expected and correct.

**Eugo** exports only `PyInit_cygrpc` (and possibly `_fini`/`_init`).
Everything else appears as `U` (undefined - resolved from shared libs at
runtime). This is the correct behaviour for dynamic linking against system
`libgrpc`.

**Upstream** exports 300+ symbols as `T` (text/defined): all of c-ares
(`ares_*`), `std::` exception constructors, abseil internals, and more. This
is because upstream's `setup.py` statically bundles the entire gRPC C core,
c-ares, abseil, BoringSSL, and parts of libstdc++ into `cygrpc.so` - making
it a self-contained binary with no external C++ dependencies.

The key check is that **`PyInit_cygrpc` is present as a `T` symbol** in our
wheel. Its absence would mean the module init function isn't exported (broken
`-fvisibility` or missing `PyMODINIT_FUNC` override). Everything else being
`U` is expected.

If upstream adds *new* `T` symbols beyond the usual bundled set, investigate
whether `setup.py` added new extension modules or changed visibility flags.

## Step 4: Compare dynamic library dependencies

```bash
# Check what shared libraries each cygrpc links against
readelf -d "${EUGO_EUGO_WHEEL_DIR}"/grpc/_cython/cygrpc*.so  | grep NEEDED | awk '{print $5}' | sort > /tmp/eugo/eugo_needed.txt
readelf -d "${EUGO_UPSTREAM_DIR}"/grpc/_cython/cygrpc*.so     | grep NEEDED | awk '{print $5}' | sort > /tmp/eugo/upstream_needed.txt

diff /tmp/eugo/eugo_needed.txt /tmp/eugo/upstream_needed.txt
```

**Expected result**: These will intentionally differ. Upstream bundles all
C/C++ dependencies statically into `cygrpc.so` (BoringSSL, abseil, c-ares,
etc.), so it typically has very few `NEEDED` entries (just `libc`,
`libpthread`, `libm`, etc.). Our wheel dynamically links against system
`libgrpc.so`, `libgpr.so`, `libabsl_*.so`, etc. - this is by design. The key
check is that our wheel **does** list `libgrpc.so` and `libgpr.so` as
`NEEDED` dependencies.

## Step 5: Compare package metadata

```bash
diff "${EUGO_EUGO_WHEEL_DIR}"/grpcio-*.dist-info/METADATA \
     "${EUGO_UPSTREAM_DIR}"/grpcio-*.dist-info/METADATA
```

**Key fields to verify**:
- `Name`: must be `grpcio` in both
- `Version`: must match
- `Requires-Dist`: our `typing-extensions~=4.12` must match upstream's runtime
  dependencies. If upstream adds new dependencies, update `pyproject.toml`.

## Step 6: Runtime smoke tests

Run smoke tests against the files already installed by the setup step above -
no separate venv needed. All five tests must pass.

```bash
# Point Python at the installed files from the comparison setup step
export PYTHONPATH="${EUGO_INSTALLED_DIR}"
```

| Test | What it validates |
|---|---|
| 6a | Native extension loads, version correct, `roots.pem` present, `grpc.aio` available |
| 6b | Generic (non-proto) unary RPC roundtrip - exercises core gRPC Python API |
| 6c | `protoc` + `grpc_python_plugin` code generation produces valid Python modules |
| 6d | Synchronous client-server roundtrip with proto-generated stubs |
| 6e | AsyncIO client-server roundtrip with proto-generated stubs |

### Test 6a: Import and basic API surface

Verifies the native extension loads, the version is correct, and fundamental
gRPC objects can be created.

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

### Test 6b: Generic (non-proto) unary RPC roundtrip

End-to-end test that creates a server, sends a request, and validates the
response - all without protobuf. This exercises `grpc.server()`,
`grpc.method_handlers_generic_handler()`,
`grpc.unary_unary_rpc_method_handler()`, `grpc.insecure_channel()`, and
`channel.unary_unary()`.

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

### Test 6c: Protoc + grpc_python_plugin code generation

Verifies that the native `protoc` binary and `grpc_python_plugin` (installed
by CMake) can compile a `.proto` file and produce working Python stubs. This
is the Eugo replacement for `grpcio_tools` (see `eugo-grpcio-tools-migration`).

**Prerequisites**: `protoc` and `grpc_python_plugin` must be on `$PATH`
(installed by `cmake --install build`).

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

### Test 6d: Full client-server roundtrip with generated stubs

End-to-end test using proto-generated stubs. Combines code generation (Test
6c) with a real RPC call.

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

### Test 6e: AsyncIO client-server roundtrip

Same as Test 6d but using `grpc.aio` - validates that the async API works
end-to-end.

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

After the tests:

```bash
unset PYTHONPATH
```

## What to do when differences are found

| Difference | Likely cause | Action |
|---|---|---|
| Missing `.py` files in Eugo wheel | Upstream added new Python modules | Update `install_subdir` / `install_data` in `meson.build`, or merge upstream changes |
| Extra files in Eugo wheel | Stale files or build artifacts included | Add exclusions to `meson.build`'s `exclude_files` / `exclude_directories` |
| Missing `roots.pem` | `install_data` for `etc/roots.pem` broken | Fix the `install_data()` call in `meson.build` |
| `.py` file content differs | Upstream modified Python source since fork point | Merge upstream (`git merge upstream/master`) |
| Different exported symbols | Visibility flags or new extension modules | Review `meson.build` cpp_args and check if upstream added new `.pyx` files |
| `cygrpc` fails to load | Linking issue - missing `libgrpc.so` at runtime | Ensure `libgrpc` and `libgpr` are installed; check `LD_LIBRARY_PATH` or `ldconfig` |
| Version mismatch | `_grpcio_metadata.py` or `grpc_version.py` out of date | Update version to match upstream after merge |
| New `Requires-Dist` in upstream | Upstream added a runtime Python dependency | Add it to `pyproject.toml` `[project] dependencies` |

## Related

- `eugo-build-and-test` - the build flows that produce the wheel being validated.
- `eugo-meson-build-review` - pre-commit checklist that mandates this gate.
- `eugo-upstream-merge` - run this comparison after every upstream merge.
- `eugo-grpcio-tools-migration` - the protoc/grpc_python_plugin path that 6c-6e exercise.
