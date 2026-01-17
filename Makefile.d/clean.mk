# Makefile.d/clean.mk
# (c) 2026 Sam Caldwell. See LICENSE.txt for details

.PHONY: all build clean configure help lint ready ready/fix test
clean:
	@echo "--- Cleaning build directory ---"
	@[ -d "$(CURDIR)/build" ] && rm -rf $(CURDIR)/build && echo "    ✅ Removed build directory."
	@mkdir -p $(CURDIR)/build
	@echo "    ✅ Created empty build directory."
	@echo "--- Clean complete ---"
