const ffi = require('ffi-napi');
const ref = require('ref-napi');
const path = require('path');
const stream = require('stream');

// FFI

const { Callback } = require("ffi-napi");

const StructType = require('ref-struct-di')(ref);
const ArrayType = require('ref-array-di')(ref);

// Define types
const AprilASRModel = ref.refType(ref.types.void);
const AprilASRSession = ref.refType(ref.types.void);
const UInt8Array16 = ArrayType(ref.types.uint8, 16);
const AprilSpeakerID = StructType({
    data: UInt8Array16,
});
const AprilResultType = ref.types.int;
const AprilTokenFlagBits = ref.types.uint;
const AprilToken = StructType({
    token: ref.refType('CString'),
    logprob: ref.types.float,
    flags: AprilTokenFlagBits,
    time_ms: ref.types.size_t,
    reserved: ref.refType(ref.types.void),
});
const AprilTokenArray = ArrayType(AprilToken);
const AprilRecognitionResultHandler = ffi.Function(
    'void',
    [ref.refType(ref.types.void), AprilResultType, ref.types.size_t, ref.refType(AprilToken)]
);
const AprilConfig = StructType({
    speaker: AprilSpeakerID,
    handler: AprilRecognitionResultHandler,
    userdata: ref.refType(ref.types.void),
    flags: ref.types.uint,
});

const ext = process.platform === 'win32' ? 'dll' : 'so';

// Load the shared library
const libName = 'onnxruntime.' + ext;
const libAprilName = 'libaprilasr.' + ext;

const pathLib = process.env.ONNX_ROOT ? process.env.ONNX_ROOT : process.cwd();
const pathAprilLib = process.env.APRIL_ASR_ROOT ? process.env.APRIL_ASR_ROOT : process.cwd();

new ffi.DynamicLibrary(
  path.join(pathLib, libName),
  ffi.DynamicLibrary.FLAGS.RTLD_NOW | ffi.DynamicLibrary.FLAGS.RTLD_GLOBAL
);

const aprilLib = ffi.Library(
  new ffi.DynamicLibrary(
    path.join(pathAprilLib, libAprilName),
    ffi.DynamicLibrary.FLAGS.RTLD_NOW | ffi.DynamicLibrary.FLAGS.RTLD_GLOBAL
  ), {
    aam_api_init: ['void', ['int']],
    aam_create_model: [AprilASRModel, ['string']],
    aam_get_name: ['CString', [AprilASRModel]],
    aam_get_description: ['CString', [AprilASRModel]],
    aam_get_language: ['CString', [AprilASRModel]],
    aam_get_sample_rate: ['size_t', [AprilASRModel]],
    aam_free: ['void', [AprilASRModel]],
    aas_create_session: [AprilASRSession, [AprilASRModel, AprilConfig]],
    aas_feed_pcm16: ['void', [AprilASRSession, ref.refType(ref.types.short), 'size_t']],
    aas_flush: ['void', [AprilASRSession]],
    aas_realtime_get_speedup: ['float', [AprilASRSession]],
    aas_free: ['void', [AprilASRSession]],
});

// Classes

class Model {
  handle = null;

  /**
   * @param {string} path 
   */
  constructor(path) {
      const model = aprilLib.aam_create_model(path);

      if (model == null) {
          throw new Error("Failed to load model at given path");
      }

      this.handle = model;
  }
  
  /**
   * @returns {string}
   */
  getName() {
      return aprilLib.aam_get_name(this.handle);
  }

  /**
   * @returns {string}
   */
  getLanguage() {
      return aprilLib.aam_get_language(this.handle);
  }

  /**
   * @returns {string}
   */
  getDescription() {
      return aprilLib.aam_get_description(this.handle);
  }

  /**
   * @returns {number}
   */
  getSampleRate() {
      return aprilLib.aam_get_sample_rate(this.handle);
  }

}

class Session {

  /**
 * This callback is displayed as a global member.
 * @callback requestCallback
 * @param {number} result
 * @param {{token_str: string, logprob: number, flags: number, time_ms: number}[]} tokens
 */

  /**
   * @param {Model} model
   * @param {requestCallback} callback
   * @param {boolean} asynchronous
   * @param {boolean} no_rt
   * @param {string} speaker_name
   */
  constructor(model, callback, asynchronous = false, no_rt = false, speaker_name = '') {

    /** @var {AprilConfig} config */
    const config = new AprilConfig();
    config.flags = asynchronous && no_rt ? 2 : asynchronous ? 1 : 0;

    const nameHash = this._stringHash(speaker_name);
    config.speaker[0] = (nameHash >>> 24);
    config.speaker[1] = (nameHash >>> 16);
    config.speaker[2] = (nameHash >>> 8);
    config.speaker[3] = (nameHash);

    this.callback = callback;

    this._handler = new Callback(
      'void',
      [ref.refType('void'), AprilResultType, ref.types.size_t, ref.refType(AprilToken)],
      (_, result, count, tokensPtr) => {

        if (count === 0) {
          this.callback(result, []);
        }

        try {
          tokensPtr = ref.reinterpret(tokensPtr, AprilTokenArray.BYTES_PER_ELEMENT * count);

          const tokens = [];
          for (let i = 0; i < count; i++) {
            const tokenPtr = ref.reinterpret(tokensPtr, AprilTokenArray.BYTES_PER_ELEMENT, AprilTokenArray.BYTES_PER_ELEMENT * i);
            tokenPtr.type = AprilToken;

            const derefToken = tokenPtr.deref(AprilToken);

            const token_str = derefToken.token.readCString();
            const logprob = derefToken.logprob;
            const flags = derefToken.flags;
            const time_ms = derefToken.time_ms;

            tokens.push({token_str, logprob, flags, time_ms});
          }
          this.callback(result, tokens);
        }
        catch(err) {
          this.callback(result, []);
        }
      }
    );

    config.handler = this._handler;
    
    config.userdata = null;

    this.model = model;
    this._handle = aprilLib.aas_create_session(model.handle, config);
    if (this._handle.isNull()) {
      throw new Error('Session creation failed');
    }
  }

  /**
   * @returns {number} 
   */
  getRTSpeedup() {
    return aprilLib.aas_realtime_get_speedup(this._handle);
  }

  /**
   * @param {Buffer} data
   * @returns {void} 
   */
  feedPCM16(data) {
    aprilLib.aas_feed_pcm16(this._handle, data, data.length / 2);
  }

  /**
   * @returns {void} 
   */
  flush() {
    aprilLib.aas_flush(this._handle);
  }

  /**
   * @param {string} str
   * @returns {void} 
   */
  _stringHash(str) {
    let hash = 0;
    for (let i = 0; i < str.length; i++) {
      hash = (hash << 5) - hash + str.charCodeAt(i);
      hash |= 0; // Convert to 32-bit integer
    }
    return hash;
  }

  /**
   * @returns {void} 
   */
  close() {
    aprilLib.aas_free(this._handle);
    this.model = null;
    this._handle = null;
  }
}

class Chunker extends stream.Transform {
  constructor(chunkSize) {
    super();
    this.chunkSize = chunkSize;
    this.buffer = Buffer.alloc(0);
  }

  _transform(chunk, encoding, callback) {
    this.buffer = Buffer.concat([this.buffer, chunk]);
    while (this.buffer.length >= this.chunkSize) {
      const smallChunk = this.buffer.subarray(0, this.chunkSize);
      this.push(smallChunk);
      this.buffer = this.buffer.subarray(this.chunkSize);
    }
    callback();
  }

  _flush(callback) {
    if (this.buffer.length > 0) {
      this.push(this.buffer);
    }
    callback();
  }
}

const AprilAsrNative = {
  Model,
  Session,
  Chunker,
  init: aprilLib.aam_api_init
}

module.exports = {AprilAsrNative, Model, Session, Chunker, init: aprilLib.aam_api_init};