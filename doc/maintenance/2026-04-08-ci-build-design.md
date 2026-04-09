# CI / Cross-Platform Build Design

**Date:** 2026-04-08
**Status:** Design (approved by user, implementation plan next)
**Sub-project of:** WaveEdit maintained-fork stewardship
**Predecessor:** `2026-04-08-fork-research.md`
**Depends on:** ~~Submodule sourcing audit & upgrade sub-project~~ — **satisfied** as of 2026-04-09 (see `2026-04-09-submodule-upgrade-plan.md` and commits `bf5da97`..`34d9ce2` on `m1-modernization`). Modern `ocornut/imgui` v1.92.7, canonical `jpommier/pffft`, current `lvandeve/lodepng` are all in place. `ext/osdialog` pin was deliberately held for the future Linux zenity sub-project; this does not block CI because the Linux job will still compile the current `osdialog_gtk2.c` which is present in the stale pin.

**Implementation notes discovered during the submodule upgrade** (2026-04-09) that affect CI:

1. The Makefile now passes `-DIMGUI_USER_CONFIG=\"src/imconfig_user.h\"` to enable 32-bit draw indices. No CI impact — `make` reads the Makefile.
2. `ext/imgui` added four source files (`imgui_tables.cpp`, `imgui_widgets.cpp`, `backends/imgui_impl_sdl2.cpp`, `backends/imgui_impl_opengl2.cpp`). Build time per fresh run is slightly higher; dep cache still works unchanged.
3. `ext/pffft` now clones from `bitbucket.org/jpommier/pffft.git` instead of GitHub. CI runners can fetch Bitbucket over HTTPS without any special setup — it works identically to GitHub for `git submodule update --init --recursive`. Worth a note in the workflow file comments for future readers.
4. Linux build against the current `ext/osdialog` pin (`e66caf0`, which still has `osdialog_gtk2.c`) will need `libgtk2.0-dev` in the apt install step as the CI design already specifies. When the zenity sub-project lands, that apt line changes to install `zenity` instead of GTK dev headers, and the Linux Makefile entry swaps `osdialog_gtk2.c` for `osdialog_zenity.c`.

## Goal

Stand up GitHub Actions CI that:
1. Builds WaveEdit on every push and PR for Linux, macOS (Apple Silicon only), and Windows.
2. On `v*` tag push, builds release artifacts and creates a draft GitHub Release with the zips attached.

## Non-goals (deferred to other sub-projects)

- Code-signing / notarization (mac + win)
- AppImage for Linux
- Smoke-testing the GUI beyond verifying the binary exists and reports the expected architecture
- MSVC or CMake migration
- Switching macOS dep handling off Homebrew (prerequisite for universal binary)
- Cherry-picking source-code changes from other forks (`fixWorkingDirectory()`, zenity backend, etc.)
- Pinning or vendoring `AndrewBelt/imgui` and `AndrewBelt/osdialog` submodules
- Auto-version-bumping; `Makefile:1` `VERSION = 1.1` is bumped manually before tagging
- Smoke tests requiring xvfb or audio-device emulation
- Intel macOS CI (see "Why no Intel mac" below)

## Why no Intel mac in CI

GitHub retired the last free Intel macOS runner (`macos-13`) some time before 2026-04. All current Intel x86_64 macOS runner labels (`macos-14-large`, `macos-15-large`, `macos-15-intel`, `macos-26-large`, `macos-26-intel`, `macos-latest-large`) carry the `-large`/`-intel` suffix and fall under "larger runners," which per GitHub billing docs are *"always charged for, even when used by public repositories or when you have quota available from your plan."* Estimated cost for typical small-project usage is ~$30/month for an Intel mac job.

This fork is public and free CI is a hard constraint. Therefore:

- The CI matrix is **arm64-only on macOS**.
- The Makefile retains its `mac` (Intel) target unchanged. Anyone with an Intel Mac can build locally with `make ARCH=mac`.
- The long-term fix is a **universal binary** (`-arch arm64 -arch x86_64`) built on a single arm64 runner. This requires switching macOS dep handling off Homebrew (Homebrew cannot provide both arches side-by-side). That work is deferred to its own sub-project.

## Linux as first-class

Per the user's stated goal, Linux is on equal footing with macOS and Windows in this fork. The first CI cut ships a plain tarball that needs gtk2 at runtime — matching what every other WaveEdit fork does — but the explicit roadmap is:

1. **Now:** ubuntu-22.04 build, gtk2 runtime dep, plain tarball. (this sub-project)
2. **Fast-follow sub-project:** Switch dialog backend from `osdialog_gtk2.c` to `osdialog_zenity.c`. Removes the gtk2 link-time dep entirely; runtime needs only `zenity` or `kdialog` (which most desktops already have).
3. **Later sub-project:** Ship as AppImage for true portability across distros.

This sub-project covers only step 1. Steps 2 and 3 are explicitly out of scope here but flagged for future planning.

## Architecture

### File layout

A single workflow file: **`.github/workflows/build.yml`**.

One file rather than separate `ci.yml` and `release.yml`. Reasoning: 90% of the steps are identical between PR builds and release builds; splitting them produces two files that have to be kept in lockstep. Instead, the workflow triggers on both push/PR **and** tags, with release-only steps gated by `if: startsWith(github.ref, 'refs/tags/v')`. One file, one source of truth.

### Triggers

```yaml
on:
  push:
    branches: [m1-modernization, main]
    tags: ['v*']
  pull_request:
    branches: [m1-modernization, main]
  workflow_dispatch:
```

`workflow_dispatch` provides a manual "Run workflow" button in the Actions UI for ad-hoc sanity builds.

### Build matrix

Three parallel jobs:

| `arch` | `runner` | Notes |
|---|---|---|
| `lin` | `ubuntu-22.04` | Pinned, not `ubuntu-latest` — gives reproducible glibc baseline (2.35); supported until 2027 |
| `mac_arm64` | `macos-15` | Apple Silicon; `macos-15` chosen over `macos-14` (deprecation-approaching) and `macos-26` (newer than necessary) |
| `win` | `windows-latest` | Host image version barely matters since MSYS2 installs the toolchain inside the runner |

### Job sequence (per-platform)

Each matrix job runs the same logical sequence:

1. **Checkout** with `submodules: recursive` (the four `ext/` submodules: `imgui`, `lodepng`, `pffft`, `osdialog`)
2. **Install platform toolchain** (apt / brew / msys2 — see §"Platform-specific install" below)
3. **Restore `dep/` cache** (Linux + Windows only; macOS skipped because Homebrew bottle install is fast and has its own caching)
4. **Build deps** — `cd dep && make` (cache miss only on lin/win; mac always runs since brew is fast and idempotent)
5. **Build app** — `make` (the Makefile auto-detects arch via `Makefile-arch.inc`)
6. **Verify binary** — `file WaveEdit` (or `WaveEdit.exe`) reports the expected architecture; `test -x WaveEdit`. Nothing more.
7. **Build dist zip** — `make dist`. Always runs, even on PR builds; cheap and gives "did packaging break?" signal.
8. **Upload artifact** — `actions/upload-artifact@v4` with the zip; 30-day retention on PR builds, 90-day on push to default branch
9. **(Tag builds only)** Skipped here; the release job downstream handles uploads to the GitHub Release.

### Platform-specific install steps

**Linux (`ubuntu-22.04`):**
```yaml
- run: sudo apt-get update
- run: sudo apt-get install -y build-essential pkg-config libgtk2.0-dev wget zip
```
GTK2 dev headers are still in 22.04's universe repo.

**macOS (`macos-15`):**
```yaml
- run: brew install sdl2 libsamplerate libsndfile
```
Skip the `dep/Makefile` `homebrew-deps` target's bootstrap dance — it just runs the same brew install plus symlinks. The Makefile already detects ARM via `uname -m` so no further tweaking.

**Windows (`windows-latest`):**
```yaml
- uses: msys2/setup-msys2@v2
  with:
    msystem: MINGW64
    install: >-
      mingw-w64-x86_64-gcc
      mingw-w64-x86_64-make
      mingw-w64-x86_64-pkg-config
      wget tar zip unzip
- shell: msys2 {0}
  run: cd dep && make
- shell: msys2 {0}
  run: make ARCH=win
```
**Important deviation from current Makefile:** the Makefile (`Makefile:127-129`) copies DLLs from `/mingw32/bin`, but CI uses **MINGW64** (x86_64) not MINGW32. `make dist` will fail on Windows without a fix. See §"Required code changes" §1 below.

### Caching

```yaml
- uses: actions/cache@v4
  if: matrix.arch == 'lin' || matrix.arch == 'win'
  with:
    path: |
      dep/lib
      dep/include
      dep/bin
    key: dep-${{ matrix.arch }}-${{ hashFiles('dep/Makefile') }}
```

Cache key is bound to a hash of `dep/Makefile`. The cache invalidates the moment a dependency version is bumped in that file. macOS skipped — Homebrew bottle install is ~30s and has its own internal caching on GH-hosted runners.

### Release pipeline

Triggered when a tag matching `v*` is pushed. After all three matrix jobs complete successfully, a `release` job runs:

```yaml
release:
  needs: build
  if: startsWith(github.ref, 'refs/tags/v')
  runs-on: ubuntu-latest
  permissions:
    contents: write
  steps:
    - uses: actions/download-artifact@v4
      with:
        path: artifacts
    - run: |
        gh release create "${{ github.ref_name }}" \
          --draft \
          --generate-notes \
          --title "${{ github.ref_name }}" \
          artifacts/*/WaveEdit-*.zip
      env:
        GH_TOKEN: ${{ github.token }}
```

Releases are created **as drafts**. Manual review and publish. This is deliberate: the first time auto-release misbehaves, you do not want it to have already pushed a broken zip to a public downloads page.

To cut a release:
1. Bump `VERSION = 1.1` in `Makefile:1` to the new version
2. Commit the bump
3. `git tag v1.2.0 && git push origin v1.2.0`
4. Wait for CI; review and publish the draft release

## Required code changes

These are small, scoped changes to the existing build that the CI work depends on. They are not separate sub-projects.

1. **`Makefile:127-129`** — fix the hardcoded `/mingw32/bin` DLL paths for `make dist` on Windows. The MSYS2 environment we use in CI is MINGW64, so the DLLs live at `/mingw64/bin/libgcc_s_*-1.dll`, `/mingw64/bin/libwinpthread-1.dll`, `/mingw64/bin/libstdc++-6.dll`. Two acceptable approaches:
   - **(a)** Hard-switch to `/mingw64/bin`. Simple. Implies "we only support MINGW64 for Windows builds going forward," which is fine — 32-bit Windows is no longer worth targeting.
   - **(b)** Detect `MSYSTEM` at dist time and pick the right path. Slightly more general; preserves the ability to build under MINGW32 if anyone ever wants to.
   - **Recommendation: (a).** Simpler, and 32-bit Windows is not a priority.
   - Note also that the libgcc DLL name differs: 32-bit MinGW is `libgcc_s_dw2-1.dll`, 64-bit is `libgcc_s_seh-1.dll`. Adjust accordingly.

2. **`.github/workflows/build.yml`** — new file (the workflow itself). Net new addition.

3. **No changes to `Makefile-arch.inc`** — `gcc -dumpmachine` correctly identifies all three CI environments.

4. **No changes to `dep/Makefile`** — works as-is on Linux/Windows; macOS path runs `homebrew-deps` which we bypass in CI by running `brew install` directly.

## Risks & open issues

1. **Supply-chain risk on submodules.** `ext/imgui` and `ext/osdialog` point to `AndrewBelt/`-owned repos that are themselves unmaintained. If those repos disappear, fresh clones break and CI breaks with them. Not addressed in this sub-project but flagged for a future supply-chain hardening sub-project. Mitigation options for later: vendor the relevant subdirs into the main repo, switch to upstream `ocornut/imgui` and `andlabs/osdialog` (or whichever is the canonical osdialog), or maintain forks under the same account as the WaveEdit fork itself.
2. **Linux gtk2 fragility.** This CI ships a binary that needs gtk2 at runtime. Per the "Linux first-class" goal, the fast-follow zenity-backend swap addresses this — explicit dependency for the next sub-project.
3. **Windows DLL path bug** (item §"Required code changes" §1). Will be discovered immediately on the first CI run. Fix is small.
4. **`-large` runner billing assumption.** This design assumes that `macos-*-large` and `macos-*-intel` runners are billed even on public repos. The GitHub docs are slightly ambiguous on this point. If it turns out Intel runners are actually free for public repos, we can revisit and add Intel mac to the matrix. The cost of being wrong in the conservative direction is "we don't have Intel mac CI for a while," which is acceptable.
5. **`ubuntu-22.04` deprecation timeline.** Ubuntu 22.04 is supported until 2027. When it's retired we'll need to bump to a newer pinned LTS and verify gtk2 still ships. Low-priority concern for now.

## Implementation order (for the eventual plan)

This belongs in the writing-plans phase, but sketched here for grounding:

1. Fix `Makefile:127-129` MINGW64 DLL paths first (small, isolated, can be done locally).
2. Create `.github/workflows/build.yml` with only the Linux job. Verify it goes green.
3. Add the macOS job. Verify it goes green.
4. Add the Windows job. This is where the Makefile fix proves itself.
5. Add the `release` job. Test by pushing a `v0.0.1-test` throwaway tag.
6. Delete the test tag and release. Bump `VERSION` to whatever the real next version is. Document the release process in `doc/maintenance/release-process.md` (a separate small companion doc, not part of this spec).

## What this design does NOT decide

- The exact YAML — that is the implementation plan's job. This spec describes structure, sequence, and rationale.
- The next sub-project after this one (likely either zenity backend swap or thread-safety/TODO cleanup; user's choice).
- Anything about source code beyond the single MINGW64 DLL-path fix.
