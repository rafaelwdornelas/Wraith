# Wraith 1.0.0 - paranoid-full profile (amalgamated)

Everything in paranoid-classic plus AMSI patch, hardware-breakpoint hooks, threadless execution, page-guard self-encryption, heap masquerade, anti-debug spoofing, and Cronos sleep. Built without CRT in the canonical CMake path; the amalgamated build keeps the CRT for portability since the dist/ artifact is meant to be dropped into existing projects.

## Drop-in build (3 commands)

```sh
# 1. Pull these files into your project (alongside your loader)
cp wraith.c wraith.h LICENSE /path/to/your/project/

# 2. Compile against the MinGW-w64 cross toolchain
x86_64-w64-mingw32-gcc -c wraith.c -o wraith.o

# 3. Link your loader against wraith.o
x86_64-w64-mingw32-gcc loader.c wraith.o -o loader.exe
```

## Minimal usage

```c
#include "wraith.h"

int main(void) {
    unsigned char *dll_bytes = /* ... */;
    size_t dll_size           = /* ... */;

    wraith_load_options opt = {0};
    opt.flags = WRAITH_F_RELIABILITY_ALL;

    wraith_handle_t h = NULL;
    wraith_status_t rc = wraith_load_library(dll_bytes, dll_size, &opt, &h);
    if (WRAITH_SUCCESS(rc)) wraith_free_library(h);
    return rc;
}
```

## What's inlined

- 49 compilation units from `src/`
- 7 public + 33 internal headers
- 16 indirect-syscall stubs (inline-asm naked C)

## Caveats vs the CMake build

- No `__has_include("wr_api_hashes.h")` codegen path. The amalgamation
  relies on the in-source fallback constants (kernel32 + LoadLibraryA
  + FreeLibrary). If you need the full hash table, regenerate via
  `python3 tools/hashgen.py` from the canonical tree.
- No CET / CFG / -nostdlib hardening. The dist/ artifact targets
  portability; rebuild from the CMake tree with the appropriate
  `WRAITH_HARDEN_*` flags if you need them.
- Headers are written for x86_64. Compile flags must include
  `-DCMAKE_C_STANDARD=11` (or `-std=c11`) and `-m64` (default for
  MinGW-w64 x86_64).
