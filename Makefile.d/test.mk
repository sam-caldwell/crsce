.PHONY: test

test:
	@go test -failfast -v ./...
	@echo 'done: ok'
