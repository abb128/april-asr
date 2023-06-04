extern crate bindgen;

use std::{
    env, fs,
    path::{PathBuf}, io,
};

fn main() {
    // Tell cargo to look for shared libraries in the specified directory
    copy_shared_objects().expect("Failed to copy shared objects to target directory");
    println!("cargo:rustc-link-lib=aprilasr");
    println!("cargo:rerun-if-changed=../../april_api.h");

    // The bindgen::Builder is the main entry point
    // to bindgen, and lets you build up options for
    // the resulting bindings.
    let bindings = bindgen::Builder::default()
        // The input header we would like to generate
        // bindings for.
        .header("../../april_api.h")
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

fn copy_shared_objects() -> io::Result<()> {
    let out_dir = PathBuf::from(env::var("OUT_DIR").unwrap());
    println!("cargo:rustc-link-search={}", out_dir.display());

    let libonnxfile = out_dir.join("libonnxruntime.so");
    let libaprilfile = out_dir.join("libaprilasr.so");
    let libapril2023file = out_dir.join("libaprilasr.so.2023");

    println!("cargo:rerun-if-changed=../../lib/lib/libonnxruntime.so");
    println!("cargo:rerun-if-changed=../../build/libaprilasr.so");
    println!("cargo:rerun-if-changed=../../build/libaprilasr.so.2023");

    fs::copy("../../lib/lib/libonnxruntime.so", libonnxfile)?;
    fs::copy("../../build/libaprilasr.so.2023", libaprilfile)?;
    fs::copy("../../build/libaprilasr.so", libapril2023file)?;
    Ok(())
}
