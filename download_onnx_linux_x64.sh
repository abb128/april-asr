#!/usr/bin/env bash
set -euo pipefail
project_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
version=${APRIL_ORT_VERSION:-1.30.0}
[[ "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || { echo "Invalid APRIL_ORT_VERSION" >&2; exit 1; }
if [[ -e "$project_dir/lib" ]]; then
    echo "lib already exists. Use ONNX_ROOT for another SDK, or move your existing lib directory first." >&2
    exit 1
fi
task_dir=$(mktemp -d)
trap 'rm -rf -- "$task_dir"' EXIT
name="onnxruntime-linux-x64-$version"
curl -fL --retry 3 "https://github.com/microsoft/onnxruntime/releases/download/v$version/$name.tgz" -o "$task_dir/sdk.tgz"
tar -xzf "$task_dir/sdk.tgz" -C "$task_dir"
mv -- "$task_dir/$name" "$project_dir/lib"
echo "Installed local ONNX Runtime SDK $version. Export ORT_DISABLE_TELEMETRY=1 before running."
