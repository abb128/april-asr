{
  description = "april-asr flake";
  inputs = {
    flake-utils.url = "github:numtide/flake-utils";
    nixpkgs.url = "github:nixos/nixpkgs/nixos-unstable";
    nixpkgs-onnxruntime114.url = "github:nixos/nixpkgs/c7d48290f9da479dcab26eac5db6299739c595c2";
  };
  outputs = {
    self,
    nixpkgs,
    nixpkgs-onnxruntime114,
    flake-utils,
  }:
    flake-utils.lib.eachDefaultSystem (
      system: let
        pkgs = import nixpkgs {
          inherit system;
          overlays = [
            # TODO: remove once https://github.com/NixOS/nixpkgs/pull/226734 is merged
            (
              final: prev: {
                onnxruntime = nixpkgs-onnxruntime114.legacyPackages.${prev.system}.onnxruntime;
              }
            )
          ];
        };
        inherit (pkgs) lib;
        inherit (pkgs.stdenv.hostPlatform) isDarwin;
      in {
        packages = rec {
          onnxruntime_1_14 =
            if isDarwin
            then pkgs.onnxruntime
            else
              (pkgs.onnxruntime.overrideAttrs
                (final: {
                  buildInputs = final.buildInputs ++ [pkgs.protobuf];
                }))
              .override {
                pythonSupport = false;
                useOneDNN = false;
              };
          april-asr = pkgs.stdenv.mkDerivation {
            src = ./.;
            name = "april-asr";

            nativeBuildInputs = with pkgs; [
              cmake
              pkg-config
            ];
            buildInputs = [onnxruntime_1_14];

            outputs = ["out" "dev"];

            meta = with lib; {
              description = "Speech-to-text library in C";
              homepage = "https://github.com/abb128/april-asr";
              license = licenses.gpl3;
            };
          };
          default = april-asr;
        };
        devShells.default = pkgs.mkShell {
          inherit (self.packages.${system}.default) buildInputs nativeBuildInputs;
          shellHook = ''
            export LD_LIBRARY_PATH=${self.packages.${system}.onnxruntime_1_14}/lib
          '';
        };
      }
    );
}
