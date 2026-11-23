ASSEMBLY_C := $(BUILD_DIR)/compiler-assembly
ASSEMBLY_MACHINE_SOURCES := cmd/compiler_vector_form.zi cmd/compiler_vectorize.zi cmd/compiler_vector_x86.zi \
     cmd/compiler_target.zi cmd/compiler_machine_image.zi cmd/compiler_abi.zi cmd/compiler_machine_ir.zi \
    cmd/compiler_machine_verify.zi cmd/compiler_machine_aggregate.zi cmd/compiler_machine_fields.zi cmd/compiler_machine_select.zi cmd/compiler_machine_lower.zi \
    cmd/compiler_machine_verify_allocated.zi cmd/compiler_regalloc.zi cmd/compiler_x86_bytes.zi cmd/compiler_x86_64.zi cmd/compiler_x86_runtime.zi \
    cmd/compiler_x86_win64.zi cmd/compiler_x86_host.zi cmd/compiler_x86_image.zi \
    cmd/compiler_encode.zi cmd/compiler_aarch64.zi cmd/compiler_aarch64_runtime.zi cmd/compiler_aarch64_elf.zi cmd/compiler_armv7.zi cmd/compiler_armv7_runtime.zi cmd/compiler_armv7_elf.zi \
    cmd/compiler_arm_bytes.zi \
    cmd/compiler_machine_elf.zi cmd/compiler_machine_pe.zi cmd/compiler_machine_pipeline.zi cmd/compiler_native_cache.zi
ASSEMBLY_SOURCES := std/math.zi cmd/compiler_bundle.zi cmd/compiler_lowered_ir.zi cmd/compiler_lowered_verify.zi cmd/compiler_lowered_proven.zi cmd/compiler_zir_lower.zi cmd/compiler_zx_run.zi cmd/compiler_exec_serial.zi cmd/compiler_exec_debug.zi cmd/compiler_exec_link.zi cmd/compiler_exec.zi cmd/compiler_opt.zi cmd/compiler_opt_ssa.zi cmd/compiler_opt_rotate.zi cmd/compiler_opt_loop.zi cmd/compiler_opt_licm.zi cmd/compiler_opt_induction.zi cmd/compiler_opt_layout.zi cmd/compiler_opt_inline.zi cmd/compiler_opt_specialize.zi cmd/compiler_opt_thread.zi cmd/compiler_opt_accumulate.zi cmd/compiler_opt_dom.zi cmd/compiler_learning.zi cmd/compiler_learning_policy.zi cmd/compiler_learning_table.zi $(ASSEMBLY_MACHINE_SOURCES)
ASSEMBLY_SOURCES += std/finite.zi

ifneq ($(BOOTSTRAP),1)
ifeq ($(NATIVE_HOST_ENABLED),1)
# Only these objects read the generated assembly headers. A target-specific
# CFLAGS would also reach the prerequisites make builds for it, zi2c among
# them, where the set's own math.h and vec.h would shadow the real ones;
# `private` prevents that but needs GNU Make 3.82, and macOS ships 3.81.
$(foreach source,cmd/zir/zir_host_native.c cmd/zir/zir_host_native_armv7.c,\
    $(eval $(call obj,$(source))_CFLAGS += -I$(ASSEMBLY_C)))
$(call obj,cmd/zir/zir_host_native.c cmd/zir/zir_host_native_armv7.c): $(ASSEMBLY_C)/.generated
endif
endif

.PHONY: zx-tools
zx-tools: $(BIN_DIR)/zi2zx $(BIN_DIR)/zx2wasm $(BIN_DIR)/zx2native

check: zx-tools

# The generator runs whenever its sources or zi2c change, but only files
# whose text changed are replaced, and .generated moves only then: a zi2c
# relinked for a new checkout path (a prebuilt tree) recompiles nothing.
$(ASSEMBLY_C)/.ran: assembly.mk $(ASSEMBLY_SOURCES) $(GEN_ZI2C)
	rm -rf $(BUILD_DIR)/compiler-assembly.next
	env -u DISPLAY -u WAYLAND_DISPLAY $(GEN_ZI2C) --no-main --root cmd -o $(BUILD_DIR)/compiler-assembly.next \
	    cmd/compiler_lowered_verify.zi cmd/compiler_zir_lower.zi cmd/compiler_exec_serial.zi cmd/compiler_exec_link.zi cmd/compiler_exec.zi cmd/compiler_opt.zi \
	    cmd/compiler_machine_pipeline.zi cmd/compiler_native_cache.zi cmd/compiler_zx_run.zi
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
ASSEMBLY_NATIVE_MODULES := compiler_vector_form compiler_vectorize compiler_vector_x86 compiler_opt_ssa \
     compiler_abi compiler_machine_image \
    compiler_machine_ir compiler_machine_verify \
    compiler_machine_aggregate compiler_machine_fields compiler_machine_select compiler_machine_lower compiler_machine_verify_allocated compiler_regalloc \
    compiler_target compiler_learning compiler_learning_policy compiler_x86_bytes compiler_x86_64 compiler_x86_runtime compiler_encode compiler_aarch64 compiler_aarch64_runtime compiler_aarch64_elf compiler_armv7 compiler_armv7_runtime compiler_armv7_elf \
    compiler_x86_win64 compiler_x86_host compiler_x86_image \
    compiler_arm_bytes \
    compiler_machine_elf compiler_machine_pe \
    compiler_machine_pipeline \
    compiler_native_cache

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

# zx2native's driver is Ziran (cmd/zx2native.zi); the generated copy uses the
# assembly set's own headers and links its unprefixed entry points.
ZX2NATIVE_C := $(BUILD_DIR)/zx2native-c
$(ZX2NATIVE_C)/.generated: cmd/zx2native.zi cmd/compiler_cli.zi cmd/compiler_ir.zi \
    $(BUILD_DIR)/compiler-assembly/.generated $(BIN_DIR)/zi2c
	$(BIN_DIR)/zi2c --entry zx2native:main --root cmd --module-path std -o $(ZX2NATIVE_C) cmd/zx2native.zi
	touch $@
$(ZX2NATIVE_C)/%.c: $(ZX2NATIVE_C)/.generated
	@test -f $@

$(BIN_DIR)/zx2native: $(ZX2NATIVE_C)/zx2native.c \
    $(BUILD_DIR)/obj/assembly-core.o $(BUILD_DIR)/libziran.a | $(BIN_DIR)
	$(NICE) $(CC) $(CFLAGS) -I$(ZX2NATIVE_C) -I$(BUILD_DIR)/compiler-assembly -o $@ \
	    $(ZX2NATIVE_C)/zx2native.c $(BUILD_DIR)/obj/assembly-core.o $(BUILD_DIR)/libziran.a $(RUNTIME_LIBS)
	chmod 755 $@
