use april_asr::{april_asr_init, Model, Session};
use std::{env, io::Read};

pub fn main() {
    april_asr_init();

    let args = env::args().collect::<Vec<String>>();
    let file_path = args.get(1).expect("Usage: ./main <model> <wav>");
    let model_path = args.get(2).expect("Usage: ./main <model> <wav>");

    let model1 = Model::new(model_path).unwrap();
    let model2 = model1.clone();

    let session = Session::new(model1, april_asr::SessionFlags::ASYNC_REALTIME, |result| {
        println!("Session 1 Result: {:?}", result);
    });
    let mut buffer = Vec::new();
    let mut file = std::fs::File::open(file_path).unwrap();
    let _ = file.read(&mut buffer);
    session.feed_pcm16(buffer);

    let session2 = Session::new(model2, april_asr::SessionFlags::SYNC, |result| {
        println!("Session 2 Result: {:?}", result);
    });
    let mut buffer = Vec::new();
    let mut file = std::fs::File::open(file_path).unwrap();
    let _ = file.read(&mut buffer);
    session2.feed_pcm16(buffer);

    session.flush();
    session2.flush();
}
