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

struct AudioProvider_i;
typedef struct AudioProvider_i *AudioProvider;

AudioProvider ap_create();
void ap_push_audio(AudioProvider ap, const short *audio, size_t short_count);
short *ap_pull_audio(AudioProvider ap,  size_t *short_count);
void ap_pull_audio_finish(AudioProvider ap, size_t short_count);
void ap_free(AudioProvider ap);

#endif