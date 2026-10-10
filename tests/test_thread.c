/* Regression: work raised during a callback must run without another signal. */
#include <assert.h>
#include <stdlib.h>
#include "proc_thread.h"
#include "thread_compat.h"

struct State { mtx_t lock; cnd_t changed; int calls, flags; bool released; };
static void callback(void *arg, int flags) {
    struct State *s = arg;
    mtx_lock(&s->lock);
    ++s->calls;
    s->flags |= flags;
    cnd_broadcast(&s->changed);
    while (s->calls == 1 && !s->released) cnd_wait(&s->changed, &s->lock);
    mtx_unlock(&s->lock);
}
int main(void) {
    for (int n = 0; n < 200; ++n) {
        struct State s = {0};
        assert(mtx_init(&s.lock, mtx_plain) == thrd_success);
        assert(cnd_init(&s.changed) == thrd_success);
        ProcThread thread = pt_create(callback, &s);
        assert(thread);
        pt_raise(thread, PT_FLAG_AUDIO);
        mtx_lock(&s.lock);
        while (!s.calls) cnd_wait(&s.changed, &s.lock);
        pt_raise(thread, PT_FLAG_FLUSH);
        s.released = true;
        cnd_broadcast(&s.changed);
        mtx_unlock(&s.lock);
        assert(pt_wait_idle(thread));
        assert(s.calls == 2 && s.flags == (PT_FLAG_AUDIO | PT_FLAG_FLUSH));
        pt_free(thread);
        mtx_destroy(&s.lock);
        cnd_destroy(&s.changed);
    }
    /* pt_take_flags clears only the requested pending flags. */
    struct State s = {0};
    assert(mtx_init(&s.lock, mtx_plain) == thrd_success);
    assert(cnd_init(&s.changed) == thrd_success);
    ProcThread thread = pt_create(callback, &s);
    assert(thread);
    pt_raise(thread, PT_FLAG_AUDIO);
    mtx_lock(&s.lock);
    while (!s.calls) cnd_wait(&s.changed, &s.lock);
    pt_raise(thread, PT_FLAG_AUDIO | PT_FLAG_OVERFLOW);
    assert(pt_take_flags(thread, PT_FLAG_OVERFLOW) == PT_FLAG_OVERFLOW);
    assert(pt_take_flags(thread, PT_FLAG_OVERFLOW) == 0);
    s.released = true;
    cnd_broadcast(&s.changed);
    mtx_unlock(&s.lock);
    assert(pt_wait_idle(thread));
    assert(s.calls == 2 && s.flags == PT_FLAG_AUDIO);
    pt_free(thread);
    mtx_destroy(&s.lock);
    cnd_destroy(&s.changed);
    return 0;
}
