/*
 * tests/integration/fixtures/payload_dll/payload.c
 *
 * Minimal payload DLL used by the integration test suite. Exposes a
 * single deterministic export so tests can confirm the loader
 * dispatched the call correctly post-load.
 */

#include <windows.h>

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
  (void)inst;
  (void)reason;
  (void)reserved;
  return TRUE;
}

__declspec(dllexport) int addNumbers(int a, int b)
{
  return a + b;
}
