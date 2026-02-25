# Makefile.d/configure.mk
# (c) 2026 Sam Caldwell. See LICENSE.txt for details
#
# Default preset can be overridden from the command line
# e.g., make configure PRESET=llvm-release
PRESET ?= llvm-debug

# On macOS, auto-export metal-cpp includes; presets support Metal optionally.
UNAME_S := $(shell uname -s 2>/dev/null || echo unknown)
ifeq ($(UNAME_S),Darwin)
  export CRSCE_METAL_CPP_INCLUDE ?= $(CURDIR)/deps/metal-cpp
endif

# Aggregate list used by configure/all
PRESETS_ALL := llvm-debug llvm-release arm64-release

.PHONY: all build clean configure help lint ready ready/fix test

configure:
	@echo "--- Configuring project with preset: $(PRESET) ---"
	@echo "    CRSCE_METAL_CPP_INCLUDE=$(CRSCE_METAL_CPP_INCLUDE)"
	@cmake --preset=$(PRESET) -S . -DCRSCE_SOLVER_CONSTRAINT=ON -DCRSCE_ENABLE_METAL=ON
	@echo "--- Building tool dependencies (make deps) ---"
	@$(MAKE) deps

# Note: legacy per-dir configure targets removed; 'make configure' sets Metal+Constraint globally
