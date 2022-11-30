#include <stdio.h>
#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <assert.h>
#include <time.h>
#include "april_api.h"

int ends_with(const char *str, const char *suffix) {
    if (!str || !suffix)
        return 0;
    size_t lenstr = strlen(str);
    size_t lensuffix = strlen(suffix);
    if (lensuffix >  lenstr)
        return 0;
    return strncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
}

const char *ExampleState = "This is a test to ensure userdata is passed correctly";

int backspace_needed = 0;
int line_length = 0;
void handler(void *userdata, AprilResultType result, size_t count, const AprilToken *tokens) {
    assert(userdata == ExampleState);
    
    switch(result){
        case APRIL_RESULT_RECOGNITION_FINAL: 
            printf("@ ");
            break;
        case APRIL_RESULT_RECOGNITION_PARTIAL:
            printf("- ");
            break;
        default:
            assert(false);
    }

    for(int t=0; t<count; t++){
        const char *text = tokens[t].token;
        printf("%s", text);
    }
    printf("\n");
}

void dummy_handler(void *userdata, AprilResultType result, size_t count, const AprilToken *tokens){}

int main(int argc, char *argv[]){
    if(argc != 3){
        printf("Usage: %s [file] [modelpath]\n", argv[0]);
        printf(" - [file] must be a 16000Hz raw PCM16 file\n");
        printf(" - [modelpath] must be a path to the models\n");
        return 1;
    }

    const size_t BUFFER_SIZE = 512*2;

    aam_api_init();
    AprilASRModel model = aam_create_model(argv[2]);
    assert(model != NULL);
    printf("Model name: %s\n", aam_get_name(model));
    printf("Model desc: %s\n", aam_get_description(model));
    printf("Model lang: %s\n", aam_get_language(model));
    printf("Model samplerate: %d\n\n", aam_get_sample_rate(model));

    AprilConfig config = {
        .handler = handler,
        .userdata = (void*)ExampleState,
        .flags = ARPIL_CONFIG_FLAG_SYNCHRONOUS
    };

    AprilASRSession session = aas_create_session(model, config);
    if(argv[1][0] == '-' && argv[1][1] == 0) {
        // read from stdin
        char data[BUFFER_SIZE];
        for(;;){
            size_t r = 0;
            //while(r < BUFFER_SIZE){
                r += read(STDIN_FILENO, &data[r], BUFFER_SIZE - r);
            //}

            // avoid processing silence
            bool found_nonzero = false;
            for(int i=0; i<r; i++){
                if(data[i] != 0){
                    found_nonzero = true;
                    break;
                }
            }
            if(!found_nonzero) {
                aas_flush(session);
                continue;
            }
            

            aas_feed_pcm16(session, (short *)data, r/2);
        }
    } else if (argv[1][0] == '?' && argv[1][1] == 0) {
        // memory leak check, just send some blank 
        char data[6400];
        memset(data, 0, 6400);
        aas_feed_pcm16(session, (short *)data, 3200);
        aas_flush(session);
    } else if (argv[1][0] == 'b' && argv[1][1] == 0) {
        // benchmark
        /*
        short noise_1sec[48000];
        for(int i=0; i<48000; i++){
            noise_1sec[i] = rand();
        }

        size_t sr = aam_get_sample_rate(model);


        feed_pcm_task tasks[64];
        AprilASRSession sessions[64];
        for(int i=0; i<64; i++){
            sessions[i] = aas_create_session(model, dummy_handler, (void*)ExampleState, NULL);
            tasks[i].noise_1sec = noise_1sec;
            tasks[i].noise_1sec_size = 48000;
            tasks[i].session_sample_rate = sr;
            tasks[i].session = sessions[i];
            tasks[i].num_seconds_to_feed = 10;
        }

        pthread_t threads[64];
        for(int num_sessions=1; num_sessions<64; num_sessions++){
            time_t begin = time(NULL);

            for(int i=0; i<num_sessions; i++)
                pthread_create(&threads[i], NULL, feed_pcm_task_runner, &tasks[i]);

            for(int i=0; i<num_sessions; i++)
                pthread_join(threads[i], NULL);

            time_t end = time(NULL);
            printf("%d sessions - %.2fx faster than realtime\n", num_sessions, 10.0 / difftime(end, begin));
        }


        for(int i=0; i<64; i++){
            aas_free(sessions[i]);
        }
        */
    } else {
        FILE *fd = fopen(argv[1], "r");
        
        if(fd == 0){
            printf("Failed to open file %s\n", argv[1]);
            return 2;
        }

        fseek(fd, 0L, SEEK_END);
        size_t sz = ftell(fd);
        

        if(ends_with(argv[1], ".wav")) {
            fseek(fd, 44L, SEEK_SET);
        } else {
            fseek(fd, 0L, SEEK_SET);
        }


        void *file_data = malloc(sz);
        size_t sz1 = fread(file_data, 1, sz, fd);

        fclose(fd);
        

        if(sz1 % 2 != 0){
            printf("File size not divisible by two, is the file raw pcm16?\nSize: %llu\n", sz1);
            return 4;
        }

        printf("Read file, %llu bytes\n", sz1);

        size_t samples_per_msec = aam_get_sample_rate(model) / 1000;
        for(int j=0; j<1; j++) {
            for(int i=0; i<sz1/BUFFER_SIZE; i++){
                aas_feed_pcm16(session, &((short *)file_data)[i*(BUFFER_SIZE/2)], BUFFER_SIZE/2);
            }
        }
        //for(int i=0; i<1; i++)
        //    aas_feed_pcm16(session, (short *)file_data, sz1/2);

        aas_flush(session);

        printf("\ndone\n");

        free(file_data);
    }

    aas_free(session);
    aam_free(model);

    return 0;
}
