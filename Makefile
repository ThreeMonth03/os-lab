SHELL := /bin/bash
.DEFAULT_GOAL := help

PROJECT_ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
BUILD_DIR ?= $(PROJECT_ROOT)/build
GENERATOR ?= Ninja
GENERATOR_BIN ?= ninja
CMAKE ?= cmake
CLANG_FORMAT ?= clang-format
DOCKER_COMPOSE ?= $(shell if docker compose version >/dev/null 2>&1; then \
	printf 'docker compose'; \
elif command -v docker-compose >/dev/null 2>&1; then \
	printf 'docker-compose'; \
fi)
DOCKER_RUN_ENV := LOCAL_UID=$(shell id -u) LOCAL_GID=$(shell id -g)

TOOLCHAIN_FILE := $(PROJECT_ROOT)/cmake/toolchains/x86_64-none-clang.cmake
KERNEL_ELF := $(BUILD_DIR)/artifacts/kernel.elf
ISO_IMAGE := $(BUILD_DIR)/os-lab.iso

.PHONY: help deps demo gui test format shell clean ci
.PHONY: _check-native-tools _check-clang-format _check-docker-compose _configure _kernel _iso _run _run-gui _smoke _format _format-check _docker-image _docker-iso

help:
	@printf '%s\n' \
		'Common targets:' \
		'  make deps      Install host QEMU on Debian/LMDE' \
		'  make demo      Build in Docker, then boot headless' \
		'  make gui       Build in Docker, then boot with a QEMU window' \
		'  make test      Build in Docker, then run the QEMU smoke test' \
		'  make format    Apply clang-format inside Docker' \
		'  make shell     Open the development container' \
		'  make clean     Remove generated files'

deps:
	./scripts/install-deps-debian.sh host

demo: _docker-iso _run

gui: _docker-iso _run-gui

test: _docker-iso _smoke

format: _docker-image
	$(DOCKER_RUN_ENV) $(DOCKER_COMPOSE) run --rm builder make _format

shell: _docker-image
	$(DOCKER_RUN_ENV) $(DOCKER_COMPOSE) run --rm builder bash

clean:
	rm -rf $(BUILD_DIR) .cache

ci: _format-check _iso _smoke

_check-native-tools:
	@for tool in clang clang++ g++ ld.lld $(CMAKE) xorriso curl tar sha256sum $(GENERATOR_BIN); do \
		if ! command -v "$$tool" >/dev/null 2>&1; then \
			printf 'Missing native tool: %s\n' "$$tool" >&2; \
			printf 'Use `make demo` or install native deps with scripts/install-deps-debian.sh native.\n' >&2; \
			exit 1; \
		fi; \
	done

_check-clang-format:
	@if ! command -v $(CLANG_FORMAT) >/dev/null 2>&1; then \
		printf 'Missing tool: %s\n' '$(CLANG_FORMAT)' >&2; \
		exit 1; \
	fi

_check-docker-compose:
	@if [[ -z "$(DOCKER_COMPOSE)" ]]; then \
		printf 'Missing Docker Compose. Install the Docker Compose plugin or legacy docker-compose.\n' >&2; \
		exit 1; \
	fi

_configure: _check-native-tools
	$(CMAKE) -S $(PROJECT_ROOT) -B $(BUILD_DIR) -G $(GENERATOR) \
		-DCMAKE_BUILD_TYPE=Debug \
		-DCMAKE_TOOLCHAIN_FILE=$(TOOLCHAIN_FILE) \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=ON

_kernel: _configure
	$(CMAKE) --build $(BUILD_DIR) --target kernel

_iso: _kernel
	./scripts/create_iso.sh $(KERNEL_ELF) $(ISO_IMAGE)

_run:
	@if [[ ! -f "$(ISO_IMAGE)" ]]; then $(MAKE) _iso; fi
	./scripts/run_qemu.sh $(ISO_IMAGE)

_run-gui:
	@if [[ ! -f "$(ISO_IMAGE)" ]]; then $(MAKE) _iso; fi
	QEMU_HEADLESS=0 ./scripts/run_qemu.sh $(ISO_IMAGE)

_smoke:
	@if [[ ! -f "$(ISO_IMAGE)" ]]; then $(MAKE) _iso; fi
	@set -euo pipefail; \
	if ! command -v qemu-system-x86_64 >/dev/null 2>&1; then \
		printf 'Smoke test requires native Linux qemu-system-x86_64 in PATH\n' >&2; \
		exit 1; \
	fi; \
	log_file=$$(mktemp); \
	debug_file=$$(mktemp); \
	trap 'rm -f "$$log_file" "$$debug_file"' EXIT; \
	set +e; \
	timeout 15s qemu-system-x86_64 \
		-M q35 \
		-m 256M \
		-boot d \
		-cdrom "$(ISO_IMAGE)" \
		-serial stdio \
		-debugcon "file:$$debug_file" \
		-display none \
		-no-reboot \
		-no-shutdown | tee "$$log_file"; \
	status=$$?; \
	set -e; \
	if [[ $$status -ne 0 && $$status -ne 124 ]]; then exit "$$status"; fi; \
	cat "$$debug_file" >> "$$log_file"; \
	grep -q "os-lab: reached _start" "$$log_file"; \
	grep -q "os-lab: entering kernel_main" "$$log_file"; \
	printf 'Smoke test passed\n'

_format: _check-clang-format
	@mapfile -d '' sources < <(find kernel/include/kernel kernel/src -type f \
		\( -name '*.hpp' -o -name '*.cpp' \) -print0 | sort -z); \
	if [[ $${#sources[@]} -gt 0 ]]; then \
		$(CLANG_FORMAT) -i "$${sources[@]}"; \
	fi

_format-check: _check-clang-format
	@mapfile -d '' sources < <(find kernel/include/kernel kernel/src -type f \
		\( -name '*.hpp' -o -name '*.cpp' \) -print0 | sort -z); \
	if [[ $${#sources[@]} -gt 0 ]]; then \
		$(CLANG_FORMAT) --dry-run --Werror "$${sources[@]}"; \
	fi

_docker-image: _check-docker-compose
	$(DOCKER_RUN_ENV) $(DOCKER_COMPOSE) build builder

_docker-iso: _docker-image
	$(DOCKER_RUN_ENV) $(DOCKER_COMPOSE) run --rm builder make _iso
