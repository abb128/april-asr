#ifndef _APRIL_API
#define _APRIL_API

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct AprilASRModel_i;
struct AprilASRSession_i;

typedef struct AprilASRModel_i * AprilASRModel;
typedef struct AprilASRSession_i * AprilASRSession;

// Must be called once before calling any other functions
void aam_api_init(void);

// Creates a model given a path. May return NULL if loading failed.
AprilASRModel aam_create_model(const char *model_path);

// Get the sample rate of model in Hz. For example, may return 16000
size_t aam_get_sample_rate(AprilASRModel model);

// Caller must ensure all sessions backed by model are freed before model
// is freed
void aam_free(AprilASRModel model);


typedef struct AprilUUID {
    uint64_t lower;
    uint64_t upper;
} AprilUUID;

enum AprilResultType {
    APRIL_RESULT_UNKNOWN = 0,

     // TODO provide full  text not juts append, else going back is impossible
    APRIL_RESULT_RECOGNITION_APPEND,
    APRIL_RESULT_RECOGNITION_LOOKAHEAD
};


// (void *userdata, AprilResultType result_type, size_t text_size, const char *text)
// Text pointer is guaranteed to be valid only for the duration of the call.
typedef void(*AprilRecognitionResultHandler)(void*, int, size_t, const char*);

// Creates a session with a given model. A model may have many sessions
// associated with it. The handler will be called with events as they occur,
// it may be called from a different thread. UUID may be NULL or a pointer to a
// unique ID identifying the speaker to restore context and state.
AprilASRSession aas_create_session(
    AprilASRModel model,
    AprilRecognitionResultHandler handler,
    void *userdata,
    AprilUUID *uuid
);

// Feed PCM16 audio data to the session, must be single-channel and sampled
// to the sample rate given in `aam_get_sample_rate`
void aas_feed_pcm16(AprilASRSession session, short *pcm16, size_t short_count);

// Frees the session, this must be called for all sessions before freeing
// the model
void aas_free(AprilASRSession session);

#ifdef __cplusplus
}
#endif

#endif