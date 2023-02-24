using System.Runtime.InteropServices;

namespace AprilAsrPINVOKE {
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
    public struct AprilSpeakerID
    {
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 16)]
        public byte[] data;
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
    public struct AprilToken
    {
        public IntPtr token;
        public float logprob;
        public int flags;

        public string GetToken()
        {
            return Marshal.PtrToStringUTF8(this.token) ?? "";
        }
    }

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void AprilRecognitionResultHandler(
        IntPtr userdata,
        int resultType,
        ulong numTokens,

        [MarshalAs(UnmanagedType.LPArray, SizeParamIndex = 2)]
        AprilToken[] tokens
    );

    [StructLayout(LayoutKind.Sequential)]
    public struct AprilConfig 
    {
        public AprilSpeakerID speaker;

        [MarshalAs(UnmanagedType.FunctionPtr)]
        public AprilRecognitionResultHandler handler;
        public IntPtr userdata;

        public int flags;
    }
    class AprilAsrPINVOKE
    {
        [DllImport("aprilasr", CallingConvention = CallingConvention.Cdecl)]
        public static extern void aam_api_init();

        [DllImport("aprilasr", CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr aam_create_model(string path);

        [DllImport("aprilasr", CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr aam_get_name(IntPtr handle);

        [DllImport("aprilasr", CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr aam_get_description(IntPtr handle);

        [DllImport("aprilasr", CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr aam_get_language(IntPtr handle);

        [DllImport("aprilasr", CallingConvention = CallingConvention.Cdecl)]
        public static extern int aam_get_sample_rate(IntPtr handle);

        [DllImport("aprilasr", CallingConvention = CallingConvention.Cdecl)]
        public static extern void aam_free(IntPtr handle);

        [DllImport("aprilasr", CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr aas_create_session(IntPtr model, AprilConfig config);

        [DllImport("aprilasr", CallingConvention = CallingConvention.Cdecl)]
        public static extern void aas_feed_pcm16(IntPtr session, short[] samples, int num_samples);

        [DllImport("aprilasr", CallingConvention = CallingConvention.Cdecl)]
        public static extern void aas_flush(IntPtr session);

        [DllImport("aprilasr", CallingConvention = CallingConvention.Cdecl)]
        public static extern float aas_realtime_get_speedup(IntPtr session);

        [DllImport("aprilasr", CallingConvention=CallingConvention.Cdecl)]
        public static extern void aas_free(IntPtr session);
    }
}
