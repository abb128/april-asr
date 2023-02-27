# General Concepts

Before writing any code, it's recommended to understand these concepts. These apply to all of the language bindings.

## Model

Models end with the file extension `.april`. You can load these files using the AprilASR API.

Each model has its own sample rate in which it expects audio. There is a method to get the expected sample rate. Usually, this is 16000 Hz.

Models also have additional metadata such as name, description, language.

After loading a model, you can create one or more sessions that use the model. By re-using the same model instance across sessions, you can reduce memory use.

## Session

You should create one session for each audio stream. Typically you have one audio stream. However, if you have isolated audio streams of multiple speakers, you can create sessions to perform speech recognition on each one separately.

<!--(This feature is not yet implemented) When creating a session, you may specify a speaker ID or name. If not empty, this may be used to save and restore the hidden state, to help initialize the session in a way it can provide more accurate results instantly.-->

### Async vs Non-Async

By default, sessions are synchronous. This means that when you call the function to feed audio, it will block and slow down the calling thread. The handler will also be called synchronously from the same thread, before the feed function returns.

On the other hand, an asynchronous session does not block the calling thread. Calls to feed audio are quick, and the handler gets called at some point from a different thread.

In an asynchronous session, you should feed audio at a rate that comes out to 1 second per second. You should not feed multiple seconds or minutes at once, the internal buffer cannot fit more than a few seconds. Asynchronous sessions are intended for streaming audio as it comes in. If you feed more than 1 second every second, you will get poor results (if any).

### Async-Realtime vs Async-Non-Realtime

In an asynchronous session, there is a problem that the system may not be fast enough to process audio at the rate that it's coming in. This is where realtime and non-realtime sessions differ in behavior.

A realtime session will work around this by speeding up incoming audio to a rate where the system can keep up. This involves some audio processing code, which may or may not be desirable.

Speeding up audio too much will also typically reduce accuracy. There is a method you can call to get the current speedup value so you can tell when it happens. (`GetRTSpeedup`, `get_rt_speedup`, `aas_realtime_get_speedup`)

A non-realtime session ignores this problem and assumes the system is fast enough. If this is not the case, the results will fall behind, the internal buffer will start to get full, `ErrorCantKeepUp` result will be called, and the results may be horrible.

## Handler

A session needs a handler function to get results. The handler gets called by the session whenever it has new results. The typical parameters are the result type and the token array.

Note that in an asynchronous session, the handler will be called from a different thread.

You should try to make your handler function fast to avoid slowing down the session.

The actual text can be extracted from the token array.

### Result Type

The handler gets called with an enum explaining the result type:
* Partial Recognition - the token array is a partial result and may change in the next call
* Final Recognition - the token array is final, the next call will start from an empty array
* Error Can't Keep Up - called in an asynchronous non-realtime session if the system is not fast enough, may also be called briefly in an asynchronous realtime session, the token array is empty or null
* Silence - there has been silence, the token array is empty or null

## Token

A token may be a single letter, a word chunk, an entire word, punctuation, or other arbitrary set of characters.

To convert a token array to a string, simply concatenate the strings from each token. You don't need to add spaces between tokens, the tokens contain their own formatting.

For example, the text `"THAT'S COOL ELEPHANTS"` may be represented as tokens like so:
* `[" THAT", "'", "S", " CO", "OL", " E", "LE", "P", "H", "ANT", "S"]`
* Concatenating these strings will give you the correct `" THAT'S COOL ELEPHANTS"`, but with an extra space at the beginning. You may want to strip the final string to avoid the extra space.

Tokens contain more data than just the string however. They also contain the log probability, and a boolean denoting whether or not it's a word boundary. In English, the word boundary value is equivalent to checking if the first character is a space.

## Dependencies

AprilASR depends on ONNXRuntime for ML inference. You will need both libraries for it to work:
* Linux: `libaprilasr.so` and `libonnxruntime.so`
* Windows: `libaprilasr.dll` and `onnxruntime.dll`