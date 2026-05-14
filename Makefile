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

.PHONY: help deps demo gui test demo-exception test-exception demo-timer test-timer unit format tidy shell clean ci
.PHONY: _check-native-tools _check-clang-format _check-clang-tidy _check-docker-compose _configure _kernel _iso _run _run-gui _smoke _exception-configure _exception-kernel _exception-iso _run-exception _smoke-exception _timer-configure _timer-kernel _timer-iso _run-timer _smoke-timer _unit _format _format-check _tidy _docker-image _docker-iso _docker-exception-iso _docker-timer-iso

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
		'  make unit      Run host-side unit tests' \
		'  make format    Apply clang-format inside Docker' \
		'  make tidy      Run clang-tidy on host-side pure logic' \
		'  make shell     Open the development container' \
		'  make clean     Remove generated files'

deps:
	./scripts/install-deps-debian.sh host

demo: _docker-iso _run

gui: _docker-iso _run-gui

test: _docker-iso _smoke

demo-exception: _docker-exception-iso _run-exception

test-exception: _docker-exception-iso _smoke-exception

demo-timer: _docker-timer-iso _run-timer

test-timer: _docker-timer-iso _smoke-timer

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
			printf 'Use `make demo` or install native deps with scripts/install-deps-debian.sh native.\n' >&2; \
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
	./scripts/create_iso.sh $(KERNEL_ELF) $(ISO_IMAGE)

_exception-configure: _check-native-tools
	$(CMAKE) -S $(PROJECT_ROOT) -B $(EXCEPTION_BUILD_DIR) -G $(GENERATOR) \
		-DCMAKE_BUILD_TYPE=Debug \
		-DCMAKE_TOOLCHAIN_FILE=$(TOOLCHAIN_FILE) \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
		-DOS_LAB_EXCEPTION_SMOKE=$(EXCEPTION_SMOKE)

_exception-kernel: _exception-configure
	$(CMAKE) --build $(EXCEPTION_BUILD_DIR) --target kernel

_exception-iso: _exception-kernel
	./scripts/create_iso.sh $(EXCEPTION_KERNEL_ELF) $(EXCEPTION_ISO_IMAGE)

_timer-configure: _check-native-tools
	$(CMAKE) -S $(PROJECT_ROOT) -B $(TIMER_BUILD_DIR) -G $(GENERATOR) \
		-DCMAKE_BUILD_TYPE=Debug \
		-DCMAKE_TOOLCHAIN_FILE=$(TOOLCHAIN_FILE) \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
		-DOS_LAB_TIMER_SMOKE=ON

_timer-kernel: _timer-configure
	$(CMAKE) --build $(TIMER_BUILD_DIR) --target kernel

_timer-iso: _timer-kernel
	./scripts/create_iso.sh $(TIMER_KERNEL_ELF) $(TIMER_ISO_IMAGE)

_run:
	@if [[ ! -f "$(ISO_IMAGE)" ]]; then $(MAKE) _iso; fi
	./scripts/run_qemu.sh $(ISO_IMAGE)

_run-gui:
	@if [[ ! -f "$(ISO_IMAGE)" ]]; then $(MAKE) _iso; fi
	QEMU_HEADLESS=0 ./scripts/run_qemu.sh $(ISO_IMAGE)

_run-exception:
	@if [[ ! -f "$(EXCEPTION_ISO_IMAGE)" ]]; then $(MAKE) _exception-iso EXCEPTION_SMOKE=$(EXCEPTION_SMOKE); fi
	@set -euo pipefail; \
	status=0; \
	timeout 15s ./scripts/run_qemu.sh "$(EXCEPTION_ISO_IMAGE)" || status=$$?; \
	if [[ $$status -ne 0 && $$status -ne 124 ]]; then exit "$$status"; fi

_run-timer:
	@if [[ ! -f "$(TIMER_ISO_IMAGE)" ]]; then $(MAKE) _timer-iso; fi
	@set -euo pipefail; \
	status=0; \
	timeout 15s ./scripts/run_qemu.sh "$(TIMER_ISO_IMAGE)" || status=$$?; \
	if [[ $$status -ne 0 && $$status -ne 124 ]]; then exit "$$status"; fi

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

_smoke-exception:
	@if [[ ! -f "$(EXCEPTION_ISO_IMAGE)" ]]; then $(MAKE) _exception-iso EXCEPTION_SMOKE=$(EXCEPTION_SMOKE); fi
	@set -euo pipefail; \
	case "$(EXCEPTION_SMOKE)" in \
		invalid_opcode) expected='invalid opcode' ;; \
		page_fault) expected='page fault' ;; \
		divide_error) expected='divide error' ;; \
		*) printf 'Invalid EXCEPTION_SMOKE: %s\n' "$(EXCEPTION_SMOKE)" >&2; \
		   printf 'Use one of: invalid_opcode, page_fault, divide_error\n' >&2; exit 1 ;; \
	esac; \
	log_file=$$(mktemp); \
	trap 'rm -f "$$log_file"' EXIT; \
	set +e; \
	timeout 15s ./scripts/run_qemu.sh "$(EXCEPTION_ISO_IMAGE)" | tee "$$log_file"; \
	status=$$?; \
	set -e; \
	if [[ $$status -ne 0 && $$status -ne 124 ]]; then exit "$$status"; fi; \
	grep -q "os-lab: triggering exception smoke: $$expected" "$$log_file"; \
	grep -q "kernel exception" "$$log_file"; \
	grep -q "name: $$expected" "$$log_file"; \
	grep -q "vector:" "$$log_file"; \
	grep -q "error code:" "$$log_file"; \
	grep -q "rip:" "$$log_file"; \
	grep -q "rsp:" "$$log_file"; \
	grep -q "rflags:" "$$log_file"; \
	if [[ "$(EXCEPTION_SMOKE)" == "page_fault" ]]; then grep -q "cr2:" "$$log_file"; fi; \
	printf 'Exception smoke passed: %s\n' "$$expected"

_smoke-timer:
	@if [[ ! -f "$(TIMER_ISO_IMAGE)" ]]; then $(MAKE) _timer-iso; fi
	@set -euo pipefail; \
	log_file=$$(mktemp); \
	trap 'rm -f "$$log_file"' EXIT; \
	set +e; \
	timeout 15s ./scripts/run_qemu.sh "$(TIMER_ISO_IMAGE)" | tee "$$log_file"; \
	status=$$?; \
	set -e; \
	if [[ $$status -ne 0 && $$status -ne 124 ]]; then exit "$$status"; fi; \
	grep -q "os-lab: PIT timer configured at 100 Hz" "$$log_file"; \
	grep -q "os-lab: hardware interrupts enabled" "$$log_file"; \
	grep -q "os-lab: timer smoke waiting for PIT ticks" "$$log_file"; \
	grep -q "os-lab: timer smoke passed ticks=" "$$log_file"; \
	printf 'Timer smoke passed\n'

_unit:
	$(CMAKE) -S $(PROJECT_ROOT) -B $(UNIT_BUILD_DIR) -G $(GENERATOR) \
		-DCMAKE_BUILD_TYPE=Debug \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
		-DOS_LAB_BUILD_KERNEL=OFF \
		-DOS_LAB_BUILD_TESTS=ON
	$(CMAKE) --build $(UNIT_BUILD_DIR)
	cd $(UNIT_BUILD_DIR) && ctest --output-on-failure

_format: _check-clang-format
	@mapfile -d '' sources < <(find kernel/include/kernel kernel/src tests -type f \
		\( -name '*.hpp' -o -name '*.cpp' \) -print0 | sort -z); \
	if [[ $${#sources[@]} -gt 0 ]]; then \
		$(CLANG_FORMAT) -i "$${sources[@]}"; \
	fi

_format-check: _check-clang-format
	@mapfile -d '' sources < <(find kernel/include/kernel kernel/src tests -type f \
		\( -name '*.hpp' -o -name '*.cpp' \) -print0 | sort -z); \
	if [[ $${#sources[@]} -gt 0 ]]; then \
		$(CLANG_FORMAT) --dry-run --Werror "$${sources[@]}"; \
	fi

_tidy: _check-clang-tidy _unit
	@mapfile -d '' sources < <( \
		find kernel/src/text tests/unit -type f -name '*.cpp' -print0; \
		printf '%s\0' \
			kernel/src/display/display.cpp \
			kernel/src/input/keyboard_decoder.cpp \
			kernel/src/input/mouse_packet_decoder.cpp \
			kernel/src/input/pointer_state.cpp \
			kernel/src/memory/memory_map.cpp \
			kernel/src/memory/physical_frame_allocator.cpp \
			kernel/src/shell/shell_command.cpp; \
	); \
	if [[ $${#sources[@]} -gt 0 ]]; then \
		$(CLANG_TIDY) -p $(UNIT_BUILD_DIR) --quiet "$${sources[@]}" \
			2> >(grep -vE '^[0-9]+ warnings generated\.$$' >&2); \
	fi

_docker-image: _check-docker-compose
	$(DOCKER_RUN_ENV) $(DOCKER_COMPOSE) build builder

_docker-iso: _docker-image
	$(DOCKER_RUN_ENV) $(DOCKER_COMPOSE) run --rm builder make _iso

_docker-exception-iso: _docker-image
	$(DOCKER_RUN_ENV) $(DOCKER_COMPOSE) run --rm builder make _exception-iso EXCEPTION_SMOKE=$(EXCEPTION_SMOKE)

_docker-timer-iso: _docker-image
	$(DOCKER_RUN_ENV) $(DOCKER_COMPOSE) run --rm builder make _timer-iso
