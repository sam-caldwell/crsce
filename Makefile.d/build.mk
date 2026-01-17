# Makefile.d/build.mk
# (c) 2026 Sam Caldwell. See LICENSE.txt for details
#
# Default preset can be overridden from the command line
# e.g., make build PRESET=llvm-release
PRESET ?= llvm-debug

.PHONY: all build clean configure help lint ready ready/fix test
build:
	@echo "--- Building project with preset: $(PRESET) ---"
	@cmake --build build/$(PRESET) || {echo "❌ build failed" && exit 1}
	@echo "--- ✅ Build complete ---"
