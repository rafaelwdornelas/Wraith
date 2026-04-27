/*
 * tests/integration/test_threadless.c
 *
 * Verifies : threadless execution.
 *
 *  1. Snapshot the calling thread's TID.
 *  2. wr_threadless_run(callback, &state) submits a callback that
 *  records the TID it ran on plus a "I ran" flag.
 *  3. After the call returns:
 *  - the flag must be set (the callback executed)
 *  - the recorded TID must DIFFER from main's TID (it ran on
 *  a thread-pool-owned thread, not on our calling thread)
 *
 * The test does NOT assert that no thread was created - the system
 * thread pool may spin up a worker the first time it sees activity.
 * The IOC we eliminate is "the loader called CreateThread", which is
 * verifiable by code review of wr_threadless_run and is also what
 * the technique is named for.
 */

#include "wraith/wraith.h"
#include "stealth/threadless/threadless.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

typedef struct probe_state {
  DWORD callee_tid;
  int  ran;
} probe_state;

static void probe_cb(void *arg)
{
  probe_state *s = (probe_state *)arg;
  s->callee_tid = GetCurrentThreadId();
  s->ran = 1;
}

int main(void)
{
  DWORD main_tid = GetCurrentThreadId();
  printf("  main thread tid = %lu\n", (unsigned long)main_tid);

  probe_state state = {0};
  wraith_status_t rc = wr_threadless_run(probe_cb, &state);
  if (rc != WRAITH_OK) {
  fprintf(stderr, "FAIL: wr_threadless_run -> %s\n",
  wraith_status_string(rc));
  return 1;
  }

  if (!state.ran) {
  fprintf(stderr, "FAIL: callback did not execute\n");
  return 1;
  }
  printf("PASS: callback executed (callee tid = %lu)\n",
  (unsigned long)state.callee_tid);

  if (state.callee_tid == main_tid) {
  fprintf(stderr,
  "FAIL: callback ran on main thread (no off-loading)\n");
  return 1;
  }
  printf("PASS: callback ran on a distinct system-pool thread "
  "(%lu != %lu)\n",
  (unsigned long)state.callee_tid, (unsigned long)main_tid);
  return 0;
}
