# Makefile.d/test-zeroes.mk
# (c) 2026 Sam Caldwell. See LICENSE.txt for details
# Run only the random test runner (no CTest). Optional overrides:

.PHONY: test/zeroes

#   MIN=<bytes>  MAX=<bytes>
test/zeroes: build
	@echo "--- Running testRunnerZeroes (preset: $(PRESET)) ---"
	@TEST_BINARY_DIR="$(BUILD_DIR)/$(PRESET)" \
	PATH="$(CURDIR)/bin:$$PATH" \
	"$(CURDIR)/bin/testRunnerZeroes"
	@echo "--- ✅ testRunnerZeroes complete ---"
