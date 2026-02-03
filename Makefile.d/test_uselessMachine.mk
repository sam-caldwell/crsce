# Makefile.d/test_uselessMachine.mk
# (c) 2026 Sam Caldwell. See LICENSE.txt for details

.PHONY: test/uselessMachine
test/uselessMachine:
	@TEST_BINARY_DIR="$(BUILD_DIR)/$(PRESET)" \
	PATH="$(CURDIR)/bin:$$PATH" \
	"$(CURDIR)/bin/uselessTest"

# More extreme annealing schedule (per-phase overrides)
.PHONY: test/uselessMachine-anneal
test/uselessMachine-anneal:
	@TEST_BINARY_DIR="$(BUILD_DIR)/$(PRESET)" \
	PATH="$(CURDIR)/bin:$$PATH" \
	"$(CURDIR)/bin/uselessTest"
