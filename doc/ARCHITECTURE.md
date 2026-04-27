# Wraith — Architecture

**English** · [Português (Brasil)](ARCHITECTURE.pt-BR.md)

This document is the technical orientation for engineers reading the
codebase or adding a new technique. It mirrors what is in
[`README.md`](../README.md) but goes deeper on the internal
contracts.

---

## Layered overview

```
┌──────────────────────────────────────────────────────────────┐
│ Layer 5  │ Public API  │ include/wraith/*.h  │
│  │  │ wraith_load_library, wraith_get_proc_address │
│  │  │ wraith_free_library, wraith_sleep, ...  │
├──────────┼──────────────┼─────────────────────────────────────┤
│ Layer 4  │ Loader  │ src/loader/loader_pipeline.c  │
│  │ pipeline  │ orchestrates 17-step load  │
├──────────┴──┬───────────┴───────────┬─────────────────────────┤
│ Layer 3a  │ Layer 3b  │ Layer 3c  │
│ PE parser  │ Mapping vtable  │ Stealth hooks  │
│ src/pe/*  │ src/mapping/*  │ src/stealth/*  │
├─────────────┴───────────────┬───────┴─────────────────────────┤
│ Layer 2 │ Runtime  │ src/runtime/*  │
│  │  │ rt_ops vtable: load_library,  │
│  │  │ get_proc, free_library,  │
│  │  │ nt_alloc, nt_protect, nt_free,  │
│  │  │ nt_flush_icache  │
├─────────┴───────────────────┼─────────────────────────────────┤
│ Layer 1 │ Syscall engine  │ src/syscalls/* — Hell's Hall  │
│  │  │ + Halo's Gate + 8 Nt* stubs  │
├─────────┴───────────────────┴─────────────────────────────────┤
│ Layer 0 │ Intrinsics, hashes, math primitives  │
└────────────────────────────────────────────────────────────────┘
```

Each layer can only call **into** layers below it. The public API
never includes `<windows.h>` (except `wraith_resource.h` where the
surface requires it). All Windows-specific code lives below layer 6.

---

## The three pivotal vtables

The whole project hinges on three function-pointer tables. New
techniques almost always plug into one of them — touching
`loader_pipeline.c` is rare.

### `wr_map_ops` (layer 3b)

Defined in `src/mapping/map_strategy.h`. Implemented by:

| Strategy | File | Default | Purpose |
|----------|------|---------|---------|
| `WRAITH_MAP_PRIVATE_RW_RX` | `map_private_rwx.c` | yes | `NtAllocate*` + RW→RX hygiene |
| `WRAITH_MAP_PHANTOM_HOLLOW` | `map_phantom.c` | opt-in | `NtCreateSection(SEC_IMAGE)` host backing |
| `WRAITH_MAP_MODULE_STOMPING` | `map_stomping.c` | opt-in | `LoadLibraryW` host + byte backup/restore |

`map_dispatch.c` resolves the requested strategy enum to its vtable
at load time. Each vtable ships `reserve / commit / protect /
release / destroy`.

### `wr_rt_ops` (layer 2)

Defined in `src/runtime/rt_api.h`. Two implementations:

| Vtable | File | Selection | Notes |
|--------|------|-----------|-------|
| `wr_rt_ops_baseline` | `rt_api_baseline.c` | when `WRAITH_F_API_HASHING` is OFF | Wraps Win32 `LoadLibraryA` / `VirtualAlloc` / etc. |
| `wr_rt_ops_ntapi` | `rt_api_ntapi.c` | when `WRAITH_F_API_HASHING` is ON | Walks PEB.Ldr; routes memory ops through Hell's Hall when `WRAITH_F_INDIRECT_SYSCALLS` is set |

Both expose: `load_library / get_proc / free_library / nt_alloc /
nt_protect / nt_free / nt_flush_icache`.

### `wr_stealth_hooks` (layer 3c, scaffolded)

A chain of `post_map / post_load / pre_unload / pre_sleep` callbacks
that the pipeline invokes at fixed phase IDs. The 1.0 release ships
the slot machinery; concrete hook chains land with future extensions.

---

## The 17-step load pipeline

Each step lives in a dedicated file under `src/loader/` and is
tagged with the same numeric ID that `wr_trace` emits. Skipping a
step means flipping its compile-time flag — the pipeline is
defensive about empty / missing data.

| # | Step | File | Active |
|---|------|------|--------|
| 1 | PE bounds + magic validation | `loader_pipeline.c` calls `pe_validate.c` | always |
| 2 | Image metrics (`SizeOfImage`, last section end) | `pe_image_metrics.c` | always |
| 3 | Runtime selection | `rt_api.c` | always |
| 4 | Mapping strategy selection | `map_dispatch.c` | always |
| 5 | Reserve image | `<strategy>->reserve` | always |
| 6 | `post_map` hook (chain) | `wr_stealth_hooks` | when registered |
| 7 | Copy headers + sections | `loader_sections.c` | always |
| 8 | Base relocations (DIR64) | `loader_relocs.c` | always |
| 9 | Imports — bound (skip) → normal → delay | `loader_imports*.c` | per flag |
| 10 | Finalize per-section protections | `loader_finalize.c` | always |
| 11 | `post_protect` hook (chain) | `wr_stealth_hooks` | when registered |
| 12 | x64 SEH (`RtlAddFunctionTable`) | `loader_seh_x64.c` | `WRAITH_REGISTER_SEH_X64` |
| 13 | PEB.Ldr linkage | `peb_link.c` | `WRAITH_USE_PEB_LINKAGE` |
| 14 | TLS callbacks (ATTACH) | `loader_tls.c` | always |
| 15 | DllMain / EXE entry | `loader_entry.c` | always |
| 16 | `post_load` hook (chain) | `wr_stealth_hooks` | when registered |
| 17 | Return handle | `loader_api.c` | always |

`wr_pipeline_unwind` reverses 15 → 14 → 13 → 12 → 5 (run
detach + tls detach + peb-link remove + SEH unregister + map
release) so the unwind is symmetric.

---

## `wr_ctx` — what the pipeline carries

Defined in `src/core/wr_context_internal.h`. Every public API
that takes an `wraith_handle_t` casts back to `wr_ctx*` and validates
`magic == WRAITH_CTX_MAGIC` before dereferencing.

Notable field groups:

- **Image metadata** — `image_base`, `image_size`, `headers`,
  `image_type`, `is_relocated`, `initialized`.
- **Imports** — `imported_modules`, `imported_owned`,
  `imported_count`. Forwarders, normal imports, and delay imports
  all land in the same arrays.
- **Mapping** — `map_ops`, `map_state`. Strategy-specific
  state lives behind the opaque `map_state` pointer (e.g.
  `phantom_state` for SEC_IMAGE, `stomping_state` for backup/host).
- **Runtime / syscalls** — `rt_ops`, `ntdll_base`, `kernel32_base`,
  `sc_engine`.
- **Stealth slots** — `peb_ldr_entry`, `masquerade_name`,
  `masquerade_path`, `sleep_key`, `runtime_funcs`,
  `functbl_registered`.
- **Diagnostics** — per-thread `last_status`, `err_context`,
  `trace`/`trace_userdata` callbacks for pipeline phase events.

---

## Adding a new technique — the 5-step contract

1. Create `src/<category>/<name>.{c,h}` with the implementation.
2. Add a `WRAITH_USE_<NAME>` option to `cmake/options.cmake`.
3. List the source file in `CMakeLists.txt` under
  `if(WRAITH_USE_<NAME>)`.
4. Wire into the appropriate vtable / pipeline hook
  (`map_dispatch.c`, `rt_api.c`, or `loader_pipeline.c` step 6/11/16).
5. Add an integration test under
  `tests/integration/test_<feature>.c` and register it in
  `tests/integration/CMakeLists.txt`.

The codebase enforces this layout convention; PRs that touch
`loader_pipeline.c` get extra scrutiny.

---

## File map cheat-sheet

```
include/wraith/  Public API headers
src/core/  wr_ctx lifecycle, status strings
src/pe/  Bounds-checked PE parser, iterators
src/loader/  17-step pipeline + section/reloc/imports/tls/seh/entry/finalize
src/exports/  Binary-search lookup + forwarder follower
src/resource/  Three-level resource walker
src/runtime/  rt_ops vtables (baseline, ntapi)
  PEB walker, hash-based export resolver
src/syscalls/  Hell's Hall + Halo's Gate, asm trampoline
src/mapping/  map_ops vtables (private_rwx, phantom, stomping)
src/stealth/peb_link/  PEB.Ldr insertion with masquerade
src/stealth/sleep/  XOR sleep obfuscation (Ekko alias)
src/stealth/unhook/  ntdll disk-refresh
src/stealth/etw/  EtwEventWrite hot-patch
src/stealth/amsi/  AmsiScanBuffer hot-patch
src/stealth/hashing/  Case-insensitive DJB2 ASCII + UTF-16
tests/integration/  End-to-end tests under wine64 / Win10+
tests/integration/fixtures/  Tiny DLLs (forwarder, SEH) used by tests
tools/  hashgen.py + api_list.txt + ioc_audit
ci/github-actions/  build-mingw, build-msvc, test-wine
```
