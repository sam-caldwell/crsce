# Makefile.d/test-random.mk
# (c) 2026 Sam Caldwell. See LICENSE.txt for details
# Run only the random test runner (no CTest). Optional overrides:

.PHONY: test/random

#   MIN=<bytes>  MAX=<bytes>
# Provide reasonable defaults to keep runs predictable and fast.
# Increase defaults to exercise GOBP more often
MIN ?= 262144
MAX ?= 262144
test/random: build
	@echo "--- Running testRunnerRandom (preset: $(PRESET)) ---"
	@TEST_BINARY_DIR="$(BUILD_DIR)/$(PRESET)" \
	PATH="$(CURDIR)/bin:$$PATH" \
	"$(CURDIR)/bin/testRunnerRandom" --min-bytes $(MIN) --max-bytes $(MAX)
	@echo "--- ✅ testRunnerRandom complete ---"
