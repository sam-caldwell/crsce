# Makefile.d/lint-cpp.mk

.PHONY: lint-cpp
lint-cpp:
	@cmake -D LINT_TARGET=cpp -P cmake/pipeline/lint.cmake
