# april_asr
This crate is a (safe) wrapper around [April ASR](https://github.com/abb128/april-asr) through its C API.
From its [GitHub page](https://github.com/abb128/april-asr):
 * April-ASR is a minimal library that provides an API for offline streaming speech-to-text applications

## Usage

Reading a WAV file and feeding it to the APRIL-ASR model:

```
use april_asr::{Model, Session, Result, AprilResult};
use std::io::Read;

april_asr::april_asr_init();

let model = Model::new("/path/to/model.april").unwrap();

let session = Session::new(model, april_asr::SessionFlags::SYNC, |result: AprilResult| {
    println!("Result: {:?}", result);
});
let mut buffer = Vec::new();
let mut file = std::fs::File::open("/path/to/file.wav").unwrap();
let _ = file.read(&mut buffer);
session.feed_pcm16(buffer);

session.flush();
```
