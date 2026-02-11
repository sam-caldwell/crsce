# Makefile.d/build.mk
# (c) 2026 Sam Caldwell. See LICENSE.txt for details
#
# Default preset can be overridden from the command line for single-preset builds
# e.g., make build/one PRESET=llvm-release
PRESET ?= llvm-debug
JOBS := $(shell cmake -P cmake/tools/print_num_cpus.cmake | tail -n1 | sed 's/[^0-9].*//')

# Compute the set of presets to build when running the aggregate 'build' target.
# Start with portable presets; add LLVM presets if Homebrew LLVM is available.
PRESETS_ALL := cmake-build-debug cmake-build-release arm64-debug arm64-release
ifeq ($(HAVE_LLVM),yes)
  PRESETS_ALL += llvm-debug llvm-release
endif

.PHONY: all build build/one clean configure help lint ready ready/fix test

# Build every configured preset, configuring first if necessary.
build:
	@echo "=== Building all presets: $(PRESETS_ALL) ==="
	@set -e; \
	for p in $(PRESETS_ALL); do \
	  echo "--- [$$p] configure ---"; \
	  cmake --preset=$$p -S .; \
	  echo "--- [$$p] build ---"; \
	  cmake --build build/$$p --parallel $(JOBS); \
	done; \
	echo "--- ✅ Build (all presets) complete ---"

# Build only the selected PRESET (expects it has been configured already)
build/one:
	@echo "--- Building project with preset: $(PRESET) ---"
	@cmake --build build/$(PRESET) --parallel $(JOBS) || { echo "❌ build failed"; exit 1; }
	@echo "--- ✅ Build complete ---"
