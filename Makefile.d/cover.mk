# Makefile.d/cover.mk
# (c) 2026 Sam Caldwell. See LICENSE.txt for details
#
.PHONY: cover
cover:
	@cmake -D BUILD_DIR=$(BUILD_DIR) -D SOURCE_DIR=$(CURDIR) -P cmake/pipeline/cover.cmake
