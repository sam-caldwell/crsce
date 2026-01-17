# Makefile.d/test.mk
# (c) 2026 Sam Caldwell. See LICENSE.txt for details
#
# Default preset can be overridden from the command line
# e.g., make test PRESET=arm64-release
PRESET ?= arm64-debug

.PHONY: all build clean configure help lint ready ready/fix test
test:
	@echo "--- Running tests for preset: $(PRESET) ---"
	@cd build/$(PRESET) && ctest --output-on-failure
	@echo "--- Tests complete ---"
