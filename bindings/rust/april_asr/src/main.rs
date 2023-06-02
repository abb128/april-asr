use april_asr::{Model, april_asr_init, Session};
use std::{io::Read, env};

pub fn usage() {
    println!("Usage: ./main <model> <wav>");
}

pub fn main() {
    april_asr_init();

    let args = env::args().collect::<Vec<String>>();
    let file_path = args.get(1).expect("Please provide a file path");
    let model_path = args.get(2).expect("Please provide a model path");

    let model = Model::new(model_path);
    let session = Session::new(model, april_asr::SessionFlags::ZERO_BIT, |result| {
        println!("Result: {:?}", result);
    });

    let mut buffer = Vec::new();
    let mut file = std::fs::File::open(file_path).unwrap();
    let _ = file.read(&mut buffer);
    session.feed_pcm16(buffer);
    session.flush();
}
