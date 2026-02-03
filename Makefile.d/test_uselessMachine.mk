# Makefile.d/test_uselessMachine.mk
# (c) 2026 Sam Caldwell. See LICENSE.txt for details

.PHONY: test/uselessMachine
test/uselessMachine:
	@TEST_BINARY_DIR="$(BUILD_DIR)/$(PRESET)" \
	PATH="$(CURDIR)/bin:$$PATH" \
	CRSCE_DE_MAX_ITERS=20000 \
	CRSCE_GOBP_ITERS=20000 \
	CRSCE_GOBP_CONF=0.85 \
	CRSCE_GOBP_DAMP=0.20 \
	CRSCE_GOBP_MULTIPHASE=1 \
	CRSCE_BACKTRACK=1 \
	"$(CURDIR)/bin/uselessTest"
