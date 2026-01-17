# Makefile.d/cover.mk
# (c) 2026 Sam Caldwell. See LICENSE.txt for details
#
.PHONY: cover
cover:
	@bash scripts/coverage.sh $(BUILD_DIR) $(CURDIR)
