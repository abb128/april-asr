extern crate bindgen;

use std::{
    env, fs,
    io::{self, Write},
    path::{Path, PathBuf},
};

const APRIL_RELEASE_URL: &str = "https://github.com/arguflow/april-asr/releases/download/v-0.0.1/libaprilasr.so";
const ONNX_RELEASE_URL: &str = "https://github.com/microsoft/onnxruntime/releases/download/v1.13.1/onnxruntime-linux-x64-1.13.1.tgz";

fn main() {
    // Tell cargo to look for shared libraries in the specified directory
    copy_shared_objects().expect("Failed to copy shared objects to target directory");
    println!("cargo:rustc-link-lib=aprilasr");
    println!("cargo:rerun-if-changed=april_api.h");

    // The bindgen::Builder is the main entry point
    // to bindgen, and lets you build up options for
    // the resulting bindings.
    let bindings = bindgen::Builder::default()
        // The input header we would like to generate
        // bindings for.
        .header("april_api.h")
        // Tell cargo to invalidate the built crate whenever any of the
        // included header files changed.
        .parse_callbacks(Box::new(bindgen::CargoCallbacks))
        // Finish the builder and generate the bindings.
        .generate()
        // Unwrap the Result and panic on failure.
        .expect("Unable to generate bindings");

    // Write the bindings to the $OUT_DIR/bindings.rs file.
    let out_path = PathBuf::from("./src/bindgen_bindings.rs");
    bindings
        .write_to_file(out_path)
        .expect("Couldn't write bindings!");
}

fn download_file<P>(url: String, target_file: &P)
where
    P: AsRef<Path>,
{
    let resp = ureq::get(&url)
        .timeout(std::time::Duration::from_secs(300))
        .call()
        .unwrap_or_else(|err| panic!("ERROR: Failed to download {}: {:?}", url, err));

    let len = resp
        .header("Content-Length")
        .and_then(|s| s.parse::<usize>().ok())
        .unwrap();
    let mut reader = resp.into_reader();
    // FIXME: Save directly to the file
    let mut buffer = vec![];
    let read_len = reader.read_to_end(&mut buffer).unwrap();
    assert_eq!(buffer.len(), len);
    assert_eq!(buffer.len(), read_len);

    let f = fs::File::create(target_file).unwrap();
    let mut writer = io::BufWriter::new(f);
    writer.write_all(&buffer).unwrap();
}

fn copy_shared_objects() -> io::Result<()> {
    let out_dir = PathBuf::from(env::var("OUT_DIR").unwrap());
    println!("cargo:rustc-link-search={}", out_dir.display());

    let libonnxfilezip = out_dir.join("libonnxruntime.tgz");
    let libaprilfile = out_dir.join("libaprilasr.so");
    let libapril2023file = out_dir.join("libaprilasr.so.2023");

    if libonnxfilezip.as_path().exists()
        && libaprilfile.as_path().exists()
        && libapril2023file.as_path().exists()
    {
        return Ok(());
    }

    download_file(
        ONNX_RELEASE_URL.to_string(),
        &libonnxfilezip,
    );
    extract_tgz(&libonnxfilezip, out_dir.as_path());
    download_file(
        APRIL_RELEASE_URL.to_string(),
        &libaprilfile,
    );
    fs::copy(libaprilfile, libapril2023file)?;

    Ok(())
}

fn extract_tgz(filename: &Path, output: &Path) {
    let file = fs::File::open(filename).unwrap();
    let buf = io::BufReader::new(file);
    let tar = flate2::read::GzDecoder::new(buf);
    let mut archive = tar::Archive::new(tar);
    archive.unpack(output).unwrap();
}

