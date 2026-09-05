#!/usr/bin/env bash
set -euo pipefail

source_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
kwin_version=$(kwin_wayland --version | awk 'NR == 1 { print $2 }')
cache_root=$(realpath -m "${XDG_CACHE_HOME:-"$HOME/.cache"}")
build_root=$cache_root/desktop-depth-preview
build_dir=$build_root/kwin-$kwin_version

cmake -S "$source_dir" -B "$build_dir" -G Ninja \
	-DBUILD_TESTING=ON \
	-DCMAKE_BUILD_TYPE=Release
cmake --build "$build_dir"
ctest --test-dir "$build_dir" --output-on-failure

plugin_path=$build_dir/bin/kwin/effects/plugins/desktop-depth-preview.so
[ -f "$plugin_path" ]
printf '%s\n' "$plugin_path"
