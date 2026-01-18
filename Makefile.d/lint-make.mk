# Makefile.d/lint-make.mk

.PHONY: lint-make
lint-make:
	@cmake -D LINT_TARGET=make -P cmake/pipeline/lint.cmake
