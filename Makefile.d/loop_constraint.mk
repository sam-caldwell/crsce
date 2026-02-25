# Makefile.d/loop_constraint.mk
# (c) 2026 Sam Caldwell. See LICENSE.txt for details

.PHONY: loop/constraint
loop/constraint:
	@echo "--- Starting constraint watchdog loop ---"
	@mkdir -p build/uselessTest
	@nohup python3 tools/constraint_watchdog_loop.py --hours $${HOURS:-24} --watchdog $${WATCHDOG_SECS:-3600} \
	  > build/uselessTest/loop_watchdog.out 2>&1 & echo $$! > build/uselessTest/loop_watchdog.pid; \
	  echo "pid=$$(cat build/uselessTest/loop_watchdog.pid)"; \
	  echo "logs: build/uselessTest/loop_watchdog.out"; \
	  echo "analysis: build/uselessTest/loop_analysis.log"

