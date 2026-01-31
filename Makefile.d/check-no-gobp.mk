# Makefile.d/check-no-gobp.mk
# (c) 2026 Sam Caldwell. See LICENSE.txt for details
# CI check to ensure deterministic-pattern runs (zeroes/ones)
# do not emit any GOBP-related solver markers in decompressor logs.

.PHONY: ci/no-gobp-deterministic

# Pattern list is intentionally specific to GOBP concepts to avoid false positives.
GOBP_MARKERS := GOBP|Gobp|\\bBelief\\b|\\bbelief\\b|\\bBeliefs\\b|\\bbeliefs\\b|Bayes|\\bodds\\b|\\bposterior\\b|\\bprior\\b|\\blikelihood\\b


ci/no-gobp-deterministic: build
	@echo "--- CI: checking deterministic logs for GOBP markers ---"
	@set -euo pipefail; \
	# Ensure logs exist; run runners sequentially only if needed. \ \
	for DIR in "$(BUILD_DIR)/testRunnerZeroes" "$(BUILD_DIR)/testRunnerOnes"; do \ \
	  if ! ls "$$DIR"/*.decompress.stderr.txt >/dev/null 2>&1; then \ \
	    if echo "$$DIR" | grep -q testRunnerZeroes; then \ \
	      $(MAKE) -s test/zeroes; \ \
	    else \ \
	      $(MAKE) -s test/ones; \ \
	    fi; \ \
	  fi; \ \
	done; \
	FAIL=0; \
	for DIR in "$(BUILD_DIR)/testRunnerZeroes" "$(BUILD_DIR)/testRunnerOnes"; do \
	  if [ ! -d "$$DIR" ]; then \
	    echo "ERROR: expected directory '$$DIR' not found. Did the tests run?"; \
	    FAIL=1; \
	    continue; \
	  fi; \
	  FILES=$$(find "$$DIR" -maxdepth 1 -type f -name "*.decompress.stderr.txt"); \
	  if [ -z "$$FILES" ]; then \
	    echo "ERROR: no decompressor stderr logs found in '$$DIR'"; \
	    FAIL=1; \
	    continue; \
	  fi; \
	  MATCHES=$$(grep -E -n -i -H -e "$(GOBP_MARKERS)" $$FILES || true); \
	  if [ -n "$$MATCHES" ]; then \
	    echo "ERROR: GOBP-related markers detected in deterministic decompressor logs:"; \
	    echo "$$MATCHES"; \
	    FAIL=1; \
	  else \
	    echo "OK: no GOBP markers found in '$$DIR'"; \
	  fi; \
	done; \
	if [ "$$FAIL" -ne 0 ]; then \
	  echo "--- ❌ CI check failed: deterministic logs contain GOBP markers ---"; \
	  exit 1; \
	fi; \
	echo "--- ✅ CI check passed: deterministic logs are GOBP-free ---"
