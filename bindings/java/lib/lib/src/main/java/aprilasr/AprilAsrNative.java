package aprilasr;

import com.sun.jna.Native;
import com.sun.jna.Platform;
import com.sun.jna.Pointer;
import com.sun.jna.NativeLong;
import com.sun.jna.Callback;
import com.sun.jna.Structure;
import com.sun.jna.Structure.FieldOrder;
import java.io.File;
import java.io.InputStream;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.StandardCopyOption;


public class AprilAsrNative {
    @FieldOrder({"data"})
    public static class AprilSpeakerID extends Structure {
        public byte[] data = new byte[16];

        public AprilSpeakerID(){}
    };

    @FieldOrder({"token","logprob","flags"})
    public static class AprilToken extends Structure {
        public String token;
        public float logprob;
        public int flags;
        public NativeLong time_ms;
        private Pointer reserved;
    };

    public static interface AprilRecognitionResultHandler extends Callback {
        void invoke(Pointer userdata, int result, NativeLong count, Pointer tokens);
    }

    @FieldOrder({"speaker", "handler", "userdata", "flags"})
    public static class AprilConfig extends Structure {
        public AprilSpeakerID speaker = new AprilSpeakerID();
        
        public AprilRecognitionResultHandler handler = null;
        public Pointer userdata = null;

        public int flags = 0;

        public AprilConfig(){}
    };

    private static void unpackDll(File targetDir, String lib) throws IOException {
        InputStream source = AprilAsrNative.class.getResourceAsStream("/win32-x86-64/" + lib + ".dll");
        Files.copy(source, new File(targetDir, lib + ".dll").toPath(), StandardCopyOption.REPLACE_EXISTING);
    }

    static {
        if (Platform.isWindows()) {
            // We have to unpack dependencies
            try {
                // To get a tmp folder we unpack small library and mark it for deletion
                File tmpFile = Native.extractFromResourcePath("/win32-x86-64/empty", AprilAsrNative.class.getClassLoader());
                File tmpDir = tmpFile.getParentFile();
                new File(tmpDir, tmpFile.getName() + ".x").createNewFile();

                // Now unpack dependencies
                unpackDll(tmpDir, "onnxruntime");
            } catch (IOException e) {
                // Nothing for now, it will fail on next step
            } finally {
                Native.register(AprilAsrNative.class, "libaprilasr");
            }
        } else {
            Native.register(AprilAsrNative.class, "aprilasr");
        }
    }

    public static final int APRIL_VERSION = 1;

    public static native void aam_api_init(int version);
    public static native Pointer aam_create_model(String path);

    public static native String aam_get_name(Pointer model);
    public static native String aam_get_description(Pointer model);
    public static native String aam_get_language(Pointer model);

    public static native NativeLong aam_get_sample_rate(Pointer model);

    public static native void aam_free(Pointer model);

    public static native Pointer aas_create_session(Pointer model, AprilConfig config);
    public static native void aas_feed_pcm16(Pointer session, short[] pcm16, NativeLong short_count);
    public static native void aas_flush(Pointer session);

    public static native float aas_realtime_get_speedup(Pointer session);

    public static native void aas_free(Pointer session);
}
