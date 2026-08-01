#!/bin/bash
# Copy the full closure of non-system dylibs the executable depends on into
# Contents/MacOS and rewrite load commands so the bundle is self-contained.
# Inter-dylib references use @loader_path (not @executable_path) so each
# bundled dylib also resolves when loaded outside the app, e.g. by the
# dlopen smoke test in CI.
set -euo pipefail

app="${1:?usage: bundle_dylibs.sh path/to/WaveEdit.app}"
macos_dir="$app/Contents/MacOS"
exe="$macos_dir/WaveEdit"
pattern='/(opt/homebrew|usr/local)/'

deps_of() {
	# Skip otool's header line and, for dylibs, the install-name ID line
	# (the ID is rewritten before this is called, so it never matches).
	otool -L "$1" | tail -n +2 | awk '{print $1}' | grep -E "$pattern" || true
}

bundle_lib() {
	local src="$1" name="$2"
	cp "$(readlink -f "$src")" "$macos_dir/$name"
	chmod u+w "$macos_dir/$name"
	install_name_tool -id "@loader_path/$name" "$macos_dir/$name" 2> /dev/null
	queue+=("$macos_dir/$name")
}

drain_queue() {
	while [ ${#queue[@]} -gt 0 ]; do
		local current="${queue[0]}" prefix="@loader_path"
		queue=("${queue[@]:1}")
		[ "$current" = "$exe" ] && prefix="@executable_path"
		for dep in $(deps_of "$current"); do
			local name="$(basename "$dep")"
			[ -f "$macos_dir/$name" ] || bundle_lib "$dep" "$name"
			install_name_tool -change "$dep" "$prefix/$name" "$current" 2> /dev/null
		done
	done
}

queue=("$exe")
drain_queue

# sdl2-compat has no SDL implementation of its own: it dlopens libSDL3 at the
# first SDL call. That dependency is invisible to otool, so bundle SDL3
# explicitly whenever the "SDL2" we shipped is actually the compat shim.
# @loader_path/libSDL3.dylib is the first path the shim probes.
if grep -q sdl2-compat "$macos_dir/libSDL2-2.0.0.dylib"; then
	bundle_lib "$(pkg-config --variable=libdir sdl3)/libSDL3.0.dylib" libSDL3.dylib
	drain_queue
fi

# install_name_tool invalidates each dylib's ad-hoc signature; re-sign them
# individually (the caller re-signs the whole bundle afterwards).
find "$macos_dir" -name '*.dylib' -exec codesign --force --sign - {} +
