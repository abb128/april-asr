#ifndef _APRIL_MODEL_FILE
#define _APRIL_MODEL_FILE

#include "params.h"

/*  [0]: encoder
    [1]: decoder
    [2]: joiner   */
#define LSTM_TRANSDUCER_STATELESS_NETWORK_COUNT 3

typedef enum ModelType {
    MODEL_UNKNOWN = 0,
    MODEL_LSTM_TRANSDUCER_STATELESS = 1,
    MODEL_MAX = 2,
} ModelType;


struct ModelFile_i;
typedef struct ModelFile_i * ModelFile;

// May return NULL if loading failed
ModelFile model_read(const char *path);

ModelType model_type(ModelFile model);

// Pointers are freed when free_model is called
const char *model_name(ModelFile model);
const char *model_desc(ModelFile model);

bool model_read_params(ModelFile model, ModelParameters *out);

size_t model_network_count(ModelFile model);
size_t model_network_size(ModelFile model, size_t index);
size_t model_network_read(ModelFile model, size_t index, void *data, size_t data_len);

// Transfers ownership of strings if provided, and frees model.
// If a char ** was provided, the caller must take responsibility to
// eventually free the char * that was given.
void transfer_strings_and_free_model(ModelFile model, char **out_name, char **out_desc, char **out_lang);

// Equivalent to transfer_strings_and_free_model(model, NULL, NULL, NULL)
void free_model(ModelFile model);

#endif