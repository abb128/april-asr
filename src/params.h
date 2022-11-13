#ifndef _APRIL_PARAMS
#define _APRIL_PARAMS

#include <stddef.h>
#include <stdint.h>

typedef struct ModelParameters {
    int batch_size;
    int segment_size;
    int segment_step;
    int mel_features;
    int sample_rate;
    int blank_id;

    int token_count;
    size_t token_length;
    
    char *tokens;
} ModelParameters;

char *get_token(ModelParameters *params, size_t token_index);
void read_params(ModelParameters *params, const char *params_file);
void free_params(ModelParameters *params);

#endif