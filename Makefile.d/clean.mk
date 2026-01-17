# Makefile.d/clean.mk
# (c) 2026 Sam Caldwell. See LICENSE.txt for details
#
# Look at me using ❌ and ✅ like a youngster.  Take that ya hipsters!
# I may be old but I can adapt new ways AND enjoy music from an era when
# people actually played an instrument.

.PHONY: all build clean configure help lint ready ready/fix test
clean:
	@echo "--- Cleaning build directory ---"
	@[ -d "$(CURDIR)/build" ] && rm -rf $(CURDIR)/build && echo "    ✅ Removed build directory."
	@mkdir -p $(CURDIR)/build || {echo "❌ failed to create build directory" && exit 1}
	@echo "    ✅ Created empty build directory."
	@echo "--- Clean complete ---"
