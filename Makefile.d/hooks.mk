.PHONY: hooks
hooks:
	@cmake -D SOURCE_DIR=$(CURDIR) -P cmake/tools/hooks.cmake
