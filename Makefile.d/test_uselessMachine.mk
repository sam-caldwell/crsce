# Makefile.d/test_uselessMachine.mk
# (c) 2026 Sam Caldwell. See LICENSE.txt for details

.PHONY: test/uselessMachine
test/uselessMachine:
	@# Prefer an optimized release binary when present
	@if [ -x "$(BUILD_DIR)/llvm-release/uselessTest" ]; then \
	  USL_EXE="$(BUILD_DIR)/llvm-release/uselessTest"; \
	elif [ -x "$(BUILD_DIR)/cmake-build-release/uselessTest" ]; then \
	  USL_EXE="$(BUILD_DIR)/cmake-build-release/uselessTest"; \
	elif [ -x "$(BUILD_DIR)/arm64-release/uselessTest" ]; then \
	  USL_EXE="$(BUILD_DIR)/arm64-release/uselessTest"; \
	else \
	  USL_EXE="$(CURDIR)/bin/uselessTest"; \
	fi; \
	TEST_BINARY_DIR="$(BUILD_DIR)/$(PRESET)" \
	PATH="$(CURDIR)/bin:$$PATH" \
	"$$USL_EXE"
