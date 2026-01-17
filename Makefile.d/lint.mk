# Makefile.d/lint.mk

# Use the local venv and node_modules
VENV_DIR := $(CURDIR)/venv
NODE_BIN_DIR := $(CURDIR)/node/node_modules/.bin
export PATH := $(VENV_DIR)/bin:$(NODE_BIN_DIR):$(PATH)

.PHONY: all build clean configure help lint ready ready/fix test
lint:
	@echo "--- Running Linters ---"
	@$(MAKE) -s lint-make
	@$(MAKE) -s lint-md
	@$(MAKE) -s lint-sh
	@$(MAKE) -s lint-py
	@$(MAKE) -s lint-cpp
	@echo "--- ✅ Linters check complete ---"

.PHONY: lint-md
lint-md:
	@echo "    🔎 Linting Markdown files..."
	@npx markdownlint '**/*.md'

.PHONY: lint-sh
lint-sh:
	@echo "    🔎 Linting Shell scripts..."
	@find . -name "*.sh" | xargs shellcheck

.PHONY: lint-py
lint-py:
	@echo "    🔎 Linting Python files..."
	@flake8 --exclude=venv/,./node

.PHONY: lint-cpp
lint-cpp:
	@if ! command -v clang-tidy >/dev/null; then \
		echo "    ❌ clang-tidy is required. Run 'make ready/fix' to install llvm."; \
		exit 1; \
	fi
	@echo "    🔎 C++ analysis (clang-tidy, required)..."
	@echo "       Running build-integrated clang-tidy during 'build' (via presets)."
	@clang-tidy --version >/dev/null

.PHONY: lint-make
lint-make:
	@echo "    🔎 Linting Makefile files (checkmake)..."
	@checkmake --config=.checkmake.conf Makefile Makefile.d/*.mk
