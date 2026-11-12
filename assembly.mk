ASSEMBLY_C := $(BUILD_DIR)/compiler-assembly
ASSEMBLY_SOURCES := cmd/compiler_lowered_ir.zi cmd/compiler_lowered_verify.zi cmd/compiler_lowered_x64.zi cmd/compiler_exec_serial.zi cmd/compiler_exec.zi cmd/compiler_opt.zi

.PHONY: assembly
assembly: $(BIN_DIR)/zi2asm

check: $(BIN_DIR)/zi2asm

$(ASSEMBLY_C)/.generated: assembly.mk $(ASSEMBLY_SOURCES) $(BIN_DIR)/zi2c
	@mkdir -p $(ASSEMBLY_C)
	env -u DISPLAY -u WAYLAND_DISPLAY $(BIN_DIR)/zi2c --no-main --root cmd -o $(ASSEMBLY_C) \
	    cmd/compiler_lowered_verify.zi cmd/compiler_lowered_x64.zi cmd/compiler_exec_serial.zi cmd/compiler_exec.zi cmd/compiler_opt.zi
	touch $@

$(BUILD_DIR)/obj/assembly-core.o: $(ASSEMBLY_C)/.generated $(BUILD_DIR)/.compiler-flags
	@mkdir -p $(BUILD_DIR)/obj/assembly
	@set -eu; objects=; for source in $(ASSEMBLY_C)/*.c; do \
	    object=$(BUILD_DIR)/obj/assembly/$$(basename "$$source" .c).o; \
	    $(CC) $(CFLAGS) $(FRAMEFLAGS) -I$(ASSEMBLY_C) -c -o "$$object" "$$source"; \
	    objects="$$objects $$object"; \
	done; $(CC) -r -nostdlib -o $@ $$objects

$(call obj,cmd/zir-asm/main.c cmd/zir-asm/zir_asm_lower.c): $(BUILD_DIR)/obj/%.o: %.c $(ASSEMBLY_C)/.generated $(BUILD_DIR)/.compiler-flags $(IR_READY)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(FRAMEFLAGS) $(DEPFLAGS) -I$(ASSEMBLY_C) -c -o $@ $<

$(BIN_DIR)/zi2asm: $(call obj,cmd/zir-asm/main.c cmd/zir-asm/zir_asm_lower.c) \
    $(BUILD_DIR)/obj/assembly-core.o $(BUILD_DIR)/libziran.a | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(RUNTIME_LIBS)
