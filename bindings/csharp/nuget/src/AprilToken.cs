using AprilAsr.PINVOKE;
using System;
using System.Runtime.InteropServices;

namespace AprilAsr
{
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
    public struct AprilToken
    {
        private IntPtr _token;
        private float _logProb;
        private int _flags;

        public string Token
        {
            get { return AprilAsrPINVOKE.PtrToStringUTF8(_token) ?? ""; }
        }

        public float LogProb
        {
            get { return _logProb; }
        }

        public bool WordBoundary
        {
            get { return (_flags & 1) != 0; }
        }
    }
}
