# Makefile.d/lint-py.mk

.PHONY: lint-py
lint-py:
	@cmake -D LINT_TARGET=py -P cmake/pipeline/lint.cmake
