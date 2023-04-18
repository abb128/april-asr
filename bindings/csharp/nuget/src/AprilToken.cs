using AprilAsr.PINVOKE;
using System;
using System.Runtime.InteropServices;

namespace AprilAsr
{
    /// <summary>
    /// A token may be a single letter, a word chunk, an entire word, punctuation,
    /// or other arbitrary set of characters.
    /// 
    /// To convert a token array to a string, simply concatenate the strings from
    /// each token. You don't need to add spaces between tokens, the tokens
    /// contain their own formatting.
    /// 
    /// Tokens also contain the log probability, and a boolean denoting whether or
    /// not it's a word boundary. In English, the word boundary value is equivalent
    /// to checking if the first character is a space.
    /// </summary>
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
    public struct AprilToken
    {
        private IntPtr _token;
        private float _logProb;
        private int _flags;
        private UIntPtr _timeMs;
        private IntPtr _reserved;

        /// <value>The token as a string</value>
        public string Token
        {
            get { return AprilAsrPINVOKE.PtrToStringUTF8(_token) ?? ""; }
        }

        /// <value>The probability this is a correct token</value>
        public float LogProb
        {
            get { return _logProb; }
        }

        /// <value>Whether or not this marks the start of a new word</value>
        public bool WordBoundary
        {
            get { return (_flags & 1) != 0; }
        }

        /// <value>Whether or not this marks the end of a sentence</value>
        public bool SentenceEnd
        {
            get { return (_flags & 2) != 0; }
        }

        /// <value>
        /// The millisecond at which this was emitted. Counting is based on how much
        /// audio is being fed (time is not advanced when the session is not being
        /// given audio)
        /// </value>
        public float Time
        {
            get { return (float)(((double)_timeMs.ToUInt64()) / 1000.0); }
        }
    }
}
