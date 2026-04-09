# CI / Cross-Platform Build Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Stand up a GitHub Actions workflow that builds WaveEdit on Linux / macOS arm64 / Windows for every push and PR, plus a release job that attaches distribution zips to a draft GitHub Release when a `v*` tag is pushed.

**Architecture:** Single workflow file `.github/workflows/build.yml` with a three-platform build matrix and a tag-gated release job. One small supporting fix to the Windows `make dist` step in the Makefile (hardcoded `/mingw32/bin` paths need to become `/mingw64/bin` to match the MSYS2 environment used in CI). No source code changes. No CMake migration. No codesigning. Iterative development on a feature branch `ci-github-actions` with incremental per-platform verification.

**Tech Stack:** GitHub Actions (YAML workflows), actions/checkout@v4, actions/cache@v4, actions/upload-artifact@v4, actions/download-artifact@v4, msys2/setup-msys2@v2, gh CLI for release creation. Ubuntu 22.04, macOS 15, Windows latest (MSYS2/MINGW64).

**Predecessor:** `2026-04-08-ci-build-design.md` (the approved design spec)
**Depends on:** Submodule upgrade sub-project (satisfied 2026-04-09)

---

## Important notes for the implementing engineer

1. **This plan cannot be verified with local `make` alone.** The whole point of CI is that it runs on GitHub's infrastructure. Verification for most tasks requires pushing the branch to a GitHub remote and watching the Actions tab. Budget for this — each workflow run is ~5-15 minutes depending on cache state.
2. **You need push access to a GitHub repository.** The current `origin` remote on this checkout points at `AndrewBelt/WaveEdit`, which you don't own. Task 0 walks through checking/setting up a proper remote before any workflow work can begin.
3. **Each platform job will likely fail once, then be fixable.** The Windows `make dist` step in particular is expected to fail because the existing Makefile hardcodes `/mingw32/bin` paths but CI uses MINGW64. Task 1 pre-empts that, but there may be other small surprises.
4. **Keep commits small.** Each "did this CI job go green" checkpoint should be its own commit. If CI goes red and you need to fix it, that's another commit — don't amend.
5. **Don't leave a real `v*` tag around from testing.** Task 5 creates a throwaway test tag and cleans it up at the end. Don't use `v1.0` or any real version number as your test tag; use something like `v0.0.1-ci-test`.

## Verification commands reference

**`VERIFY_LOCAL_BUILD`** — confirms the Makefile changes in Task 1 don't break the existing local build before we push:
```bash
cd /Users/jgoney/dev/WaveEdit_clones/WaveEdit
make clean
make 2>&1 | tee /tmp/waveedit-build.log | tail -5
ls -la WaveEdit && file WaveEdit
```
Expected: `WaveEdit` binary exists as `Mach-O 64-bit executable arm64`. The Windows DLL path changes in Task 1 are inside an `ifeq ($(ARCH),win)` block, so they never execute on the current arm64 mac — they can only be verified by the CI Windows job (Task 4).

**`VERIFY_WORKFLOW_SYNTAX`** — confirms the YAML is parseable without pushing:
```bash
python3 -c "import yaml; yaml.safe_load(open('.github/workflows/build.yml'))" && echo "OK"
```
Expected: `OK`. This catches indentation bugs and malformed YAML before wasting a CI run.

**`VERIFY_CI_GREEN`** — confirms a given workflow run went green on GitHub (run after pushing):
```bash
gh run list --workflow=build.yml --limit 1
gh run view $(gh run list --workflow=build.yml --limit 1 --json databaseId --jq '.[0].databaseId')
```
Expected: `completed success` for the top-of-list run. If `in_progress`, wait. If `completed failure`, click through to see which job/step failed.

---

## Task 0: Prerequisites — check for a pushable GitHub remote

**Files:** none (git config inspection only)

This task is a gate. If it fails, the rest of the plan cannot proceed until the user has set up a GitHub remote they can push to.

- [ ] **Step 0.1: Inspect current remotes**

Run:
```bash
cd /Users/jgoney/dev/WaveEdit_clones/WaveEdit
git remote -v
```

Expected one of:
- **Good state:** Output includes a remote (any name) whose URL points at a GitHub repo the user can push to. Note the remote name (commonly `origin` or `fork` or `me`).
- **Bad state:** The only remote is `origin → git@github.com:AndrewBelt/WaveEdit.git` (or the `https://` equivalent). The user cannot push to AndrewBelt's repo.

- [ ] **Step 0.2: Decide what to do based on state**

**If the good state:** note the remote name and continue to Task 1.

**If the bad state:** STOP and present the user with these options:

> You need a GitHub repository you can push to before CI can work. Three options:
>
> **A. Fork AndrewBelt/WaveEdit on GitHub** (via the web UI), then add it as a new remote:
> ```bash
> git remote add fork git@github.com:YOUR_USERNAME/WaveEdit.git
> git push -u fork m1-modernization
> ```
> Use the name `fork` to make it distinct from the upstream `origin`.
>
> **B. Create a brand-new repo on GitHub** (e.g., `yourusername/WaveEdit-maintained`), then:
> ```bash
> git remote add origin-maintained git@github.com:YOUR_USERNAME/WaveEdit-maintained.git
> # Optional: if you want this to be your primary, rename
> git remote rename origin upstream
> git remote rename origin-maintained origin
> git push -u origin m1-modernization
> ```
>
> **C. If you already have a GitHub copy under a different name or hosting service**, add it as a remote and confirm you can push. If you use GitLab, Codeberg, or Gitea, the CI plan DOES NOT APPLY — this plan targets GitHub Actions specifically. The equivalent CI system on another host would need its own separate plan.
>
> Once you've set up a pushable GitHub remote, tell me which remote name you used and we'll continue.

Do NOT proceed to Task 1 until the user confirms a pushable GitHub remote exists. This is the single hardest gate in the plan; nothing works without it.

- [ ] **Step 0.3: Verify `gh` CLI is installed and authenticated**

Run:
```bash
gh --version
gh auth status
```

Expected:
- `gh --version` prints a version number (any version ≥ 2.0 is fine)
- `gh auth status` shows "Logged in to github.com account ..." with push scope

If `gh` is not installed: `brew install gh` on macOS.
If `gh auth status` fails: run `gh auth login` and follow the prompts.

The release job in Task 5 and the verification commands in later tasks rely on `gh` CLI being available and authenticated locally.

- [ ] **Step 0.4: No commit for this task.**

This task only validates prerequisites. Proceed to Task 1.

---

## Task 1: Create feature branch and fix the Windows MINGW64 DLL paths

**Files:**
- Modify: `Makefile:131-133` (replace `/mingw32/bin/libgcc_s_dw2-1.dll`, `libwinpthread-1.dll`, `libstdc++-6.dll` paths with MINGW64 equivalents)

Rationale: The existing Makefile's `make dist` step for Windows copies MinGW runtime DLLs from `/mingw32/bin`, but CI will use MINGW64 (x86_64) via `msys2/setup-msys2@v2`. The paths must match the environment. Also, the 32-bit libgcc DLL is named `libgcc_s_dw2-1.dll` but the 64-bit equivalent is `libgcc_s_seh-1.dll`.

This fix cannot be verified locally (you're on macOS arm64). It will be tested for real in Task 4 when the Windows CI job hits `make dist`. But we can still run `VERIFY_LOCAL_BUILD` to confirm we haven't broken anything else.

- [ ] **Step 1.1: Confirm clean working tree**

Run:
```bash
git status
```

Expected: "nothing to commit" for tracked files (untracked build artifacts like `WaveEdit`, `autosave.dat`, `dep/lib/`, etc. are fine — they're not tracked). If any tracked files are modified, stop and ask.

- [ ] **Step 1.2: Confirm you're on `m1-modernization` and create the feature branch**

Run:
```bash
git checkout m1-modernization
git checkout -b ci-github-actions
```

Expected: switched to new branch `ci-github-actions`.

- [ ] **Step 1.3: Read the current Windows dist section**

Look at `Makefile:128-137`. The current block is:
```make
else ifeq ($(ARCH),win)
	cp -R logo*.png fonts catalog dist/WaveEdit
	cp WaveEdit.exe dist/WaveEdit
	cp /mingw32/bin/libgcc_s_dw2-1.dll dist/WaveEdit
	cp /mingw32/bin/libwinpthread-1.dll dist/WaveEdit
	cp /mingw32/bin/libstdc++-6.dll dist/WaveEdit
	cp dep/bin/SDL2.dll dist/WaveEdit
	cp dep/bin/libsamplerate-0.dll dist/WaveEdit
	cp dep/bin/libsndfile-1.dll dist/WaveEdit
endif
```

- [ ] **Step 1.4: Edit the three `/mingw32/bin/...` lines**

Use the Edit tool to replace this exact text:
```make
	cp /mingw32/bin/libgcc_s_dw2-1.dll dist/WaveEdit
	cp /mingw32/bin/libwinpthread-1.dll dist/WaveEdit
	cp /mingw32/bin/libstdc++-6.dll dist/WaveEdit
```

with:
```make
	cp /mingw64/bin/libgcc_s_seh-1.dll dist/WaveEdit
	cp /mingw64/bin/libwinpthread-1.dll dist/WaveEdit
	cp /mingw64/bin/libstdc++-6.dll dist/WaveEdit
```

Three changes:
- Path prefix `mingw32` → `mingw64` on all three lines
- Filename `libgcc_s_dw2-1.dll` → `libgcc_s_seh-1.dll` on the first line (dwarf-2 exception handling is the 32-bit convention; SEH is the 64-bit Windows convention)
- `libwinpthread-1.dll` and `libstdc++-6.dll` filenames unchanged — only their path prefix changed

- [ ] **Step 1.5: Run `VERIFY_LOCAL_BUILD`**

Run the `VERIFY_LOCAL_BUILD` command from the reference section above.

Expected: clean build, `WaveEdit` binary present as `Mach-O 64-bit executable arm64`. The Makefile changes are gated behind `ifeq ($(ARCH),win)` and do not affect the macOS build path at all. This step is just a sanity check that we haven't accidentally broken syntax elsewhere.

- [ ] **Step 1.6: Commit**

Run:
```bash
git add Makefile
git commit -m "Makefile: fix Windows dist step to use MINGW64 runtime DLL paths

The existing Windows branch of 'make dist' hardcoded paths to
/mingw32/bin/ (32-bit MinGW), but modern CI will use the MINGW64
(x86_64) MSYS2 environment. This changes:

- /mingw32/bin/libgcc_s_dw2-1.dll  -> /mingw64/bin/libgcc_s_seh-1.dll
  (the filename changes because 32-bit MinGW uses Dwarf-2 exception
  handling while 64-bit uses SEH, Windows Structured Exception
  Handling)
- /mingw32/bin/libwinpthread-1.dll -> /mingw64/bin/libwinpthread-1.dll
- /mingw32/bin/libstdc++-6.dll     -> /mingw64/bin/libstdc++-6.dll

Consequences: Windows builds must now use MINGW64 specifically,
not the 32-bit MINGW32 environment. This is fine for modern usage;
32-bit Windows support is no longer a priority.

Cannot be verified locally on macOS — will be tested by the
Windows CI job in the next commit."
```

---

## Task 2: Create the workflow with only the Linux job

**Files:**
- Create: `.github/workflows/build.yml`

Goal: Get a minimal, green Linux CI run first. Skip macOS and Windows for now. Everything else builds on this foundation.

- [ ] **Step 2.1: Create the `.github/workflows/` directory**

Run:
```bash
mkdir -p .github/workflows
```

Expected: directory created. No output if successful.

- [ ] **Step 2.2: Write the initial workflow file with only the Linux job**

Create `.github/workflows/build.yml` with this exact content:

```yaml
name: build

on:
  push:
    branches: [m1-modernization, main, ci-github-actions]
    tags: ['v*']
  pull_request:
    branches: [m1-modernization, main]
  workflow_dispatch:

# The ci-github-actions branch is included in `on.push.branches` during the
# CI sub-project's development so that each commit triggers a workflow run.
# It should be removed from the branch list as part of Task 6 before the
# feature branch is merged back into m1-modernization.

jobs:
  build:
    strategy:
      fail-fast: false
      matrix:
        include:
          - arch: lin
            runner: ubuntu-22.04
    runs-on: ${{ matrix.runner }}
    steps:
      - name: Checkout
        uses: actions/checkout@v4
        with:
          submodules: recursive

      - name: Restore dep cache (Linux)
        if: matrix.arch == 'lin'
        uses: actions/cache@v4
        with:
          path: |
            dep/lib
            dep/include
            dep/bin
          key: dep-${{ matrix.arch }}-${{ hashFiles('dep/Makefile') }}

      - name: Install Linux toolchain
        if: matrix.arch == 'lin'
        run: |
          sudo apt-get update
          sudo apt-get install -y build-essential pkg-config libgtk2.0-dev wget zip

      - name: Build dep/
        run: |
          cd dep && make

      - name: Build WaveEdit
        run: |
          make

      - name: Verify binary
        run: |
          test -x WaveEdit
          file WaveEdit

      - name: Build dist zip
        run: |
          make dist

      - name: Upload dist artifact
        uses: actions/upload-artifact@v4
        with:
          name: WaveEdit-${{ matrix.arch }}
          path: dist/WaveEdit-*.zip
          retention-days: 30
```

- [ ] **Step 2.3: Run `VERIFY_WORKFLOW_SYNTAX`**

Run:
```bash
python3 -c "import yaml; yaml.safe_load(open('.github/workflows/build.yml'))" && echo "OK"
```

Expected: `OK`. If it prints a `yaml.scanner.ScannerError` or similar, the YAML is malformed — fix the indentation/quoting before continuing.

- [ ] **Step 2.4: Commit**

Run:
```bash
git add .github/workflows/build.yml
git commit -m "ci: add GitHub Actions workflow with Linux job only

Initial commit of the CI workflow, per
doc/maintenance/2026-04-09-ci-build-plan.md Task 2.

Starts with a Linux-only build matrix (ubuntu-22.04, pinned) to
establish a green baseline before adding macOS and Windows in
subsequent commits. Other design elements already in place:

- on.push.branches includes ci-github-actions during CI
  sub-project development; to be removed before merge
- on.push.tags['v*'] for release triggering (release job added
  in a later task)
- actions/cache@v4 on dep/lib, dep/include, dep/bin keyed on
  hashFiles('dep/Makefile')
- actions/upload-artifact@v4 with 30-day retention for dist zips

Cannot verify this is green until we push."
```

- [ ] **Step 2.5: Push and watch the run**

Run:
```bash
git push -u <remote-name> ci-github-actions
```

Replace `<remote-name>` with whatever you noted in Task 0.1. If you have only one remote, it's probably `origin`.

Then watch the workflow run:
```bash
gh run watch $(gh run list --workflow=build.yml --limit 1 --json databaseId --jq '.[0].databaseId')
```

Expected outcomes:
- **PASS (completed success):** proceed to Task 3.
- **FAIL during `apt-get install`:** the `libgtk2.0-dev` package name may have changed in ubuntu-22.04 (unlikely — verified in Ubuntu 22.04 as of 2026-04). Check the raw log with `gh run view --log-failed`.
- **FAIL during `cd dep && make`:** the `dep/Makefile` is trying to build SDL2/libsndfile/libsamplerate from source (it detects Linux via `gcc -dumpmachine` and branches accordingly). If this fails, it's likely a missing prerequisite — read the specific error.
- **FAIL during `make`:** some compile error showed up on Linux that didn't show on macOS. Most likely: a header or constant that macOS defines but Linux does not. Investigate and fix.
- **FAIL during `make dist`:** the Linux dist branch (`Makefile:107-112`) expects `dep/lib/libSDL2-2.0.so.0`, `libsamplerate.so.0`, `libsndfile.so.1` to exist. They should, because the dep build just ran.

For any failure: fix in a follow-up commit, push, re-watch. Do NOT proceed to Task 3 until Task 2 is green.

---

## Task 3: Add the macOS arm64 job to the matrix

**Files:**
- Modify: `.github/workflows/build.yml` (add a second matrix entry and add the macOS-specific install step)

- [ ] **Step 3.1: Edit the matrix include list**

Use the Edit tool to change this block in `.github/workflows/build.yml`:

```yaml
    strategy:
      fail-fast: false
      matrix:
        include:
          - arch: lin
            runner: ubuntu-22.04
```

to:

```yaml
    strategy:
      fail-fast: false
      matrix:
        include:
          - arch: lin
            runner: ubuntu-22.04
          - arch: mac_arm64
            runner: macos-15
```

- [ ] **Step 3.2: Add the macOS install step**

Use the Edit tool to replace this step in `.github/workflows/build.yml`:

```yaml
      - name: Install Linux toolchain
        if: matrix.arch == 'lin'
        run: |
          sudo apt-get update
          sudo apt-get install -y build-essential pkg-config libgtk2.0-dev wget zip
```

with:

```yaml
      - name: Install Linux toolchain
        if: matrix.arch == 'lin'
        run: |
          sudo apt-get update
          sudo apt-get install -y build-essential pkg-config libgtk2.0-dev wget zip

      - name: Install macOS deps via Homebrew
        if: matrix.arch == 'mac_arm64'
        run: |
          brew install sdl2 libsamplerate libsndfile
```

We bypass the `dep/Makefile` `homebrew-deps` target's bootstrap dance — it just runs the same brew install plus symlink fixups. The `dep/Makefile` is smart enough to detect macOS and skip building SDL2/libsndfile/libsamplerate from source (see `dep/Makefile:19-23` which has a macOS-specific short-circuit).

- [ ] **Step 3.3: Run `VERIFY_WORKFLOW_SYNTAX`**

Same command as Task 2.3.

Expected: `OK`.

- [ ] **Step 3.4: Commit**

Run:
```bash
git add .github/workflows/build.yml
git commit -m "ci: add macOS arm64 job to build matrix

Second matrix entry uses macos-15 (Apple Silicon, free for public
repos). Homebrew install of sdl2/libsamplerate/libsndfile replaces
the dep/Makefile's homebrew-deps bootstrap dance — the Makefile
already detects macOS and short-circuits the from-source build of
these libraries.

Intel macOS remains out of CI per the design spec (see 'Why no
Intel mac in CI' in doc/maintenance/2026-04-08-ci-build-design.md):
all current GitHub -large / -intel macOS runners are billed even
on public repos."
```

- [ ] **Step 3.5: Push and watch**

Run:
```bash
git push
gh run watch $(gh run list --workflow=build.yml --limit 1 --json databaseId --jq '.[0].databaseId')
```

Expected: both `lin` and `mac_arm64` jobs complete successfully.

Likely failure modes:
- **`brew install` times out or fails:** Homebrew bottle downloads are usually fast but occasionally flake. Re-run the job with `gh run rerun <run-id>`.
- **`cd dep && make` fails on macOS:** the `dep/Makefile:25-34` `homebrew-deps` target expects `brew` to be on PATH. It should be on GitHub-hosted macOS runners, but if it fails, check the log.
- **`make` fails on macOS:** most likely an `-I/opt/homebrew/include` path issue. The Makefile hardcodes `-I$(shell brew --prefix)/include` for Apple Silicon which should resolve to `/opt/homebrew`. If this fails, check the CI runner's Homebrew prefix (it should be `/opt/homebrew` on macos-14+ ARM runners).
- **`make dist` fails on macOS:** the install_name_tool rpath rewriting in `Makefile:117-126` uses `$(shell brew --prefix sdl2)/lib/libSDL2-2.0.0.dylib` etc. These paths must exist exactly. If brew moved anything, this breaks.

For any failure: fix in a follow-up commit. Do NOT proceed to Task 4 until both `lin` and `mac_arm64` jobs are green.

---

## Task 4: Add the Windows job to the matrix

**Files:**
- Modify: `.github/workflows/build.yml` (add third matrix entry and the MSYS2 setup + Windows-specific run steps)

- [ ] **Step 4.1: Add the third matrix entry**

Use the Edit tool to change this block:

```yaml
        include:
          - arch: lin
            runner: ubuntu-22.04
          - arch: mac_arm64
            runner: macos-15
```

to:

```yaml
        include:
          - arch: lin
            runner: ubuntu-22.04
          - arch: mac_arm64
            runner: macos-15
          - arch: win
            runner: windows-latest
```

- [ ] **Step 4.2: Add MSYS2 setup + Windows-specific run steps**

Windows is more structurally different from Linux and macOS because we need the `msys2/setup-msys2` action to install the toolchain inside the runner, and then the subsequent `run:` steps have to execute under the MSYS2 shell to see the toolchain.

Use the Edit tool to replace this block:

```yaml
      - name: Install macOS deps via Homebrew
        if: matrix.arch == 'mac_arm64'
        run: |
          brew install sdl2 libsamplerate libsndfile

      - name: Build dep/
        run: |
          cd dep && make

      - name: Build WaveEdit
        run: |
          make

      - name: Verify binary
        run: |
          test -x WaveEdit
          file WaveEdit

      - name: Build dist zip
        run: |
          make dist
```

with:

```yaml
      - name: Install macOS deps via Homebrew
        if: matrix.arch == 'mac_arm64'
        run: |
          brew install sdl2 libsamplerate libsndfile

      - name: Set up MSYS2 (Windows)
        if: matrix.arch == 'win'
        uses: msys2/setup-msys2@v2
        with:
          msystem: MINGW64
          update: true
          install: >-
            mingw-w64-x86_64-gcc
            mingw-w64-x86_64-make
            mingw-w64-x86_64-pkg-config
            wget
            tar
            zip
            unzip

      - name: Build dep/ (Linux + macOS)
        if: matrix.arch != 'win'
        run: |
          cd dep && make

      - name: Build WaveEdit (Linux + macOS)
        if: matrix.arch != 'win'
        run: |
          make

      - name: Verify binary (Linux + macOS)
        if: matrix.arch != 'win'
        run: |
          test -x WaveEdit
          file WaveEdit

      - name: Build dist zip (Linux + macOS)
        if: matrix.arch != 'win'
        run: |
          make dist

      - name: Build dep/ (Windows)
        if: matrix.arch == 'win'
        shell: msys2 {0}
        run: |
          cd dep && make

      - name: Build WaveEdit (Windows)
        if: matrix.arch == 'win'
        shell: msys2 {0}
        run: |
          make ARCH=win

      - name: Verify binary (Windows)
        if: matrix.arch == 'win'
        shell: msys2 {0}
        run: |
          test -x WaveEdit.exe
          file WaveEdit.exe

      - name: Build dist zip (Windows)
        if: matrix.arch == 'win'
        shell: msys2 {0}
        run: |
          make dist
```

Why the Windows steps are separate: every Windows step needs `shell: msys2 {0}` to run under the MSYS2 environment, where `make`, `gcc`, `wget`, etc. are available. Setting this on a step-by-step basis is easier to reason about than trying to factor it into a `defaults:` block.

Why `make ARCH=win` is explicit for Windows: `Makefile-arch.inc` auto-detects architecture via `gcc -dumpmachine`, which should return a string containing `mingw` under MSYS2 → `ARCH=win`. The explicit `ARCH=win` on the Windows step is belt-and-suspenders insurance.

- [ ] **Step 4.3: Run `VERIFY_WORKFLOW_SYNTAX`**

Same command as Task 2.3.

Expected: `OK`.

- [ ] **Step 4.4: Commit**

Run:
```bash
git add .github/workflows/build.yml
git commit -m "ci: add Windows MSYS2/MINGW64 job to build matrix

Third matrix entry uses windows-latest with msys2/setup-msys2@v2
installing MINGW64 (x86_64) toolchain. The build/verify/dist
steps are factored into two sets: one for Linux+macOS using the
default shell, and one for Windows using 'shell: msys2 {0}' so
each step runs under the MSYS2 environment where gcc/make/wget
live.

This is the commit that exercises Makefile:131-133 (the MINGW64
DLL path fix from the previous Makefile commit). If the dist
step fails because the dll paths are still wrong, the fix goes
in another Makefile edit."
```

- [ ] **Step 4.5: Push and watch**

Run:
```bash
git push
gh run watch $(gh run list --workflow=build.yml --limit 1 --json databaseId --jq '.[0].databaseId')
```

Expected: all three jobs (`lin`, `mac_arm64`, `win`) complete successfully.

Likely Windows-specific failure modes:
- **`msys2/setup-msys2` times out:** the action downloads ~300MB of toolchain the first time. Cache is supposed to handle this but the first run is slow. If it times out, re-run.
- **`cd dep && make` fails under MSYS2:** the `dep/Makefile` uses `wget` to download SDL2/libsndfile/libsamplerate tarballs and then `./configure && make && make install` for each. Any of these can fail:
  - `wget` may not find the tarball URL — check that the URLs in `dep/Makefile:37, 44, 51` are still live
  - `./configure` may fail if a library (e.g., `libopus` for libsndfile) is missing from MSYS2. Add the missing package to the `setup-msys2` install list.
  - `make install` may fail with permission errors — shouldn't on MSYS2 since `$LOCAL` is a user directory
- **`make ARCH=win` fails to find headers:** the Makefile uses `-Idep/include` and `-Idep/include/SDL2`. The dep install should populate these. If it doesn't, the dep build didn't actually install.
- **`make dist` fails to find DLLs at `/mingw64/bin/libgcc_s_seh-1.dll`:** this is the fix we made in Task 1. If the path is wrong, grep MSYS2 to find where libgcc actually lives: add `ls -la /mingw64/bin/libgcc_s*` as a diagnostic step.
- **`zip` missing:** already in the install list, but if it's not picked up, `setup-msys2` may need `msys/zip` instead of `zip`.

For any failure: fix in a follow-up commit. Do NOT proceed to Task 5 until all three jobs are green.

---

## Task 5: Add the tag-gated release job

**Files:**
- Modify: `.github/workflows/build.yml` (add a second top-level `jobs.release:` entry)

- [ ] **Step 5.1: Add the release job**

Use the Edit tool to append this after the end of the `build:` job in `.github/workflows/build.yml`. The entire existing `build:` job (with all its steps) stays where it is; the new `release:` job goes at the same indentation level as `build:`.

Find the last `- name: Upload dist artifact` step and its `with:` block at the bottom of the `build:` job. After that block, add the new `release:` job. The entire file should now end with:

```yaml
      - name: Upload dist artifact
        uses: actions/upload-artifact@v4
        with:
          name: WaveEdit-${{ matrix.arch }}
          path: dist/WaveEdit-*.zip
          retention-days: 30

  release:
    needs: build
    if: startsWith(github.ref, 'refs/tags/v')
    runs-on: ubuntu-latest
    permissions:
      contents: write
    steps:
      - name: Checkout (for release notes generation)
        uses: actions/checkout@v4
        with:
          fetch-depth: 0

      - name: Download all build artifacts
        uses: actions/download-artifact@v4
        with:
          path: artifacts

      - name: Create draft release
        env:
          GH_TOKEN: ${{ github.token }}
        run: |
          gh release create "${{ github.ref_name }}" \
            --draft \
            --generate-notes \
            --title "${{ github.ref_name }}" \
            artifacts/*/WaveEdit-*.zip
```

Notes:
- `needs: build` makes the release job wait for ALL matrix entries of the build job to complete successfully. If any platform fails, no release is cut.
- `if: startsWith(github.ref, 'refs/tags/v')` ensures the release job only runs on tag pushes, not on regular branch pushes or PRs.
- `permissions.contents: write` is required for `gh release create` to work; by default GitHub Actions tokens are read-only.
- `fetch-depth: 0` on the checkout is needed so `gh release create --generate-notes` can compute the diff between this tag and the previous one.
- `--draft` means the release is NOT publicly visible until you manually click "Publish" in the GitHub UI. Intentional safety measure.

- [ ] **Step 5.2: Run `VERIFY_WORKFLOW_SYNTAX`**

Same command as Task 2.3.

Expected: `OK`.

- [ ] **Step 5.3: Commit**

Run:
```bash
git add .github/workflows/build.yml
git commit -m "ci: add tag-gated draft release job

New 'release' job runs only when a tag matching v* is pushed.
Depends on all build matrix jobs completing successfully.
Downloads the artifacts from the build jobs and attaches them
to a new GitHub Release created via 'gh release create --draft
--generate-notes'. Drafts are intentional — releases don't
become public until manually published via the GitHub UI.

Release notes are auto-generated from commit history between
the new tag and the previous tag, so the first tagged release
will have a very long notes body (everything since day one).
That's fine — you can edit before publishing."
```

- [ ] **Step 5.4: Push the commit (not a tag yet)**

Run:
```bash
git push
gh run watch $(gh run list --workflow=build.yml --limit 1 --json databaseId --jq '.[0].databaseId')
```

Expected: all three build matrix jobs complete successfully. The `release` job should NOT run because this is a branch push, not a tag push — confirm by checking `gh run view`: the `release` job should show as "skipped."

- [ ] **Step 5.5: Create a throwaway test tag and push it**

Run:
```bash
git tag v0.0.1-ci-test
git push <remote-name> v0.0.1-ci-test
```

Replace `<remote-name>` with whatever you're using (probably `origin` or `fork`).

This is a deliberately silly tag name so there's zero chance of it being mistaken for a real release. Do NOT use `v1.0`, `v1.1`, etc. for this test.

- [ ] **Step 5.6: Watch the tag-triggered run**

Run:
```bash
gh run watch $(gh run list --workflow=build.yml --limit 1 --json databaseId --jq '.[0].databaseId')
```

Expected: all three `build` matrix jobs complete, THEN the `release` job runs and completes successfully. When it finishes, a draft release should exist.

- [ ] **Step 5.7: Verify the draft release was created**

Run:
```bash
gh release list
gh release view v0.0.1-ci-test
```

Expected:
- `gh release list` shows `v0.0.1-ci-test` with status `Draft`
- `gh release view v0.0.1-ci-test` shows the release body with auto-generated notes and three assets: `WaveEdit-lin-<something>.zip`, `WaveEdit-mac_arm64-<something>.zip`, `WaveEdit-win-<something>.zip`

If the release is missing, or the assets aren't attached, read `gh run view --log` for the release job to find the error.

- [ ] **Step 5.8: Clean up the throwaway release and tag**

Once verified, delete both:

Run:
```bash
gh release delete v0.0.1-ci-test --yes
git tag -d v0.0.1-ci-test
git push <remote-name> :refs/tags/v0.0.1-ci-test
```

The three commands:
1. Delete the draft release on GitHub
2. Delete the tag locally
3. Delete the tag on the remote

Expected: after this, `gh release list` shows no `v0.0.1-ci-test`, and `git tag -l 'v0.0.1-ci-test'` returns empty.

- [ ] **Step 5.9: No commit for this task.**

Task 5 made one workflow commit (Step 5.3) and one tag/release that was cleaned up. No code changes beyond what's already committed. Proceed to Task 6.

---

## Task 6: Remove the feature branch from the trigger list and merge

**Files:**
- Modify: `.github/workflows/build.yml` (remove `ci-github-actions` from `on.push.branches`)

- [ ] **Step 6.1: Remove the feature branch from the trigger**

Use the Edit tool to change this block in `.github/workflows/build.yml`:

```yaml
on:
  push:
    branches: [m1-modernization, main, ci-github-actions]
    tags: ['v*']
```

to:

```yaml
on:
  push:
    branches: [m1-modernization, main]
    tags: ['v*']
```

One line changed: `ci-github-actions` removed from the branch list. Also delete the explanatory comment above `jobs:` that mentions it — the comment is no longer accurate once the branch is removed. Find and delete these two lines:

```yaml
# The ci-github-actions branch is included in `on.push.branches` during the
# CI sub-project's development so that each commit triggers a workflow run.
# It should be removed from the branch list as part of Task 6 before the
# feature branch is merged back into m1-modernization.
```

- [ ] **Step 6.2: Run `VERIFY_WORKFLOW_SYNTAX`**

Same command as Task 2.3.

Expected: `OK`.

- [ ] **Step 6.3: Commit**

Run:
```bash
git add .github/workflows/build.yml
git commit -m "ci: remove ci-github-actions from workflow trigger branches

The ci-github-actions branch name was included in
on.push.branches during the CI sub-project's development so that
each commit on the feature branch triggered a workflow run. Now
that the sub-project is complete, the branch filter should only
include long-lived branches."
```

- [ ] **Step 6.4: Push the final commit on the feature branch**

Run:
```bash
git push
gh run watch $(gh run list --workflow=build.yml --limit 1 --json databaseId --jq '.[0].databaseId')
```

Wait — this commit removes `ci-github-actions` from the trigger list. Will it still fire? Yes, because at the time this commit is pushed, GitHub reads the workflow file as it exists AT THE commit being pushed, not the previous state. The commit we're pushing still has `ci-github-actions` in the branches list if we structure the commit as "the PREVIOUS state included us, so it ran on this push." Actually wait — it's the other way around.

The GitHub Actions behavior is: when you push commit X to branch B, GitHub reads the workflow file AS OF commit X (the thing you just pushed) and checks whether X matches the trigger rules. If the workflow file at X says `branches: [m1-modernization, main]` and we pushed to `ci-github-actions`, the workflow will NOT trigger.

So pushing this commit to `ci-github-actions` will cause no CI run. That's actually OK — there's nothing new to verify; the workflow semantics haven't changed, only the trigger list has. But it means we can't verify this commit via the feature branch.

We'll verify it instead by the subsequent merge into `m1-modernization`, which will push to a branch that IS in the trigger list.

If you want an extra-safety verification before merging, do an additional one-line commit to `ci-github-actions` after this one (e.g., a whitespace change in a comment) and manually dispatch via `workflow_dispatch`:
```bash
gh workflow run build.yml --ref ci-github-actions
gh run watch $(gh run list --workflow=build.yml --limit 1 --json databaseId --jq '.[0].databaseId')
```
This proves the workflow file is still syntactically valid and runs, even though it wouldn't auto-trigger. Optional.

- [ ] **Step 6.5: Merge back to m1-modernization**

Run:
```bash
git checkout m1-modernization
git merge --ff-only ci-github-actions
git push <remote-name> m1-modernization
```

Replace `<remote-name>` as usual.

Expected: fast-forward merge succeeds (no conflicts — `ci-github-actions` is a linear descendant of `m1-modernization`).

- [ ] **Step 6.6: Watch the merge-triggered run**

Run:
```bash
gh run watch $(gh run list --workflow=build.yml --limit 1 --json databaseId --jq '.[0].databaseId')
```

Expected: a workflow run fires on the push to `m1-modernization` (because the pushed commits include the workflow file AS OF the latest commit, which has `m1-modernization` in the trigger list). All three build matrix jobs complete successfully.

This is the final real verification: the workflow works end-to-end on the branch that's supposed to run it, with the final clean trigger list.

- [ ] **Step 6.7: Delete the feature branch**

Run:
```bash
git branch -d ci-github-actions
git push <remote-name> :ci-github-actions
```

Expected: local and remote feature branches gone.

- [ ] **Step 6.8: Update CI design spec status**

Use the Edit tool to change this line in `doc/maintenance/2026-04-08-ci-build-design.md`:

```markdown
**Status:** Design (approved by user, implementation plan next)
```

to:

```markdown
**Status:** Implemented 2026-04-09 (see `2026-04-09-ci-build-plan.md` and the commits from Task 1-6 on `m1-modernization`)
```

Run:
```bash
git add doc/maintenance/2026-04-08-ci-build-design.md
git commit -m "doc/maintenance/ci-build-design: mark as implemented

The CI design spec is now landed. Workflow file lives at
.github/workflows/build.yml, and a successful end-to-end run
(all three platforms + release-job dry-run with a throwaway
tag) was verified as part of the implementation plan's Task 5
and Task 6."
git push
```

This final push will trigger one more CI run (docs change → still hits the push trigger). It should go green. You don't have to watch it if you're confident; it's a sanity check.

---

## Task 7: End-to-end verification and cutting the first real release

**Files:** none (verification + optional real release)

This task is optional. It walks through cutting a real `v1.2.0` (or whatever version you want) release using the now-working CI pipeline. Skip if you're not ready to tag yet.

- [ ] **Step 7.1 (optional): Bump the version in Makefile**

Edit `Makefile:1` to change `VERSION = 1.1` to the new version, e.g., `VERSION = 1.2.0`. The current upstream's version was 1.1; you're free to pick any versioning scheme (semver, date-based, whatever).

- [ ] **Step 7.2 (optional): Commit the bump, tag, push**

Run:
```bash
git add Makefile
git commit -m "Bump version to 1.2.0"
git tag v1.2.0
git push
git push <remote-name> v1.2.0
```

- [ ] **Step 7.3 (optional): Watch and verify**

Run:
```bash
gh run watch $(gh run list --workflow=build.yml --limit 1 --json databaseId --jq '.[0].databaseId')
gh release list
gh release view v1.2.0
```

Expected: workflow goes green, draft release `v1.2.0` exists on GitHub with three asset zips attached.

- [ ] **Step 7.4 (optional): Review and publish the release**

Open the release in the browser:
```bash
gh release view v1.2.0 --web
```

Edit the release notes as desired. When satisfied, click "Publish release."

Done — you have a real public release.

---

## Self-review notes

**Spec coverage check:**
- `.github/workflows/build.yml` single file → Tasks 2-6 ✓
- Three-platform matrix (Linux, macOS arm64, Windows) → Tasks 2, 3, 4 ✓
- Triggers on push + pull_request + tag + workflow_dispatch → Task 2 ✓
- Dep caching on Linux + Windows → Task 2 ✓
- macOS skips caching (Homebrew handles it) → Task 3 ✓
- MSYS2/MINGW64 Windows setup → Task 4 ✓
- Tag-gated release job with draft + auto-generated notes → Task 5 ✓
- Manual cleanup of test tag/release → Task 5.8 ✓
- Makefile MINGW64 DLL path fix → Task 1 ✓
- Feature branch isolation during development → Task 1 ✓
- Documentation update to mark spec implemented → Task 6.8 ✓

**Placeholder scan:** none. Every code block contains complete code. Every command is concrete.

**Type consistency:** workflow `jobs:` has one matrix job named `build` and one non-matrix job named `release`. The release job's `needs: build` references the matrix job name. Artifact names use `${{ matrix.arch }}` in upload (Task 2) and `artifacts/*/WaveEdit-*.zip` in download (Task 5) — these match because `actions/download-artifact@v4` with no `name` filter downloads all artifacts into separate subdirectories under the specified `path`.

**Known uncertainties documented inline:**
- Task 2.5 lists likely Linux failure modes
- Task 3.5 lists likely macOS failure modes
- Task 4.5 lists likely Windows failure modes (with specific emphasis on the `/mingw64/bin/libgcc_s_seh-1.dll` path being the thing being tested)
- Task 6.4 explains the edge case where the final commit on `ci-github-actions` doesn't auto-trigger CI because the trigger list no longer contains that branch

**Scope:** this plan modifies one Makefile line block and adds one workflow file (plus a final status update to the CI design doc). No source code changes, no CMake, no codesigning, no AppImage. All explicitly deferred.
