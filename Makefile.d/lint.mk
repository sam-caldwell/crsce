# Makefile.d/lint.mk

# Use the local venv and node_modules
VENV_DIR := $(CURDIR)/venv
NODE_BIN_DIR := $(CURDIR)/node/node_modules/.bin
export PATH := $(VENV_DIR)/bin:$(NODE_BIN_DIR):$(PATH)

.PHONY: all build clean configure help lint ready ready/fix test
lint:
	@cmake -D LINT_TARGET=all -P cmake/pipeline/lint.cmake

.PHONY: lint-md
lint-md:
	@cmake -D LINT_TARGET=md -P cmake/pipeline/lint.cmake

.PHONY: lint-sh
lint-sh:
	@cmake -D LINT_TARGET=sh -P cmake/pipeline/lint.cmake

.PHONY: lint-py
lint-py:
	@cmake -D LINT_TARGET=py -P cmake/pipeline/lint.cmake

.PHONY: lint-cpp
lint-cpp:
	@cmake -D LINT_TARGET=cpp -P cmake/pipeline/lint.cmake

.PHONY: lint-make
lint-make:
	@cmake -D LINT_TARGET=make -P cmake/pipeline/lint.cmake
