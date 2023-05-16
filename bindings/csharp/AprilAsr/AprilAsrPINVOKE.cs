using System;
using System.Runtime.CompilerServices;
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
        [DllImport("libaprilasr", EntryPoint="aam_api_init", CallingConvention = CallingConvention.Cdecl)]
        internal static extern void aam_api_init(int version);

        [DllImport("libaprilasr", EntryPoint="aam_create_model", CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr aam_create_model(string path);

        [DllImport("libaprilasr", EntryPoint="aam_get_name", CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr aam_get_name(IntPtr handle);

        [DllImport("libaprilasr", EntryPoint="aam_get_description", CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr aam_get_description(IntPtr handle);

        [DllImport("libaprilasr", EntryPoint="aam_get_language", CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr aam_get_language(IntPtr handle);

        [DllImport("libaprilasr", EntryPoint="aam_get_sample_rate", CallingConvention = CallingConvention.Cdecl)]
        internal static extern int aam_get_sample_rate(IntPtr handle);

        [DllImport("libaprilasr", EntryPoint="aam_free", CallingConvention = CallingConvention.Cdecl)]
        internal static extern void aam_free(IntPtr handle);

        [DllImport("libaprilasr", EntryPoint="aas_create_session", CallingConvention = CallingConvention.Cdecl)]
        internal static extern IntPtr aas_create_session(IntPtr model, AprilConfig config);

        [DllImport("libaprilasr", EntryPoint="aas_feed_pcm16", CallingConvention = CallingConvention.Cdecl)]
        internal static extern void aas_feed_pcm16(IntPtr session, short[] samples, int num_samples);

        [DllImport("libaprilasr", EntryPoint="aas_flush", CallingConvention = CallingConvention.Cdecl)]
        internal static extern void aas_flush(IntPtr session);

        [DllImport("libaprilasr", EntryPoint="aas_realtime_get_speedup", CallingConvention = CallingConvention.Cdecl)]
        internal static extern float aas_realtime_get_speedup(IntPtr session);

        [DllImport("libaprilasr", EntryPoint="aas_free", CallingConvention=CallingConvention.Cdecl)]
        internal static extern void aas_free(IntPtr session);

        static AprilAsrPINVOKE()
        {
            aam_api_init(1);
        }


        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        internal static string PtrToStringUTF8(System.IntPtr ptr)
        {
#if NETSTANDARD2_0
            int len = 0;
            while (System.Runtime.InteropServices.Marshal.ReadByte(ptr, len) != 0)
                len++;
            byte[] array = new byte[len];
            System.Runtime.InteropServices.Marshal.Copy(ptr, array, 0, len);
            return System.Text.Encoding.UTF8.GetString(array);
#endif
#if NETSTANDARD2_1_OR_GREATER
            return Marshal.PtrToStringUTF8(ptr);
#endif

        }
    }
}
