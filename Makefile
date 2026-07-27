VERSION = 1.2.0

LINUXDEPLOY ?= linuxdeploy-x86_64.AppImage
APPIMAGE_DIR = dist/WaveEdit.AppDir
APPIMAGE_OUTPUT = dist/WaveEdit-$(VERSION)-x86_64.AppImage

FLAGS = -Wall -Wextra -Wno-unused-parameter -g -Wno-unused -O3 -ffast-math \
	-DVERSION=$(VERSION) -DPFFFT_SIMD_DISABLE \
	-DIMGUI_USER_CONFIG=\"src/imconfig_user.h\" \
	-I. -Iext -Iext/imgui -Idep/include -Idep/include/SDL2

# Architecture-specific optimizations
ifeq ($(ARCH),mac_arm64)
	FLAGS += -march=native
else ifeq ($(ARCH),mac)
	FLAGS += -march=nocona
else ifneq (,$(filter $(ARCH),lin win))
	FLAGS += -march=nocona
endif
CFLAGS =
CXXFLAGS = -std=c++11
LDFLAGS =


SOURCES = \
	ext/pffft/pffft.c \
	ext/lodepng/lodepng.cpp \
	ext/osdialog/osdialog.c \
	ext/imgui/imgui.cpp \
	ext/imgui/imgui_draw.cpp \
	ext/imgui/imgui_demo.cpp \
	ext/imgui/imgui_tables.cpp \
	ext/imgui/imgui_widgets.cpp \
	ext/imgui/misc/freetype/imgui_freetype.cpp \
	ext/imgui/backends/imgui_impl_sdl2.cpp \
	ext/imgui/backends/imgui_impl_opengl2.cpp \
	$(wildcard src/*.cpp)


# OS-specific
include Makefile-arch.inc
ifeq ($(ARCH),lin)
	# Linux
	FLAGS += -DARCH_LIN $(shell pkg-config --cflags freetype2) $(shell pkg-config --cflags sdl2)
	LDFLAGS += -static-libstdc++ -static-libgcc \
		-lGL -lpthread \
		-Ldep/lib -lSDL2 -lsamplerate -lsndfile \
		$(shell pkg-config --libs freetype2)
	SOURCES += ext/osdialog/osdialog_zenity.c
else ifneq (,$(filter $(ARCH),mac mac_arm64))
	# Mac (Intel or Apple Silicon)
	FLAGS += -DARCH_MAC \
		-mmacosx-version-min=11.0 \
		-I$(shell brew --prefix)/include -I$(shell brew --prefix)/include/SDL2 \
		-I$(shell brew --prefix freetype)/include/freetype2
	CXXFLAGS += -stdlib=libc++
	LDFLAGS += -mmacosx-version-min=11.0 \
		-stdlib=libc++ -lpthread \
		-framework Cocoa -framework OpenGL -framework IOKit -framework CoreVideo \
		$(shell brew --prefix sdl2)/lib/libSDL2-2.0.0.dylib \
		$(shell brew --prefix libsamplerate)/lib/libsamplerate.0.dylib \
		$(shell brew --prefix libsndfile)/lib/libsndfile.1.dylib \
		$(shell brew --prefix freetype)/lib/libfreetype.6.dylib
	SOURCES += ext/osdialog/osdialog_mac.m
ifeq ($(ARCH),mac_arm64)
	FLAGS += -DARCH_ARM64
endif
else ifeq ($(ARCH),win)
	# Windows
	FLAGS += -DARCH_WIN -I/mingw64/include/freetype2 -I/mingw64/include/SDL2
	LDFLAGS += \
		-Ldep/lib -lmingw32 -lSDL2main -lSDL2 -lsamplerate -lsndfile \
		-lopengl32 -mwindows -lfreetype
	SOURCES += ext/osdialog/osdialog_win.c
	OBJECTS += info.o
info.o: info.rc
	windres $^ $@
endif


.DEFAULT_GOAL := build
build: WaveEdit

run: WaveEdit
ifneq (,$(filter $(ARCH),mac mac_arm64))
	cd "$(shell pwd)" && ./WaveEdit
else
	LD_LIBRARY_PATH=dep/lib ./WaveEdit
endif

debug: WaveEdit
ifneq (,$(filter $(ARCH),mac mac_arm64))
	lldb ./WaveEdit
else
	gdb -ex 'run' ./WaveEdit
endif


OBJECTS += $(SOURCES:%=build/%.o)


WaveEdit: $(OBJECTS)
	$(CXX) -o $@ $^ $(LDFLAGS)

clean:
	rm -frv $(OBJECTS) WaveEdit dist


.PHONY: dist
dist: WaveEdit
	mkdir -p dist/WaveEdit
	cp -R banks dist/WaveEdit
	cp LICENSE* dist/WaveEdit
	cp doc/manual.pdf dist/WaveEdit
ifeq ($(ARCH),lin)
	cp -R logo*.png fonts catalog themes dist/WaveEdit
	cp WaveEdit WaveEdit.sh dist/WaveEdit
	cp dep/lib/libSDL2-2.0.so.0 dist/WaveEdit
	cp dep/lib/libsamplerate.so.0 dist/WaveEdit
	cp dep/lib/libsndfile.so.1 dist/WaveEdit
else ifneq (,$(filter $(ARCH),mac mac_arm64))
	mkdir -p dist/WaveEdit/WaveEdit.app/Contents/MacOS
	mkdir -p dist/WaveEdit/WaveEdit.app/Contents/Resources
	cp Info.plist dist/WaveEdit/WaveEdit.app/Contents
	cp WaveEdit dist/WaveEdit/WaveEdit.app/Contents/MacOS
	cp -R logo*.png logo.icns fonts catalog themes dist/WaveEdit/WaveEdit.app/Contents/Resources
	# Remap dylibs in executable
	otool -L dist/WaveEdit/WaveEdit.app/Contents/MacOS/WaveEdit
	cp $(shell brew --prefix sdl2)/lib/libSDL2-2.0.0.dylib dist/WaveEdit/WaveEdit.app/Contents/MacOS
	install_name_tool -change $(shell brew --prefix sdl2)/lib/libSDL2-2.0.0.dylib @executable_path/libSDL2-2.0.0.dylib dist/WaveEdit/WaveEdit.app/Contents/MacOS/WaveEdit
	cp $(shell brew --prefix libsamplerate)/lib/libsamplerate.0.dylib dist/WaveEdit/WaveEdit.app/Contents/MacOS
	install_name_tool -change $(shell brew --prefix libsamplerate)/lib/libsamplerate.0.dylib @executable_path/libsamplerate.0.dylib dist/WaveEdit/WaveEdit.app/Contents/MacOS/WaveEdit
	cp $(shell brew --prefix libsndfile)/lib/libsndfile.1.dylib dist/WaveEdit/WaveEdit.app/Contents/MacOS
	install_name_tool -change $(shell brew --prefix libsndfile)/lib/libsndfile.1.dylib @executable_path/libsndfile.1.dylib dist/WaveEdit/WaveEdit.app/Contents/MacOS/WaveEdit
	cp $(shell brew --prefix freetype)/lib/libfreetype.6.dylib dist/WaveEdit/WaveEdit.app/Contents/MacOS
	install_name_tool -change $(shell brew --prefix freetype)/lib/libfreetype.6.dylib @executable_path/libfreetype.6.dylib dist/WaveEdit/WaveEdit.app/Contents/MacOS/WaveEdit
	otool -L dist/WaveEdit/WaveEdit.app/Contents/MacOS/WaveEdit
	# Re-sign the app bundle after install_name_tool modified the binary.
	# Without this, macOS reports "damaged and can't be opened" because
	# install_name_tool invalidates the ad-hoc signature that clang created.
	codesign --force --deep --sign - dist/WaveEdit/WaveEdit.app
else ifeq ($(ARCH),win)
	cp -R logo*.png fonts catalog themes dist/WaveEdit
	cp WaveEdit.exe dist/WaveEdit
	cp /mingw64/bin/libgcc_s_seh-1.dll dist/WaveEdit
	cp /mingw64/bin/libwinpthread-1.dll dist/WaveEdit
	cp /mingw64/bin/libstdc++-6.dll dist/WaveEdit
	cp dep/bin/SDL2.dll dist/WaveEdit
	cp dep/bin/libsamplerate-0.dll dist/WaveEdit
	cp dep/bin/libsndfile-1.dll dist/WaveEdit
	cp /mingw64/bin/libfreetype-6.dll dist/WaveEdit
endif
	cd dist && zip -9 -r WaveEdit-$(VERSION)-$(ARCH).zip WaveEdit


.PHONY: appimage
ifeq ($(ARCH),lin)
appimage: WaveEdit
	rm -rf $(APPIMAGE_DIR) $(APPIMAGE_OUTPUT)
	mkdir -p $(APPIMAGE_DIR)/usr/bin
	mkdir -p $(APPIMAGE_DIR)/usr/share/waveedit
	mkdir -p $(APPIMAGE_DIR)/apprun-hooks
	cp WaveEdit $(APPIMAGE_DIR)/usr/bin
	cp -R banks catalog fonts themes $(APPIMAGE_DIR)/usr/share/waveedit
	cp logo-dark.png logo-light.png doc/manual.pdf LICENSE* $(APPIMAGE_DIR)/usr/share/waveedit
	cp packaging/linux/appimage-resource-dir.sh $(APPIMAGE_DIR)/apprun-hooks
	LDAI_OUTPUT=$(APPIMAGE_OUTPUT) LINUXDEPLOY_OUTPUT_VERSION=$(VERSION) $(LINUXDEPLOY) \
		--appdir $(APPIMAGE_DIR) \
		--executable $(APPIMAGE_DIR)/usr/bin/WaveEdit \
		--desktop-file packaging/linux/waveedit.desktop \
		--icon-file packaging/linux/waveedit.png \
		--output appimage
else
appimage:
	$(error AppImage packaging requires ARCH=lin)
endif


# SUFFIXES:

build/%.c.o: %.c
	@mkdir -p $(@D)
	$(CC) $(FLAGS) $(CFLAGS) -c -o $@ $<

build/%.cpp.o: %.cpp
	@mkdir -p $(@D)
	$(CXX) $(FLAGS) $(CXXFLAGS) -c -o $@ $<

build/%.m.o: %.m
	@mkdir -p $(@D)
	$(CC) $(FLAGS) $(CFLAGS) -c -o $@ $<
