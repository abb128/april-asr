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

#ifndef USE_TINYCTHREAD
#include <threads.h>
#else
#include "tinycthread/tinycthread.h"
#endif

int run_pt(void *userdata);

struct ProcThread_i {
    volatile bool initialized;
    volatile bool terminating;
    volatile int flags;

    thrd_t thrd;

    cnd_t cond;
    mtx_t mutex;

    ProcThreadCallback callback;
    void *userdata;
};

ProcThread pt_create(ProcThreadCallback callback, void *userdata) {
    ProcThread thread = (ProcThread)calloc(1, sizeof(struct ProcThread_i));
    if(thread == NULL) return NULL;

    thread->initialized = false;
    thread->callback = callback;
    thread->userdata = userdata;

    if(cnd_init(&thread->cond) != thrd_success){
        LOG_WARNING("Failed to initialize cnd_t");
        pt_free(thread);
        return NULL;
    }

    if(mtx_init(&thread->mutex, mtx_plain) != thrd_success){
        LOG_WARNING("Failed to initialize mutex");
        pt_free(thread);
        return NULL;
    }

    if(thrd_create(&thread->thrd, run_pt, thread) != thrd_success) {
        LOG_WARNING("Failed to start thread");
        pt_free(thread);
        return NULL;
    }

    return thread;
}

void pt_raise(ProcThread thread, int flag) {
    if(mtx_lock(&thread->mutex) != thrd_success){
        LOG_ERROR("Failed to lock mutex in pt_raise!");
    }

    thread->flags |= flag;

    if(mtx_unlock(&thread->mutex) != thrd_success){
        LOG_ERROR("Failed to unlock mutex in pt_raise!");
    }

    while(!thread->initialized) {}
    if(cnd_signal(&thread->cond) != thrd_success){
        LOG_ERROR("Failed to signal cond!");
    }
}

void pt_terminate(ProcThread thread) {
    if(thread->terminating) return;

    thread->terminating = true;
    for(int i=0; i<8; i++) pt_raise(thread, PT_FLAG_KILL);

    int res;
    if(thrd_join(thread->thrd, &res) != thrd_success){
        LOG_ERROR("Failed to join thread!");
        return;
    }

    if(res != 0){
        LOG_ERROR("Thread exited with non-zero status %d!", res);
    }
}

void pt_free(ProcThread thread) {
    if(thread == NULL) return;
    
    pt_terminate(thread);

    mtx_destroy(&thread->mutex);
    cnd_destroy(&thread->cond);

    free(thread);
}



int run_pt(void *userdata){
    ProcThread thread = (ProcThread)userdata;

    if(mtx_lock(&thread->mutex) != thrd_success){
        LOG_ERROR("Failed to lock mutex!");
        return 1;
    }

    for(;;){
        thread->initialized = true;
        if(cnd_wait(&thread->cond, &thread->mutex) != thrd_success) {
            LOG_ERROR("Failed to wait for cond!");
            return 2;
        }

        int flags = thread->flags;
        thread->flags = 0;

        if(mtx_unlock(&thread->mutex) != thrd_success) {
            LOG_ERROR("Failed to unlock mutex!");
            return 3;
        }

        if((flags & PT_FLAG_KILL) || (thread->terminating)) return 0;

        thread->callback(thread->userdata, flags);

        if((thread->flags & PT_FLAG_KILL) || (thread->terminating)) return 0;

        if(mtx_lock(&thread->mutex) != thrd_success){
            LOG_ERROR("Failed to lock mutex! 1");
            return 4;
        }
    }
}