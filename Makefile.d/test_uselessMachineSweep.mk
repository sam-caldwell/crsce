# Makefile.d/test_uselessMachineSweep.mk

.PHONY: test/uselessMachine-sweep
test/uselessMachine-sweep:
	@N=$${N:-100}; \
	for s in $$(seq 1 $$N); do \
	  echo "[sweep] seed=$$s"; \
	  CRSCE_SOLVER_SEED=$$s $(MAKE) --no-print-directory test/uselessMachine || true; \
	done

.PHONY: test/uselessMachine-summary
test/uselessMachine-summary:
	@python3 tools/useless_log_summary.py build/uselessTest/completion_stats.log
