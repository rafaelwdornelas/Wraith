/*
 * src/stealth/threadless/threadless.h
 *
 * Threadless execution primitive.
 *
 * Submits a callback to the OS-managed thread pool via
 * CreateThreadpoolWork + SubmitThreadpoolWork. The callback runs on
 * a thread the system already owns - we never call CreateThread,
 * RtlCreateUserThread, or any equivalent that would generate a
 * "new thread in suspicious process" telemetry event.
 *
 * The published Threadless Inject technique (ZeroMemoryEx) goes
 * further by hijacking an existing TpAllocWork entry in another
 * thread's pool, achieving cross-process threadless execution. This
 * file ships the in-process variant which is sufficient for the
 * loader's sleep awakener and for general "run a callback off this
 * thread" plumbing.
 */

#ifndef WRAITH_THREADLESS_H
#define WRAITH_THREADLESS_H

#include "wraith/wraith_status.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*wr_threadless_fn)(void *arg);

/* Submit `fn(arg)` to the thread pool and wait for it to complete.
 * Returns S_OK only when the callback ran to completion. The
 * calling thread parks in the wait, so RIP stays in kernel-side
 * NtWaitForSingleObject during the work. */
wraith_status_t wr_threadless_run(wr_threadless_fn fn, void *arg);

#ifdef __cplusplus
}
#endif

#endif  /* WRAITH_THREADLESS_H */
