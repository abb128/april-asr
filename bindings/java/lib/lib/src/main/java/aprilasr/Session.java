package aprilasr;

import com.sun.jna.Pointer;
import com.sun.jna.NativeLong;
import com.sun.jna.CallbackReference;
import com.sun.jna.Structure;
import com.sun.jna.Native;

import java.nio.ShortBuffer;

class SampleHandler implements AprilAsrNative.AprilRecognitionResultHandler {
    @Override
    public void invoke(Pointer userdata, int result, NativeLong count, Pointer ptr) {
        if(ptr == null) return;
        int size = count.intValue();

        AprilAsrNative.AprilToken first = new AprilAsrNative.AprilToken(ptr);
        AprilAsrNative.AprilToken[] tokens = (AprilAsrNative.AprilToken[]) first.toArray(size);

        String s = "RESULT: ";
        for(AprilAsrNative.AprilToken token : tokens) {
            s = s + token.token;
        }
        System.out.println(s);
    }

    public SampleHandler() {}
}

public class Session {
    
    private SampleHandler handler = new SampleHandler();

    private Model model;

    private Pointer handle;

    public Session(Model model) {
        this.model = model;

        AprilAsrNative.AprilConfig.ByValue config = new AprilAsrNative.AprilConfig.ByValue();
        config.handler = CallbackReference.getFunctionPointer(this.handler);
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