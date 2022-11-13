#include <stdio.h>
#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <assert.h>
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
void handler(void *userdata, int result, size_t text_size, const char *text) {
    assert(userdata == ExampleState);
    switch(result){
        case APRIL_RESULT_RECOGNITION_APPEND:
            for(int i=0; i<backspace_needed; i++) fprintf(stderr, "\b");
            for(int i=0; i<backspace_needed; i++) fprintf(stderr, " ");
            for(int i=0; i<backspace_needed; i++) fprintf(stderr, "\b");
            backspace_needed = 0;
            fprintf(stderr, "%s", text);
            break;

        case APRIL_RESULT_RECOGNITION_LOOKAHEAD:
            for(int i=0; i<backspace_needed; i++) fprintf(stderr, "\b");
            for(int i=0; i<backspace_needed; i++) fprintf(stderr, " ");
            for(int i=0; i<backspace_needed; i++) fprintf(stderr, "\b");
            backspace_needed = text_size;
            fprintf(stderr, "%s", text);
            break;

        default:
            assert(false);
    }
}

int main(int argc, char *argv[]){
    if(argc != 3){
        printf("Usage: %s [file] [modelpath]\n", argv[0]);
        printf(" - [file] must be a 16000Hz raw PCM16 file\n");
        printf(" - [modelpath] must be a path to the models\n");
        return 1;
    }


    aam_api_init();
    AprilASRModel model = aam_create_model(argv[2]);
    AprilASRSession session = aas_create_session(model, handler, (void*)ExampleState, NULL);
    if(argv[1][0] == '-' && argv[1][1] == 0) {
        // read from stdin
        char data[6400];
        for(;;){
            size_t r = 0;
            while(r < 6400){
                r += read(STDIN_FILENO, &data[r], 6400 - r);
            }

            // avoid processing silence
            //bool found_nonzero = false;
            //for(int i=0; i<r; i++){
            //    if(data[i] != 0){
            //        found_nonzero = true;
            //        break;
            //    }
            //}
            //if(!found_nonzero) continue;
            

            aas_feed_pcm16(session, (short *)data, r/2);
        }
    } else if (argv[1][0] == '?' && argv[1][1] == 0) {
        // memory leak check, just send some blank 
        char data[6400];
        memset(data, 0, 6400);
        aas_feed_pcm16(session, (short *)data, 3200);
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

        for(int i=0; i<100; i++)
            aas_feed_pcm16(session, (short *)file_data, sz1/2);
        printf("\n");
        free(file_data);
    }

    aas_free(session);
    aam_free(model);

    return 0;
}