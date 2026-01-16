# Makefile.d/ready.mk

.PHONY: ready
ready: check-brew check-python check-pip check-cmake check-make check-ninja check-clang check-linters
	@echo "\n✅ Prerequisite check complete."

.PHONY: all build clean configure help lint ready ready/fix test
ready/fix:
	@echo "🛠️  Setting up development environment..."
	@$(MAKE) -s fix-brew
	@$(MAKE) -s fix-system-deps
	@$(MAKE) -s fix-python-deps
	@$(MAKE) -s fix-node-deps
	@echo "\n✅ Environment setup complete. Run 'make ready' to verify."

# --- FIX TARGETS ---

.PHONY: fix-brew
fix-brew:
	@if ! command -v brew >/dev/null; then \
		echo "--- Installing Homebrew ---"; \
		/bin/bash -c "$$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"; \
	fi

.PHONY: fix-system-deps
fix-system-deps:
	@echo "--- Installing system dependencies via Homebrew ---"
	@brew install cmake ninja llvm shellcheck || true

.PHONY: fix-python-deps
fix-python-deps:
	@echo "--- Setting up Python virtual environment and dependencies venv: '$(VENV_DIR)' ---"
	@if [ ! -d "$(VENV_DIR)" ]; then \
  		echo "create virtual environment"; \
		python3 -m venv $(VENV_DIR); \
	else \
  		echo "virtual environment exists"; \
	fi
	@pip install --upgrade pip
	@$(VENV_DIR)/bin/pip3 install -q flake8 cmakelang

.PHONY: fix-node-deps
fix-node-deps:
	@echo "--- Setting up local npm packages ---"
	@mkdir -p $(NODE_DIR)
	@if [ ! -f "$(NODE_DIR)/package.json" ]; then \
		echo '{"name": "crsce-dev-env", "version": "1.0.0", "description": "Dev environment for crsce project"}' > $(NODE_DIR)/package.json; \
	fi
	@cd $(NODE_DIR) && npm install --silent markdownlint-cli

# --- CHECK TARGETS (Updated for local environments) ---

.PHONY: check-brew
check-brew:
	@if command -v brew >/dev/null; then \
			echo "✅ Homebrew is installed."; \
	else \
			echo "❌ Homebrew is not installed. Run 'make ready/fix'."; \
		exit 1; \
	fi

.PHONY: check-python
check-python:
	@if command -v python3 >/dev/null; then \
			echo "✅ python3 is installed."; \
	else \
			echo "❌ python3 is not installed. Run 'make ready/fix'."; \
		exit 1; \
	fi

.PHONY: check-pip
check-pip:
	@if command -v pip3 >/dev/null; then \
			echo "✅ pip3 is installed."; \
	else \
			echo "❌ pip3 is not installed. Run 'make ready/fix'."; \
		exit 1; \
	fi

.PHONY: check-cmake
check-cmake:
	@if command -v cmake >/dev/null; then \
			echo "✅ cmake is installed."; \
	else \
			echo "❌ cmake is not installed. Run 'make ready/fix'."; \
		exit 1; \
	fi

.PHONY: check-make
check-make:
	@if command -v make >/dev/null; then \
			echo "✅ make is installed."; \
	else \
			echo "❌ make is not installed."; \
		exit 1; \
	fi

.PHONY: check-ninja
check-ninja:
	@if command -v ninja >/dev/null; then \
			echo "✅ ninja is installed."; \
	else \
			echo "❌ ninja is not installed. Run 'make ready/fix'."; \
		exit 1; \
	fi

.PHONY: check-clang
check-clang:
	@mkdir -p $(BUILD_DIR) # Ensure build directory exists for temporary files
	@if command -v clang++ >/dev/null; then \
			echo "✅ clang++ is installed."; \
			echo "static_assert(true);" > $(BUILD_DIR)/test-cpp26.cpp; \
			echo "int main() { return 0; }" >> $(BUILD_DIR)/test-cpp26.cpp; \
			if clang++ -std=c++26 -o $(BUILD_DIR)/test-cpp26.out $(BUILD_DIR)/test-cpp26.cpp >/dev/null 2>&1; then \
					echo "    ✅ clang++ appears to support C++26."; \
					rm $(BUILD_DIR)/test-cpp26.cpp $(BUILD_DIR)/test-cpp26.out; \
			else \
					echo "    ❌ clang++ does not appear to support C++26. Try upgrading llvm (e.g., 'brew upgrade llvm')."; \
					rm $(BUILD_DIR)/test-cpp26.cpp; \
				exit 1; \
			fi; \
	else \
			echo "❌ clang++ is not installed. Run 'make ready/fix'."; \
		exit 1; \
	fi

.PHONY: all clean configure lint build test ready ready/fix check-linters
check-linters:
	@echo "--- Checking Linters ---"
	@if command -v clang-tidy >/dev/null; then \
			echo "    ✅ clang-tidy is installed."; \
	else \
			echo "    ❌ clang-tidy is not installed. Run 'make ready/fix'."; \
		exit 1; \
	fi
	@if command -v clang-format >/dev/null; then \
			echo "    ✅ clang-format is installed."; \
	else \
			echo "    ❌ clang-format is not installed. Run 'make ready/fix'."; \
		exit 1; \
	fi
	@if command -v markdownlint >/dev/null; then \
			echo "    ✅ markdownlint is installed (locally)."; \
	else \
			echo "    ❌ markdownlint is not installed. Run 'make ready/fix'."; \
		exit 1; \
	fi
	@if command -v shellcheck >/dev/null; then \
			echo "    ✅ shellcheck is installed."; \
	else \
			echo "    ❌ shellcheck is not installed. Run 'make ready/fix'."; \
		exit 1; \
	fi
	@if command -v flake8 >/dev/null; then \
			echo "    ✅ flake8 is installed (locally)."; \
	else \
			echo "    ❌ flake8 is not installed. Run 'make ready/fix'."; \
		exit 1; \
	fi
	@if command -v cmake-lint >/dev/null; then \
			echo "    ✅ cmakelang is installed (locally)."; \
	else \
			echo "    ❌ cmakelang is not installed. Run 'make ready/fix'."; \
		exit 1; \
	fi
	@echo "--- Linters check complete ---"