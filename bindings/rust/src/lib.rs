#![allow(non_upper_case_globals)]
#![allow(non_camel_case_types)]
#![allow(non_snake_case)]
#![allow(dead_code)]
#![warn(missing_docs)]

#![doc = include_str!("../README.md")]

use std::{ffi::CStr, mem, rc::Rc};
mod bindgen_bindings;

#[derive(Debug)]
/// Error Types for this crate
pub enum AprilError {
    /// The model could not be found.
    FileNotFound,
    /// The model file is not a valid APRIL-ASR model
    InvalidModel,
}

/// Initialize the April ASR library.
///
/// This function must be called before any other April ASR functions.
pub fn april_asr_init() {
    unsafe {
        bindgen_bindings::aam_api_init(1);
    }
}

/// The speach recognition model.
///
/// This struct is used to create a new [`Session`].
///
/// # Example
///
/// Basic usage:
/// ```
/// use april_asr::{april_asr_init, Model};
/// let model = Model::new("/path/to/model.april").unwrap();
/// println!("Model Name {}", model.get_name());
/// println!("Model Description {}", model.get_description());
/// println!("Model Language {}", model.get_language());
/// println!("Model Sample Rate {}", model.get_sample_rate());
/// ```
#[derive(Debug)]
pub struct Model {
    internal_model: bindgen_bindings::AprilASRModel,
}

impl Model {
    /// Create a new [`Model`] from a file path.
    ///
    /// Returns as a `Result<Rc<Model>, AprilError>` in the case where
    /// the model passed is invalid or not found.
    ///
    /// It uses a [`Rc`] so that the model can be shared multiple times across
    /// multiple sessions.
    ///
    /// # Example
    /// ```
    /// use april_asr::{april_asr_init, Model};
    /// let model = Model::new("/path/to/model.april").unwrap();
    /// ```
    pub fn new(file_path: &str) -> Result<Rc<Model>, AprilError> {
        let file_exists = std::path::Path::new(file_path).exists();
        if !file_exists {
            return Err(AprilError::FileNotFound);
        }
        let file_path = file_path.to_string();
        let internal_model =
            unsafe { bindgen_bindings::aam_create_model(file_path.as_ptr() as *const i8) };
        if internal_model.is_null() {
            return Err(AprilError::InvalidModel);
        }
        Ok(Rc::new(Model { internal_model }))
    }

    /// Get the name of the model.
    pub fn get_name(&self) -> &str {
        let char_ptr = unsafe { bindgen_bindings::aam_get_name(self.internal_model) };
        let c_str = unsafe { CStr::from_ptr(char_ptr) };

        c_str.to_str().unwrap()
    }

    /// Get the description of the model.
    pub fn get_description(&self) -> &str {
        let char_ptr = unsafe { bindgen_bindings::aam_get_description(self.internal_model) };
        let c_str = unsafe { CStr::from_ptr(char_ptr) };

        c_str.to_str().unwrap()
    }

    /// Get the language of the model.
    pub fn get_language(&self) -> &str {
        let char_ptr = unsafe { bindgen_bindings::aam_get_language(self.internal_model) };
        let c_str = unsafe { CStr::from_ptr(char_ptr) };

        c_str.to_str().unwrap()
    }

    /// Get the sample rate of model in Hz. For example, may return 16000
    pub fn get_sample_rate(&self) -> u32 {
        let sample: u32 =
            unsafe { bindgen_bindings::aam_get_sample_rate(self.internal_model) } as u32;

        sample
    }
}

impl Drop for Model {
    fn drop(&mut self) {
        unsafe {
            bindgen_bindings::aam_free(self.internal_model);
        }
    }
}

/// Flags for how the [`Session`] should behave.
pub enum SessionFlags {
    /// When called it is assumed that the audio is real-time and the session will
    /// return results as soon as possible.
    SYNC = 0,

    /// If set, the input audio should be fed in realtime (1 second of
    /// audio per second) in small chunks. Calls to [`Session::feed_pcm16`] and
    /// [`Session::flush`] will be fast as it will delegate processing to a background
    /// thread. The handler will be called from the background thread at some
    /// point later. The accuracy may be degraded depending on the system
    /// hardware. You may get an accuracy estimate by calling
    /// [`Session::realtime_get_speedup`].
    ASYNC_REALTIME = 1,

    /// Similar to [`SessionFlags::ASYNC_REALTIME`], but does not degrade accuracy depending on system
    /// hardware. However, if the system is not fast enough to process audio,
    /// the background thread will fall behind, results may become unusable,
    /// and the handler will be called with [`AprilResult::CantKeepUp`].
    ASYNC_NON_REALTIME = 2,
}

impl From<SessionFlags> for bindgen_bindings::AprilConfigFlagBits {
    fn from(value: SessionFlags) -> Self {
        let n: u32 = value as u32;
        n
    }
}

#[derive(Debug)]
/// Flags for what type of token is being returned.
pub enum TokenFlags {
    /// If set, this token marks the start of a new word.
    WordBoundary,
    /// If set, this token marks the end of a sentence, meaning the token is
    /// equal to ".", "!", or "?". Some models may not have this token.
    SentenceEnd,
}

impl From<bindgen_bindings::AprilTokenFlagBits> for TokenFlags {
    fn from(value: bindgen_bindings::AprilTokenFlagBits) -> Self {
        match value {
            bindgen_bindings::AprilTokenFlagBits_APRIL_TOKEN_FLAG_WORD_BOUNDARY_BIT => {
                TokenFlags::WordBoundary
            }
            bindgen_bindings::AprilTokenFlagBits_APRIL_TOKEN_FLAG_SENTENCE_END_BIT => {
                TokenFlags::SentenceEnd
            }
            _ => panic!("Invalid token flag when parsing token, this should never happen"),
        }
    }
}

#[derive(Debug)]
/// A token is a single word or punctuation mark that is returned from the model
pub struct Token {
    /// Null-terminated string. The string contains its own formatting,
    /// for example it may start with a space to denote a new word, or
    /// not start with a space to denote the next part of a word.
    pub token: String,
    /// Log probability of this being the correct token.
    pub logprob: f32,
    /// Flags for this token. See [`TokenFlags`] for more information.
    pub flags: TokenFlags,
    /// The milisecond at which this was emitted. Counting is based on how much
    /// audio is being fed (time is not advanced when the session is not being
    /// given audio)
    pub time_ms: usize,
}

impl From<bindgen_bindings::AprilToken> for Token {
    fn from(token: bindgen_bindings::AprilToken) -> Self {
        let cstr = unsafe { CStr::from_ptr(token.token) };
        Token {
            token: cstr.to_str().unwrap().to_string(),
            logprob: token.logprob,
            flags: token.flags.into(),
            time_ms: token.time_ms,
        }
    }
}

/// The result of a completion passed into handler function.
#[derive(Debug)]
pub enum AprilResult {
    /// Undefined result.
    Unknown,
    /// If in non-synchronous mode, this may be called when the internal
    /// audio buffer is full and processing can't keep up.
    CantKeepUp,
    /// Specifies that the result is only partial.
    /// A future call will contain much of the same text but updated.
    PartialRecognition(Vec<Token>),
    /// Specifies that the result is final.
    /// Future calls will start from empty and will not contain any of the given [`Token`]
    FinalRecognition(Vec<Token>),
    /// Specifies there has been some silence.
    /// Will not be called repeatedly
    Silence,
}

/// A session is a single instance of a speech recognition engine.
///
/// It is created from a [`Model`] and can be used to perform speech recognition.
///
/// # Example
///
/// Reading data from a file and performing speech recognition on it.
/// ```
/// use april_asr::{Model, Session, SessionFlags, AprilResult};
/// use std::io::Read;
/// let model = Model::new("/path/to/model.april").unwrap();
/// let session = Session::new(&model, SessionFlags::SYNC, |result: AprilResult| {
///    println!("Result: {:?}", result);
///    // Do something with the results
/// });
///
/// let mut buffer = Vec::new();
/// let mut file = std::fs::File::open("/path/to/file.wav").unwrap();
/// let _ = file.read(&mut buffer);
/// session.feed_pcm16(&[0; 1024]);
/// ```
/// [`Model`]: struct.Model.html
/// [`SessionFlags`]: enum.SessionFlags.html
#[derive(Debug)]
pub struct Session {
    internal_session: bindgen_bindings::AprilASRSession,
    // Hold onto these so it won't be freed
    _callback_func: fn(AprilResult),
    _config: bindgen_bindings::AprilConfig,
    _model: Rc<Model>,
}

extern "C" fn handler_wrapper(
    userdata: *mut ::std::os::raw::c_void,
    result_type: bindgen_bindings::AprilResultType,
    count: usize,
    tokens: *const bindgen_bindings::AprilToken,
) {
    let result: AprilResult = match result_type {
        bindgen_bindings::AprilResultType_APRIL_RESULT_UNKNOWN => AprilResult::Unknown,
        bindgen_bindings::AprilResultType_APRIL_RESULT_ERROR_CANT_KEEP_UP => {
            AprilResult::CantKeepUp
        }
        bindgen_bindings::AprilResultType_APRIL_RESULT_RECOGNITION_PARTIAL => {
            let tokens = unsafe { std::slice::from_raw_parts(tokens, count) };
            let tokens: Vec<Token> = tokens.iter().map(|t| (*t).into()).collect();
            AprilResult::PartialRecognition(tokens)
        }
        bindgen_bindings::AprilResultType_APRIL_RESULT_RECOGNITION_FINAL => {
            let tokens = unsafe { std::slice::from_raw_parts(tokens, count) };
            let tokens: Vec<Token> = tokens.iter().map(|t| (*t).into()).collect();
            AprilResult::FinalRecognition(tokens)
        }
        bindgen_bindings::AprilResultType_APRIL_RESULT_SILENCE => AprilResult::Silence,
        _ => unreachable!(),
    };

    let callback: fn(result: AprilResult) -> () = unsafe { mem::transmute(userdata) };
    callback(result);
}

impl Session {
    /// Creates a new session
    /// from a [`Model`] and [`SessionFlags`].
    ///
    /// # Arguments
    /// * `model` - The [`Model`] to use for this session.
    /// * `flags` - The [`SessionFlags`] to use for this session.
    /// * `handler` - The callback function to use for this session. This will be called
    ///               when the session has a completion from calling [`Session::feed_pcm16`].
    ///               The callback function will be passed a [`AprilResult`] which will contains
    ///               the results of the speech recognition.
    ///
    /// # Example:
    ///
    /// ```
    /// use april_asr::{Model, Session, SessionFlags, AprilResult};
    /// let model = Model::new("/path/to/model.april").unwrap();
    /// let session = Session::new(&model, SessionFlags::SYNC, |result: AprilResult| {
    ///    println!("Result: {:?}", result);
    ///    // Do something with the results
    /// });
    /// ```
    ///
    /// [`Model`]: struct.Model.html
    /// [`SessionFlags`]: enum.SessionFlags.html
    pub fn new(
        model: Rc<Model>,
        flags: SessionFlags,
        handler: fn(result: AprilResult) -> (),
    ) -> Session {
        let config = bindgen_bindings::AprilConfig {
            speaker: bindgen_bindings::AprilSpeakerID { data: [0; 16usize] },
            handler: Some(handler_wrapper),
            userdata: handler as *mut ::std::os::raw::c_void,
            flags: flags.into(),
        };

        let internal_session =
            unsafe { bindgen_bindings::aas_create_session(model.internal_model, config) };

        Session {
            internal_session,
            _callback_func: handler,
            _config: config,
            _model: model,
        }
    }

    /// Processes any unprocessed samples and produces a final result.
    pub fn flush(&self) {
        unsafe {
            bindgen_bindings::aas_flush(self.internal_session);
        }
    }

    /// If [`SessionFlags::ASYNC_REALTIME`] is set, this may return a number describing
    /// how much audio is being sped up to keep up with realtime. If the number is
    /// below 1.0, audio is not being sped up. If greater than 1.0, the audio is
    /// being sped up and the accuracy may be reduced.
    pub fn realtime_get_speedup(&self) -> f32 {
        unsafe { bindgen_bindings::aas_realtime_get_speedup(self.internal_session) }
    }

    /// Feed PCM16 audio data to the session, must be single-channel and sampled
    /// to the sample rate given in [`Model::get_sample_rate`].
    pub fn feed_pcm16(&self, data: Vec<u8>) {
        let mut short_vec: Vec<i16> = data
            .chunks_exact(2)
            .map(|c| i16::from_ne_bytes([c[0], c[1]]))
            .collect();

        unsafe {
            bindgen_bindings::aas_feed_pcm16(
                self.internal_session,
                short_vec.as_mut_ptr(),
                short_vec.len(),
            );
        }
    }
}

impl Drop for Session {
    fn drop(&mut self) {
        unsafe {
            bindgen_bindings::aas_free(self.internal_session);
        }
    }
}
