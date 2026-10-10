#ifndef APRIL_THREAD_COMPAT_H
#define APRIL_THREAD_COMPAT_H
#ifdef USE_TINYCTHREAD
#include "tinycthread/tinycthread.h"
#else
#include <threads.h>
#endif
#endif
