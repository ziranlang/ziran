CC ?= cc
AR ?= ar
OBJCOPY ?= objcopy
CFLAGS ?= -O2
override CFLAGS += -D_GNU_SOURCE -std=c11 -Iinclude -Icmd/zir
DEPFLAGS = -MMD -MP

# Build in parallel on half the cores, between 2 and 16 jobs, at low
# priority so the desktop and other work keep the CPU when they need it.
# `make -jN` and `NICE=` override.
JOBS ?= $(shell n=$$(nproc 2>/dev/null || echo 2); n=$$((n / 2)); \
    [ $$n -lt 2 ] && n=2; [ $$n -gt 16 ] && n=16; echo $$n)
NICE ?= nice -n 10
ifeq ($(filter -j%,$(MAKEFLAGS)),)
MAKEFLAGS += -j$(JOBS)
endif

BUILD_DIR ?= build
BIN_DIR := $(BUILD_DIR)/bin
FRONTEND := cmd/zir/zir.c cmd/zir/zir_enum.c cmd/zir/zir_text.c \
    cmd/zir/zir_token.c cmd/zir/zir_cleanup.c cmd/zir/zir_expr.c \
    cmd/zir/zir_borrow.c cmd/zir/zir_law.c \
    cmd/zir/zir_serial.c cmd/zir/zir_load.c \
    cmd/zir/zir_packages.c \
    cmd/zir/zir_diagnostic.c
PORTABLE := cmd/zir/zir_bundle.c
HEADERS := $(wildcard cmd/zir/*.h) $(wildcard include/*.h)
LIB_SOURCES := $(FRONTEND) $(PORTABLE) cmd/zir/zir_host.c

# Every C file compiles once to $(BUILD_DIR)/obj/<path>.o; binaries link objects.
obj = $(patsubst %.c,$(BUILD_DIR)/obj/%.o,$(1))
LIB_OBJECTS := $(call obj,$(LIB_SOURCES)) $(BUILD_DIR)/obj/check.o \
    $(BUILD_DIR)/obj/parse.o $(BUILD_DIR)/obj/emit.o $(BUILD_DIR)/obj/vm.o
FRONTEND_OBJECTS := $(call obj,$(FRONTEND)) $(BUILD_DIR)/obj/check.o \
    $(BUILD_DIR)/obj/parse.o $(BUILD_DIR)/obj/emit.o
BUNDLE_OBJECT := $(call obj,cmd/zir/zir_bundle.c)
RUNTIME_OBJECTS := $(call obj,cmd/zir/zir_runtime.c) $(BUILD_DIR)/obj/runtime_headers.o

.PHONY: all check curl-http-test clean install-user package-objects
CHECK_JOBS ?= 4
all: $(BIN_DIR)/ziran $(BIN_DIR)/zi-fmt $(BIN_DIR)/zi2zir $(BIN_DIR)/zi-api $(BIN_DIR)/zi-inspect $(BIN_DIR)/zi2c $(BIN_DIR)/zi2go $(BIN_DIR)/zi2cpp $(BIN_DIR)/zi2rust $(BIN_DIR)/zi2zib $(BUILD_DIR)/libziran.a

USER_BIN ?= $(HOME)/.local/bin
USER_SHARE ?= $(HOME)/.local/share/ziran/bootstrap
install-user: all
	mkdir -p $(USER_BIN) $(USER_SHARE)/build/bin
	cp $(BIN_DIR)/ziran $(BIN_DIR)/zi-fmt $(BIN_DIR)/zi2zir $(BIN_DIR)/zi-api \
	    $(BIN_DIR)/zi-inspect $(BIN_DIR)/zi2c $(BIN_DIR)/zi2go \
	    $(BIN_DIR)/zi2cpp $(BIN_DIR)/zi2rust $(BIN_DIR)/zi2zib \
	    $(USER_SHARE)/build/bin/
	$(RM) $(USER_SHARE)/build/bin/ziran_pkg.py $(USER_SHARE)/build/bin/ziran-add
	printf '%s\n' '#!/bin/sh' 'set -eu' \
		'exec "$(USER_SHARE)/build/bin/ziran" "$$@"' > $(USER_BIN)/ziran
	chmod 755 $(USER_BIN)/ziran

$(BUILD_DIR)/obj/%.o: %.c
	@mkdir -p $(dir $@)
	$(NICE) $(CC) $(CFLAGS) $(DEPFLAGS) -c -o $@ $<

$(BUILD_DIR)/obj/runtime_headers.o: $(BUILD_DIR)/runtime_headers.c
	@mkdir -p $(dir $@)
	$(NICE) $(CC) $(CFLAGS) $(DEPFLAGS) -c -o $@ $<

# Large modules compile in parts, then merge into one object each; their
# shared helpers are hidden and localized so they never leave it.
merge = $(CC) -r -nostdlib -o $@ $^ && $(OBJCOPY) --localize-hidden $@
PARSE_PARTS := $(addprefix cmd/zir/zir_parse,.c _declaration.c _eval.c _typed.c \
    _condition.c _discover.c _source.c)
CHECK_PARTS := $(addprefix cmd/zir/zir_check,.c _value.c _expr.c _statement.c \
    _function.c _link.c _program.c)
EMIT_PARTS := $(addprefix cmd/zir/zir_emit,.c _value.c _call.c _expr.c _statement.c)
VM_PARTS := $(addprefix cmd/zir/zir_vm,.c _verify.c _eval.c _run.c)

$(BUILD_DIR)/obj/parse.o: $(call obj,$(PARSE_PARTS))
	$(merge)

$(BUILD_DIR)/obj/check.o: $(call obj,$(CHECK_PARTS))
	$(merge)

$(BUILD_DIR)/obj/emit.o: $(call obj,$(EMIT_PARTS))
	$(merge)

$(BUILD_DIR)/obj/vm.o: $(call obj,$(VM_PARTS))
	$(merge)

-include $(shell find $(BUILD_DIR)/obj -name '*.d' 2>/dev/null)

$(BUILD_DIR)/libziran.a: $(LIB_OBJECTS) Makefile
	$(RM) $@
	$(AR) rcs $@ $(LIB_OBJECTS)

$(BIN_DIR):
	mkdir -p $@

PACKAGE_C := $(BUILD_DIR)/package-c
PACKAGE_SOURCES := cmd/package.zi cmd/package_add.zi cmd/package_guide.zi \
    cmd/package_capabilities.zi cmd/package_features.zi cmd/package_explain.zi cmd/package_common.zi \
    cmd/package_manifest.zi cmd/package_lock.zi cmd/package_map.zi cmd/package_options.zi \
    std/byte_text_linux.zi std/file_linux.zi std/process_capture_linux.zi \
    std/json_scan.zi std/text.zi
# Generated files are listed when the ziran recipe expands, after generation.
PACKAGE_OBJECTS = $(patsubst $(PACKAGE_C)/%.c,$(BUILD_DIR)/obj/package-c/%.o,$(wildcard $(PACKAGE_C)/*.c)) \
    $(call obj,cmd/package_main.c cmd/package_host.c)

# Regenerate beside the old output and copy over only files whose text
# changed, so a compiler edit that leaves ziran's C alone recompiles nothing.
$(PACKAGE_C)/.generated: $(PACKAGE_SOURCES) $(BIN_DIR)/zi2c
	rm -rf $(PACKAGE_C).next
	$(BIN_DIR)/zi2c --no-main --root cmd --module-path std -o $(PACKAGE_C).next cmd/package.zi
	mkdir -p $(PACKAGE_C)
	for f in $(PACKAGE_C).next/*; do \
	    cmp -s "$$f" "$(PACKAGE_C)/$${f##*/}" || cp "$$f" $(PACKAGE_C)/; done
	for f in $(PACKAGE_C)/*; do \
	    [ -e "$(PACKAGE_C).next/$${f##*/}" ] || rm -f "$$f"; done
	rm -rf $(PACKAGE_C).next
	touch $@

$(BUILD_DIR)/obj/package-c/%.o: $(PACKAGE_C)/%.c
	@mkdir -p $(dir $@)
	$(NICE) $(CC) $(CFLAGS) -I$(PACKAGE_C) $(DEPFLAGS) -c -o $@ $<

$(call obj,cmd/package_main.c cmd/package_host.c): CFLAGS += -I$(PACKAGE_C)

package-objects: $(PACKAGE_OBJECTS)

$(BIN_DIR)/ziran: $(PACKAGE_C)/.generated cmd/package_main.c cmd/package_host.c $(HEADERS) | $(BIN_DIR)
	+$(MAKE) --no-print-directory package-objects
	$(CC) $(CFLAGS) -o $@ $(PACKAGE_OBJECTS) -lcrypto -lm

$(BIN_DIR)/zi-fmt: scripts/zi-fmt.sh | $(BIN_DIR)
	cp $< $@
	chmod +x $@

$(BIN_DIR)/zi2zir: $(call obj,cmd/zir-ir/main.c) $(BUNDLE_OBJECT) $(FRONTEND_OBJECTS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^

$(BIN_DIR)/zi-api: $(call obj,cmd/zir-api/main.c) $(FRONTEND_OBJECTS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^

$(BIN_DIR)/zi-inspect: $(call obj,cmd/zir-inspect/main.c) $(FRONTEND_OBJECTS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^

RUNTIME_HEADERS := include/zir_bounds.h include/zir_string.h include/zir_slice.h \
    include/zir_vec.h include/ziran_parallel.h

$(BUILD_DIR)/runtime_headers.c: scripts/embed_headers.sh $(RUNTIME_HEADERS)
	mkdir -p $(dir $@)
	sh scripts/embed_headers.sh $(RUNTIME_HEADERS) > $@

$(BIN_DIR)/zi2c: $(call obj,cmd/zir-c/main.c cmd/zir-c/zir_c_lower.c cmd/zir-c/zir_c_plan9.c) \
    $(BUNDLE_OBJECT) $(FRONTEND_OBJECTS) $(RUNTIME_OBJECTS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^

$(BIN_DIR)/zi2go: $(call obj,cmd/zir-go/main.c cmd/zir-go/zir_go_lower.c) $(BUNDLE_OBJECT) $(FRONTEND_OBJECTS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^

$(BIN_DIR)/zi2cpp: $(call obj,cmd/zir-cpp/main.c cmd/zir-cpp/zir_cpp_lower.c) \
    $(BUNDLE_OBJECT) $(FRONTEND_OBJECTS) $(RUNTIME_OBJECTS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^

$(BIN_DIR)/zi2rust: $(call obj,cmd/zir-rust/main.c cmd/zir-rust/zir_rust_lower.c) $(BUNDLE_OBJECT) $(FRONTEND_OBJECTS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ -pthread

$(BIN_DIR)/zi2zib: $(call obj,cmd/zir-zib/main.c) $(BUILD_DIR)/libziran.a | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^

$(BIN_DIR)/bundle-link-test: $(call obj,tests/bundle_link_test.c) $(FRONTEND_OBJECTS) $(call obj,$(PORTABLE)) \
    $(BUILD_DIR)/obj/vm.o | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^

$(BIN_DIR)/host-capability-test: tests/host_capability_test.c $(BUILD_DIR)/libziran.a | $(BIN_DIR)
	$(CC) -D_GNU_SOURCE -std=c11 -Iinclude -o $@ tests/host_capability_test.c $(BUILD_DIR)/libziran.a

$(BIN_DIR)/record-host-test: tests/record_host_test.c $(BUILD_DIR)/libziran.a | $(BIN_DIR)
	$(CC) -D_GNU_SOURCE -std=c11 -Iinclude -o $@ tests/record_host_test.c $(BUILD_DIR)/libziran.a

$(BIN_DIR)/array-host-test: tests/host_array_test.c $(BUILD_DIR)/libziran.a | $(BIN_DIR)
	$(CC) -D_GNU_SOURCE -std=c11 -Iinclude -o $@ tests/host_array_test.c $(BUILD_DIR)/libziran.a

$(BIN_DIR)/startup-graph-test: tests/startup_graph_test.c $(BUILD_DIR)/libziran.a | $(BIN_DIR)
	$(CC) -D_GNU_SOURCE -std=c11 -Iinclude -Icmd/zir -o $@ \
		tests/startup_graph_test.c $(BUILD_DIR)/libziran.a

$(BIN_DIR)/vm-vec-scope-test: tests/vm_vec_scope_test.c $(BUILD_DIR)/libziran.a | $(BIN_DIR)
	$(CC) -D_GNU_SOURCE -std=c11 -Iinclude -Icmd/zir -o $@ \
		tests/vm_vec_scope_test.c $(BUILD_DIR)/libziran.a

$(BIN_DIR)/process-host-test: tests/process_host_test.c $(BUILD_DIR)/libziran.a | $(BIN_DIR)
	$(CC) -D_GNU_SOURCE -std=c11 -Iinclude -o $@ tests/process_host_test.c $(BUILD_DIR)/libziran.a

$(BIN_DIR)/slice-host-test: tests/host_slice_test.c $(BUILD_DIR)/libziran.a | $(BIN_DIR)
	$(CC) -D_GNU_SOURCE -std=c11 -Iinclude -o $@ tests/host_slice_test.c $(BUILD_DIR)/libziran.a

check: all $(BIN_DIR)/bundle-link-test $(BIN_DIR)/host-capability-test $(BIN_DIR)/record-host-test $(BIN_DIR)/array-host-test $(BIN_DIR)/slice-host-test $(BIN_DIR)/process-host-test $(BIN_DIR)/startup-graph-test $(BIN_DIR)/vm-vec-scope-test
	env -u DISPLAY -u WAYLAND_DISPLAY $(BIN_DIR)/startup-graph-test
	python3 tests/run_check.py --bin-dir $(BIN_DIR) --jobs $(CHECK_JOBS)

curl-http-test: $(BIN_DIR)/zi2c
	python3 tests/net_http_curl_linux_test.py $(BIN_DIR)/zi2c

clean:
	rm -rf $(BUILD_DIR)
