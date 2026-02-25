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

.PHONY: loop/uselessMachine
loop/uselessMachine:
	@echo "--- Starting loop runner (constraint) ---"
	@mkdir -p build/uselessTest
	@nohup python3 tools/useless_constraint_loop.py --hours $${HOURS:-8} --watchdog $${WATCHDOG_SECS:-1800} \
	  > build/uselessTest/loop_runner.out 2>&1 & echo $$! > build/uselessTest/loop_runner.pid; \
	  echo "pid=$$(cat build/uselessTest/loop_runner.pid)"; \
	  echo "logs: build/uselessTest/loop_runner.out"
