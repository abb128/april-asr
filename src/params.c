#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "params.h"

char *get_token(ModelParameters *params, size_t token_index){
    return &params->tokens[params->token_length * token_index];
}

void read_params(ModelParameters *params, const char *params_file) {
    char buffer[64];
    int32_t *i32_ptr = (int32_t*)buffer;
    int64_t *i64_ptr = (int64_t*)buffer;

    FILE *fd = fopen(params_file, "r");
    fread(buffer, sizeof(int64_t), 1, fd);

    // 'PARAMS\0\0'
    const int64_t expected = 0x0000534D41524150L;
    assert(i64_ptr[0] == expected);

    fread(buffer, sizeof(int32_t), 7, fd);

    params->batch_size   = i32_ptr[0];
    params->segment_size = i32_ptr[1];
    params->segment_step = i32_ptr[2];
    params->mel_features = i32_ptr[3];
    params->sample_rate  = i32_ptr[4];
    params->token_count  = i32_ptr[5];
    params->blank_id     = i32_ptr[6];

    assert(params->batch_size == 1);
    assert((params->segment_size > 0) && (params->segment_size < 100));
    assert((params->segment_step > 0) && (params->segment_step < 100) && (params->segment_step <= params->segment_size));
    assert((params->mel_features > 0) && (params->mel_features < 256));
    assert((params->sample_rate > 0) && (params->sample_rate < 144000));
    assert((params->token_count > 0) && (params->token_count < 16384));
    assert((params->blank_id >= 0) && (params->blank_id < params->token_count));
    

    // Read all piece lengths and figure out the maximum
    size_t tokens_start = ftell(fd);
    
    params->token_length = 0;
    for(int i=0; i<params->token_count; i++){
        fread(buffer, sizeof(int32_t), 1, fd);
        int token_len = i32_ptr[0];
        if(token_len > params->token_length)
            params->token_length = token_len;
        
        fseek(fd, token_len, SEEK_CUR);
    }
    params->token_length += 1; // for '\0' byte

    // Allocate the memory
    params->tokens = (char *)calloc(params->token_count, params->token_length);

    // Rewind back and read
    fseek(fd, tokens_start, SEEK_SET);
    for(int i=0; i<params->token_count; i++){
        fread(buffer, sizeof(int32_t), 1, fd);
        int token_len = i32_ptr[0];
        
        assert(token_len < params->token_length);
        fread(get_token(params, i), 1, token_len, fd);
    }
}

void free_params(ModelParameters *params){
    free(params->tokens);
}
