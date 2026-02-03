# Makefile.d/test_uselessMachine.mk
# (c) 2026 Sam Caldwell. See LICENSE.txt for details

.PHONY: test/uselessMachine
test/uselessMachine:
	@TEST_BINARY_DIR="$(BUILD_DIR)/$(PRESET)" \
	PATH="$(CURDIR)/bin:$$PATH" \
	CRSCE_DE_MAX_ITERS=60000 \
	CRSCE_GOBP_ITERS=100000 \
	CRSCE_GOBP_CONF=0.72 \
	CRSCE_GOBP_DAMP=0.08 \
	CRSCE_GOBP_MULTIPHASE=1 \
	CRSCE_BACKTRACK=1 \
	CRSCE_BT_DE_ITERS=300 \
	"$(CURDIR)/bin/uselessTest"
