# Makefile.d/lint.mk

# Use the local venv and node_modules
VENV_DIR := $(CURDIR)/venv
NODE_BIN_DIR := $(CURDIR)/node/node_modules/.bin
export PATH := $(VENV_DIR)/bin:$(NODE_BIN_DIR):$(PATH)

.PHONY: all build clean configure help lint ready ready/fix test
lint:
	@cmake -D LINT_TARGET=all -P cmake/pipeline/lint.cmake
