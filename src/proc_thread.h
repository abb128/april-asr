#ifndef _APRIL_PROC_THREAD
#define _APRIL_PROC_THREAD

#define PT_FLAG_KILL 1
#define PT_FLAG_AUDIO 2
#define PT_FLAG_FLUSH 4

struct ProcThread_i;

typedef struct ProcThread_i * ProcThread;
typedef void(*ProcThreadCallback)(void*, int);

ProcThread pt_create(ProcThreadCallback callback, void *userdata);
void pt_raise(ProcThread thread, int flag);
void pt_free(ProcThread thread);

#endif