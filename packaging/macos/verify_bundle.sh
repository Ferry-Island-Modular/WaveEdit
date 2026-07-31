#!/bin/bash
# Verify that WaveEdit.app is self-contained. Checks every Mach-O in the
# bundle, not just the executable: v1.3.0-beta.2 shipped a clean executable
# whose bundled libsndfile still hard-linked seven Homebrew dylibs.
set -euo pipefail

app="${1:?usage: verify_bundle.sh path/to/WaveEdit.app}"
macos_dir="$app/Contents/MacOS"
status=0

for file in "$macos_dir"/*; do
	if otool -L "$file" | grep -E '/(opt/homebrew|usr/local)/'; then
		echo "ERROR: $(basename "$file") references Homebrew or /usr/local paths (above)" >&2
		status=1
	fi
done

otool -L "$macos_dir/WaveEdit" | grep -qF '@executable_path/libSDL2-2.0.0.dylib' || {
	echo "ERROR: executable does not load the bundled libSDL2" >&2
	status=1
}

# sdl2-compat provides no SDL implementation itself; it dlopens libSDL3 at
# SDL_Init, which otool -L cannot see. v1.3.0-beta.2 shipped the compat shim
# with no SDL3 beside it and died on launch with "Failed loading SDL3 library".
if grep -q sdl2-compat "$macos_dir/libSDL2-2.0.0.dylib"; then
	if [ ! -f "$macos_dir/libSDL3.dylib" ]; then
		echo "ERROR: bundled libSDL2 is sdl2-compat but libSDL3.dylib is not bundled" >&2
		status=1
	fi
fi

exit $status
