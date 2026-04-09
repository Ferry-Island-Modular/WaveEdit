#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_internal.h"

#include "theme.hpp"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <strings.h>
#include <vector>
#include <dirent.h>


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

		// Strip inline comments: find the first '#' that is NOT inside a
		// quoted substring, and truncate there. This handles base16 files
		// like darcula.yaml that put `# background` etc. after the value.
		{
			bool in_quote = false;
			char quote_char = 0;
			for (char *q = val; *q; q++) {
				if (in_quote) {
					if (*q == quote_char) in_quote = false;
				}
				else if (*q == '"' || *q == '\'') {
					in_quote = true;
					quote_char = *q;
				}
				else if (*q == '#') {
					*q = '\0';
					break;
				}
			}
			// Re-trim any trailing whitespace exposed by truncation.
			size_t vlen = strlen(val);
			while (vlen > 0 && isspace((unsigned char)val[vlen - 1])) {
				val[--vlen] = '\0';
			}
		}

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
	SET(InputTextCursor,        5);   // matches imgui's default of inheriting from Text
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
	SET(ButtonActive,           3);

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
	SET(TabSelected,            2);
	SET(TabDimmed,              1);
	SET(TabDimmedSelected,      1);

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
	SET(NavCursor,              0xD);
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
