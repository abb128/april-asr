#![allow(non_upper_case_globals)]
#![allow(non_camel_case_types)]
#![allow(non_snake_case)]

use std::{ffi::CStr, mem};
mod bindgen_bindings;

pub fn april_asr_init() {
    unsafe {
        bindgen_bindings::aam_api_init(1);
    }
}

pub struct Model {
    internal_model: bindgen_bindings::AprilASRModel,
}

impl Model {
    pub fn new(file_path: &String) -> Model {
        let file_exists = std::path::Path::new(&file_path).exists();
        if !file_exists {
            panic!("File {} does not exist", file_path);
        }
        let internal_model = unsafe { bindgen_bindings::aam_create_model(file_path.as_ptr() as *const i8) };
        Model { internal_model }
    }

    pub fn get_name(&self) -> String {
        let char_ptr = unsafe { bindgen_bindings::aam_get_name(self.internal_model) };
        let c_str = unsafe { CStr::from_ptr(char_ptr) };

        c_str.to_str().unwrap().to_string()
    }

    pub fn get_description(&self) -> String {
        let char_ptr = unsafe { bindgen_bindings::aam_get_description(self.internal_model) };
        let c_str = unsafe { CStr::from_ptr(char_ptr) };

        c_str.to_str().unwrap().to_string()
    }

    pub fn get_language(&self) -> String {
        let char_ptr = unsafe { bindgen_bindings::aam_get_language(self.internal_model) };
        let c_str = unsafe { CStr::from_ptr(char_ptr) };

        c_str.to_str().unwrap().to_string()
    }

    pub fn get_sample_rate(&self) -> u32 {
        let sample: u32 = unsafe { bindgen_bindings::aam_get_sample_rate(self.internal_model) } as u32;

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

pub enum SessionFlags {
    ZERO_BIT = 0,
    ASYNC_REALTIME = 1,
    ASYNC_NON_REALTIME = 2,
}

impl From<SessionFlags> for bindgen_bindings::AprilConfigFlagBits {
    fn from(value: SessionFlags) -> Self {
        let n: u32 = value as u32;
        n
    }
}

#[derive(Debug)]
pub enum TokenFlags {
    WordBoundary,
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
            _ => panic!("Invalid token flag"),
        }
    }
}

#[derive(Debug)]
pub struct Token {
    pub token: String,
    pub logprob: f32,
    pub flags: TokenFlags,
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

#[derive(Debug)]
pub enum AprilResult {
    Unknown,
    CantKeepUp,
    PartialRecognition(Vec<Token>),
    FinalRecognition(Vec<Token>),
    Silence,
}

pub struct Session {
    internal_session: bindgen_bindings::AprilASRSession,
    // Hold onto these so it won't be freed
    _callback_func: fn(AprilResult),
    _config: bindgen_bindings::AprilConfig,
    _model: Model,
}

extern "C" fn handler_wrapper(
    userdata: *mut ::std::os::raw::c_void,
    result_type: bindgen_bindings::AprilResultType,
    count: usize,
    tokens: *const bindgen_bindings::AprilToken,
) {
    let result: AprilResult = match result_type {
        bindgen_bindings::AprilResultType_APRIL_RESULT_UNKNOWN => AprilResult::Unknown,
        bindgen_bindings::AprilResultType_APRIL_RESULT_ERROR_CANT_KEEP_UP => AprilResult::CantKeepUp,
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
    pub fn new(
        model: Model,
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

    pub fn flush(&self) {
        unsafe {
            bindgen_bindings::aas_flush(self.internal_session);
        }
    }

    pub fn aas_realtime_get_speedup(&self) -> f32 {
        unsafe { bindgen_bindings::aas_realtime_get_speedup(self.internal_session) }
    }

    pub fn feed_pcm16(&self, data: Vec<u8>) {
        let mut short_vec: Vec<i16> = data
            .chunks_exact(2)
            .map(|c| {
                i16::from_ne_bytes([c[0], c[1]]) })
            .collect();

        unsafe {
            bindgen_bindings::aas_feed_pcm16(self.internal_session, short_vec.as_mut_ptr(), short_vec.len());
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
