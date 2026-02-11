# Makefile.d/test-alternating01.mk
# (c) 2026 Sam Caldwell. See LICENSE.txt for details
# Run only the alternating01 test runner (no CTest).

.PHONY: test/alternating01

test/alternating01:
	@echo "--- Running testRunnerAlternating01 (preset: $(PRESET)) ---"
	@TEST_BINARY_DIR="$(BUILD_DIR)/$(PRESET)" \
	PATH="$(CURDIR)/bin:$$PATH" \
	"$(CURDIR)/bin/testRunnerAlternating01"
	@echo "--- ✅ testRunnerAlternating01 complete ---"
