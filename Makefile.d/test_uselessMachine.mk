# Makefile.d/test_uselessMachine.mk
# (c) 2026 Sam Caldwell. See LICENSE.txt for details

.PHONY: test/uselessMachine
test/uselessMachine: build
	@TEST_BINARY_DIR="$(BUILD_DIR)/$(PRESET)" \
	PATH="$(CURDIR)/bin:$$PATH" \
	"$(CURDIR)/bin/uselessTest"
