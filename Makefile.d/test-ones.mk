# Makefile.d/test-ones.mk
# (c) 2026 Sam Caldwell. See LICENSE.txt for details
# Run only the ones test runner (no CTest).

.PHONY: test/ones

test/ones: build
	@echo "--- Running testRunnerOnes (preset: $(PRESET)) ---"
	@TEST_BINARY_DIR="$(BUILD_DIR)/$(PRESET)" \
	PATH="$(CURDIR)/bin:$$PATH" \
	"$(CURDIR)/bin/testRunnerOnes"
	@echo "--- ✅ testRunnerOnes complete ---"
