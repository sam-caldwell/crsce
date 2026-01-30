# Makefile.d/test-zeroes.mk
# (c) 2026 Sam Caldwell. See LICENSE.txt for details
# Run only the random test runner (no CTest). Optional overrides:

.PHONY: test/zeroes

#   MIN=<bytes>  MAX=<bytes>
test/zeroes: build
	@echo "--- Running testRunnerZeroes (preset: $(PRESET)) ---"
	@TEST_BINARY_DIR="$(BUILD_DIR)/$(PRESET)" \
	PATH="$(BUILD_DIR)/$(PRESET):$$PATH" \
	CRSCE_TESTRUNNER_MIN_BYTES=$(MIN) CRSCE_TESTRUNNER_MAX_BYTES=$(MAX) \
	"$(BUILD_DIR)/$(PRESET)/testRunnerZeroes"
	@echo "--- ✅ testRunnerZeroes complete ---"
