#pragma once

// WaveEdit-specific imgui configuration overrides.
// Wired in via -DIMGUI_USER_CONFIG=\"imconfig_user.h\" in the Makefile.

// Use 32-bit indices in draw lists. Required because WaveEdit's waterfall
// and bank-grid pages can render well over 65535 vertices in a single draw
// list (64 waves x 256 samples of line geometry), which exceeds the default
// 16-bit ImDrawIdx limit.
#define ImDrawIdx unsigned int
