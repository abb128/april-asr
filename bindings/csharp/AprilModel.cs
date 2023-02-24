using AprilAsr.PINVOKE;
using System.Runtime.InteropServices;

namespace AprilAsr
{
    internal class AprilModel
    {
        internal IntPtr handle;
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

        public string Name
        {
            get { return Marshal.PtrToStringUTF8(AprilAsrPINVOKE.aam_get_name(handle)) ?? ""; }
        }
        public string Description
        {
            get { return Marshal.PtrToStringUTF8(AprilAsrPINVOKE.aam_get_description(handle)) ?? ""; }
        }
        
        public string Language
        {
            get { return Marshal.PtrToStringUTF8(AprilAsrPINVOKE.aam_get_language(handle)) ?? ""; }
        }

        public int SampleRate
        {
            get { return AprilAsrPINVOKE.aam_get_sample_rate(handle); }
        }
    }
}
