# Makefile.d/lint-md.mk

.PHONY: lint-md
lint-md:
	@cmake -D LINT_TARGET=md -P cmake/pipeline/lint.cmake
