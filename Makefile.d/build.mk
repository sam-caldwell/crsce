.PHONY: build

build:
	@go build -o build/crsce cmd/main.go
	@echo 'done: ok'