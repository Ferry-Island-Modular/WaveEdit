# Submodule Sourcing Audit

**Date:** 2026-04-08
**Status:** Investigation report (no decisions made)
**Purpose:** Audit each `ext/` submodule for upstream health, divergence, and migration cost. Inform the brainstorming of the submodule-upgrade sub-project.

## TL;DR

Of four submodules:

- **Two** (`lodepng`, `osdialog`) are fine and need only a pin bump to current upstream.
- **One** (`pffft`) needs a **URL switch** from the `marton78/pffft` community fork (GitHub) to the canonical `jpommier/pffft` upstream (Bitbucket). Small, mechanical; main consideration is accepting Bitbucket as a hosting dependency.
- **One** — `ext/imgui` — is the real concern: it's pinned to a 2017-era unmaintained Belt fork that's roughly 41 minor versions behind canonical upstream, and migrating will require rewriting the imgui integration glue (~50 lines in `main.cpp`) plus auditing 40+ uses of `imgui_internal.h` APIs in `widgets.cpp` and `tablabels.hpp`. **Realistic cost: 3-5 focused hours of fix-compile-errors + interactive widget validation.**

## Per-submodule findings

### `ext/lodepng` → `lvandeve/lodepng` ✓ FINE

| Field | Value |
|---|---|
| Pinned commit | `8a0f16a` |
| Upstream status | **Alive**. Last push 2026-02-10. 2,312 stars. Not archived. |
| Is a fork? | No — this is the canonical upstream |
| Action | **Bump pin to current `master`.** No code changes needed. |

Lodepng has a stable, dependency-free C++ API. PNG decoding behavior doesn't change between minor versions. Zero migration risk.

### `ext/pffft` → switch from `marton78/pffft` to `jpommier/pffft` (canonical upstream) ✓ FINE

| Field | Value |
|---|---|
| Current submodule URL | `https://github.com/marton78/pffft.git` (community fork) |
| Current pinned commit | `c5062dc` |
| Target submodule URL | **`https://bitbucket.org/jpommier/pffft.git`** (canonical upstream) |
| Upstream status | **Alive**. Most recent commit 2026-01-05 (Julien Pommier). Continuous commit cadence from 2024-11 through 2026-01. Intermittent but actively maintained. |
| Action | **Change submodule URL** to the Bitbucket-hosted canonical upstream and pin a current commit. Update `.gitmodules` accordingly. |

#### Why switch

WaveEdit uses pffft only for single-precision 256-sample RFFT/IRFFT calls in `wave.cpp`. The Makefile passes `-DPFFFT_SIMD_DISABLE` (`Makefile:4`), so we're not even using SIMD codepaths. Every addition that `marton78/pffft` offers over the canonical upstream — double precision, AVX/AVX2, C++ wrapper, `pffastconv`, `pfdsp`, CMake build — is something WaveEdit doesn't use.

The supply-chain principle is "prefer canonical upstream when it's healthy," and jpommier/pffft is healthy. The bug fixes marton78 cherry-picked (MSVC `/fp:strict`, WebAssembly SIMD) have also been merged upstream by Pommier himself — the two forks have effectively converged on the fixes WaveEdit cares about.

#### The one real tradeoff: Bitbucket hosting

Bitbucket is qualitatively less durable as a hosting platform than GitHub. Atlassian killed Mercurial hosting in 2020, has repeatedly changed free-tier terms, and the platform is clearly not a strategic priority. If Bitbucket ever goes paid-only or shuts down, a Bitbucket submodule breaks fresh clones (cached clones keep working).

**Accepted mitigation:** pffft's source is tiny — one `.c`, one `.h`, plus a few SIMD headers. If Bitbucket ever goes dark, we can either (a) pin to a specific commit and mirror it into our own repo, or (b) vendor it outright and drop the submodule. Fallback is minutes of work, not days. The "use canonical upstream" principle outweighs the low-probability hosting-platform risk.

#### Compatibility note

The `RFFT`/`IRFFT` wrapper API WaveEdit uses is stable across both forks. No code changes expected in `wave.cpp`. The only integration point is `Makefile` (`ext/pffft/pffft.c` is listed as a source on line 21) — that path should remain identical under either upstream since both lay out `pffft.c` at the repo root.

### `ext/osdialog` → `AndrewBelt/osdialog` ✓ FINE (surprise)

| Field | Value |
|---|---|
| Pinned commit | `e66caf0` |
| Upstream status | **Alive**. Last push 2025-11-20. Updated 2026-03-07. |
| Is a fork? | **No — this is original work by AndrewBelt**, not a fork-of-anything. Belt wrote osdialog for VCV Rack and continues to maintain it independently of WaveEdit. |
| Action | **Bump pin to current `master`.** Verify the gtk2 / cocoa / win backends still have the same source filenames the Makefile references (`osdialog_gtk2.c`, `osdialog_mac.m`, `osdialog_win.c` per `Makefile:39, 52, 62`). |

This was the biggest surprise of the audit. I had been treating "AndrewBelt-owned submodule" as automatically dead, but osdialog is actively maintained — Belt continues working on it for VCV Rack purposes. The sub-project's framing ("dead Belt forks") was wrong about this one. The submodule URL is correct as-is; we just need to bump the pin.

### `ext/imgui` → `AndrewBelt/imgui` ⚠ THE REAL PROBLEM

| Field | Value |
|---|---|
| Pinned commit | `cfb1dd6` (described by `git submodule status` as `v1.50-304-gcfb1dd6`) |
| Pinned-version meaning | 304 commits past imgui v1.50, which was released late 2016. So the pinned imgui is roughly mid-2017. |
| Upstream status | **Frozen 2017-09-06.** Last push 8.5 years ago. Belt's fork was made for WaveEdit and never updated. |
| Source upstream | `ocornut/imgui` (the canonical) |
| Canonical upstream's current version | **v1.92.7**, released 2026-04-02. Approximately **41 minor versions** of API evolution since the pinned version. |
| Action | **Migrate to upstream `ocornut/imgui` v1.92.7.** This is real work, not a pin bump. See "Migration cost" below. |

#### Migration cost analysis

WaveEdit uses **57 distinct `ImGui::*` API symbols** across 5 source files (601 total call sites). Categorized:

**Stable APIs (no work expected):**
- Window/layout: `Begin`, `End`, `BeginChild`, `EndChild`, `BeginMenu`, `EndMenu`, `BeginMenuBar`, `EndMenuBar`, `BeginPopup`, `EndPopup`, `OpenPopup`, `SetNextWindowPos`, `SetNextWindowSize`, `GetWindowSize`, `GetWindowWidth`, `GetWindowDrawList`, `SameLine`
- Widgets: `Button`, `Checkbox`, `RadioButton`, `SliderFloat`, `Selectable`, `MenuItem`, `Text`, `Image`
- Style/state: `GetIO`, `GetStyle`, `PushID`, `PopID`, `PushStyleVar`, `PopStyleVar`, `PushItemWidth`, `PopItemWidth`, `PushClipRect`, `PopClipRect`, `CalcItemWidth`, `CalcTextSize`, `GetItemRectSize`, `GetMousePos`, `IsItemHovered`, `IsKeyPressed`, `Render`, `RenderFrame`, `SetTooltip`

These are all stable across the entire imgui v1.x line. They will keep working with no code changes. **This covers ~95% of the call sites by volume.**

**Trivially-renamed APIs:**
- `ImGui::ShowTestWindow` → `ImGui::ShowDemoWindow` (renamed in v1.53, 2017). One call site (`ui.cpp:792`). One-line fix.
- `ImGui::ColorConvertU...` → likely stable but names may have evolved slightly. Worth verifying.
- `ImGui::GetColorU` → stable as `GetColorU32`. Probably already correct.

**Internal-API uses (the risky part):**

These are calls into `imgui_internal.h`, which exists in modern imgui but is explicitly unstable — Belt's WaveEdit reaches into it for custom widget hit-testing. The 8 functions used:

| Call | Where | Modern status |
|---|---|---|
| `ImGui::IsHovered(box, id)` | `widgets.cpp` (4 sites) | Internal; signature changed. Modern equivalent is `ItemHoverable(bb, id, item_flags)`. Each site needs review. |
| `ImGui::SetHoveredID(id)` | `widgets.cpp` (4 sites) | Internal; still exists, signature may be stable |
| `ImGui::SetActiveID(id, window)` | `widgets.cpp` (4 sites) | Internal; still exists |
| `ImGui::ClearActiveID()` | `widgets.cpp` (5 sites) | Internal; still exists |
| `ImGui::FocusWindow(window)` | `widgets.cpp` (4 sites) | Internal; still exists |
| `ImGui::ItemAdd(box, NULL)` | `widgets.cpp` (4 sites) | Internal; **signature definitely changed** — modern signature is `ItemAdd(const ImRect& bb, ImGuiID id, const ImRect* nav_bb = NULL, ImGuiItemFlags extra_flags = 0)`. The `NULL` second arg won't compile; needs `0` as the ImGuiID. |
| `ImGui::ItemSize(box, padding_y)` | `widgets.cpp` (4 sites) | Internal; signature may have changed |
| `ImGui::GetCurrentWindow()` | `main.cpp` etc. | Internal; stable |

**Plan:** these are all isolated to `widgets.cpp` (the custom widget implementations: `renderHistogram`, knob widgets, etc.) plus a couple of references in `main.cpp`. Total surface: ~30 call sites in one file. The work is mechanical signature-tweaking, **not architectural rewriting**. Risk: ~half a session of clang errors followed by interactive verification that custom widgets still respond to mouse correctly.

**`TabLabels` (custom extension, refactor opportunity):**

`tablabels.hpp` (27 ImGui-related lines) implements a custom `ImGui::TabLabels(...)` function that doesn't exist in stock imgui. It's used in `ui.cpp:773` to render the page-switcher tabs.

**Modern imgui has stock tab APIs (`BeginTabBar`/`BeginTabItem`/`EndTabItem`/`EndTabBar`) that were added in v1.66 (2018) — *after* WaveEdit's pinned imgui.** The right move is to **delete `tablabels.hpp` entirely** and replace the one call site with stock imgui tab calls. This is a *simplification*, not extra work — we delete custom code in favor of stable upstream code.

**Backend integration glue (`main.cpp`):**

Current code uses:
- `ImGui_ImplSdlGL2_Init`, `ImGui_ImplSdlGL2_NewFrame`, `ImGui_ImplSdlGL2_Shutdown`, `ImGui_ImplSdlGL2_ProcessEvent`, `ImGui_ImplSdlGL2_CreateDeviceObjects`, `ImGui_ImplSdlGL2_InvalidateDeviceObjects`
- These come from `ext/imgui/examples/sdl_opengl2_example/imgui_impl_sdl.cpp` (referenced in `Makefile:26`) — a single combined SDL+OpenGL2 backend file from old imgui.

Modern imgui split this into two separate backend files in `backends/`:
- `imgui_impl_sdl2.cpp` — SDL2-specific event/clipboard/window handling
- `imgui_impl_opengl2.cpp` — OpenGL2 rendering

With renamed functions: `ImGui_ImplSDL2_*` and `ImGui_ImplOpenGL2_*`. The init/event/newframe/render call sequence is the same shape, but each call site needs to be doubled (one SDL call + one OpenGL call where there used to be one combined call) and renamed.

**Plan:** rewrite the ~30 lines of integration glue in `main.cpp`. Mechanical, well-documented in imgui's own examples (`backends/imgui_impl_sdl2.cpp` has copy-pasteable example usage at the top of the file). Update `Makefile:26` to reference the two new backend files instead of the one old example file.

**Key handling:**

In imgui v1.87 (2022), the `ImGuiKey_*` enum was overhauled — old key constants became deprecated, and there's now a "use native scancodes" mode. WaveEdit uses `ImGui::IsKeyPressed(...)` in a few places. If those calls pass SDL scancodes or numeric keycodes, they need to be updated to use `ImGuiKey_*` enum values, OR the SDL2 backend needs to be configured in legacy keycode mode (which still works, just deprecated).

Sites using `IsKeyPressed`: needs grep, but likely small (~5 sites).

#### Recommended target

**Pin `ext/imgui` to `ocornut/imgui` tag `v1.92.7` (2026-04-02).**

Reasoning:
- Most recent stable release at audit time
- Tagged release, not HEAD — predictable, won't drift under us
- Imgui maintains good backwards compatibility within the v1.x line; future minor bumps will be cheap once we're on a recent baseline

#### Realistic effort estimate

| Task | Effort |
|---|---|
| Update `.gitmodules` URL + bump pin | 5 minutes |
| Rewrite `main.cpp` backend glue | 30-60 minutes |
| Update `Makefile:26` to point at `backends/imgui_impl_sdl2.cpp` + `backends/imgui_impl_opengl2.cpp` | 10 minutes |
| Fix `ShowTestWindow` → `ShowDemoWindow` | 1 minute |
| Audit + fix `widgets.cpp` internal-API calls | 1-2 hours of "compile, fix error, repeat" |
| Replace `TabLabels` with stock `BeginTabBar` | 30-60 minutes (and deletes `tablabels.hpp`) |
| Audit + fix `IsKeyPressed` callers if affected | 15-30 minutes |
| Interactive validation: launch WaveEdit, test all custom widgets respond to mouse + keyboard correctly | 30-60 minutes (this is the irreducible part) |
| **Total** | **~3-5 focused hours** |

This is a one-day sub-project, not a multi-day epic. The "interactive validation" step is the only part that can't be hurried.

## Summary recommendation

| Submodule | Action | Risk | Effort |
|---|---|---|---|
| `ext/lodepng` | Bump pin | None | 5 min |
| `ext/pffft` | **Switch URL** from `marton78/pffft` (GitHub) to canonical `jpommier/pffft` (Bitbucket); pin current commit | Low (Bitbucket hosting durability) | 10 min |
| `ext/osdialog` | Bump pin | Low (verify backend filenames stable) | 15 min |
| `ext/imgui` | Migrate URL + version, rewrite glue, audit internal API uses | Medium | ~3-5 hours |

The right brainstorming question for the next session is **not** "should we do this" but "in what order and at what verification checkpoints." That's a writing-plans question, not a brainstorming question. I'd suggest going straight from this audit to the implementation plan once you've reviewed and approved.

## What this audit does NOT decide

- The exact step ordering (that's the implementation plan's job)
- Whether to update other things (Makefile cleanup, dep upgrades) at the same time — answer should be **no**, keep the change focused
- Whether to also re-vendor any of these into the main repo to drop the submodule machinery entirely — separate question, deferred
- Anything about the CI sub-project, which is unblocked by this work but otherwise unchanged
