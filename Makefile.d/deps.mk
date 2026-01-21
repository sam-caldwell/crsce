# Makefile.d/deps.mk
# (c) 2026 Sam Caldwell. See LICENSE.txt for details

.PHONY: deps
deps:
	@echo "--- Building tool dependencies under tools/** ---"
	@cmake -D SOURCE_DIR=$(CURDIR) -D BUILD_DIR=$(CURDIR)/build -P cmake/pipeline/deps.cmake || { echo "❌ deps failed"; exit 1; }
	@echo "--- ✅ Tool dependencies built ---"
