# Theme System + Font Modernization Design

**Date:** 2026-04-09
**Status:** Design (approved by user, implementation plan next)
**Sub-project of:** WaveEdit maintained-fork stewardship
**Predecessor:** `2026-04-08-fork-research.md` (OsirisEdit's existing dark/light theme system was the seed for this work)

## Goal

Replace WaveEdit's hardcoded `styleId 0..3` theme blocks with a runtime theme loader that reads base16 YAML files from a `themes/` directory. Ship 7 curated built-in themes plus a path for users to drop in any of the 200+ schemes from [tinted-theming/base16-schemes](https://github.com/tinted-theming/base16-schemes) and have them "just work." Concurrently upgrade font rendering from imgui's default stb_truetype rasterizer to FreeType, and ship Inter (UI) + JetBrains Mono (numerics) as the new default fonts.

## Why both at once

Themes and font rendering are adjacent concerns ("visual quality") that share a sub-project context naturally. Bundling them keeps the commit history coherent (one "make WaveEdit look modern" arc) and amortizes the interactive validation cost — you only need to live with the new UI once.

## Non-goals

Explicitly deferred to future improvement items:

- **In-app theme editor with live preview.** The data layer (this sub-project) is the foundation; a polished `ImGui::ShowStyleEditor()` wrapper or custom editor UI is its own future Tier-1 sub-project per `future-improvements.md`.
- **Per-theme font choice.** Themes can override colors, alpha defaults, and style vars. They cannot change the font. Adding per-theme fonts would require font-atlas rebuilding at runtime (~50 lines of GL texture-reupload code) for marginal benefit.
- **WaveEdit-native JSON theme format** with full per-`ImGuiCol_*` control alongside base16. YAGNI for v1; can be added later if someone needs pixel control over a specific slot.
- **Multiple font sizes.** Single 14px size per font family in v1. Adding more sizes is one extra `AddFontFromFileTTF` call when needed.
- **Bold/italic font variants.** Single Regular weight per family in v1.
- **Glassy main window** (`SDL_WINDOW_TRANSPARENT` + transparent OpenGL context). Separate sub-project. Complex enough to deserve its own design pass.
- **User-adjustable transparency intensity slider.** Hardcoded alpha defaults in v1. Settings UI is a future improvement.
- **Removing the old Lekton fonts** from `fonts/`. Keep as dead weight in v1; clean up in a follow-up once nothing references them.

## Decisions made during brainstorming

| Decision | Choice | Why |
|---|---|---|
| Scope ceiling | **B (medium)** — runtime loader from disk, no in-app editor in v1 | Matches the "drop in darcula.yaml" wish without overbuilding |
| Theme file format | **base16 YAML** (canonical tinted-theming format) | Hundreds of existing schemes; no new dependencies (~40-line hand-rolled parser) |
| Font pair | **Inter (UI) + JetBrains Mono (numerics)** | Zed-aligned, modern, clean, very widely deployed |
| Transparency scope | **B + hardcoded** — popups/modals translucent, main window opaque, no user slider | Matches Zed's actual approach; zero SDL/OpenGL changes; simple and adjustable in code if it ever feels wrong |
| Theme picker UI | **A** — `View → Theme` submenu with checkmarks | Lightest weight, matches OsirisEdit's existing pattern, matches imgui conventions |
| Persistence | **Theme name string** in `ui.dat` (not int ID) | Survives users adding/removing theme files |
| Logo swapping | **Auto** — derived from base16 luminance of `base00` | Zero per-theme metadata; existing logo files reused |
| Hot-reload | **Instant** | Free with imgui's immediate-mode rendering |
| Code structure | **Approach B** — separate `src/theme.hpp` + `src/theme.cpp` module | `ui.cpp` is already too large; clean module boundary; natural home for future theme work |

## Architecture

A new self-contained `theme` module loads themes from disk at startup, exposes them by id/name, and applies them to imgui's color table on demand. `ui.cpp` becomes a consumer that calls `themeInit()` once at startup and `themeApply(id)` from a menu handler. The hardcoded per-`styleId` blocks currently in `ui.cpp::refreshStyle()` (~400 lines of duplicated style assignments) are deleted and replaced by data-driven loading.

```
┌─────────────────────────┐
│ themes/*.yaml (on disk) │
└────────────┬────────────┘
             │ scanned at startup
             ▼
┌─────────────────────────┐
│  theme.cpp (loader +    │
│  base16 parser +        │
│  base16→imgui mapper)   │
└────────────┬────────────┘
             │ themeApply(id)
             ▼
┌─────────────────────────┐
│  ImGui::GetStyle()      │
│  .Colors[ImGuiCol_*]    │
└─────────────────────────┘
             ▲
             │ menu click
             │
┌─────────────────────────┐
│  ui.cpp (View → Theme   │
│  submenu, builds list   │
│  from themeCount() +    │
│  themeName(i))          │
└─────────────────────────┘
```

## Module API: `src/theme.hpp`

```cpp
#pragma once
#include "imgui.h"

struct Theme {
    char name[64];     // parsed from base16 `scheme:` field
    char author[64];   // parsed from base16 `author:` field (truncated if longer)
    ImVec4 base[16];   // base00..base0F as RGBA (alpha always 1)
    bool isDark;       // computed at load from luminance of base00
};

// Scan `dir` for *.yaml files, parse each, store in module state.
// Always succeeds — if directory missing or no files parse, falls back to a
// hardcoded built-in default theme so themeCount() >= 1 always.
// Call once after ImGui::CreateContext() in main().
void themeInit(const char* dir);

// Number of themes loaded (always >= 1).
int themeCount();

// Display name for theme `id`. Used by ui.cpp to build the View → Theme submenu.
const char* themeName(int id);

// Apply theme `id` to ImGui::GetStyle().Colors[...], style vars, and the
// global logo pointer. Called from menu handler. Fast — no allocation.
void themeApply(int id);

// Resolve a persisted theme name back to an id. Returns -1 if not found
// (e.g., user deleted the corresponding theme file between sessions).
int themeByName(const char* name);

// Whether theme `id`'s base00 indicates a dark scheme. Used by callers that
// need this without applying the theme.
bool themeIsDark(int id);
```

## File layout after the sub-project

```
src/
  theme.hpp         (new, ~50 lines)
  theme.cpp         (new, ~300 lines)
  ui.cpp            (modified: -400 lines hardcoded styles, +30 menu code,
                     +40 font loading wiring; net ≈ -330 lines)
  main.cpp          (modified: one themeInit() call after CreateContext)
  imconfig_user.h   (modified: add #define IMGUI_ENABLE_FREETYPE)
themes/             (new directory; shipped in app bundle)
  darcula.yaml
  dracula.yaml
  nord.yaml
  tokyo-night.yaml
  gruvbox-dark-medium.yaml
  one-dark.yaml
  solarized-light.yaml
fonts/
  Inter-Regular.ttf            (new; ~310 KB)
  JetBrainsMono-Regular.ttf    (new; ~190 KB)
  Lekton-Regular.ttf           (kept for now; removed in follow-up)
  Lekton-Bold.ttf              (kept for now; removed in follow-up)
  Lekton-Italic.ttf            (kept for now; removed in follow-up)
  SIL Open Font License.txt    (extended to cover Inter and JBM)
Makefile            (modified: add ext/imgui/misc/freetype/imgui_freetype.cpp
                     to SOURCES, add freetype include + link flags per arch,
                     add libfreetype dylib bundling to mac dist target)
dep/Makefile        (modified: add freetype to homebrew-deps)
ui.dat              (format change: new versioned format with theme name string)
```

## Base16 → ImGuiCol_ mapping

The mapping interprets base16's 16 semantic colors against imgui's ~50 color slots. The strategy: keep surfaces grayscale using base00–base07 (the luminance ramp), use base0D as the primary accent, and use base0A/base09/base0B for data-display colors (waveform plots, histograms).

### Foregrounds

| ImGuiCol_ | Source | Notes |
|---|---|---|
| `Text` | base05 | default foreground |
| `TextDisabled` | base03 | dimmed labels |
| `TextSelectedBg` | base02 (alpha 0.80) | text selection highlight |

### Window surfaces

| ImGuiCol_ | Source | Notes |
|---|---|---|
| `WindowBg` | base00 | main canvas — fully opaque |
| `ChildBg` | base00 | child windows — fully opaque |
| `PopupBg` | base00 (alpha 0.92) | menus, tooltips — slightly translucent |
| `MenuBarBg` | base01 | top menu bar |
| `ModalWindowDimBg` | base00 (alpha 0.60) | dim layer behind modals |

### Borders

| ImGuiCol_ | Source | Notes |
|---|---|---|
| `Border` | base02 | window/frame borders |
| `BorderShadow` | (transparent) | no shadows |

### Frames (slider tracks, input field bgs)

| ImGuiCol_ | Source | Notes |
|---|---|---|
| `FrameBg` | base01 | input field background |
| `FrameBgHovered` | base02 | mouse over |
| `FrameBgActive` | base02 | being interacted with |

### Title bar

| ImGuiCol_ | Source | Notes |
|---|---|---|
| `TitleBg` | base01 | (WaveEdit's main window has no title bar but floating windows might) |
| `TitleBgCollapsed` | base01 | |
| `TitleBgActive` | base02 | |

### Scrollbars

| ImGuiCol_ | Source | Notes |
|---|---|---|
| `ScrollbarBg` | base00 | track |
| `ScrollbarGrab` | base02 | thumb |
| `ScrollbarGrabHovered` | base03 | thumb hover |
| `ScrollbarGrabActive` | base04 | thumb dragging |

### Interactive controls

| ImGuiCol_ | Source | Notes |
|---|---|---|
| `CheckMark` | base0D | checkbox check |
| `SliderGrab` | base0D | slider knob |
| `SliderGrabActive` | base0E | slider knob being dragged |

### Buttons

| ImGuiCol_ | Source | Notes |
|---|---|---|
| `Button` | base01 | default state |
| `ButtonHovered` | base02 | mouse over |
| `ButtonActive` | base0D | pressed |

### Headers (collapsibles, selectables, tree nodes)

| ImGuiCol_ | Source | Notes |
|---|---|---|
| `Header` | base01 | |
| `HeaderHovered` | base02 | |
| `HeaderActive` | base0D | |

### Separators

| ImGuiCol_ | Source | Notes |
|---|---|---|
| `Separator` | base02 | |
| `SeparatorHovered` | base03 | |
| `SeparatorActive` | base0D | |

### Resize grips

| ImGuiCol_ | Source | Notes |
|---|---|---|
| `ResizeGrip` | base02 | |
| `ResizeGripHovered` | base03 | |
| `ResizeGripActive` | base0D | |

### Tabs

| ImGuiCol_ | Source | Notes |
|---|---|---|
| `Tab` | base01 | inactive tab |
| `TabHovered` | base02 | mouse over |
| `TabActive` | base02 | currently selected |
| `TabUnfocused` | base01 | when window not focused |
| `TabUnfocusedActive` | base01 | |

### Plots (the WaveEdit-critical ones)

| ImGuiCol_ | Source | Notes |
|---|---|---|
| `PlotLines` | base05 | main waveform line — uses fg color |
| `PlotLinesHovered` | base0D | hover highlight |
| `PlotHistogram` | base0A | effects rack histogram bars |
| `PlotHistogramHovered` | base09 | bars on hover |

### Tables (modern imgui addition; not used by WaveEdit yet)

| ImGuiCol_ | Source | Notes |
|---|---|---|
| `TableHeaderBg` | base01 | |
| `TableBorderStrong` | base02 | |
| `TableBorderLight` | base01 | |
| `TableRowBg` | (transparent) | |
| `TableRowBgAlt` | base01 (alpha 0.30) | zebra striping |

### Drag and drop / navigation

| ImGuiCol_ | Source | Notes |
|---|---|---|
| `DragDropTarget` | base0A | drop zone highlight |
| `NavHighlight` | base0D | keyboard focus indicator |
| `NavWindowingHighlight` | base0D | |
| `NavWindowingDimBg` | base00 (alpha 0.60) | |

### Style vars (same for all themes; hardcoded in `themeApply()`)

| ImGuiStyleVar | Value |
|---|---|
| `WindowRounding` | 4.0 |
| `ChildRounding` | 4.0 |
| `FrameRounding` | 3.0 |
| `PopupRounding` | 4.0 |
| `ScrollbarRounding` | 3.0 |
| `GrabRounding` | 3.0 |
| `TabRounding` | 3.0 |
| `FramePadding` | (8.0, 4.0) |
| `ItemSpacing` | (8.0, 4.0) |
| `ItemInnerSpacing` | (4.0, 4.0) |

**Important: this mapping is a first pass and is expected to be tweaked during interactive validation.** The mapping table lives entirely in a single function in `theme.cpp` so iteration is one-file.

## Alpha rules

Only 5 slots get alpha < 1.0; everything else is fully opaque:

| Slot | Alpha | Reason |
|---|---|---|
| `PopupBg` | 0.92 | subtle popup translucency (the "Zed feel") |
| `ModalWindowDimBg` | 0.60 | modal dim backdrop |
| `NavWindowingDimBg` | 0.60 | windowing dim backdrop |
| `TextSelectedBg` | 0.80 | selection highlight allows text to remain readable |
| `TableRowBgAlt` | 0.30 | zebra striping that doesn't overpower row content |

These are constants in `themeApply()`, not per-theme. Same alpha values applied to every loaded theme.

## FreeType integration

**Definition** added to `src/imconfig_user.h`:
```cpp
#define IMGUI_ENABLE_FREETYPE
```

This enables imgui's built-in FreeType support; the font atlas builder switches from stb_truetype to FreeType automatically. No code changes elsewhere — `AddFontFromFileTTF()` API is identical, fonts just look sharper.

**Source compiled:** `ext/imgui/misc/freetype/imgui_freetype.cpp` added to Makefile SOURCES.

**Link dependency:** libfreetype (system package on all three target platforms — Homebrew on macOS, apt on Linux, MSYS2 on Windows).

**Font loading code in `ui.cpp`** (replaces the existing single-font load):

```cpp
ImGuiIO& io = ImGui::GetIO();
io.Fonts->Clear();

io.FontDefault = io.Fonts->AddFontFromFileTTF(
    "fonts/Inter-Regular.ttf", 14.0f);

fontMono = io.Fonts->AddFontFromFileTTF(
    "fonts/JetBrainsMono-Regular.ttf", 14.0f);
```

`fontMono` is a global `ImFont*` declared `extern` in `WaveEdit.hpp`. **v1 ships with `fontMono` loaded and ready but does not yet apply it to any specific call sites.** Applying it (wrapping slider readouts in `ImGui::PushFont(fontMono); ImGui::Text("%.2f", value); ImGui::PopFont();`) is a deliberate follow-up polish step, deferred to its own future-improvements item. Reasons for deferring: it touches every numeric readout in `ui.cpp` and `import.cpp` (~20 sites), each one a judgment call about whether tabular numerics actually look better in that context, and this sub-project is already big enough.

**Font sizes loaded:** one (14px). Single weight per family. ~4 MB total font atlas texture.

## Built-in themes shipped in `themes/`

Seven curated base16 schemes vendored from [github.com/tinted-theming/base16-schemes](https://github.com/tinted-theming/base16-schemes), all redistributable under permissive licenses:

1. **darcula.yaml** — JetBrains Darcula. **Default on first launch.**
2. **dracula.yaml** — Dracula. Slightly more saturated than Darcula.
3. **nord.yaml** — Nord. Cool blue/gray, very popular.
4. **tokyo-night.yaml** — Tokyo Night. Modern, slightly more colorful.
5. **gruvbox-dark-medium.yaml** — Gruvbox Dark. Warm earth tones.
6. **one-dark.yaml** — Atom's One Dark.
7. **solarized-light.yaml** — Solarized Light, the benchmark light theme.

Adding more is a drag-and-drop operation: download any `.yaml` from the tinted-theming repo, copy to `themes/`, restart WaveEdit.

## `ui.dat` format change

Old format (4 bytes total): `[ int styleId ]`
New format (5+ bytes): `[ uint32_t version=2 ][ uint8_t nameLen ][ char name[nameLen] ]`

Read logic on startup:
- File missing or 0 bytes → use default theme name `"darcula"`
- File exactly 4 bytes long → old format, discard the value, use `"darcula"`
- File ≥ 5 bytes and starts with `version == 2` → new format, parse the name
- Anything else → fall back to `"darcula"`

Resolution: pass the loaded name to `themeByName()`. If it returns -1, fall back to `themeByName("darcula")`. If that also returns -1, use id 0.

Write logic on shutdown: write `version=2`, length-prefixed current theme name. ~25 lines of read+write code total.

## Build system changes

### `Makefile`

Add to `SOURCES`:
```make
ext/imgui/misc/freetype/imgui_freetype.cpp \
src/theme.cpp \
```

(`src/theme.cpp` is also covered by the existing `$(wildcard src/*.cpp)` rule, so the explicit listing is redundant but harmless. Can be omitted.)

Add to `FLAGS` per platform:
- Linux: `$(shell pkg-config --cflags freetype2)`
- macOS: `-I$(shell brew --prefix freetype)/include/freetype2`
- Windows (MINGW64): `-I/mingw64/include/freetype2`

Add to `LDFLAGS` per platform:
- Linux: `$(shell pkg-config --libs freetype2)`
- macOS: `-L$(shell brew --prefix freetype)/lib -lfreetype`
- Windows: `-lfreetype`

Add to **all three** dist targets: copy `themes/` into the distributed bundle alongside `banks/`, `catalog/`, `fonts/`. The current dist targets copy these via `cp -R logo*.png fonts catalog dist/WaveEdit` (Linux), into `Contents/Resources/` on macOS, and similar on Windows. Add `themes` to each of those source lists.

Add to mac `dist` target only (Linux and Windows handle freetype differently — see below):
```make
cp $(shell brew --prefix freetype)/lib/libfreetype.6.dylib dist/WaveEdit/WaveEdit.app/Contents/MacOS
install_name_tool -change $(shell brew --prefix freetype)/lib/libfreetype.6.dylib @executable_path/libfreetype.6.dylib dist/WaveEdit/WaveEdit.app/Contents/MacOS/WaveEdit
```

Linux and Windows `dist` targets need analogous freetype bundling — Linux copies `libfreetype.so.6` from system paths into the dist folder; Windows copies `libfreetype-6.dll` from `/mingw64/bin`. Both will be addressed when the implementation plan walks through the dist targets per platform.

### `dep/Makefile`

Add `freetype` to the `homebrew-deps` target's `brew install` line on macOS. No source-build path for freetype on Linux/Windows; we expect the system/MSYS2 package.

### CI workflow

The CI design spec already accounts for libgtk2.0-dev on Linux and SDL2/libsamplerate/libsndfile via Homebrew on macOS. When the CI sub-project lands, it will need to also install `libfreetype6-dev` on Linux and `mingw-w64-x86_64-freetype` on Windows. Captured as a CI follow-up note in `future-improvements.md`.

## Error handling

| Failure mode | Behavior |
|---|---|
| `themes/` directory missing | Log warning to stderr; fall back to built-in default theme. App runs. |
| Empty `themes/` directory | Same as above. |
| Single `.yaml` file is malformed | Log warning with filename and parser line. Skip that file. Continue loading others. |
| All theme files malformed | Same as missing directory — fall back to built-in default. |
| Theme name in `ui.dat` doesn't match | Fall back to `"darcula"`, then to id 0. |
| FreeType atlas build fails (font file missing/corrupt) | imgui's `AddFontFromFileTTF` returns NULL. We assert in debug, fall back to imgui's built-in `ProggyClean` font in release. App stays usable. |
| libfreetype not installed at runtime | Hard link error at startup. App won't launch. Documented in README as a runtime dependency. |
| User drops a non-base16 YAML in `themes/` (e.g., a Zed theme) | Parse fails (missing `base00..base0F` keys). Logged warning, file skipped. No crash. |

All "log warning" cases write to stderr via `printf`/`fprintf`, matching how the rest of WaveEdit reports issues today. No new logging library.

## Verification approach

No test infrastructure (the project has none). Verification is interactive:

1. **Build verification:** `make clean && make` succeeds locally on macOS arm64. Binary launches.
2. **Theme loading sanity:** at startup, log how many themes were loaded. Should print 7 with the shipped `themes/` intact. Verify by opening the View → Theme submenu.
3. **Theme switching:** click each of the 7 themes in the menu. Confirm the UI redraws with new colors immediately. No artifacts, no crashes.
4. **Persistence:** quit and re-launch. Confirm the chosen theme is still active.
5. **Drop-in test:** download a base16 YAML from tinted-theming's repo (e.g., `catppuccin-mocha.yaml`), copy it into `themes/`, restart, confirm it appears in the menu and applies correctly.
6. **Font rendering:** visually inspect that text is sharp (FreeType) rather than blurry (stb_truetype). The difference should be obvious at 14px.
7. **Mapping iteration:** spend time looking at the actual rendered UI. Tweak the mapping table in `theme.cpp` until the colors feel right for each theme. This is the irreducible "you have to see it" step.

Cross-platform verification (Linux/Windows) is deferred to the CI sub-project — we land themes-and-fonts on macOS arm64 only, and discover any portability issues when CI catches up.

## What this design does NOT decide

- The exact code in each function (that's the implementation plan's job).
- The final mapping table values — the table in this spec is a first-pass mapping, expected to be tweaked during interactive validation.
- What happens to the legacy Lekton fonts (kept in v1, cleaned up later).
- Anything about the in-app theme editor (Tier 1 future improvement).

## Risks

1. **The default mapping might look bad for some themes.** Base16's hue choices are designed for syntax highlighting, and base0A (typically yellow) being applied to histogram bars might clash with some palettes. **Mitigation:** the mapping is one function in one file; iterate after first build.
2. **FreeType might fail to install on some target system.** Mitigation: documented as a runtime requirement. Build fails fast at startup with a clear linker error.
3. **`ui.dat` format change could trip a user upgrading from an old build.** Mitigation: the read code handles the old 4-byte format by discarding it, not crashing. Worst case, a user loses their previously-selected style preference (which was an int that doesn't map to a name anyway).
4. **The 7 built-in themes ship as files, not compiled in.** If `themes/` is missing from a distribution (e.g., a user `make install`s without the data files), only the built-in default works. Mitigation: `make dist` already copies `banks/`, `catalog/`, etc. into the bundle; we add `themes/` to that list.
