use april_asr::{april_asr_init, Model, Session};
use std::{env, io::Read};

pub fn main() {
    april_asr_init();

    let args = env::args().collect::<Vec<String>>();
    let file_path = args.get(1).expect("Usage: ./main <model> <wav>");
    let model_path = args.get(2).expect("Usage: ./main <model> <wav>");

    let model = Model::new(model_path).unwrap();

    let session = Session::new(model, april_asr::SessionFlags::SYNC, |result| {
        println!("Result: {:?}", result);
    });
    let mut buffer = Vec::new();
    let mut file = std::fs::File::open(file_path).unwrap();
    let _ = file.read(&mut buffer);
    session.feed_pcm16(buffer);

    session.flush();
}
