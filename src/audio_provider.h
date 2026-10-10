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

#ifndef _APRIL_AUDIO_PROVIDER
#define _APRIL_AUDIO_PROVIDER

#include "common.h"
#include <stdbool.h>
#include <stddef.h>

struct AudioProvider_i;
typedef struct AudioProvider_i *AudioProvider;

AudioProvider ap_create();

// Returns true if successful, false if buffer is full
bool ap_push_audio(AudioProvider ap, const short *audio, size_t short_count);

// Pulls at most *short_count samples (0 means any), stopping at the next
// flush mark so that audio pushed after a flush is never pulled before it.
short *ap_pull_audio(AudioProvider ap,  size_t *short_count);
void ap_pull_audio_finish(AudioProvider ap, size_t short_count);

// Marks a flush after all audio pushed so far. Repeated marks with no audio
// in between are merged. Returns false if too many flushes are pending, in
// which case the newest pending flush is moved here instead.
bool ap_mark_flush(AudioProvider ap);

// Returns true and consumes the mark if all audio before the next pending
// flush has been pulled and finished.
bool ap_take_flush(AudioProvider ap);

// Number of samples pushed but not yet finished
size_t ap_pending_samples(AudioProvider ap);
void ap_free(AudioProvider ap);

#endif
