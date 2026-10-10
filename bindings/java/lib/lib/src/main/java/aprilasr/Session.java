package aprilasr;

import com.sun.jna.Pointer;
import com.sun.jna.NativeLong;
import com.sun.jna.CallbackReference;

class NativeHandler implements AprilAsrNative.AprilRecognitionResultHandler {
    Session.CallbackHandler userHandler;

    @Override
    public void invoke(Pointer userdata, int result, NativeLong count, Pointer ptr) {
        int size = count.intValue();

        if(ptr == null) {
            if(result == 3) userHandler.onErrorCantKeepUp();
            else if(result == 4) userHandler.onSilence();

            return;
        }

        Token[] userTokens = new Token[size];

        if(size > 0) {
            AprilAsrNative.AprilToken first = new AprilAsrNative.AprilToken(ptr);
            AprilAsrNative.AprilToken[] tokens = (AprilAsrNative.AprilToken[]) first.toArray(size);

            for (int i = 0; i < size; i++) {
                userTokens[i] = new Token(tokens[i]);
            }
        }

        if(result == 1){
            userHandler.onPartialResult(userTokens);
        }else if(result == 2){
            userHandler.onFinalResult(userTokens);
        }
    }

    public NativeHandler(Session.CallbackHandler userHandler) {
        this.userHandler = userHandler;
    }
}

public class Session {
    public interface CallbackHandler {
        void onPartialResult(Token[] tokens);
        void onFinalResult(Token[] tokens);

        void onSilence();
        void onErrorCantKeepUp();
    }


    private NativeHandler nativeHandler;

    private Model model;

    private Pointer handle;

    public Session(Model model, CallbackHandler handler, boolean async, boolean noRT, String speakerName) {
        this.model = model;
        this.nativeHandler = new NativeHandler(handler);

        AprilAsrNative.AprilConfig.ByValue config = new AprilAsrNative.AprilConfig.ByValue();
        config.handler = CallbackReference.getFunctionPointer(this.nativeHandler);
        config.flags = (async && noRT) ? 2 : (async ? 1 : 0);

        if((speakerName != null) && (speakerName.length() > 0)){
            int nameHash = speakerName.hashCode();
            config.speaker[0] = (byte)(nameHash >>> 24);
            config.speaker[1] = (byte)(nameHash >>> 16);
            config.speaker[2] = (byte)(nameHash >>> 8);
            config.speaker[3] = (byte)(nameHash);
        }


        Pointer session = AprilAsrNative.aas_create_session(model.handle, config);
        if(session == null){
            throw new IllegalArgumentException("Failed to create session with given model");
        }

        this.handle = session;
    }

    public Session(Model model, CallbackHandler handler, boolean async, String speakerName) {
        this(model, handler, async, false, speakerName);
    }

    public Session(Model model, CallbackHandler handler, boolean async) {
        this(model, handler, async, false, null);
    }

    public Session(Model model, CallbackHandler handler) {
        this(model, handler, false, false, null);
    }

    public void feedPCM16(short[] data, int length) {
        AprilAsrNative.aas_feed_pcm16(this.handle, data, (long)length);
    }

    /**
     * If the session is asynchronous and realtime, returns the factor by
     * which audio is being sped up to keep up. Sessions created with noRT
     * never speed up audio and always return 1.0; use getBacklogMs instead.
     */
    public float getRTSpeedup() {
        return AprilAsrNative.aas_realtime_get_speedup(this.handle);
    }

    /**
     * Returns how many milliseconds of fed audio an asynchronous session has
     * not processed yet, or 0 for a synchronous session. A backlog that keeps
     * growing means the system cannot keep up; once the internal buffer is
     * full, audio is dropped and onErrorCantKeepUp is called.
     */
    public long getBacklogMs() {
        return AprilAsrNative.aas_get_backlog_ms(this.handle).longValue();
    }

    /**
     * Flush any remaining samples and force the session to produce a final
     * result. In an asynchronous session the flush is queued in order with
     * the audio, so audio fed afterwards belongs to the next utterance and
     * feeding may continue immediately. Call {@link #waitIdle()} when the
     * final result must have been delivered before continuing.
     */
    public void flush() {
        AprilAsrNative.aas_flush(this.handle);
    }

    /**
     * Wait for queued asynchronous work and callbacks to finish, including
     * the final results of every queued flush. Returns immediately for
     * synchronous sessions.
     *
     * @throws IllegalStateException if called from this session's callback,
     *         which would deadlock
     */
    public void waitIdle() {
        if (AprilAsrNative.aas_wait(this.handle) == 0) {
            throw new IllegalStateException("Cannot wait for a session from its own callback");
        }
    }
}