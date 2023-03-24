#include <stdio.h>
#include <cstdlib>
#include <cstring>
#include <assert.h>
#include <time.h>
#include <errno.h>
#include "april_api.h"

#ifndef _MSC_VER
#include <unistd.h>
#else
#include <io.h>
#include <BaseTsd.h>
#define STDIN_FILENO 0
typedef SSIZE_T ssize_t;
#endif

#define BUFFER_SIZE 1024
int ends_with(const char *str, const char *suffix);

struct wav_header {
    // RIFF Header
    char riff_header[4]; // Contains "RIFF"
    uint32_t wav_size; // Size of the wav portion of the file, which follows the first 8 bytes. File size - 8
    char wave_header[4]; // Contains "WAVE"
    
    // Format Header
    char fmt_header[4]; // Contains "fmt " (includes trailing space)
    int32_t fmt_chunk_size; // Should be 16 for PCM
    int16_t audio_format; // Should be 1 for PCM. 3 for IEEE Float
    int16_t num_channels;
    int32_t sample_rate;
    int32_t byte_rate; // Number of bytes per second. sample_rate * num_channels * Bytes Per Sample
    int16_t sample_alignment; // num_channels * Bytes Per Sample
    int16_t bit_depth; // Number of bits per sample
    
    // Data
    char data_header[4]; // Contains "data"
    uint32_t data_bytes; // Number of bytes in data. Number of samples * num_channels * sample byte size
};

static_assert(sizeof(wav_header) == 44L, "wav header must be 44 bytes");

// In this example, the internal state is just a global struct just for testing
// In your program you can pass any pointer into userdata to access it in the
// handler
struct {
    int xyz;
} some_internal_state;

// This callback function will get called every time a new result is decoded.
// It's passed into the AprilConfig along with the userdata pointer.
void handler(void *userdata, AprilResultType result, size_t count, const AprilToken *tokens) {
    assert(userdata == &some_internal_state);

    switch(result){
        case APRIL_RESULT_RECOGNITION_FINAL: 
            printf("@ ");
            break;
        case APRIL_RESULT_RECOGNITION_PARTIAL:
            printf("- ");
            break;
        case APRIL_RESULT_SILENCE:
            break;
        default:
            assert(false);
            return;
    }

    for(int t=0; t<count; t++){
        const char *text = tokens[t].token;
        printf("%s", text);
    }
    printf("\n");
}

int main(int argc, char *argv[]){
    if(argc != 3){
        printf("Usage: %s [file] [modelpath]\n", argv[0]);
        printf(" - [file] must be a 16000Hz raw PCM16 file, or may be - for stdin\n");
        printf(" - [modelpath] must be a path to the models\n");
        return 1;
    }

    const char *input_file = argv[1];
    const char *input_model = argv[2];
    
    // In the start of our program we should call aam_api_init.
    // This should only be called once.
    aam_api_init(APRIL_VERSION);

    // Next we should load the model. The model by itself doesn't allow us
    // to do much except for get the metadata. If loading the model
    // fails, NULL is returned.
    AprilASRModel model = aam_create_model(input_model);
    if(model == NULL){
        printf("Loading model %s failed!\n", input_model);
        return 1;
    }
    
    size_t model_sample_rate = aam_get_sample_rate(model);
    printf("Model name: %s\n", aam_get_name(model));
    printf("Model desc: %s\n", aam_get_description(model));
    printf("Model lang: %s\n", aam_get_language(model));
    printf("Model samplerate: %ld\n\n", model_sample_rate);


    // To do actual speech recognition, it's necessary to create a session.
    // Models and sessions are separate to allow for more efficient handling
    // of multiple sessions. For example, an application may be performing
    // recognition on 20 different audio streams by using 20 different sessions
    // and by re-using the same AprilASRModel the relative memory use is low.
    // However, it's important that the AprilASRModel does not get freed
    // before all of its sessions.
    AprilConfig config = { 0 };
    config.handler = handler;
    config.userdata = (void*)&some_internal_state;

    // By default, the session runs in synchronous mode. If you want async
    // processing, you may choose to set it to APRIL_CONFIG_FLAG_ASYNC_RT_BIT
    // here.
    config.flags = APRIL_CONFIG_FLAG_ZERO_BIT;

    AprilASRSession session = aas_create_session(model, config);


    if(argv[1][0] == '-' && argv[1][1] == 0) {
        // Reading stdin mode. It's assumed that the input data is pcm16 audio,
        // sampled in the model's sample rate.
        // You can achieve this on Linux like this:
        // $ parec --format=s16 --rate=16000 --channels=1 --latency-ms=100 | ./main - /path/to/model.april

        char data[BUFFER_SIZE];
        ssize_t r;
        for(;;){
            r = read(STDIN_FILENO, data, BUFFER_SIZE);
            
            if (r == -1) {
                aas_flush(session);
                break;
            } else
            if (r <= 0) {
                continue;
            }
            
            aas_feed_pcm16(session, (short *)data, r/2);
        }
    } else if (argv[1][0] == '?' && argv[1][1] == 0) {
        // Run some blank data, mainly for memory leak testing
        char data[6400];
        memset(data, 0, 6400);
        aas_feed_pcm16(session, (short *)data, 3200);
        aas_flush(session);
    } else {
        // wave file mode, the file must be in PCM16 format sampled in the
        // model's sample rate.
        FILE *fd = fopen(argv[1], "rb");
        
        if(fd == 0){
            printf("Failed to open file %s\n", argv[1]);
            return 2;
        }

        fseek(fd, 0L, SEEK_END);
        size_t sz = ftell(fd);
        size_t offset = 0L;

        fseek(fd, 0L, SEEK_SET);

        // Verify the RIFF header if supplied
        if(ends_with(argv[1], ".wav")) {
            wav_header header;
            fread(&header, 1L, 44L, fd);

            bool is_valid_wav = (header.fmt_chunk_size == 16)
                             && (header.audio_format == 1)
                             && (header.sample_rate == model_sample_rate)
                             && (header.num_channels == 1);
            
            if(!is_valid_wav){
                printf("Wave file must be single-channel 16-bit PCM sampled in %llu Hz!\n", model_sample_rate);
                return 2;
            }

            offset = 44L;
        }

        fseek(fd, 0L, SEEK_SET);

        // read the file
        uint8_t *file = (uint8_t *)calloc(1, sz);
        if (fread(file, 1, sz, fd) != sz) {
            printf("reading file failed\n");
            return 4;
        }

        int16_t *file_data = (int16_t *)(file + offset);
        size_t num_shorts = (sz - offset) / 2;

        // For synchronous mode, it's possible to feed the entire thing at once
        // For asynchronous, you may want to break it up into smaller chunks
        // over time because the size of the internal buffer is limited
        aas_feed_pcm16(session, file_data, num_shorts);

        // Flushing makes sure any remaining frames get processed and a final
        // result is given
        aas_flush(session);

        printf("\ndone\n");

        free(file);
        fclose(fd);
    }

    aas_free(session);
    aam_free(model);

    return 0;
}


int ends_with(const char *str, const char *suffix) {
    if (!str || !suffix)
        return 0;
    size_t lenstr = strlen(str);
    size_t lensuffix = strlen(suffix);
    if (lensuffix >  lenstr)
        return 0;
    return strncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
}
