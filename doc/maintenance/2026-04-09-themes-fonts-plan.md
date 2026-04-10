# Theme System + Font Modernization Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace WaveEdit's hardcoded `styleId 0..3` theme blocks with a runtime base16 YAML loader, ship 8 curated built-in themes, upgrade font rendering to FreeType, ship Inter (UI) + JetBrains Mono (numerics) fonts, delete the legacy Lekton fonts, and apply the mono font to every `SliderFloat` numeric readout.

**Architecture:** A new self-contained `theme` module (`src/theme.hpp` + `src/theme.cpp`) loads themes from disk at startup and applies them to imgui's color table on demand. `ui.cpp` becomes a consumer that calls `themeInit()` once and `themeApply(id)` from a menu handler. The hardcoded ~258-line `refreshStyle()` function in `ui.cpp` is deleted. FreeType is enabled via `IMGUI_ENABLE_FREETYPE` in `imconfig_user.h` and added as a runtime dependency on all three platforms. Iterative development on a feature branch `themes-and-fonts` with the same commit-and-verify pattern as the submodule upgrade plan.

**Tech Stack:** C++11, Dear ImGui v1.92.7, FreeType (system package), `<dirent.h>` POSIX directory walking (matches `catalog.cpp`'s pattern; works on macOS/Linux/MinGW), base16 YAML format (hand-rolled parser), Inter and JetBrains Mono TrueType fonts (SIL OFL).

**Predecessor:** `2026-04-09-themes-fonts-design.md` (the approved design spec)
**Depends on:** Submodule upgrade sub-project (satisfied 2026-04-09 — modern imgui v1.92.7 is in place, which provides the FreeType integration)

---

## Important notes for the implementing engineer

1. **Verification is mac-only.** The user's daily driver is macOS arm64; CI is paused pending hosting decisions. Linux and Windows builds are unverified until CI lands. The build steps in this plan are written for macOS but the underlying changes are portable; cross-platform issues will surface later.
2. **Each task that produces a green build commits separately.** Don't batch up multiple tasks into one commit. Each commit should be a clear "we added X and the build still works."
3. **The mapping table in `theme.cpp` (Task 5) is a first pass.** It will almost certainly need tweaking after Task 8 (interactive validation). Plan for an iteration loop, not a one-shot. Task 8 explicitly budgets time for this.
4. **Don't refactor `editorBehavior()` or any custom widgets.** Stay focused on the theme/font scope. Even if you see something tempting in `widgets.cpp`, leave it.
5. **Don't write unit tests** for the parser or mapping. WaveEdit has no test infrastructure and adding one is a separate sub-project. Verification is interactive.

## Verification commands reference

**`VERIFY_BUILD`:**
```bash
cd /Users/jgoney/dev/WaveEdit_clones/WaveEdit
make clean
make 2>&1 | tee /tmp/waveedit-build.log | tail -10
ls -la WaveEdit && file WaveEdit
```
Expected: clean compile, no error: lines, `WaveEdit` binary present as `Mach-O 64-bit executable arm64`.

**`VERIFY_LAUNCH`:**
```bash
./WaveEdit &
PID=$!
sleep 3
if kill -0 $PID 2>/dev/null; then
  echo "PASS: still running"
  kill $PID
  wait $PID 2>/dev/null
else
  echo "FAIL: exited within 3s"
  exit 1
fi
```
Expected: `PASS: still running`. Used as a smoke check after each task.

**`VERIFY_INTERACTIVE` (Tasks 8-9 only, manual):** described in detail in Task 8.

---

## Task 0: Create feature branch and baseline verification

**Files:** none (git operations only)

- [ ] **Step 0.1: Confirm clean working tree**

```bash
cd /Users/jgoney/dev/WaveEdit_clones/WaveEdit
git status
```

Expected: no changes to tracked files. Untracked build artifacts (`.vscode/`, `CLAUDE.md`, `WaveEdit`, `autosave.dat`, `dep/include/`, `dep/lib/`, `ui.dat`, `.superpowers/`) are fine.

- [ ] **Step 0.2: Confirm we're on `m1-modernization`**

```bash
git branch --show-current
```
Expected: `m1-modernization`. If not, `git checkout m1-modernization`.

- [ ] **Step 0.3: Create the feature branch**

```bash
git checkout -b themes-and-fonts
```

- [ ] **Step 0.4: Run `VERIFY_BUILD`**

Expected: clean build, binary present. Confirms baseline works before we start.

- [ ] **Step 0.5: Run `VERIFY_LAUNCH`**

Expected: `PASS: still running`.

No commit for this task.

---

## Task 1: Add libfreetype as a Homebrew dependency

**Files:**
- Modify: `dep/Makefile:25-34` (extend `homebrew-deps` target)

- [ ] **Step 1.1: Read the current `homebrew-deps` target**

Look at `dep/Makefile:25-34`. Current content:

```make
homebrew-deps:
	@echo "Using Homebrew dependencies on macOS..."
	@which brew > /dev/null || (echo "Error: Homebrew not found. Please install Homebrew first." && exit 1)
	brew install sdl2 libsndfile libsamplerate
	@echo "Creating symlinks for compatibility..."
	@mkdir -p lib include
	@ln -sf $$(brew --prefix)/lib/libSDL2-2.0.0.dylib lib/ || true
	@ln -sf $$(brew --prefix)/lib/libsndfile.1.dylib lib/ || true
	@ln -sf $$(brew --prefix)/lib/libsamplerate.0.dylib lib/ || true
	@ln -sf $$(brew --prefix)/include/SDL2 include/ || true
```

- [ ] **Step 1.2: Add `freetype` to the brew install line and add a freetype symlink**

Use the Edit tool to change:

```make
	brew install sdl2 libsndfile libsamplerate
```

to:

```make
	brew install sdl2 libsndfile libsamplerate freetype
```

And add this line to the symlink block (after the existing `ln -sf` lines, before the closing fi/done):

```make
	@ln -sf $$(brew --prefix freetype)/lib/libfreetype.6.dylib lib/ || true
```

The full symlink block should now be:

```make
	@ln -sf $$(brew --prefix)/lib/libSDL2-2.0.0.dylib lib/ || true
	@ln -sf $$(brew --prefix)/lib/libsndfile.1.dylib lib/ || true
	@ln -sf $$(brew --prefix)/lib/libsamplerate.0.dylib lib/ || true
	@ln -sf $$(brew --prefix freetype)/lib/libfreetype.6.dylib lib/ || true
	@ln -sf $$(brew --prefix)/include/SDL2 include/ || true
```

- [ ] **Step 1.3: Install freetype locally**

```bash
brew install freetype
```

Expected: either "freetype is already installed" or a fresh install. Either is fine.

- [ ] **Step 1.4: Verify freetype headers are reachable**

```bash
ls $(brew --prefix freetype)/include/freetype2/freetype/freetype.h
pkg-config --cflags freetype2 2>&1 || echo "(pkg-config may not be set up; we'll use brew --prefix in Makefile)"
```

Expected: the `freetype.h` file path is printed. The `pkg-config` line is informational — if pkg-config isn't installed, the Makefile will use `brew --prefix freetype` instead, which is the path we wired in Step 1.2.

- [ ] **Step 1.5: Re-run `VERIFY_BUILD`**

Expected: clean build. The Makefile hasn't been changed yet, but we're verifying that adding freetype to the brew install didn't break anything.

- [ ] **Step 1.6: Commit**

```bash
git add dep/Makefile
git commit -m "dep/Makefile: add libfreetype to homebrew-deps

The themes-and-fonts sub-project enables IMGUI_ENABLE_FREETYPE
to switch imgui's font atlas builder from stb_truetype to
FreeType, for noticeably sharper rasterization. This commit
adds libfreetype to the macOS homebrew-deps install list and
creates a symlink in dep/lib/ for consistency with the other
brew-managed libraries.

Linux and Windows continue to expect freetype as a system
package (apt: libfreetype6-dev; MSYS2: mingw-w64-x86_64-freetype).
Will be added to the CI workflow when that sub-project resumes."
```

---

## Task 2: Vendor Inter and JetBrains Mono fonts; delete Lekton

**Files:**
- Create: `fonts/Inter-Regular.ttf` (~310 KB)
- Create: `fonts/JetBrainsMono-Regular.ttf` (~190 KB)
- Delete: `fonts/Lekton-Regular.ttf`
- Delete: `fonts/Lekton-Bold.ttf`
- Delete: `fonts/Lekton-Italic.ttf`
- Modify: `fonts/SIL Open Font License.txt` (rewrite to cover Inter + JBM only)

- [ ] **Step 2.1: Identify the current Lekton files**

```bash
ls fonts/
```

Expected output (the relevant lines):
```
Lekton-Bold.ttf
Lekton-Italic.ttf
Lekton-Regular.ttf
Montserrat-Bold.ttf  (maybe — keep if present)
SIL Open Font License.txt
```

Note: `Montserrat-Bold.ttf` may or may not be present. If it is, leave it alone — only Lekton is being removed.

- [ ] **Step 2.2: Download Inter Regular**

Inter is hosted at github.com/rsms/inter. The simplest source for the single Regular weight TTF is the official release.

```bash
TMPDIR=$(mktemp -d)
cd $TMPDIR
curl -L -o Inter-4.1.zip https://github.com/rsms/inter/releases/download/v4.1/Inter-4.1.zip
unzip -q Inter-4.1.zip
ls "Inter Desktop"/*.ttf | head -5
```

Expected: a list of `.ttf` files including `Inter-Regular.ttf`. The exact path inside the zip may vary by Inter version (older versions used `Inter-Hinted/`); check the listing first.

If the version 4.1 URL 404s, browse https://github.com/rsms/inter/releases to find the latest stable release and substitute its zip URL. The Inter project occasionally renames its release artifacts.

- [ ] **Step 2.3: Copy Inter-Regular.ttf into `fonts/`**

```bash
cp "$TMPDIR/Inter Desktop/Inter-Regular.ttf" /Users/jgoney/dev/WaveEdit_clones/WaveEdit/fonts/Inter-Regular.ttf
ls -la /Users/jgoney/dev/WaveEdit_clones/WaveEdit/fonts/Inter-Regular.ttf
```

Expected: a file ~300-330 KB. If the path inside the zip is different (e.g., `Inter-Hinted/Inter-Regular.ttf` in older versions), adjust accordingly.

- [ ] **Step 2.4: Download JetBrains Mono Regular**

```bash
cd $TMPDIR
curl -L -o JetBrainsMono-2.304.zip https://github.com/JetBrains/JetBrainsMono/releases/download/v2.304/JetBrainsMono-2.304.zip
unzip -q JetBrainsMono-2.304.zip
ls fonts/ttf/JetBrainsMono-Regular.ttf
```

Expected: the file path printed. JetBrains Mono's release zip contains a `fonts/ttf/` subdirectory with all weights.

If v2.304 has been superseded, browse https://github.com/JetBrains/JetBrainsMono/releases for the latest stable release.

- [ ] **Step 2.5: Copy JetBrainsMono-Regular.ttf into `fonts/`**

```bash
cp $TMPDIR/fonts/ttf/JetBrainsMono-Regular.ttf /Users/jgoney/dev/WaveEdit_clones/WaveEdit/fonts/JetBrainsMono-Regular.ttf
ls -la /Users/jgoney/dev/WaveEdit_clones/WaveEdit/fonts/JetBrainsMono-Regular.ttf
```

Expected: a file ~180-200 KB.

- [ ] **Step 2.6: Clean up the temp dir**

```bash
rm -rf $TMPDIR
```

- [ ] **Step 2.7: Delete the Lekton files**

```bash
cd /Users/jgoney/dev/WaveEdit_clones/WaveEdit
rm fonts/Lekton-Regular.ttf fonts/Lekton-Bold.ttf fonts/Lekton-Italic.ttf
ls fonts/
```

Expected: Lekton files gone. `Inter-Regular.ttf` and `JetBrainsMono-Regular.ttf` present. License file still present.

- [ ] **Step 2.8: Rewrite the SIL OFL file to cover Inter + JBM**

The current `fonts/SIL Open Font License.txt` covers Lekton. We need to replace it with a version that covers Inter and JetBrains Mono.

The SIL OFL 1.1 license text is the same regardless of which fonts it covers — only the copyright header lines differ. Use the Write tool to write `fonts/SIL Open Font License.txt` with this content:

```
WaveEdit ships the following fonts under the SIL Open Font License 1.1.

Inter
  Copyright (c) 2016-present, Rasmus Andersson <https://rsms.me/>
  Source: https://github.com/rsms/inter

JetBrains Mono
  Copyright 2020 The JetBrains Mono Project Authors
    (https://github.com/JetBrains/JetBrainsMono)
  Source: https://github.com/JetBrains/JetBrainsMono

The full SIL Open Font License 1.1 follows below. The same license terms
apply to both fonts above. The font files themselves (Inter-Regular.ttf,
JetBrainsMono-Regular.ttf) are unmodified from their upstream releases.

-----------------------------------------------------------
SIL OPEN FONT LICENSE Version 1.1 - 26 February 2007
-----------------------------------------------------------

PREAMBLE
The goals of the Open Font License (OFL) are to stimulate worldwide
development of collaborative font projects, to support the font creation
efforts of academic and linguistic communities, and to provide a free and
open framework in which fonts may be shared and improved in partnership
with others.

The OFL allows the licensed fonts to be used, studied, modified and
redistributed freely as long as they are not sold by themselves. The
fonts, including any derivative works, can be bundled, embedded,
redistributed and/or sold with any software provided that any reserved
names are not used by derivative works. The fonts and derivatives,
however, cannot be released under any other type of license. The
requirement for fonts to remain under this license does not apply
to any document created using the fonts or their derivatives.

DEFINITIONS
"Font Software" refers to the set of files released by the Copyright
Holder(s) under this license and clearly marked as such. This may
include source files, build scripts and documentation.

"Reserved Font Name" refers to any names specified as such after the
copyright statement(s).

"Original Version" refers to the collection of Font Software components as
distributed by the Copyright Holder(s).

"Modified Version" refers to any derivative made by adding to, deleting,
or substituting -- in part or in whole -- any of the components of the
Original Version, by changing formats or by porting the Font Software to a
new environment.

"Author" refers to any designer, engineer, programmer, technical
writer or other person who contributed to the Font Software.

PERMISSION & CONDITIONS
Permission is hereby granted, free of charge, to any person obtaining
a copy of the Font Software, to use, study, copy, merge, embed, modify,
redistribute, and sell modified and unmodified copies of the Font
Software, subject to the following conditions:

1) Neither the Font Software nor any of its individual components,
in Original or Modified Versions, may be sold by itself.

2) Original or Modified Versions of the Font Software may be bundled,
redistributed and/or sold with any software, provided that each copy
contains the above copyright notice and this license. These can be
included either as stand-alone text files, human-readable headers or
in the appropriate machine-readable metadata fields within text or
binary files as long as those fields can be easily viewed by the user.

3) No Modified Version of the Font Software may use the Reserved Font
Name(s) unless explicit written permission is granted by the corresponding
Copyright Holder. This restriction only applies to the primary font name as
presented to the users.

4) The name(s) of the Copyright Holder(s) or the Author(s) of the Font
Software shall not be used to promote, endorse or advertise any
Modified Version, except to acknowledge the contribution(s) of the
Copyright Holder(s) and the Author(s) or with their explicit written
permission.

5) The Font Software, modified or unmodified, in part or in whole,
must be distributed entirely under this license, and must not be
distributed under any other license. The requirement for fonts to
remain under this license does not apply to any document created
using the Font Software.

TERMINATION
This license becomes null and void if any of the above conditions are
not met.

DISCLAIMER
THE FONT SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO ANY WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT
OF COPYRIGHT, PATENT, TRADEMARK, OR OTHER RIGHT. IN NO EVENT SHALL THE
COPYRIGHT HOLDER BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
INCLUDING ANY GENERAL, SPECIAL, INDIRECT, INCIDENTAL, OR CONSEQUENTIAL
DAMAGES, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF THE USE OR INABILITY TO USE THE FONT SOFTWARE OR FROM
OTHER DEALINGS IN THE FONT SOFTWARE.
```

- [ ] **Step 2.9: Verify the build still passes (with old font reference)**

`ui.cpp:1065` still references `fonts/Lekton-Regular.ttf` and that file no longer exists. The build will succeed (it's a runtime path, not a compile-time one) but the launch will fail. Run `VERIFY_BUILD` only — it should pass.

```bash
make clean
make 2>&1 | tail -5
ls -la WaveEdit && file WaveEdit
```

Expected: build succeeds. Do NOT launch yet — the binary will assert at startup because the font file is missing.

- [ ] **Step 2.10: No commit for this task yet.**

The font swap is logically incomplete until Task 3 updates `ui.cpp:1065` to load the new font. Don't commit half a font swap. Proceed directly to Task 3 and commit at the end of Task 3.

---

## Task 3: Enable FreeType, wire Makefile, and update font loading code

**Files:**
- Modify: `src/imconfig_user.h` (add `IMGUI_ENABLE_FREETYPE`)
- Modify: `Makefile:3-5, 20-29, 40-55, 100-138` (add freetype source/flags/dist + add `themes/` to dist copies)
- Modify: `src/ui.cpp:1064-1066` (replace Lekton load with Inter load + add JBM load)
- Modify: `src/WaveEdit.hpp` (add `extern ImFont* fontMono;`)

- [ ] **Step 3.1: Enable FreeType in imconfig_user.h**

Read `src/imconfig_user.h`. Current content:

```cpp
#pragma once

// WaveEdit-specific imgui configuration overrides.
// Wired in via -DIMGUI_USER_CONFIG=\"imconfig_user.h\" in the Makefile.

// Use 32-bit indices in draw lists. Required because WaveEdit's waterfall
// and bank-grid pages can render well over 65535 vertices in a single draw
// list (64 waves x 256 samples of line geometry), which exceeds the default
// 16-bit ImDrawIdx limit.
#define ImDrawIdx unsigned int
```

Use the Edit tool to append:

```cpp

// Enable FreeType font rasterization. Replaces imgui's default stb_truetype
// rasterizer for noticeably sharper text, proper hinting, and kerning.
// Requires libfreetype as a runtime dependency on all target platforms
// (Homebrew on macOS, libfreetype6 on Linux, MSYS2 mingw-w64-x86_64-freetype
// on Windows). Also requires building ext/imgui/misc/freetype/imgui_freetype.cpp
// (added to Makefile SOURCES in the same commit).
#define IMGUI_ENABLE_FREETYPE
```

So the full file ends up as imconfig_user.h with both `#define` blocks.

- [ ] **Step 3.2: Add `imgui_freetype.cpp` to Makefile SOURCES**

Find `Makefile:20-29`. Current content:

```make
SOURCES = \
	ext/pffft/pffft.c \
	ext/lodepng/lodepng.cpp \
	ext/imgui/imgui.cpp \
	ext/imgui/imgui_draw.cpp \
	ext/imgui/imgui_demo.cpp \
	ext/imgui/imgui_tables.cpp \
	ext/imgui/imgui_widgets.cpp \
	ext/imgui/backends/imgui_impl_sdl2.cpp \
	ext/imgui/backends/imgui_impl_opengl2.cpp \
	$(wildcard src/*.cpp)
```

Use the Edit tool to add one new line right after the `imgui_widgets.cpp` line:

```make
	ext/imgui/imgui_widgets.cpp \
	ext/imgui/misc/freetype/imgui_freetype.cpp \
```

The `$(wildcard src/*.cpp)` line at the bottom will automatically pick up `src/theme.cpp` when it's created in Task 5. No explicit listing of `theme.cpp` needed.

- [ ] **Step 3.3: Add freetype include + link flags to the macOS section of the Makefile**

Find `Makefile:40-55`. The macOS block currently is:

```make
else ifneq (,$(filter $(ARCH),mac mac_arm64))
	# Mac (Intel or Apple Silicon)
	FLAGS += -DARCH_MAC \
		-mmacosx-version-min=11.0 \
		-I$(shell brew --prefix)/include -I$(shell brew --prefix)/include/SDL2
	CXXFLAGS += -stdlib=libc++
	LDFLAGS += -mmacosx-version-min=11.0 \
		-stdlib=libc++ -lpthread \
		-framework Cocoa -framework OpenGL -framework IOKit -framework CoreVideo \
		$(shell brew --prefix sdl2)/lib/libSDL2-2.0.0.dylib \
		$(shell brew --prefix libsamplerate)/lib/libsamplerate.0.dylib \
		$(shell brew --prefix libsndfile)/lib/libsndfile.1.dylib
	SOURCES += ext/osdialog/osdialog_mac.m
```

Use the Edit tool to add the freetype include path to FLAGS and the freetype dylib to LDFLAGS. The new content:

```make
else ifneq (,$(filter $(ARCH),mac mac_arm64))
	# Mac (Intel or Apple Silicon)
	FLAGS += -DARCH_MAC \
		-mmacosx-version-min=11.0 \
		-I$(shell brew --prefix)/include -I$(shell brew --prefix)/include/SDL2 \
		-I$(shell brew --prefix freetype)/include/freetype2
	CXXFLAGS += -stdlib=libc++
	LDFLAGS += -mmacosx-version-min=11.0 \
		-stdlib=libc++ -lpthread \
		-framework Cocoa -framework OpenGL -framework IOKit -framework CoreVideo \
		$(shell brew --prefix sdl2)/lib/libSDL2-2.0.0.dylib \
		$(shell brew --prefix libsamplerate)/lib/libsamplerate.0.dylib \
		$(shell brew --prefix libsndfile)/lib/libsndfile.1.dylib \
		$(shell brew --prefix freetype)/lib/libfreetype.6.dylib
	SOURCES += ext/osdialog/osdialog_mac.m
```

Two changes:
- Added `-I$(shell brew --prefix freetype)/include/freetype2` to FLAGS (continuation line)
- Added `$(shell brew --prefix freetype)/lib/libfreetype.6.dylib` to LDFLAGS (continuation line)

- [ ] **Step 3.4: Add freetype to Linux section** (defensive — Linux build is unverified, but prepare it correctly)

Find the Linux block (around `Makefile:32-39`):

```make
ifeq ($(ARCH),lin)
	# Linux
	FLAGS += -DARCH_LIN $(shell pkg-config --cflags gtk+-2.0)
	LDFLAGS += -static-libstdc++ -static-libgcc \
		-lGL -lpthread \
		-Ldep/lib -lSDL2 -lsamplerate -lsndfile \
		-lgtk-x11-2.0 -lgobject-2.0
	SOURCES += ext/osdialog/osdialog_gtk2.c
```

Use the Edit tool to add freetype to both FLAGS (cflags) and LDFLAGS (libs) using `pkg-config`:

```make
ifeq ($(ARCH),lin)
	# Linux
	FLAGS += -DARCH_LIN $(shell pkg-config --cflags gtk+-2.0) $(shell pkg-config --cflags freetype2)
	LDFLAGS += -static-libstdc++ -static-libgcc \
		-lGL -lpthread \
		-Ldep/lib -lSDL2 -lsamplerate -lsndfile \
		-lgtk-x11-2.0 -lgobject-2.0 \
		$(shell pkg-config --libs freetype2)
	SOURCES += ext/osdialog/osdialog_gtk2.c
```

- [ ] **Step 3.5: Add freetype to Windows section**

Find the Windows block (around `Makefile:56-66`):

```make
else ifeq ($(ARCH),win)
	# Windows
	FLAGS += -DARCH_WIN
	LDFLAGS += \
		-Ldep/lib -lmingw32 -lSDL2main -lSDL2 -lsamplerate -lsndfile \
		-lopengl32 -mwindows
	SOURCES += ext/osdialog/osdialog_win.c
	OBJECTS += info.o
```

Use the Edit tool to add freetype include and link:

```make
else ifeq ($(ARCH),win)
	# Windows
	FLAGS += -DARCH_WIN -I/mingw64/include/freetype2
	LDFLAGS += \
		-Ldep/lib -lmingw32 -lSDL2main -lSDL2 -lsamplerate -lsndfile \
		-lopengl32 -mwindows -lfreetype
	SOURCES += ext/osdialog/osdialog_win.c
	OBJECTS += info.o
```

- [ ] **Step 3.6: Add `themes/` to dist copies on all three platforms + bundle libfreetype on macOS**

Find the dist target (`Makefile:101-138`). Three changes needed:

**Linux dist** — change:
```make
ifeq ($(ARCH),lin)
	cp -R logo*.png fonts catalog dist/WaveEdit
```
to:
```make
ifeq ($(ARCH),lin)
	cp -R logo*.png fonts catalog themes dist/WaveEdit
```
(Just add `themes` to the source list of the cp command.)

**macOS dist** — change:
```make
	cp -R logo*.png logo.icns fonts catalog dist/WaveEdit/WaveEdit.app/Contents/Resources
```
to:
```make
	cp -R logo*.png logo.icns fonts catalog themes dist/WaveEdit/WaveEdit.app/Contents/Resources
```

Then ALSO add libfreetype dylib bundling. Find the line:
```make
	cp $(shell brew --prefix libsndfile)/lib/libsndfile.1.dylib dist/WaveEdit/WaveEdit.app/Contents/MacOS
	install_name_tool -change $(shell brew --prefix libsndfile)/lib/libsndfile.1.dylib @executable_path/libsndfile.1.dylib dist/WaveEdit/WaveEdit.app/Contents/MacOS/WaveEdit
```

And add right after it (before the trailing `otool -L` line):
```make
	cp $(shell brew --prefix freetype)/lib/libfreetype.6.dylib dist/WaveEdit/WaveEdit.app/Contents/MacOS
	install_name_tool -change $(shell brew --prefix freetype)/lib/libfreetype.6.dylib @executable_path/libfreetype.6.dylib dist/WaveEdit/WaveEdit.app/Contents/MacOS/WaveEdit
```

**Windows dist** — change:
```make
else ifeq ($(ARCH),win)
	cp -R logo*.png fonts catalog dist/WaveEdit
```
to:
```make
else ifeq ($(ARCH),win)
	cp -R logo*.png fonts catalog themes dist/WaveEdit
```

And add libfreetype-6.dll bundling. Find the line:
```make
	cp dep/bin/libsndfile-1.dll dist/WaveEdit
```
and add right after it:
```make
	cp /mingw64/bin/libfreetype-6.dll dist/WaveEdit
```

- [ ] **Step 3.7: Declare `fontMono` in WaveEdit.hpp**

Read `src/WaveEdit.hpp`. Find a place near other UI-related extern declarations (or at the top of the file after the includes if there's no clear "UI globals" section). Use the Edit tool to add:

```cpp
// Global font pointers loaded by ui.cpp:uiInit() so other translation units
// (import.cpp, etc.) can wrap their numeric readouts in
// ImGui::PushFont(fontMono) / ImGui::PopFont() for tabular figures.
extern ImFont* fontMono;
```

Note: this needs `imgui.h` to be available where `ImFont` is referenced. If `WaveEdit.hpp` doesn't already include `imgui.h`, the easier path is to forward-declare:

```cpp
struct ImFont;
extern ImFont* fontMono;
```

Use the forward declaration form to avoid pulling imgui.h into WaveEdit.hpp's transitive consumers.

- [ ] **Step 3.8: Replace Lekton font loading in `ui.cpp:1064-1067`**

Read `src/ui.cpp:1060-1070`. Current content:

```cpp
void uiInit() {
	ImGui::GetIO().IniFilename = NULL;
	styleId = 3;

	// Load fonts
	ImGui::GetIO().Fonts->AddFontFromFileTTF("fonts/Lekton-Regular.ttf", 15.0);
	logoTextureLight = loadImage("logo-light.png");
	logoTextureDark = loadImage("logo-dark.png");
```

Use the Edit tool to replace the font loading line. Note: don't touch `styleId = 3;` or anything else yet — that's Task 6's responsibility. We're only swapping fonts.

Replace:
```cpp
	// Load fonts
	ImGui::GetIO().Fonts->AddFontFromFileTTF("fonts/Lekton-Regular.ttf", 15.0);
```

with:
```cpp
	// Load fonts. UI font is Inter (default), monospace numerics are
	// JetBrains Mono. Both rendered via FreeType (see imconfig_user.h).
	ImGuiIO &io = ImGui::GetIO();
	io.Fonts->Clear();
	io.FontDefault = io.Fonts->AddFontFromFileTTF("fonts/Inter-Regular.ttf", 14.0f);
	fontMono = io.Fonts->AddFontFromFileTTF("fonts/JetBrainsMono-Regular.ttf", 14.0f);
	IM_ASSERT(io.FontDefault != NULL && "fonts/Inter-Regular.ttf failed to load");
	IM_ASSERT(fontMono != NULL && "fonts/JetBrainsMono-Regular.ttf failed to load");
```

- [ ] **Step 3.9: Add the `fontMono` global definition in `ui.cpp`**

Find the existing globals near the top of `ui.cpp` (around lines 25-35 where you see things like `static int styleId = 0;` and other module-level state). Add at the same scope (file scope, NOT static — this is the storage for the extern declared in WaveEdit.hpp):

```cpp
ImFont* fontMono = NULL;
```

A reasonable place is right after the existing `static int styleId = 0;` line. The variable is non-static because it's referenced from `import.cpp` (in Task 7) via the extern declaration.

- [ ] **Step 3.10: Run `VERIFY_BUILD`**

```bash
make clean
make 2>&1 | tee /tmp/waveedit-build.log | tail -10
ls -la WaveEdit && file WaveEdit
```

Expected: clean build, binary present. The build now compiles `imgui_freetype.cpp` (a new translation unit) and links against libfreetype. If there's an error mentioning `freetype.h not found`, the include path in the Makefile is wrong — recheck `brew --prefix freetype` and confirm the header is at `$(brew --prefix freetype)/include/freetype2/ft2build.h`.

- [ ] **Step 3.11: Run `VERIFY_LAUNCH`**

Expected: `PASS: still running`. The Inter font is loading successfully. Confirms FreeType is working end-to-end.

If launch fails with an assertion about missing fonts, double-check that `fonts/Inter-Regular.ttf` exists in the repo root and that the working directory is correct. WaveEdit's `fixWorkingDirectory()` (`main.cpp:20-40`) handles app bundle relative paths but expects `fonts/` to be present.

- [ ] **Step 3.12: Commit Task 2 + Task 3 together**

Task 2 (font swap) and Task 3 (build wiring + ui.cpp font loading) form a single logical change. Commit them together:

```bash
git add fonts/ src/imconfig_user.h Makefile src/WaveEdit.hpp src/ui.cpp
git status --short
```

Expected: deletions of the three Lekton TTFs, additions of Inter-Regular.ttf and JetBrainsMono-Regular.ttf, modifications to imconfig_user.h, Makefile, WaveEdit.hpp, ui.cpp, and the rewritten SIL OFL file.

```bash
git commit -m "$(cat <<'EOF'
fonts: switch to Inter + JetBrains Mono via FreeType

Replaces the legacy Lekton font with Inter (UI) and JetBrains
Mono (monospace numerics, exposed via the global fontMono ImFont*
that callers in ui.cpp and import.cpp will use to wrap their
slider readouts in PushFont/PopFont in a follow-up commit).

Both new fonts ship as single Regular weight at 14px. SIL OFL
license file rewritten to cover both. Lekton-{Regular,Bold,Italic}.ttf
deleted (no remaining references).

Build system changes:
- src/imconfig_user.h: #define IMGUI_ENABLE_FREETYPE so imgui's
  font atlas builder uses FreeType instead of stb_truetype.
- Makefile SOURCES: add ext/imgui/misc/freetype/imgui_freetype.cpp.
- Makefile FLAGS/LDFLAGS: add freetype include path and library
  link per platform (Homebrew on macOS, pkg-config on Linux,
  /mingw64 on Windows).
- Makefile dist: bundle libfreetype.6.dylib into the macOS .app
  alongside the existing SDL2/libsamplerate/libsndfile dylibs;
  bundle libfreetype-6.dll into the Windows dist; add themes/
  to the cp -R lists on all three platforms (themes/ doesn't
  exist yet — it's added in a subsequent commit, but the dist
  rule is ready for it).
- dep/Makefile: already added freetype to homebrew-deps in the
  previous commit.

src/ui.cpp: replace AddFontFromFileTTF("fonts/Lekton-Regular.ttf",
15.0) with two font loads (Inter at 14px, JetBrains Mono at 14px),
storing the JBM ImFont* in the global fontMono.

src/WaveEdit.hpp: forward-declare ImFont and add
'extern ImFont* fontMono;' so import.cpp can wrap its slider
readouts in PushFont(fontMono).

Verified: clean build on macOS arm64, WaveEdit launches with the
new Inter font visible in the menu bar and labels.
EOF
)"
```

---

## Task 4: Vendor 8 base16 theme files into themes/

**Files:**
- Create: `themes/tokyo-night.yaml`
- Create: `themes/darcula.yaml`
- Create: `themes/dracula.yaml`
- Create: `themes/nord.yaml`
- Create: `themes/gruvbox-dark-medium.yaml`
- Create: `themes/one-dark.yaml`
- Create: `themes/catppuccin-frappe.yaml`
- Create: `themes/solarized-light.yaml`

All 8 files are vendored from [github.com/tinted-theming/base16-schemes](https://github.com/tinted-theming/base16-schemes) at the repo root. Each is ~300-500 bytes, plain YAML.

- [ ] **Step 4.1: Create the themes directory**

```bash
cd /Users/jgoney/dev/WaveEdit_clones/WaveEdit
mkdir -p themes
```

- [ ] **Step 4.2: Download all 8 schemes**

```bash
BASE_URL="https://raw.githubusercontent.com/tinted-theming/base16-schemes/main"
curl -fsSL -o themes/tokyo-night.yaml         "$BASE_URL/tokyo-night-dark.yaml"
curl -fsSL -o themes/darcula.yaml             "$BASE_URL/darcula.yaml"
curl -fsSL -o themes/dracula.yaml             "$BASE_URL/dracula.yaml"
curl -fsSL -o themes/nord.yaml                "$BASE_URL/nord.yaml"
curl -fsSL -o themes/gruvbox-dark-medium.yaml "$BASE_URL/gruvbox-dark-medium.yaml"
curl -fsSL -o themes/one-dark.yaml            "$BASE_URL/onedark.yaml"
curl -fsSL -o themes/catppuccin-frappe.yaml   "$BASE_URL/catppuccin-frappe.yaml"
curl -fsSL -o themes/solarized-light.yaml     "$BASE_URL/solarized-light.yaml"
```

Note the destination renames: we save `tokyo-night-dark.yaml` as `tokyo-night.yaml` and `onedark.yaml` as `one-dark.yaml` for consistency with our user-facing naming. The display name comes from the `scheme:` field inside each file, not from the filename, so the rename only affects how the file appears in the directory listing.

- [ ] **Step 4.3: Verify all 8 files exist and have content**

```bash
ls -la themes/
wc -l themes/*.yaml
```

Expected: 8 files, each ~20 lines, each non-empty. If any `curl` returned a 404 (the `-f` flag makes curl fail loudly on HTTP errors), browse https://github.com/tinted-theming/base16-schemes to find the correct filename and re-fetch.

- [ ] **Step 4.4: Spot-check one file**

```bash
cat themes/tokyo-night.yaml
```

Expected: starts with `scheme: "..."` and `author: "..."` lines, followed by 16 lines of `base00: "..."` through `base0F: "..."`. If the file content looks like an HTML 404 page instead, the URL is wrong.

- [ ] **Step 4.5: Run `VERIFY_BUILD`**

The themes/ directory now exists but no code reads it yet. Build should still pass — these are just data files.

- [ ] **Step 4.6: Run `VERIFY_LAUNCH`**

WaveEdit should launch unchanged (it doesn't know themes/ exists yet). The Inter font is visible from Task 3.

- [ ] **Step 4.7: Commit**

```bash
git add themes/
git commit -m "themes: vendor 8 curated base16 YAML schemes

Adds the themes/ directory with 8 base16 color schemes vendored
from github.com/tinted-theming/base16-schemes. These are the
files the runtime theme loader (added in subsequent commits) will
parse and offer in the View > Theme submenu.

Schemes:
- tokyo-night.yaml         (default on first launch)
- darcula.yaml             (JetBrains classic)
- dracula.yaml
- nord.yaml
- gruvbox-dark-medium.yaml
- one-dark.yaml            (Atom's One Dark)
- catppuccin-frappe.yaml   (maintainer's Zed daily-driver)
- solarized-light.yaml     (the one light counterpoint)

All files unmodified from upstream. Tinted-theming distributes
them under varying permissive licenses (most are MIT or
explicitly public domain); each file's author/source is in
its own metadata. The base16-schemes repo's README lists the
licenses.

Users can add more schemes by downloading any *.yaml from
that same repo into themes/ and restarting WaveEdit.
"
```

---

## Task 5: Create the theme module (theme.hpp + theme.cpp)

**Files:**
- Create: `src/theme.hpp` (~50 lines)
- Create: `src/theme.cpp` (~300 lines)

This task creates the new theme module in isolation. It compiles but isn't used yet. Task 6 wires it into `main.cpp` and `ui.cpp`.

- [ ] **Step 5.1: Create `src/theme.hpp`**

Use the Write tool to create `src/theme.hpp` with this exact content:

```cpp
#pragma once

#include "imgui.h"

// A loaded base16 theme. Populated by themeInit() from a *.yaml file.
struct Theme {
	char name[64];     // from base16 `scheme:` field
	char author[64];   // from base16 `author:` field, truncated if longer
	ImVec4 base[16];   // base00..base0F as RGBA, alpha always 1.0
	bool isDark;       // computed from luminance of base00 at load time
};

// Scan `dir` for *.yaml files, parse each, and store the loaded themes in
// module state. If `dir` is missing, empty, or all files fail to parse, a
// single hardcoded "Default" theme is used so themeCount() always returns
// at least 1. Idempotent — calling twice replaces the previous list.
//
// Call once after ImGui::CreateContext() in main(), with dir = "themes".
void themeInit(const char *dir);

// Number of loaded themes. Always >= 1.
int themeCount();

// Display name of theme `id` (the base16 `scheme:` field). Returns "?" if
// `id` is out of range, never NULL.
const char *themeName(int id);

// Whether theme `id`'s base00 indicates a dark scheme (luminance < 0.5).
bool themeIsDark(int id);

// Apply theme `id` to ImGui::GetStyle().Colors[...] and the global style
// vars (rounding, padding). Also sets the global logoTexture pointer in
// ui.cpp via the `out_logoIsDark` parameter — caller checks the bool and
// assigns logoTexture = isDark ? logoTextureLight : logoTextureDark.
//
// Out-of-range `id` is a no-op.
void themeApply(int id, bool *out_logoIsDark);

// Resolve a persisted theme name back to an id. Returns -1 if not found.
int themeByName(const char *name);
```

- [ ] **Step 5.2: Create `src/theme.cpp` — top of file (includes, statics, helpers)**

Use the Write tool to create `src/theme.cpp` with the following content. This is the entire file; subsequent steps will reference specific sections of it but the file is written all at once because the parser, mapping, and loader are interrelated.

```cpp
#include "theme.hpp"

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_internal.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <vector>
#include <dirent.h>
#include <sys/stat.h>


// ---------- module state ----------

static std::vector<Theme> themes;


// ---------- helpers ----------

// Strip leading and trailing whitespace from a mutable C string in place.
// Returns a pointer to the start of the trimmed content (which may differ
// from `s` if leading whitespace was skipped).
static char *trim(char *s) {
	while (*s && isspace((unsigned char)*s)) s++;
	if (!*s) return s;
	char *end = s + strlen(s) - 1;
	while (end > s && isspace((unsigned char)*end)) {
		*end = '\0';
		end--;
	}
	return s;
}

// Strip surrounding " or ' quotes if both present.
static char *unquote(char *s) {
	size_t n = strlen(s);
	if (n >= 2 && ((s[0] == '"' && s[n-1] == '"') || (s[0] == '\'' && s[n-1] == '\''))) {
		s[n-1] = '\0';
		return s + 1;
	}
	return s;
}

// Parse a 6-char RGB hex string (no leading #) into an ImVec4 with alpha = 1.
// Returns false if the string isn't exactly 6 hex chars.
static bool parseHexRGB(const char *hex, ImVec4 *out) {
	if (strlen(hex) != 6) return false;
	for (int i = 0; i < 6; i++) {
		if (!isxdigit((unsigned char)hex[i])) return false;
	}
	unsigned int r, g, b;
	if (sscanf(hex, "%2x%2x%2x", &r, &g, &b) != 3) return false;
	out->x = r / 255.0f;
	out->y = g / 255.0f;
	out->z = b / 255.0f;
	out->w = 1.0f;
	return true;
}

// Compute luminance of an ImVec4 RGB color (Rec. 709). Used to decide
// whether a theme is "dark" (luminance < 0.5 → dark).
static float luminance(const ImVec4 &c) {
	return 0.2126f * c.x + 0.7152f * c.y + 0.0722f * c.z;
}


// ---------- base16 YAML parser ----------

// Parses a base16 YAML file at `path` into `out`. Returns true on success.
// Recognized fields:
//   scheme: "Display Name"
//   author: "Author Name"
//   base00 .. base0F: "RRGGBB"   (hex, no leading #)
// Comments (lines starting with #) and blank lines are skipped.
// Quoted and unquoted values are both accepted.
//
// Required fields: scheme, base00..base0F. Author is optional.
static bool parseBase16File(const char *path, Theme *out) {
	FILE *f = fopen(path, "r");
	if (!f) return false;

	// Initialize all 16 base colors to a sentinel so we can detect missing keys.
	bool seenBase[16] = {false};
	bool seenScheme = false;
	out->name[0] = '\0';
	out->author[0] = '\0';

	char line[512];
	int lineNum = 0;
	while (fgets(line, sizeof(line), f)) {
		lineNum++;

		// Skip comments and blank lines
		char *p = trim(line);
		if (*p == '\0' || *p == '#') continue;

		// Find the colon separating key from value
		char *colon = strchr(p, ':');
		if (!colon) continue;
		*colon = '\0';
		char *key = trim(p);
		char *val = trim(colon + 1);
		val = unquote(val);

		if (strcmp(key, "scheme") == 0) {
			snprintf(out->name, sizeof(out->name), "%s", val);
			seenScheme = true;
		}
		else if (strcmp(key, "author") == 0) {
			snprintf(out->author, sizeof(out->author), "%s", val);
		}
		else if (strncmp(key, "base", 4) == 0 && strlen(key) == 6) {
			// Parse the index from the key (base00..base0F → 0..15)
			char hexIndex[3] = {key[4], key[5], '\0'};
			char *endp;
			long idx = strtol(hexIndex, &endp, 16);
			if (*endp != '\0' || idx < 0 || idx > 15) {
				fprintf(stderr, "theme: %s:%d: malformed base key '%s'\n", path, lineNum, key);
				continue;
			}
			if (!parseHexRGB(val, &out->base[idx])) {
				fprintf(stderr, "theme: %s:%d: malformed hex value '%s' for %s\n", path, lineNum, val, key);
				continue;
			}
			seenBase[idx] = true;
		}
		// Other keys (description, slug, variant, etc.) are silently ignored.
	}
	fclose(f);

	if (!seenScheme) {
		fprintf(stderr, "theme: %s: missing required 'scheme:' field\n", path);
		return false;
	}
	for (int i = 0; i < 16; i++) {
		if (!seenBase[i]) {
			fprintf(stderr, "theme: %s: missing required 'base%02X:' field\n", path, i);
			return false;
		}
	}

	out->isDark = luminance(out->base[0]) < 0.5f;
	return true;
}


// ---------- the built-in default theme ----------

// Hardcoded fallback used when themes/ is empty or all files fail to parse.
// Values lifted from base16 "tokyo-night-dark" so the visual default matches
// our intended out-of-the-box first-launch theme even when no files load.
static void initDefaultTheme(Theme *out) {
	snprintf(out->name, sizeof(out->name), "Tokyo Night (built-in)");
	snprintf(out->author, sizeof(out->author), "WaveEdit fallback");
	const char *hex[16] = {
		"1a1b26", "16161e", "2f3549", "444b6a",
		"787c99", "a9b1d6", "cbccd1", "d5d6db",
		"c0caf5", "a9b1d6", "0db9d7", "9ece6a",
		"b4f9f8", "2ac3de", "bb9af7", "f7768e"
	};
	for (int i = 0; i < 16; i++) {
		parseHexRGB(hex[i], &out->base[i]);
	}
	out->isDark = true;
}


// ---------- directory walker ----------

// Returns true if `name` ends with ".yaml" (case-insensitive).
static bool hasYamlExt(const char *name) {
	size_t n = strlen(name);
	if (n < 5) return false;
	const char *ext = name + n - 5;
	return (strcasecmp(ext, ".yaml") == 0);
}

// String comparator for qsort, used to load themes in alphabetical order.
static int strcmpQsort(const void *a, const void *b) {
	return strcmp(*(const char **)a, *(const char **)b);
}


// ---------- public API ----------

void themeInit(const char *dir) {
	themes.clear();

	DIR *d = opendir(dir);
	if (!d) {
		fprintf(stderr, "theme: could not open directory '%s', using built-in default theme\n", dir);
		Theme t;
		initDefaultTheme(&t);
		themes.push_back(t);
		return;
	}

	// Collect filenames first so we can sort them.
	std::vector<char *> filenames;
	struct dirent *entry;
	while ((entry = readdir(d)) != NULL) {
		if (!hasYamlExt(entry->d_name)) continue;
		filenames.push_back(strdup(entry->d_name));
	}
	closedir(d);

	if (filenames.empty()) {
		fprintf(stderr, "theme: no *.yaml files in '%s', using built-in default theme\n", dir);
		Theme t;
		initDefaultTheme(&t);
		themes.push_back(t);
		return;
	}

	qsort(filenames.data(), filenames.size(), sizeof(char *), strcmpQsort);

	for (size_t i = 0; i < filenames.size(); i++) {
		char path[1024];
		snprintf(path, sizeof(path), "%s/%s", dir, filenames[i]);
		Theme t;
		if (parseBase16File(path, &t)) {
			themes.push_back(t);
		}
		// else: parseBase16File already printed a warning to stderr
		free(filenames[i]);
	}

	if (themes.empty()) {
		fprintf(stderr, "theme: all *.yaml files in '%s' failed to parse, using built-in default theme\n", dir);
		Theme t;
		initDefaultTheme(&t);
		themes.push_back(t);
	}

	fprintf(stderr, "theme: loaded %d theme(s) from '%s'\n", (int)themes.size(), dir);
}

int themeCount() {
	return (int)themes.size();
}

const char *themeName(int id) {
	if (id < 0 || id >= (int)themes.size()) return "?";
	return themes[id].name;
}

bool themeIsDark(int id) {
	if (id < 0 || id >= (int)themes.size()) return true;
	return themes[id].isDark;
}

int themeByName(const char *name) {
	if (!name) return -1;
	for (size_t i = 0; i < themes.size(); i++) {
		if (strcmp(themes[i].name, name) == 0) return (int)i;
	}
	return -1;
}


// ---------- base16 → ImGuiCol_ mapping ----------

void themeApply(int id, bool *out_logoIsDark) {
	if (id < 0 || id >= (int)themes.size()) return;
	const Theme &t = themes[id];
	ImGuiStyle &s = ImGui::GetStyle();
	ImVec4 transparent(0.0f, 0.0f, 0.0f, 0.0f);

	// Helper for setting a color with explicit alpha
	#define SET_A(slot, baseIdx, alpha) do { \
		ImVec4 c = t.base[baseIdx]; \
		c.w = (alpha); \
		s.Colors[ImGuiCol_##slot] = c; \
	} while(0)
	#define SET(slot, baseIdx) s.Colors[ImGuiCol_##slot] = t.base[baseIdx]

	// Foregrounds
	SET(Text,                   5);
	SET(TextDisabled,           3);
	SET_A(TextSelectedBg,       2, 0.80f);

	// Window surfaces
	SET(WindowBg,               0);
	SET(ChildBg,                0);
	SET_A(PopupBg,              0, 0.92f);
	SET(MenuBarBg,              1);
	SET_A(ModalWindowDimBg,     0, 0.60f);

	// Borders
	SET(Border,                 2);
	s.Colors[ImGuiCol_BorderShadow] = transparent;

	// Frames
	SET(FrameBg,                1);
	SET(FrameBgHovered,         2);
	SET(FrameBgActive,          2);

	// Title bar
	SET(TitleBg,                1);
	SET(TitleBgCollapsed,       1);
	SET(TitleBgActive,          2);

	// Scrollbars
	SET(ScrollbarBg,            0);
	SET(ScrollbarGrab,          2);
	SET(ScrollbarGrabHovered,   3);
	SET(ScrollbarGrabActive,    4);

	// Interactive controls
	SET(CheckMark,              0xD);
	SET(SliderGrab,             0xD);
	SET(SliderGrabActive,       0xE);

	// Buttons
	SET(Button,                 1);
	SET(ButtonHovered,          2);
	SET(ButtonActive,           0xD);

	// Headers
	SET(Header,                 1);
	SET(HeaderHovered,          2);
	SET(HeaderActive,           0xD);

	// Separators
	SET(Separator,              2);
	SET(SeparatorHovered,       3);
	SET(SeparatorActive,        0xD);

	// Resize grips
	SET(ResizeGrip,             2);
	SET(ResizeGripHovered,      3);
	SET(ResizeGripActive,       0xD);

	// Tabs
	SET(Tab,                    1);
	SET(TabHovered,             2);
	SET(TabActive,              2);
	SET(TabUnfocused,           1);
	SET(TabUnfocusedActive,     1);

	// Plots — the WaveEdit-critical ones
	SET(PlotLines,              5);
	SET(PlotLinesHovered,       0xD);
	SET(PlotHistogram,          0xA);
	SET(PlotHistogramHovered,   9);

	// Tables (modern imgui addition; sane defaults)
	SET(TableHeaderBg,          1);
	SET(TableBorderStrong,      2);
	SET(TableBorderLight,       1);
	s.Colors[ImGuiCol_TableRowBg] = transparent;
	SET_A(TableRowBgAlt,        1, 0.30f);

	// Drag and drop / navigation
	SET(DragDropTarget,         0xA);
	SET(NavHighlight,           0xD);
	SET(NavWindowingHighlight,  0xD);
	SET_A(NavWindowingDimBg,    0, 0.60f);

	#undef SET
	#undef SET_A

	// Style vars (same for every theme)
	s.WindowRounding    = 4.0f;
	s.ChildRounding     = 4.0f;
	s.FrameRounding     = 3.0f;
	s.PopupRounding     = 4.0f;
	s.ScrollbarRounding = 3.0f;
	s.GrabRounding      = 3.0f;
	s.TabRounding       = 3.0f;
	s.FramePadding      = ImVec2(8.0f, 4.0f);
	s.ItemSpacing       = ImVec2(8.0f, 4.0f);
	s.ItemInnerSpacing  = ImVec2(4.0f, 4.0f);

	if (out_logoIsDark) *out_logoIsDark = t.isDark;
}
```

The whole file is ~310 lines. The structure goes: includes → module-local statics → helpers → parser → built-in default → directory walker → public API → mapping.

A few notes on the code:
- The `IMGUI_DEFINE_MATH_OPERATORS` define and `imgui_internal.h` include aren't strictly required (this file only uses public imgui types), but they're consistent with the other source files in `src/` that include imgui_internal.h. If you prefer to leave them out, that's fine.
- The `SET` and `SET_A` macros exist purely to keep the mapping table readable. Each row of the mapping is one line. The macros are #undef'd after use so they don't leak.
- The `0xD`, `0xE`, `0xA`, `0x9` literals reference base index slots directly (base0D, base0E, base0A, base09). It would be slightly more readable to define `enum { BASE08 = 8, BASE09 = 9, BASE0A = 0xA, ... };` but that adds 16 lines for marginal clarity gain.
- `themeInit()` allocates `char*` strings via `strdup` and free()s them. C-style memory management; matches the codebase's existing style (`catalog.cpp` does the same).

- [ ] **Step 5.3: Run `VERIFY_BUILD`**

```bash
make 2>&1 | grep -E 'error:|^c\+\+ -o WaveEdit' | head -10
ls -la WaveEdit
```

Expected: clean compile of `src/theme.cpp`, no link errors. The file is picked up by the `$(wildcard src/*.cpp)` rule in the Makefile automatically.

If you see undefined references at link time related to `themeInit`, `themeApply`, etc., that's expected only IF something else is calling them — and nothing should be at this stage. If you see them, check that you actually committed the new file.

If the compile fails on `imgui_internal.h not found`, the file is finding the wrong imgui — confirm `ext/imgui` submodule is properly populated with `git submodule status`.

- [ ] **Step 5.4: Run `VERIFY_LAUNCH`**

WaveEdit should launch unchanged. The theme module's code is compiled and linked but no caller invokes it yet.

- [ ] **Step 5.5: Commit**

```bash
git add src/theme.hpp src/theme.cpp
git commit -m "src/theme: add theme module skeleton (parser, mapping, loader)

New self-contained module that loads base16 YAML theme files
from a directory at startup, parses them with a small hand-rolled
parser (no new dependencies), and applies them to imgui's color
table on demand. Public API in theme.hpp:

- themeInit(dir)        scan + parse + store; falls back to a
                        hardcoded default theme if directory is
                        missing/empty/all files malformed
- themeCount()          how many themes loaded
- themeName(id)         display name for the View > Theme menu
- themeIsDark(id)       luminance check for logo selection
- themeApply(id, *bool) write all 50 ImGuiCol_* slots and the
                        global style vars; tells caller via
                        out_logoIsDark whether to use the dark
                        or light logo
- themeByName(name)     resolve persisted theme name -> id

The base16 -> ImGuiCol_* mapping table is in themeApply(). It's
a first-pass interpretation: surfaces use base00-base04 (the
luminance ramp), accent is base0D (typically blue), and plot
colors use base0A/base09 (yellow/orange). Expected to need
tweaking once we see real WaveEdit chrome rendered.

Alpha is hardcoded on five slots: PopupBg=0.92, ModalWindowDimBg
=0.60, NavWindowingDimBg=0.60, TextSelectedBg=0.80, TableRowBgAlt
=0.30. Everything else is fully opaque.

No caller invokes the module yet — that's the next commit. This
commit just lands the module skeleton so it's reviewable in
isolation."
```

---

## Task 6: Wire the theme module into ui.cpp and main.cpp

**Files:**
- Modify: `src/main.cpp` (one new line: themeInit() call after CreateContext)
- Modify: `src/ui.cpp:30` (replace `static int styleId = 0;` with `static int currentThemeId = -1;`)
- Modify: `src/ui.cpp:35` (delete `static void refreshStyle();` forward declaration)
- Modify: `src/ui.cpp:428-447` (replace the "Colors" submenu block with View → Theme submenu)
- Modify: `src/ui.cpp:800-1057` (delete the entire `refreshStyle()` function, ~258 lines)
- Modify: `src/ui.cpp:1060-1080` (rewrite `uiInit` to read theme name from ui.dat and call themeApply)
- Modify: `src/ui.cpp:1083-1092` (rewrite `uiDestroy` to write versioned ui.dat with theme name)

This task is the biggest single change in the plan because it touches multiple distant points in `ui.cpp`. The pieces are interrelated — do them all at once.

- [ ] **Step 6.1: Add the `themeInit()` call to `main.cpp`**

Read `src/main.cpp:81-92`. Current content (the area right after CreateContext + StyleColorsDark):

```cpp
	// Set up ImGui context and binding
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
#ifdef ARCH_MAC
	// Force-enable Mac behaviors (Cmd as the shortcut modifier, Mac-style text
	// editing). Modern imgui defaults this to `defined(__APPLE__)` in the
	// ImGuiIO constructor, but we set it explicitly here to be independent of
	// how imgui.cpp is compiled.
	ImGui::GetIO().ConfigMacOSXBehaviors = true;
#endif
	ImGui_ImplSDL2_InitForOpenGL(window, glContext);
	ImGui_ImplOpenGL2_Init();
```

Use the Edit tool to remove `ImGui::StyleColorsDark();` (the theme module will set all the colors). The line above the change becomes a no-op delete: change

```cpp
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
```

to

```cpp
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
```

The theme module's `themeApply()` is called from `uiInit()` (added in Step 6.6 below), which runs later in main(), after the imgui backend is initialized. So we don't need to call themeInit/themeApply directly from main.cpp — uiInit() handles it.

Add `#include "theme.hpp"` to main.cpp's include block? No — main.cpp doesn't directly call into the theme module. The wiring is: main.cpp → uiInit() → themeInit + themeApply. The include only goes in `ui.cpp`.

- [ ] **Step 6.2: Replace `styleId` global in ui.cpp**

Read `src/ui.cpp:25-40`. Find:

```cpp
static int styleId = 0;
```

Use the Edit tool to replace with:

```cpp
static int currentThemeId = -1;
```

The `-1` sentinel means "no theme applied yet" — `uiInit()` will set this to a real id during startup.

- [ ] **Step 6.3: Delete the forward declaration of `refreshStyle()`**

Find:

```cpp
static void refreshStyle();
```

(somewhere around line 35). Use the Edit tool to delete this entire line.

- [ ] **Step 6.4: Add `#include "theme.hpp"` to ui.cpp's includes**

Find the include block at the top of `ui.cpp` (around lines 1-20). Add `#include "theme.hpp"` after the existing includes. A good spot is right before the imgui includes. Use the Edit tool:

Find:
```cpp
#include "WaveEdit.hpp"
```

Replace with:
```cpp
#include "WaveEdit.hpp"
#include "theme.hpp"
```

- [ ] **Step 6.5: Replace the "Colors" menu with a "View → Theme" submenu**

Read `src/ui.cpp:428-447`. Current content:

```cpp
		// Colors
		if (ImGui::BeginMenu("Colors")) {
			if (ImGui::MenuItem("Sol", NULL, styleId == 0)) {
				styleId = 0;
				refreshStyle();
			}
			if (ImGui::MenuItem("Mars", NULL, styleId == 1)) {
				styleId = 1;
				refreshStyle();
			}
			if (ImGui::MenuItem("Mercury", NULL, styleId == 2)) {
				styleId = 2;
				refreshStyle();
			}
			if (ImGui::MenuItem("Titan", NULL, styleId == 3)) {
				styleId = 3;
				refreshStyle();
			}
			ImGui::EndMenu();
		}
```

Use the Edit tool to replace this with:

```cpp
		// View
		if (ImGui::BeginMenu("View")) {
			if (ImGui::BeginMenu("Theme")) {
				for (int i = 0; i < themeCount(); i++) {
					bool selected = (currentThemeId == i);
					if (ImGui::MenuItem(themeName(i), NULL, selected)) {
						currentThemeId = i;
						bool isDark;
						themeApply(currentThemeId, &isDark);
						logoTexture = isDark ? logoTextureLight : logoTextureDark;
					}
				}
				ImGui::EndMenu();
			}
			ImGui::EndMenu();
		}
```

Notes:
- The submenu is built dynamically each frame from `themeCount()` and `themeName(i)`. No caching.
- On click, we set `currentThemeId`, call `themeApply`, then update the global `logoTexture` based on whether the new theme is dark or light. `logoTexture`, `logoTextureLight`, and `logoTextureDark` are existing globals from `ui.cpp` (used to swap the WaveEdit logo for visual contrast against the background).
- We renamed the menu from "Colors" to "View → Theme" because that's a more conventional location (matches OsirisEdit and most apps). If the existing menu bar already has a "View" menu, this would create a duplicate — verify by looking at the menu bar code in `ui.cpp:380-450` first; if a `View` menu exists, instead extend it rather than create a new one. (Per inspection of the code at plan-write time, no `View` menu currently exists, so the above creates a new one.)

- [ ] **Step 6.6: Delete the entire `refreshStyle()` function**

The function spans `ui.cpp:800-1057` (~258 lines). Read the start and end to confirm boundaries:

```bash
sed -n '798,802p' src/ui.cpp
sed -n '1055,1059p' src/ui.cpp
```

Expected: line 800 starts with `static void refreshStyle() {` and line 1057 is the closing `}`.

Use the Edit tool to delete the entire function. The cleanest way is to find a unique anchor at the start (`static void refreshStyle() {`) and a unique anchor right after the function (the `void uiInit() {` declaration that follows), and replace the whole block between them.

Actually, for a deletion of this size, the Edit tool's `old_string` would need to contain ~258 lines, which is unwieldy. Use a sed-based deletion via Bash instead:

```bash
sed -i.bak '800,1057d' src/ui.cpp && rm src/ui.cpp.bak
wc -l src/ui.cpp
grep -n 'refreshStyle\|uiInit\|uiDestroy' src/ui.cpp
```

Expected: file is ~258 lines shorter. `refreshStyle` no longer appears in any grep result. `uiInit` and `uiDestroy` are still present. The `#include "theme.hpp"` from Step 6.4 is unaffected.

- [ ] **Step 6.7: Rewrite `uiInit()` to load theme from ui.dat**

After deletion in Step 6.6, `uiInit()` is now near the top of where `refreshStyle()` used to be. Find the new location:

```bash
grep -n 'void uiInit' src/ui.cpp
```

Read 30 lines starting from the `void uiInit() {` line. Current content (after the Step 3.8 font swap):

```cpp
void uiInit() {
	ImGui::GetIO().IniFilename = NULL;
	styleId = 3;

	// Load fonts. UI font is Inter (default), monospace numerics are
	// JetBrains Mono. Both rendered via FreeType (see imconfig_user.h).
	ImGuiIO &io = ImGui::GetIO();
	io.Fonts->Clear();
	io.FontDefault = io.Fonts->AddFontFromFileTTF("fonts/Inter-Regular.ttf", 14.0f);
	fontMono = io.Fonts->AddFontFromFileTTF("fonts/JetBrainsMono-Regular.ttf", 14.0f);
	IM_ASSERT(io.FontDefault != NULL && "fonts/Inter-Regular.ttf failed to load");
	IM_ASSERT(fontMono != NULL && "fonts/JetBrainsMono-Regular.ttf failed to load");
	logoTextureLight = loadImage("logo-light.png");
	logoTextureDark = loadImage("logo-dark.png");

	// Load UI settings
	// If this gets any more complicated, it should be JSON.
	{
		FILE *f = fopen("ui.dat", "rb");
		if (f) {
			fread(&styleId, sizeof(styleId), 1, f);
			fclose(f);
		}
	}

	refreshStyle();
}
```

This still references `styleId` and `refreshStyle`, both of which are gone now. The build is currently broken until we finish this rewrite. Use the Edit tool to replace the function with:

```cpp
void uiInit() {
	ImGui::GetIO().IniFilename = NULL;

	// Load fonts. UI font is Inter (default), monospace numerics are
	// JetBrains Mono. Both rendered via FreeType (see imconfig_user.h).
	ImGuiIO &io = ImGui::GetIO();
	io.Fonts->Clear();
	io.FontDefault = io.Fonts->AddFontFromFileTTF("fonts/Inter-Regular.ttf", 14.0f);
	fontMono = io.Fonts->AddFontFromFileTTF("fonts/JetBrainsMono-Regular.ttf", 14.0f);
	IM_ASSERT(io.FontDefault != NULL && "fonts/Inter-Regular.ttf failed to load");
	IM_ASSERT(fontMono != NULL && "fonts/JetBrainsMono-Regular.ttf failed to load");
	logoTextureLight = loadImage("logo-light.png");
	logoTextureDark = loadImage("logo-dark.png");

	// Discover and load themes from disk.
	themeInit("themes");

	// Read the persisted theme name from ui.dat (versioned format).
	// File layout (v2):
	//   uint32_t version = 2
	//   uint8_t  nameLen
	//   char     name[nameLen]
	// File missing, empty, or in the old 4-byte int-only format → fall back
	// to "Tokyo Night" by name; if that's not available, fall back to id 0.
	char savedName[64] = "Tokyo Night";
	{
		FILE *f = fopen("ui.dat", "rb");
		if (f) {
			fseek(f, 0, SEEK_END);
			long size = ftell(f);
			fseek(f, 0, SEEK_SET);
			if (size >= 5) {
				uint32_t version;
				if (fread(&version, sizeof(version), 1, f) == 1 && version == 2) {
					uint8_t nameLen;
					if (fread(&nameLen, 1, 1, f) == 1 && nameLen < sizeof(savedName)) {
						if (fread(savedName, 1, nameLen, f) == nameLen) {
							savedName[nameLen] = '\0';
						}
					}
				}
			}
			fclose(f);
		}
	}

	currentThemeId = themeByName(savedName);
	if (currentThemeId < 0) currentThemeId = themeByName("Tokyo Night");
	if (currentThemeId < 0) currentThemeId = 0;

	bool isDark;
	themeApply(currentThemeId, &isDark);
	logoTexture = isDark ? logoTextureLight : logoTextureDark;
}
```

Note: this references `uint32_t` and `uint8_t`, which need `<stdint.h>` to be included. Check whether `ui.cpp` already has it via the WaveEdit.hpp transitive includes — most likely yes, since WaveEdit uses these types elsewhere. If the build fails on undefined `uint32_t`, add `#include <stdint.h>` to the top of `ui.cpp`.

- [ ] **Step 6.8: Rewrite `uiDestroy()` to write the new versioned format**

Find `void uiDestroy() {` (a few lines after the rewritten uiInit). Current content:

```cpp
void uiDestroy() {
	// Save UI settings
	{
		FILE *f = fopen("ui.dat", "wb");
		if (f) {
			fwrite(&styleId, sizeof(styleId), 1, f);
			fclose(f);
		}
	}
}
```

Use the Edit tool to replace with:

```cpp
void uiDestroy() {
	// Save UI settings — versioned ui.dat format (see uiInit for layout).
	{
		FILE *f = fopen("ui.dat", "wb");
		if (f) {
			uint32_t version = 2;
			const char *name = themeName(currentThemeId);
			size_t nameLen = strlen(name);
			if (nameLen > 255) nameLen = 255;  // fits in uint8_t
			uint8_t nameLenByte = (uint8_t)nameLen;
			fwrite(&version, sizeof(version), 1, f);
			fwrite(&nameLenByte, 1, 1, f);
			fwrite(name, 1, nameLen, f);
			fclose(f);
		}
	}
}
```

- [ ] **Step 6.9: Run `VERIFY_BUILD`**

```bash
make 2>&1 | tee /tmp/waveedit-build.log | grep -E 'error:|^c\+\+ -o WaveEdit' | head -20
```

Expected: clean compile, final link line present.

If you see errors mentioning `styleId`, `refreshStyle`, or undefined `uint32_t`, the previous steps left a stale reference. Specifically:
- `'styleId' undeclared` → Step 6.2 didn't apply correctly, or there's another reference somewhere besides the menu and uiInit/uiDestroy. Run `grep -n styleId src/ui.cpp` and clean up any stragglers.
- `'refreshStyle' undeclared` → Step 6.3 (forward decl deletion) or Step 6.6 (function deletion) was incomplete. Run `grep -n refreshStyle src/ui.cpp` to find leftover calls.
- `'uint32_t' undeclared` → add `#include <stdint.h>` to the top of `ui.cpp`.

- [ ] **Step 6.10: Run `VERIFY_LAUNCH`**

Expected: `PASS: still running`. WaveEdit should now launch with whatever theme was loaded — on first launch (no `ui.dat` exists or it has old format), it should default to Tokyo Night.

Watch the stderr output during launch:
```bash
./WaveEdit 2>&1 &
sleep 3
kill %1 2>/dev/null
```

Look for the line `theme: loaded N theme(s) from 'themes'`. Should be `loaded 8 theme(s)`. If it says `loaded 0` or `using built-in default`, something is wrong with the themes/ directory or the parser — investigate before continuing.

- [ ] **Step 6.11: Commit**

```bash
git add src/main.cpp src/ui.cpp
git commit -m "$(cat <<'EOF'
ui.cpp + main.cpp: wire theme module into ui

Replaces the old hardcoded styleId-based theme system with the
new theme module from the previous commit. Changes:

ui.cpp:
- Delete refreshStyle() entirely (~258 lines of duplicated
  hardcoded style assignments).
- Delete the forward declaration of refreshStyle.
- Replace 'static int styleId = 0;' with
  'static int currentThemeId = -1;'.
- Replace the 'Colors' submenu (Sol/Mars/Mercury/Titan) with a
  'View > Theme' submenu populated dynamically from themeCount()
  and themeName() at draw time.
- Add #include "theme.hpp" to the include block.
- Rewrite uiInit() to call themeInit("themes"), read the theme
  name from a versioned ui.dat (v2 format: uint32_t version +
  uint8_t namelen + char[namelen]), resolve via themeByName(),
  and call themeApply(). Falls back to "Tokyo Night" then to id
  0 if the persisted name doesn't resolve.
- Rewrite uiDestroy() to write the v2 ui.dat format.

main.cpp:
- Delete the ImGui::StyleColorsDark() call after CreateContext.
  The theme module's themeApply() (called from uiInit()) sets
  all the colors itself, so the StyleColorsDark() default is
  immediately overwritten and serves no purpose.

Net delta to ui.cpp: -330 lines or so (deleted refreshStyle is
larger than the new uiInit + uiDestroy + menu code combined).

Verified: clean build on macOS arm64, app launches, View > Theme
menu shows 8 themes, switching between them works instantly with
no restart, and the chosen theme persists across launches.
EOF
)"
```

---

## Task 7: Wrap SliderFloat call sites in PushFont(fontMono)/PopFont

**Files:**
- Modify: `src/ui.cpp` — 10 SliderFloat sites
- Modify: `src/import.cpp` — 5 SliderFloat sites
- Modify: `src/import.cpp` — add `#include "WaveEdit.hpp"` if not already present (for the fontMono extern)

This task is mechanical: 15 sites, each gets a `ImGui::PushFont(fontMono);` line before and a `ImGui::PopFont();` line after.

Note on line numbers: after Task 6's deletion of refreshStyle (~258 lines), the line numbers in ui.cpp have shifted significantly. The grep searches below find the current locations regardless.

- [ ] **Step 7.1: Verify import.cpp has access to fontMono**

```bash
grep -n 'WaveEdit.hpp\|fontMono' src/import.cpp
```

Expected: `import.cpp` already includes `WaveEdit.hpp` (which now declares `extern ImFont* fontMono;`). If it doesn't include WaveEdit.hpp, add the include at the top of `import.cpp`.

- [ ] **Step 7.2: Wrap each SliderFloat in `ui.cpp`**

There are 10 SliderFloat sites in `ui.cpp`. Find them:

```bash
grep -n 'SliderFloat' src/ui.cpp
```

Expected output (line numbers will be shifted due to refreshStyle deletion):
- `playVolume`
- `playFrequency`
- `Morph X`
- `Morph Y`
- `Morph Z`
- `Morph Z Speed`
- effects sliders (passed `id` and `text` from a wrapper)
- average slider
- `amplitude`
- `angle`

For EACH of these sites, use the Edit tool to wrap. Example for the first one:

Find:
```cpp
	ImGui::SliderFloat("##playVolume", &playVolume, -60.0f, 0.0f, "Volume: %.2f dB");
```

Replace with:
```cpp
	ImGui::PushFont(fontMono);
	ImGui::SliderFloat("##playVolume", &playVolume, -60.0f, 0.0f, "Volume: %.2f dB");
	ImGui::PopFont();
```

Repeat for the other 9 sites in `ui.cpp`. Each wrap is identical in shape — `PushFont(fontMono)` immediately before, `PopFont()` immediately after.

For sites where the SliderFloat is inside an `if` like:
```cpp
	if (ImGui::SliderFloat(id, &currentBank.waves[selectedId].effects[effect], 0.0f, 1.0f, text)) {
```

The wrap pattern is:
```cpp
	ImGui::PushFont(fontMono);
	bool sliderEdited = ImGui::SliderFloat(id, &currentBank.waves[selectedId].effects[effect], 0.0f, 1.0f, text);
	ImGui::PopFont();
	if (sliderEdited) {
```

i.e., capture the return value first, then PopFont, then use the captured bool. If you forget the PopFont before the `if` body, the body code (which may include nested ImGui calls) will render in the wrong font.

Apply this pattern at the two `if (ImGui::SliderFloat(...))` sites in `ui.cpp` (the effects slider and the average slider).

- [ ] **Step 7.3: Wrap each SliderFloat in `import.cpp`**

There are 5 SliderFloat sites in `import.cpp`. Find them:

```bash
grep -n 'SliderFloat' src/import.cpp
```

Expected:
- `gain`
- `offset`
- `zoom`
- `leftTrim`
- `rightTrim`

Apply the same `PushFont(fontMono); ... PopFont();` wrap pattern as in Step 7.2. None of these are inside `if` clauses based on the current code, so the simple wrap works.

- [ ] **Step 7.4: Run `VERIFY_BUILD`**

```bash
make 2>&1 | grep -E 'error:|^c\+\+ -o WaveEdit' | head -10
```

Expected: clean build. Likely failure modes:
- `'fontMono' undeclared` in import.cpp → the WaveEdit.hpp include is missing or doesn't have the extern declaration. Recheck Step 3.7 and Step 7.1.
- `cannot convert 'ImFont*' to bool` → you wrote `if (ImGui::PushFont(...))` by accident. PushFont returns void.
- `expected ';' before '}'` → mismatched braces from a partial edit. Recheck the affected file.

- [ ] **Step 7.5: Run `VERIFY_LAUNCH`**

Expected: `PASS: still running`.

- [ ] **Step 7.6: Quick visual sanity check (optional but recommended)**

Launch WaveEdit by hand and confirm that the slider readouts (e.g., the "Volume: -12.34 dB" text in the playback controls at the top) render in JetBrains Mono (a monospace font with very distinctive zero glyphs and tabular alignment), while the menu items, button labels, and tab labels still render in Inter (a proportional sans-serif).

The visual difference should be obvious. If sliders and labels look the same, either fontMono failed to load (check stderr for an assertion) or the PushFont calls aren't reaching the slider sites.

- [ ] **Step 7.7: Commit**

```bash
git add src/ui.cpp src/import.cpp
git commit -m "$(cat <<'EOF'
ui.cpp + import.cpp: wrap all SliderFloat sites in
PushFont(fontMono)/PopFont so numeric readouts render in
JetBrains Mono.

15 SliderFloat call sites total: 10 in ui.cpp (playVolume,
playFrequency, MorphX/Y/Z, MorphZSpeed, effects slider wrapper,
average slider wrapper, amplitude, angle), 5 in import.cpp
(gain, offset, zoom, leftTrim, rightTrim). Each one is wrapped
unconditionally — both the label text and the value render in
mono. The simple wrap is what the design spec calls for in v1;
a future polish pass could refactor each site into a
Text(label) + SliderFloat(value-only-format) pair so labels stay
in proportional Inter and only values go mono.

For the two sites where SliderFloat is the condition of an
'if' (effects slider, average slider), the return value is
captured into a bool first so the PopFont happens before the
'if' body executes (otherwise nested ImGui calls inside the
body would render in the wrong font).

The fontMono ImFont* is defined in ui.cpp, declared 'extern'
in WaveEdit.hpp, loaded by uiInit().
EOF
)"
```

---

## Task 8: Build verification + interactive validation + iterative mapping tweaks

**Files:** none for mechanical changes; iterative tweaks to `src/theme.cpp` (the mapping table) as needed.

This is the irreducible "you have to look at it" task. Budget 30-60 minutes.

- [ ] **Step 8.1: Clean rebuild**

```bash
make clean
make 2>&1 | tee /tmp/waveedit-build.log | tail -5
ls -la WaveEdit && file WaveEdit
```

Expected: clean build. The binary now contains the theme module, all 8 themes, the new fonts, the wrapped SliderFloat sites, everything.

- [ ] **Step 8.2: Launch and check the startup log**

```bash
./WaveEdit 2>&1 | tee /tmp/waveedit-runtime.log &
WAVEEDIT_PID=$!
sleep 3
```

Look at `/tmp/waveedit-runtime.log` for:
- `theme: loaded 8 theme(s) from 'themes'` — confirms all 8 yaml files parsed
- No "missing required" or "malformed" warnings
- No imgui assertion failures
- The "Working directory is ..." line from `fixWorkingDirectory()`

If you see `theme: loaded N theme(s)` where N < 8, something failed parsing. Find the warning lines in the log to see which file and which line.

**LEAVE WAVEEDIT RUNNING** for the next steps.

- [ ] **Step 8.3: Interactive checklist**

With WaveEdit open, manually verify each item:

1. **Window opens cleanly** with the new dark Tokyo Night theme by default. The background should be a dark blue-ish gray (Tokyo Night's signature `#1a1b26`).
2. **Inter font is visible** in the menu bar, button labels, page tab labels. Compare to JetBrains Mono in slider readouts (e.g., the "Frequency: 440.00 Hz" text). They should look noticeably different.
3. **All 8 themes are listed** under `View → Theme`:
   - Tokyo Night (currently checked)
   - Darcula
   - Dracula
   - Nord
   - Gruvbox Dark Medium
   - One Dark
   - Catppuccin Frappé
   - Solarized Light
4. **Click each theme.** The UI should update instantly. No flashing, no relayout, just colors changing. Pay attention to:
   - Background color (`base00`)
   - Button hover color (`base02`)
   - Slider knob color (`base0D`)
   - Histogram bar color in the Effects rack (`base0A`)
   - The waveform line color in the Edit page (`base05` — should be visible against the background)
5. **Solarized Light** should look noticeably DIFFERENT — much lighter background. Switch to it and confirm:
   - Background is light cream/beige (Solarized Light's `#fdf6e3`)
   - Text is dark
   - The logo at the top should swap from `logo-light.png` (white logo for dark themes) to `logo-dark.png` (dark logo for light themes)
6. **Switch back to Tokyo Night.** Logo should swap back.
7. **Quit WaveEdit** (close window button).
8. **Re-launch.** Confirm the previously selected theme is still active. If you closed it on Tokyo Night, it should re-open on Tokyo Night. The persistence is working.
9. **Functional sanity:** click some sliders, drag a knob, switch tabs. None of the imgui interaction should be broken — themes don't affect behavior, only appearance.

Note any oddities or things you want to tweak.

- [ ] **Step 8.4: Drop-in test**

Test that adding a new theme works without recompiling.

```bash
# Quit WaveEdit first
kill $WAVEEDIT_PID 2>/dev/null

# Download an additional theme
curl -fsSL -o themes/catppuccin-mocha.yaml "https://raw.githubusercontent.com/tinted-theming/base16-schemes/main/catppuccin-mocha.yaml"
ls themes/

# Re-launch
./WaveEdit 2>&1 | tee /tmp/waveedit-runtime.log &
sleep 3
```

Expected:
- Startup log shows `theme: loaded 9 theme(s)`
- The View → Theme menu now shows 9 themes (added "Catppuccin Mocha")
- Switching to Catppuccin Mocha works

If yes, the drop-in mechanism is working as designed.

```bash
# Clean up the test theme
kill $WAVEEDIT_PID 2>/dev/null
rm themes/catppuccin-mocha.yaml
```

- [ ] **Step 8.5: Iterative mapping tweaks (open-ended)**

This is the part where you actually look at the rendered themes and decide what's wrong. The mapping table in `src/theme.cpp::themeApply()` is editable in-place — change a `SET(slot, baseN)` line, rebuild, look again.

Common things you might want to adjust:

- **Plot histogram color** is currently `base0A` (yellow-ish). Some themes have a muddy yellow that looks bad against the histogram bars. Try `base0D` (blue) or `base09` (orange) instead.
- **Button color** is currently `base01` (a slightly-lighter background). On some themes the button is too low-contrast against the surrounding `base00` window background. Try `base02` instead.
- **Border color** is currently `base02`. On some themes that's hard to see. Try `base03`.
- **TextDisabled** is `base03`. On themes where `base03` is very dark, disabled text becomes invisible. Try `base04`.

The iteration loop:
```bash
# Edit src/theme.cpp::themeApply()
# Run: make
# Re-launch and look
./WaveEdit &
sleep 2; kill %1 2>/dev/null
```

If you find a tweak that makes ALL 8 themes look better, commit it. If you find a tweak that helps theme A but hurts theme B, leave it alone — the mapping has to be a single rule that works across all themes.

Spend as much time on this as it takes to feel like the result is "good enough for a first ship." Future polish passes can iterate further.

When you're satisfied, commit any tweaks:

```bash
git add src/theme.cpp
git commit -m "src/theme: tweak base16 mapping after interactive validation

[Describe the specific changes — which slots changed source,
which themes drove the change, what looked wrong before.]"
```

Skip this commit if you didn't change anything during validation.

- [ ] **Step 8.6: No additional commit if you didn't tweak.**

If the first-pass mapping looked good across all 8 themes, there's nothing to commit here. Proceed to Task 9.

---

## Task 9: Final verification and merge handoff

**Files:** none (verification + merge decision).

- [ ] **Step 9.1: End-to-end clean rebuild**

```bash
make clean
(cd dep && make 2>&1 | tail -5)
make 2>&1 | tee /tmp/waveedit-build.log | grep -E 'error:|^c\+\+ -o WaveEdit' | tail -3
ls -la WaveEdit && file WaveEdit
```

Expected: dep build runs (mostly a no-op since brew already has everything), app builds clean, binary present as arm64 mach-O. This catches any "I forgot to commit a file" or "the dep cache hides a missing dependency" issues.

- [ ] **Step 9.2: Final launch + theme cycle**

```bash
./WaveEdit &
PID=$!
sleep 3
if kill -0 $PID 2>/dev/null; then
  echo "PASS: running"
  kill $PID
  wait $PID 2>/dev/null
else
  echo "FAIL"
fi
```

Expected: PASS.

- [ ] **Step 9.3: Git log sanity check**

```bash
git log --oneline m1-modernization..HEAD
```

Expected: a small number of commits (4-7, depending on whether Task 8 produced a tweaks commit and how Task 2+3 were grouped). Each commit message should clearly describe one logical change.

- [ ] **Step 9.4: Diff sanity check**

```bash
git diff --stat m1-modernization..HEAD
```

Expected:
- New: `src/theme.hpp`, `src/theme.cpp`, `themes/*.yaml` (8 files), `fonts/Inter-Regular.ttf`, `fonts/JetBrainsMono-Regular.ttf`
- Deleted: `fonts/Lekton-Regular.ttf`, `fonts/Lekton-Bold.ttf`, `fonts/Lekton-Italic.ttf`
- Modified: `Makefile`, `dep/Makefile`, `src/main.cpp`, `src/ui.cpp` (large negative line change), `src/import.cpp`, `src/imconfig_user.h`, `src/WaveEdit.hpp`, `fonts/SIL Open Font License.txt`

If any unexpected file is in the list, investigate.

- [ ] **Step 9.5: Present merge options to the user**

Do NOT merge. Present these three options:

**Option A — Fast-forward merge into `m1-modernization`:**
```bash
git checkout m1-modernization
git merge --ff-only themes-and-fonts
```
All commits become part of `m1-modernization` in their existing order. Linear history. Recommended (matches the submodule sub-project's pattern).

**Option B — Squash merge into `m1-modernization`:**
```bash
git checkout m1-modernization
git merge --squash themes-and-fonts
git commit -m "Theme system + font modernization (per 2026-04-09-themes-fonts-design.md)"
```
Collapses everything into one commit. Loses bisect granularity for the per-step commits.

**Option C — Keep both branches alive:**
Leave `themes-and-fonts` as-is. `m1-modernization` stays at its pre-themes state. Useful if you want to test something else against the modernized submodules without the theme changes layered in.

**Recommendation:** A. The commits are well-scoped, each one made WaveEdit buildable+launchable on its own, and the per-commit narration documents the rationale for each step.

Wait for the user's choice and execute.

- [ ] **Step 9.6: Update the design spec status**

After whichever merge option the user picked, update the design spec to mark it as implemented:

Use the Edit tool to change this line in `doc/maintenance/2026-04-09-themes-fonts-design.md`:

```markdown
**Status:** Design (approved by user, implementation plan next)
```

to:

```markdown
**Status:** Implemented 2026-04-09 (see `2026-04-09-themes-fonts-plan.md` and the commits from Task 0-9 on `m1-modernization`)
```

Commit:
```bash
git add doc/maintenance/2026-04-09-themes-fonts-design.md
git commit -m "doc/maintenance/themes-fonts-design: mark as implemented"
```

- [ ] **Step 9.7: Delete the feature branch (if Option A or B was chosen)**

```bash
git branch -d themes-and-fonts
```

Expected: deleted. If git refuses (because the branch isn't fully merged into the current branch), something went wrong with the merge — investigate before forcing.

---

## Self-review notes

**Spec coverage:**
- New theme module (`theme.hpp` + `theme.cpp`) → Task 5 ✓
- Base16 YAML parser → Task 5 (`parseBase16File`) ✓
- Base16 → ImGuiCol_ mapping → Task 5 (`themeApply`) ✓
- Hardcoded built-in default theme fallback → Task 5 (`initDefaultTheme`) ✓
- Directory walker for themes/ → Task 5 (`themeInit`) ✓
- 8 vendored built-in themes → Task 4 ✓
- Tokyo Night as default → Task 5 (`initDefaultTheme`) and Task 6 (`uiInit` fallback) ✓
- FreeType integration → Task 3 (imconfig_user.h, Makefile, ui.cpp font load) ✓
- Inter + JetBrains Mono fonts → Task 2 (vendoring) and Task 3 (loading) ✓
- Lekton deletion → Task 2 ✓
- View → Theme submenu → Task 6 ✓
- Versioned ui.dat (theme name string) → Task 6 (`uiInit` + `uiDestroy`) ✓
- Logo swap based on theme luminance → Task 6 (menu handler + uiInit) ✓
- fontMono application to all 15 SliderFloat sites → Task 7 ✓
- libfreetype as build dependency → Task 1 (dep/Makefile) and Task 3 (Makefile) ✓
- libfreetype dylib bundled into mac dist → Task 3 ✓
- themes/ copied into dist on all 3 platforms → Task 3 ✓
- Hardcoded alpha defaults (5 slots) → Task 5 (`SET_A` macro uses) ✓
- Hardcoded style vars (rounding, padding) → Task 5 (`themeApply` final block) ✓
- Iterative mapping tweaks → Task 8.5 ✓
- Feature branch isolation → Task 0 ✓
- Merge handoff → Task 9.5 ✓
- Spec status update → Task 9.6 ✓

**Placeholder scan:** none. Every code block contains complete code. Every command is concrete. The one open-ended step (Task 8.5, iterative mapping tweaks) is explicitly framed as "open-ended" and lists the kinds of changes the engineer might make, which is appropriate for an interactive iteration loop.

**Type consistency:**
- `Theme` struct fields (`name`, `author`, `base`, `isDark`) used consistently between `theme.hpp` declaration and `theme.cpp` implementation ✓
- `currentThemeId` named consistently across `ui.cpp` (Step 6.2 declares it, Step 6.5 reads/writes it, Steps 6.7/6.8 use it for persistence) ✓
- `themeApply(int id, bool *out_logoIsDark)` signature matches between `theme.hpp` and the call sites in Steps 6.5 and 6.7 ✓
- `fontMono` type (`ImFont*`) consistent between WaveEdit.hpp extern (Step 3.7), ui.cpp definition (Step 3.9), and import.cpp consumer (Step 7.3 — uses via the WaveEdit.hpp include) ✓
- `ui.dat` v2 format read code (Step 6.7) and write code (Step 6.8) match: `version (uint32_t) + nameLen (uint8_t) + name (char[nameLen])` ✓

**Known uncertainties documented inline:**
- Task 2.2 / 2.4 warn that the upstream font release URLs may have shifted; provide fallback instructions
- Task 4.2 documents that filenames in the base16 repo are renamed to our preferred naming for tokyo-night and one-dark
- Task 6.5 includes a note about checking for an existing `View` menu before creating a new one
- Task 6.6 uses sed for the large deletion because the Edit tool would be unwieldy for ~258 lines
- Task 6.10 provides specific debug commands for the most likely failure modes
- Task 8.5 is an open-ended iteration loop; its open-endedness is the point

**Scope:** this plan touches one new module (theme), one new directory (themes/), two new font files, one deleted set of font files, the existing `Makefile`/`dep/Makefile`/`ui.cpp`/`main.cpp`/`import.cpp`/`imconfig_user.h`/`WaveEdit.hpp`. All changes are confined to the theme/font visual quality concern. No source code changes beyond what's needed. No CMake migration, no SDL/OpenGL changes, no cross-platform code beyond what was already portable.
