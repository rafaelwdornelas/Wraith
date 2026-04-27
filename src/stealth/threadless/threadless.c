/*
 * src/stealth/threadless/threadless.c
 *
 * Implementation. Adapts the user-supplied callback to the Win32
 * threadpool's PTP_WORK_CALLBACK signature, submits the work item,
 * and blocks until completion via WaitForThreadpoolWorkCallbacks.
 *
 * Wine note: wine 9.0 implements CreateThreadpoolWork on top of its
 * own thread pool subsystem - the system pool may create a new
 * physical thread the first time it's used, but subsequent
 * dispatches reuse the pool. Either way, our code never issues a
 * CreateThread call.
 */

#include "stealth/threadless/threadless.h"

#include <stdlib.h>
#include <windows.h>

typedef struct work_ctx {
  wr_threadless_fn fn;
  void  *arg;
} work_ctx;

static VOID CALLBACK threadless_trampoline(PTP_CALLBACK_INSTANCE instance,
  PVOID context, PTP_WORK work)
{
  (void)instance;
  (void)work;
  work_ctx *c = (work_ctx *)context;
  if (c && c->fn) {
  c->fn(c->arg);
  }
}

wraith_status_t wr_threadless_run(wr_threadless_fn fn, void *arg)
{
  if (!fn) {
  return WRAITH_E_NULL_ARG;
  }

  work_ctx ctx = { fn, arg };

  PTP_WORK work = CreateThreadpoolWork(threadless_trampoline, &ctx, NULL);
  if (!work) {
  return WRAITH_E_STEALTH_INSTALL;
  }

  SubmitThreadpoolWork(work);
  /* The wait blocks the calling thread inside NtWaitForSingleObject
  * (kernel-side) until the trampoline returns - so even though we
  * don't create a thread ourselves, we also don't burn the calling
  * thread spinning. */
  WaitForThreadpoolWorkCallbacks(work, FALSE);
  CloseThreadpoolWork(work);
  return WRAITH_OK;
}
