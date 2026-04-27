# Wraith — Arquitetura

[English](ARCHITECTURE.md) · **Português (Brasil)**

Este documento é a orientação técnica para quem vai ler o código ou
adicionar uma técnica nova. Espelha o que está no
[`README.pt-BR.md`](../README.pt-BR.md), mas vai mais fundo nos
contratos internos.

---

## Visão em camadas

```
┌──────────────────────────────────────────────────────────────┐
│ Camada 5 │ API pública  │ include/wraith/*.h                │
│          │              │ wraith_load_library, wraith_get_proc_address │
│          │              │ wraith_free_library, wraith_sleep, ...       │
├──────────┼──────────────┼─────────────────────────────────────┤
│ Camada 4 │ Pipeline do  │ src/loader/loader_pipeline.c       │
│          │ loader       │ orquestra o load de 17 passos      │
├──────────┴──┬───────────┴───────────┬─────────────────────────┤
│ Camada 3a │ Camada 3b   │ Camada 3c                          │
│ Parser PE │ Vtable de   │ Hooks de stealth                   │
│ src/pe/*  │ mapping     │ src/stealth/*                      │
│           │ src/mapping/*│                                   │
├───────────┴─────────────┬┴────────────────────────────────────┤
│ Camada 2 │ Runtime       │ src/runtime/*                      │
│          │               │ vtable rt_ops: load_library,       │
│          │               │ get_proc, free_library,            │
│          │               │ nt_alloc, nt_protect, nt_free,     │
│          │               │ nt_flush_icache                    │
├──────────┴───────────────┼────────────────────────────────────┤
│ Camada 1 │ Engine de     │ src/syscalls/* — Hell's Hall       │
│          │ syscalls      │ + Halo's Gate + 8 stubs Nt*        │
├──────────┴───────────────┴────────────────────────────────────┤
│ Camada 0 │ Intrínsecos, hashes, primitivas matemáticas        │
└────────────────────────────────────────────────────────────────┘
```

Cada camada só pode chamar **para baixo**. A API pública nunca inclui
`<windows.h>` (exceto `wraith_resource.h`, onde a superfície exige).
Todo código específico do Windows mora abaixo da camada 5.

---

## As três vtables centrais

O projeto inteiro gira em torno de três tabelas de ponteiros de
função. Técnicas novas quase sempre se conectam em uma delas — encostar
em `loader_pipeline.c` é raro.

### `wr_map_ops` (camada 3b)

Definida em `src/mapping/map_strategy.h`. Implementada por:

| Estratégia | Arquivo | Padrão | Propósito |
|------------|---------|--------|-----------|
| `WRAITH_MAP_PRIVATE_RW_RX` | `map_private_rwx.c` | sim | `NtAllocate*` + higiene RW→RX |
| `WRAITH_MAP_PHANTOM_HOLLOW` | `map_phantom.c` | opt-in | Backing por `NtCreateSection(SEC_IMAGE)` |
| `WRAITH_MAP_MODULE_STOMPING` | `map_stomping.c` | opt-in | Hospedeiro via `LoadLibraryW` + backup/restore de bytes |

`map_dispatch.c` resolve o enum da estratégia para a vtable
correspondente no momento do load. Cada vtable expõe `reserve /
commit / protect / release / destroy`.

### `wr_rt_ops` (camada 2)

Definida em `src/runtime/rt_api.h`. Duas implementações:

| Vtable | Arquivo | Quando seleciona | Notas |
|--------|---------|------------------|-------|
| `wr_rt_ops_baseline` | `rt_api_baseline.c` | quando `WRAITH_F_API_HASHING` está OFF | Encapsula Win32 `LoadLibraryA` / `VirtualAlloc` / etc. |
| `wr_rt_ops_ntapi` | `rt_api_ntapi.c` | quando `WRAITH_F_API_HASHING` está ON | Caminha pela PEB.Ldr; roteia ops de memória pelo Hell's Hall quando `WRAITH_F_INDIRECT_SYSCALLS` está setado |

Ambas expõem: `load_library / get_proc / free_library / nt_alloc /
nt_protect / nt_free / nt_flush_icache`.

### `wr_stealth_hooks` (camada 3c, scaffold)

Cadeia de callbacks `post_map / post_load / pre_unload / pre_sleep` que
o pipeline invoca em fases numeradas fixas. A release 1.0 entrega o
mecanismo de slots; cadeias concretas chegam em extensões futuras.

---

## Pipeline de load em 17 passos

Cada passo mora em um arquivo dedicado em `src/loader/` e carrega o
mesmo ID numérico que `wr_trace` emite. Pular um passo significa
desligar a flag de compilação correspondente — o pipeline é defensivo
contra dados ausentes / vazios.

| # | Passo | Arquivo | Ativo |
|---|-------|---------|-------|
| 1 | Validação de bounds + magic do PE | `loader_pipeline.c` chama `pe_validate.c` | sempre |
| 2 | Métricas da imagem (`SizeOfImage`, fim da última seção) | `pe_image_metrics.c` | sempre |
| 3 | Seleção do runtime | `rt_api.c` | sempre |
| 4 | Seleção da estratégia de mapping | `map_dispatch.c` | sempre |
| 5 | Reserve da imagem | `<estratégia>->reserve` | sempre |
| 6 | Hook `post_map` (cadeia) | `wr_stealth_hooks` | quando registrado |
| 7 | Cópia de headers + seções | `loader_sections.c` | sempre |
| 8 | Relocations base (DIR64) | `loader_relocs.c` | sempre |
| 9 | Imports — bound (skip) → normal → delay | `loader_imports*.c` | por flag |
| 10 | Finalização das proteções por seção | `loader_finalize.c` | sempre |
| 11 | Hook `post_protect` (cadeia) | `wr_stealth_hooks` | quando registrado |
| 12 | SEH x64 (`RtlAddFunctionTable`) | `loader_seh_x64.c` | `WRAITH_REGISTER_SEH_X64` |
| 13 | Linkagem na PEB.Ldr | `peb_link.c` | `WRAITH_USE_PEB_LINKAGE` |
| 14 | Callbacks TLS (ATTACH) | `loader_tls.c` | sempre |
| 15 | DllMain / entry point do EXE | `loader_entry.c` | sempre |
| 16 | Hook `post_load` (cadeia) | `wr_stealth_hooks` | quando registrado |
| 17 | Retorna o handle | `loader_api.c` | sempre |

`wr_pipeline_unwind` reverte 15 → 14 → 13 → 12 → 5 (roda detach + tls
detach + remoção da peb-link + unregister do SEH + release do mapping)
de forma simétrica.

---

## `wr_ctx` — o que o pipeline carrega

Definido em `src/core/wr_context_internal.h`. Toda API pública que
recebe um `wraith_handle_t` faz cast de volta para `wr_ctx*` e valida
`magic == WRAITH_CTX_MAGIC` antes de dereferenciar.

Grupos de campos relevantes:

- **Metadados da imagem** — `image_base`, `image_size`, `headers`,
  `image_type`, `is_relocated`, `initialized`.
- **Imports** — `imported_modules`, `imported_owned`,
  `imported_count`. Forwarders, imports normais e delay imports caem
  todos nos mesmos arrays.
- **Mapping** — `map_ops`, `map_state`. Estado específico da
  estratégia mora atrás do ponteiro opaco `map_state` (ex.:
  `phantom_state` para SEC_IMAGE, `stomping_state` para
  backup/host).
- **Runtime / syscalls** — `rt_ops`, `ntdll_base`, `kernel32_base`,
  `sc_engine`.
- **Slots de stealth** — `peb_ldr_entry`, `masquerade_name`,
  `masquerade_path`, `sleep_key`, `runtime_funcs`,
  `functbl_registered`.
- **Diagnóstico** — `last_status` por thread, `err_context`,
  callbacks `trace`/`trace_userdata` para eventos de fase do
  pipeline.

---

## Adicionar uma técnica nova — o contrato de 5 passos

1. Crie `src/<categoria>/<nome>.{c,h}` com a implementação.
2. Adicione uma opção `WRAITH_USE_<NOME>` em `cmake/options.cmake`.
3. Liste o arquivo fonte no `CMakeLists.txt` dentro de
   `if(WRAITH_USE_<NOME>)`.
4. Conecte na vtable / hook de pipeline apropriado
   (`map_dispatch.c`, `rt_api.c`, ou step 6/11/16 em
   `loader_pipeline.c`).
5. Adicione um teste de integração em
   `tests/integration/test_<feature>.c` e registre em
   `tests/integration/CMakeLists.txt`.

A convenção de layout é checada na revisão; PRs que mexem em
`loader_pipeline.c` recebem escrutínio extra.

---

## Mapa de arquivos (cheat-sheet)

```
include/wraith/         Headers da API pública
src/core/               Ciclo de vida do wr_ctx, status strings
src/pe/                 Parser PE com bounds-check, iteradores
src/loader/             Pipeline de 17 passos: section/reloc/imports/tls/seh/entry/finalize
src/exports/            Lookup binário + follower de forwarders
src/resource/           Walker de resources em três níveis
src/runtime/            Vtables rt_ops (baseline, ntapi)
                        Walker da PEB, resolver de exports baseado em hash
src/syscalls/           Hell's Hall + Halo's Gate, trampolim em asm
src/mapping/            Vtables map_ops (private_rwx, phantom, stomping)
src/stealth/peb_link/   Inserção na PEB.Ldr com masquerade
src/stealth/sleep/      Sleep obfuscation por XOR (alias para Ekko)
src/stealth/unhook/     Refresh de ntdll a partir do disco
src/stealth/etw/        Hot-patch em EtwEventWrite
src/stealth/amsi/       Hot-patch em AmsiScanBuffer
src/stealth/hashing/    DJB2 case-insensitive ASCII + UTF-16
tests/integration/      Testes ponta-a-ponta sob wine64 / Win10+
tests/integration/fixtures/ DLLs minúsculas (forwarder, SEH) usadas pelos testes
tools/                  hashgen.py + api_list.txt + ioc_audit
ci/github-actions/      build-mingw, build-msvc, test-wine
```
