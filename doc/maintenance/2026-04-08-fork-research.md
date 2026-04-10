# WaveEdit Fork Research

**Date:** 2026-04-08
**Purpose:** Inventory of WaveEdit forks, identify changes worth consolidating into this maintained fork.
**Status:** Research only — does not commit to any cherry-picks. Decisions belong in subsequent specs.

## Summary

The upstream `AndrewBelt/WaveEdit` is effectively unmaintained — its last commit (`31d5fb2`, 2025-03-02) only removed the discontinued WaveEdit Online code, and is already on this branch. Of ~19 GitHub forks, three contain meaningful divergence; one additional fork has a single notable commit on a non-default branch. None has modern CI, CMake, or runtime-configurable `WAVE_LEN`.

The most valuable single architectural pattern across all forks is **OsirisEdit's runtime `BANK_LEN`** (with a static `MAX_BANK_LEN=64` cap), which is the cleanest template for the configurable-dimensions goal.

The OXIWave / SphereEdit lineage represents a different design philosophy: a hard fork to a 3D `3×3×3=27` wavetable cube with `WAVE_LEN=512` and a rewritten audio engine. These are **incompatible with the OsirisEdit pattern** and represent a real "fork in the road" decision for the eventual configurable-dimensions work.

## Forks with meaningful divergence

### switchupcb/OsirisEdit (Modbap Osiris)
**URL:** https://github.com/switchupcb/OsirisEdit
**Lineage:** Forked from AndrewBelt directly.
**Target hardware:** Modbap Osiris (32-wave wavetables).

| Change | Files | Pull in? | Notes |
|---|---|---|---|
| Runtime `BANK_LEN` (with `MAX_BANK_LEN=64` static cap) | `WaveEdit.hpp:186-187`, `ui.cpp:29`, `bank.cpp`, `wave.cpp` | **Yes — high priority** | Exact pattern needed for configurable bank size. `WAVE_LEN` stays fixed at 256. |
| Dark/Light theme runtime toggle + persistence | `ui.cpp:36`, `ui.cpp:572-590`, `ui.cpp:1232-1499`, `ui.cpp:1512` | **Yes** | `refreshStyle()` function, saved to config file. Foundation for theme expansion. |
| Two logo assets (`logo-light.png`, `logo-dark.png`) | `ui.cpp:1503-1504` | Yes | Trivial. |
| Clipboard array copy/paste (full wavetable) | `wave.cpp:308-344` | Yes | Small, isolated, useful. New methods: `clipboardCopyAll`, `clipboardPasteAll`, `clipboardSetLength`. |
| Wave/Bank `saveWAV()` accepts `bank_len`, `wave_len` parameters | `bank.cpp`, `wave.cpp` | Yes | Necessary companion to runtime `BANK_LEN`. |
| "Bank" → "Wavetable" UI terminology rename | `import.cpp:211`, etc. | Maybe | Cosmetic; clearer for non-E370 users. Decide alongside theme work. |
| `db.cpp` online catalog (libcurl + libjansson) | `db.cpp`, `Makefile` | **No** | Dead `waveeditonline.com`; same code Belt removed in `31d5fb2`. |
| Apple Silicon (`mac_arm64`) target removed; macOS min downgraded 11.0 → 10.7 | `Makefile` | **No** | Would undo this fork's `c6a809b` M1 work. |
| Simplified app-bundle path detection | `main.cpp:28-31` | Maybe | Less robust than current. Test on M1 if pulled. |

### OXI-Instruments/OXIWave (OXI One)
**URL:** https://github.com/OXI-Instruments/OXIWave
**Lineage:** Shares ancestry commits `d95b6b2` and `14a1dd4` with SphereEdit — both descend from a common pre-4ms 3D-cube fork.
**Target hardware:** OXI One (3D 3×3×3 = 27 wavetables, 512-sample tables, 48 kHz).

| Change | Files | Pull in? | Notes |
|---|---|---|---|
| `WAVE_LEN=512`, `BANK_GRID_DIM1/2/3=3`, `SAMPLE_RATE=48000` | `WaveEdit.hpp:128-133` | **Tier 2 / design decision** | Architectural divergence. Conflicts with OsirisEdit pattern. See "Design fork in the road" below. |
| `eucmodi()` / `eucmodf()` euclidean-modulo helpers | `WaveEdit.hpp:26-29, 46-49` | **Yes** | Correct circular-wraparound math. Useful independent of 3D. |
| Trilinear 3D XYZ audio interpolation rewrite | `audio.cpp:35-121` | Tier 2 | Only with 3D bank decision. |
| `playexport.cpp` state isolation during render-to-WAV | `playexport.cpp` (new) | **Yes** | Prevents UI-controlled morphs from glitching during export. Generally useful. |
| `bank.cpp`: `loadWaves()`, `loadMultiWAVs()`, `exportMultiWAVs()` | `bank.cpp:120-183` | **Yes** | Batch WAV import/export workflow. |
| 3D bank-cube widget (`renderBankCube()`) | `widgets.cpp:451-640` | Tier 2 | Tied to 3D bank model. |
| Two ImGui themes ("Black Swan" / "White Swan") | `ui.cpp:31, 938-1043` | Maybe | OsirisEdit's theme system is cleaner; consider as additional palettes. |
| `WINDOWS.md` MSYS2 build guide | `WINDOWS.md` | **Yes (reference)** | Direct input for GitHub Actions Windows job. |
| `db.cpp` (4mscompany.com Spheres API) | `db.cpp` | **No** | Same dead-code situation. |
| Title/branding strings | `ui.cpp` | No | Cosmetic, hardware-specific. |

### danngreen/SphereEdit (4ms Spherical Wavetable Navigator)
**URL:** https://github.com/danngreen/SphereEdit
**Lineage:** Same common ancestor as OXIWave. Both forks reflect the same 3D-cube architecture; SphereEdit is the more polished/documented variant.
**Target hardware:** 4ms SWN (same 3D 3×3×3 architecture as OXI One).

| Change | Files | Pull in? | Notes |
|---|---|---|---|
| `fixWorkingDirectory()` macOS app-translocation workaround | `main.cpp:20-35` | **Yes — high priority** | Real bug fix. Apple's code-signed-quarantine randomizes app dir; this `chdir(dirname(path)); chdir("../Resources")` approach handles both bundled and CLI execution. |
| `dep/Makefile` `$(CURDIR)` instead of `$(PWD)` | `dep/Makefile:1` | **Yes** | mingw compatibility. |
| `wget --no-check-certificate` flags | `dep/Makefile` | **Yes** | Older host compatibility for dependency fetch. |
| Explicit `CC = gcc.exe` for Windows | `Makefile:37` | Yes (reference) | Useful for the GH Actions Windows recipe. |
| Bundled user manual PDF + in-bundle copy for translocation | `doc/`, `Makefile` | Maybe | Real polish for distribution; defer to a "release pipeline" sub-project. |
| `loadWaves()` / multi-WAV functions | `bank.cpp:132-168` | Yes | Same as OXIWave; this is the better-documented variant. |
| `str_ends_with()` utility | `util.cpp:99-111` | Yes | Independently useful. |
| 3D `BANK_GRID_DIM1/2/3` + audio rewrite | `WaveEdit.hpp:187-190`, `audio.cpp:67-97` | Tier 2 | Same conflict as OXIWave. |
| `db.cpp` (4mscompany.com API, `dbInit()` disabled in `main.cpp:86`) | `db.cpp` | **No** | Same dead-code situation. |
| `bkgnd/background-2x.png` | `bkgnd/` | No | Single retina background asset, not a theme system. |
| Removed Apple Silicon detection | `Makefile-arch.inc:8-14` | **No** | Would undo M1 work. |

### joez2103/WaveEditShapeshifter (Intellijel Shapeshifter)
**URL:** https://github.com/joez2103/WaveEditShapeshifter
**Default branch (`master`):** Unchanged from upstream.
**Notable branch:** `cursor/firmware-wavetable-integration-42a5` — single commit `b351776` (2026-01-24, AI-generated by Cursor Agent).
**Status:** Branch only, never merged to master.

| Change | Files | Pull in? | Notes |
|---|---|---|---|
| Firmware blob patching: `Firmware` class + "Export Firmware..." UI | `firmware.hpp`, `firmware.cpp`, `ui.cpp` (+83 lines), `PLAN.md` | **No (reference only)** | Hardware-specific (Shapeshifter offsets `0x100000` for waves, `0x0F000` for bank names). 64 waves × 512 samples × 16-bit LE. Useful as a **pattern reference** for the eventual "pluggable hardware export targets" sub-project. |

## Forks with no meaningful divergence (skip)

| Fork | Status |
|---|---|
| `PierceLBrooks/WaveEdit` | Clean mirror, no divergence. Recent `pushed_at` (2026-01-18) is just a re-fork. |
| `pjx3/waveedit`, `mattfromatlanta/WaveEdit`, `pleprince/WaveEdit` | Identical 2025-03-02 fork operations of upstream HEAD. No divergence. |
| `senarodrigo/WaveEditForPigments` | Description claims Pigments target but `master` HEAD is `f699e1a` from 2018; no Pigments code ever materialized. |
| `krfantasy`, `notagoodidea`, `Sebo1971`, `estmaza`, `coderofsalvation`, `Olnium`, `ifranco`, `DaoTwenty`, `rubyglow`, `nitz`, `apiel`, `IndigoMK`, `dstmu` | Unmodified mirrors. |

## Cross-cutting findings

### Common ancestry (OXIWave ↔ SphereEdit)
Both share commits `d95b6b2` (OSX DMG target) and `14a1dd4`. The git-log subject of `14a1dd4` claims "Restrict freq range to 1.2k to avoid glitchy audio on OSX" but **inspection of the actual code shows the frequency clamp is 10 kHz, identical to upstream**. The commit message is misleading. There is no real frequency-limiter fix to pull in.

### Universal "do not pull"
- Any `db.cpp` (online catalog) — incomplete in all forks; AndrewBelt himself removed this in `31d5fb2`.
- Any Makefile change that removes Apple Silicon support or downgrades macOS deployment target — would regress the M1 modernization already done on this branch.
- Hardware-specific UI branding strings.

### Universal "yes, pull"
- OsirisEdit: runtime `BANK_LEN` + `MAX_BANK_LEN` cap, dark/light theme system, clipboard array.
- SphereEdit: `fixWorkingDirectory()` macOS translocation fix, `$(CURDIR)`, wget cert flag.
- OXIWave: `eucmodi/eucmodf` helpers, `playexport.cpp` state isolation.
- OXIWave/SphereEdit: `loadWaves()` / multi-WAV bank I/O.
- OXIWave: `WINDOWS.md` as reference for GitHub Actions Windows job.

### The design fork in the road
The eventual "make wave/bank dimensions configurable" sub-project must choose between:

- **Path A — OsirisEdit pattern (extended).** Runtime `BANK_LEN` (1..64), runtime `WAVE_LEN` (256/512/1024 with a static `MAX_WAVE_LEN` cap), keep 2D grid. Minimal-change refactor of `float[WAVE_LEN]` stack arrays into bounded heap or `std::array<float, MAX_WAVE_LEN>`. Covers Osiris (32×256), E370 (64×256), Shapeshifter (64×512), and similar 2D-grid hardware.
- **Path B — Generalized 2D/3D.** Add a 3D mode (`BANK_GRID_DIM1/2/3`) and trilinear audio path on top of Path A. Covers SWN/OXI One in addition to everything in Path A. Substantially larger scope; the OXIWave/SphereEdit codebases provide a working reference implementation but in a hard-fork, not-runtime-toggleable form.
- **Path C — Pluggable hardware target plugins.** Extract per-target export/format logic (à la Shapeshifter firmware patcher) into a clean interface, separate from the in-memory model. Orthogonal to A/B and likely worth doing alongside whichever architectural path is chosen.

This decision belongs in its own brainstorming session, not here.

## What this report does NOT decide

- The order or grouping of cherry-picks.
- Whether to rewrite history vs. squash-merge each fork's contributions.
- Whether to adopt the OXIWave/SphereEdit 3D model.
- Anything about CI, build matrix, or theme system internals beyond what was discovered.

Each of those is a separate sub-project to brainstorm in turn.
