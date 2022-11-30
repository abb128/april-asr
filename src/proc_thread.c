#include <threads.h>
#include <stdbool.h>
#include <stdlib.h>
#include "log.h"
#include "proc_thread.h"

int run_pt(void *userdata);

struct ProcThread_i {
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

    if(cnd_signal(&thread->cond) != thrd_success){
        LOG_ERROR("Failed to signal cond!");
    }
}

void pt_terminate(ProcThread thread) {
    if(thread->terminating) return;

    thread->terminating = true;
    pt_raise(thread, PT_FLAG_KILL);

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
        if(cnd_wait(&thread->cond, &thread->mutex) != thrd_success) {
            LOG_ERROR("Failed to wait for cond!");
            continue;
        }

        int flags = thread->flags;
        thread->flags = 0;

        if(mtx_unlock(&thread->mutex) != thrd_success) {
            LOG_ERROR("Failed to unlock mutex!");
            return 1;
        }

        if((flags & PT_FLAG_KILL) || (thread->terminating)) return 0;

        thread->callback(thread->userdata, flags);

        if(mtx_lock(&thread->mutex) != thrd_success){
            LOG_ERROR("Failed to lock mutex! 1");
            return 1;
        }
    }
}