/* Features must be independent of capture packet sizes and finite at edges. */
#include <assert.h>
#include <math.h>
#include <string.h>
#include "fbank.h"
#include "sonic/sonic.h"
#define LENGTH 4800
static FBankOptions options(bool corrected, bool snip) {
    FBankOptions o = {0};
    o.sample_freq = 16000; o.frame_shift_ms = 10; o.frame_length_ms = 25;
    o.num_bins = 80; o.round_pow2 = true; o.mel_low = 20;
    o.snip_edges = snip; o.corrected_window = corrected;
    o.pull_segment_count = 1; o.pull_segment_step = 1;
    o.remove_dc_offset = true; o.preemph_coeff = 0.97f;
    return o;
}
static size_t run(float *wave, int chunk, bool corrected, bool snip, float *out) {
    OnlineFBank f = make_fbank(options(corrected, snip));
    size_t frames = 0;
    for (int head = 0; head < LENGTH; head += chunk) {
        int count = LENGTH - head < chunk ? LENGTH - head : chunk;
        fbank_accept_waveform(f, wave + head, count);
        while (fbank_pull_segments(f, out + frames * 80, 80 * sizeof(float))) ++frames;
    }
    free_fbank(f);
    return frames;
}

static void pull(OnlineFBank f, float *out, size_t *count) {
    while (fbank_pull_segments(f, out + *count * 80, 80 * sizeof(float))) {
        ++*count;
        assert(*count < 100);
    }
}

static void test_sonic(float *wave, int chunk, bool corrected) {
    /* Compare streamed features with all output from a separately flushed
       Sonic stream. A short final tail must be present in both. */
    FBankOptions o = options(corrected, true);
    o.use_sonic = true;
    OnlineFBank streamed = make_fbank(o);
    assert(streamed);
    for (int repeat = 0; repeat < 2; ++repeat) {
        sonicStream reference = sonicCreateStream(16000, 1);
        sonicSetSpeed(reference, 1.5f);
        fbank_set_speed(streamed, 1.5);
        float actual[80 * 100], expected[80 * 100], transformed[LENGTH * 2];
        size_t actual_count = 0, expected_count = 0;
        float original[LENGTH];
        memcpy(original, wave, sizeof(original));
        for (int head = 0; head < LENGTH; head += chunk) {
            int n = LENGTH - head < chunk ? LENGTH - head : chunk;
            assert(sonicWriteFloatToStream(reference, wave + head, n));
            fbank_accept_waveform(streamed, wave + head, n);
            pull(streamed, actual, &actual_count);
        }
        assert(!memcmp(original, wave, sizeof(original)));
        assert(sonicFlushStream(reference));
        int n = sonicReadFloatFromStream(reference, transformed, LENGTH * 2);
        assert(n > 0 && sonicSamplesAvailable(reference) == 0);
        sonicDestroyStream(reference);
        o.use_sonic = false;
        OnlineFBank plain = make_fbank(o);
        for (int head = 0; head < n; head += 160) {
            int count = n - head < 160 ? n - head : 160;
            fbank_accept_waveform(plain, transformed + head, count);
            pull(plain, expected, &expected_count);
        }
        while (fbank_finish(plain)) pull(plain, expected, &expected_count);
        while (fbank_finish(streamed)) pull(streamed, actual, &actual_count);
        assert(actual_count == expected_count);
        assert(!memcmp(actual, expected, actual_count * 80 * sizeof(float)));
        free_fbank(plain);
        fbank_reset(streamed);
    }
    free_fbank(streamed);
}

static void test_large_drain(void) {
    /* A single packet exceeds the 32-frame ring; pulling/finishing must resume
       the buffered waveform, including the reflected final frame. */
    float wave[LENGTH * 3], a[80 * 100], b[80 * 100];
    for (int i = 0; i < LENGTH * 3; ++i) wave[i] = 0.3f * sinf(i * 0.13f);
    for (int sonic = 0; sonic < 2; ++sonic) {
        FBankOptions o = options(true, false); o.use_sonic = sonic != 0;
        size_t counts[2] = {0};
        for (int mode = 0; mode < 2; ++mode) {
            OnlineFBank f = make_fbank(o);
            fbank_set_speed(f, 1.5);
            int chunk = mode ? LENGTH * 3 : 160;
            float *out = mode ? b : a;
            for (int head = 0; head < LENGTH * 3; head += chunk) {
                fbank_accept_waveform(f, wave + head, chunk);
                pull(f, out, &counts[mode]);
            }
            while (fbank_finish(f)) pull(f, out, &counts[mode]);
            free_fbank(f);
        }
        assert(counts[0] == counts[1]);
        assert(!memcmp(a, b, counts[0] * 80 * sizeof(float)));
    }
}
int main(void) {
    float wave[LENGTH], a[80 * 40], b[80 * 40];
    for (int i = 0; i < LENGTH; ++i) wave[i] = 0.3f * sinf(i * 0.19f) + 0.1f * cosf(i * 0.031f);
    for (int mode = 0; mode < 3; ++mode) {
        bool corrected = mode != 0, snip = mode != 2;
        size_t count = run(wave, 640, corrected, snip, a);
        for (int chunk = 1; chunk <= 1024; chunk *= 2) {
            size_t other = run(wave, chunk, corrected, snip, b);
            assert(other == count);
            for (size_t i = 0; i < count * 80; ++i) {
                assert(isfinite(a[i]) && isfinite(b[i]));
                assert(fabsf(a[i] - b[i]) < 0.00001f);
            }
        }
        if (corrected && snip) assert(count == 28);
        if (corrected && !snip) assert(count == 29);
    }
    /* A short utterance exercises repeated left/right reflection at EOF. */
    OnlineFBank f = make_fbank(options(true, false));
    fbank_accept_waveform(f, wave, 100);
    assert(fbank_flush(f));
    assert(fbank_pull_segments(f, a, 80 * sizeof(float)));
    for (int i = 0; i < 80; ++i) assert(isfinite(a[i]));
    free_fbank(f);
    for (int mode = 0; mode < 2; ++mode) {
        test_sonic(wave, 37, mode != 0);
        test_sonic(wave, 320, mode != 0);
        test_sonic(wave, 1024, mode != 0);
    }
    FBankOptions bad = options(true, true);
    bad.frame_shift_ms = 0;
    assert(!make_fbank(bad));
    test_large_drain();
    return 0;
}
