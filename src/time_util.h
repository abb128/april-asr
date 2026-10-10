#ifndef APRIL_TIME_UTIL_H
#define APRIL_TIME_UTIL_H
#ifdef _WIN32
#include <windows.h>
static inline double april_now_ms(void) {
    LARGE_INTEGER count, frequency;
    QueryPerformanceCounter(&count);
    QueryPerformanceFrequency(&frequency);
    return (double)count.QuadPart * 1000.0 / (double)frequency.QuadPart;
}
#else
#include <time.h>
static inline double april_now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0;
}
#endif
#endif
