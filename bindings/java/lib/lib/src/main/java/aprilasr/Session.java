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

    public float getRTSpeedup() {
        return AprilAsrNative.aas_realtime_get_speedup(this.handle);
    }

    public void flush() {
        AprilAsrNative.aas_flush(this.handle);
    }
}