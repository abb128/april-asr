using System;
using System.Runtime.InteropServices;
using AprilAsr;

namespace AprilAsr.PINVOKE {
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
    internal struct AprilSpeakerID
    {
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 16)]
        public byte[] data;
    }


    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    internal delegate void AprilRecognitionResultHandler(
        IntPtr userdata,
        int resultType,
        ulong numTokens,

        [MarshalAs(UnmanagedType.LPArray, SizeParamIndex = 2)]
        AprilToken[] tokens
    );

    [StructLayout(LayoutKind.Sequential)]
    internal struct AprilConfig 
    {
        public AprilSpeakerID speaker;

        [MarshalAs(UnmanagedType.FunctionPtr)]
        public AprilRecognitionResultHandler handler;
        public IntPtr userdata;

        public int flags;
    }

    internal class AprilAsrPINVOKE
    {
        [DllImport("libaprilasr", CallingConvention = CallingConvention.Cdecl)]
        internal static extern void aam_api_init(int version);

        [DllImport("libaprilasr", CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr aam_create_model(string path);

        [DllImport("libaprilasr", CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr aam_get_name(IntPtr handle);

        [DllImport("libaprilasr", CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr aam_get_description(IntPtr handle);

        [DllImport("libaprilasr", CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr aam_get_language(IntPtr handle);

        [DllImport("libaprilasr", CallingConvention = CallingConvention.Cdecl)]
        internal static extern int aam_get_sample_rate(IntPtr handle);

        [DllImport("libaprilasr", CallingConvention = CallingConvention.Cdecl)]
        internal static extern void aam_free(IntPtr handle);

        [DllImport("libaprilasr", CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr aas_create_session(IntPtr model, AprilConfig config);

        [DllImport("libaprilasr", CallingConvention = CallingConvention.Cdecl)]
        internal static extern void aas_feed_pcm16(IntPtr session, short[] samples, int num_samples);

        [DllImport("libaprilasr", CallingConvention = CallingConvention.Cdecl)]
        internal static extern void aas_flush(IntPtr session);

        [DllImport("libaprilasr", CallingConvention = CallingConvention.Cdecl)]
        internal static extern float aas_realtime_get_speedup(IntPtr session);

        [DllImport("libaprilasr", CallingConvention=CallingConvention.Cdecl)]
        internal static extern void aas_free(IntPtr session);

        static AprilAsrPINVOKE()
        {
            aam_api_init(1);
        }

        internal static string PtrToStringUTF8(System.IntPtr ptr)
        {
            int len = 0;
            while (System.Runtime.InteropServices.Marshal.ReadByte(ptr, len) != 0)
                len++;
            byte[] array = new byte[len];
            System.Runtime.InteropServices.Marshal.Copy(ptr, array, 0, len);
            return System.Text.Encoding.UTF8.GetString(array);
        }
    }
}
