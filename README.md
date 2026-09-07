# WaveEdit (Ferry Island Modular fork)

A maintained fork of [Synthesis Technology WaveEdit](https://github.com/AndrewBelt/WaveEdit),
the wavetable and bank editor originally written for the
[E370](http://synthtech.com/eurorack/E370/) and
[E352](http://synthtech.com/eurorack/E352/) Eurorack modules.

Upstream has not seen a release since 2018. This fork keeps the tool
building and running on current systems, and extends it so it can author
wavetables for hardware beyond the E370 and E352, including Ferry Island
Modular's [Four Seas](https://ferryislandmodular.com).

## Download

Prebuilt beta packages for macOS (Apple Silicon), Windows (x64), and
Linux (x86-64 AppImage and folder) are on the
[releases page](https://github.com/Ferry-Island-Modular/WaveEdit/releases).
The builds are not code-signed yet; the release notes explain how to get
past the first-launch warning on each platform.

## What the fork adds

- **Configurable wave width.** Banks can be 256, 512, 1024, or 2048
  samples per wave (`Bank > Samples per Wave`). Changing the width
  resamples all 64 waves, and opening a mono bank WAV with 64 waves of a
  supported width selects that width automatically. Catalog waves are
  resampled to the active width when inserted.
- **HiDPI support** with a UI scale setting.
- **Themes and fonts.** Eight base16 colour schemes, Inter and JetBrains
  Mono rendered through FreeType, and a decluttered grid at wide sample
  counts.
- **Native ImGui page tabs** and assorted layout and focus fixes.
- **Modern builds.** Dear ImGui updated from a 2017 snapshot to v1.92,
  the other vendored dependencies moved to their current upstreams, an
  Apple Silicon build with a self-contained app bundle, a Windows build
  that bundles its full DLL closure, a Linux AppImage, and CI that
  produces all of them from a tag.
- Fixes for the playback-frequency slider feedback from the audio thread
  and for native file dialogs on Linux (via `zenity`).

## Using it with Four Seas

Four Seas reads wavetables from an SD card as `/<bank 1-12>/<page 1-8>.wav`,
where each page is 64 single-cycle waves of 2048 samples laid out as an
8 x 8 grid. That is exactly one WaveEdit bank at 2048 samples per wave:

1. Set `Bank > Samples per Wave` to 2048.
2. Build your 64 waves. Wave 1 is the top-left cell of the page; waves
   run left to right along X, then down along Y.
3. Save the bank as a WAV and copy it to the card as `<page>.wav` inside
   the bank folder you want it in.

Eight such banks make one Four Seas bank folder, with the Z axis morphing
between pages. For turning existing audio into whole banks automatically,
see [Harbor](https://github.com/Ferry-Island-Modular/Harbor).

### Building

Make dependencies with

	cd dep
	make

Clone the in-source dependencies.

	cd ..
	git submodule update --init --recursive

Compile the program. The Makefile will automatically detect your operating system.

	make

Launch the program.

	./WaveEdit

On Linux, native file dialogs require `zenity` to be available on `PATH`.

Linux releases also provide a portable AppImage with the application libraries
bundled. Make it executable and launch it directly:

	chmod +x WaveEdit-*-x86_64.AppImage
	./WaveEdit-*-x86_64.AppImage

You can even try your luck with building the polished distributable. Although this method is unsupported, it may work with some tweaks to the Makefile.

	make dist

Linux maintainers can build the AppImage after downloading `linuxdeploy`:

	make appimage LINUXDEPLOY=./linuxdeploy-x86_64.AppImage

## Maintenance notes

Design notes, audits, and the roadmap for the fork live in
[`doc/maintenance/`](doc/maintenance/). Start with
[`future-improvements.md`](doc/maintenance/future-improvements.md).

## Credits and license

WaveEdit was written by Andrew Belt for Synthesis Technology. This fork
is maintained by Ferry Island Modular. See [`LICENSE.txt`](LICENSE.txt)
for the license, which is unchanged from upstream.
