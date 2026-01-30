# Makefile.d/test-random.mk
# (c) 2026 Sam Caldwell. See LICENSE.txt for details
# Run only the random test runner (no CTest). Optional overrides:

.PHONY: test/random

#   MIN=<bytes>  MAX=<bytes>
test/random: build
	@echo "--- Running testRunnerRandom (preset: $(PRESET)) ---"
	@TEST_BINARY_DIR="$(BUILD_DIR)/$(PRESET)" \
	PATH="$(BUILD_DIR)/$(PRESET):$$PATH" \
	CRSCE_TESTRUNNER_MIN_BYTES=$(MIN) CRSCE_TESTRUNNER_MAX_BYTES=$(MAX) \
	"$(BUILD_DIR)/$(PRESET)/testRunnerRandom"
	@echo "--- ✅ testRunnerRandom complete ---"
