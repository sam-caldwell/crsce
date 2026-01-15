include Makefile.d/*.mk

# Define local environment directories
VENV_DIR := $(CURDIR)/venv
NODE_DIR := $(CURDIR)/node
NODE_BIN_DIR := $(NODE_DIR)/node_modules/.bin
BUILD_DIR := $(CURDIR)/build

# Add local binaries to the PATH for make commands
export PATH := $(VENV_DIR)/bin:$(NODE_BIN_DIR):$(PATH)


# Add local binaries to the PATH for make commands
export PATH := $(VENV_DIR)/bin:$(NODE_BIN_DIR):$(PATH)

.PHONY: help
help:
	@echo "Available targets:"
	@echo "  all       - Run clean configure lint build test"
	@echo "  clean     - Delete and recreate the build/ directory"
	@echo "  configure - Configure the build pipeline"
	@echo "  lint      - Run clang-tidy and other linters"
	@echo "  build     - Build all tools and targets"
	@echo "  test      - Build and run all test programs"
	@echo "  cover     - Ensure minimum test coverage threshold is maintained"
	@echo "  ready     - Check development environment prerequisites"
	@echo "  ready/fix - Install missing development environment prerequisites"
	@echo ""
	@echo "Other targets are available in Makefile.d/."

.PHONY: all
all: clean configure lint build test

