# Makefile.d/lint-sh.mk

.PHONY: lint-sh
lint-sh:
	@cmake -D LINT_TARGET=sh -P cmake/pipeline/lint.cmake
