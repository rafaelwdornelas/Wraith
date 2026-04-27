# Wraith — Techniques reference

**English** · [Português (Brasil)](TECHNIQUES.pt-BR.md)

One row per `WRAITH_USE_*` flag. Each entry lists the IOC the module
neutralizes, the implementation file, the published reference, and
known caveats. Read [`ARCHITECTURE.md`](ARCHITECTURE.md) first for
the layered overview.

---

## Reliability features (ON by default)

| Flag | IOC fixed | Implementation | Notes |
|------|-----------|----------------|-------|
| `WRAITH_FORWARDED_EXPORTS` | Forwarder string returned where a function pointer was expected | `src/exports/export_forward.c` | Follows `"DLL.Func"` and `"DLL.#NNN"` recursively (depth cap = 8) |
| `WRAITH_DELAY_LOAD_IMPORTS` | `IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT` ignored | `src/loader/loader_imports_delay.c` | Eager resolution, V2 (RVA) descriptor only — V1 (VA) rejected |
| `WRAITH_BOUND_IMPORTS` | Bound-import IATs blindly trusted | `src/loader/loader_imports_bound.c` | Always re-resolves; emits trace event when directory exists |
| `WRAITH_REGISTER_SEH_X64` | Loaded DLL crashes on `__try` because no `RtlAddFunctionTable` | `src/loader/loader_seh_x64.c` | Pointer to in-image `.pdata`; no copy; `RtlDeleteFunctionTable` on free |
| `WRAITH_TLS_FULL_LIFECYCLE` | TLS DETACH never delivered | `src/loader/loader_tls.c` | DETACH on `wraith_free_library` |
| `WRAITH_RW_TO_RX_HYGIENE` | `PAGE_EXECUTE_READWRITE` intermediate flagged by Moneta | `src/mapping/map_dispatch.c` (`wr_prot_to_win32`) | The `wraith_prot_t` enum has no RWX value — invariant is enforced at the type level |

## Classic stealth (OFF by default)

| Flag | IOC neutralized | Implementation | Reference |
|------|-----------------|----------------|-----------|
| `WRAITH_USE_API_HASHING` | API name strings in IAT / `.rdata` | `src/stealth/hashing/hash_djb2.c` + `tools/hashgen.py` | DJB2 case-insensitive; mirror in C and Python |
| `WRAITH_USE_PEB_WALK` | `GetModuleHandle` calls in IAT | `src/runtime/rt_pebwalk.c` | Walks `InMemoryOrderModuleList` via `NtCurrentTeb()->ProcessEnvironmentBlock` |
| `WRAITH_USE_INDIRECT_SYSCALLS` | `Nt*` calls go through hooked ntdll prologues | `src/syscalls/sc_engine.c` + `sc_trampoline_x64.S` | Hell's Hall (C5pider) + Halo's Gate fallback |
| `WRAITH_USE_PHANTOM_HOLLOWING` | Unbacked `MEM_PRIVATE+RX` region | `src/mapping/map_phantom.c` | `NtCreateSection(SEC_IMAGE)` of System32 host (Forrest Orr) |
| `WRAITH_USE_MODULE_STOMPING` | Phantom + auto PEB.Ldr identity | `src/mapping/map_stomping.c` | `LoadLibraryW` host + backup/restore. Lab-only |
| `WRAITH_USE_PEB_LINKAGE` | Module missing from `EnumProcessModulesEx` | `src/stealth/peb_link/peb_link.c` | Inserts `LDR_DATA_TABLE_ENTRY` into 3 lists with masquerade name |
| `WRAITH_USE_SLEEP_OBFUSCATION` | `.text` plaintext while idle | `src/stealth/sleep/*.c` | RDTSC-keyed XOR baseline; Ekko / Foliage / Cronos opt-in |
| `WRAITH_USE_UNHOOK_NTDLL` | Inline hooks on ntdll exports | `src/stealth/unhook/unhook.c` | 16-byte chunk diff vs disk copy of `ntdll.dll` |
| `WRAITH_USE_ETW_PATCH` | `EtwEventWrite` userland telemetry | `src/stealth/etw/etw_patch.c` | Replace prologue with `33 c0 c3` (`xor eax,eax; ret`). Does NOT silence ETW-Ti kernel |
| `WRAITH_USE_AMSI_PATCH` | `AmsiScanBuffer` calls (.NET / PowerShell) | `src/stealth/amsi/amsi_patch.c` | Force `AMSI_RESULT_CLEAN`; loads `amsi.dll` if not present |

## Bleeding-edge tier

| Flag | IOC neutralized | Implementation | Reference |
|------|-----------------|----------------|-----------|
| `WRAITH_USE_STACK_SPOOF` | Stack walker sees loader region during syscall | `src/syscalls/sc_trampoline_x64.S` (`wr_sc_stub_spoof_*`) + `g_ret_gadget` | SilentMoonwalk (SecIdiot) |
| `WRAITH_USE_HWBP_HOOKS` | Inline-patch bytes detectable in `.text` | `src/stealth/hwbp/hwbp.c` — DR0–DR3 + VEH redirect | @CCob |
| `WRAITH_SLEEP_ALGO=cronos` | Calling thread RIP visible during idle | `src/stealth/sleep/sleep_cronos.c` — timer queue + park-in-kernel | rad9800 / Klez |
| `WRAITH_USE_PRIVATE_NTDLL` | Loaded ntdll's hooks affect SSN parsing | `src/stealth/private_ntdll/private_ntdll.c` — `NtCreateSection(SEC_IMAGE)` second copy | Forrest Orr |
| `WRAITH_SC_RESOLVER=freshycalls` | Per-function hooks defeat Hell's Hall + Halo's Gate | `src/syscalls/sc_ssn_resolver.c` — sort `Nt*` by RVA, index = SSN | Crummie5 |
| `WRAITH_USE_THREADLESS_EXEC` | `CreateThread` telemetry on awakener | `src/stealth/threadless/threadless.c` — `CreateThreadpoolWork` dispatch | ZeroMemoryEx |
| `WRAITH_USE_PAGE_GUARD_ENCRYPT` | `.text` plaintext while idle | `src/stealth/page_guard/page_guard.c` — lazy per-page decrypt via `EXCEPTION_GUARD_PAGE` | Page-guard exception trick |
| `WRAITH_MAP_MOCKINGJAY` | "New RWX page" allocation IOC | `src/mapping/map_mockingjay.c` + scanner — overlay onto pre-existing `MEM_IMAGE+RWX` | @Cracked5pider |
| `WRAITH_USE_HEAP_MASQUERADE` | Loader allocs visible in default `ProcessHeap` | `src/stealth/heap_masq/heap_masq.c` — private `HeapCreate` | — |
| `WRAITH_BUILD_PIC` | Loader requires PE entry to invoke | CMake option (placeholder; shellcode blob deferred) | — |
| `WRAITH_USE_ANTI_DEBUG_SPOOF` | `PEB.BeingDebugged` / `NtGlobalFlag` reveal debugger | `src/stealth/anti_debug/anti_debug.c` — zero PEB flags | — |
| `WRAITH_USE_HOST_IAT_REDIRECT` | Consumer's `Sleep` runs without obfuscation | `src/stealth/host_iat/host_iat.c` — walk PEB.Ldr, patch IAT thunks | — |

---

## Profile presets

| Profile | Reliability | Classic stealth | Bleeding-edge | Use case |
|---------|-------------|-----------------|---------------|----------|
| `default` | ✅ | ❌ | ❌ | Lightweight loader |
| `teaching` | ✅ | compiled, runtime-flag | compiled, runtime-flag | Live demos / portfolio walkthrough |
| `paranoid-classic` | ✅ | hashing + Hell's Hall + phantom + PEB-link + Ekko + ETW + stack spoof + private ntdll | — | Classic full chain |
| `paranoid-full` | ✅ | Adds AMSI patch + Cronos sleep | + HWBP, threadless, Page-Guard, heap masq, anti-debug, FreshyCalls | Bleeding-edge full chain |
| `minimal` | partial | ❌ | ❌ | Smallest binary |

`cmake/options.cmake` has the resolver — picking a profile flips
the underlying `WRAITH_USE_*` defaults; explicit
`-DWRAITH_USE_X=ON/OFF` on the command line still wins.

---

## References (per-technique)

- Forrest Orr — *Masking Malicious Memory Artifacts* (Phantom DLL Hollowing)
- C5pider — *Ekko: Sleep Obfuscation*; *Hell's Hall* SSN resolver
- Crummie5 — *FreshyCalls* SSN-by-RVA syscall resolver
- @x86matthew — Foliage `NtContinue` sleep technique
- @rad9800 / Klez — *Cronos*, *Zilean* sleep obfuscation chains
- SecIdiot — *SilentMoonwalk* call-stack spoofing
- @CCob — *Hardware-Breakpoint Hooks via VEH*
- @Cracked5pider — *Mockingjay*: hunting RWX in pre-existing modules
- ZeroMemoryEx — *Threadless Inject* via `TpAllocWork`
- MITRE ATT&CK — `T1620` Reflective Code Loading, `T1055.013` Process Doppelgänging
- MDSec — *Bypassing User-Mode Hooks and Direct Invocation of System Calls*
