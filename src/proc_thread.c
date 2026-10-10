/*
 * Copyright (C) 2022 abb128
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include <stdbool.h>
#include <stdlib.h>
#include "common.h"
#include "log.h"
#include "proc_thread.h"
#include "thread_compat.h"

struct ProcThread_i {
    int flags;
    bool working;
    bool terminating;
    bool thrd_init, cond_init, mutex_init;
    thrd_t thrd;
    cnd_t cond;
    mtx_t mutex;
    ProcThreadCallback callback;
    void *userdata;
};

static int run_pt(void *userdata) {
    ProcThread thread = userdata;
    mtx_lock(&thread->mutex);
    for (;;) {
        /* The predicate is the queue, not the notification. A signal received
           during inference must be processed before going back to sleep. */
        while (!thread->flags && !thread->terminating)
            cnd_wait(&thread->cond, &thread->mutex);
        if (thread->terminating) break;
        int flags = thread->flags;
        thread->flags = 0;
        thread->working = true;
        mtx_unlock(&thread->mutex);
        thread->callback(thread->userdata, flags);
        mtx_lock(&thread->mutex);
        thread->working = false;
        cnd_broadcast(&thread->cond);
    }
    mtx_unlock(&thread->mutex);
    return 0;
}

ProcThread pt_create(ProcThreadCallback callback, void *userdata) {
    if (!callback) return NULL;
    ProcThread thread = calloc(1, sizeof(*thread));
    if (!thread) return NULL;
    thread->callback = callback;
    thread->userdata = userdata;
    if (cnd_init(&thread->cond) != thrd_success) goto fail;
    thread->cond_init = true;
    if (mtx_init(&thread->mutex, mtx_plain) != thrd_success) goto fail;
    thread->mutex_init = true;
    if (thrd_create(&thread->thrd, run_pt, thread) != thrd_success) goto fail;
    thread->thrd_init = true;
    return thread;
fail:
    pt_free(thread);
    return NULL;
}

void pt_raise(ProcThread thread, int flag) {
    if (!thread) return;
    mtx_lock(&thread->mutex);
    if (!thread->terminating) {
        thread->flags |= flag;
        cnd_broadcast(&thread->cond);
    }
    mtx_unlock(&thread->mutex);
}

int pt_take_flags(ProcThread thread, int mask) {
    if (!thread) return 0;
    mtx_lock(&thread->mutex);
    int flags = thread->flags & mask;
    thread->flags &= ~mask;
    mtx_unlock(&thread->mutex);
    return flags;
}

bool pt_wait_idle(ProcThread thread) {
    if (!thread) return true;
    /* Waiting inside the result callback would deadlock the same worker. */
    if (thrd_equal(thrd_current(), thread->thrd)) return false;
    mtx_lock(&thread->mutex);
    while (thread->flags || thread->working)
        cnd_wait(&thread->cond, &thread->mutex);
    mtx_unlock(&thread->mutex);
    return true;
}

void pt_free(ProcThread thread) {
    if (!thread) return;
    if (thread->thrd_init) {
        if (!pt_wait_idle(thread)) {
            LOG_ERROR("Do not free a session from its recognition callback");
            return;
        }
        mtx_lock(&thread->mutex);
        thread->terminating = true;
        cnd_signal(&thread->cond);
        mtx_unlock(&thread->mutex);
        thrd_join(thread->thrd, NULL);
    }
    if (thread->mutex_init) mtx_destroy(&thread->mutex);
    if (thread->cond_init) cnd_destroy(&thread->cond);
    free(thread);
}
