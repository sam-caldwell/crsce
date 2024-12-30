.PHONY: clean

clean:
	@rm -rf build || true
	@mkdir -p build
	@echo 'done: ok'