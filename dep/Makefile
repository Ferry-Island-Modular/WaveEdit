
LOCAL = $(PWD)

include ../Makefile-arch.inc
ifneq (,$(filter $(ARCH),mac mac_arm64))
	export CFLAGS = \
		-mmacosx-version-min=11.0
	export CXXFLAGS = $(CFLAGS)
	export LDFLAGS = $(CFLAGS)
endif

sdl2 = SDL2-2.30.8
libsndfile = libsndfile-1.0.28
libsamplerate = libsamplerate-0.1.9

# This instance of make should be serialized, but -j flags are passed down to each recursive Makefile
.NOTPARALLEL:

ifneq (,$(filter $(ARCH),mac mac_arm64))
all: homebrew-deps
else
all: $(sdl2) $(libsndfile) $(libsamplerate)
endif

homebrew-deps:
	@echo "Using Homebrew dependencies on macOS..."
	@which brew > /dev/null || (echo "Error: Homebrew not found. Please install Homebrew first." && exit 1)
	brew install sdl2 libsndfile libsamplerate
	@echo "Creating symlinks for compatibility..."
	@mkdir -p lib include
	@ln -sf $$(brew --prefix)/lib/libSDL2-2.0.0.dylib lib/ || true
	@ln -sf $$(brew --prefix)/lib/libsndfile.1.dylib lib/ || true
	@ln -sf $$(brew --prefix)/lib/libsamplerate.0.dylib lib/ || true
	@ln -sf $$(brew --prefix)/include/SDL2 include/ || true

$(sdl2):
	wget -nc https://github.com/libsdl-org/SDL/releases/download/release-2.30.8/$@.tar.gz
	tar xf $@.tar.gz
	cd $@ && ./configure --prefix="$(LOCAL)" --enable-input-tslib=no
	$(MAKE) -C $@
	$(MAKE) -C $@ install

$(libsndfile):
	wget -nc http://www.mega-nerd.com/libsndfile/files/$@.tar.gz
	tar xf $@.tar.gz
	cd $@ && ./configure --prefix="$(LOCAL)"
	$(MAKE) -C $@
	$(MAKE) -C $@ install

$(libsamplerate):
	wget -nc http://www.mega-nerd.com/SRC/$@.tar.gz
	tar xf $@.tar.gz
	cd $@ && ./configure --prefix="$(LOCAL)"
	$(MAKE) -C $@
	$(MAKE) -C $@ install

clean:
	git clean -fdx