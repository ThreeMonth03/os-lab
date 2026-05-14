SHELL := /bin/bash
.DEFAULT_GOAL := help

PROJECT_ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
BUILD_DIR ?= $(PROJECT_ROOT)/build
GENERATOR ?= Ninja
GENERATOR_BIN ?= ninja
CMAKE ?= cmake
CLANG_FORMAT ?= clang-format-19
CLANG_TIDY ?= clang-tidy-19
DOCKER_COMPOSE ?= $(shell if docker compose version >/dev/null 2>&1; then \
	printf 'docker compose'; \
elif command -v docker-compose >/dev/null 2>&1; then \
	printf 'docker-compose'; \
fi)
DOCKER_RUN_ENV := LOCAL_UID=$(shell id -u) LOCAL_GID=$(shell id -g)

CREATE_ISO := ./scripts/build/create_iso.sh
INSTALL_DEPS := ./scripts/dev/install-deps-debian.sh
FORMAT_SOURCES := ./scripts/dev/format_sources.sh
RUN_TIDY := ./scripts/dev/run_tidy.sh
RUN_QEMU := ./scripts/qemu/run_qemu.sh
SMOKE_DEMO := ./scripts/smoke/smoke_demo.sh
SMOKE_EXCEPTION := ./scripts/smoke/smoke_exception.sh
SMOKE_PAGING := ./scripts/smoke/smoke_paging.sh
SMOKE_TIMER := ./scripts/smoke/smoke_timer.sh

TOOLCHAIN_FILE := $(PROJECT_ROOT)/cmake/toolchains/x86_64-none-clang.cmake
KERNEL_ELF := $(BUILD_DIR)/artifacts/kernel.elf
ISO_IMAGE := $(BUILD_DIR)/os-lab.iso
UNIT_BUILD_DIR := $(BUILD_DIR)/unit
EXCEPTION_SMOKE ?= invalid_opcode
EXCEPTION_BUILD_DIR := $(BUILD_DIR)/exception-smoke/$(EXCEPTION_SMOKE)
EXCEPTION_KERNEL_ELF := $(EXCEPTION_BUILD_DIR)/artifacts/kernel.elf
EXCEPTION_ISO_IMAGE := $(EXCEPTION_BUILD_DIR)/os-lab.iso
TIMER_BUILD_DIR := $(BUILD_DIR)/timer-smoke
TIMER_KERNEL_ELF := $(TIMER_BUILD_DIR)/artifacts/kernel.elf
TIMER_ISO_IMAGE := $(TIMER_BUILD_DIR)/os-lab.iso
PAGING_BUILD_DIR := $(BUILD_DIR)/paging-smoke
PAGING_KERNEL_ELF := $(PAGING_BUILD_DIR)/artifacts/kernel.elf
PAGING_ISO_IMAGE := $(PAGING_BUILD_DIR)/os-lab.iso

.PHONY: help deps demo gui test demo-exception test-exception demo-timer test-timer test-paging unit format tidy shell clean ci
.PHONY: _check-native-tools _check-clang-format _check-clang-tidy _check-docker-compose _configure _kernel _iso _run _run-gui _smoke _exception-configure _exception-kernel _exception-iso _run-exception _smoke-exception _timer-configure _timer-kernel _timer-iso _run-timer _smoke-timer _paging-configure _paging-kernel _paging-iso _smoke-paging _unit _format _format-check _tidy _docker-image _docker-iso _docker-exception-iso _docker-timer-iso _docker-paging-iso

help:
	@printf '%s\n' \
		'Common targets:' \
		'  make deps      Install host QEMU on Debian/LMDE' \
		'  make demo      Build in Docker, then boot headless' \
		'  make gui       Build in Docker, then boot with a QEMU window' \
		'  make test      Build in Docker, then run the QEMU smoke test' \
		'  make demo-exception EXCEPTION_SMOKE=page_fault' \
		'                 Build a debug exception ISO and show the exception dump' \
		'  make test-exception EXCEPTION_SMOKE=invalid_opcode' \
		'                 Build and verify a debug exception dump' \
		'  make test-timer' \
		'                 Build and verify the debug PIT timer smoke path' \
		'  make test-paging' \
		'                 Build and verify the debug active paging smoke path' \
		'  make unit      Run host-side unit tests' \
		'  make format    Apply clang-format inside Docker' \
		'  make tidy      Run clang-tidy on host-side pure logic' \
		'  make shell     Open the development container' \
		'  make clean     Remove generated files'

deps:
	$(INSTALL_DEPS) host

demo: _docker-iso _run

gui: _docker-iso _run-gui

test: _docker-iso _smoke

demo-exception: _docker-exception-iso _run-exception

test-exception: _docker-exception-iso _smoke-exception

demo-timer: _docker-timer-iso _run-timer

test-timer: _docker-timer-iso _smoke-timer

test-paging: _docker-paging-iso _smoke-paging

unit: _docker-image
	$(DOCKER_RUN_ENV) $(DOCKER_COMPOSE) run --rm builder make _unit

format: _docker-image
	$(DOCKER_RUN_ENV) $(DOCKER_COMPOSE) run --rm builder make _format

tidy: _docker-image
	$(DOCKER_RUN_ENV) $(DOCKER_COMPOSE) run --rm builder make _tidy

shell: _docker-image
	$(DOCKER_RUN_ENV) $(DOCKER_COMPOSE) run --rm builder bash

clean:
	rm -rf $(BUILD_DIR) .cache

ci: _format-check _iso _smoke

_check-native-tools:
	@for tool in clang clang++ g++ ld.lld $(CMAKE) xorriso curl tar sha256sum $(GENERATOR_BIN); do \
		if ! command -v "$$tool" >/dev/null 2>&1; then \
			printf 'Missing native tool: %s\n' "$$tool" >&2; \
			printf 'Use `make demo` or install native deps with $(INSTALL_DEPS) native.\n' >&2; \
			exit 1; \
		fi; \
	done

_check-clang-format:
	@if ! command -v $(CLANG_FORMAT) >/dev/null 2>&1; then \
		printf 'Missing tool: %s\n' '$(CLANG_FORMAT)' >&2; \
		exit 1; \
	fi
	@version="$$( $(CLANG_FORMAT) --version 2>/dev/null | sed -n 's/.*version \([0-9][0-9]*\).*/\1/p' )"; \
	if [[ -z "$$version" || "$$version" -lt 19 ]]; then \
		printf 'clang-format >= 19 is required for the project style; found: %s\n' "$$($(CLANG_FORMAT) --version)" >&2; \
		exit 1; \
	fi

_check-clang-tidy:
	@if ! command -v $(CLANG_TIDY) >/dev/null 2>&1; then \
		printf 'Missing tool: %s\n' '$(CLANG_TIDY)' >&2; \
		exit 1; \
	fi
	@version="$$( $(CLANG_TIDY) --version 2>/dev/null | sed -n 's/.*version \([0-9][0-9]*\).*/\1/p' )"; \
	if [[ -z "$$version" || "$$version" -lt 19 ]]; then \
		printf 'clang-tidy >= 19 is required; found: %s\n' "$$($(CLANG_TIDY) --version)" >&2; \
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
	$(CREATE_ISO) $(KERNEL_ELF) $(ISO_IMAGE)

_exception-configure: _check-native-tools
	$(CMAKE) -S $(PROJECT_ROOT) -B $(EXCEPTION_BUILD_DIR) -G $(GENERATOR) \
		-DCMAKE_BUILD_TYPE=Debug \
		-DCMAKE_TOOLCHAIN_FILE=$(TOOLCHAIN_FILE) \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
		-DOS_LAB_EXCEPTION_SMOKE=$(EXCEPTION_SMOKE)

_exception-kernel: _exception-configure
	$(CMAKE) --build $(EXCEPTION_BUILD_DIR) --target kernel

_exception-iso: _exception-kernel
	$(CREATE_ISO) $(EXCEPTION_KERNEL_ELF) $(EXCEPTION_ISO_IMAGE)

_timer-configure: _check-native-tools
	$(CMAKE) -S $(PROJECT_ROOT) -B $(TIMER_BUILD_DIR) -G $(GENERATOR) \
		-DCMAKE_BUILD_TYPE=Debug \
		-DCMAKE_TOOLCHAIN_FILE=$(TOOLCHAIN_FILE) \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
		-DOS_LAB_TIMER_SMOKE=ON

_timer-kernel: _timer-configure
	$(CMAKE) --build $(TIMER_BUILD_DIR) --target kernel

_timer-iso: _timer-kernel
	$(CREATE_ISO) $(TIMER_KERNEL_ELF) $(TIMER_ISO_IMAGE)

_paging-configure: _check-native-tools
	$(CMAKE) -S $(PROJECT_ROOT) -B $(PAGING_BUILD_DIR) -G $(GENERATOR) \
		-DCMAKE_BUILD_TYPE=Debug \
		-DCMAKE_TOOLCHAIN_FILE=$(TOOLCHAIN_FILE) \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
		-DOS_LAB_PAGING_SMOKE=ON

_paging-kernel: _paging-configure
	$(CMAKE) --build $(PAGING_BUILD_DIR) --target kernel

_paging-iso: _paging-kernel
	$(CREATE_ISO) $(PAGING_KERNEL_ELF) $(PAGING_ISO_IMAGE)

_run:
	@if [[ ! -f "$(ISO_IMAGE)" ]]; then $(MAKE) _iso; fi
	$(RUN_QEMU) $(ISO_IMAGE)

_run-gui:
	@if [[ ! -f "$(ISO_IMAGE)" ]]; then $(MAKE) _iso; fi
	QEMU_HEADLESS=0 $(RUN_QEMU) $(ISO_IMAGE)

_run-exception:
	@if [[ ! -f "$(EXCEPTION_ISO_IMAGE)" ]]; then $(MAKE) _exception-iso EXCEPTION_SMOKE=$(EXCEPTION_SMOKE); fi
	@set -euo pipefail; \
	status=0; \
	timeout 15s $(RUN_QEMU) "$(EXCEPTION_ISO_IMAGE)" || status=$$?; \
	if [[ $$status -ne 0 && $$status -ne 124 ]]; then exit "$$status"; fi

_run-timer:
	@if [[ ! -f "$(TIMER_ISO_IMAGE)" ]]; then $(MAKE) _timer-iso; fi
	@set -euo pipefail; \
	status=0; \
	timeout 15s $(RUN_QEMU) "$(TIMER_ISO_IMAGE)" || status=$$?; \
	if [[ $$status -ne 0 && $$status -ne 124 ]]; then exit "$$status"; fi

_smoke:
	@if [[ ! -f "$(ISO_IMAGE)" ]]; then $(MAKE) _iso; fi
	$(SMOKE_DEMO) $(ISO_IMAGE)

_smoke-exception:
	@if [[ ! -f "$(EXCEPTION_ISO_IMAGE)" ]]; then $(MAKE) _exception-iso EXCEPTION_SMOKE=$(EXCEPTION_SMOKE); fi
	$(SMOKE_EXCEPTION) $(EXCEPTION_ISO_IMAGE) $(EXCEPTION_SMOKE)

_smoke-timer:
	@if [[ ! -f "$(TIMER_ISO_IMAGE)" ]]; then $(MAKE) _timer-iso; fi
	$(SMOKE_TIMER) $(TIMER_ISO_IMAGE)

_smoke-paging:
	@if [[ ! -f "$(PAGING_ISO_IMAGE)" ]]; then $(MAKE) _paging-iso; fi
	$(SMOKE_PAGING) $(PAGING_ISO_IMAGE)

_unit:
	$(CMAKE) -S $(PROJECT_ROOT) -B $(UNIT_BUILD_DIR) -G $(GENERATOR) \
		-DCMAKE_BUILD_TYPE=Debug \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
		-DOS_LAB_BUILD_KERNEL=OFF \
		-DOS_LAB_BUILD_TESTS=ON
	$(CMAKE) --build $(UNIT_BUILD_DIR)
	cd $(UNIT_BUILD_DIR) && ctest --output-on-failure

_format: _check-clang-format
	$(FORMAT_SOURCES) apply "$(CLANG_FORMAT)"

_format-check: _check-clang-format
	$(FORMAT_SOURCES) check "$(CLANG_FORMAT)"

_tidy: _check-clang-tidy _unit
	$(RUN_TIDY) "$(UNIT_BUILD_DIR)" "$(CLANG_TIDY)"

_docker-image: _check-docker-compose
	$(DOCKER_RUN_ENV) $(DOCKER_COMPOSE) build builder

_docker-iso: _docker-image
	$(DOCKER_RUN_ENV) $(DOCKER_COMPOSE) run --rm builder make _iso

_docker-exception-iso: _docker-image
	$(DOCKER_RUN_ENV) $(DOCKER_COMPOSE) run --rm builder make _exception-iso EXCEPTION_SMOKE=$(EXCEPTION_SMOKE)

_docker-timer-iso: _docker-image
	$(DOCKER_RUN_ENV) $(DOCKER_COMPOSE) run --rm builder make _timer-iso

_docker-paging-iso: _docker-image
	$(DOCKER_RUN_ENV) $(DOCKER_COMPOSE) run --rm builder make _paging-iso
