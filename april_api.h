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

#ifndef _APRIL_API
#define _APRIL_API

#include <stddef.h>
#include <stdint.h>


#ifdef _APRIL_EXPORT
#if defined(_WIN32) || defined(__WIN32__) || defined(__WINDOWS__)
#define APRIL_EXPORT __declspec(dllexport)
#else
#define APRIL_EXPORT __attribute__((visibility("default")))
#endif
#else

#if defined(_WIN32) || defined(__WIN32__) || defined(__WINDOWS__)
#ifdef APRIL_DLL_IMPORT
#define APRIL_EXPORT __declspec(dllimport)
#else
#define APRIL_EXPORT
#endif
#else
#define APRIL_EXPORT
#endif

#endif

#ifdef __cplusplus
extern "C" {
#endif

struct AprilASRModel_i;
struct AprilASRSession_i;

typedef struct AprilASRModel_i * AprilASRModel;
typedef struct AprilASRSession_i * AprilASRSession;

#define APRIL_VERSION 1

/* Must be called once before calling any other functions */
/* Version must be set to APRIL_VERSION like so: aam_api_init(APRIL_VERSION) */
APRIL_EXPORT void aam_api_init(int version);

/* Creates a model given a path. Returns NULL if loading failed. */
APRIL_EXPORT AprilASRModel aam_create_model(const char *model_path);

/* Get the name/desc/lang of the model. The pointers are valid for the
   lifetime of the model (i.e. until aam_free is called on the model) */
APRIL_EXPORT const char *aam_get_name(AprilASRModel model);
APRIL_EXPORT const char *aam_get_description(AprilASRModel model);
APRIL_EXPORT const char *aam_get_language(AprilASRModel model);

/* Get the sample rate of model in Hz. For example, may return 16000 */
APRIL_EXPORT size_t aam_get_sample_rate(AprilASRModel model);

/* Caller must ensure all sessions backed by model are freed before model
   is freed */
APRIL_EXPORT void aam_free(AprilASRModel model);



/* Unique identifier for a speaker. For example, it may be a hash of the
   speaker's name. This may be provided to `aas_create_session` for saving
   and restoring state. If set to all zeros, will be ignored.
   Currently not implemented, has no effect. */
typedef struct AprilSpeakerID {
    uint8_t data[16];
} AprilSpeakerID;

typedef enum AprilResultType {
    APRIL_RESULT_UNKNOWN = 0,

    /* Specifies that the result is only partial, and a future call will
       contain much of the same text but updated. */
    APRIL_RESULT_RECOGNITION_PARTIAL,

    /* Specifies that the result is final. Future calls will start from
       empty and will not contain any of the given text. */
    APRIL_RESULT_RECOGNITION_FINAL,

    /* If in non-synchronous mode, this may be called when the internal
       audio buffer is full and processing can't keep up.
       It will be called with count = 0, tokens = NULL */
    APRIL_RESULT_ERROR_CANT_KEEP_UP,

    /* Specifies that there has been some silence. Will not be called
       repeatedly.
       It will be called with count = 0, tokens = NULL */
    APRIL_RESULT_SILENCE
} AprilResultType;

typedef enum AprilTokenFlagBits {
    /* If set, this token marks the start of a new word. In English, this
       is equivalent to (token[0] == ' ') */
    APRIL_TOKEN_FLAG_WORD_BOUNDARY_BIT = 0x00000001
} AprilTokenFlagBits;

typedef struct AprilToken {
    /* Null-terminated string. The string contains its own formatting,
       for example it may start with a space to denote a new word, or
       not start with a space to denote the next part of a word.
       The pointer will remain valid for the lifetime of the model. */
    const char *token;

    /* Log probability of this being the correct token */
    float logprob;

    /* See AprilTokenFlagBits */
    AprilTokenFlagBits flags;

    /* The millisecond at which this was emitted. Counting is based on how much
       audio is being fed (time is not advanced when the session is not being
       given audio) */
    size_t time_ms;

    void *reserved;
} AprilToken;

/* (void* userdata, AprilResultType result, size_t count, const AprilToken *tokens);
   The tokens pointer is only valid for the duration of the call. It may be
   count may be 0, and if so then tokens may be NULL. */
typedef void(*AprilRecognitionResultHandler)(void*, AprilResultType, size_t, const AprilToken*);


typedef enum AprilConfigFlagBits {
    APRIL_CONFIG_FLAG_ZERO_BIT = 0x00000000,

    /* If set, the input audio should be fed in realtime (1 second of
       audio per second) in small chunks. Calls to `aas_feed_pcm16` and
       `aas_flush` will be fast as it will delegate processing to a background
       thread. The handler will be called from the background thread at some
       point later. The accuracy may be degraded depending on the system
       hardware. You may get an accuracy estimate by calling
       `aas_realtime_get_speedup`. */
    APRIL_CONFIG_FLAG_ASYNC_RT_BIT = 0x00000001,

    /* Similar to ASYNC_RT, but does not degrade accuracy depending on system
       hardware. However, if the system is not fast enough to process audio,
       the background thread will fall behind, results may become unusable,
       and the handler will be called with APRIL_RESULT_ERROR_CANT_KEEP_UP. */
    APRIL_CONFIG_FLAG_ASYNC_NO_RT_BIT = 0x00000002,
} AprilConfigFlagBits;

typedef struct AprilConfig {
    AprilSpeakerID speaker;

    /* The handler that will be called as events occur.
       This may be called from a different thread. */
    AprilRecognitionResultHandler handler;
    void *userdata;

    /* See AprilConfigFlagBits */
    AprilConfigFlagBits flags;
} AprilConfig;

/* Creates a session with a given model. A model may have many sessions
   associated with it. */
APRIL_EXPORT AprilASRSession aas_create_session(AprilASRModel model, AprilConfig config);

/* Feed PCM16 audio data to the session, must be single-channel and sampled
   to the sample rate given in `aam_get_sample_rate`.
   Note `short_count` is the number of shorts, not bytes! */
APRIL_EXPORT void aas_feed_pcm16(AprilASRSession session, short *pcm16, size_t short_count);

/* Processes any unprocessed samples and produces a final result. */
APRIL_EXPORT void aas_flush(AprilASRSession session);

/* If APRIL_CONFIG_FLAG_ASYNC_RT_BIT is set, this may return a number describing
   how much audio is being sped up to keep up with realtime. If the number is
   below 1.0, audio is not being sped up. If greater than 1.0, the audio is
   being sped up and the accuracy may be reduced. */
APRIL_EXPORT float aas_realtime_get_speedup(AprilASRSession session);

/* Frees the session, this must be called for all sessions before freeing
   the model. Saves state to a file if AprilSpeakerID was supplied. */
APRIL_EXPORT void aas_free(AprilASRSession session);

#ifdef __cplusplus
}
#endif

#endif
