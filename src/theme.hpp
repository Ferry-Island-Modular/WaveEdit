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
