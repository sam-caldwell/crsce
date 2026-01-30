# Makefile
# (c) 2026 Sam Caldwell. See LICENSE.txt for details

# Define local environment directories
VENV_DIR := $(CURDIR)/venv
NODE_DIR := $(CURDIR)/node
NODE_BIN_DIR := $(NODE_DIR)/node_modules/.bin
BUILD_DIR := $(CURDIR)/build
LLVMBIN := /opt/homebrew/opt/llvm/bin

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
	@echo "  build     - Build all tools and targets"
	@echo "  deps      - Discover and build all tools/** CMake projects"
	@echo "  test      - Build and run all test programs"
	@echo "  test/random - Run only the random test runner (cmd/testRunnerRandom)"
	@echo "  test/zeroes - Run only the zeroes test runner (cmd/testRunnerZeroes)"
	@echo "  cover     - Ensure minimum test coverage threshold is maintained"
	@echo "  ready     - Check development environment prerequisites"
	@echo "  ready/fix - Install missing development environment prerequisites"
	@echo "  whitespace/fix  - Normalize LF, trim trailing spaces, add final newline"
	@echo ""
	@echo "Other targets are available in Makefile.d/."

all:
	@$(MAKE) clean
	@$(MAKE) configure
	@$(MAKE) lint
	@$(MAKE) build
	@$(MAKE) test
	@$(MAKE) cover

include Makefile.d/*.mk
