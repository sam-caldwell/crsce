# Makefile.d/configure.mk
# (c) 2026 Sam Caldwell. See LICENSE.txt for details
#
# Default preset can be overridden from the command line
# e.g., make configure PRESET=arm64-release
PRESET ?= arm64-debug

.PHONY: all build clean configure help lint ready ready/fix test
configure:
	@echo "--- Configuring project with preset: $(PRESET) ---"
	@cmake --preset=$(PRESET) -S .
	@echo "--- Configuration complete ---"
