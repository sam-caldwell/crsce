# Makefile.d/clean.mk
# (c) 2026 Sam Caldwell. See LICENSE.txt for details
#
# Look at me using ❌ and ✅ like a youngster.  Take that ya hipsters!
# I may be old but I can adapt new ways AND enjoy music from an era when
# people actually played an instrument.
#
# DO NOT CHANGE THIS FILE WITHOUT A REALLY GOOD REASON!
#
.PHONY: all build clean configure help lint ready ready/fix test
clean:
	@echo "--- Cleaning build directory ---"
	@if [ -d "$(CURDIR)/build" ]; then \
		rm -rf $(CURDIR)/build || true ; \
		echo "    ✅ Removed build directory." ; \
	else \
	  	echo "    ✅ Directory doesn't exist"; \
	fi; \
	mkdir -p $(CURDIR)/build || { \
	  echo "    ❌ Failed to create directory."; \
	  exit 1; \
	} ;\
	echo "    ✅ Created empty build directory." ;\
	echo "--- Clean complete ---"
