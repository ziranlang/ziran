ASSEMBLY_C := $(BUILD_DIR)/compiler-assembly
ASSEMBLY_MACHINE_SOURCES := cmd/compiler_target.zi cmd/compiler_abi.zi cmd/compiler_machine_ir.zi \
    cmd/compiler_machine_verify.zi cmd/compiler_machine_lower.zi cmd/compiler_machine_verify_allocated.zi \
    cmd/compiler_regalloc.zi cmd/compiler_x86_bytes.zi cmd/compiler_x86_64.zi \
    cmd/compiler_machine_elf.zi cmd/compiler_machine_pipeline.zi
ASSEMBLY_SOURCES := cmd/compiler_bundle.zi cmd/compiler_lowered_ir.zi cmd/compiler_lowered_verify.zi cmd/compiler_lowered_x64.zi cmd/compiler_exec_serial.zi cmd/compiler_exec_link.zi cmd/compiler_exec.zi cmd/compiler_opt.zi $(ASSEMBLY_MACHINE_SOURCES)

.PHONY: assembly
assembly: $(BIN_DIR)/zi2asm

check: $(BIN_DIR)/zi2asm

$(ASSEMBLY_C)/.generated: assembly.mk $(ASSEMBLY_SOURCES) $(BIN_DIR)/zi2c
	@mkdir -p $(ASSEMBLY_C)
	env -u DISPLAY -u WAYLAND_DISPLAY $(BIN_DIR)/zi2c --no-main --root cmd -o $(ASSEMBLY_C) \
	    cmd/compiler_lowered_verify.zi cmd/compiler_lowered_x64.zi cmd/compiler_exec_serial.zi cmd/compiler_exec_link.zi cmd/compiler_exec.zi cmd/compiler_opt.zi \
	    cmd/compiler_machine_pipeline.zi
	touch $@

$(BUILD_DIR)/obj/assembly-core.o: $(ASSEMBLY_C)/.generated $(BUILD_DIR)/.compiler-flags
	@mkdir -p $(BUILD_DIR)/obj/assembly
	@set -eu; objects=; for source in $(ASSEMBLY_C)/*.c; do \
	    object=$(BUILD_DIR)/obj/assembly/$$(basename "$$source" .c).o; \
	    $(CC) $(CFLAGS) $(FRAMEFLAGS) -I$(ASSEMBLY_C) -c -o "$$object" "$$source"; \
	    objects="$$objects $$object"; \
	done; $(CC) -r -nostdlib -o $@ $$objects

$(call obj,cmd/zir-asm/main.c cmd/zir-asm/zir_asm_lower.c cmd/zir-asm/zx_exec_run.c cmd/zir-zx/main.c): $(BUILD_DIR)/obj/%.o: %.c $(ASSEMBLY_C)/.generated $(BUILD_DIR)/.compiler-flags $(IR_READY)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(FRAMEFLAGS) $(DEPFLAGS) -I$(ASSEMBLY_C) -Icmd/zir-asm -Icmd/zir-wasm -c -o $@ $<

# Wasm is one more lowering of verified revision-28 ZX (zi2asm --emit wasm).
$(call obj,cmd/zir-wasm/zx_wasm_lower.c): $(BUILD_DIR)/obj/%.o: %.c $(ASSEMBLY_C)/.generated $(BUILD_DIR)/.compiler-flags $(IR_READY)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(FRAMEFLAGS) $(DEPFLAGS) -I$(ASSEMBLY_C) -Icmd/zir-wasm -c -o $@ $<

$(BIN_DIR)/zi2asm: $(call obj,cmd/zir-asm/main.c cmd/zir-asm/zir_asm_lower.c cmd/zir-asm/zx_exec_run.c cmd/zir-wasm/zx_wasm_lower.c) \
    $(BUILD_DIR)/obj/assembly-core.o $(BUILD_DIR)/libziran.a | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(RUNTIME_LIBS)
