using System;
using System.Runtime.InteropServices;
using AprilAsr.PINVOKE;

namespace AprilAsr
{
    /// <summary>
    /// Models end with the file extension `.april`. You need to pass a path to
    /// such a file to construct a Model type.
    /// 
    /// Each model has its own sample rate in which it expects audio. There is a
    /// method to get the expected sample rate. Usually, this is 16000 Hz.
    /// 
    /// Models also have additional metadata such as name, description, language.
    /// 
    /// After loading a model, you can create one or more sessions that use the
    /// model.
    /// </summary>
    public class AprilModel
    {
        internal IntPtr handle;

        /// <summary>
        /// Loads an april model given a path to a model file.
        /// May throw an exception if the file is invalid.
        /// </summary>
        /// <param name="modelPath">Path to an april model file</param>
        public AprilModel(string modelPath)
        {
            handle = AprilAsrPINVOKE.aam_create_model(modelPath);
            if(handle == IntPtr.Zero)
            {
                throw new Exception("Failed to load model");
            }
        }

        ~AprilModel()
        {
            AprilAsrPINVOKE.aam_free(handle);
        }

        /// <value>The name of the model as stored in the file metadata</value>
        public string Name
        {
            get { return AprilAsrPINVOKE.PtrToStringUTF8(AprilAsrPINVOKE.aam_get_name(handle)) ?? ""; }
        }

        /// <value>The description of the model as stored in the file metadata</value>
        public string Description
        {
            get { return AprilAsrPINVOKE.PtrToStringUTF8(AprilAsrPINVOKE.aam_get_description(handle)) ?? ""; }
        }

        /// <value>The language of the model as stored in the file metadata</value>
        public string Language
        {
            get { return AprilAsrPINVOKE.PtrToStringUTF8(AprilAsrPINVOKE.aam_get_language(handle)) ?? ""; }
        }

        /// <value>The sample rate of the model as stored in the file metadata</value>
        public int SampleRate
        {
            get { return AprilAsrPINVOKE.aam_get_sample_rate(handle); }
        }
    }
}
