CC ?= cc
AR ?= ar
OBJCOPY ?= objcopy
CFLAGS ?= -O2
override CFLAGS += -D_GNU_SOURCE -std=c11 -Iinclude -Icmd/zir
# Kept names and texts (KeepName, KeepText) are shared and immutable, so
# writing through one, as copy_text(e->argument_name, ...) would, must not
# compile. GCC and Clang name the check differently.
QUALIFIER_CHECK := $(shell for f in -Werror=discarded-qualifiers \
    -Werror=incompatible-pointer-types-discards-qualifiers; do \
    echo 'int x;' | $(CC) -Werror $$f -x c -c -o /dev/null - 2>/dev/null && \
    { echo $$f; break; }; done)
override CFLAGS += $(QUALIFIER_CHECK)
# `make sanitize` sets this; it reaches every compile and link.
SANITIZE_FLAGS ?=
override CFLAGS += $(SANITIZE_FLAGS)
DEPFLAGS = -MMD -MP
# Compiler code keeps large buffers on the heap so the deepest nesting the
# language allows fits the default stack; a bigger frame is a build error.
FRAMEFLAGS = -Werror=frame-larger-than=16384

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
# Generated seeds break the cycle between the compiler and its Ziran
# modules. Normal tools always link freshly compiled .zi implementations.
HOST_CC ?= cc
BOOTSTRAP ?= 0
SCANNER_OBJECT := $(BUILD_DIR)/obj/compiler-scan.o
TEXT_OBJECT := $(BUILD_DIR)/obj/compiler-text.o
SOURCE_OBJECT := $(BUILD_DIR)/obj/compiler-source.o
DECLARATION_OBJECT := $(BUILD_DIR)/obj/compiler-declaration.o
ifeq ($(BOOTSTRAP),1)
SCANNER_C := bootstrap/compiler_scan
SCANNER_READY := $(SCANNER_C)/compiler_scan.c $(SCANNER_C)/compiler_scan.h
TEXT_C := bootstrap/compiler_text
TEXT_READY := $(TEXT_C)/compiler_text.c $(TEXT_C)/compiler_text.h
SOURCE_C := bootstrap/compiler_source
SOURCE_READY := $(SOURCE_C)/compiler_source.c $(SOURCE_C)/compiler_source.h
DECLARATION_C := bootstrap/compiler_declaration
DECLARATION_READY := $(DECLARATION_C)/compiler_declaration.c $(DECLARATION_C)/compiler_declaration.h
else
SCANNER_C := $(BUILD_DIR)/compiler-scan
SCANNER_READY := $(SCANNER_C)/.generated
TEXT_C := $(BUILD_DIR)/compiler-text
TEXT_READY := $(TEXT_C)/.generated
SOURCE_C := $(BUILD_DIR)/compiler-source
SOURCE_READY := $(SOURCE_C)/.generated
DECLARATION_C := $(BUILD_DIR)/compiler-declaration
DECLARATION_READY := $(DECLARATION_C)/.generated
endif
override CFLAGS += -I$(SCANNER_C) -I$(TEXT_C) -I$(SOURCE_C) -I$(DECLARATION_C)
FRONTEND := cmd/zir/zir.c cmd/zir/zir_enum.c cmd/zir/zir_text.c \
    cmd/zir/zir_token.c cmd/zir/zir_cleanup.c cmd/zir/zir_expr.c \
    cmd/zir/zir_borrow.c cmd/zir/zir_law.c cmd/zir/zir_proof.c \
    cmd/zir/zir_proof_kernel.c \
    cmd/zir/zir_serial.c cmd/zir/zir_load.c \
    cmd/zir/zir_packages.c \
    cmd/zir/zir_diagnostic.c
PORTABLE := cmd/zir/zir_bundle.c
HEADERS := $(wildcard cmd/zir/*.h) $(wildcard include/*.h)
LIB_SOURCES := $(FRONTEND) $(PORTABLE) cmd/zir/zir_host.c

# Every C file compiles once to $(BUILD_DIR)/obj/<path>.o; binaries link objects.
obj = $(patsubst %.c,$(BUILD_DIR)/obj/%.o,$(1))
LIB_OBJECTS := $(call obj,$(LIB_SOURCES)) $(SCANNER_OBJECT) $(TEXT_OBJECT) $(SOURCE_OBJECT) $(DECLARATION_OBJECT) $(BUILD_DIR)/obj/check.o \
    $(BUILD_DIR)/obj/parse.o $(BUILD_DIR)/obj/emit.o $(BUILD_DIR)/obj/vm.o
FRONTEND_OBJECTS := $(call obj,$(FRONTEND)) $(SCANNER_OBJECT) $(TEXT_OBJECT) $(SOURCE_OBJECT) $(DECLARATION_OBJECT) $(BUILD_DIR)/obj/check.o \
    $(BUILD_DIR)/obj/parse.o $(BUILD_DIR)/obj/emit.o
BUNDLE_OBJECT := $(call obj,cmd/zir/zir_bundle.c)
RUNTIME_OBJECTS := $(call obj,cmd/zir/zir_runtime.c) $(BUILD_DIR)/obj/runtime_headers.o

.PHONY: all check curl-http-test clean install-user package-objects package-link force-compiler-flags
.PHONY: sanitize fuzz
.PHONY: proof-model
LEAN ?= lean
CHECK_JOBS ?= 4
all: $(BIN_DIR)/ziran $(BIN_DIR)/zi-fmt $(BIN_DIR)/zi2zir $(BIN_DIR)/zi-api $(BIN_DIR)/zi-inspect $(BIN_DIR)/zi2c $(BIN_DIR)/zi2go $(BIN_DIR)/zi2cpp $(BIN_DIR)/zi2rust $(BIN_DIR)/zi2py $(BIN_DIR)/zi2zib $(BUILD_DIR)/libziran.a

proof-model:
	mkdir -p $(BUILD_DIR)/proofs
	$(CC) $(CFLAGS) -o $(BUILD_DIR)/proofs/kernel tests/proof_kernel_test.c cmd/zir/zir_proof_kernel.c
	env -u DISPLAY -u WAYLAND_DISPLAY $(BUILD_DIR)/proofs/kernel $(BUILD_DIR)/proofs/corpus.txt
	$(LEAN) -DwarningAsError=true -o $(BUILD_DIR)/proofs/Scalar.olean proofs/Scalar.lean
	LEAN_PATH=$(abspath $(BUILD_DIR)/proofs) $(LEAN) -DwarningAsError=true --run proofs/Oracle.lean $(BUILD_DIR)/proofs/corpus.txt

USER_BIN ?= $(HOME)/.local/bin
USER_SHARE ?= $(HOME)/.local/share/ziran/bootstrap
install-user: all
	mkdir -p $(USER_BIN) $(USER_SHARE)/build/bin
	cp $(BIN_DIR)/ziran $(BIN_DIR)/zi-fmt $(BIN_DIR)/zi2zir $(BIN_DIR)/zi-api \
	    $(BIN_DIR)/zi-inspect $(BIN_DIR)/zi2c $(BIN_DIR)/zi2go \
	    $(BIN_DIR)/zi2cpp $(BIN_DIR)/zi2rust $(BIN_DIR)/zi2py $(BIN_DIR)/zi2zib \
	    $(USER_SHARE)/build/bin/
	rm -rf $(USER_SHARE)/std $(USER_SHARE)/include $(USER_SHARE)/templates
	cp -r std include templates $(USER_SHARE)/
	$(RM) $(USER_SHARE)/build/bin/ziran_pkg.py $(USER_SHARE)/build/bin/ziran-add
	printf '%s\n' '#!/bin/sh' 'set -eu' \
		'exec "$(USER_SHARE)/build/bin/ziran" "$$@"' > $(USER_BIN)/ziran
	chmod 755 $(USER_BIN)/ziran

# A reused build directory must not mix sanitized and ordinary objects.
# Preserve the stamp's mtime unless the actual compiler configuration changes.
quote = '$(subst ','"'"',$(1))'
$(BUILD_DIR)/.compiler-flags: force-compiler-flags
	@mkdir -p $(BUILD_DIR)
	@printf '%s\n' $(call quote,$(CC)) $(call quote,$(CFLAGS)) \
		$(call quote,$(FRAMEFLAGS)) $(call quote,$(OBJCOPY)) > $@.tmp
	@if cmp -s $@.tmp $@; then rm $@.tmp; else mv $@.tmp $@; fi

force-compiler-flags:

$(BUILD_DIR)/obj/%.o: %.c $(BUILD_DIR)/.compiler-flags
	@mkdir -p $(dir $@)
	$(NICE) $(CC) $(CFLAGS) $(FRAMEFLAGS) $(DEPFLAGS) -c -o $@ $<

$(call obj,cmd/zir/zir_token.c): $(SCANNER_READY)
$(SCANNER_OBJECT): $(SCANNER_READY) $(BUILD_DIR)/.compiler-flags
	@mkdir -p $(dir $@)
	$(NICE) $(CC) $(CFLAGS) $(FRAMEFLAGS) $(DEPFLAGS) -c -o $@ $(SCANNER_C)/compiler_scan.c

$(call obj,cmd/zir/zir_text.c): $(TEXT_READY)
$(TEXT_OBJECT): $(TEXT_READY) $(BUILD_DIR)/.compiler-flags
	@mkdir -p $(dir $@)
	$(NICE) $(CC) $(CFLAGS) $(FRAMEFLAGS) $(DEPFLAGS) -c -o $@ $(TEXT_C)/compiler_text.c

$(call obj,cmd/zir/zir_parse.c cmd/zir/zir_parse_condition.c cmd/zir/zir_parse_declaration.c): $(SOURCE_READY)
$(SOURCE_OBJECT): $(SOURCE_READY) $(BUILD_DIR)/.compiler-flags
	@mkdir -p $(dir $@)
	$(NICE) $(CC) $(CFLAGS) $(FRAMEFLAGS) $(DEPFLAGS) -c -o $@ $(SOURCE_C)/compiler_source.c

$(call obj,cmd/zir/zir_parse_declaration.c): $(DECLARATION_READY)
$(DECLARATION_OBJECT): $(DECLARATION_READY) $(TEXT_READY) $(SOURCE_READY) $(BUILD_DIR)/.compiler-flags
	@mkdir -p $(dir $@)
	$(NICE) $(CC) $(CFLAGS) $(FRAMEFLAGS) $(DEPFLAGS) -c -o $@ $(DECLARATION_C)/compiler_declaration.c

$(BUILD_DIR)/obj/runtime_headers.o: $(BUILD_DIR)/runtime_headers.c $(BUILD_DIR)/.compiler-flags
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

# wasm objcopy cannot localize hidden symbols. Browser builds give each
# group's private helpers distinct names through generated prefix headers.
ifneq ($(WASM_PRIVATE_HEADERS),)
$(call obj,$(PARSE_PARTS)): CFLAGS += -include $(WASM_PRIVATE_HEADERS)/parse.h
$(call obj,$(PARSE_PARTS)): $(WASM_PRIVATE_HEADERS)/parse.h
$(call obj,$(CHECK_PARTS)): CFLAGS += -include $(WASM_PRIVATE_HEADERS)/check.h
$(call obj,$(CHECK_PARTS)): $(WASM_PRIVATE_HEADERS)/check.h
$(call obj,$(EMIT_PARTS)): CFLAGS += -include $(WASM_PRIVATE_HEADERS)/emit.h
$(call obj,$(EMIT_PARTS)): $(WASM_PRIVATE_HEADERS)/emit.h
$(call obj,$(VM_PARTS)): CFLAGS += -include $(WASM_PRIVATE_HEADERS)/vm.h
$(call obj,$(VM_PARTS)): $(WASM_PRIVATE_HEADERS)/vm.h
endif

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
PACKAGE_SOURCES := cmd/package.zi cmd/package_entry.zi cmd/package_process.zi \
    cmd/package_add.zi cmd/package_guide.zi \
    cmd/package_capabilities.zi cmd/package_features.zi cmd/package_explain.zi cmd/package_common.zi \
    cmd/package_manifest.zi cmd/package_source.zi cmd/package_lock.zi cmd/package_map.zi cmd/package_options.zi cmd/package_compile_commands.zi cmd/package_template.zi \
    std/byte_text_linux.zi std/c_string.zi std/file_linux.zi \
    std/json_scan.zi std/text.zi std/vec.zi VERSION
# Generated files are listed when the ziran recipe expands, after generation.
PACKAGE_OBJECTS = $(patsubst $(PACKAGE_C)/%.c,$(BUILD_DIR)/obj/package-c/%.o,$(wildcard $(PACKAGE_C)/*.c))

# Regenerate beside the old output and copy over only files whose text
# changed, so a compiler edit that leaves ziran's C alone recompiles nothing.
$(PACKAGE_C)/.generated: $(PACKAGE_SOURCES) $(BIN_DIR)/zi2c
	rm -rf $(PACKAGE_C).next
	mkdir -p $(BUILD_DIR)/package-source
	printf 'Version :: "%s";\n' '$(ZIRAN_VERSION)' > $(BUILD_DIR)/package-source/version.zi
	$(BIN_DIR)/zi2c --no-main --root cmd --module-path std \
	    --module-path $(BUILD_DIR)/package-source -o $(PACKAGE_C).next cmd/package_entry.zi
	mkdir -p $(PACKAGE_C)
	for f in $(PACKAGE_C).next/*; do \
	    cmp -s "$$f" "$(PACKAGE_C)/$${f##*/}" || cp "$$f" $(PACKAGE_C)/; done
	for f in $(PACKAGE_C)/*; do \
	    [ -e "$(PACKAGE_C).next/$${f##*/}" ] || rm -f "$$f"; done
	rm -rf $(PACKAGE_C).next
	touch $@

$(BUILD_DIR)/obj/package-c/%.o: $(PACKAGE_C)/%.c $(BUILD_DIR)/.compiler-flags
	@mkdir -p $(dir $@)
	$(NICE) $(CC) $(CFLAGS) -I$(PACKAGE_C) $(DEPFLAGS) -c -o $@ $<

# Standalone commands fall back to this checkout's standard modules and C
# headers when none sit beside the installed compiler.
$(call obj,cmd/zir/zir_load.c): CFLAGS += -DZIRAN_STD_DIR='"$(abspath std)"' \
    -DZIRAN_INCLUDE_DIR='"$(abspath include)"'

# The native command imports a generated Ziran constant from VERSION.
ZIRAN_VERSION := $(shell cat VERSION)

package-objects: $(PACKAGE_OBJECTS)

# Make expands a whole recipe before running its first line, so the object
# list is only correct in a fresh make started after generation. That sub-make
# both compiles and links; linking in this recipe would miss the generated
# objects on a clean build.
package-link: $(PACKAGE_OBJECTS)
	$(CC) $(CFLAGS) -o $(BIN_DIR)/ziran $(PACKAGE_OBJECTS) -lcrypto -lm

$(BIN_DIR)/ziran: $(PACKAGE_C)/.generated $(HEADERS) | $(BIN_DIR)
	+$(MAKE) --no-print-directory package-link

FORMAT_C := $(BUILD_DIR)/format-c
FORMAT_SOURCES := cmd/format.zi cmd/format_source.zi std/vec.zi std/option.zi \
    std/byte_text_linux.zi std/c_string.zi std/file_linux.zi
$(BIN_DIR)/zi-fmt: $(FORMAT_SOURCES) $(BIN_DIR)/zi2c $(BUILD_DIR)/.compiler-flags | $(BIN_DIR)
	$(BIN_DIR)/zi2c --entry format:main --root cmd --module-path std \
	    -o $(FORMAT_C) cmd/format.zi
	$(NICE) $(CC) $(CFLAGS) -I$(FORMAT_C) -o $@ $(FORMAT_C)/*.c -lm

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
	$(CC) $(CFLAGS) -o $@ $^

$(BIN_DIR)/zi2py: $(call obj,cmd/zir-py/main.c cmd/zir-py/zir_py_lower.c cmd/zir-py/zir_py_runtime.c) $(BUNDLE_OBJECT) $(FRONTEND_OBJECTS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ -lm

$(BIN_DIR)/zi2zib: $(call obj,cmd/zir-zib/main.c) $(BUILD_DIR)/libziran.a | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^

ifneq ($(BOOTSTRAP),1)
BOOTSTRAP_BIN := $(BUILD_DIR)/bootstrap/bin/zi2c
$(BOOTSTRAP_BIN): $(FRONTEND) $(PARSE_PARTS) $(CHECK_PARTS) $(EMIT_PARTS) \
    $(PORTABLE) cmd/zir/zir_runtime.c cmd/zir-c/main.c cmd/zir-c/zir_c_lower.c \
    cmd/zir-c/zir_c_plan9.c $(HEADERS) $(RUNTIME_HEADERS) scripts/embed_headers.sh \
    bootstrap/compiler_scan/compiler_scan.c bootstrap/compiler_scan/compiler_scan.h \
    bootstrap/compiler_text/compiler_text.c bootstrap/compiler_text/compiler_text.h \
    bootstrap/compiler_source/compiler_source.c bootstrap/compiler_source/compiler_source.h \
    bootstrap/compiler_declaration/compiler_declaration.c bootstrap/compiler_declaration/compiler_declaration.h Makefile
	+$(MAKE) --no-print-directory BOOTSTRAP=1 BUILD_DIR=$(BUILD_DIR)/bootstrap \
	    CC=$(call quote,$(HOST_CC)) AR=ar OBJCOPY=objcopy CFLAGS=-O2 \
	    SANITIZE_FLAGS= WASM_PRIVATE_HEADERS= $(BOOTSTRAP_BIN)

$(SCANNER_C)/.generated: cmd/compiler_scan.zi $(BOOTSTRAP_BIN)
	env -u DISPLAY -u WAYLAND_DISPLAY $(BOOTSTRAP_BIN) --no-main --root cmd \
	    -o $(SCANNER_C) cmd/compiler_scan.zi
	touch $@

$(TEXT_C)/.generated: cmd/compiler_text.zi $(BOOTSTRAP_BIN)
	env -u DISPLAY -u WAYLAND_DISPLAY $(BOOTSTRAP_BIN) --no-main --root cmd \
	    -o $(TEXT_C) cmd/compiler_text.zi
	touch $@

$(SOURCE_C)/.generated: cmd/compiler_source.zi $(BOOTSTRAP_BIN)
	env -u DISPLAY -u WAYLAND_DISPLAY $(BOOTSTRAP_BIN) --no-main --root cmd \
	    -o $(SOURCE_C) cmd/compiler_source.zi
	touch $@

$(DECLARATION_C)/.generated: cmd/compiler_declaration.zi cmd/compiler_text.zi cmd/compiler_source.zi $(BOOTSTRAP_BIN)
	env -u DISPLAY -u WAYLAND_DISPLAY $(BOOTSTRAP_BIN) --no-main --root cmd \
	    -o $(DECLARATION_C) cmd/compiler_declaration.zi
	touch $@

.PHONY: check-bootstrap update-bootstrap
check-bootstrap: $(BIN_DIR)/zi2c
	env -u DISPLAY -u WAYLAND_DISPLAY $(BIN_DIR)/zi2c --no-main --root cmd \
	    -o $(BUILD_DIR)/bootstrap-check cmd/compiler_scan.zi
	cmp bootstrap/compiler_scan/compiler_scan.c $(BUILD_DIR)/bootstrap-check/compiler_scan.c
	cmp bootstrap/compiler_scan/compiler_scan.h $(BUILD_DIR)/bootstrap-check/compiler_scan.h
	env -u DISPLAY -u WAYLAND_DISPLAY $(BIN_DIR)/zi2c --no-main --root cmd \
	    -o $(BUILD_DIR)/bootstrap-check cmd/compiler_text.zi
	cmp bootstrap/compiler_text/compiler_text.c $(BUILD_DIR)/bootstrap-check/compiler_text.c
	cmp bootstrap/compiler_text/compiler_text.h $(BUILD_DIR)/bootstrap-check/compiler_text.h
	env -u DISPLAY -u WAYLAND_DISPLAY $(BIN_DIR)/zi2c --no-main --root cmd \
	    -o $(BUILD_DIR)/bootstrap-check cmd/compiler_source.zi
	cmp bootstrap/compiler_source/compiler_source.c $(BUILD_DIR)/bootstrap-check/compiler_source.c
	cmp bootstrap/compiler_source/compiler_source.h $(BUILD_DIR)/bootstrap-check/compiler_source.h
	env -u DISPLAY -u WAYLAND_DISPLAY $(BIN_DIR)/zi2c --no-main --root cmd \
	    -o $(BUILD_DIR)/bootstrap-check cmd/compiler_declaration.zi
	cmp bootstrap/compiler_declaration/compiler_declaration.c $(BUILD_DIR)/bootstrap-check/compiler_declaration.c
	cmp bootstrap/compiler_declaration/compiler_declaration.h $(BUILD_DIR)/bootstrap-check/compiler_declaration.h

update-bootstrap: $(BIN_DIR)/zi2c
	env -u DISPLAY -u WAYLAND_DISPLAY $(BIN_DIR)/zi2c --no-main --root cmd \
	    -o $(BUILD_DIR)/bootstrap-check cmd/compiler_scan.zi
	cp $(BUILD_DIR)/bootstrap-check/compiler_scan.c bootstrap/compiler_scan/
	cp $(BUILD_DIR)/bootstrap-check/compiler_scan.h bootstrap/compiler_scan/
	env -u DISPLAY -u WAYLAND_DISPLAY $(BIN_DIR)/zi2c --no-main --root cmd \
	    -o $(BUILD_DIR)/bootstrap-check cmd/compiler_text.zi
	cp $(BUILD_DIR)/bootstrap-check/compiler_text.c bootstrap/compiler_text/
	cp $(BUILD_DIR)/bootstrap-check/compiler_text.h bootstrap/compiler_text/
	env -u DISPLAY -u WAYLAND_DISPLAY $(BIN_DIR)/zi2c --no-main --root cmd \
	    -o $(BUILD_DIR)/bootstrap-check cmd/compiler_source.zi
	cp $(BUILD_DIR)/bootstrap-check/compiler_source.c bootstrap/compiler_source/
	cp $(BUILD_DIR)/bootstrap-check/compiler_source.h bootstrap/compiler_source/
	env -u DISPLAY -u WAYLAND_DISPLAY $(BIN_DIR)/zi2c --no-main --root cmd \
	    -o $(BUILD_DIR)/bootstrap-check cmd/compiler_declaration.zi
	cp $(BUILD_DIR)/bootstrap-check/compiler_declaration.c bootstrap/compiler_declaration/
	cp $(BUILD_DIR)/bootstrap-check/compiler_declaration.h bootstrap/compiler_declaration/
endif

$(BIN_DIR)/bundle-link-test: $(call obj,tests/bundle_link_test.c) $(FRONTEND_OBJECTS) $(call obj,$(PORTABLE)) \
    $(BUILD_DIR)/obj/vm.o | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^

$(BIN_DIR)/host-capability-test: tests/host_capability_test.c $(BUILD_DIR)/libziran.a | $(BIN_DIR)
	$(CC) $(SANITIZE_FLAGS) -D_GNU_SOURCE -std=c11 -Iinclude -o $@ tests/host_capability_test.c $(BUILD_DIR)/libziran.a

$(BIN_DIR)/record-host-test: tests/record_host_test.c $(BUILD_DIR)/libziran.a | $(BIN_DIR)
	$(CC) $(SANITIZE_FLAGS) -D_GNU_SOURCE -std=c11 -Iinclude -o $@ tests/record_host_test.c $(BUILD_DIR)/libziran.a

$(BIN_DIR)/array-host-test: tests/host_array_test.c $(BUILD_DIR)/libziran.a | $(BIN_DIR)
	$(CC) $(SANITIZE_FLAGS) -D_GNU_SOURCE -std=c11 -Iinclude -o $@ tests/host_array_test.c $(BUILD_DIR)/libziran.a

$(BIN_DIR)/startup-graph-test: tests/startup_graph_test.c $(BUILD_DIR)/libziran.a | $(BIN_DIR)
	$(CC) $(SANITIZE_FLAGS) -D_GNU_SOURCE -std=c11 -Iinclude -Icmd/zir -o $@ \
		tests/startup_graph_test.c $(BUILD_DIR)/libziran.a

$(BIN_DIR)/vm-vec-scope-test: tests/vm_vec_scope_test.c $(BUILD_DIR)/libziran.a | $(BIN_DIR)
	$(CC) $(SANITIZE_FLAGS) -D_GNU_SOURCE -std=c11 -Iinclude -Icmd/zir -o $@ \
		tests/vm_vec_scope_test.c $(BUILD_DIR)/libziran.a

$(BIN_DIR)/process-host-test: tests/process_host_test.c $(BUILD_DIR)/libziran.a | $(BIN_DIR)
	$(CC) $(SANITIZE_FLAGS) -D_GNU_SOURCE -std=c11 -Iinclude -o $@ tests/process_host_test.c $(BUILD_DIR)/libziran.a

$(BIN_DIR)/slice-host-test: tests/host_slice_test.c $(BUILD_DIR)/libziran.a | $(BIN_DIR)
	$(CC) $(SANITIZE_FLAGS) -D_GNU_SOURCE -std=c11 -Iinclude -o $@ tests/host_slice_test.c $(BUILD_DIR)/libziran.a

check: all $(BIN_DIR)/bundle-link-test $(BIN_DIR)/host-capability-test $(BIN_DIR)/record-host-test $(BIN_DIR)/array-host-test $(BIN_DIR)/slice-host-test $(BIN_DIR)/process-host-test $(BIN_DIR)/startup-graph-test $(BIN_DIR)/vm-vec-scope-test
	env -u DISPLAY -u WAYLAND_DISPLAY $(BIN_DIR)/startup-graph-test
	python3 tests/run_check.py --bin-dir $(BIN_DIR) --jobs $(CHECK_JOBS)

# Address and undefined-behavior sanitizers over the whole toolchain and
# test suite. It also checks every kept type lookup against a fresh one
# (ZIRAN_VERIFY_TYPE_LOOKUPS), so a change that forgets TypeLookupsChanged
# fails here. Instrumentation grows stack frames past the frame limit, so
# the limit is off for this build. The compiler frees little on exit by
# design, so leak reports are off. Tests that link libziran.a add
# VM_CFLAGS to their compile.
SANITIZE_DIR ?= build/sanitize
SANITIZE_OPTIONS = -g -fsanitize=address,undefined \
    -fno-sanitize-recover=undefined -fno-omit-frame-pointer
SANITIZE_ENV = ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=print_stacktrace=1 \
    ZIRAN_VERIFY_TYPE_LOOKUPS=1 VM_CFLAGS="$(SANITIZE_OPTIONS)"
SANITIZE_MAKE = $(MAKE) --no-print-directory BUILD_DIR=$(SANITIZE_DIR) \
    FRAMEFLAGS= CFLAGS=-O1 SANITIZE_FLAGS="$(SANITIZE_OPTIONS)"
sanitize:
	+$(SANITIZE_ENV) $(SANITIZE_MAKE) check

# Mutates example and standard-library sources and feeds them to the
# sanitized compiler; new crashes land in $(SANITIZE_DIR)/fuzz.
FUZZ_SECONDS ?= 60
fuzz:
	+$(SANITIZE_MAKE) $(SANITIZE_DIR)/bin/zi2zir
	$(SANITIZE_ENV) python3 scripts/fuzz.py --compiler $(SANITIZE_DIR)/bin/zi2zir \
	    --seconds $(FUZZ_SECONDS) --out $(SANITIZE_DIR)/fuzz

curl-http-test: $(BIN_DIR)/zi2c
	python3 tests/net_http_curl_linux_test.py $(BIN_DIR)/zi2c

clean:
	rm -rf $(BUILD_DIR)
