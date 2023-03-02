using System;
using AprilAsr.PINVOKE;

namespace AprilAsr
{
    public enum AprilResultKind : int {
        Unknown = 0,
        PartialRecognition = 1,
        FinalRecognition = 2,
        ErrorCantKeepUp = 3,
        Silence = 4
    }

    public delegate void SessionCallback(AprilResultKind Kind, AprilToken[] Tokens);

    public class AprilSession
    {
        IntPtr handle;
        AprilModel model;
        SessionCallback callback;

        void handleAprilCallback(
            IntPtr userdata,
            int resultType,
            ulong numTokens,
            AprilToken[] o_tokens
        )
        {
            if(o_tokens == null) numTokens = 0;
            
            var tokens = new AprilToken[numTokens];
            if(o_tokens != null){
                for(ulong i=0; i<numTokens; i++){
                    tokens[i] = o_tokens[i];
                }
            }
            
            this.callback((AprilResultKind)resultType, tokens);
        }

        public AprilSession(AprilModel model, SessionCallback callback, bool async = false, bool noRT = false, string speakerName = "") {
            this.model = model;
            this.callback = callback;

            AprilConfig config = new AprilConfig();
            config.handler = this.handleAprilCallback;

            if(async && noRT) config.flags = 2;
            else if(async) config.flags = 1;
            else config.flags = 0;

            if(speakerName.Length > 0){
                int hash = speakerName.GetHashCode();
                
                config.speaker.data[0] = (byte)(hash >> 24);
                config.speaker.data[1] = (byte)(hash >> 16);
                config.speaker.data[2] = (byte)(hash >> 8);
                config.speaker.data[3] = (byte)(hash >> 0);
            }

            handle = AprilAsrPINVOKE.aas_create_session(model.handle, config);
            if(handle == IntPtr.Zero)
            {
                throw new Exception("Failed to create session");
            }
        }

        ~AprilSession()
        {
            AprilAsrPINVOKE.aas_free(handle);
        }

        public void FeedPCM16(short[] samples, int num_samples)
        {
            AprilAsrPINVOKE.aas_feed_pcm16(handle, samples, num_samples);
        }

        public float GetRTSpeedup()
        {
            return AprilAsrPINVOKE.aas_realtime_get_speedup(handle);
        }

        public void Flush()
        {
            AprilAsrPINVOKE.aas_flush(handle);
        }
    }
}
