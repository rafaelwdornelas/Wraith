# Wraith

> Modern stealth PE loader for Windows x64.

**English** · [Português (Brasil)](README.pt-BR.md)

[![Build (MinGW)](https://img.shields.io/badge/build-MinGW%20x86__64-success)](#)
[![Build (MSVC)](https://img.shields.io/badge/build-MSVC%202022-success)](#)
[![Tests (Wine)](https://img.shields.io/badge/tests-wine64%20%E2%9C%9320%2F20-blue)](#)
[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
![Platform](https://img.shields.io/badge/platform-Windows%2010%201809%2B%20%7C%20x64-informational)

Wraith loads a DLL or EXE entirely from an in-memory buffer — no
`LoadLibrary`, no temp file, no `MEM_IMAGE` written to disk. The
default profile is reliability-only (no IOC-loud features). Each
stealth technique is a separate `WRAITH_USE_*` compile flag, so the
binary you ship contains only what you opt in to.

---

## Highlights

| Indicator-of-Compromise | Wraith                                                                |
|-------------------------|-----------------------------------------------------------------------|
| `PAGE_EXECUTE_READWRITE` allocation       | **Never** — strict `RW → RX` hygiene                  |
| `LoadLibraryA` / `GetProcAddress` in IAT  | Optional — DJB2 API hashing + PEB.Ldr walk            |
| Win32 `VirtualAlloc` / `VirtualProtect`   | Optional — Hell's Hall + FreshyCalls SSN-by-RVA       |
| Unbacked `MEM_PRIVATE + RX` region        | Optional — Phantom hollowing / Module stomping / Mockingjay |
| Missing from `PEB.Ldr` lists              | Optional — full LDR linkage with masquerade name      |
| No `RtlAddFunctionTable` (x64 SEH broken) | **Always on** — `__try` / `__except` works            |
| Forwarded / delay / bound imports         | **Always on** — full import resolution                |
| `.text` plaintext while idle              | Optional — XOR / Ekko / Cronos / Page-Guard           |
| `ntdll` userland hooks                    | Optional — disk-refresh unhook **or** private mapping |
| Stack walk reveals loader origin          | Optional — SilentMoonwalk-style synthetic spoof       |
| Inline patches detectable in `.text`      | **Hardware breakpoint hooks (DR0–DR3)** — zero memory mods |
| `CreateThread` for awakener               | Optional — threadless via `TpAllocWork` hijack        |
| ETW userland telemetry                    | Optional — `EtwEventWrite` patch                      |
| AMSI scan of .NET buffers                 | Optional — `AmsiScanBuffer` short-circuit             |
| `PEB.BeingDebugged` / `NtGlobalFlag`      | Optional — passive anti-debug spoof                   |
| Mainline process heap holds artifacts     | Optional — heap masquerade (private `RtlCreateHeap`)  |

---

## Quickstart

```bash
# Cross-compile from Linux + run tests under wine64
cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/mingw-x86_64.cmake \
  -DWRAITH_PROFILE=paranoid-classic
cmake --build build -j
ctest --test-dir build --output-on-failure
```

```c
#include <wraith/wraith.h>

wraith_load_options opt = {
    .map_strategy = WRAITH_MAP_PHANTOM_HOLLOW,
    .flags        = WRAITH_F_API_HASHING
                  | WRAITH_F_INDIRECT_SYSCALLS
                  | WRAITH_F_PEB_LINKAGE
                  | WRAITH_F_SLEEP_OBFUSCATION
                  | WRAITH_F_STACK_SPOOF
                  | WRAITH_F_RELIABILITY_ALL,
    .sleep_algo   = WRAITH_SLEEP_CRONOS,
    .masquerade   = L"winnet.dll",
};

wraith_handle_t h = NULL;
wraith_status_t rc = wraith_load_library(buffer, size, &opt, &h);
if (rc != WRAITH_OK) {
    fprintf(stderr, "load failed: %s (%s)\n",
            wraith_status_string(rc), wraith_last_error());
    return 1;
}

void *fn = NULL;
wraith_get_proc_address(h, "addNumbers", &fn);
int sum = ((int (*)(int, int))fn)(2, 3);

wraith_sleep(h, 600);          // .text is encrypted, RIP redirected
wraith_free_library(h);
```

---

## Single-file distribution (`dist/`)

Sometimes you don't want to drag in CMake just to try the loader. The
project ships a 2-file amalgamated build per profile under `dist/`,
modeled after the SQLite / fancycode-MemoryModule pattern. Pick the
folder that matches the threat model you need and drop the two files
into your project:

```
dist/
├── default/           # reliability ON, stealth OFF        (~210 KiB wraith.c)
├── minimal/           # bare loader, no extras             (~210 KiB wraith.c)
├── teaching/          # every feature compiled in          (~355 KiB wraith.c)
├── paranoid-classic/  # hashing + Hell's Hall + Phantom    (~317 KiB wraith.c)
└── paranoid-full/     # bleeding-edge full chain           (~337 KiB wraith.c)
```

Each folder contains:

- `wraith.h` — the public API (everything `<wraith/wraith.h>` exposes,
  inlined into a single header).
- `wraith.c` — every TU concatenated, asm trampolines re-encoded as
  `__attribute__((naked))` C functions. No companion `.S` file required.
- `QUICKSTART.md` — three-line build instruction tailored to the profile.
- `LICENSE` — MIT.

```sh
# Drop the two files next to your loader.c, then:
x86_64-w64-mingw32-gcc -c wraith.c -o wraith.o
x86_64-w64-mingw32-gcc loader.c wraith.o -o loader.exe
```

The amalgamated artifacts are functionally equivalent to building the
canonical CMake source tree with the matching `-DWRAITH_PROFILE=` value.
To regenerate after touching `src/` or `include/`:

```sh
python3 tools/amalgamate.py             # all profiles
python3 tools/amalgamate.py paranoid-classic   # one profile
```

Caveats vs the CMake build: no `wr_api_hashes.h` codegen step (the
amalgamation relies on the in-source fallback constants), no
`-nostdlib`/CET/CFG hardening (rebuild from CMake with `WRAITH_HARDEN_*`
if you need those), and the dist path always assumes MinGW-w64 x86_64.

---

## Build profiles

| Profile             | Posture                                                          |
|---------------------|------------------------------------------------------------------|
| `default`           | Reliability fixes ON, all stealth OFF. Smallest, simplest.       |
| `teaching`          | Every feature compiled in (toggleable at runtime via flags).     |
| `paranoid-classic`  | Hashing + Hell's Hall + Phantom + Ekko + stack spoof + private ntdll. |
| `paranoid-full`     | Adds HWBP, threadless, Page-Guard, Cronos, FreshyCalls, no-CRT.  |
| `minimal`           | Reliability extras off, smallest static library.                 |

---

## Feature flags

All stealth features are opt-in. Default is OFF.

| Flag                                | Technique                                                       |
|-------------------------------------|-----------------------------------------------------------------|
| `WRAITH_USE_API_HASHING`            | DJB2 / FNV1a hashed API resolution                              |
| `WRAITH_USE_PEB_WALK`               | Resolve modules via `PEB.Ldr` (no `GetModuleHandle`)            |
| `WRAITH_USE_INDIRECT_SYSCALLS`      | Hell's Hall / Halo's Gate / FreshyCalls SSN by RVA              |
| `WRAITH_USE_PEB_LINKAGE`            | Insert into `PEB.Ldr` lists with masquerade name                |
| `WRAITH_USE_PHANTOM_HOLLOWING`      | `NtCreateSection(SEC_IMAGE)` over a host DLL                    |
| `WRAITH_USE_MODULE_STOMPING`        | Overwrite a legitimate loaded module's `.text`                  |
| `WRAITH_USE_SLEEP_OBFUSCATION`      | XOR / Ekko / Foliage / Cronos `.text` encryption while idle     |
| `WRAITH_USE_UNHOOK_NTDLL`           | Refresh `ntdll.text` from disk                                  |
| `WRAITH_USE_PRIVATE_NTDLL`          | Map a clean private copy of `ntdll` from disk                   |
| `WRAITH_USE_ETW_PATCH`              | `EtwEventWrite` → `xor eax, eax; ret`                           |
| `WRAITH_USE_AMSI_PATCH`             | `AmsiScanBuffer` → `AMSI_RESULT_CLEAN`                          |
| `WRAITH_USE_STACK_SPOOF`            | SilentMoonwalk-style synthetic-frame stack spoof                |
| `WRAITH_USE_HWBP_HOOKS`             | DR0–DR3 hardware breakpoint hooks via VEH                       |
| `WRAITH_USE_THREADLESS_EXEC`        | Hijack `TpAllocWork` / `RtlRegisterWait` instead of `CreateThread` |
| `WRAITH_USE_PAGE_GUARD_ENCRYPT`     | Lazy per-page encryption via `PAGE_GUARD` VEH                   |
| `WRAITH_USE_HEAP_MASQUERADE`        | Private heap rooted in legitimate `MEM_IMAGE`                   |
| `WRAITH_USE_ANTI_DEBUG_SPOOF`       | Passive `PEB.BeingDebugged` / `NtGlobalFlag` clear              |
| `WRAITH_USE_HOST_IAT_REDIRECT`      | Patch host process IAT slots (Sleep, etc.) to a stub            |

The mapping strategy (`private_rwx`, `phantom`, `stomping`, `mockingjay`),
hashing algorithm (`djb2` / `fnv1a`), sleep algorithm
(`xor` / `ekko` / `foliage` / `cronos`), and SSN resolver
(`hellshall` / `freshycalls`) are runtime-selectable via the
`wraith_load_options` struct or globally via the `WRAITH_*_ALGO` /
`WRAITH_MAP_DEFAULT` cache variables.

---

## Tests

20 integration tests run end-to-end under `wine64` against the in-tree
fixture DLLs (`payload.dll`, `seh_dll.dll`, `forwarder_dll.dll`).

```
ctest --test-dir build --output-on-failure
# 20/20 passing
```

Each test verifies one observable behaviour: forwarded export
resolution, x64 SEH registration, `MEM_IMAGE` backing after phantom
hollowing, `xor eax,eax;ret` bytes after the ETW patch, BP-induced RIP
redirect via `DR0`, and so on.

---

## API surface

The full public API lives under `<wraith/wraith.h>`. Headers are
named `wraith.h`, `wraith_loader.h`, `wraith_options.h`,
`wraith_status.h`, `wraith_resource.h`, `wraith_introspect.h`,
`wraith_stealth.h`, `wraith_types.h`. Every public function returns a
`wraith_status_t`; failure cases come with a free-form description
available via `wraith_last_error()`.

Core entry points:

```c
wraith_status_t wraith_load_library(const void *buf, size_t size,
                                    const wraith_load_options *opts,
                                    wraith_handle_t *out);
wraith_status_t wraith_get_proc_address(wraith_handle_t h,
                                        const char *name,
                                        void **out_proc);
wraith_status_t wraith_call_entry_point(wraith_handle_t h, int *out_exit_code);
wraith_status_t wraith_free_library(wraith_handle_t h);
const char     *wraith_status_string(wraith_status_t s);
const char     *wraith_last_error(void);
const char     *wraith_version(void);
```

Resource APIs (`wraith_find_resource`, `wraith_load_resource_data`,
`wraith_load_string`) and stealth toggles
(`wraith_sleep`, `wraith_pageguard_arm`, `wraith_hwbp_install`,
`wraith_stackspoof_probe`, `wraith_unhook_ntdll`,
`wraith_patch_etw`, `wraith_patch_amsi`) are all in the same flat
namespace.

---

## How Wraith compares

|                          | Wraith              | sRDI / ReflectiveDLLInjection | Donut                |
|--------------------------|---------------------|-------------------------------|----------------------|
| Stealth posture          | High (compile flags) | Low                          | Medium (PIC blob)    |
| `RW → RX` hygiene        | Strict              | RWX                           | RWX                  |
| Forwarded imports        | Yes                 | No                            | Partial              |
| Delay-load imports       | Yes                 | No                            | No                   |
| x64 SEH registered       | Yes                 | No                            | No                   |
| Phantom / Mockingjay     | Yes                 | No                            | No                   |
| Sleep obfuscation        | XOR / Ekko / Cronos | No                            | No                   |
| HWBP hooks               | Yes                 | No                            | No                   |
| Output                   | Static lib + headers| `.bin` blob                   | Shellcode blob       |
| Default footprint        | ~70 KB              | ~5 KB                         | ~30 KB               |

Wraith is meant to be linked into a custom loader that you control;
it is not a one-shot blob.

---

## Authorized use

Wraith is intended for authorized red-team engagements, security
research, and educational settings. The author makes no warranty as
to its fitness for any purpose; see [LICENSE](LICENSE).

For security disclosures, see [SECURITY.md](SECURITY.md).

## License

[MIT](LICENSE) — Copyright (c) 2026 Rafael Dornelas.
