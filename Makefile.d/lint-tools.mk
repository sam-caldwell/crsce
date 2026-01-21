# Makefile.d/lint-tools.mk
# (c) 2026 Sam Caldwell. See LICENSE.txt for details

.PHONY: lint/tools
lint/tools:
	@cmake -D LINT_TARGET=cpp_tools -P cmake/pipeline/lint.cmake
