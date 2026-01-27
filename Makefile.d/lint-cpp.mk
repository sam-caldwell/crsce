# Makefile.d/lint-cpp.mk

.PHONY: lint-cpp
lint-cpp:
	@cmake -D LINT_TARGET=cpp -D LINT_CHANGED_ONLY=$(LINT_CHANGED_ONLY) -P cmake/pipeline/lint.cmake

.PHONY: lint-cpp-otpf
lint-cpp-otpf:
	@cmake -D LINT_TARGET=cpp-otpf -D LINT_CHANGED_ONLY=$(LINT_CHANGED_ONLY) -P cmake/pipeline/lint.cmake

.PHONY: lint-cpp-odpcpp
lint-cpp-odpcpp:
	@cmake -D LINT_TARGET=cpp-odpcpp -D LINT_CHANGED_ONLY=$(LINT_CHANGED_ONLY) -P cmake/pipeline/lint.cmake

.PHONY: lint-cpp-odph
lint-cpp-odph:
	@cmake -D LINT_TARGET=cpp-odph -D LINT_CHANGED_ONLY=$(LINT_CHANGED_ONLY) -P cmake/pipeline/lint.cmake
