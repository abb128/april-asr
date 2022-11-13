#ifdef __cplusplus
extern "C" {
#endif


void aam_api_init(void);



struct AprilASRModel_i;
typedef struct AprilASRModel_i * AprilASRModel;

AprilASRModel aam_create_model(const char *model_path);

// Caller must ensure all sessions backed by model are freed
// before model is freed
void aam_free(AprilASRModel model);




struct AprilASRSession_i;
typedef struct AprilASRSession_i * AprilASRSession;

AprilASRSession aas_create_session(AprilASRModel model);
void aas_feed_pcm16(AprilASRSession session, short *pcm16, size_t short_count);
void aas_free(AprilASRSession session);

#ifdef __cplusplus
}
#endif