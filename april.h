#ifdef __cplusplus
extern "C" {
#endif


void aam_api_init(void);



struct AprilASRModel_i;
typedef struct AprilASRModel_i * AprilASRModel;

AprilASRModel aam_create_model(const char *model_path);




struct AprilASRSession_i;
typedef struct AprilASRSession_i * AprilASRSession;

AprilASRSession aas_create_session(AprilASRModel model);
void aas_feed_pcm16(AprilASRSession aas, short *pcm16, size_t short_count);


#ifdef __cplusplus
}
#endif