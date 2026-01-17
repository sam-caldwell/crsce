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
	@echo "    🔎 Static analysis (cppcheck)..."
	@if command -v cppcheck >/dev/null; then \
		cppcheck --quiet --error-exitcode=1 \
		  --enable=warning,style,performance,portability \
		  --inline-suppr \
		  -i build -i node -i venv \
		  --suppress=missingIncludeSystem --suppress=missingInclude \
		  cmd src include test; \
	else \
		echo "    ❌ cppcheck not found. Run 'make ready/fix'."; \
		exit 1; \
	fi

.PHONY: lint-make
lint-make:
	@echo "    🔎 Linting Makefile files (checkmake)..."
	@checkmake --config=.checkmake.conf Makefile Makefile.d/*.mk
