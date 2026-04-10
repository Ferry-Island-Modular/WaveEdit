# Submodule Upgrade Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace WaveEdit's four `ext/` git submodules with current canonical upstream sources, eliminating the dependency on 8-year-old unmaintained Belt forks. Produce a working, interactively-verified build at the end.

**Architecture:** Each submodule is updated in isolation with a build-verification commit between changes, so any breakage can be bisected to a single commit. The `ext/imgui` migration is the only substantive code change and gets its own multi-commit sub-sequence. All work happens on a dedicated feature branch to keep `m1-modernization` clean.

**Tech Stack:** GNU Make, git submodules, C++11, SDL2, OpenGL 2, Dear ImGui, SDL2/libsamplerate/libsndfile (Homebrew on macOS).

**Predecessor:** `2026-04-08-submodule-audit.md`
**Blocks:** `2026-04-08-ci-build-design.md` (CI should be built against modernized submodules)

---

## Important notes for the implementing engineer

1. **This project has no test infrastructure.** There are no unit tests, integration tests, or automated smoke tests. The verification pattern is:
   - After each mechanical change: `make clean && make` must succeed
   - After imgui migration: launch `./WaveEdit` and manually interact with the GUI
   - Commits happen after verification succeeds, not after TDD cycles
2. **Keep commits focused.** One submodule per commit for the small ones. The imgui migration is broken into multiple commits inside Task 5 because its scope is larger.
3. **All work is on macOS arm64** (user's daily driver). Linux and Windows will be verified later via CI (separate sub-project).
4. **Do not skip the interactive validation step in Task 5.** It is the only way to catch silent behavior regressions in the custom widgets. Budget 30-60 minutes for it.
5. **If a step fails:** do not force-push, do not `git reset --hard`, do not skip ahead. Stop, investigate, and consult the audit document for context. Each task is designed to be revertable with a single `git revert`.

## Verification commands reference

These commands are used throughout the plan. Definitions here so each step can reference them by name.

**`VERIFY_BUILD`:**
```bash
make clean
make 2>&1 | tee /tmp/waveedit-build.log
echo "Build exit: $?"
```
Expected: exit code 0, produces `WaveEdit` (or `WaveEdit.exe` on Windows) in the repo root.

**`VERIFY_LAUNCH`:**
```bash
./WaveEdit &
WAVEEDIT_PID=$!
sleep 3
if kill -0 $WAVEEDIT_PID 2>/dev/null; then
  echo "PASS: WaveEdit still running after 3s"
  kill $WAVEEDIT_PID
else
  echo "FAIL: WaveEdit exited within 3s"
  exit 1
fi
```
Expected: `PASS: WaveEdit still running after 3s`. Kills the launched instance cleanly.

**`VERIFY_INTERACTIVE`** (manual, Task 5 only):
Launch `./WaveEdit` and confirm each of these by direct interaction:
1. Window opens without crash or visible error
2. Top menu bar is clickable; File menu opens
3. All tabs at the top (Edit, Effects, Grid View, Waterfall, Spectrum, Import, Audio Output) switch when clicked
4. On the "Edit" tab: click-and-drag on the main wave display draws a visible change
5. On the "Edit" tab: each effect knob can be dragged to change its value
6. On the "Grid View" tab: clicking a cell in the bank grid changes the selection highlight
7. On the "Waterfall" tab: clicking changes the active wave index
8. Keyboard shortcuts work: press `Cmd+Z` (macOS), undo should succeed; press the number keys `1`-`5`, the active tool should change
9. Close the window via the window's close button — program exits cleanly (no crash, no hang)

If any of these fail, stop and investigate before continuing.

---

## Task 1: Create feature branch and baseline verification

**Files:** none (git operations only)

- [ ] **Step 1.1: Confirm clean working tree**

Run:
```bash
cd /Users/jgoney/dev/WaveEdit_clones/WaveEdit
git status
```
Expected: no staged or unstaged changes to tracked files. Untracked files like `autosave.dat`, `dep/include/`, `dep/lib/`, `dist/`, `ui.dat`, `WaveEdit`, `.vscode/`, `CLAUDE.md` are fine — they are build artifacts and local tooling. Do NOT add or commit those; they already are not tracked.

If there ARE modifications to tracked files, stop and ask the user what they want done with them before proceeding.

- [ ] **Step 1.2: Create the feature branch**

Run:
```bash
git checkout m1-modernization
git pull origin m1-modernization 2>/dev/null || echo "(no remote to pull from, or nothing to pull)"
git checkout -b submodule-upgrade
```
Expected: new branch `submodule-upgrade` created and checked out.

- [ ] **Step 1.3: Baseline build verification**

Run the `VERIFY_BUILD` command from the reference section above.

Expected: exit code 0, `WaveEdit` binary exists in repo root. This confirms the **current** state builds cleanly before we touch anything. If this fails, stop — the baseline is broken and must be fixed before submodule work can proceed.

- [ ] **Step 1.4: Baseline launch verification**

Run the `VERIFY_LAUNCH` command from the reference section above.

Expected: `PASS: WaveEdit still running after 3s`. If this fails, stop — the current binary is broken at launch time.

- [ ] **Step 1.5: No commit for this task**

This task only creates a branch and verifies baseline state. No code changes, no commit. Proceed directly to Task 2.

---

## Task 2: Bump `ext/lodepng` pin to current upstream

**Files:**
- Modify: `ext/lodepng` (submodule pin)
- Modify: `.gitmodules` — NO CHANGE. URL already points to `https://github.com/lvandeve/lodepng.git` which is canonical.

- [ ] **Step 2.1: Confirm the current URL is correct**

Run:
```bash
grep -A1 'submodule "ext/lodepng"' .gitmodules
```
Expected output:
```
[submodule "ext/lodepng"]
	path = ext/lodepng
	url = https://github.com/lvandeve/lodepng.git
```
If the URL is different, stop and ask — the audit assumed this was already canonical.

- [ ] **Step 2.2: Fetch latest upstream**

Run:
```bash
cd ext/lodepng
git fetch origin
git log --oneline -5 origin/master
cd ../..
```
Expected: a list of 5 recent upstream commits on `lvandeve/lodepng` master. Note the newest commit SHA for reference.

- [ ] **Step 2.3: Bump the pin to origin/master**

Run:
```bash
cd ext/lodepng
git checkout origin/master
NEW_SHA=$(git rev-parse HEAD)
echo "New lodepng pin: $NEW_SHA"
cd ../..
git add ext/lodepng
```
Expected: `git status` now shows `modified: ext/lodepng (new commits)`.

- [ ] **Step 2.4: Verify the build still works**

Run the `VERIFY_BUILD` command.

Expected: exit code 0. Lodepng has a stable C++ API; no breakage expected.

- [ ] **Step 2.5: Verify launch still works**

Run the `VERIFY_LAUNCH` command.

Expected: `PASS`.

- [ ] **Step 2.6: Commit**

Run:
```bash
git commit -m "ext/lodepng: bump to current upstream

Per doc/maintenance/2026-04-08-submodule-audit.md, lodepng
submodule URL already points to the canonical upstream
(lvandeve/lodepng). This commit bumps the pin from the
ancient snapshot (8a0f16a) to current master.

No code changes expected; lodepng's C++ API is stable across
the versions in scope."
```

---

## Task 3: Bump `ext/osdialog` pin to current upstream

**Files:**
- Modify: `ext/osdialog` (submodule pin)
- Modify: `.gitmodules` — NO CHANGE. URL points to `https://github.com/AndrewBelt/osdialog.git` which is Belt's own *actively maintained* project (confirmed in the audit — it is original work, not a fork-of-something).

- [ ] **Step 3.1: Confirm the current URL is correct**

Run:
```bash
grep -A1 'submodule "ext/osdialog"' .gitmodules
```
Expected output:
```
[submodule "ext/osdialog"]
	path = ext/osdialog
	url = https://github.com/AndrewBelt/osdialog.git
```

- [ ] **Step 3.2: Fetch latest upstream and inspect backend filenames**

Run:
```bash
cd ext/osdialog
git fetch origin
ls *.c *.m 2>/dev/null
git checkout origin/master
ls *.c *.m 2>/dev/null
cd ../..
```
Expected: both listings should include `osdialog_gtk2.c`, `osdialog_mac.m`, `osdialog_win.c` (the three filenames referenced by `Makefile:39`, `Makefile:52`, `Makefile:62`). If any of these filenames has been renamed in upstream (e.g., `osdialog_gtk3.c` added alongside), DO NOT panic — WaveEdit's Makefile still references the exact filenames above, and as long as those files still exist, we're fine.

If `osdialog_gtk2.c`, `osdialog_mac.m`, or `osdialog_win.c` no longer exists at HEAD, stop and investigate — we'd need to update the Makefile, and that's a separate decision.

- [ ] **Step 3.3: Bump the pin**

Run:
```bash
cd ext/osdialog
# already on origin/master from previous step
NEW_SHA=$(git rev-parse HEAD)
echo "New osdialog pin: $NEW_SHA"
cd ../..
git add ext/osdialog
```

- [ ] **Step 3.4: Verify the build**

Run the `VERIFY_BUILD` command.

Expected: exit code 0. If compile errors mention `osdialog_*`, consult the audit's osdialog section — there may be small API changes worth cross-referencing against upstream.

- [ ] **Step 3.5: Verify launch**

Run the `VERIFY_LAUNCH` command.

Expected: `PASS`.

- [ ] **Step 3.6: Commit**

Run:
```bash
git commit -m "ext/osdialog: bump to current upstream

Per doc/maintenance/2026-04-08-submodule-audit.md, AndrewBelt/osdialog
is Belt's own actively maintained project (not a dead fork), and the
submodule URL is correct. This commit bumps the pin from the old
snapshot (e66caf0) to current master.

The Makefile references osdialog_gtk2.c, osdialog_mac.m, and
osdialog_win.c, all of which still exist upstream."
```

---

## Task 4: Switch `ext/pffft` from `marton78/pffft` to canonical `jpommier/pffft`

**Files:**
- Modify: `.gitmodules:9-11` (submodule URL change from GitHub fork to canonical Bitbucket upstream)
- Modify: `ext/pffft` (repoint and re-pin)

- [ ] **Step 4.1: Read the current `.gitmodules` pffft block**

Run:
```bash
grep -n -A2 'submodule "ext/pffft"' .gitmodules
```
Expected output (the two lines after the section header):
```
[submodule "ext/pffft"]
	path = ext/pffft
	url = https://github.com/marton78/pffft.git
```

- [ ] **Step 4.2: Edit `.gitmodules` to use canonical upstream**

Use the Edit tool to change the pffft URL in `.gitmodules` from `https://github.com/marton78/pffft.git` to `https://bitbucket.org/jpommier/pffft.git`.

After editing, verify:
```bash
grep -A2 'submodule "ext/pffft"' .gitmodules
```
Expected:
```
[submodule "ext/pffft"]
	path = ext/pffft
	url = https://bitbucket.org/jpommier/pffft.git
```

- [ ] **Step 4.3: Sync and deinit the old submodule**

Run:
```bash
git submodule sync ext/pffft
git submodule deinit -f ext/pffft
rm -rf .git/modules/ext/pffft
```
Expected: no errors. This clears the old `marton78` clone from `.git/modules/` so the next init uses the new URL.

- [ ] **Step 4.4: Re-init and fetch from the new URL**

Run:
```bash
git submodule update --init ext/pffft
cd ext/pffft
git log --oneline -5
cd ../..
```
Expected: recent commits from `jpommier/pffft` (not `marton78/pffft`). You should see commits authored by "Julien Pommier" as the most recent. If you see "marton78" commits, the sync didn't take — redo Step 4.3.

- [ ] **Step 4.5: Pin to a current commit**

Run:
```bash
cd ext/pffft
git fetch origin
git checkout origin/master
NEW_SHA=$(git rev-parse HEAD)
echo "New pffft pin (jpommier): $NEW_SHA"
cd ../..
git add ext/pffft .gitmodules
```

- [ ] **Step 4.6: Verify the build**

Run the `VERIFY_BUILD` command.

Expected: exit code 0. The `pffft.c` file is at the repo root in both forks, so `Makefile:21` (`ext/pffft/pffft.c`) works unchanged. WaveEdit uses only `pffft_new_setup`, `pffft_transform_ordered`, `pffft_destroy_setup` via the `RFFT`/`IRFFT` wrappers in `wave.cpp` — these are stable.

If the build fails, check `/tmp/waveedit-build.log` for the specific error. A plausible failure mode: jpommier's upstream has a different header layout or filename. Investigate before retrying.

- [ ] **Step 4.7: Verify launch**

Run the `VERIFY_LAUNCH` command.

Expected: `PASS`.

- [ ] **Step 4.8: Commit**

Run:
```bash
git commit -m "ext/pffft: switch to canonical jpommier upstream

Per doc/maintenance/2026-04-08-submodule-audit.md, switch from
the marton78/pffft GitHub community fork to the canonical
jpommier/pffft Bitbucket upstream. WaveEdit only uses the
minimal float RFFT/IRFFT surface, so none of marton78's
additions (double precision, AVX, pffastconv, C++ wrapper)
are relevant.

Known tradeoff: depending on Bitbucket for a submodule. If
Bitbucket ever becomes unavailable, pffft is a 3-file library
that can be vendored into ext/ directly with minimal effort."
```

---

## Task 5: Migrate `ext/imgui` from `AndrewBelt/imgui` (2017) to `ocornut/imgui` v1.92.7

**This is the big one.** It is broken into sub-tasks because it involves code changes in `main.cpp`, `src/widgets.cpp`, and `src/ui.cpp`, plus Makefile edits. Each sub-task has its own verification.

**Files touched across this task:**
- Modify: `.gitmodules` (imgui URL change)
- Modify: `ext/imgui` (repoint and re-pin to v1.92.7 tag)
- Modify: `Makefile:23-26` (backend file references)
- Modify: `src/main.cpp:1-151` (backend integration glue — several locations)
- Modify: `src/widgets.cpp` (internal-API signature updates in 4 custom widget functions)
- Modify: `src/ui.cpp:244-298` (key handling: `SDLK_*` / `SDL_SCANCODE_*` → `ImGuiKey_*`)

### Task 5a: Switch `ext/imgui` URL and pin to v1.92.7

- [ ] **Step 5a.1: Edit `.gitmodules`**

Use the Edit tool to change the imgui URL in `.gitmodules` from `https://github.com/AndrewBelt/imgui.git` to `https://github.com/ocornut/imgui.git`.

After editing, verify:
```bash
grep -A2 'submodule "ext/imgui"' .gitmodules
```
Expected:
```
[submodule "ext/imgui"]
	path = ext/imgui
	url = https://github.com/ocornut/imgui.git
```

- [ ] **Step 5a.2: Deinit old imgui submodule**

Run:
```bash
git submodule sync ext/imgui
git submodule deinit -f ext/imgui
rm -rf .git/modules/ext/imgui
```

- [ ] **Step 5a.3: Re-init and pin to v1.92.7**

Run:
```bash
git submodule update --init ext/imgui
cd ext/imgui
git fetch origin --tags
git checkout v1.92.7
git rev-parse HEAD
cd ../..
git add ext/imgui .gitmodules
```
Expected: the HEAD SHA of `ext/imgui` now matches `ocornut/imgui`'s v1.92.7 tag. Note the SHA for the eventual commit message.

- [ ] **Step 5a.4: Do NOT try to build yet**

The build will fail catastrophically because Makefile references old backend paths, widgets.cpp uses old internal APIs, etc. Skip verification for 5a and proceed directly to 5b.

### Task 5b: Update Makefile to reference modern imgui backend paths

**Files:**
- Modify: `Makefile:23-26`

- [ ] **Step 5b.1: Read the current SOURCES list**

Look at `Makefile:20-27`. The current content is:
```make
SOURCES = \
	ext/pffft/pffft.c \
	ext/lodepng/lodepng.cpp \
	ext/imgui/imgui.cpp \
	ext/imgui/imgui_draw.cpp \
	ext/imgui/imgui_demo.cpp \
	ext/imgui/examples/sdl_opengl2_example/imgui_impl_sdl.cpp \
	$(wildcard src/*.cpp)
```

- [ ] **Step 5b.2: Edit the SOURCES list**

Use the Edit tool to replace the line:
```
	ext/imgui/examples/sdl_opengl2_example/imgui_impl_sdl.cpp \
```
with these two lines:
```
	ext/imgui/backends/imgui_impl_sdl2.cpp \
	ext/imgui/backends/imgui_impl_opengl2.cpp \
```

Also add modern imgui sources that may be needed. Modern imgui split some code out of `imgui.cpp` and `imgui_draw.cpp` into additional translation units. After the Edit, the SOURCES list should look like:

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

The `imgui_tables.cpp` and `imgui_widgets.cpp` files were added to imgui around v1.80 and are required for the main library to link in modern versions.

- [ ] **Step 5b.3: Verify the new source files exist in the submodule**

Run:
```bash
ls ext/imgui/backends/imgui_impl_sdl2.cpp \
   ext/imgui/backends/imgui_impl_opengl2.cpp \
   ext/imgui/imgui_tables.cpp \
   ext/imgui/imgui_widgets.cpp
```
Expected: all four files listed without "No such file" errors.

- [ ] **Step 5b.4: Do NOT try to build yet.** Proceed to 5c.

### Task 5c: Rewrite `main.cpp` backend integration glue

**Files:**
- Modify: `src/main.cpp:8-10` (imgui includes)
- Modify: `src/main.cpp:82` (init)
- Modify: `src/main.cpp:98` (event processing)
- Modify: `src/main.cpp:125` (new frame)
- Modify: `src/main.cpp:136-139` (render)
- Modify: `src/main.cpp:146` (shutdown)

- [ ] **Step 5c.1: Replace the imgui include block**

In `src/main.cpp`, find lines 8-10:
```cpp
#include "imconfig.h"
#include "imgui.h"
#include "imgui/examples/sdl_opengl2_example/imgui_impl_sdl.h"
```

Replace with:
```cpp
#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_opengl2.h"
```

Note: `imconfig.h` is still pulled in automatically by `imgui.h`; we don't need to include it separately. The `backends/` prefix on the two backend headers is relative to the `-Iext/imgui` include path that `Makefile:5` already provides.

- [ ] **Step 5c.2: Replace init call**

Find `src/main.cpp:81-82`:
```cpp
	// Set up Imgui binding
	ImGui_ImplSdlGL2_Init(window);
```

Replace with:
```cpp
	// Set up ImGui context and binding
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	ImGui_ImplSDL2_InitForOpenGL(window, glContext);
	ImGui_ImplOpenGL2_Init();
```

`glContext` is already in scope at this point — it was created on line 75. `StyleColorsDark()` is the modern equivalent of the default style; if WaveEdit's existing theme code in `ui.cpp` overrides this, great — the dark default just provides sane starting values.

- [ ] **Step 5c.3: Replace event-loop call**

Find `src/main.cpp:97-98`:
```cpp
		while (SDL_PollEvent(&event)) {
			ImGui_ImplSdlGL2_ProcessEvent(&event);
```

Replace with:
```cpp
		while (SDL_PollEvent(&event)) {
			ImGui_ImplSDL2_ProcessEvent(&event);
```

Just the function name changes: `ImGui_ImplSdlGL2_ProcessEvent` → `ImGui_ImplSDL2_ProcessEvent`.

- [ ] **Step 5c.4: Replace new-frame call**

Find `src/main.cpp:125`:
```cpp
		ImGui_ImplSdlGL2_NewFrame(window);
```

Replace with:
```cpp
		ImGui_ImplOpenGL2_NewFrame();
		ImGui_ImplSDL2_NewFrame();
		ImGui::NewFrame();
```

Modern imgui requires three separate `NewFrame()` calls, in this order: OpenGL backend, SDL backend, then ImGui core. The old single combined call is gone.

- [ ] **Step 5c.5: Replace render call**

Find `src/main.cpp:136-139`:
```cpp
		glClearColor(0.0, 0.0, 0.0, 1.0);
		glClear(GL_COLOR_BUFFER_BIT);
		ImGui::Render();
		SDL_GL_SwapWindow(window);
```

Replace with:
```cpp
		glClearColor(0.0, 0.0, 0.0, 1.0);
		glClear(GL_COLOR_BUFFER_BIT);
		ImGui::Render();
		ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
		SDL_GL_SwapWindow(window);
```

One line added between `ImGui::Render()` and `SDL_GL_SwapWindow`. `ImGui::Render()` no longer draws anything on its own — it just builds the draw-data, and the OpenGL2 backend is now responsible for the actual GL calls.

- [ ] **Step 5c.6: Replace shutdown call**

Find `src/main.cpp:144-146`:
```cpp
	// Cleanup
	uiDestroy();
	ImGui_ImplSdlGL2_Shutdown();
```

Replace with:
```cpp
	// Cleanup
	uiDestroy();
	ImGui_ImplOpenGL2_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();
```

Three calls replace the one old call. Order is reverse of init: OpenGL backend, SDL backend, ImGui context.

- [ ] **Step 5c.7: Do NOT build yet.** The build will still fail because of widgets.cpp and ui.cpp. Proceed to 5d.

### Task 5d: Simple renames in `ui.cpp`

**Files:**
- Modify: `src/ui.cpp:792` (`ShowTestWindow` → `ShowDemoWindow`)

- [ ] **Step 5d.1: Rename `ShowTestWindow`**

Find `src/ui.cpp:792`:
```cpp
		ImGui::ShowTestWindow(&showTestWindow);
```

Replace with:
```cpp
		ImGui::ShowDemoWindow(&showTestWindow);
```

The variable name `showTestWindow` is WaveEdit's own — leave it as-is. Only the imgui function name changes.

### Task 5e: Update key handling in `src/ui.cpp`

**Files:**
- Modify: `src/ui.cpp:244-298` (24 call sites using `SDLK_*` or `SDL_SCANCODE_*` keycodes)

**Background:** In imgui v1.87 (2022), the `ImGuiKey_*` enum was expanded to cover all physical keys as named enum values. `IsKeyPressed()` now expects an `ImGuiKey` value, not a native SDL keycode or scancode. WaveEdit has 24 call sites using SDL constants that need to be converted.

- [ ] **Step 5e.1: Reference mapping table**

Apply these one-to-one replacements throughout `src/ui.cpp:244-298`:

| Current | Replace with |
|---|---|
| `SDLK_n` | `ImGuiKey_N` |
| `SDLK_o` | `ImGuiKey_O` |
| `SDLK_s` | `ImGuiKey_S` |
| `SDLK_q` | `ImGuiKey_Q` |
| `SDLK_z` | `ImGuiKey_Z` |
| `SDLK_a` | `ImGuiKey_A` |
| `SDLK_c` | `ImGuiKey_C` |
| `SDLK_x` | `ImGuiKey_X` |
| `SDLK_v` | `ImGuiKey_V` |
| `SDLK_r` | `ImGuiKey_R` |
| `SDLK_BACKSPACE` | `ImGuiKey_Backspace` |
| `SDLK_DELETE` | `ImGuiKey_Delete` |
| `SDLK_SPACE` | `ImGuiKey_Space` |
| `SDLK_1` | `ImGuiKey_1` |
| `SDLK_2` | `ImGuiKey_2` |
| `SDLK_3` | `ImGuiKey_3` |
| `SDLK_4` | `ImGuiKey_4` |
| `SDLK_5` | `ImGuiKey_5` |
| `SDL_SCANCODE_F1` | `ImGuiKey_F1` |
| `SDL_SCANCODE_UP` | `ImGuiKey_UpArrow` |
| `SDL_SCANCODE_DOWN` | `ImGuiKey_DownArrow` |
| `SDL_SCANCODE_LEFT` | `ImGuiKey_LeftArrow` |
| `SDL_SCANCODE_RIGHT` | `ImGuiKey_RightArrow` |

- [ ] **Step 5e.2: Apply the replacements**

Use the Edit tool with `replace_all: true` for each of the 23 distinct old tokens above. Each Edit call is one pair from the table. This will touch lines 244-298 of `ui.cpp`.

After all edits, verify no stragglers remain:
```bash
grep -n 'SDLK_\|SDL_SCANCODE_' src/ui.cpp
```
Expected: **no output**. If any hits remain, they're sites the mapping table missed — add them to the table and repeat.

### Task 5f: Fix `widgets.cpp` internal-API signature updates

**Files:**
- Modify: `src/widgets.cpp` — 4 custom widget functions plus the `editorBehavior` helper

**This step is iterative.** The exact compile errors depend on which `imgui_internal.h` signatures have shifted between 2017 and v1.92.7. The engineer should treat this as "compile, fix the error, repeat" with the guidance below as a cheat sheet.

- [ ] **Step 5f.1: Try to build and capture the errors**

Run:
```bash
make clean
make 2>&1 | tee /tmp/waveedit-build.log
```
Expected: **build will fail**. Look at the first few errors in `/tmp/waveedit-build.log` — they will be in `widgets.cpp`.

- [ ] **Step 5f.2: Known-bad patterns and their fixes**

The following patterns **definitely need fixing**. Apply them throughout `widgets.cpp`:

**Pattern A — `ItemAdd(box, NULL)` — 4 sites: lines 169, 232, 304, 555**

Old:
```cpp
if (!ImGui::ItemAdd(box, NULL))
```
New:
```cpp
if (!ImGui::ItemAdd(box, 0))
```

Modern `ItemAdd` takes `ImGuiID id` by value (not by pointer). `NULL` is not a valid `ImGuiID` — use `0` (which is a valid sentinel meaning "no ID").

**Pattern B — `IsHovered(box, id)` — 4 sites: lines 87, 343, 469, 559**

Old:
```cpp
bool hovered = ImGui::IsHovered(box, id);
```
New:
```cpp
bool hovered = ImGui::ItemHoverable(box, id, 0);
```

`ImGui::IsHovered(box, id)` was removed. Modern internal equivalent is `ItemHoverable(const ImRect& bb, ImGuiID id, ImGuiItemFlags item_flags)`. Pass `0` as the third arg to mean "no special flags."

**Pattern C — `SetActiveID`, `SetHoveredID`, `ClearActiveID`, `FocusWindow`**

These internal functions should still exist with the same signatures. Do NOT change them unless the compiler complains. If the compiler complains, consult `ext/imgui/imgui_internal.h` to find the current signature.

**Pattern D — direct access to context fields (`g.ActiveIdClickOffset`, `g.ActiveId`, `g.IO.MouseDown`, etc.)**

These field names have been stable. Do NOT preemptively rename. Only change if the compiler complains, and check `imgui_internal.h` for the current name.

**Pattern E — `ItemSize(box, style.FramePadding.y)` — 4 sites: lines 168, 231, 303, 554**

The modern signature is `ItemSize(const ImRect& bb, float text_baseline_y = -1.0f)`. The old calls should work unchanged. **Do not modify unless the compiler complains.**

- [ ] **Step 5f.3: Apply fixes iteratively**

1. Apply Pattern A (four `ItemAdd(box, NULL)` → `ItemAdd(box, 0)` fixes) using the Edit tool with `replace_all: true` on the string `ItemAdd(box, NULL)` → `ItemAdd(box, 0)`.
2. Apply Pattern B (four `IsHovered(box, id)` → `ItemHoverable(box, id, 0)` fixes) similarly with `replace_all: true`.
3. Run `make 2>&1 | tee /tmp/waveedit-build.log` and read the errors.
4. For each remaining error, consult `ext/imgui/imgui_internal.h` and the cheat-sheet patterns above. Apply one fix, recompile, repeat.
5. Do NOT refactor `editorBehavior()` into modern `ButtonBehavior()`. That is a future improvement, not part of this plan. Minimal diff is the goal.

- [ ] **Step 5f.4: Build must succeed before moving on**

Run the `VERIFY_BUILD` command.

Expected: exit code 0, `WaveEdit` binary produced. If the build is still failing after an hour of iterative fixes, stop and consult the user — the migration may have hit an unexpectedly deep API change.

### Task 5g: First launch verification

- [ ] **Step 5g.1: Verify launch**

Run the `VERIFY_LAUNCH` command.

Expected: `PASS: WaveEdit still running after 3s`. If the program crashes on startup, run it in lldb and capture the crash site — common causes:
- Missing `ImGui::CreateContext()` call (should be in 5c.2)
- Backend init called before context creation (order-dependent in modern imgui)
- Font atlas build failure (imgui changed the default font atlas handling in v1.90+)

### Task 5h: Interactive widget validation

- [ ] **Step 5h.1: Run the `VERIFY_INTERACTIVE` checklist**

Launch `./WaveEdit` and step through all nine items in the `VERIFY_INTERACTIVE` checklist at the top of this document. Take your time. The custom widgets in `widgets.cpp` are the highest risk — if any of them fail to respond to mouse, consult the imgui_internal.h changes and compare old vs new pattern.

- [ ] **Step 5h.2: Note any regressions**

Write down any failing items from the checklist. Common expected failures:
- **Tab labels not switching:** `tablabels.hpp` uses public APIs only, so this is unlikely, but possible if `ButtonActive`/`ButtonHovered` style colors were renamed.
- **Custom widgets not hoverable/draggable:** the `IsHovered` → `ItemHoverable` migration may not be behaviorally identical. Check `ImGuiItemFlags` flags.
- **Keyboard shortcuts silently not firing:** verify the key enum replacements in 5e are correct.

If any regression can be fixed with a small additional change, do it and re-verify. If not, **stop and consult the user** before committing.

### Task 5i: Commit the imgui migration

- [ ] **Step 5i.1: Stage everything**

Run:
```bash
git status
git add .gitmodules ext/imgui Makefile src/main.cpp src/ui.cpp src/widgets.cpp
```
Expected: the staged set should include those six paths and nothing else. No untracked files should be newly tracked.

- [ ] **Step 5i.2: Commit**

Run:
```bash
git commit -m "ext/imgui: migrate from AndrewBelt/imgui (2017) to ocornut/imgui v1.92.7

Per doc/maintenance/2026-04-08-submodule-audit.md, the previous
ext/imgui pin was a 2017-era snapshot of AndrewBelt/imgui, a
fork of ocornut/imgui that has not been updated in 8 years.
This commit replaces it with canonical upstream pinned to tag
v1.92.7 (2026-04-02).

Code changes required by the migration:
- Makefile: reference backends/imgui_impl_sdl2.cpp +
  backends/imgui_impl_opengl2.cpp instead of the old combined
  examples/sdl_opengl2_example/imgui_impl_sdl.cpp. Add
  imgui_tables.cpp and imgui_widgets.cpp to SOURCES (required
  in modern imgui).
- src/main.cpp: rewrite backend init, event-loop,
  NewFrame, Render, and Shutdown glue for the split SDL2 +
  OpenGL2 backend API. Add explicit CreateContext/
  DestroyContext calls.
- src/ui.cpp: replace SDLK_* and SDL_SCANCODE_* constants in
  IsKeyPressed() calls with ImGuiKey_* enum values (required
  since imgui v1.87). Rename ShowTestWindow to ShowDemoWindow.
- src/widgets.cpp: fix internal-API signature drift:
  ItemAdd(box, NULL) -> ItemAdd(box, 0) and
  IsHovered(box, id) -> ItemHoverable(box, id, 0).

Interactively verified on macOS arm64: window launches, all
tabs switch, custom widgets respond to mouse, keyboard
shortcuts fire, clean exit."
```

---

## Task 6: End-to-end final verification

**Files:** none (verification only)

- [ ] **Step 6.1: Clean build from scratch**

Run:
```bash
make clean
cd dep && make && cd ..
make
```
Expected: all three commands exit 0. This is slightly more than `VERIFY_BUILD` because it re-fetches the Homebrew deps through `dep/Makefile` to make sure nothing we changed broke that path.

- [ ] **Step 6.2: Git log sanity check**

Run:
```bash
git log --oneline m1-modernization..HEAD
```
Expected: exactly 4 commits:
1. `ext/lodepng: bump to current upstream`
2. `ext/osdialog: bump to current upstream`
3. `ext/pffft: switch to canonical jpommier upstream`
4. `ext/imgui: migrate from AndrewBelt/imgui (2017) to ocornut/imgui v1.92.7`

If the count is different, something got committed that shouldn't have or a commit got lost. Investigate before proceeding.

- [ ] **Step 6.3: Submodule status sanity check**

Run:
```bash
git submodule status
```
Expected output format: 4 lines, one per submodule, each starting with a space (not `+` or `-`), each showing a different SHA than the baseline. No dirty submodules.

- [ ] **Step 6.4: Final interactive validation**

Re-run the `VERIFY_INTERACTIVE` checklist one more time as a sanity check that the clean rebuild behaves the same as the development rebuild.

---

## Task 7: Post-implementation handoff

**Files:** none (merge decision for the user)

- [ ] **Step 7.1: Present merge options to the user**

Do NOT merge, rebase, or push. Present the user with these three options and let them decide:

**Option A — Fast-forward merge into `m1-modernization`:**
```bash
git checkout m1-modernization
git merge --ff-only submodule-upgrade
```
Clean linear history, which is appropriate for this kind of mechanical maintenance work.

**Option B — Squash-merge into `m1-modernization`:**
```bash
git checkout m1-modernization
git merge --squash submodule-upgrade
git commit -m "Submodule modernization (per 2026-04-08-submodule-audit.md)"
```
Collapses the four commits into one. Loses bisect granularity for the individual submodule bumps; gains summary-level history.

**Option C — Keep the feature branch:**
Leave `submodule-upgrade` as-is and move on to the CI sub-project, which will presumably build on top of this work before anything lands on `m1-modernization`.

**Recommendation:** **Option A.** The four commits are already well-scoped and narrated; fast-forward merge preserves bisect utility and reads cleanly in `git log`.

---

## Self-review notes

**Spec coverage check:**
- `ext/lodepng` bump → Task 2 ✓
- `ext/osdialog` bump → Task 3 ✓
- `ext/pffft` URL switch to jpommier → Task 4 ✓
- `ext/imgui` migration to ocornut v1.92.7 → Task 5 (multi-part) ✓
- Interactive validation (irreducible) → Task 5h, Task 6.4 ✓
- Build verification between each change → verify-build in every task ✓
- Feature branch isolation → Task 1 ✓

**Placeholder scan:** none found. Every step has concrete code, exact file paths, and expected outputs.

**Type consistency:** function names (`ImGui_ImplSDL2_*` vs `ImGui_ImplOpenGL2_*`) are consistent across Tasks 5b, 5c. `VERIFY_BUILD` / `VERIFY_LAUNCH` / `VERIFY_INTERACTIVE` are defined once in the reference section and referenced consistently.

**Known uncertainties documented inline:**
- Task 5f warns that the widgets.cpp migration is iterative and may need cheat-sheet patterns beyond those listed
- Task 5g warns about possible startup-crash causes
- Task 5h lists expected classes of behavioral regression to watch for

**Scope:** this plan modifies four submodules, one Makefile, three source files. All changes are isolated to the dependency upgrade. No unrelated refactoring, no speculative improvements, no `tablabels.hpp` replacement (deferred), no `ButtonBehavior()` refactor of custom widgets (deferred).
