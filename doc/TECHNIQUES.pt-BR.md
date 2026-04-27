# Wraith — Referência de técnicas

[English](TECHNIQUES.md) · **Português (Brasil)**

Uma linha por flag `WRAITH_USE_*`. Cada entrada lista o IOC que o
módulo neutraliza, o arquivo de implementação, a referência publicada
e ressalvas conhecidas. Leia primeiro [`ARCHITECTURE.pt-BR.md`](ARCHITECTURE.pt-BR.md)
para a visão em camadas.

---

## Features de confiabilidade (ON por padrão)

| Flag | IOC corrigido | Implementação | Notas |
|------|---------------|---------------|-------|
| `WRAITH_FORWARDED_EXPORTS` | Devolveu o ponteiro da string forwarder onde se esperava um ponteiro de função | `src/exports/export_forward.c` | Segue `"DLL.Func"` e `"DLL.#NNN"` recursivamente (limite de profundidade = 8) |
| `WRAITH_DELAY_LOAD_IMPORTS` | `IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT` ignorado | `src/loader/loader_imports_delay.c` | Resolução eager, só aceita descritor V2 (RVA) — V1 (VA) é rejeitado |
| `WRAITH_BOUND_IMPORTS` | IATs com bound-import são confiadas cegamente | `src/loader/loader_imports_bound.c` | Sempre re-resolve; emite evento de trace quando o diretório existe |
| `WRAITH_REGISTER_SEH_X64` | DLL carregada quebra em `__try` por falta de `RtlAddFunctionTable` | `src/loader/loader_seh_x64.c` | Aponta para o `.pdata` da imagem; sem cópia; `RtlDeleteFunctionTable` no free |
| `WRAITH_TLS_FULL_LIFECYCLE` | TLS DETACH nunca é entregue | `src/loader/loader_tls.c` | DETACH em `wraith_free_library` |
| `WRAITH_RW_TO_RX_HYGIENE` | Intermediário `PAGE_EXECUTE_READWRITE` flagado pelo Moneta | `src/mapping/map_dispatch.c` (`wr_prot_to_win32`) | O enum `wraith_prot_t` não tem valor RWX — invariante garantida no nível de tipo |

## Stealth clássico (OFF por padrão)

| Flag | IOC neutralizado | Implementação | Referência |
|------|------------------|---------------|-----------|
| `WRAITH_USE_API_HASHING` | Strings de nome de API na IAT / `.rdata` | `src/stealth/hashing/hash_djb2.c` + `tools/hashgen.py` | DJB2 case-insensitive; mesma arithmetic em C e Python |
| `WRAITH_USE_PEB_WALK` | Chamadas a `GetModuleHandle` na IAT | `src/runtime/rt_pebwalk.c` | Caminha em `InMemoryOrderModuleList` via `NtCurrentTeb()->ProcessEnvironmentBlock` |
| `WRAITH_USE_INDIRECT_SYSCALLS` | Chamadas `Nt*` passam pelos prólogos hookados de `ntdll` | `src/syscalls/sc_engine.c` + `sc_trampoline_x64.S` | Hell's Hall (C5pider) + fallback Halo's Gate |
| `WRAITH_USE_PHANTOM_HOLLOWING` | Região `MEM_PRIVATE+RX` sem backing | `src/mapping/map_phantom.c` | `NtCreateSection(SEC_IMAGE)` sobre uma DLL do System32 (Forrest Orr) |
| `WRAITH_USE_MODULE_STOMPING` | Phantom + identidade auto na PEB.Ldr | `src/mapping/map_stomping.c` | `LoadLibraryW` do host + backup/restore. Lab apenas |
| `WRAITH_USE_PEB_LINKAGE` | Módulo ausente em `EnumProcessModulesEx` | `src/stealth/peb_link/peb_link.c` | Insere `LDR_DATA_TABLE_ENTRY` em 3 listas com nome de masquerade |
| `WRAITH_USE_SLEEP_OBFUSCATION` | `.text` em texto plano enquanto ocioso | `src/stealth/sleep/*.c` | Baseline XOR com chave RDTSC; Ekko / Foliage / Cronos opt-in |
| `WRAITH_USE_UNHOOK_NTDLL` | Hooks inline em exports do `ntdll` | `src/stealth/unhook/unhook.c` | Diff em chunks de 16 bytes contra a cópia do `ntdll.dll` em disco |
| `WRAITH_USE_ETW_PATCH` | Telemetria userland do `EtwEventWrite` | `src/stealth/etw/etw_patch.c` | Substitui o prólogo por `33 c0 c3` (`xor eax,eax; ret`). NÃO silencia o ETW-Ti do kernel |
| `WRAITH_USE_AMSI_PATCH` | Chamadas a `AmsiScanBuffer` (.NET / PowerShell) | `src/stealth/amsi/amsi_patch.c` | Força `AMSI_RESULT_CLEAN`; carrega `amsi.dll` se ausente |

## Tier bleeding-edge

| Flag | IOC neutralizado | Implementação | Referência |
|------|------------------|---------------|-----------|
| `WRAITH_USE_STACK_SPOOF` | Stack walker enxerga a região do loader durante syscall | `src/syscalls/sc_trampoline_x64.S` (`wr_sc_stub_spoof_*`) + `g_ret_gadget` | SilentMoonwalk (SecIdiot) |
| `WRAITH_USE_HWBP_HOOKS` | Bytes de patch inline detectáveis em `.text` | `src/stealth/hwbp/hwbp.c` — DR0–DR3 + redirect via VEH | @CCob |
| `WRAITH_SLEEP_ALGO=cronos` | RIP da thread chamadora visível enquanto ocioso | `src/stealth/sleep/sleep_cronos.c` — timer queue + park no kernel | rad9800 / Klez |
| `WRAITH_USE_PRIVATE_NTDLL` | Hooks no ntdll carregado afetam o parsing de SSN | `src/stealth/private_ntdll/private_ntdll.c` — segunda cópia via `NtCreateSection(SEC_IMAGE)` | Forrest Orr |
| `WRAITH_SC_RESOLVER=freshycalls` | Hooks por função derrotam Hell's Hall + Halo's Gate | `src/syscalls/sc_ssn_resolver.c` — ordena `Nt*` por RVA, índice = SSN | Crummie5 |
| `WRAITH_USE_THREADLESS_EXEC` | Telemetria de `CreateThread` no awakener | `src/stealth/threadless/threadless.c` — dispatch via `CreateThreadpoolWork` | ZeroMemoryEx |
| `WRAITH_USE_PAGE_GUARD_ENCRYPT` | `.text` em texto plano enquanto ocioso | `src/stealth/page_guard/page_guard.c` — descifra preguiçosa por página via `EXCEPTION_GUARD_PAGE` | Truque de exception por page-guard |
| `WRAITH_MAP_MOCKINGJAY` | IOC de "página RWX nova" alocada | `src/mapping/map_mockingjay.c` + scanner — sobrepõe em `MEM_IMAGE+RWX` pré-existente | @Cracked5pider |
| `WRAITH_USE_HEAP_MASQUERADE` | Allocs do loader visíveis no `ProcessHeap` padrão | `src/stealth/heap_masq/heap_masq.c` — `HeapCreate` privado | — |
| `WRAITH_BUILD_PIC` | Loader exige entry-point PE para invocação | Opção CMake (placeholder; blob de shellcode adiado) | — |
| `WRAITH_USE_ANTI_DEBUG_SPOOF` | `PEB.BeingDebugged` / `NtGlobalFlag` revelam debugger | `src/stealth/anti_debug/anti_debug.c` — zera flags da PEB | — |
| `WRAITH_USE_HOST_IAT_REDIRECT` | `Sleep` do consumidor roda sem obfuscation | `src/stealth/host_iat/host_iat.c` — caminha pela PEB.Ldr, faz patch em thunks da IAT | — |

---

## Profiles preset

| Profile | Confiabilidade | Stealth clássico | Bleeding-edge | Caso de uso |
|---------|----------------|------------------|---------------|-------------|
| `default` | ✅ | ❌ | ❌ | Loader leve |
| `teaching` | ✅ | compilado, flag em runtime | compilado, flag em runtime | Demos ao vivo / showcase |
| `paranoid-classic` | ✅ | hashing + Hell's Hall + phantom + PEB-link + Ekko + ETW + stack spoof + private ntdll | — | Cadeia clássica completa |
| `paranoid-full` | ✅ | Adiciona AMSI patch + Cronos sleep | + HWBP, threadless, Page-Guard, heap masq, anti-debug, FreshyCalls | Cadeia bleeding-edge completa |
| `minimal` | parcial | ❌ | ❌ | Binário menor possível |

`cmake/options.cmake` tem o resolver — escolher um profile vira os
defaults `WRAITH_USE_*` correspondentes; passar
`-DWRAITH_USE_X=ON/OFF` explicitamente na linha de comando ainda
prevalece.

---

## Referências (por técnica)

- Forrest Orr — *Masking Malicious Memory Artifacts* (Phantom DLL Hollowing)
- C5pider — *Ekko: Sleep Obfuscation*; resolver de SSN do *Hell's Hall*
- Crummie5 — *FreshyCalls* SSN-por-RVA
- @x86matthew — Foliage, técnica de sleep com `NtContinue`
- @rad9800 / Klez — *Cronos*, *Zilean*, cadeias de sleep obfuscation
- SecIdiot — *SilentMoonwalk* call-stack spoofing
- @CCob — *Hardware-Breakpoint Hooks via VEH*
- @Cracked5pider — *Mockingjay*: caça por RWX em módulos pré-existentes
- ZeroMemoryEx — *Threadless Inject* via `TpAllocWork`
- MITRE ATT&CK — `T1620` Reflective Code Loading, `T1055.013` Process Doppelgänging
- MDSec — *Bypassing User-Mode Hooks and Direct Invocation of System Calls*
