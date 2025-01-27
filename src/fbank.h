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

#ifndef _APRIL_FBANK
#define _APRIL_FBANK

#include <stdbool.h>
#include "common.h"

struct OnlineFBank_i;
typedef struct OnlineFBank_i * OnlineFBank;

typedef struct FBankOptions {
    // Frequency in Hz, e.g. 16000
    int sample_freq;

    // Stride in milliseconds, for example 10
    int frame_shift_ms;

    // Window length in milliseconds, for example 25
    int frame_length_ms;

    // Number of mel bins, for example 80
    int num_bins;

    // Whether or not to round window size to pow2
    bool round_pow2;

    // Mel low frequency, for example 20 Hz
    int mel_low;

    // Mel high frequency. If 0, sample_freq/2 is assumed
    int mel_high;

    // ???????????
    bool snip_edges;

    // How many segments to pull in fbank_pull_segments.
    // For example, if this is equal to 9, then you should call
    // fbank_pull_segments with a tensor of size (1, 9, num_bins)
    int pull_segment_count;

    // How many segments to step over in fbank_pull_segments.
    // For example, if this is set to 4, then each call to fbank_pull_segments
    // will step over 4 segments
    int pull_segment_step;

    // If false, speed feature will be unavailable
    bool use_sonic;

    bool remove_dc_offset; // true
    float preemph_coeff; // 0.97
} FBankOptions;

OnlineFBank make_fbank(FBankOptions opts);
void fbank_accept_waveform(OnlineFBank fbank, float *wave, size_t wave_count);
bool fbank_pull_segments(OnlineFBank fbank, float *output, size_t output_count);
bool fbank_flush(OnlineFBank fbank); // Returns false if no more left to flush

void fbank_set_speed(OnlineFBank fbank, double factor);
double fbank_get_speed(OnlineFBank fbank);

// Returns how many milliseconds of audio was consumed
// in the last `fbank_pull_segments` call
size_t fbank_get_segments_stride_ms(OnlineFBank fbank);
void free_fbank(OnlineFBank fbank);

#endif