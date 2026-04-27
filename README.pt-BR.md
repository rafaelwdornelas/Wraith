# Wraith

> Carregador stealth de PE em memória, para Windows x64.

[English](README.md) · **Português (Brasil)**

[![Build (MinGW)](https://img.shields.io/badge/build-MinGW%20x86__64-success)](#)
[![Build (MSVC)](https://img.shields.io/badge/build-MSVC%202022-success)](#)
[![Tests (Wine)](https://img.shields.io/badge/tests-wine64%20%E2%9C%9320%2F20-blue)](#)
[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
![Platform](https://img.shields.io/badge/platform-Windows%2010%201809%2B%20%7C%20x64-informational)

O Wraith carrega uma DLL ou EXE inteiramente a partir de um buffer em
memória — sem `LoadLibrary`, sem arquivo temporário, sem `MEM_IMAGE`
escrito em disco. O profile `default` entrega só os fixes de
confiabilidade (nada que gere IOC alto). Cada técnica stealth é uma flag
de compilação `WRAITH_USE_*` separada, então o binário que você
distribui contém apenas o que você escolheu ativar.

---

## Destaques

| Indicador-de-Comprometimento (IOC) | Wraith                                                              |
|------------------------------------|---------------------------------------------------------------------|
| Alocação `PAGE_EXECUTE_READWRITE`        | **Nunca** — higiene `RW → RX` estrita                          |
| `LoadLibraryA` / `GetProcAddress` na IAT | Opcional — hashing DJB2 da API + caminhada na PEB.Ldr          |
| Win32 `VirtualAlloc` / `VirtualProtect`  | Opcional — Hell's Hall + FreshyCalls SSN-por-RVA               |
| Região `MEM_PRIVATE + RX` sem backing    | Opcional — Phantom hollowing / Module stomping / Mockingjay    |
| Módulo ausente das listas `PEB.Ldr`      | Opcional — linkagem completa na LDR com nome de masquerade     |
| Sem `RtlAddFunctionTable` (SEH x64 quebrado) | **Sempre ativo** — `__try` / `__except` funcionam          |
| Imports forwarded / delay / bound        | **Sempre ativo** — resolução completa de imports               |
| `.text` em texto plano enquanto ocioso   | Opcional — XOR / Ekko / Cronos / Page-Guard                    |
| Hooks userland em `ntdll`                | Opcional — refresh do disco **ou** mapeamento privado          |
| Stack walk revela a origem do loader     | Opcional — spoof sintético estilo SilentMoonwalk               |
| Patches inline detectáveis no `.text`    | **Hooks por hardware breakpoint (DR0–DR3)** — zero modificação de memória |
| `CreateThread` no awakener               | Opcional — threadless via hijack de `TpAllocWork`              |
| Telemetria userland do ETW               | Opcional — patch em `EtwEventWrite`                            |
| Scan de buffers .NET pela AMSI           | Opcional — short-circuit de `AmsiScanBuffer`                   |
| `PEB.BeingDebugged` / `NtGlobalFlag`     | Opcional — spoof passivo anti-debug                            |
| Heap principal do processo guarda artefatos | Opcional — heap masquerade (`RtlCreateHeap` privado)        |

---

## Quickstart

```bash
# Cross-compile do Linux + roda os testes sob wine64
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
    fprintf(stderr, "load falhou: %s (%s)\n",
            wraith_status_string(rc), wraith_last_error());
    return 1;
}

void *fn = NULL;
wraith_get_proc_address(h, "addNumbers", &fn);
int sum = ((int (*)(int, int))fn)(2, 3);

wraith_sleep(h, 600);          // .text fica criptografado, RIP redirecionado
wraith_free_library(h);
```

---

## Distribuição single-file (`dist/`)

Quando você não quiser instalar CMake só para experimentar o loader, o
projeto entrega um pacote amalgamado de 2 arquivos por profile dentro de
`dist/`, no mesmo estilo de SQLite / fancycode-MemoryModule. Escolha a
pasta que corresponde ao threat model desejado e copie os dois arquivos
para o seu projeto:

```
dist/
├── default/           # reliability ON, stealth OFF        (~210 KiB wraith.c)
├── minimal/           # loader puro, sem extras            (~210 KiB wraith.c)
├── teaching/          # cada feature compilada             (~355 KiB wraith.c)
├── paranoid-classic/  # hashing + Hell's Hall + Phantom    (~317 KiB wraith.c)
└── paranoid-full/     # full chain bleeding-edge           (~337 KiB wraith.c)
```

Cada pasta contém:

- `wraith.h` — a API pública (tudo que `<wraith/wraith.h>` expõe,
  inlineado em um único header).
- `wraith.c` — todas as TUs concatenadas; trampolines em asm reescritos
  como funções `__attribute__((naked))` em C. Não precisa de `.S` junto.
- `QUICKSTART.md` — instrução de build em 3 linhas para o profile.
- `LICENSE` — MIT.

```sh
# Copie os dois arquivos ao lado do seu loader.c, depois:
x86_64-w64-mingw32-gcc -c wraith.c -o wraith.o
x86_64-w64-mingw32-gcc loader.c wraith.o -o loader.exe
```

O artefato amalgamado é funcionalmente equivalente a compilar a árvore
CMake canônica com o `-DWRAITH_PROFILE=` correspondente. Para
regenerar após mudar algo em `src/` ou `include/`:

```sh
python3 tools/amalgamate.py             # todos os profiles
python3 tools/amalgamate.py paranoid-classic   # um único profile
```

Limitações vs o build CMake: não roda o codegen de `wr_api_hashes.h` (a
amalgamação usa as constantes de fallback embutidas), não aplica
`-nostdlib`/CET/CFG (recompile via CMake com `WRAITH_HARDEN_*` se
precisar) e o path do dist assume sempre MinGW-w64 x86_64.

---

## Profiles de build

| Profile             | Postura                                                                |
|---------------------|------------------------------------------------------------------------|
| `default`           | Fixes de confiabilidade ON, todo stealth OFF. Menor e mais simples.    |
| `teaching`          | Toda feature compilada (alternável em runtime via flags).              |
| `paranoid-classic`  | Hashing + Hell's Hall + Phantom + Ekko + stack spoof + private ntdll.  |
| `paranoid-full`     | Acrescenta HWBP, threadless, Page-Guard, Cronos, FreshyCalls, no-CRT.  |
| `minimal`           | Sem extras de confiabilidade, menor lib estática possível.             |

---

## Flags de feature

Toda feature stealth é opt-in. Padrão é OFF.

| Flag                                | Técnica                                                            |
|-------------------------------------|--------------------------------------------------------------------|
| `WRAITH_USE_API_HASHING`            | Resolução de API por hash DJB2 / FNV1a                             |
| `WRAITH_USE_PEB_WALK`               | Resolve módulos via `PEB.Ldr` (sem `GetModuleHandle`)              |
| `WRAITH_USE_INDIRECT_SYSCALLS`      | Hell's Hall / Halo's Gate / FreshyCalls SSN por RVA                |
| `WRAITH_USE_PEB_LINKAGE`            | Insere nas listas da `PEB.Ldr` com nome de masquerade              |
| `WRAITH_USE_PHANTOM_HOLLOWING`      | `NtCreateSection(SEC_IMAGE)` sobre uma DLL hospedeira              |
| `WRAITH_USE_MODULE_STOMPING`        | Sobrescreve o `.text` de um módulo legítimo já carregado           |
| `WRAITH_USE_SLEEP_OBFUSCATION`      | Cifra do `.text` enquanto ocioso (XOR / Ekko / Foliage / Cronos)   |
| `WRAITH_USE_UNHOOK_NTDLL`           | Restaura o `ntdll.text` a partir do disco                          |
| `WRAITH_USE_PRIVATE_NTDLL`          | Mapeia uma cópia privada e limpa do `ntdll`                        |
| `WRAITH_USE_ETW_PATCH`              | `EtwEventWrite` → `xor eax, eax; ret`                              |
| `WRAITH_USE_AMSI_PATCH`             | `AmsiScanBuffer` → `AMSI_RESULT_CLEAN`                             |
| `WRAITH_USE_STACK_SPOOF`            | Stack spoof sintético estilo SilentMoonwalk                        |
| `WRAITH_USE_HWBP_HOOKS`             | Hooks por hardware breakpoint (DR0–DR3) via VEH                    |
| `WRAITH_USE_THREADLESS_EXEC`        | Hijack de `TpAllocWork` / `RtlRegisterWait` em vez de `CreateThread` |
| `WRAITH_USE_PAGE_GUARD_ENCRYPT`     | Cifra preguiçosa por página via VEH em `PAGE_GUARD`                |
| `WRAITH_USE_HEAP_MASQUERADE`        | Heap privada vinculada a `MEM_IMAGE` legítimo                      |
| `WRAITH_USE_ANTI_DEBUG_SPOOF`       | Limpeza passiva de `PEB.BeingDebugged` / `NtGlobalFlag`            |
| `WRAITH_USE_HOST_IAT_REDIRECT`      | Patch nos slots da IAT do processo host (Sleep, etc.) para um stub |

A estratégia de mapeamento (`private_rwx`, `phantom`, `stomping`,
`mockingjay`), o algoritmo de hashing (`djb2` / `fnv1a`), o algoritmo
de sleep (`xor` / `ekko` / `foliage` / `cronos`) e o resolver de SSN
(`hellshall` / `freshycalls`) podem ser escolhidos em runtime via a
struct `wraith_load_options`, ou globalmente pelas variáveis de cache
`WRAITH_*_ALGO` / `WRAITH_MAP_DEFAULT`.

---

## Testes

20 testes de integração rodam ponta-a-ponta sob `wine64`, contra as
DLLs de fixture do próprio repositório (`payload.dll`, `seh_dll.dll`,
`forwarder_dll.dll`).

```
ctest --test-dir build --output-on-failure
# 20/20 passando
```

Cada teste verifica um comportamento observável: resolução de export
forwarded, registro de SEH x64, backing `MEM_IMAGE` após phantom
hollowing, bytes `xor eax,eax;ret` após o patch ETW, redirecionamento
de RIP induzido por BP em `DR0`, e por aí vai.

---

## Superfície da API

A API pública completa fica em `<wraith/wraith.h>`. Os headers se
chamam `wraith.h`, `wraith_loader.h`, `wraith_options.h`,
`wraith_status.h`, `wraith_resource.h`, `wraith_introspect.h`,
`wraith_stealth.h`, `wraith_types.h`. Toda função pública retorna um
`wraith_status_t`; em caso de falha, há uma descrição livre disponível
via `wraith_last_error()`.

Pontos de entrada principais:

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

As APIs de recurso (`wraith_find_resource`, `wraith_load_resource_data`,
`wraith_load_string`) e os toggles de stealth (`wraith_sleep`,
`wraith_pageguard_arm`, `wraith_hwbp_install`,
`wraith_stackspoof_probe`, `wraith_unhook_ntdll`, `wraith_patch_etw`,
`wraith_patch_amsi`) ficam todos no mesmo namespace plano.

---

## Como o Wraith se compara

|                          | Wraith              | sRDI / ReflectiveDLLInjection | Donut                |
|--------------------------|---------------------|-------------------------------|----------------------|
| Postura stealth          | Alta (flags de compilação) | Baixa                  | Média (blob PIC)     |
| Higiene `RW → RX`        | Estrita             | RWX                           | RWX                  |
| Imports forwarded        | Sim                 | Não                           | Parcial              |
| Imports delay-load       | Sim                 | Não                           | Não                  |
| SEH x64 registrado       | Sim                 | Não                           | Não                  |
| Phantom / Mockingjay     | Sim                 | Não                           | Não                  |
| Sleep obfuscation        | XOR / Ekko / Cronos | Não                           | Não                  |
| Hooks por HWBP           | Sim                 | Não                           | Não                  |
| Saída                    | Lib estática + headers | Blob `.bin`                | Blob shellcode       |
| Footprint padrão         | ~70 KB              | ~5 KB                         | ~30 KB               |

O Wraith foi feito para ser linkado dentro de um loader customizado que
você controla; **não** é um blob de uso único.

---

## Uso autorizado

O Wraith é destinado a engagements de red-team autorizados, pesquisa
em segurança e contextos educacionais. O autor não oferece nenhuma
garantia sobre adequação a qualquer propósito; veja a [LICENSE](LICENSE).

Para divulgação responsável de vulnerabilidades, veja o
[SECURITY.md](SECURITY.md).

## Licença

[MIT](LICENSE) — Copyright (c) 2026 Rafael Dornelas.
