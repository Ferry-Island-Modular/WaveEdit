This is a beta build of WaveEdit. Back up important banks before editing them
and please report any unexpected behavior.

## Downloads

- **macOS:** `WaveEdit-*-mac_arm64.zip` (Apple Silicon, macOS 15 or newer)
- **Windows:** `WaveEdit-*-win.zip` (64-bit Windows)
- **Linux AppImage:** `WaveEdit-*-x86_64.AppImage` (recommended)
- **Linux folder:** `WaveEdit-*-lin.zip`

The builds are not yet code-signed. On macOS, Control-click `WaveEdit.app`,
choose **Open**, and confirm the prompt. On Windows, SmartScreen may require
choosing **More info** and then **Run anyway**. On Linux, make the AppImage
executable before launching it:

```sh
chmod +x WaveEdit-*-x86_64.AppImage
./WaveEdit-*-x86_64.AppImage
```

## Suggested beta checks

1. Launch WaveEdit from a fresh download.
2. Open, edit, save, and reopen banks at 256, 512, 1024, and 2048 samples per
   wave.
3. Move the playback frequency both up and down and check its audible pitch.
4. Reorder page tabs and confirm that every tab remains selectable.
5. Change themes, restart WaveEdit, and confirm the selection is retained.
6. Import and export WAV files.
7. On a HiDPI display, check that the interface is a comfortable size; the
   **UI Scale** menu (next to Theme) can adjust it live, and "Auto" should
   match your desktop's scaling on Linux.

When reporting a problem, include your operating system and version, computer
architecture, WaveEdit version, reproduction steps, and the affected WAV file
when possible.
