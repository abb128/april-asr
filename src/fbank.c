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

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>
#include <stdint.h>
#include "common.h"
#include "fbank.h"
#include "fft/pocketfft.h"
#include "sonic/sonic.h"
#include "log.h"

#ifdef _MSC_VER
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#endif

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

const float kEps = 1.1920928955078125e-07f;

int round_up_to_nearest_power_of_two(int n) {
    n -= 1;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    return n + 1;
}

void generate_povey_window(float *out, int N) {
    double N_f = (double)N;
    for (int i=0; i<N; i++){
        double n = (double)i;
        out[i] = (float)pow((0.5 - 0.5 * cos(n / N_f * 6.283185307)), 0.85);
    }
}

double inverse_mel_scale(double mel_freq) {
    return 700.0 * (exp(mel_freq / 1127.0) - 1.0);
}

double mel_scale(double freq) {
    return 1127.0 * log(1.0 + freq / 700.0);
}

void generate_banks(float *bins_mat, int num_bins, int num_fft_bins, int padded_window_size, int sample_freq, int mel_low_freq, int mel_high_freq){
    if(mel_high_freq == 0) mel_high_freq = sample_freq / 2;

    float fft_bin_width = (float)sample_freq / (float)padded_window_size;

    float mel_low = (float)mel_scale((double)mel_low_freq);
    float mel_high = (float)mel_scale((double)mel_high_freq);

    float mel_freq_delta = (mel_high - mel_low) / ((float)num_bins + 1.0f);

    for(int i=0; i<num_bins; i++){
        float left_mel = mel_low + (float)i * mel_freq_delta;
        float center_mel = left_mel + mel_freq_delta;
        float right_mel = center_mel + mel_freq_delta;

        for(int j=0; j<num_fft_bins; j++){
            float freq = fft_bin_width * (float)j;
            float mel = (float)mel_scale((double)freq);

            float weight = 0.0f;
            if((mel > left_mel) && (mel < right_mel)) {
                if(mel <= center_mel){
                    weight = (mel - left_mel) / (center_mel - left_mel);
                } else {
                    weight = (right_mel - mel) / (right_mel - center_mel);
                }
            }
            bins_mat[i * num_fft_bins + j] = weight;
        }
    }
}


struct OnlineFBank_i {
    FBankOptions opts;

    int window_shift;
    int window_size;
    int padded_window_size;
    int num_fft_bins;

    float *window;
    float *mel_bins;
    int *mel_first, *mel_last;
    float *wave_buffer;
    size_t wave_capacity, wave_size;
    int64_t wave_start, samples_received, next_frame;
    bool wave_finalized;
    bool wave_blocked;

    float *temp_segments;
    size_t temp_segments_y;
    size_t temp_segments_count;
    size_t temp_segment_head; // y, write
    size_t temp_segment_tail; // y, read
    size_t temp_segment_avail;
    ssize_t temp_segment_avail_f;


    float *prev_leftover;
    size_t prev_leftover_count;

    rfft_plan plan;
    double *data;
    double *ret;

    double speed_factor;
    sonicStream sonic_stream;
    bool sonic_active;
    bool sonic_flushed;
};

OnlineFBank make_fbank(FBankOptions opts) {
    int64_t window = (int64_t)opts.frame_length_ms * opts.sample_freq / 1000;
    int64_t shift = (int64_t)opts.frame_shift_ms * opts.sample_freq / 1000;
    if ((!opts.snip_edges && !opts.corrected_window) || window < 2 || window > 32768 ||
        shift < 1 || shift > window || opts.num_bins < 1 || opts.num_bins > 255 ||
        opts.pull_segment_count < 1 || opts.pull_segment_count > 99 ||
        opts.pull_segment_step < 1 || opts.pull_segment_step > opts.pull_segment_count ||
        (!opts.round_pow2 && window % 2)) return NULL;

    OnlineFBank fbank = (OnlineFBank)calloc(1, sizeof(struct OnlineFBank_i));
    if (!fbank) return NULL;
    fbank->opts = opts;

    fbank->window_shift = opts.frame_shift_ms * opts.sample_freq / 1000;
    fbank->window_size = opts.frame_length_ms * opts.sample_freq / 1000;
    fbank->padded_window_size = opts.round_pow2 ? round_up_to_nearest_power_of_two(fbank->window_size) : fbank->window_size;
    fbank->num_fft_bins = fbank->padded_window_size / 2;

    fbank->window = (float*)calloc(fbank->padded_window_size, sizeof(float));
    if (!fbank->window) goto fail;
    if (opts.corrected_window) {
        for (int i = 0; i < fbank->window_size; ++i)
            fbank->window[i] = (float)pow(0.5 - 0.5 * cos(2.0 * 3.141592653589793 * i / (fbank->window_size - 1)), 0.85);
    } else {
        generate_povey_window(fbank->window, fbank->padded_window_size);
    }

    fbank->mel_bins = (float*)calloc(fbank->num_fft_bins * opts.num_bins, sizeof(float));
    if (!fbank->mel_bins) goto fail;
    generate_banks(fbank->mel_bins, opts.num_bins, fbank->num_fft_bins,
        fbank->padded_window_size, opts.sample_freq, opts.mel_low, opts.mel_high);

    /* Each triangular mel filter touches only a small fraction of FFT bins.
       Keep the original summation order while skipping its zero weights. */
    fbank->mel_first = calloc(opts.num_bins, sizeof(int));
    fbank->mel_last = calloc(opts.num_bins, sizeof(int));
    if (!fbank->mel_first || !fbank->mel_last) goto fail;
    for (int mel = 0; mel < opts.num_bins; ++mel) {
        int first = 0, last = fbank->num_fft_bins;
        while (first < last && fbank->mel_bins[mel * fbank->num_fft_bins + first] == 0.0f) ++first;
        while (last > first && fbank->mel_bins[mel * fbank->num_fft_bins + last - 1] == 0.0f) --last;
        fbank->mel_first[mel] = first;
        fbank->mel_last[mel] = last;
    }
    fbank->temp_segments_y = opts.pull_segment_count * 32;
    fbank->temp_segments_count = fbank->temp_segments_y * opts.num_bins;
    fbank->temp_segments = (float*)calloc(fbank->temp_segments_count, sizeof(float));

    fbank->temp_segment_tail = 0;
    fbank->temp_segment_head = 0;
    fbank->temp_segment_avail = 0;

    fbank->prev_leftover = (float*)calloc(fbank->padded_window_size * 2, sizeof(float));
    fbank->prev_leftover_count = 0;

    fbank->plan = make_rfft_plan(fbank->padded_window_size);
    fbank->data = (double*)calloc(fbank->padded_window_size, sizeof(double));
    fbank->ret  = (double*)calloc(fbank->padded_window_size + 1, sizeof(double));
    if (!fbank->temp_segments || !fbank->prev_leftover || !fbank->plan || !fbank->data || !fbank->ret)
        goto fail;

    fbank->speed_factor = 1.0;

    /* Allocate Sonic lazily: normal-speed audio needs no pitch processing. */

    return fbank;
fail:
    free_fbank(fbank);
    return NULL;
}

static void compute_frame(OnlineFBank fbank) {
    int frame_size = fbank->opts.corrected_window ? fbank->window_size : fbank->padded_window_size;
    float preemph_coeff = fbank->opts.preemph_coeff;
    // Not included: dither

    // Apply remove dc offset
    if(fbank->opts.remove_dc_offset) {
        float sum = 0;
        for(int j=0; j<frame_size; j++) sum += fbank->data[j];
        float mean = sum / frame_size;
        for(int j=0; j<frame_size; j++) fbank->data[j] -= mean;
    }

    // Apply preemphasize
    if(preemph_coeff > 0.0f) {
        for(int j=frame_size-1; j>0; --j)
            fbank->data[j] -= preemph_coeff * fbank->data[j - 1];
        fbank->data[0] -= preemph_coeff * fbank->data[0];
    }

    // Apply window function
    for(int j=0; j<frame_size; j++)
        fbank->data[j] *= fbank->window[j];

    double *dptr = fbank->data;
    double *rptr = fbank->ret;
    memcpy((char *)(rptr+1), dptr, fbank->padded_window_size * sizeof(double));

    int res = rfft_forward(fbank->plan, rptr+1, 1.0);
    if(res != 0){
        LOG_ERROR("fbank rfft failure %d", res);
        return;
    }

    rptr[0] = fbank->ret[1];
    rptr[1] = 0.0;

    float *out = &fbank->temp_segments[fbank->temp_segment_head * fbank->opts.num_bins];

    // Convert to magnitude
    for(int fft=0; fft<fbank->num_fft_bins; fft++){
        float real = (float)(rptr[fft * 2]);
        float imaginary = (float)(rptr[fft * 2 + 1]);

        fbank->data[fft] = real * real + imaginary * imaginary;
    }

    // Convert to mel energies
    for(int mel=0; mel<fbank->opts.num_bins; mel++){
        float val = 0.0f;
        for(int fft=fbank->mel_first[mel]; fft<fbank->mel_last[mel]; fft++){
            float magnitude = (float)(fbank->data[fft]);

            val += magnitude * fbank->mel_bins[mel * fbank->num_fft_bins + fft];
        }
        out[mel] = val;
    }

    // Log mel energies
    for(int mel=0; mel<fbank->opts.num_bins; mel++){
        out[mel] = (float)log((double)MAX(kEps, out[mel]));
    }

    fbank->temp_segment_head++;
    fbank->temp_segment_avail++;
    fbank->temp_segment_avail_f = fbank->temp_segment_avail;

    fbank->temp_segment_head = fbank->temp_segment_head % fbank->temp_segments_y;
}

static int64_t frame_start(OnlineFBank fbank) {
    int64_t start = fbank->next_frame * fbank->window_shift;
    if (!fbank->opts.snip_edges) start += fbank->window_shift / 2 - fbank->window_size / 2;
    return start;
}

static bool compute_corrected_frames(OnlineFBank fbank, bool final) {
    fbank->wave_blocked = false;
    int64_t frame_limit = fbank->opts.snip_edges
        ? (fbank->samples_received < fbank->window_size ? 0 : 1 + (fbank->samples_received - fbank->window_size) / fbank->window_shift)
        : (fbank->samples_received + fbank->window_shift / 2) / fbank->window_shift;
    while (fbank->samples_received > 0) {
        int64_t start = frame_start(fbank);
        if (final ? fbank->next_frame >= frame_limit : start + fbank->window_size > fbank->samples_received) break;
        if (fbank->temp_segment_avail == fbank->temp_segments_y) {
            fbank->wave_blocked = true;
            break;
        }
        memset(fbank->data, 0, fbank->padded_window_size * sizeof(double));
        for (int j = 0; j < fbank->window_size; ++j) {
            int64_t index = start + j;
            /* Kaldi-style reflection, including utterances shorter than a frame. */
            while (index < 0 || index >= fbank->samples_received) {
                if (index < 0) index = -index - 1;
                else index = 2 * fbank->samples_received - 1 - index;
            }
            assert(index >= fbank->wave_start && index < fbank->wave_start + (int64_t)fbank->wave_size);
            fbank->data[j] = fbank->wave_buffer[index - fbank->wave_start];
        }
        compute_frame(fbank);
        ++fbank->next_frame;
    }
    /* Keep a full trailing frame for reflection at EOF; memory stays bounded. */
    int64_t keep_from = frame_start(fbank);
    int64_t tail_start = fbank->samples_received - fbank->window_size;
    if (keep_from > tail_start) keep_from = tail_start;
    if (keep_from < 0) keep_from = 0;
    if (keep_from > fbank->wave_start) {
        size_t drop = (size_t)(keep_from - fbank->wave_start);
        memmove(fbank->wave_buffer, fbank->wave_buffer + drop, (fbank->wave_size - drop) * sizeof(float));
        fbank->wave_size -= drop;
        fbank->wave_start = keep_from;
    }
    return fbank->next_frame >= frame_limit;
}

static void accept_corrected_waveform(OnlineFBank fbank, const float *wave, size_t count) {
    size_t needed = fbank->wave_size + count;
    if (needed > fbank->wave_capacity) {
        size_t capacity = needed * 2;
        float *buffer = realloc(fbank->wave_buffer, capacity * sizeof(float));
        if (!buffer) { LOG_ERROR("Could not allocate audio features"); abort(); }
        fbank->wave_buffer = buffer;
        fbank->wave_capacity = capacity;
    }
    memcpy(fbank->wave_buffer + fbank->wave_size, wave, count * sizeof(float));
    fbank->wave_size += count;
    fbank->samples_received += (int64_t)count;
    fbank->wave_finalized = false;
    compute_corrected_frames(fbank, false);
}

const float ZEROS[32768] = { 0 };
static void accept_waveform(OnlineFBank fbank, const float *wave, size_t wave_count) {
    const float *samples = wave ? wave : ZEROS;

    if (fbank->opts.corrected_window) {
        if (wave_count) accept_corrected_waveform(fbank, samples, wave_count);
        return;
    }
    for(ssize_t i=0;; i++) {
        if((fbank->temp_segment_avail + 1) > fbank->temp_segments_y){
            LOG_WARNING("fbank ran out of space. Please call fbank_pull_segments. Can't eat wave");
            return;
        }

        ssize_t start_idx = i * fbank->window_shift - fbank->prev_leftover_count;
        ssize_t end_idx = start_idx + fbank->padded_window_size;

        if (end_idx < 0 || (size_t)end_idx > wave_count) {
            if(start_idx >= 0){
                assert((wave_count - start_idx) < ((size_t)fbank->padded_window_size * 2));
                memcpy(fbank->prev_leftover, &samples[start_idx], (wave_count - start_idx) * sizeof(float));
            }else{
                // This branch may be hit when wave_count < fbank->padded_window_size
                // We need to copy to prev_leftover not only data in wave, but also from
                // prev_leftover itself.

                size_t num_to_move_from_prev = -start_idx;

                assert((wave_count + num_to_move_from_prev) <= ((size_t)fbank->padded_window_size * 2));
                assert((fbank->prev_leftover_count + start_idx + num_to_move_from_prev) <= ((size_t)fbank->padded_window_size * 2));

                memmove(
                    fbank->prev_leftover,
                    &fbank->prev_leftover[fbank->prev_leftover_count + start_idx],
                    num_to_move_from_prev * sizeof(float)
                );

                memcpy(
                    &fbank->prev_leftover[num_to_move_from_prev],
                    samples,
                    wave_count * sizeof(float)
                );
            }
            fbank->prev_leftover_count = wave_count - start_idx;
            return;
        }

        for(int j=0; j<fbank->padded_window_size; j++){
            ssize_t wave_idx = start_idx + j;
            if(wave_idx < 0){
                ssize_t ll_idx = fbank->prev_leftover_count + wave_idx;
                fbank->data[j] = fbank->prev_leftover[ll_idx];
            } else {
                fbank->data[j] = samples[start_idx + j];
            }
        }

        compute_frame(fbank);
    }

    fbank->prev_leftover_count = 0;
}

static void drain_sonic(OnlineFBank fbank, bool final) {
    if (!fbank->sonic_active) return;
    if (final && !fbank->sonic_flushed) {
        if (!sonicFlushStream(fbank->sonic_stream)) {
            LOG_ERROR("Could not flush Sonic audio");
            abort();
        }
        fbank->sonic_flushed = true;
    }
    /* At most one feature frame per read; the caller can pull features before
       continuing a large drain. Never overwrite the caller's input samples. */
    float wave[1024];
    int count = MIN(fbank->window_shift, 1024);
    while (fbank->temp_segment_avail < fbank->temp_segments_y) {
        int n = sonicReadFloatFromStream(fbank->sonic_stream, wave, count);
        if (!n) break;
        accept_waveform(fbank, wave, (size_t)n);
    }
}

void fbank_accept_waveform(OnlineFBank fbank, float *wave, size_t wave_count) {
    if (wave && fbank->opts.use_sonic && (fbank->speed_factor > 1.0 || fbank->sonic_active)) {
        if (!fbank->sonic_stream)
            fbank->sonic_stream = sonicCreateStream(fbank->opts.sample_freq, 1);
        if (!fbank->sonic_stream) { LOG_ERROR("Could not allocate Sonic audio"); abort(); }
        fbank->sonic_active = true;
        fbank->sonic_flushed = false;
        sonicSetSpeed(fbank->sonic_stream, (float)fbank->speed_factor);
        if (!sonicWriteFloatToStream(fbank->sonic_stream, wave, (int)wave_count)) {
            LOG_ERROR("Could not buffer Sonic audio");
            abort();
        }
        drain_sonic(fbank, false);
    } else {
        accept_waveform(fbank, wave, wave_count);
    }
}

bool fbank_flush(OnlineFBank fbank) {
    drain_sonic(fbank, true);
    if (fbank->sonic_active && sonicSamplesAvailable(fbank->sonic_stream))
        return true; /* Feature ring is full: pull before finishing reflection. */
    if (fbank->opts.corrected_window && !fbank->wave_finalized) {
        fbank->wave_finalized = compute_corrected_frames(fbank, true);
    }
    ssize_t min = -(fbank->opts.pull_segment_count * 3);
    if(fbank->temp_segment_avail_f < min) return false;

    const size_t pull_segment_count = (size_t)fbank->opts.pull_segment_count;
    while (fbank->temp_segment_avail < pull_segment_count) {
        float *out = &fbank->temp_segments[fbank->temp_segment_head * fbank->opts.num_bins];
        for(int mel=0; mel<fbank->opts.num_bins; mel++){
            out[mel] = (float)log((double)kEps);
        }

        fbank->temp_segment_head++;
        fbank->temp_segment_avail++;

        fbank->temp_segment_head = fbank->temp_segment_head % fbank->temp_segments_y;
    }

    return true;
}

bool fbank_finish(OnlineFBank fbank) {
    drain_sonic(fbank, true);
    if (fbank->sonic_active && sonicSamplesAvailable(fbank->sonic_stream))
        return true;
    if (fbank->opts.corrected_window && !fbank->wave_finalized) {
        fbank->wave_finalized = compute_corrected_frames(fbank, true);
    }
    if (fbank->temp_segment_avail_f <= 0) return false;
    return fbank_flush(fbank);
}

void fbank_reset(OnlineFBank fbank) {
    fbank->wave_size = 0;
    fbank->wave_start = fbank->samples_received = fbank->next_frame = 0;
    fbank->wave_finalized = false;
    fbank->wave_blocked = false;
    fbank->temp_segment_head = fbank->temp_segment_tail = 0;
    fbank->temp_segment_avail = 0;
    fbank->temp_segment_avail_f = 0;
    fbank->prev_leftover_count = 0;
    fbank->speed_factor = 1.0;
    if (fbank->sonic_stream) sonicDestroyStream(fbank->sonic_stream);
    fbank->sonic_stream = NULL;
    fbank->sonic_active = fbank->sonic_flushed = false;
}

bool fbank_pull_segments(OnlineFBank fbank, float *output, size_t output_count) {
    const size_t pull_segment_count = (size_t)fbank->opts.pull_segment_count;
    const size_t expected_output_size =
        pull_segment_count *
        (size_t)fbank->opts.num_bins *
        sizeof(float);

    if (output_count != expected_output_size) {
        LOG_ERROR("Invalid fbank output buffer size");
        return false;
    }

    if (fbank->temp_segment_avail < pull_segment_count) {
        if (fbank->wave_blocked) compute_corrected_frames(fbank, false);
        drain_sonic(fbank, false);
    }
    if (fbank->temp_segment_avail < pull_segment_count) {
        return false;
    }

    for(int i=0; i<fbank->opts.pull_segment_count; i++){
        int curr_idx = (fbank->temp_segment_tail + i) % fbank->temp_segments_y;
        memcpy(
            &output[i * fbank->opts.num_bins],
            &fbank->temp_segments[curr_idx * fbank->opts.num_bins],
            fbank->opts.num_bins * sizeof(float)
        );
    }

    fbank->temp_segment_tail += fbank->opts.pull_segment_step;
    fbank->temp_segment_tail = fbank->temp_segment_tail % fbank->temp_segments_y;
    fbank->temp_segment_avail -= fbank->opts.pull_segment_step;
    fbank->temp_segment_avail_f -= fbank->opts.pull_segment_step;

    return true;
}

void fbank_set_speed(OnlineFBank fbank, double factor) {
    fbank->speed_factor = isfinite(factor) && factor >= 1.0 ? factor : 1.0;
}

double fbank_get_speed(OnlineFBank fbank) {
    return fbank->speed_factor;
}

size_t fbank_get_segments_stride_ms(OnlineFBank fbank) {
    return fbank->opts.pull_segment_step * fbank->opts.frame_shift_ms;
}

void free_fbank(OnlineFBank fbank) {
    if (!fbank) return;
    if(fbank->sonic_stream) sonicDestroyStream(fbank->sonic_stream);
    free(fbank->wave_buffer);
    free(fbank->mel_first);
    free(fbank->mel_last);

    free(fbank->ret);
    free(fbank->data);
    if (fbank->plan) destroy_rfft_plan(fbank->plan);

    free(fbank->prev_leftover);
    free(fbank->temp_segments);
    free(fbank->mel_bins);
    free(fbank->window);
    free(fbank);
}
