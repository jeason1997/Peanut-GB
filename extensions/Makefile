.PHONY: all sdl2 test clean-help
all: sdl2
sdl2:
	python3 tools/build_peanut.py
test:
	sh tests/run.sh
clean-help:
	@echo "Generated files are under ../build-extensions/ (ignored by Git)."
