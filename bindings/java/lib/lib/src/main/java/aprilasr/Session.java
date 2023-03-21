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

    public Session(Model model, CallbackHandler handler) {
        this.model = model;
        this.nativeHandler = new NativeHandler(handler);

        AprilAsrNative.AprilConfig.ByValue config = new AprilAsrNative.AprilConfig.ByValue();
        config.handler = CallbackReference.getFunctionPointer(this.nativeHandler);
        config.flags = 0;

        Pointer session = AprilAsrNative.aas_create_session(model.handle, config);
        if(session == null){
            throw new IllegalArgumentException("Failed to create session with given model");
        }

        this.handle = session;
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