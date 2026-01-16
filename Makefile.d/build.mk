# Makefile.d/build.mk

# Default preset can be overridden from the command line
# e.g., make build PRESET=arm64-release
PRESET ?= arm64-debug

.PHONY: all build clean configure help lint ready ready/fix test
build:
	@echo "--- Building project with preset: $(PRESET) ---"
	@cmake --build build/$(PRESET)
	@echo "--- Build complete ---"
