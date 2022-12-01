#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"
#include "audio_provider.h"

#define MIN(A, B) ((A) < (B)) ? (A) : (B)

#define MAX_AUDIO 48000
struct AudioProvider_i {
    short audio[MAX_AUDIO];

    // Current index to read data
    _Atomic size_t head;

    // Current index to write data
    _Atomic size_t tail;
};

AudioProvider ap_create() {
    AudioProvider ap = (AudioProvider)calloc(1, sizeof(struct AudioProvider_i));
    if(ap == NULL) return NULL;

    return ap;
}

void ap_push_audio(AudioProvider ap, const short *audio, size_t short_count) {
    if(short_count > (MAX_AUDIO / 2)) {
        LOG_WARNING("AudioProvider is being given a lot of audio (%lu samples), please reduce", short_count);
    }

    size_t audio_head = 0;
    while(short_count > 0){
        bool will_overflow = ((ap->tail < ap->head) && ((ap->tail + short_count) > ap->head))
                          || ((ap->tail > ap->head) && ((ap->tail + short_count) > (MAX_AUDIO + ap->head)));
        if(will_overflow){
            LOG_WARNING("Can't keep up! Attempted to write %lu samples", short_count);
            return;
        }

        size_t num_shorts_to_write = MIN(short_count, MAX_AUDIO - ap->tail);
        memcpy(&ap->audio[ap->tail], &audio[audio_head], num_shorts_to_write * 2);

        ap->tail = (ap->tail + num_shorts_to_write) % MAX_AUDIO;
        short_count -= num_shorts_to_write;
        audio_head += num_shorts_to_write;
    }
}

short *ap_pull_audio(AudioProvider ap, size_t *short_count) {
    if(ap->tail == ap->head) {
        *short_count = 0;
        return NULL;
    }

    size_t num_samples_available = (ap->tail > ap->head) ? (ap->tail - ap->head) : ((MAX_AUDIO - ap->head) + ap->tail);
    if(*short_count != 0){
        num_samples_available = MIN(num_samples_available, *short_count);
    }

    num_samples_available = MIN(num_samples_available, (MAX_AUDIO - ap->head));

    *short_count = num_samples_available;

    return &ap->audio[ap->head];
}


void ap_pull_audio_finish(AudioProvider ap, size_t short_count) {
    ap->head = (ap->head + short_count) % MAX_AUDIO;
}

void ap_free(AudioProvider ap) {
    free(ap);
}