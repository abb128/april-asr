using System.Runtime.InteropServices;

namespace AprilAsr
{
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
    public struct AprilToken
    {
        private IntPtr _token;
        public float LogProb;
        public int Flags;

        public string Token
        {
            get { return Marshal.PtrToStringUTF8(_token) ?? ""; }
        }
    }
}
