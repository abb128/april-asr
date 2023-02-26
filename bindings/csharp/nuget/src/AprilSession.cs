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
            AprilToken[] tokens
        )
        {
            this.callback((AprilResultKind)resultType, tokens);
        }

        public AprilSession(AprilModel model, SessionCallback callback, bool async = false) {
            this.model = model;
            this.callback = callback;

            AprilConfig config = new AprilConfig() { flags = (async ? 0 : 1) };
            config.handler = this.handleAprilCallback;

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

        public void Flush()
        {
            AprilAsrPINVOKE.aas_flush(handle);
        }
    }
}
