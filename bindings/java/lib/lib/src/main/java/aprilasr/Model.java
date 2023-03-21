package aprilasr;

import com.sun.jna.Pointer;

public class Model {
    Pointer handle;

    public Model(String path){
        Pointer model = AprilAsrNative.aam_create_model(path);

        if(model == null){
            throw new IllegalArgumentException("Failed to load model at given path");
        }

        this.handle = model;
    }

    public String getName() {
        return AprilAsrNative.aam_get_name(this.handle);
    }

    public String getLanguage() {
        return AprilAsrNative.aam_get_language(this.handle);
    }

    public String getDescription() {
        return AprilAsrNative.aam_get_description(this.handle);
    }

    public int getSampleRate() {
        return AprilAsrNative.aam_get_sample_rate(this.handle).intValue();
    }

}