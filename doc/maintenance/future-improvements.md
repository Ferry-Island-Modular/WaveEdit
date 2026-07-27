# Future Improvements Roadmap

**Last updated:** 2026-07-26
**Purpose:** Running index of sub-projects and improvements that have been **identified but explicitly deferred** during planning of other work. Each item here is a potential future brainstorm session — not a commitment.

## Completed sub-projects (for reference)

These were previously listed here but have been implemented:

- **FreeType font rendering** — DONE (2026-04-09, `themes-and-fonts` branch). Inter (UI) + JetBrains Mono (numerics) via `IMGUI_ENABLE_FREETYPE`. Lekton fonts deleted.
- **Runtime theme system with base16 YAML loader** — DONE (2026-04-09, `themes-and-fonts` branch). 8 built-in schemes, drop-in user themes from tinted-theming, Theme menu in menu bar, persistence in ui.dat.
- **Initial submodule modernization** — DONE (2026-04-09, `m1-modernization` branch). imgui→ocornut v1.92.7, pffft→jpommier, and lodepng bumped; osdialog was deliberately held for the later Zenity sub-project.
- **Zenity dialog backend for Linux** — DONE (2026-07-26, `linux-zenity-dialogs` branch). Bumped osdialog to current upstream, adopted its shared source, replaced the GTK2 backend with Zenity, and removed the GTK2 build dependency.
- **AppImage packaging for Linux** — DONE (2026-07-26, `linux-zenity-dialogs` branch). Added a linuxdeploy-based AppImage target, desktop integration metadata, dependency bundling, and a headless CI smoke test.

Items are grouped by theme, not by priority. Priorities will be decided at the time a given sub-project is started.

---

## UI / visual modernization (unlocked by imgui v1.92 migration)

The submodule upgrade migrates `ext/imgui` from a 2017-era snapshot to current upstream. Once landed, it unlocks a large amount of modern imgui capability that wasn't available before. This roadmap is pickable in three independent tiers.

### Tier 1 — low risk, high polish ("make it look modern")

1. **Replace `tablabels.hpp` with native `BeginTabBar`/`BeginTabItem`.** The stock imgui tab API was added in v1.66 (2018). Gives: closeable tabs, reorderable tabs by drag, keyboard navigation, theme-consistent styling. Small visual upgrade, bigger UX upgrade.
2. ~~**Enable FreeType font rendering**~~ — **DONE** (see "Completed sub-projects" above).
3. **Add an icon font** (Font Awesome Free or Material Design Icons via an icon-font header). Gives the effects rack and toolbars scannable icons instead of text-only buttons. No new runtime dependencies — just a font file and a header of Unicode code points.
4. ~~**Expanded theme system**~~ — **DONE** (see "Completed sub-projects" above). Remaining follow-up: in-app theme editor with live preview using `ImGui::ShowStyleEditor()` — the data layer exists, this would add a polished UI on top.
4a. **Split-label/value typography for sliders.** v1 wraps each whole `SliderFloat` in `PushFont(fontMono)`, rendering both label and value in monospace. The "gold-standard" approach is to refactor each into `Text(label)` + `SliderFloat(value-only-format)` so labels stay in proportional Inter and only numeric values go mono. ~15 call sites, each a small refactor.
4b. **Per-theme font choice.** Themes currently control only colors, alpha, and style vars — not fonts. Adding per-theme fonts would require font-atlas rebuilding at runtime.
4c. **Multiple font sizes / bold+italic variants.** Currently single Regular weight at 14px per family. Adding more is straightforward (`AddFontFromFileTTF` per size/weight) but grows the font atlas texture.

### Tier 2 — medium risk, medium payoff ("use stock widgets where custom was overkill")

5. **Tables API for layouts.** Replace ad-hoc custom layouts (catalog browser, import page wave preview grid, bank overview) with stock `BeginTable`/`EndTable`. Gains: sortable columns, resizable columns, scrollable regions, consistent styling.
6. **Docking mode** (`ImGuiConfigFlags_DockingEnable`). Lets users drag panels out of the main window into floating OS windows, dock them together, save/load layouts. Would fundamentally change how WaveEdit feels — simultaneous view of edit/grid/waterfall instead of tab switching. Requires layouts to be persisted to `ui.dat`.
7. **Improved keyboard navigation.** Modern imgui has significantly better tab-order and arrow-key navigation built into every stock widget. Most of this comes for free once Tier 2 #5 and #6 are done.
8. **`ButtonBehavior()` refactor** of custom widgets in `widgets.cpp`. The current `editorBehavior()` helper reaches into `imgui_internal.h` for hit-testing; stock `ButtonBehavior()` encapsulates all of that with a cleaner API. Pure cleanup — no user-visible change — but removes technical debt.

### Tier 3 — bigger project, biggest visual impact ("make it feel like a 2025 audio tool")

9. **ImPlot integration.** ImPlot (`epezent/implot`) is a third-party plotting library built on imgui. Gives: labeled axes with units, zoomable/pannable plots, multi-series overlays, heatmaps (useful for spectrum), surface plots (useful for waterfall), legends. WaveEdit's custom `renderWave`/`renderHistogram`/`renderWaterfall` could be replaced or augmented by ImPlot for a dramatic visual upgrade. New submodule dependency. This is the big one.
10. **Proper Fourier / spectrogram view.** WaveEdit has FFT infrastructure (`pffft`, `wave.cpp` spectral code) but the visualization is basic. Modern DSP expectations include a proper spectrogram heatmap, harmonic decomposition view, and per-harmonic editing. Builds on #9.

---

## Build / release / distribution

### Deferred from the CI design spec (`2026-04-08-ci-build-design.md`)

1. **macOS universal binary.** Single `.app` that runs natively on both Intel and Apple Silicon. Currently impossible because Homebrew can't provide both arches side-by-side. Requires switching macOS dep handling from Homebrew to source-built via `dep/Makefile` (same approach Linux/Windows use), then compiling with `-arch arm64 -arch x86_64`. Would restore Intel mac to the CI matrix without paying for `-large` runners.
2. **Code-signing and notarization.** macOS requires an Apple Developer account ($99/year) plus secrets in GitHub Actions. Windows code-signing requires a cert and provisioning. Both are money + bureaucracy commitments. Payoff: users don't hit Gatekeeper / SmartScreen warnings on first launch.
3. **CMake migration.** Larger refactor of the build system. Would enable MSVC Windows builds, simplify CI, and make the project more approachable to new contributors. No immediate user-visible benefit; defer until a concrete trigger (usually MSVC for codesigning, or cross-compilation for universal binary).
4. **MSVC build for Windows.** Cleaner Windows ABI, no MinGW runtime DLLs to ship, plays nicer with Windows codesigning. Requires either CMake or a `.sln` rewrite plus vcpkg for deps. Real risk of a long "fix the GCC-isms" debugging tail.
5. **Broader smoke tests in CI.** Linux now launches the AppImage under Xvfb with dummy audio and verifies that bundled resources load. Interaction-level coverage—opening dialogs, editing waves, exercising audio, and clean shutdown—still needs a test harness. Separate "make WaveEdit testable" sub-project.

### Supply chain

6. **Vendor the submodules or switch to upstream-only.** Post-upgrade, `ext/imgui`, `ext/lodepng`, `ext/pffft` all point at third-party upstreams (one of which is on Bitbucket). Supply chain hardening options: vendor each into the main repo and drop the submodule machinery entirely, or maintain mirror forks under the same account as the WaveEdit fork itself. Decision point: how much do we value the "clean `git clone` just works with no submodule fetch" experience vs the "we're tracking upstream" experience.

---

## Core architecture / hardware support

### Configurable wave/bank dimensions (the big architectural one)

The original project brief called for making sample number and wavetable shape configurable (Four Seas, Osiris, SWN, Oxi, etc.). Fork research showed two competing precedents:

- **Path A — OsirisEdit pattern.** Runtime `BANK_LEN` with static `MAX_BANK_LEN=64` cap. Keeps `WAVE_LEN=256` and 2D grid. Minimal-change refactor. Covers Osiris (32×256), E370 (64×256). Does not cover SWN/OXI (3D) or 512-sample tables.
- **Path B — Generalized 2D/3D.** Runtime `WAVE_LEN` + runtime `BANK_LEN` + optional 3D grid. Adds trilinear audio path. Covers everything but is a substantial refactor of `float[WAVE_LEN]` stack arrays throughout the codebase.
- **Path C — Pluggable hardware target plugins.** Extract per-target export/format logic into a clean interface. Orthogonal to A/B. Borrows ideas from WaveEditShapeshifter's firmware-blob patching approach.

**Next step when this gets picked up:** brainstorming session to choose A/B/C (or hybrid) and scope the refactor. Prerequisite: clean baseline (done by submodule upgrade + CI).

### Thread-safety and TODO cleanup

Known issues flagged in the original code audit:

- `wave.cpp:198` — "TODO Fix possible race condition with audio thread here"
- `ui.cpp:648` — incomplete histogram hover behavior
- `widgets.cpp:134` — incomplete line-tool undo-history integration
- `audio.cpp:141` — "TODO Be more tolerant of devices which can't use floats or 1 channel"
- `wave.cpp:141` — "TODO Maybe change this into a more musical filter"
- `wave.cpp:155` — "TODO Consider removing because Normalize does this for you"
- `ui.cpp:363` — `// HACK` comment
- `widgets.cpp:63` — bare `// TODO`

Small, bounded cleanup sub-project. Good confidence-builder after the larger architectural work.

### Pluggable hardware export targets

Borrowed pattern from `joez2103/WaveEditShapeshifter`'s `Firmware` class: patch hardware firmware blobs with wavetables at fixed offsets. Current WaveEdit writes `.dat` files and `.wav` files; a `Firmware`-like interface would let users export directly to target hardware's binary format. Candidates: Intellijel Shapeshifter, any future hardware with a documented wavetable offset.

Scope: interface design + at least one reference implementation. Can be split per-target: start with Shapeshifter (pattern already exists upstream), add others as users ask.

---

## Fork consolidation leftovers

From `2026-04-08-fork-research.md`, items not yet pulled in:

- **OsirisEdit clipboard array copy/paste.** Small, isolated enhancement. `wave.cpp` new methods: `clipboardCopyAll`, `clipboardPasteAll`, `clipboardSetLength`.
- **OXIWave `eucmodi()` / `eucmodf()` helpers.** Correct circular-wraparound math. Useful independent of the 3D stuff.
- **OXIWave `playexport.cpp` state isolation.** Prevents UI-controlled morphs from glitching during render-to-WAV export. Generally useful.
- **OXIWave / SphereEdit `loadWaves()` / `loadMultiWAVs()` / `exportMultiWAVs()`.** Batch WAV import/export workflow in `bank.cpp`. Useful for hardware preset distribution.
- **OsirisEdit "Bank" → "Wavetable" UI terminology rename.** Cosmetic; clearer for non-E370 users. Decide alongside theme work.
- ~~**OsirisEdit dark/light theme system.**~~ Superseded by the base16 theme system (implemented 2026-04-09). OsirisEdit's dark/light toggle was the seed inspiration but the final implementation went much further with 8 base16 schemes + drop-in support.

None of these are urgent; all can be cherry-picked individually in a dedicated fork-consolidation sub-project.

---

## How to use this document

- When finishing a sub-project, check if any newly-completed work unblocks items here. If so, note it inline.
- When starting a new brainstorming session, skim this list to see if the proposed work overlaps with anything already captured.
- **Do not treat items here as a TODO list.** They are options, not commitments. Any item can be dropped if priorities shift.
- When an item is picked up as an active sub-project, move it out of this file and into its own spec + plan.
