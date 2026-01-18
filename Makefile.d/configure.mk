# Makefile.d/configure.mk
# (c) 2026 Sam Caldwell. See LICENSE.txt for details
#
# Default preset can be overridden from the command line
# e.g., make configure PRESET=llvm-release
PRESET ?= llvm-debug

.PHONY: all build clean configure help lint ready ready/fix test
configure:
	@echo "--- Configuring project with preset: $(PRESET) ---"
	@cmake --preset=$(PRESET) -S .
	@echo "--- Building tool dependencies (make deps) ---"
	@$(MAKE) deps
