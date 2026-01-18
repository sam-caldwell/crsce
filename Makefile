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

.PHONY: all build clean configure help lint ready ready/fix test
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
	@echo "  cover     - Ensure minimum test coverage threshold is maintained"
	@echo "  ready     - Check development environment prerequisites"
	@echo "  ready/fix - Install missing development environment prerequisites"
	@echo ""
	@echo "Other targets are available in Makefile.d/."

all: clean configure lint build test cover

include Makefile.d/*.mk
