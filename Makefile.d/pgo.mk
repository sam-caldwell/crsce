# Makefile.d/pgo.mk
# (c) 2026 Sam Caldwell. See LICENSE.txt for details
#
# Profile-Guided Optimization workflow:
#   make pgo        -- full pipeline: instrument -> profile -> optimize
#   make pgo/gen    -- build with instrumentation
#   make pgo/run    -- run representative workload to generate profile data
#   make pgo/merge  -- merge raw profiles into default.profdata
#   make pgo/use    -- rebuild with PGO profile data

PROFDATA_DIR := $(BUILD_DIR)/profdata
LLVM_PROFDATA := /opt/homebrew/opt/llvm/bin/llvm-profdata

.PHONY: pgo pgo/gen pgo/run pgo/merge pgo/use

# Full PGO pipeline
pgo: pgo/gen pgo/run pgo/merge pgo/use
	@echo "--- PGO pipeline complete ---"
	@echo "  PGO-optimized binaries: $(BUILD_DIR)/llvm-pgo-use/"

# Step 1: Build with instrumentation
pgo/gen:
	@echo "--- [pgo/gen] Building with PGO instrumentation ---"
	@mkdir -p $(PROFDATA_DIR)
	@cmake --preset=llvm-pgo-gen -S .
	@cmake --build $(BUILD_DIR)/llvm-pgo-gen --parallel $(JOBS)
	@echo "--- [pgo/gen] Instrumented build complete ---"

# Step 2: Run representative workload to generate raw profile data
pgo/run:
	@echo "--- [pgo/run] Running profiling workload ---"
	@rm -f $(PROFDATA_DIR)/*.profraw
	@if [ -x "$(BUILD_DIR)/llvm-pgo-gen/uselessTest" ]; then \
	  LLVM_PROFILE_FILE="$(PROFDATA_DIR)/useless-%p.profraw" \
	  DISABLE_COMPRESS_DI=1 \
	  "$(BUILD_DIR)/llvm-pgo-gen/uselessTest"; \
	else \
	  echo "ERROR: instrumented uselessTest not found"; exit 1; \
	fi
	@echo "--- [pgo/run] Profile data collected ---"

# Step 3: Merge raw profiles
pgo/merge:
	@echo "--- [pgo/merge] Merging profile data ---"
	@$(LLVM_PROFDATA) merge -output=$(PROFDATA_DIR)/default.profdata $(PROFDATA_DIR)/*.profraw
	@echo "--- [pgo/merge] Merged into $(PROFDATA_DIR)/default.profdata ---"

# Step 4: Rebuild with PGO optimization
pgo/use:
	@echo "--- [pgo/use] Building with PGO optimization ---"
	@cmake --preset=llvm-pgo-use -S .
	@cmake --build $(BUILD_DIR)/llvm-pgo-use --parallel $(JOBS)
	@echo "--- [pgo/use] PGO-optimized build complete ---"
