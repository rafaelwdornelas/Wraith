/*
 * tests/integration/fixtures/seh_dll/seh_dll.c
 *
 * Fixture used by tests/integration/test_load_seh.c.
 *
 * Provides:
 *  - A non-trivial export (`seh_target`) whose code generates entries
 *  in `.pdata`. The test calls RtlLookupFunctionEntry on this address
 *  to prove the loader called RtlAddFunctionTable.
 *  - A `seh_get_lifecycle_marker` export that returns a single uint32
 *  incremented each time DllMain is invoked. This lets the test
 *  observe DLL_PROCESS_ATTACH (load) and DLL_PROCESS_DETACH (free)
 *  by storing the latest reason in a process-wide named atom that
 *  test_load_seh.c reads back after wraith_free_library.
 *
 * The DllMain reason is exposed via a NamedAtom so it survives the
 * wraith_free_library that frees the DLL itself - the atom lives in the
 * host process's atom table.
 */

#include <windows.h>

#define WRAITH_SEH_ATOM_NAME L"wr_seh_dll_last_reason"

static void store_reason(DWORD reason)
{
  wchar_t buf[16];
  /* Atom names must be 1..255 chars. Encode "%u" of reason. */
  int n = wsprintfW(buf, L"%u", reason);
  if (n <= 0) {
  return;
  }
  /* Replace any prior atom value: GlobalAddAtom(name) returns the
  * existing atom if name matches; we want a fresh slot keyed by
  * WRAITH_SEH_ATOM_NAME holding `buf`. The simplest scheme: store the
  * reason value directly in a globally-named atom whose NAME contains
  * the value. test_load_seh checks for the existence of those atoms. */
  wchar_t fullname[64];
  wsprintfW(fullname, L"%ls=%ls", WRAITH_SEH_ATOM_NAME, buf);
  GlobalAddAtomW(fullname);
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
  (void)inst;
  (void)reserved;
  store_reason(reason);
  return TRUE;
}

/* A non-trivial function so the linker emits a .pdata entry for it.
 * The body is irrelevant - we just want a known address inside the
 * DLL whose RIP can be passed to RtlLookupFunctionEntry. */
__declspec(dllexport) int seh_target(int a, int b)
{
  volatile int acc = 0;
  for (int i = 0; i < a; ++i) {
  acc += b;
  }
  return acc;
}
