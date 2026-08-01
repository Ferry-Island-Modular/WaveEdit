# UI scale support — design

**Date:** 2026-08-01
**Status:** approved (discussed in session; first slice)
**Problem:** WaveEdit renders the ImGui interface at fixed pixel sizes (14 px fonts, default style metrics). On Linux HiDPI desktops running 150–200 % scaling the whole UI appears half size or smaller. macOS is unaffected (Retina handled via SDL's framebuffer scale); Windows is DPI-unaware and gets bitmap-stretched by the OS (right size, slightly blurry).

## Approach

One global UI scale factor, resolved once at startup and changeable at runtime
from the menu bar. imgui v1.92's dynamic font system (`style.FontScaleMain`)
scales fonts without a font-atlas rebuild, so runtime changes are cheap.

### Resolution precedence (highest wins)

1. Manual override persisted in `ui.dat` (menu selection; `0` = Auto)
2. `WAVEEDIT_UI_SCALE` environment variable (escape hatch + CI test hook)
3. `GDK_SCALE` × `GDK_DPI_SCALE` environment variables (GTK convention)
4. System content scale — `ImGui_ImplSDL2_GetContentScaleForWindow()`,
   queried on Linux only (macOS correctly reports 1.0; on Windows the
   process is not DPI-aware, so scaling by the reported DPI would
   double-apply on top of the OS bitmap stretch)
5. Default 1.0

Resolved values are clamped to [0.5, 4.0]; unparseable values are ignored.
SDL2 reports content scale 1.0 on native Wayland — the GDK variables and the
manual override are the fallback there (in practice the app runs under
XWayland, where X11 DPI is available).

### Applying the scale

`applyThemeAndScale()` in ui.cpp (single place, called from uiInit, the Theme
menu, and the UI Scale menu):

1. Reset `ImGui::GetStyle()` to a default-constructed `ImGuiStyle` with
   `ScaleAllSizes(scale)` applied — avoids compounding on repeated calls.
2. Re-apply the current theme (colors + rounding; theme rounding values are
   multiplied by the scale in `themeApply`).
3. Set `style.FontScaleMain = scale`.

Custom widgets that size themselves from available space (waveform editor,
grids) need no changes. Any remaining hardcoded pixel offsets are out of
scope for this slice and can be polished when spotted.

### Menu + persistence

- New "UI Scale" menu (after Theme): Auto, 100 %, 125 %, 150 %, 175 %,
  200 %, 250 %, 300 %. Selection applies immediately, no restart.
- `ui.dat` bumped to v3: v2 layout + trailing `float` manual override
  (0 = Auto). v2 files load with override = Auto.

### Code layout

- `src/uiscale.{hpp,cpp}` — pure resolution logic (no SDL/ImGui includes) +
  scale state. Unit-testable.
- `tests/ui_scale.cpp` — precedence, env parsing, clamping. New
  `make test-ui-scale` target, run on all three CI platforms.
- `src/main.cpp` — feed the SDL content scale in on Linux after window
  creation.
- Startup logs `ui: scale N.NN (source)` to stderr; the Linux CI AppImage
  smoke test gains a second launch with `WAVEEDIT_UI_SCALE=2` asserting
  `ui: scale 2.00`.

## Rejected alternatives

- **Font-atlas rebuild on change** (pre-1.92 pattern): unnecessary with
  `FontScaleMain`.
- **Auto-detect on macOS/Windows too:** macOS needs none; Windows needs the
  SDL DPI-awareness hints first, deferred to its own sub-project. (To be
  added to future-improvements.md after the beta.3 packaging branch — which
  edits the same region of that file — has merged.)
- **Per-monitor / runtime rescale on monitor change:** deferred; startup +
  manual override covers the reported problem.
