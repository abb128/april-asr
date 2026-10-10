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
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "audio_provider.h"
#include "thread_compat.h"

#define MAX_AUDIO 48000
#define MAX_FLUSH_MARKS 64
struct AudioProvider_i {
    short audio[MAX_AUDIO];
    size_t head, tail, count;
    /* Absolute sample positions; marks are positions in pushed audio. */
    uint64_t pushed, pulled;
    uint64_t marks[MAX_FLUSH_MARKS];
    size_t mark_head, mark_count;
    mtx_t mutex;
};

AudioProvider ap_create(void) {
    AudioProvider ap = calloc(1, sizeof(*ap));
    if (!ap) return NULL;
    if (mtx_init(&ap->mutex, mtx_plain) != thrd_success) {
        free(ap);
        return NULL;
    }
    return ap;
}

bool ap_push_audio(AudioProvider ap, const short *audio, size_t count) {
    if (!ap || (!audio && count)) return false;
    mtx_lock(&ap->mutex);
    if (count > MAX_AUDIO - ap->count) {
        mtx_unlock(&ap->mutex);
        return false;
    }
    /* Publish the complete write in one transaction, including wraparound. */
    size_t first = count < MAX_AUDIO - ap->tail ? count : MAX_AUDIO - ap->tail;
    if (first) memcpy(ap->audio + ap->tail, audio, first * sizeof(short));
    if (count > first) memcpy(ap->audio, audio + first, (count - first) * sizeof(short));
    ap->tail = (ap->tail + count) % MAX_AUDIO;
    ap->count += count;
    ap->pushed += count;
    mtx_unlock(&ap->mutex);
    return true;
}

short *ap_pull_audio(AudioProvider ap, size_t *count) {
    mtx_lock(&ap->mutex);
    size_t available = ap->count;
    if (*count && available > *count) available = *count;
    if (available > MAX_AUDIO - ap->head) available = MAX_AUDIO - ap->head;
    if (ap->mark_count) {
        uint64_t until_mark = ap->marks[ap->mark_head] - ap->pulled;
        if (available > until_mark) available = (size_t)until_mark;
    }
    *count = available;
    short *result = available ? ap->audio + ap->head : NULL;
    mtx_unlock(&ap->mutex);
    /* The single consumer owns these slots until pull_audio_finish. */
    return result;
}

void ap_pull_audio_finish(AudioProvider ap, size_t count) {
    mtx_lock(&ap->mutex);
    if (count <= ap->count) {
        ap->head = (ap->head + count) % MAX_AUDIO;
        ap->count -= count;
        ap->pulled += count;
    }
    mtx_unlock(&ap->mutex);
}

bool ap_mark_flush(AudioProvider ap) {
    if (!ap) return false;
    bool ok = true;
    mtx_lock(&ap->mutex);
    size_t last = (ap->mark_head + ap->mark_count + MAX_FLUSH_MARKS - 1) % MAX_FLUSH_MARKS;
    if (ap->mark_count && ap->marks[last] == ap->pushed) {
        /* Nothing was pushed since the last flush. */
    } else if (ap->mark_count == MAX_FLUSH_MARKS) {
        ap->marks[last] = ap->pushed;
        ok = false;
    } else {
        ap->marks[(ap->mark_head + ap->mark_count++) % MAX_FLUSH_MARKS] = ap->pushed;
    }
    mtx_unlock(&ap->mutex);
    return ok;
}

bool ap_take_flush(AudioProvider ap) {
    if (!ap) return false;
    mtx_lock(&ap->mutex);
    bool reached = ap->mark_count && ap->marks[ap->mark_head] == ap->pulled;
    if (reached) {
        ap->mark_head = (ap->mark_head + 1) % MAX_FLUSH_MARKS;
        ap->mark_count--;
    }
    mtx_unlock(&ap->mutex);
    return reached;
}

size_t ap_pending_samples(AudioProvider ap) {
    if (!ap) return 0;
    mtx_lock(&ap->mutex);
    size_t count = ap->count;
    mtx_unlock(&ap->mutex);
    return count;
}

void ap_free(AudioProvider ap) {
    if (!ap) return;
    mtx_destroy(&ap->mutex);
    free(ap);
}
