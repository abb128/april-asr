// Offline and streaming April ASR runner; no sound server dependency.
#include "april_api.h"
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif
using Clock = std::chrono::steady_clock;
static double ms(Clock::duration d) { return std::chrono::duration<double, std::milli>(d).count(); }
static void env(const char *key, const std::string &value) {
#ifdef _WIN32
    _putenv_s(key, value.c_str());
#else
    setenv(key, value.c_str(), 1);
#endif
}
static std::string json_quote(const std::string &s) {
    std::string out = "\"";
    for (unsigned char c : s) {
        if (c == '\\' || c == '"') { out += '\\'; out += c; }
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else if (c < 32) { char hex[7]; std::snprintf(hex, sizeof(hex), "\\u%04x", c); out += hex; }
        else out += c;
    }
    return out + '"';
}
struct Results {
    bool json = false, benchmark = false, error = false;
    Clock::time_point start;
    double first_ms = -1;
    size_t first_audio_ms = 0;
    std::string transcript;
};
static void handler(void *arg, AprilResultType result, size_t count, const AprilToken *tokens) {
    auto &state = *static_cast<Results *>(arg);
    std::string text;
    for (size_t i = 0; i < count; ++i) text += tokens[i].token;
    if (count && state.first_ms < 0) {
        state.first_ms = ms(Clock::now() - state.start);
        state.first_audio_ms = tokens[0].time_ms;
    }
    const char *kind = "unknown";
    switch (result) {
    case APRIL_RESULT_RECOGNITION_FINAL: kind = "final"; state.transcript += text; break;
    case APRIL_RESULT_RECOGNITION_PARTIAL: kind = "partial"; break;
    case APRIL_RESULT_SILENCE: kind = "silence"; break;
    case APRIL_RESULT_ERROR_CANT_KEEP_UP: kind = "overflow"; state.error = true; break;
    default: return;
    }
    if (state.json) {
        std::printf("{\"type\":\"%s\",\"text\":%s}\n", kind, json_quote(text).c_str());
    } else if (!state.benchmark && count) {
        std::printf("%c %s\n", result == APRIL_RESULT_RECOGNITION_FINAL ? '@' : '-', text.c_str());
    } else if (state.error) std::fprintf(stderr, "Audio queue full: some input was rejected.\n");
    std::fflush(stdout);
}
static unsigned u16(const unsigned char *p) { return p[0] | (unsigned(p[1]) << 8); }
static uint32_t u32(const unsigned char *p) { return u16(p) | (uint32_t(u16(p + 2)) << 16); }
static bool read_wav(FILE *file, size_t rate, size_t &bytes) {
    unsigned char header[12];
    if (std::fread(header, 1, 12, file) != 12 || std::memcmp(header, "RIFF", 4) || std::memcmp(header + 8, "WAVE", 4)) return false;
    bool valid_fmt = false;
    unsigned char chunk[8];
    while (std::fread(chunk, 1, 8, file) == 8) {
        uint32_t length = u32(chunk + 4);
        if (!std::memcmp(chunk, "fmt ", 4)) {
            unsigned char fmt[16];
            if (length < 16 || std::fread(fmt, 1, 16, file) != 16) return false;
            valid_fmt = u16(fmt) == 1 && u16(fmt + 2) == 1 && u32(fmt + 4) == rate && u16(fmt + 12) == 2 && u16(fmt + 14) == 16;
            if (std::fseek(file, long(length - 16 + (length & 1)), SEEK_CUR)) return false;
        } else if (!std::memcmp(chunk, "data", 4)) {
            if (!valid_fmt || (length & 1)) return false;
            bytes = length;
            return true;
        } else if (std::fseek(file, long(length + (length & 1)), SEEK_CUR)) return false;
    }
    return false;
}
static void usage(const char *exe) {
    std::fprintf(stderr,
      "Usage: %s AUDIO.wav|AUDIO.pcm|- MODEL.april [options]\n"
      "  --profile balanced|strict|legacy  (default balanced)\n"
      "  --threads N        Encoder CPU threads (default 1; tune 1/2/4/8)\n"
      "  --chunk-ms N       Input packet size (default 20, range 5..200)\n"
      "  --realtime         Pace a recording like live speech\n"
      "  --async            Background worker (files are paced automatically)\n"
      "  --json             Newline-delimited JSON events and benchmark\n"
      "  --benchmark        Print transcript and processing measurements\n"
      "Raw PCM/stdin: little-endian, signed 16-bit, mono, model sample rate.\n", exe);
}
int main(int argc, char **argv) {
    env("ORT_DISABLE_TELEMETRY", "1");
    if (argc < 3 || !std::strcmp(argv[1], "--help")) { usage(argv[0]); return argc == 2 ? 0 : 1; }
    const char *audio_path = argv[1], *model_path = argv[2];
    std::string profile = "balanced";
    int threads = 1, chunk_ms = 20;
    bool realtime = false, async = false;
    Results state;
    for (int i = 3; i < argc; ++i) {
        std::string option = argv[i];
        if (option == "--realtime") realtime = true;
        else if (option == "--async") async = true;
        else if (option == "--json") state.json = true;
        else if (option == "--benchmark") state.benchmark = true;
        else if ((option == "--profile" || option == "--threads" || option == "--chunk-ms") && i + 1 < argc) {
            const char *value = argv[++i];
            if (option == "--profile") profile = value;
            else {
                char *end; errno = 0;
                long n = std::strtol(value, &end, 10);
                int low = option == "--threads" ? 1 : 5, high = option == "--threads" ? 64 : 200;
                if (errno || *end || n < low || n > high) { usage(argv[0]); return 1; }
                if (option == "--threads") threads = int(n); else chunk_ms = int(n);
            }
        } else { usage(argv[0]); return 1; }
    }
    if (profile != "balanced" && profile != "strict" && profile != "legacy") { usage(argv[0]); return 1; }
    env("APRIL_ENCODER_THREADS", std::to_string(threads));
    env("APRIL_CORRECT_FBANK", profile == "legacy" ? "0" : "1");
    env("APRIL_EARLY_EMIT", profile == "strict" ? "0" : "1");
    env("APRIL_PUNCTUATION_BIAS", profile == "strict" ? "0" : "3.5");
    env("APRIL_SPECULATIVE", profile == "strict" ? "0" : "1");
    env("APRIL_MAX_SYMBOLS", profile == "strict" ? "6" : "3");
    env("APRIL_SILENCE_MS", profile == "legacy" ? "2200" : "1200");
    aam_api_init(APRIL_VERSION);
    AprilASRModel model = aam_create_model(model_path);
    if (!model) { std::fprintf(stderr, "Could not load model: %s\n", model_path); return 2; }
    size_t rate = aam_get_sample_rate(model);
    bool stdin_mode = !std::strcmp(audio_path, "-");
    FILE *input = stdin_mode ? stdin : std::fopen(audio_path, "rb");
    if (!input) { std::perror(audio_path); aam_free(model); return 2; }
#ifdef _WIN32
    if (stdin_mode) _setmode(_fileno(stdin), _O_BINARY);
#endif
    size_t remaining = SIZE_MAX;
    if (!stdin_mode) {
        unsigned char magic[4];
        size_t read = std::fread(magic, 1, 4, input);
        std::rewind(input);
        if (read == 4 && !std::memcmp(magic, "RIFF", 4) && !read_wav(input, rate, remaining)) {
            std::fprintf(stderr, "WAV must be mono PCM16 at %zu Hz, with a valid RIFF header.\n", rate);
            std::fclose(input); aam_free(model); return 2;
        }
    }
    AprilConfig config = {};
    config.handler = handler; config.userdata = &state;
    config.flags = async ? APRIL_CONFIG_FLAG_ASYNC_NO_RT_BIT : APRIL_CONFIG_FLAG_ZERO_BIT;
    AprilASRSession session = aas_create_session(model, config);
    if (!session) { if (!stdin_mode) std::fclose(input); aam_free(model); return 2; }
    // Unbuffered reads do not wait to fill an entire stdio buffer on a live pipe.
    if (stdin_mode) std::setvbuf(input, nullptr, _IONBF, 0);
    size_t packet_bytes = std::max(size_t(2), rate * size_t(chunk_ms) / 1000 * 2);
    std::vector<unsigned char> packet(packet_bytes + 1);
    std::vector<short> pcm(packet_bytes / 2 + 1);
    std::vector<double> feed_times;
    state.start = Clock::now();
    size_t samples = 0, carry = 0;
    bool failed = false;
    while (remaining) {
        size_t wanted = std::min(packet_bytes - carry, remaining);
        size_t got = std::fread(packet.data() + carry, 1, wanted, input);
        if (!got) {
            if (std::ferror(input) && errno == EINTR) { std::clearerr(input); continue; }
            failed = std::ferror(input) || (remaining != SIZE_MAX && remaining != 0);
            break;
        }
        if (remaining != SIZE_MAX) remaining -= got;
        size_t bytes = carry + got, count = bytes / 2;
        for (size_t j = 0; j < count; ++j) pcm[j] = short(u16(packet.data() + 2 * j));
        if (count) {
            auto begin = Clock::now();
            aas_feed_pcm16(session, pcm.data(), count);
            feed_times.push_back(ms(Clock::now() - begin));
            samples += count;
            if (!stdin_mode && (realtime || async))
                std::this_thread::sleep_until(state.start + std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(double(samples) / rate)));
        }
        carry = bytes & 1;
        if (carry) packet[0] = packet[bytes - 1];
    }
    if (carry) { std::fprintf(stderr, "Incomplete PCM16 sample at EOF.\n"); failed = true; }
    auto flush_start = Clock::now();
    aas_flush(session);
    aas_wait(session);
    double flush_ms = ms(Clock::now() - flush_start);
    double elapsed = ms(Clock::now() - state.start);
    std::sort(feed_times.begin(), feed_times.end());
    auto percentile = [&](double q) { return feed_times.empty() ? 0.0 : feed_times[size_t(q * (feed_times.size() - 1))]; };
    if (state.benchmark || state.json) {
        double duration = double(samples) / rate;
        std::printf("{\"type\":\"benchmark\",\"profile\":%s,\"threads\":%d,\"audio_seconds\":%.6f,\"wall_seconds\":%.6f,\"rtf\":%.6f,\"feed_p50_ms\":%.6f,\"feed_p95_ms\":%.6f,\"feed_max_ms\":%.6f,\"flush_ms\":%.6f,\"first_text_wall_ms\":%.6f,\"first_text_audio_ms\":%zu,\"paced\":%s,\"async\":%s,\"transcript\":%s}\n",
            json_quote(profile).c_str(), threads, duration, elapsed / 1000.0, duration ? elapsed / 1000.0 / duration : 0.0,
            percentile(0.5), percentile(0.95), percentile(1), flush_ms, state.first_ms, state.first_audio_ms,
            realtime || async ? "true" : "false", async ? "true" : "false", json_quote(state.transcript).c_str());
    }
    bool overflow = state.error;
    aas_free(session); aam_free(model);
    if (!stdin_mode) std::fclose(input);
    if (failed) std::fprintf(stderr, "Audio input was truncated or could not be read.\n");
    return failed || overflow ? 3 : 0;
}
