# Makefile.d/configure.mk
# (c) 2026 Sam Caldwell. See LICENSE.txt for details
#
# Default preset can be overridden from the command line
# e.g., make configure PRESET=llvm-release
PRESET ?= llvm-release

# Aggregate list used by configure/all
PRESETS_ALL := arm64-release
ifeq ($(HAVE_LLVM),yes)
  PRESETS_ALL += llvm-release
endif

.PHONY: all build clean configure configure/all help lint ready ready/fix test

configure:
	@echo "--- Configuring project with preset: $(PRESET) ---"
	@cmake --preset=$(PRESET) -S .

configure/all:
	@echo "=== Configuring all presets: $(PRESETS_ALL) ==="
	@set -e; \
	for p in $(PRESETS_ALL); do \
	  echo "--- [$$p] configure ---"; \
	  cmake --preset=$$p -S .; \
	done; \
	echo "--- ✅ Configure (all presets) complete ---"
