# Native ZX engine: libziran runs verified bundles as native code, lowered
# to C and compiled in memory by the vendored TinyCC (third_party/tinycc,
# LGPL-2.1). Hosts without a TinyCC code generator keep the portable VM.
NATIVE_ZX_MACHINE := $(if $(filter wasm%,$(TARGET_MACHINE)),,$(if $(DARWIN_TARGET),,$(if $(findstring mingw,$(TARGET_MACHINE)),,$(filter x86_64% arm% aarch64%,$(TARGET_MACHINE)))))
NATIVE_ZX ?= $(if $(NATIVE_ZX_MACHINE),1,0)

ifeq ($(NATIVE_ZX),1)
NATIVE_ZX_HEADERS := $(wildcard cmd/zir/zx_native_include/*.h) third_party/tinycc/include/tccdefs.h
NATIVE_ZX_OBJECTS := $(call obj,cmd/zir/zir_zx_native.c cmd/zir/zir_zx_host.c cmd/zir/zir_sha256.c \
    cmd/zir-c/zir_c_lower.c cmd/zir-c/zir_c_build.c cmd/zir-c/zir_c_cache.c) \
    $(RUNTIME_OBJECTS) $(BUILD_DIR)/obj/zx_native_headers.o $(BUILD_DIR)/obj/tinycc.o
LIB_OBJECTS += $(NATIVE_ZX_OBJECTS)
$(BUILD_DIR)/libziran.a: $(NATIVE_ZX_OBJECTS) native-zx.mk
$(call obj,cmd/zir/zir_host.c): CFLAGS += -DZIRAN_NATIVE_ZX
$(call obj,cmd/zir/zir_host.c): native-zx.mk
$(call obj,cmd/zir/zir_zx_native.c): CFLAGS += -Ithird_party/tinycc

# Compiled bundles are cached under a key that includes this identity of
# every source that shapes generated code, so a changed compiler never
# reuses an old object.
NATIVE_ZX_IDENTITY_SOURCES := $(sort $(wildcard cmd/*.zi cmd/zir/*.c cmd/zir/*.h \
    cmd/zir-c/*.c cmd/zir-c/*.h bootstrap/*/*.c bootstrap/*/*.h include/*.h \
    cmd/zir/zx_native_include/*.h cmd/zir/tinycc/*.h third_party/tinycc/*.c \
    third_party/tinycc/*.h third_party/tinycc/include/*.h)) native-zx.mk
NATIVE_ZX_IDENTITY := $(shell cat $(NATIVE_ZX_IDENTITY_SOURCES) | \
    { sha256sum 2>/dev/null || shasum -a 256; } | cut -c1-32)
$(call obj,cmd/zir/zir_zx_native.c): CFLAGS += -DZX_NATIVE_IDENTITY=\"$(NATIVE_ZX_IDENTITY)\"
$(call obj,cmd/zir/zir_zx_native.c): $(BUILD_DIR)/.zx-native-identity
$(BUILD_DIR)/.zx-native-identity: force-compiler-flags
	@mkdir -p $(dir $@)
	@echo '$(NATIVE_ZX_IDENTITY)' > $@.tmp
	@if cmp -s $@.tmp $@; then rm $@.tmp; else mv $@.tmp $@; fi

$(BUILD_DIR)/zx_native_headers.c: scripts/embed_headers.sh $(NATIVE_ZX_HEADERS) native-zx.mk
	mkdir -p $(dir $@)
	sh scripts/embed_headers.sh $(NATIVE_ZX_HEADERS) | \
	    sed -e 's/"zir_runtime.h"/"zir_zx_native.h"/' \
	        -e 's/const ZirRuntimeHeader ZirRuntimeHeaders/const ZxNativeHeader ZxNativeHeaders/' > $@

$(BUILD_DIR)/obj/zx_native_headers.o: $(BUILD_DIR)/zx_native_headers.c $(BUILD_DIR)/.compiler-flags
	@mkdir -p $(dir $@)
	$(NICE) $(CC) $(CFLAGS) $(DEPFLAGS) -c -o $@ $<

# TinyCC builds as one translation unit with its own warnings, without
# Ziran's -Werror checks or sanitizer instrumentation.
$(BUILD_DIR)/obj/tinycc.o: $(wildcard third_party/tinycc/*.c third_party/tinycc/*.h) \
    cmd/zir/tinycc/config.h native-zx.mk
	@mkdir -p $(dir $@)
	$(NICE) $(CC) $(filter-out -Werror% -std=% -D_GNU_SOURCE -fsanitize%,$(CFLAGS)) -w -DONE_SOURCE=1 \
	    -Icmd/zir/tinycc -Ithird_party/tinycc -c -o $@ third_party/tinycc/libtcc.c
endif

# The portable VM stays the engine where TinyCC has no code generator (the
# browser, Windows, macOS). Tests of the VM itself run it everywhere through
# zi2zx-vm and libziran-vm.a, which differ only in running instances on it.
ZIRAN_VM_HOST_OBJECT := $(BUILD_DIR)/obj/zir_host_vm.o
ZIRAN_VM_LIB_OBJECTS := $(filter-out $(call obj,cmd/zir/zir_host.c) $(NATIVE_ZX_OBJECTS),$(LIB_OBJECTS)) \
    $(ZIRAN_VM_HOST_OBJECT)
$(ZIRAN_VM_HOST_OBJECT): cmd/zir/zir_host.c $(BUILD_DIR)/.compiler-flags native-zx.mk
	@mkdir -p $(dir $@)
	$(NICE) $(CC) $(CFLAGS) $(FRAMEFLAGS) $(DEPFLAGS) -c -o $@ $<
$(BUILD_DIR)/libziran-vm.a: $(ZIRAN_VM_LIB_OBJECTS) Makefile native-zx.mk
	$(RM) $@
	$(AR) rcs $@ $(ZIRAN_VM_LIB_OBJECTS)
$(BIN_DIR)/zi2zx-vm: $(call obj,cmd/zir-zx/main.c) $(BUILD_DIR)/libziran-vm.a | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(RUNTIME_LIBS)
all: $(BIN_DIR)/zi2zx-vm $(BUILD_DIR)/libziran-vm.a
