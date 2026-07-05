---
name: eugo-upstream-merge
description: Merge upstream grpc/grpc:master into the Eugo fork (eugo-inc/grpc, branch master). Covers the fork's divergence inventory, the merge-commit-only recipe, and the protomolecule commit-pin bump. Activates on "merge upstream", "upstream sync", "catch up to grpc master", "grpc fork merge", "bump grpc pin".
---

# Eugo gRPC fork: upstream merge

## What this fork is / who consumes it

Fork of https://github.com/grpc/grpc. Canonical branch is `master` (no eugo-main).
Two protomolecule packages consume it, both pinned by **git_commit** in `meta.json`
(`version.kind: git_commit`, `branch: master`, `should_auto_update: true`):

- `protomolecule/dependencies/native/grpc` - CMake build of the C/C++ libs against
  system deps (absl, protobuf, c-ares, zlib-ng, openssl, re2, opencensus-cpp;
  boringssl/systemd/otel deliberately off). See its `setup` for the exact cmake args.
- `protomolecule/dependencies/python/wave_4/grpcio` - Meson build of `cygrpc.so`
  linking system libgrpc (the Eugo-added root `meson.build` + `pyproject.toml`).

Commit pins decouple merge from adoption: merging upstream into fork master does NOT
change what protomolecule builds until the meta.json pins are bumped.

## Eugo divergence inventory

The authoritative catalog lives in `.github/copilot-instructions.md` (tables
"New files" / "Modified upstream files" + per-file conflict-resolution rules).
Read it before resolving conflicts. Summary:

- Eugo-created (no upstream counterpart, never conflict): root `meson.build` and
  `pyproject.toml` (meson-python backend for grpcio), `src/python/grpcio_observability/
  meson.build` + `pyproject.toml`, `tools/distrib/python/grpcio_tools/meson.build`
  (experimental reference only - grpcio_tools is NOT built; native protoc +
  grpc_python_plugin replace it), `.claude/skills/{meson,bazel}`, `.devcontainer/`.
- `@EUGO_CHANGE` blocks in upstream files: `setup.py` (exclude upb/address_sorting/
  utf8_range when BUILD_WITH_SYSTEM_GRPC; guard absl `initialize.cc` under
  BUILD_WITH_SYSTEM_ABSL), `src/python/grpcio/commands.py` (use our C/C++ flags),
  `src/core/util/log.cc` (add `absl/log/vlog_is_on.h` include - drop ours if
  upstream adds it).
- Known debt: several `setup.py` / `commands.py` edits are UNMARKED (listed in the
  copilot-instructions table). Do not lose them when taking upstream hunks.

## Merge recipe

```bash
git clone git@github.com:eugo-inc/grpc.git && cd grpc
git remote add upstream https://github.com/grpc/grpc.git
git fetch upstream
git checkout -b <user>/feat/MM-DD-YY-merge-upstream master   # prior art: bwl1289/chore/merge-upstream
git merge upstream/master        # merge, NOT rebase - preserve fork history
```

Conflict resolution: within `@EUGO_CHANGE` markers keep ours, elsewhere prefer
upstream; always keep the Eugo `pyproject.toml` files whole; follow the numbered
rules in `.github/copilot-instructions.md` (they cover `setup.py` DEFINE_MACROS,
`_grpcio_metadata.py` version bump, log.cc drop-if-upstreamed).

Post-merge validation (from the same doc): no leftover conflict markers, all
`@EUGO_CHANGE` intact, root `meson.build` source paths and its `setup.py:NNN`
line-number citations still correct, native cmake build passes, `pip install .`
of grpcio passes.

Then:

```bash
git push origin <branch>
# Open PR to master. Merge with a MERGE COMMIT only - never squash:
# squashing destroys upstream ancestry and makes every future sync re-conflict.
```

## Post-merge adoption

After the PR lands on `master`, bump BOTH pins to the new master merge commit:

- `protomolecule/dependencies/native/grpc/meta.json` -> `version.commit`
- `protomolecule/dependencies/python/wave_4/grpcio/meta.json` -> `version.commit`

Keep the two pins identical - grpcio's meson build links the libgrpc that
native/grpc installed; skew between them is an ABI trap. Then rebuild native/grpc
first, grpcio second (runtime dep order).

## Push-before-pin warning

Both meta.json pins are raw commit SHAs fetched by `git fetch origin <commit>` from
eugo-inc/grpc. A SHA that exists only locally (or only on an unmerged branch that
later gets squashed/deleted) breaks the protomolecule build. Push to `master` first,
verify the SHA is reachable on GitHub, then edit meta.json.
