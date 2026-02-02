# Makefile.d/test-alternating10.mk
# (c) 2026 Sam Caldwell. See LICENSE.txt for details
# Run only the alternating10 test runner (no CTest).

.PHONY: test/alternating10

test/alternating10: build
	@echo "--- Running testRunnerAlternating10 (preset: $(PRESET)) ---"
	@TEST_BINARY_DIR="$(BUILD_DIR)/$(PRESET)" \
	PATH="$(CURDIR)/bin:$$PATH" \
	"$(CURDIR)/bin/testRunnerAlternating10"
	@echo "--- ✅ testRunnerAlternating10 complete ---"
