# Changelog

All notable changes to **Wraith** are documented in this file.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and the project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.0] — 2026-04-26

Initial public release.

### Loader core
- In-memory PE loader for x64 DLLs and EXEs
- Forwarded-export resolution (`kernel32.Sleep -> KERNELBASE.Sleep`)
- Delay-load imports (`ImgDelayDescr` honored)
- Bound imports (timestamp match → fast-skip)
- TLS callbacks for the full `DLL_PROCESS_ATTACH/DETACH` and
  `DLL_THREAD_ATTACH/DETACH` lifecycle
- x64 SEH registered via `RtlAddFunctionTable`; loaded modules can
  raise / catch C++ / SEH exceptions correctly
- Strict `RW → RX` hygiene — no `PAGE_EXECUTE_READWRITE` ever requested

### Stealth (compile-time opt-in)
- DJB2 / FNV1a API hashing with PEB.Ldr walk
- Hell's Hall + Halo's Gate + FreshyCalls SSN-by-RVA indirect syscalls
- Mapping strategies: `private_rwx`, phantom hollowing
  (`NtCreateSection(SEC_IMAGE)`), module stomping, Mockingjay (RWX hunt)
- PEB.Ldr linkage with arbitrary masquerade name
- Sleep obfuscation: XOR baseline, Ekko (xor-aliased), Foliage,
  Cronos (NtContinue + APC chain)
- ntdll cleanup: disk-refresh unhook **or** private-mapping
- Userland telemetry patches: `EtwEventWrite`, `AmsiScanBuffer`
- SilentMoonwalk-style synthetic-frame stack spoof
- DR0–DR3 hardware-breakpoint hooks via VEH (zero-memory-mod hooking)
- Threadless execution via `TpAllocWork` / `RtlRegisterWait` hijack
- Lazy per-page `PAGE_GUARD`-driven self-encryption
- Heap masquerade — private `RtlCreateHeap` for loader bookkeeping
- Anti-debug spoof — passive `PEB.BeingDebugged` / `NtGlobalFlag` clear
- Host IAT redirect — rewrite host-process Sleep thunks

### Tooling
- `tools/hashgen.py` — deterministic compile-time DJB2 hash table
- `tools/ioc_audit.py` — pe-sieve / Moneta differential audit harness
  (Windows VM workflow)
- `cmake/toolchains/mingw-x86_64.cmake` — Linux → Windows x64 cross
- 20 integration tests under `wine64`; CI validates MinGW + MSVC

### Build profiles
- `default` — reliability ON, all stealth OFF
- `teaching` — every feature compiled in
- `paranoid-classic` — classic full-chain stealth (hashing + Hell's Hall +
  phantom + Ekko + stack spoof + private ntdll)
- `paranoid-full` — bleeding-edge (everything ON, hardened, no-CRT)
- `minimal` — smallest static library

[1.0.0]: #100--2026-04-26
