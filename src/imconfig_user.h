#pragma once

// WaveEdit-specific imgui configuration overrides.
// Wired in via -DIMGUI_USER_CONFIG=\"imconfig_user.h\" in the Makefile.

// Use 32-bit indices in draw lists. Required because WaveEdit's waterfall
// and bank-grid pages can render well over 65535 vertices in a single draw
// list (64 waves x 256 samples of line geometry), which exceeds the default
// 16-bit ImDrawIdx limit.
#define ImDrawIdx unsigned int

// Enable FreeType font rasterization. Replaces imgui's default stb_truetype
// rasterizer for noticeably sharper text, proper hinting, and kerning.
// Requires libfreetype as a runtime dependency on all target platforms
// (Homebrew on macOS, libfreetype6 on Linux, MSYS2 mingw-w64-x86_64-freetype
// on Windows). Also requires building ext/imgui/misc/freetype/imgui_freetype.cpp
// (added to Makefile SOURCES in the same commit).
#define IMGUI_ENABLE_FREETYPE
