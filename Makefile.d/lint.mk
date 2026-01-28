# Makefile.d/lint.mk

# Use the local venv and node_modules
VENV_DIR := $(CURDIR)/venv
NODE_BIN_DIR := $(CURDIR)/node/node_modules/.bin
LINT_CHANGED_ONLY=ON
export PATH := $(VENV_DIR)/bin:$(NODE_BIN_DIR):$(PATH)

.PHONY: all build clean configure help lint ready ready/fix test
lint:
	@cmake -D LINT_TARGET=all -D LINT_CHANGED_ONLY=$(LINT_CHANGED_ONLY) -P cmake/pipeline/lint.cmake

.PHONY: lint/headers
lint/headers:
	@cmake -D LINT_TARGET=cpp-headers -D LINT_CHANGED_ONLY=$(LINT_CHANGED_ONLY) -P cmake/pipeline/lint.cmake
