CC ?= cc
AR ?= ar
CFLAGS ?= -O2
CFLAGS += -D_GNU_SOURCE -std=c11 -Iinclude -Icmd/zir

BUILD_DIR ?= build
BIN_DIR := $(BUILD_DIR)/bin
FRONTEND := cmd/zir/zir.c cmd/zir/zir_enum.c cmd/zir/zir_parse.c cmd/zir/zir_text.c \
    cmd/zir/zir_token.c cmd/zir/zir_cleanup.c cmd/zir/zir_expr.c \
    cmd/zir/zir_check.c cmd/zir/zir_borrow.c cmd/zir/zir_law.c \
    cmd/zir/zir_emit.c cmd/zir/zir_serial.c cmd/zir/zir_load.c \
    cmd/zir/zir_packages.c \
    cmd/zir/zir_diagnostic.c
PORTABLE := cmd/zir/zir_bundle.c cmd/zir/zir_vm.c
HEADERS := $(wildcard cmd/zir/*.h) $(wildcard include/*.h)
LIB_SOURCES := $(FRONTEND) $(PORTABLE) cmd/zir/zir_host.c
LIB_OBJECTS := $(patsubst cmd/zir/%.c,$(BUILD_DIR)/obj/%.o,$(LIB_SOURCES))

.PHONY: all check curl-http-test clean install-user
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

$(BUILD_DIR)/obj/%.o: cmd/zir/%.c $(HEADERS)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/libziran.a: $(LIB_OBJECTS) Makefile
	$(RM) $@
	$(AR) rcs $@ $(LIB_OBJECTS)

$(BIN_DIR):
	mkdir -p $@

$(BIN_DIR)/ziran: cmd/package.zi cmd/package_add.zi cmd/package_guide.zi \
    cmd/package_capabilities.zi cmd/package_features.zi cmd/package_explain.zi cmd/package_common.zi \
    cmd/package_manifest.zi cmd/package_lock.zi cmd/package_map.zi \
    cmd/package_main.c cmd/package_host.c std/byte_text_linux.zi \
    std/file_linux.zi std/process_capture_linux.zi std/json_scan.zi \
    std/text.zi $(BIN_DIR)/zi2c
	$(BIN_DIR)/zi2c --no-main --root cmd --module-path std \
	    -o $(BUILD_DIR)/package-c cmd/package.zi
	$(CC) $(CFLAGS) -I$(BUILD_DIR)/package-c -o $@ \
	    cmd/package_main.c cmd/package_host.c $(BUILD_DIR)/package-c/*.c \
	    -lcrypto -lm

$(BIN_DIR)/zi-fmt: scripts/zi-fmt.sh | $(BIN_DIR)
	cp $< $@
	chmod +x $@

$(BIN_DIR)/zi2zir: cmd/zir-ir/main.c cmd/zir/zir_bundle.c $(FRONTEND) $(HEADERS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ cmd/zir-ir/main.c cmd/zir/zir_bundle.c $(FRONTEND)

$(BIN_DIR)/zi-api: cmd/zir-api/main.c $(FRONTEND) $(HEADERS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ cmd/zir-api/main.c $(FRONTEND)

$(BIN_DIR)/zi-inspect: cmd/zir-inspect/main.c $(FRONTEND) $(HEADERS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ cmd/zir-inspect/main.c $(FRONTEND)

$(BIN_DIR)/zi2c: $(wildcard cmd/zir-c/*.c) cmd/zir/zir_bundle.c $(FRONTEND) $(HEADERS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ cmd/zir-c/main.c cmd/zir-c/zir_c_lower.c \
	    cmd/zir-c/zir_c_plan9.c cmd/zir/zir_bundle.c $(FRONTEND)

$(BIN_DIR)/zi2go: cmd/zir-go/main.c cmd/zir-go/zir_go_lower.c cmd/zir/zir_bundle.c $(FRONTEND) $(HEADERS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ cmd/zir-go/main.c cmd/zir-go/zir_go_lower.c cmd/zir/zir_bundle.c $(FRONTEND)

$(BIN_DIR)/zi2cpp: cmd/zir-cpp/main.c cmd/zir-cpp/zir_cpp_lower.c cmd/zir/zir_bundle.c $(FRONTEND) $(HEADERS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ cmd/zir-cpp/main.c cmd/zir-cpp/zir_cpp_lower.c cmd/zir/zir_bundle.c $(FRONTEND)

$(BIN_DIR)/zi2rust: cmd/zir-rust/main.c cmd/zir-rust/zir_rust_lower.c cmd/zir/zir_bundle.c $(FRONTEND) $(HEADERS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ cmd/zir-rust/main.c \
	    cmd/zir-rust/zir_rust_lower.c cmd/zir/zir_bundle.c $(FRONTEND) -pthread

$(BIN_DIR)/zi2zib: cmd/zir-zib/main.c $(BUILD_DIR)/libziran.a $(HEADERS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ cmd/zir-zib/main.c $(BUILD_DIR)/libziran.a

$(BIN_DIR)/bundle-link-test: tests/bundle_link_test.c $(FRONTEND) $(PORTABLE) $(HEADERS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ tests/bundle_link_test.c $(FRONTEND) $(PORTABLE)

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
