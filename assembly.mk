ASSEMBLY_C := $(BUILD_DIR)/compiler-assembly
ASSEMBLY_MACHINE_SOURCES := cmd/compiler_target.zi cmd/compiler_machine_image.zi cmd/compiler_abi.zi cmd/compiler_machine_ir.zi \
    cmd/compiler_machine_verify.zi cmd/compiler_machine_lower.zi cmd/compiler_machine_verify_allocated.zi \
    cmd/compiler_regalloc.zi cmd/compiler_x86_bytes.zi cmd/compiler_x86_64.zi \
    cmd/compiler_aarch64.zi \
    cmd/compiler_arm_bytes.zi \
    cmd/compiler_machine_elf.zi cmd/compiler_machine_pipeline.zi
ASSEMBLY_SOURCES := cmd/compiler_bundle.zi cmd/compiler_lowered_ir.zi cmd/compiler_lowered_verify.zi cmd/compiler_exec_serial.zi cmd/compiler_exec_debug.zi cmd/compiler_exec_link.zi cmd/compiler_exec.zi cmd/compiler_opt.zi $(ASSEMBLY_MACHINE_SOURCES)

ifneq ($(BOOTSTRAP),1)
ifeq ($(NATIVE_HOST_ENABLED),1)
# Only this object reads the generated assembly headers. A target-specific
# CFLAGS would also reach the prerequisites make builds for it, zi2c among
# them, where the set's own math.h and vec.h would shadow the real ones;
# `private` prevents that but needs GNU Make 3.82, and macOS ships 3.81.
$(call obj,cmd/zir/zir_host_native.c)_CFLAGS += -I$(ASSEMBLY_C)
$(call obj,cmd/zir/zir_host_native.c): $(ASSEMBLY_C)/.generated
endif
endif

.PHONY: zx-tools
zx-tools: $(BIN_DIR)/zi2zx $(BIN_DIR)/zx2wasm $(BIN_DIR)/zx2native

check: zx-tools

# The generator runs whenever its sources or zi2c change, but only files
# whose text changed are replaced, and .generated moves only then: a zi2c
# relinked for a new checkout path (a prebuilt tree) recompiles nothing.
$(ASSEMBLY_C)/.ran: assembly.mk $(ASSEMBLY_SOURCES) $(BIN_DIR)/zi2c
	rm -rf $(BUILD_DIR)/compiler-assembly.next
	env -u DISPLAY -u WAYLAND_DISPLAY $(BIN_DIR)/zi2c --no-main --root cmd -o $(BUILD_DIR)/compiler-assembly.next \
	    cmd/compiler_lowered_verify.zi cmd/compiler_exec_serial.zi cmd/compiler_exec_link.zi cmd/compiler_exec.zi cmd/compiler_opt.zi \
	    cmd/compiler_machine_pipeline.zi
	@mkdir -p $(ASSEMBLY_C); changed=; \
	for f in $(BUILD_DIR)/compiler-assembly.next/*; do \
	    cmp -s "$$f" "$(ASSEMBLY_C)/$${f##*/}" || { cp "$$f" $(ASSEMBLY_C)/; changed=1; }; done; \
	for f in $(ASSEMBLY_C)/*; do \
	    [ -e "$(BUILD_DIR)/compiler-assembly.next/$${f##*/}" ] || { rm -f "$$f"; changed=1; }; done; \
	if [ -n "$$changed" ] || [ ! -e $(ASSEMBLY_C)/.generated ]; then touch $(ASSEMBLY_C)/.generated; fi
	rm -rf $(BUILD_DIR)/compiler-assembly.next
	touch $@

$(ASSEMBLY_C)/.generated: $(ASSEMBLY_C)/.ran
	@test -f $@ || touch $@

$(BUILD_DIR)/obj/assembly-core.o: assembly.mk $(ASSEMBLY_C)/.generated $(BUILD_DIR)/.compiler-flags
	@mkdir -p $(BUILD_DIR)/obj/assembly
	@set -eu; objects=; for source in $(ASSEMBLY_C)/*.c; do \
	    object=$(BUILD_DIR)/obj/assembly/$$(basename "$$source" .c).o; \
	    $(CC) $(CFLAGS) $(FRAMEFLAGS) -I$(ASSEMBLY_C) -c -o "$$object" "$$source"; \
	    objects="$$objects $$object"; \
	done; $(CC) -r -nostdlib -o $@ $$objects

# The embedding library already carries the source compiler and the verified-ZX
# engine, including its own compiler_bundle symbols. Keep the native machine
# pipeline in a separate relocatable object so linking libziran cannot pull in a
# second copy of those public symbols from assembly-core.o.
ASSEMBLY_NATIVE_MODULES := compiler_abi compiler_machine_image \
    compiler_machine_ir compiler_machine_verify \
    compiler_machine_lower compiler_machine_verify_allocated compiler_regalloc \
    compiler_target compiler_x86_bytes compiler_x86_64 compiler_aarch64 \
    compiler_arm_bytes \
    compiler_machine_elf \
    compiler_machine_pipeline

$(BUILD_DIR)/obj/native-core.o: assembly.mk $(ASSEMBLY_C)/.generated $(ENGINE_NAMES) $(BUILD_DIR)/.compiler-flags
	@mkdir -p $(BUILD_DIR)/obj/native-assembly
	@set -eu; objects=; for module in $(ASSEMBLY_NATIVE_MODULES); do \
	    source=$(ASSEMBLY_C)/$$module.c; \
	    object=$(BUILD_DIR)/obj/native-assembly/$$module.o; \
	    test -f "$$source"; \
	    $(CC) $(CFLAGS) $(FRAMEFLAGS) -fvisibility=hidden -I$(ASSEMBLY_C) \
	        -include $(ENGINE_NAMES) -c -o "$$object" "$$source"; \
	    objects="$$objects $$object"; \
	done; $(CC) -r -nostdlib -o $@ $$objects

$(call obj,cmd/zx2native/main.c cmd/zi2zx/zir_asm_lower.c cmd/zi2zx/zx_exec_run.c cmd/zi2zx/main.c): $(BUILD_DIR)/obj/%.o: %.c $(ASSEMBLY_C)/.generated $(BUILD_DIR)/.compiler-flags $(IR_READY)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(FRAMEFLAGS) $(DEPFLAGS) -I$(ASSEMBLY_C) -Icmd/zi2zx -Icmd/zx2wasm -c -o $@ $<

# Native output is a separate verified-ZX consumer. The source-side lowerer is
# linked only until the remaining unreachable source helpers move to zi2zx.
$(BIN_DIR)/zx2native: $(call obj,cmd/zx2native/main.c cmd/zi2zx/zir_asm_lower.c cmd/zi2zx/zx_exec_run.c) \
    $(BUILD_DIR)/obj/assembly-core.o $(BUILD_DIR)/libziran.a | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(RUNTIME_LIBS)
	chmod 755 $@
