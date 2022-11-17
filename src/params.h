#ifndef _APRIL_PARAMS
#define _APRIL_PARAMS

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct ModelParameters {
    int batch_size;
    int segment_size;
    int segment_step;
    int mel_features;
    int sample_rate;

    int frame_shift_ms;
    int frame_length_ms;
    bool round_pow2;
    int mel_low;
    int mel_high;
    bool snip_edges;

    int blank_id;

    int token_count;
    size_t token_length;
    
    char *tokens;
} ModelParameters;

char *get_token(ModelParameters *params, size_t token_index);

// Returns false if reading failed
bool read_params(ModelParameters *params, const char *path);
bool read_params_from_fd(ModelParameters *params, FILE *fd);

void free_params(ModelParameters *params);

#endif