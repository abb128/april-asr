using System;
using AprilAsr.PINVOKE;

namespace AprilAsr
{
    /// <summary>
    /// Result type that is passed to your handler
    /// </summary>
    public enum AprilResultKind : int {
        /// <summary>
        /// A partial recognition. The next handler call will contain an updated
        /// list of tokens.
        /// </summary>
        PartialRecognition = 1,

        /// <summary>
        /// A final recognition. The next handler call will start from an empty
        /// token list.
        /// </summary>
        FinalRecognition = 2,

        /// <summary>
        /// In an asynchronous session, this may be called when the system can't
        /// keep up with the incoming audio, and samples have been dropped. The
        /// accuracy will be affected. An empty token list is given
        /// </summary>
        ErrorCantKeepUp = 3,

        /// <summary>
        /// Called after some silence. An empty token list is given
        /// </summary>
        Silence = 4
    }

    /// <summary>
    /// Session callback type. You must provide one of such type when constructing
    /// a session.
    /// </summary>
    public delegate void SessionCallback(AprilResultKind kind, AprilToken[] tokens);

    /// <summary>
    /// The session is what performs the actual speech recognition. It has
    /// methods to input audio, and it calls your given handler with decoded
    /// results.
    /// 
    /// You need to pass a Model when constructing a Session.
    /// </summary>
    public class AprilSession
    {
        private IntPtr handle;
        private AprilModel model;
        private SessionCallback callback;
        private AprilRecognitionResultHandler handler;

        private void handleAprilCallback(
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

        /// <summary>
        /// Construct a session with the given session, callback and options.
        /// </summary>
        /// <param name="model">The model to use for the session.</param>
        /// <param name="callback">The callback to call with decoded results.</param>
        /// <param name="async">Whether or not to run the session asynchronously (perform calculations in background thread)</param>
        /// <param name="noRT">Whether or not to run the session non-realtime if async is true. Has no effect if async is false.</param>
        /// <param name="speakerName">Unique name if there is one specific speaker. This is not yet implemented and has no effect.</param>
        public AprilSession(AprilModel model, SessionCallback callback, bool async = false, bool noRT = false, string speakerName = "") {
            this.model = model;
            this.callback = callback;
            this.handler = new AprilRecognitionResultHandler(this.handleAprilCallback)

            AprilConfig config = new AprilConfig();
            config.handler = this.handler;

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

        /// <summary>
        /// Feed the given pcm16 samples in bytes to the session. If the session is
        /// asynchronous, this will return immediately and queue the data for the
        /// background thread to process. If the session is not asynchronous, this
        /// will block your thread and potentially call the handler before
        /// returning.
        /// </summary>
        public void FeedPCM16(short[] samples, int num_samples)
        {
            AprilAsrPINVOKE.aas_feed_pcm16(handle, samples, num_samples);
        }

        /// <summary>
        /// If the session is asynchronous and realtime, this will return a
        /// positive float. A value below 1.0 means the session is keeping up, and
        /// a value greater than 1.0 means the input audio is being sped up by that
        /// factor in order to keep up. When the value is greater 1.0, the accuracy
        /// is likely to be affected.
        /// </summary>
        public float GetRTSpeedup()
        {
            return AprilAsrPINVOKE.aas_realtime_get_speedup(handle);
        }

        /// <summary>
        /// Flush any remaining samples and force the session to produce a final
        /// result.
        /// </summary>
        public void Flush()
        {
            AprilAsrPINVOKE.aas_flush(handle);
        }
    }
}
