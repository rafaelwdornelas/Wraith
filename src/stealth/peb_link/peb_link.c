/*
 * src/stealth/peb_link/peb_link.c
 *
 * Insertion / removal of an `LDR_DATA_TABLE_ENTRY` representing our
 * memory-loaded image into PEB.Ldr.
 *
 * Layout strategy: we mirror the Win10 1809 .. Win11 24H2 LDR entry
 * up to the fields that ntdll's background walkers actually deref —
 * `DdagNode` (offset 0x98) and `BaseNameHashValue` (offset 0x108) in
 * particular. Truncating earlier (as the original implementation did)
 * caused NULL derefs in `LdrpUpdateLoadCount2` and friends on long-
 * running multi-threaded payloads.
 *
 * Concurrency: every walk of PEB.Ldr inside ntdll is protected by
 * `LoaderLock`. We acquire it via `LdrLockLoaderLock` around install
 * and remove so concurrent loaders / enumerators never see a torn
 * list state.
 *
 * Cleanup: after `list_remove` an unrelated walker may still be
 * holding a pointer into our entry from a previous traversal.
 * Freeing immediately would create a use-after-free window, so we
 * push the entry + DDAG node + masquerade strings into a process-
 * wide graveyard and never free them. Per-load leak is ~0x200 bytes;
 * acceptable for the safety guarantee.
 */

#include "stealth/peb_link/peb_link.h"
#include "core/wr_context_internal.h"
#include "runtime/rt_pebwalk.h"
#include "runtime/rt_resolver.h"
#include "stealth/hashing/hash_djb2.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <windows.h>
#include <winternl.h>

#if WRAITH_USE_PEB_LINKAGE

/* ------------------------------------------------------------------------ */
/*  Mirrored layouts                                                        */
/* ------------------------------------------------------------------------ */

/* MinGW-w64's <winternl.h> only declares one of the three module lists
 * inside PEB_LDR_DATA. Our own mirror keeps the full Win10 1809+ x64
 * layout: byte offsets are stable across Win10/11. */
typedef struct wr_peb_ldr_data {
    ULONG       Length;                       /* +0x00 */
    BOOLEAN     Initialized;                  /* +0x04 */
    PVOID       SsHandle;                     /* +0x08 */
    LIST_ENTRY  InLoadOrderModuleList;        /* +0x10 */
    LIST_ENTRY  InMemoryOrderModuleList;      /* +0x20 */
    LIST_ENTRY  InInitializationOrderModuleList; /* +0x30 */
} wr_peb_ldr_data;

/* DDAG state values (LDR_DDAG_STATE enum). */
#define WR_DDAG_STATE_READY_TO_RUN  9

/* DDAG node mirror. Only the fields ntdll's walk paths actually
 * dereference are typed; the rest stays as raw padding. Total size
 * ~0x60. Offsets validated by static_assert below. */
typedef struct wr_ddag_node {
    LIST_ENTRY  Modules;                      /* +0x00 */
    PVOID       ServiceTagList;               /* +0x10 */
    ULONG       LoadCount;                    /* +0x18 */
    ULONG       LoadWhileUnloadingCount;      /* +0x1c */
    ULONG       LowestLink;                   /* +0x20 */
    ULONG       _pad24;                       /* +0x24 */
    PVOID       Dependencies_Tail;            /* +0x28 (LDRP_CSLIST) */
    PVOID       IncomingDependencies_Tail;    /* +0x30 (LDRP_CSLIST) */
    ULONG       State;                        /* +0x38 (LDR_DDAG_STATE) */
    ULONG       _pad3c;                       /* +0x3c */
    PVOID       CondenseLink_Next;            /* +0x40 (SINGLE_LIST_ENTRY) */
    ULONG       PreorderNumber;               /* +0x48 */
    ULONG       _pad4c;                       /* +0x4c */
    UCHAR       _tail_pad[0x20];              /* +0x50 -> 0x70 */
} wr_ddag_node;

/* LDR_DATA_TABLE_ENTRY mirror, full Win10 1809 .. Win11 24H2 layout.
 * Offsets validated by static_assert below. */
typedef struct wr_ldr_entry {
    LIST_ENTRY      InLoadOrderLinks;             /* +0x00 */
    LIST_ENTRY      InMemoryOrderLinks;           /* +0x10 */
    LIST_ENTRY      InInitializationOrderLinks;   /* +0x20 */
    PVOID           DllBase;                      /* +0x30 */
    PVOID           EntryPoint;                   /* +0x38 */
    ULONG           SizeOfImage;                  /* +0x40 */
    ULONG           _pad44;                       /* +0x44 */
    UNICODE_STRING  FullDllName;                  /* +0x48 */
    UNICODE_STRING  BaseDllName;                  /* +0x58 */
    union {
        UCHAR FlagGroup[4];
        ULONG Flags;
    };                                            /* +0x68 */
    USHORT          ObsoleteLoadCount;            /* +0x6c */
    USHORT          TlsIndex;                     /* +0x6e */
    LIST_ENTRY      HashLinks;                    /* +0x70 */
    ULONG           TimeDateStamp;                /* +0x80 */
    ULONG           _pad84;                       /* +0x84 */
    PVOID           EntryPointActivationContext;  /* +0x88 */
    PVOID           Lock;                         /* +0x90 */
    wr_ddag_node   *DdagNode;                     /* +0x98 */
    LIST_ENTRY      NodeModuleLink;               /* +0xa0 */
    PVOID           LoadContext;                  /* +0xb0 */
    PVOID           ParentDllBase;                /* +0xb8 */
    PVOID           SwitchBackContext;            /* +0xc0 */
    UCHAR           BaseAddressIndexNode[24];     /* +0xc8 (RTL_BALANCED_NODE) */
    UCHAR           MappingInfoIndexNode[24];     /* +0xe0 */
    ULONG_PTR       OriginalBase;                 /* +0xf8 */
    LARGE_INTEGER   LoadTime;                     /* +0x100 */
    ULONG           BaseNameHashValue;            /* +0x108 */
    ULONG           LoadReason;                   /* +0x10c (LDR_DLL_LOAD_REASON) */
    ULONG           ImplicitPathOptions;          /* +0x110 */
    ULONG           ReferenceCount;               /* +0x114 */
    ULONG           DependentLoadFlags;           /* +0x118 */
    UCHAR           SigningLevel;                 /* +0x11c */
    UCHAR           _pad11d[3];                   /* +0x11d */
    UCHAR           _tail_pad[0x40];              /* +0x120 -> 0x160 (slack) */
} wr_ldr_entry;

/* Layout sanity. If any of these fail at compile time, an offset
 * drifted on the new toolchain - investigate before shipping. */
_Static_assert(offsetof(wr_ldr_entry, DllBase)             == 0x30,  "DllBase offset");
_Static_assert(offsetof(wr_ldr_entry, FullDllName)         == 0x48,  "FullDllName offset");
_Static_assert(offsetof(wr_ldr_entry, BaseDllName)         == 0x58,  "BaseDllName offset");
_Static_assert(offsetof(wr_ldr_entry, HashLinks)           == 0x70,  "HashLinks offset");
_Static_assert(offsetof(wr_ldr_entry, TimeDateStamp)       == 0x80,  "TimeDateStamp offset");
_Static_assert(offsetof(wr_ldr_entry, DdagNode)            == 0x98,  "DdagNode offset");
_Static_assert(offsetof(wr_ldr_entry, NodeModuleLink)      == 0xa0,  "NodeModuleLink offset");
_Static_assert(offsetof(wr_ldr_entry, OriginalBase)        == 0xf8,  "OriginalBase offset");
_Static_assert(offsetof(wr_ldr_entry, BaseNameHashValue)   == 0x108, "BaseNameHashValue offset");
_Static_assert(offsetof(wr_ldr_entry, LoadReason)          == 0x10c, "LoadReason offset");
_Static_assert(offsetof(wr_ldr_entry, SigningLevel)        == 0x11c, "SigningLevel offset");

_Static_assert(offsetof(wr_ddag_node, Modules)             == 0x00,  "DDAG.Modules offset");
_Static_assert(offsetof(wr_ddag_node, LoadCount)           == 0x18,  "DDAG.LoadCount offset");
_Static_assert(offsetof(wr_ddag_node, State)               == 0x38,  "DDAG.State offset");

/* Flag bits we care about. */
#define WRAITH_LDR_FLAG_IMAGE_DLL                0x00000004
#define WRAITH_LDR_FLAG_LOAD_NOTIFICATIONS_SENT  0x00080000
#define WRAITH_LDR_FLAG_PROCESS_ATTACH_CALLED    0x00080000

/* LoadReason values. */
#define WR_LOAD_REASON_STATIC_DEPENDENCY  1

/* Forward decl from peb_link_masquerade.c */
wraith_status_t wr_peb_make_masquerade_strings(struct wr_ctx *ctx,
                                                UNICODE_STRING *out_full,
                                                UNICODE_STRING *out_base);

/* ------------------------------------------------------------------------ */
/*  ntdll helpers (resolved lazily)                                         */
/* ------------------------------------------------------------------------ */

typedef NTSTATUS (NTAPI *wr_LdrLockLoaderLock_fn)(ULONG, PULONG, PULONG_PTR);
typedef NTSTATUS (NTAPI *wr_LdrUnlockLoaderLock_fn)(ULONG, ULONG_PTR);
typedef NTSTATUS (NTAPI *wr_RtlHashUnicodeString_fn)(
    PCUNICODE_STRING, BOOLEAN, ULONG, PULONG);

static wr_LdrLockLoaderLock_fn   g_LdrLockLoaderLock   = NULL;
static wr_LdrUnlockLoaderLock_fn g_LdrUnlockLoaderLock = NULL;
static wr_RtlHashUnicodeString_fn g_RtlHashUnicodeString = NULL;

#define LDR_LOCK_LOADER_LOCK_FLAG_RAISE_ON_ERRORS 0x00000001
#define HASH_STRING_ALGORITHM_X65599              1

static wraith_status_t resolve_ntdll_helpers(void)
{
    if (g_LdrLockLoaderLock && g_LdrUnlockLoaderLock && g_RtlHashUnicodeString) {
        return WRAITH_OK;
    }
    void *ntdll = NULL;
    wraith_status_t rc = wr_pebwalk_find_module(wr_djb2_a("ntdll.dll"), &ntdll);
    if (rc != WRAITH_OK) {
        return rc;
    }

    void *p = NULL;
    rc = wr_resolver_lookup_a(ntdll, "LdrLockLoaderLock", &p);
    if (rc != WRAITH_OK) return rc;
    g_LdrLockLoaderLock = (wr_LdrLockLoaderLock_fn)p;

    rc = wr_resolver_lookup_a(ntdll, "LdrUnlockLoaderLock", &p);
    if (rc != WRAITH_OK) return rc;
    g_LdrUnlockLoaderLock = (wr_LdrUnlockLoaderLock_fn)p;

    rc = wr_resolver_lookup_a(ntdll, "RtlHashUnicodeString", &p);
    if (rc != WRAITH_OK) return rc;
    g_RtlHashUnicodeString = (wr_RtlHashUnicodeString_fn)p;

    return WRAITH_OK;
}

/* ------------------------------------------------------------------------ */
/*  Loader-lock guard                                                       */
/* ------------------------------------------------------------------------ */

typedef struct loader_lock_guard {
    BOOL        held;
    ULONG_PTR   cookie;
} loader_lock_guard;

static wraith_status_t loader_lock_acquire(loader_lock_guard *g)
{
    g->held   = FALSE;
    g->cookie = 0;
    if (!g_LdrLockLoaderLock) {
        return WRAITH_E_RT_API_NOT_RESOLVED;
    }
    ULONG state = 0;
    NTSTATUS s = g_LdrLockLoaderLock(0, &state, &g->cookie);
    if (s < 0) {
        return WRAITH_E_RT_API_NOT_RESOLVED;
    }
    g->held = TRUE;
    return WRAITH_OK;
}

static void loader_lock_release(loader_lock_guard *g)
{
    if (!g->held || !g_LdrUnlockLoaderLock) {
        return;
    }
    (void)g_LdrUnlockLoaderLock(0, g->cookie);
    g->held = FALSE;
}

/* ------------------------------------------------------------------------ */
/*  Doubly-linked list primitives                                           */
/* ------------------------------------------------------------------------ */

static void list_insert_head(PLIST_ENTRY head, PLIST_ENTRY entry)
{
    PLIST_ENTRY first = head->Flink;
    entry->Blink = head;
    entry->Flink = first;
    first->Blink = entry;
    head->Flink  = entry;
}

static void list_remove(PLIST_ENTRY entry)
{
    PLIST_ENTRY prev = entry->Blink;
    PLIST_ENTRY next = entry->Flink;
    if (prev) prev->Flink = next;
    if (next) next->Blink = prev;
    entry->Flink = entry->Blink = NULL;
}

/* ------------------------------------------------------------------------ */
/*  Process-wide graveyard                                                  */
/* ------------------------------------------------------------------------ */

typedef struct wr_peb_grave_node {
    wr_ldr_entry              *entry;
    wr_ddag_node              *ddag;
    /* Buffers backing FullDllName/BaseDllName - kept alive so any
     * lingering walker that captured a pointer pre-unlink still
     * sees valid memory. */
    void                      *full_buf;
    void                      *base_buf;
    struct wr_peb_grave_node  *next;
} wr_peb_grave_node;

static wr_peb_grave_node *g_graveyard      = NULL;
static SRWLOCK            g_graveyard_lock = SRWLOCK_INIT;

static void graveyard_push(wr_ldr_entry *e, wr_ddag_node *d)
{
    wr_peb_grave_node *n = (wr_peb_grave_node *)calloc(1, sizeof(*n));
    if (!n) {
        /* Best-effort: if we can't even allocate a graveyard node,
         * leak the entry+ddag silently. Better than a UAF. */
        return;
    }
    n->entry    = e;
    n->ddag     = d;
    n->full_buf = e ? e->FullDllName.Buffer : NULL;
    n->base_buf = e ? e->BaseDllName.Buffer : NULL;

    AcquireSRWLockExclusive(&g_graveyard_lock);
    n->next     = g_graveyard;
    g_graveyard = n;
    ReleaseSRWLockExclusive(&g_graveyard_lock);
}

/* ------------------------------------------------------------------------ */
/*  Public API                                                              */
/* ------------------------------------------------------------------------ */

wraith_status_t wr_peb_link_install(struct wr_ctx *ctx)
{
    if (!ctx) {
        return WRAITH_E_NULL_ARG;
    }
    if (!(ctx->flags & WRAITH_F_PEB_LINKAGE)) {
        return WRAITH_OK;
    }
    if (!ctx->image_base) {
        return WRAITH_E_INVALID_HANDLE;
    }

    PPEB peb = (PPEB)NtCurrentTeb()->ProcessEnvironmentBlock;
    if (!peb || !peb->Ldr) {
        return wr_ctx_fail(ctx, WRAITH_E_RT_PEB_WALK_FAILED,
                           "peb_link: no PEB.Ldr");
    }

    wraith_status_t rc = resolve_ntdll_helpers();
    if (rc != WRAITH_OK) {
        return wr_ctx_fail(ctx, rc,
                           "peb_link: cannot resolve ntdll helpers");
    }

    wr_ldr_entry *e = (wr_ldr_entry *)calloc(1, sizeof(*e));
    if (!e) {
        return wr_ctx_fail(ctx, WRAITH_E_OOM, "peb_link: alloc entry");
    }
    wr_ddag_node *d = (wr_ddag_node *)calloc(1, sizeof(*d));
    if (!d) {
        free(e);
        return wr_ctx_fail(ctx, WRAITH_E_OOM, "peb_link: alloc ddag");
    }

    rc = wr_peb_make_masquerade_strings(ctx, &e->FullDllName, &e->BaseDllName);
    if (rc != WRAITH_OK) {
        free(d);
        free(e);
        return rc;
    }

    /* Image metadata. */
    PIMAGE_NT_HEADERS64 nt = (PIMAGE_NT_HEADERS64)ctx->headers;
    e->DllBase       = ctx->image_base;
    e->EntryPoint    = (PVOID)((uint8_t *)ctx->image_base
                               + nt->OptionalHeader.AddressOfEntryPoint);
    e->SizeOfImage   = nt->OptionalHeader.SizeOfImage;
    e->Flags         = WRAITH_LDR_FLAG_IMAGE_DLL
                     | WRAITH_LDR_FLAG_LOAD_NOTIFICATIONS_SENT
                     | WRAITH_LDR_FLAG_PROCESS_ATTACH_CALLED;
    e->ObsoleteLoadCount = 1;
    e->TlsIndex      = 0;
    e->TimeDateStamp = nt->FileHeader.TimeDateStamp;
    e->HashLinks.Flink = &e->HashLinks;
    e->HashLinks.Blink = &e->HashLinks;

    /* Extended layout fields. */
    e->OriginalBase  = (ULONG_PTR)nt->OptionalHeader.ImageBase;
    GetSystemTimeAsFileTime((FILETIME *)&e->LoadTime);
    e->LoadReason    = WR_LOAD_REASON_STATIC_DEPENDENCY;
    e->ReferenceCount = 1;

    /* BaseNameHashValue: same algorithm ntdll uses internally so
     * LdrpFindLoadedDllByName's hash-fast-path either skips us or
     * matches us coherently. */
    ULONG hashval = 0;
    NTSTATUS hs = g_RtlHashUnicodeString(&e->BaseDllName, TRUE,
                                         HASH_STRING_ALGORITHM_X65599,
                                         &hashval);
    if (hs >= 0) {
        e->BaseNameHashValue = hashval;
    }

    /* DDAG node setup. The Modules list head must be a circular
     * 1-element list whose sole node is our entry's NodeModuleLink. */
    d->LoadCount = 1;
    d->State     = WR_DDAG_STATE_READY_TO_RUN;
    d->Modules.Flink = &e->NodeModuleLink;
    d->Modules.Blink = &e->NodeModuleLink;
    e->NodeModuleLink.Flink = &d->Modules;
    e->NodeModuleLink.Blink = &d->Modules;
    e->DdagNode = d;

    /* Insert into the three PEB lists under LoaderLock. */
    loader_lock_guard guard;
    rc = loader_lock_acquire(&guard);
    if (rc != WRAITH_OK) {
        /* If we cannot take LoaderLock, refuse to install rather
         * than risk a torn-list race. */
        free(e->FullDllName.Buffer);
        free(e->BaseDllName.Buffer);
        free(d);
        free(e);
        return wr_ctx_fail(ctx, rc, "peb_link: LoaderLock acquire failed");
    }

    wr_peb_ldr_data *ldr = (wr_peb_ldr_data *)peb->Ldr;
    list_insert_head(&ldr->InLoadOrderModuleList,
                     &e->InLoadOrderLinks);
    list_insert_head(&ldr->InMemoryOrderModuleList,
                     &e->InMemoryOrderLinks);
    list_insert_head(&ldr->InInitializationOrderModuleList,
                     &e->InInitializationOrderLinks);

    loader_lock_release(&guard);

    ctx->peb_ldr_entry = e;
    return WRAITH_OK;
}

void wr_peb_link_remove(struct wr_ctx *ctx)
{
    if (!ctx || !ctx->peb_ldr_entry) {
        return;
    }
    wr_ldr_entry *e = (wr_ldr_entry *)ctx->peb_ldr_entry;
    wr_ddag_node *d = e->DdagNode;

    /* Best-effort: if helpers haven't been resolved yet, try now.
     * They almost certainly are - install resolved them. */
    (void)resolve_ntdll_helpers();

    loader_lock_guard guard;
    wraith_status_t rc = loader_lock_acquire(&guard);
    /* If LoaderLock acquire fails (extremely unlikely on a healthy
     * process) we still proceed - the alternative is to leak the
     * entry forever, which keeps the listed-but-dead state visible
     * to enumerators indefinitely. */
    (void)rc;

    list_remove(&e->InLoadOrderLinks);
    list_remove(&e->InMemoryOrderLinks);
    list_remove(&e->InInitializationOrderLinks);
    /* Detach from the DDAG.Modules circular list. */
    list_remove(&e->NodeModuleLink);

    loader_lock_release(&guard);

    /* Push to graveyard - DO NOT free. A walker that captured a
     * pointer pre-unlink may still be deref'ing it on another thread.
     * The graveyard is intentionally never drained. */
    graveyard_push(e, d);

    ctx->peb_ldr_entry = NULL;
}

#else  /* !WRAITH_USE_PEB_LINKAGE */

wraith_status_t wr_peb_link_install(struct wr_ctx *ctx)
{
    (void)ctx;
    return WRAITH_OK;
}

void wr_peb_link_remove(struct wr_ctx *ctx)
{
    (void)ctx;
}

#endif  /* WRAITH_USE_PEB_LINKAGE */
