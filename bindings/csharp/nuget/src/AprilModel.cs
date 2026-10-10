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
    public class AprilModel : IDisposable
    {
        internal IntPtr handle;

        // Sessions using this model. The native model must outlive them, but
        // finalizers of unreachable objects run in no particular order.
        private readonly object sync = new object();
        private int sessions;
        private bool released;

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

        /// <summary>
        /// Frees the model once every session using it has been disposed or
        /// collected. Prefer disposing models explicitly.
        /// </summary>
        public void Dispose()
        {
            Release();
            GC.SuppressFinalize(this);
        }

        ~AprilModel()
        {
            // The process is exiting; native threads may still be running
            // callbacks, and the OS reclaims the memory.
            if(!Environment.HasShutdownStarted) Release();
        }

        private IntPtr Handle
        {
            get
            {
                lock(sync)
                {
                    if(released || handle == IntPtr.Zero) throw new ObjectDisposedException(nameof(AprilModel));
                    return handle;
                }
            }
        }

        internal void AddSession()
        {
            lock(sync)
            {
                if(released) throw new ObjectDisposedException(nameof(AprilModel));
                sessions++;
            }
        }

        internal void RemoveSession()
        {
            lock(sync)
            {
                sessions--;
                FreeIfUnused();
            }
        }

        private void Release()
        {
            lock(sync)
            {
                released = true;
                FreeIfUnused();
            }
        }

        private void FreeIfUnused()
        {
            if(released && sessions == 0 && handle != IntPtr.Zero)
            {
                AprilAsrPINVOKE.aam_free(handle);
                handle = IntPtr.Zero;
            }
        }

        /// <value>The name of the model as stored in the file metadata</value>
        public string Name
        {
            get { return AprilAsrPINVOKE.PtrToStringUTF8(AprilAsrPINVOKE.aam_get_name(Handle)) ?? ""; }
        }

        /// <value>The description of the model as stored in the file metadata</value>
        public string Description
        {
            get { return AprilAsrPINVOKE.PtrToStringUTF8(AprilAsrPINVOKE.aam_get_description(Handle)) ?? ""; }
        }

        /// <value>The language of the model as stored in the file metadata</value>
        public string Language
        {
            get { return AprilAsrPINVOKE.PtrToStringUTF8(AprilAsrPINVOKE.aam_get_language(Handle)) ?? ""; }
        }

        /// <value>The sample rate of the model as stored in the file metadata</value>
        public int SampleRate
        {
            get { return AprilAsrPINVOKE.aam_get_sample_rate(Handle); }
        }
    }
}
