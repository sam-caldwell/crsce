# Makefile
# (c) 2026 Sam Caldwell. See LICENSE.txt for details

# Define local environment directories
VENV_DIR := $(CURDIR)/venv
NODE_DIR := $(CURDIR)/node
NODE_BIN_DIR := $(NODE_DIR)/node_modules/.bin
BUILD_DIR := $(CURDIR)/build
LLVMBIN := /opt/homebrew/opt/llvm/bin

# Choose a sensible default preset: prefer Homebrew LLVM if present, otherwise use a portable debug preset
HAVE_LLVM := $(shell [ -x "$(LLVMBIN)/clang++" ] && echo yes || echo no)
ifeq ($(HAVE_LLVM),yes)
  PRESET ?= llvm-debug
else
  PRESET ?= cmake-build-debug
endif
export PRESET

# Add local binaries to the PATH for make commands
export PATH := $(LLVMBIN):$(VENV_DIR)/bin:$(NODE_BIN_DIR):$(PATH)

# Add local binaries to the PATH for make commands
export PATH := $(LLVMBIN):$(VENV_DIR)/bin:$(NODE_BIN_DIR):$(PATH)

.PHONY: all build clean configure help lint ready ready/fix test whitespace/fix
help:
	@echo "Available targets:"
	@echo "  all       - Run clean configure lint build test"
	@echo "  clean     - Delete and recreate the build/ directory"
	@echo "  configure - Configure the build pipeline"
	@echo "  lint      - Run configured linters"
	@echo "  lint/tools- Lint only tools/**/*.cpp"
	@echo "  build     - Build current preset (Metal + Constraint)"
	@echo "  deps      - Discover and build all tools/** CMake projects"
	@echo "  test      - Build and run all test programs"
	@echo "  test/uselessMachine           - Run uselessTest on sample mp4"
	@echo "  test/{ones, zeroes, random, alternating01, alternating10} - Run only a given test runner"
	@echo "  ci/scan-logs-alternating - Non-fatal GOBP scan on alternating logs (if present)"
	@echo "  ci/no-gobp-deterministic - Verify zeroes/ones decompressor logs contain no GOBP markers"
	@echo "  cover     - Ensure minimum test coverage threshold is maintained"
	@echo "  ready     - Check development environment prerequisites"
	@echo "  ready/fix - Install missing development environment prerequisites"
	@echo "  whitespace/fix  - Normalize LF, trim trailing spaces, add final newline"
	@echo ""

all:
	@$(MAKE) clean
	@$(MAKE) configure
	@$(MAKE) lint
	@$(MAKE) build
	@$(MAKE) test
	@$(MAKE) test/zeroes
	@$(MAKE) test/ones
	@$(MAKE) test/alternating01
	@$(MAKE) test/alternating10
	@$(MAKE) test/random
	@$(MAKE) cover

include Makefile.d/*.mk
