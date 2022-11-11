#ifdef __cplusplus
extern "C" {
#endif


void aam_api_init(void);

struct AprilASRModel_i;
typedef struct AprilASRModel_i * AprilASRModel;

AprilASRModel aam_create_model(const char *model_path);

// TODO later: later separate this to AprilASRSession, we don't want ANY state in AprilASRModel
// TODO later later: maybe do have state in AprilASRModel, and run bg thread, collate, batch, etc
void aam_feed_pcm16(AprilASRModel aam, short *pcm16, size_t short_count);


#ifdef __cplusplus
}
#endif