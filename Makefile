SHELL := /bin/bash
.DEFAULT_GOAL := help

PROJECT_ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
BUILD_DIR ?= $(PROJECT_ROOT)/build
GENERATOR ?= Ninja
GENERATOR_BIN ?= ninja
CMAKE ?= cmake
CLANG_CC ?= clang-19
CLANG_CXX ?= clang++-19
CLANG_FORMAT ?= clang-format-19
CLANG_TIDY ?= clang-tidy-19
LD_LLD ?= ld.lld-19
DOCKER_COMPOSE ?= $(shell if docker compose version >/dev/null 2>&1; then \
	printf 'docker compose'; \
elif command -v docker-compose >/dev/null 2>&1; then \
	printf 'docker-compose'; \
fi)
DOCKER_RUN_ENV := LOCAL_UID=$(shell id -u) LOCAL_GID=$(shell id -g)

INSTALL_DEPS := ./scripts/dev/install-deps-debian.sh
FORMAT_SOURCES := ./scripts/dev/format_sources.sh
RUN_TIDY := ./scripts/dev/run_tidy.sh
DISPLAY_PROFILE_SUMMARY := ./scripts/dev/summarize_display_profile.sh
RUN_QEMU := ./scripts/qemu/run_qemu.sh
BUILD_SMOKE_ISO := ./scripts/smoke/build_iso.sh
BUILD_SMOKE_SUITE := ./scripts/smoke/build_all.sh
SMOKE_DEMO := ./scripts/smoke/smoke_demo.sh
SMOKE_EXCEPTION := ./scripts/smoke/smoke_exception.sh
SMOKE_HEAP := ./scripts/smoke/smoke_heap.sh
SMOKE_PAGING := ./scripts/smoke/smoke_paging.sh
SMOKE_SLAB := ./scripts/smoke/smoke_slab.sh
SMOKE_TERMINAL_APP := ./scripts/smoke/smoke_terminal_app.sh
SMOKE_TIMER := ./scripts/smoke/smoke_timer.sh
RUN_SMOKE_SUITE := ./scripts/smoke/run_all.sh

TOOLCHAIN_FILE := $(PROJECT_ROOT)/cmake/toolchains/x86_64-none-clang.cmake
SMOKE_BUILD_ENV = BUILD_DIR=$(BUILD_DIR) CMAKE=$(CMAKE) GENERATOR=$(GENERATOR) TOOLCHAIN_FILE=$(TOOLCHAIN_FILE) GUI_PANEL_VISIBLE=$(GUI_PANEL_VISIBLE) TERMINAL_WINDOW_CHROME=$(TERMINAL_WINDOW_CHROME) DISPLAY_PROFILING=$(DISPLAY_PROFILING) DISPLAY_PROFILE_SCRIPT=$(DISPLAY_PROFILE_SCRIPT)
ISO_IMAGE := $(BUILD_DIR)/os-lab.iso
UNIT_BUILD_DIR := $(BUILD_DIR)/unit
EXCEPTION_SMOKE ?= invalid_opcode
GUI_PANEL_VISIBLE ?= OFF
TERMINAL_WINDOW_CHROME ?= OFF
DISPLAY_PROFILING ?= OFF
DISPLAY_PROFILE_SCRIPT ?= OFF
DISPLAY_PROFILE_LOG ?= $(BUILD_DIR)/logs/display-profile.log
EXCEPTION_ISO_IMAGE := $(BUILD_DIR)/exception-smoke/$(EXCEPTION_SMOKE)/os-lab.iso
TIMER_ISO_IMAGE := $(BUILD_DIR)/timer-smoke/os-lab.iso
PAGING_ISO_IMAGE := $(BUILD_DIR)/paging-smoke/os-lab.iso
HEAP_ISO_IMAGE := $(BUILD_DIR)/heap-smoke/os-lab.iso
SLAB_ISO_IMAGE := $(BUILD_DIR)/slab-smoke/os-lab.iso
TERMINAL_APP_ISO_IMAGE := $(BUILD_DIR)/terminal-app-smoke/os-lab.iso

.PHONY: help deps demo gui profile-gui profile-summary test demo-exception test-exception demo-timer test-timer test-paging test-heap test-slab test-terminal-app test-smoke unit format tidy shell clean ci
.PHONY: _check-native-tools _check-clang-format _check-clang-tidy _check-docker-compose _iso _run _run-gui _smoke _exception-iso _run-exception _smoke-exception _timer-iso _run-timer _smoke-timer _paging-iso _smoke-paging _heap-iso _smoke-heap _slab-iso _smoke-slab _terminal-app-iso _smoke-terminal-app _smoke-isos _smoke-all _unit _format _format-check _tidy _docker-image _docker-iso _docker-exception-iso _docker-timer-iso _docker-paging-iso _docker-heap-iso _docker-slab-iso _docker-terminal-app-iso _docker-smoke-isos

help:
	@printf '%s\n' \
		'Common targets:' \
		'  make deps      Install host QEMU on Debian/LMDE' \
		'  make demo      Build in Docker, then boot headless' \
		'  make gui       Build in Docker, then boot with a QEMU window' \
		'  make gui GUI_PANEL_VISIBLE=ON' \
		'                 Debug-only boot with the minimal GUI panel visible' \
		'  make gui TERMINAL_WINDOW_CHROME=ON' \
		'                 Debug-only boot with terminal window chrome visible' \
		'  make profile-gui' \
		'                 Boot GUI with profiling, scripted input, and serial log capture' \
		'  make profile-summary' \
		'                 Summarize build/logs/display-profile.log' \
		'  make test      Build in Docker, then run the QEMU smoke test' \
		'  make demo-exception EXCEPTION_SMOKE=page_fault' \
		'                 Build a debug exception ISO and show the exception dump' \
		'  make test-exception EXCEPTION_SMOKE=invalid_opcode' \
		'                 Build and verify a debug exception dump' \
		'  make test-timer' \
		'                 Build and verify the debug PIT timer smoke path' \
		'  make test-paging' \
		'                 Build and verify the debug active paging smoke path' \
		'  make test-heap' \
		'                 Build and verify the debug kernel heap smoke path' \
		'  make test-slab' \
		'                 Build and verify the debug kernel slab smoke path' \
		'  make test-terminal-app' \
		'                 Build and verify the debug terminal app lifecycle smoke path' \
		'  make test-smoke' \
		'                 Build and verify every debug smoke path' \
		'  make unit      Run host-side unit tests' \
		'  make format    Apply clang-format inside Docker' \
		'  make tidy      Run clang-tidy on host-side pure logic' \
		'  make shell     Open the development container' \
		'  make clean     Remove generated files'

deps:
	$(INSTALL_DEPS) host

demo: _docker-iso _run

gui: _docker-iso _run-gui

profile-gui:
	$(MAKE) _docker-iso DISPLAY_PROFILING=ON DISPLAY_PROFILE_SCRIPT=ON
	SERIAL_LOG="$(DISPLAY_PROFILE_LOG)" QEMU_HEADLESS=0 $(RUN_QEMU) $(ISO_IMAGE)

profile-summary:
	$(DISPLAY_PROFILE_SUMMARY) "$(DISPLAY_PROFILE_LOG)"

test: _docker-iso _smoke

demo-exception: _docker-exception-iso _run-exception

test-exception: _docker-exception-iso _smoke-exception

demo-timer: _docker-timer-iso _run-timer

test-timer: _docker-timer-iso _smoke-timer

test-paging: _docker-paging-iso _smoke-paging

test-heap: _docker-heap-iso _smoke-heap

test-slab: _docker-slab-iso _smoke-slab

test-terminal-app: _docker-terminal-app-iso _smoke-terminal-app

test-smoke: _docker-smoke-isos
	$(MAKE) _smoke-all EXCEPTION_SMOKE=$(EXCEPTION_SMOKE)

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
	@for tool in $(CLANG_CC) $(CLANG_CXX) g++ $(LD_LLD) $(CMAKE) xorriso curl tar sha256sum $(GENERATOR_BIN); do \
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
		printf 'Missing Docker Compose. Install the Docker Compose plugin or docker-compose.\n' >&2; \
		exit 1; \
	fi

_iso: _check-native-tools
	$(SMOKE_BUILD_ENV) $(BUILD_SMOKE_ISO) demo

_exception-iso: _check-native-tools
	$(SMOKE_BUILD_ENV) EXCEPTION_SMOKE=$(EXCEPTION_SMOKE) $(BUILD_SMOKE_ISO) exception

_timer-iso: _check-native-tools
	$(SMOKE_BUILD_ENV) $(BUILD_SMOKE_ISO) timer

_paging-iso: _check-native-tools
	$(SMOKE_BUILD_ENV) $(BUILD_SMOKE_ISO) paging

_heap-iso: _check-native-tools
	$(SMOKE_BUILD_ENV) $(BUILD_SMOKE_ISO) heap

_slab-iso: _check-native-tools
	$(SMOKE_BUILD_ENV) $(BUILD_SMOKE_ISO) slab

_terminal-app-iso: _check-native-tools
	$(SMOKE_BUILD_ENV) $(BUILD_SMOKE_ISO) terminal-app

_smoke-isos: _check-native-tools
	$(SMOKE_BUILD_ENV) EXCEPTION_SMOKE=$(EXCEPTION_SMOKE) $(BUILD_SMOKE_SUITE)

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

_smoke-heap:
	@if [[ ! -f "$(HEAP_ISO_IMAGE)" ]]; then $(MAKE) _heap-iso; fi
	$(SMOKE_HEAP) $(HEAP_ISO_IMAGE)

_smoke-slab:
	@if [[ ! -f "$(SLAB_ISO_IMAGE)" ]]; then $(MAKE) _slab-iso; fi
	$(SMOKE_SLAB) $(SLAB_ISO_IMAGE)

_smoke-terminal-app:
	@if [[ ! -f "$(TERMINAL_APP_ISO_IMAGE)" ]]; then $(MAKE) _terminal-app-iso; fi
	$(SMOKE_TERMINAL_APP) $(TERMINAL_APP_ISO_IMAGE)

_smoke-all:
	EXCEPTION_SMOKE=$(EXCEPTION_SMOKE) BUILD_DIR=$(BUILD_DIR) $(RUN_SMOKE_SUITE)

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
	$(DOCKER_RUN_ENV) $(DOCKER_COMPOSE) run --rm builder make _iso GUI_PANEL_VISIBLE=$(GUI_PANEL_VISIBLE) TERMINAL_WINDOW_CHROME=$(TERMINAL_WINDOW_CHROME) DISPLAY_PROFILING=$(DISPLAY_PROFILING) DISPLAY_PROFILE_SCRIPT=$(DISPLAY_PROFILE_SCRIPT)

_docker-exception-iso: _docker-image
	$(DOCKER_RUN_ENV) $(DOCKER_COMPOSE) run --rm builder make _exception-iso EXCEPTION_SMOKE=$(EXCEPTION_SMOKE) GUI_PANEL_VISIBLE=$(GUI_PANEL_VISIBLE) TERMINAL_WINDOW_CHROME=$(TERMINAL_WINDOW_CHROME) DISPLAY_PROFILING=$(DISPLAY_PROFILING) DISPLAY_PROFILE_SCRIPT=$(DISPLAY_PROFILE_SCRIPT)

_docker-timer-iso: _docker-image
	$(DOCKER_RUN_ENV) $(DOCKER_COMPOSE) run --rm builder make _timer-iso GUI_PANEL_VISIBLE=$(GUI_PANEL_VISIBLE) TERMINAL_WINDOW_CHROME=$(TERMINAL_WINDOW_CHROME) DISPLAY_PROFILING=$(DISPLAY_PROFILING) DISPLAY_PROFILE_SCRIPT=$(DISPLAY_PROFILE_SCRIPT)

_docker-paging-iso: _docker-image
	$(DOCKER_RUN_ENV) $(DOCKER_COMPOSE) run --rm builder make _paging-iso GUI_PANEL_VISIBLE=$(GUI_PANEL_VISIBLE) TERMINAL_WINDOW_CHROME=$(TERMINAL_WINDOW_CHROME) DISPLAY_PROFILING=$(DISPLAY_PROFILING) DISPLAY_PROFILE_SCRIPT=$(DISPLAY_PROFILE_SCRIPT)

_docker-heap-iso: _docker-image
	$(DOCKER_RUN_ENV) $(DOCKER_COMPOSE) run --rm builder make _heap-iso GUI_PANEL_VISIBLE=$(GUI_PANEL_VISIBLE) TERMINAL_WINDOW_CHROME=$(TERMINAL_WINDOW_CHROME) DISPLAY_PROFILING=$(DISPLAY_PROFILING) DISPLAY_PROFILE_SCRIPT=$(DISPLAY_PROFILE_SCRIPT)

_docker-slab-iso: _docker-image
	$(DOCKER_RUN_ENV) $(DOCKER_COMPOSE) run --rm builder make _slab-iso GUI_PANEL_VISIBLE=$(GUI_PANEL_VISIBLE) TERMINAL_WINDOW_CHROME=$(TERMINAL_WINDOW_CHROME) DISPLAY_PROFILING=$(DISPLAY_PROFILING) DISPLAY_PROFILE_SCRIPT=$(DISPLAY_PROFILE_SCRIPT)

_docker-terminal-app-iso: _docker-image
	$(DOCKER_RUN_ENV) $(DOCKER_COMPOSE) run --rm builder make _terminal-app-iso GUI_PANEL_VISIBLE=$(GUI_PANEL_VISIBLE) TERMINAL_WINDOW_CHROME=$(TERMINAL_WINDOW_CHROME) DISPLAY_PROFILING=$(DISPLAY_PROFILING) DISPLAY_PROFILE_SCRIPT=$(DISPLAY_PROFILE_SCRIPT)

_docker-smoke-isos: _docker-image
	$(DOCKER_RUN_ENV) $(DOCKER_COMPOSE) run --rm builder make _smoke-isos EXCEPTION_SMOKE=$(EXCEPTION_SMOKE) GUI_PANEL_VISIBLE=$(GUI_PANEL_VISIBLE) TERMINAL_WINDOW_CHROME=$(TERMINAL_WINDOW_CHROME) DISPLAY_PROFILING=$(DISPLAY_PROFILING) DISPLAY_PROFILE_SCRIPT=$(DISPLAY_PROFILE_SCRIPT)
