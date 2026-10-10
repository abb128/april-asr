#ifndef APRIL_SETTINGS_H
#define APRIL_SETTINGS_H
#include <errno.h>
#include <stdlib.h>
#include <math.h>
#include "log.h"

/* Read once at model/session creation. Invalid values keep the safe default. */
static inline int april_env_int(const char *name, int fallback, int low, int high) {
    const char *s = getenv(name);
    if (!s || !*s) return fallback;
    char *end;
    errno = 0;
    long value = strtol(s, &end, 10);
    if (errno || *end || value < low || value > high) {
        LOG_WARNING("Invalid %s; using %d", name, fallback);
        return fallback;
    }
    return (int)value;
}
static inline float april_env_float(const char *name, float fallback, float low, float high) {
    const char *s = getenv(name);
    if (!s || !*s) return fallback;
    char *end;
    errno = 0;
    float value = strtof(s, &end);
    if (errno || *end || !isfinite(value) || value < low || value > high) {
        LOG_WARNING("Invalid %s; using %.2f", name, fallback);
        return fallback;
    }
    return value;
}
#endif
