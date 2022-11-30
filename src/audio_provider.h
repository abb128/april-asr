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