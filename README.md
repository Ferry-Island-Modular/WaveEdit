# Synthesis Technology WaveEdit

The wavetable and bank editor for the Synthesis Technology [E370](http://synthtech.com/eurorack/E370/) and [E352](http://synthtech.com/eurorack/E352/) Eurorack synthesizer modules.

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
