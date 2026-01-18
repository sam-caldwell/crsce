# Makefile.d/test.mk
# (c) 2026 Sam Caldwell. See LICENSE.txt for details
#
# Default preset can be overridden from the command line
# e.g., make test PRESET=llvm-release
PRESET ?= llvm-debug
JOBS := $(shell cmake -P cmake/tools/print_num_cpus.cmake | tail -n1 | sed 's/[^0-9].*//')

.PHONY: all build clean configure help lint ready ready/fix test
test:
	@echo "--- Running tests for preset: $(PRESET) ---"
	@cd build/$(PRESET) && ctest --output-on-failure -j $(JOBS)
	@echo "--- Tests complete ---"
