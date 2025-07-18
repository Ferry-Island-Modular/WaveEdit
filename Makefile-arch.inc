ifndef ARCH

# Detect architecture if ARCH is not defined
MACHINE = $(shell gcc -dumpmachine)
ifneq (,$(findstring linux,$(MACHINE)))
	# Linux
	ARCH = lin
else ifneq (,$(findstring apple,$(MACHINE)))
	# Mac - detect ARM64 vs x86_64
	CPU = $(shell uname -m)
	ifeq ($(CPU),arm64)
		ARCH = mac_arm64
	else
		ARCH = mac
	endif
else ifneq (,$(findstring mingw,$(MACHINE)))
	# Windows
	ARCH = win
else
$(error Could not detect architecture. Specify manually with `make ARCH=...`)
endif

endif