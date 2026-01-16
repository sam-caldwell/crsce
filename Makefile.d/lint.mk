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
	@$(MAKE) -s lint-cmake
	@$(MAKE) -s lint-cpp
	@echo "--- Linters check complete ---"

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

.PHONY: lint-cmake
lint-cmake:
	@echo "    🔎 Linting CMake files..."
	@find . -name "CMakeLists.txt" -o -name "*.cmake" | xargs cmakelang --format-on-edit .

.PHONY: lint-cpp
lint-cpp:
	@echo "    🔎 Linting C++ files (clang-format)..."
	@find cmd src test -name "*.cpp" -o -name "*.h" | xargs clang-format -i
	@echo "    🔎 Linting C++ files (clang-tidy)..."
	@# clang-tidy is best run during the build process.
	@# To enable, configure with -DCMAKE_CXX_CLANG_TIDY=clang-tidy
	@echo "    ℹ️  Skipping clang-tidy. Enable with -DCMAKE_CXX_CLANG_TIDY."

.PHONY: lint-make
lint-make:
	@echo "    🔎 Linting Makefile files (checkmake)..."
	@checkmake --config=.checkmake.conf Makefile Makefile.d/*.mk