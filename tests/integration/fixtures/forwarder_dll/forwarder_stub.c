/*
 * tests/integration/fixtures/forwarder_dll/forwarder_stub.c
 *
 * MinGW refuses to link a shared library with no .text content, so this
 * empty translation unit just provides a DllMain that returns TRUE. All
 * actual exports come from forwarder_dll.def.
 */

#include <windows.h>

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
  (void)inst;
  (void)reason;
  (void)reserved;
  return TRUE;
}
