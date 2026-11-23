CC ?= cc
AR ?= ar
# Plain `make` builds the toolchain. Rules for single objects come earlier
# in this file than `all`, so the goal is named rather than left to order.
.DEFAULT_GOAL := all
TARGET_MACHINE := $(shell $(CC) -dumpmachine 2>/dev/null)
DARWIN_TARGET := $(findstring darwin,$(TARGET_MACHINE))
# Merged groups localize their shared helpers. Apple's ld -r already turns
# hidden symbols into local ones; other object formats need objcopy.
OBJCOPY ?= $(if $(DARWIN_TARGET),true,objcopy)
# The shell's lookup skips a directory named cmake on PATH, as in the Emscripten
# tree; make's own lookup may try to run that directory.
CMAKE ?= $(or $(shell command -v cmake),cmake)
# Ziran platform conditions are fixed when its sources compile, so the
# tools written in Ziran name the Darwin host explicitly.
ZIRAN_DEFINES ?= $(if $(DARWIN_TARGET),--define __APPLE__ --define $(if $(findstring x86_64,$(TARGET_MACHINE)),__x86_64__,__aarch64__))
# macOS supplies SHA-256 and random bytes in libSystem.
PACKAGE_LIBS ?= $(if $(DARWIN_TARGET),,-lcrypto) -lm
# Linux before glibc 2.34 keeps POSIX threads and the dynamic loader in
# separate libraries. The portable runtime uses both.
# Android keeps __android_log_print, which diagnostics use, in liblog, and
# its NDK links no GNU build ID unless asked; the native code cache keys by it.
# (A variable, because the comma in -Wl, would end the $(if).)
ANDROID_LINK_FLAGS := -llog -Wl,--build-id=sha1
RUNTIME_LIBS ?= -lm $(if $(findstring linux,$(TARGET_MACHINE)),-pthread -ldl) $(if $(findstring android,$(TARGET_MACHINE)),$(ANDROID_LINK_FLAGS))
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
# The zi2c that generates C for the package and assembly modules. A cross
# build (CC for another target) points it at a host build's zi2c.
GEN_ZI2C ?= $(BIN_DIR)/zi2c
HOST_OBJCOPY ?= $(if $(findstring darwin,$(shell $(HOST_CC) -dumpmachine 2>/dev/null)),true,objcopy)
# The seed only emits the compiler modules' C once. Optimize the final tools
# with CFLAGS; spending optimizer time on this short-lived seed delays them.
BOOTSTRAP_CFLAGS ?= -O0
BOOTSTRAP ?= 0
SCANNER_OBJECT := $(BUILD_DIR)/obj/compiler-scan.o
TEXT_OBJECT := $(BUILD_DIR)/obj/compiler-text.o
SOURCE_OBJECT := $(BUILD_DIR)/obj/compiler-source.o
DECLARATION_OBJECT := $(BUILD_DIR)/obj/compiler-declaration.o
ENUM_OBJECT := $(BUILD_DIR)/obj/compiler-enum.o
TYPE_OBJECT := $(BUILD_DIR)/obj/compiler-type.o
EXPRESSION_OBJECT := $(BUILD_DIR)/obj/compiler-expression.o
PARSE_OBJECT := $(BUILD_DIR)/obj/compiler-parse.o
STATEMENT_OBJECT := $(BUILD_DIR)/obj/compiler-statement.o
PROOF_KERNEL_OBJECT := $(BUILD_DIR)/obj/compiler-proof-kernel.o
PROOF_OBJECT := $(BUILD_DIR)/obj/compiler-proof.o
IR_OBJECT := $(BUILD_DIR)/obj/compiler-ir.o
LAW_OBJECT := $(BUILD_DIR)/obj/compiler-law.o
SERVICES_OBJECT := $(BUILD_DIR)/obj/compiler-services.o
LINK_OBJECT := $(BUILD_DIR)/obj/compiler-link.o
BUNDLE_MODULE_OBJECT := $(BUILD_DIR)/obj/compiler-bundle.o
ifeq ($(BOOTSTRAP),1)
SCANNER_C := bootstrap/compiler_scan
SCANNER_READY := $(SCANNER_C)/compiler_scan.c $(SCANNER_C)/compiler_scan.h
TEXT_C := bootstrap/compiler_text
TEXT_READY := $(TEXT_C)/compiler_text.c $(TEXT_C)/compiler_text.h
SOURCE_C := bootstrap/compiler_source
SOURCE_READY := $(SOURCE_C)/compiler_source.c $(SOURCE_C)/compiler_source.h
DECLARATION_C := bootstrap/compiler_declaration
DECLARATION_READY := $(DECLARATION_C)/compiler_declaration.c $(DECLARATION_C)/compiler_declaration.h
ENUM_C := bootstrap/compiler_enum
ENUM_READY := $(ENUM_C)/compiler_enum.c $(ENUM_C)/compiler_enum.h
TYPE_C := bootstrap/compiler_type
TYPE_READY := $(TYPE_C)/compiler_type.c $(TYPE_C)/compiler_type.h
EXPRESSION_C := bootstrap/compiler_expression
EXPRESSION_READY := $(EXPRESSION_C)/compiler_expression.c $(EXPRESSION_C)/compiler_expression.h
PARSE_C := bootstrap/compiler_parse
PARSE_READY := $(PARSE_C)/compiler_parse.c $(PARSE_C)/compiler_parse.h
STATEMENT_C := bootstrap/compiler_statement
STATEMENT_READY := $(STATEMENT_C)/compiler_statement.c $(STATEMENT_C)/compiler_statement.h
PROOF_KERNEL_C := bootstrap/compiler_proof_kernel
PROOF_KERNEL_READY := $(PROOF_KERNEL_C)/compiler_proof_kernel.c $(PROOF_KERNEL_C)/compiler_proof_kernel.h $(PROOF_KERNEL_C)/vec.h
PROOF_C := bootstrap/compiler_proof
PROOF_READY := $(PROOF_C)/compiler_proof.c $(PROOF_C)/compiler_proof.h $(PROOF_C)/vec.h
BUNDLE_C := bootstrap/compiler_bundle
BUNDLE_READY := $(BUNDLE_C)/compiler_bundle.c $(BUNDLE_C)/compiler_bundle.h
IR_C := bootstrap/compiler_ir
IR_READY := $(IR_C)/compiler_ir.c $(IR_C)/compiler_ir.h
LAW_C := bootstrap/compiler_law
LAW_READY := $(LAW_C)/compiler_law.c $(LAW_C)/compiler_law.h
SERVICES_C := bootstrap/compiler_services
SERVICES_READY := $(SERVICES_C)/compiler_services.c $(SERVICES_C)/compiler_services.h
LINK_C := bootstrap/compiler_link
LINK_READY := $(LINK_C)/compiler_link.c $(LINK_C)/compiler_link.h $(LINK_C)/vec.h
else
SCANNER_C := $(BUILD_DIR)/compiler-scan
SCANNER_READY := $(SCANNER_C)/.generated
TEXT_C := $(BUILD_DIR)/compiler-text
TEXT_READY := $(TEXT_C)/.generated
SOURCE_C := $(BUILD_DIR)/compiler-source
SOURCE_READY := $(SOURCE_C)/.generated
DECLARATION_C := $(BUILD_DIR)/compiler-declaration
DECLARATION_READY := $(DECLARATION_C)/.generated
ENUM_C := $(BUILD_DIR)/compiler-enum
ENUM_READY := $(ENUM_C)/.generated
TYPE_C := $(BUILD_DIR)/compiler-type
TYPE_READY := $(TYPE_C)/.generated
EXPRESSION_C := $(BUILD_DIR)/compiler-expression
EXPRESSION_READY := $(EXPRESSION_C)/.generated
PARSE_C := $(BUILD_DIR)/compiler-parse
PARSE_READY := $(PARSE_C)/.generated
STATEMENT_C := $(BUILD_DIR)/compiler-statement
STATEMENT_READY := $(STATEMENT_C)/.generated
PROOF_KERNEL_C := $(BUILD_DIR)/compiler-proof-kernel
PROOF_KERNEL_READY := $(PROOF_KERNEL_C)/.generated
PROOF_C := $(BUILD_DIR)/compiler-proof
PROOF_READY := $(PROOF_C)/.generated
BUNDLE_C := $(BUILD_DIR)/compiler-bundle
BUNDLE_READY := $(BUNDLE_C)/.generated
IR_C := $(BUILD_DIR)/compiler-ir
IR_READY := $(IR_C)/.generated
LAW_C := $(BUILD_DIR)/compiler-law
LAW_READY := $(LAW_C)/.generated
SERVICES_C := $(BUILD_DIR)/compiler-services
SERVICES_READY := $(SERVICES_C)/.generated
LINK_C := $(BUILD_DIR)/compiler-link
LINK_READY := $(LINK_C)/.generated
endif
# The seed's file, process and timer modules are generated per platform:
# Darwin's copies come from the same sources with the Darwin defines.
BOOTSTRAP_PLATFORM := $(if $(DARWIN_TARGET),bootstrap/darwin,bootstrap)
ifeq ($(BOOTSTRAP),1)
RUNTIME_EMIT_C := $(BOOTSTRAP_PLATFORM)/compiler_runtime_emit
RUNTIME_EMIT_READY := $(RUNTIME_EMIT_C)/compiler_runtime_emit.c $(RUNTIME_EMIT_C)/compiler_runtime_emit.h \
    $(RUNTIME_EMIT_C)/file.c $(RUNTIME_EMIT_C)/file.h $(RUNTIME_EMIT_C)/file_linux.c $(RUNTIME_EMIT_C)/file_linux.h \
    $(RUNTIME_EMIT_C)/c_string.c $(RUNTIME_EMIT_C)/c_string.h $(RUNTIME_EMIT_C)/result.h \
    $(RUNTIME_EMIT_C)/vec.c $(RUNTIME_EMIT_C)/vec.h
else
RUNTIME_EMIT_C := $(BUILD_DIR)/runtime-emit
RUNTIME_EMIT_READY := $(RUNTIME_EMIT_C)/.generated
endif
ifneq ($(BOOTSTRAP),1)
$(RUNTIME_EMIT_C)/%.c: $(RUNTIME_EMIT_C)/.generated
	@test -f $@
endif
# A generated module directory keeps copies of the headers it imports, so
# each module's own directory comes before every directory that imports it:
# a stale copy of compiler_ir.h in compiler-proof must never shadow IR_C.
override CFLAGS += -I$(IR_C) -I$(SCANNER_C) -I$(TEXT_C) -I$(SOURCE_C) -I$(DECLARATION_C) -I$(ENUM_C) -I$(TYPE_C) -I$(EXPRESSION_C) -I$(STATEMENT_C) -I$(PROOF_KERNEL_C) -I$(PROOF_C) -I$(BUNDLE_C) -I$(LAW_C) -I$(SERVICES_C) -I$(LINK_C)
override CFLAGS += -I$(RUNTIME_EMIT_C)
ifeq ($(BOOTSTRAP),1)
BACKEND_CACHE_C := $(BOOTSTRAP_PLATFORM)/compiler_backend_cache
BACKEND_PROCESS_C := $(BOOTSTRAP_PLATFORM)/compiler_backend_process
BACKEND_CACHE_READY := $(filter %.c %.h,$(wildcard $(BACKEND_CACHE_C)/*))
BACKEND_PROCESS_READY := $(filter %.c %.h,$(wildcard $(BACKEND_PROCESS_C)/*))
else
BACKEND_CACHE_C := $(BUILD_DIR)/backend-cache
BACKEND_PROCESS_C := $(BUILD_DIR)/backend-process
BACKEND_CACHE_READY := $(BACKEND_CACHE_C)/.generated
BACKEND_PROCESS_READY := $(BACKEND_PROCESS_C)/.generated
$(BACKEND_CACHE_C)/%.c $(BACKEND_CACHE_C)/%.h: $(BACKEND_CACHE_C)/.generated
	@test -f $@
$(BACKEND_PROCESS_C)/%.c $(BACKEND_PROCESS_C)/%.h: $(BACKEND_PROCESS_C)/.generated
	@test -f $@
endif
BACKEND_PROCESS_FILES := $(filter %.c %.h,$(wildcard $(BACKEND_PROCESS_C)/*))
BACKEND_BOOTSTRAP_FILES := $(filter %.c %.h,$(wildcard $(BOOTSTRAP_PLATFORM)/compiler_backend_cache/*)) \
    $(filter %.c %.h,$(wildcard $(BOOTSTRAP_PLATFORM)/compiler_backend_process/*)) \
    $(filter %.c %.h,$(wildcard $(BOOTSTRAP_PLATFORM)/compiler_runtime_emit/*))
ifeq ($(BOOTSTRAP),1)
NATIVE_NAMES_C := bootstrap/compiler_native_names
NATIVE_NAMES_READY := $(NATIVE_NAMES_C)/compiler_native_names.c $(NATIVE_NAMES_C)/compiler_native_names.h
else
NATIVE_NAMES_C := $(BUILD_DIR)/native-names
NATIVE_NAMES_READY := $(NATIVE_NAMES_C)/.generated
endif
NATIVE_NAMES_OBJECT := $(BUILD_DIR)/obj/native-names/compiler_native_names.o
ifneq ($(BOOTSTRAP),1)
$(NATIVE_NAMES_C)/%.c: $(NATIVE_NAMES_C)/.generated
	@test -f $@
endif
override CFLAGS += -I$(BACKEND_CACHE_C) -I$(BACKEND_PROCESS_C)
override CFLAGS += -I$(NATIVE_NAMES_C)
# Eligibility rules shared by the C and Go typed-body emitters.
ifeq ($(BOOTSTRAP),1)
BODY_C := bootstrap/compiler_portable_body
BODY_READY := $(BODY_C)/compiler_portable_body.c $(BODY_C)/compiler_portable_body.h
else
BODY_C := $(BUILD_DIR)/compiler-portable-body
BODY_READY := $(BODY_C)/.generated
endif
BODY_OBJECT := $(BUILD_DIR)/obj/compiler-portable-body.o
override CFLAGS += -I$(BODY_C)
ifeq ($(BOOTSTRAP),1)
SCALAR_C := bootstrap/compiler_scalar
SCALAR_READY := $(SCALAR_C)/compiler_scalar.c $(SCALAR_C)/compiler_scalar.h
C_EMIT_C := bootstrap/compiler_c_emit
C_EMIT_READY := $(C_EMIT_C)/compiler_c_emit.c $(C_EMIT_C)/compiler_c_emit.h
C_EMIT_VALUE_C := bootstrap/compiler_c_emit_value
C_EMIT_VALUE_READY := $(C_EMIT_VALUE_C)/compiler_c_emit_value.c $(C_EMIT_VALUE_C)/compiler_c_emit_value.h
C_EMIT_STATEMENT_C := bootstrap/compiler_c_emit_statement
C_EMIT_STATEMENT_READY := $(C_EMIT_STATEMENT_C)/compiler_c_emit_statement.c $(C_EMIT_STATEMENT_C)/compiler_c_emit_statement.h
C_EMIT_CALL_C := bootstrap/compiler_c_emit_call
C_EMIT_CALL_READY := $(C_EMIT_CALL_C)/compiler_c_emit_call.c $(C_EMIT_CALL_C)/compiler_c_emit_call.h
C_EMIT_EXPR_C := bootstrap/compiler_c_emit_expr
C_EMIT_EXPR_READY := $(C_EMIT_EXPR_C)/compiler_c_emit_expr.c $(C_EMIT_EXPR_C)/compiler_c_emit_expr.h
SIMD_METADATA_C := bootstrap/compiler_simd_metadata
SIMD_METADATA_READY := $(SIMD_METADATA_C)/compiler_simd_metadata.c $(SIMD_METADATA_C)/compiler_simd_metadata.h
C_EMIT_SIMD_C := bootstrap/compiler_c_emit_simd
C_EMIT_SIMD_READY := $(C_EMIT_SIMD_C)/compiler_c_emit_simd.c $(C_EMIT_SIMD_C)/compiler_c_emit_simd.h
GO_SIMD_C := bootstrap/compiler_go_simd
GO_SIMD_READY := $(GO_SIMD_C)/compiler_go_simd.c $(GO_SIMD_C)/compiler_go_simd.h
C_BACKEND_C := bootstrap/compiler_c_backend
C_BACKEND_READY := $(C_BACKEND_C)/compiler_c_backend.c $(C_BACKEND_C)/compiler_c_backend.h
else
SCALAR_C := $(BUILD_DIR)/compiler-scalar
SCALAR_READY := $(SCALAR_C)/.generated
C_EMIT_C := $(BUILD_DIR)/compiler-c-emit
C_EMIT_READY := $(C_EMIT_C)/.generated
C_EMIT_VALUE_C := $(BUILD_DIR)/compiler-c-emit-value
C_EMIT_VALUE_READY := $(C_EMIT_VALUE_C)/.generated
C_EMIT_STATEMENT_C := $(BUILD_DIR)/compiler-c-emit-statement
C_EMIT_STATEMENT_READY := $(C_EMIT_STATEMENT_C)/.generated
C_EMIT_CALL_C := $(BUILD_DIR)/compiler-c-emit-call
C_EMIT_CALL_READY := $(C_EMIT_CALL_C)/.generated
C_EMIT_EXPR_C := $(BUILD_DIR)/compiler-c-emit-expr
C_EMIT_EXPR_READY := $(C_EMIT_EXPR_C)/.generated
SIMD_METADATA_C := $(BUILD_DIR)/compiler-simd-metadata
SIMD_METADATA_READY := $(SIMD_METADATA_C)/.generated
C_EMIT_SIMD_C := $(BUILD_DIR)/compiler-c-emit-simd
C_EMIT_SIMD_READY := $(C_EMIT_SIMD_C)/.generated
GO_SIMD_C := $(BUILD_DIR)/compiler-go-simd
GO_SIMD_READY := $(GO_SIMD_C)/.generated
C_BACKEND_C := $(BUILD_DIR)/compiler-c-backend
C_BACKEND_READY := $(C_BACKEND_C)/.generated
endif
SCALAR_OBJECT := $(BUILD_DIR)/obj/compiler-scalar.o
C_EMIT_OBJECT := $(BUILD_DIR)/obj/compiler-c-emit.o
C_EMIT_VALUE_OBJECT := $(BUILD_DIR)/obj/compiler-c-emit-value.o
C_EMIT_STATEMENT_OBJECT := $(BUILD_DIR)/obj/compiler-c-emit-statement.o
C_EMIT_CALL_OBJECT := $(BUILD_DIR)/obj/compiler-c-emit-call.o
C_EMIT_EXPR_OBJECT := $(BUILD_DIR)/obj/compiler-c-emit-expr.o
SIMD_METADATA_OBJECT := $(BUILD_DIR)/obj/compiler-simd-metadata.o
C_EMIT_SIMD_OBJECT := $(BUILD_DIR)/obj/compiler-c-emit-simd.o
GO_SIMD_OBJECT := $(BUILD_DIR)/obj/compiler-go-simd.o
C_BACKEND_OBJECT := $(BUILD_DIR)/obj/compiler-c-backend.o
override CFLAGS += -I$(SCALAR_C) -I$(C_EMIT_C) -I$(C_EMIT_VALUE_C) -I$(C_EMIT_STATEMENT_C) -I$(C_EMIT_CALL_C) -I$(C_EMIT_EXPR_C) -I$(SIMD_METADATA_C) -I$(C_EMIT_SIMD_C) -I$(GO_SIMD_C) -I$(C_BACKEND_C)

GO_PORT_MODULES := go_backend go_asm go_asm_x64
CHECK_CORE_MODULES := check check_value check_statement check_expr check_text
CHECK_PROGRAM_MODULES := check_function check_program check_link check_simd borrow
CHECK_PORT_MODULES := check_types $(CHECK_CORE_MODULES) check_s12_types $(CHECK_PROGRAM_MODULES)
S14_PARSE_MODULES := parse_declaration parse_eval parse_typed ir_cleanup
PARSE_PORT_MODULES := parse_types parse_condition parse_discover parse_source \
    frontend_parse parse_s14_types $(S14_PARSE_MODULES)
SUPPORT_PORT_MODULES := context document diagnostic files source_store overlay semantic sha256
PERSISTENCE_PORT_MODULES := zir_serial load package_resolve zx_assets
FRONTEND_PORT_MODULES := context_types $(SUPPORT_PORT_MODULES) frontend_ir law_runtime target_bridge $(PERSISTENCE_PORT_MODULES) zx_link inline_asm
compiler-port-dir = $(if $(filter 1,$(BOOTSTRAP)),bootstrap/compiler_$(1),$(BUILD_DIR)/compiler-$(subst _,-,$(1)))
compiler-port-seed = $(addprefix bootstrap/compiler_$(1)/compiler_$(1).,c h)
compiler-port-ready = $(if $(filter 1,$(BOOTSTRAP)),$(call compiler-port-seed,$(1)),$(call compiler-port-dir,$(1))/.generated)
compiler-port-object = $(BUILD_DIR)/obj/compiler-$(subst _,-,$(1)).o
GO_PORT_OBJECTS := $(foreach module,$(GO_PORT_MODULES),$(call compiler-port-object,$(module)))
CHECK_PORT_OBJECTS := $(foreach module,$(CHECK_PORT_MODULES),$(call compiler-port-object,$(module)))
PARSE_PORT_OBJECTS := $(foreach module,$(PARSE_PORT_MODULES),$(call compiler-port-object,$(module)))
FRONTEND_PORT_OBJECTS := $(foreach module,$(FRONTEND_PORT_MODULES),$(call compiler-port-object,$(module)))
override CFLAGS += $(foreach module,$(GO_PORT_MODULES) $(CHECK_PORT_MODULES) $(PARSE_PORT_MODULES) $(FRONTEND_PORT_MODULES),-I$(call compiler-port-dir,$(module)))
# zi2c's pruning of a closed program's C (cmd/compiler_c_prune.zi). Its std
# text_tools and vec code is already linked through the backend modules.
ifeq ($(BOOTSTRAP),1)
C_PRUNE_C := bootstrap/compiler_c_prune
C_PRUNE_READY := $(filter %.c %.h,$(wildcard $(C_PRUNE_C)/*))
else
C_PRUNE_C := $(BUILD_DIR)/c-prune
C_PRUNE_READY := $(C_PRUNE_C)/.generated
$(C_PRUNE_C)/%.c $(C_PRUNE_C)/%.h: $(C_PRUNE_C)/.generated
	@test -f $@
endif
C_PRUNE_OBJECTS := $(addprefix $(BUILD_DIR)/obj/c-prune/,compiler_c_prune.o hash_map.o)
# The C emitter's index range proofs (cmd/compiler_c_range*.zi).
ifeq ($(BOOTSTRAP),1)
RANGE_C := bootstrap/compiler_c_range
RANGE_READY := $(filter %.c %.h,$(wildcard $(RANGE_C)/*))
else
RANGE_C := $(BUILD_DIR)/c-range
RANGE_READY := $(RANGE_C)/.generated
$(RANGE_C)/%.c $(RANGE_C)/%.h: $(RANGE_C)/.generated
	@test -f $@
endif
RANGE_MODULES := compiler_c_range compiler_c_range_facts compiler_c_range_values compiler_c_range_poly \
    compiler_c_range_callers compiler_c_range_fill
RANGE_OBJECTS := $(patsubst %,$(BUILD_DIR)/obj/c-range/%.o,$(RANGE_MODULES))
override CFLAGS += -I$(RANGE_C)
FRONTEND := cmd/zir/zir_memory.c cmd/zir/zir_profile.c cmd/zir/zir_services.c \
    cmd/zir/zir_atomics.c cmd/zir/zir_diagnostic_host.c \
    cmd/zir/zir_files_host.c cmd/zir/zir_sha256_host.c
PERSISTENCE_HOST := cmd/zir/zir_persistence_host.c
HEADERS := $(wildcard cmd/zir/*.h) $(wildcard include/*.h)
# std/simd's C helpers, vector types and typed Go helpers are generated at
# build time by scripts/simd/header.zi, compiled from its seed in
# bootstrap/simd; update-bootstrap refreshes the seed, check-bootstrap checks it.
SIMD_DIR ?= $(BUILD_DIR)/simd
SIMD_HEADERS := $(SIMD_DIR)/ziran_simd_ops.h $(SIMD_DIR)/ziran_simd_types.h $(SIMD_DIR)/zir_simd_go_bodies.h
HEADERS += $(SIMD_HEADERS)
override CFLAGS += -I$(SIMD_DIR)
LIB_SOURCES := $(FRONTEND) cmd/zir/zir_native_memory.c cmd/zir/zir_zx_module.c

# Every C file compiles once to $(BUILD_DIR)/obj/<path>.o; binaries link objects.
obj = $(patsubst %.c,$(BUILD_DIR)/obj/%.o,$(1))
# The portable instruction engine (cmd/compiler_runtime.zi), behind the
# embedding API in cmd/zir/zir_host_exec.c. Every name the engine's modules
# define gets the ziran_engine_ prefix (ENGINE_NAMES, force-included in them
# and in the API), so a program with its own copies of the same standard
# modules (math, vec, ...) links beside it on ELF, COFF and wasm alike. Its
# headers are found with -iquote only: the set has its own math.h. The
# bootstrap compiler opens no bundle and has none.
ENGINE_C := $(BUILD_DIR)/compiler-runtime
ENGINE_MODULES := binary math utf8 vec compiler_lowered_ir compiler_lowered_verify \
    compiler_lowered_proven compiler_exec_serial compiler_exec_debug compiler_exec_link compiler_exec compiler_opt compiler_opt_ssa compiler_opt_rotate compiler_opt_loop compiler_opt_licm compiler_opt_induction compiler_opt_layout compiler_opt_inline compiler_opt_specialize compiler_opt_thread compiler_opt_accumulate compiler_opt_dom compiler_learning compiler_learning_policy \
    text_buffer number_text compiler_zx_wasm_types compiler_zx_wasm_fn compiler_zx_wasm_plan compiler_zx_wasm_code \
    compiler_zx_wasm_body compiler_zx_wasm_helpers compiler_zx_wasm
# The Wasm lowering of revision-28 ZX (cmd/compiler_zx_wasm*.zi) is part of
# the engine; zir_zx_wasm.c and zx2wasm call it under the engine's names.
# The native loader's per-architecture veneers are zir_host_native_<arch>.c.
HOST_EXEC_SOURCES := cmd/zir/zir_host_exec.c cmd/zir/zir_host_native.c cmd/zir/zir_host_native_x86_64.c \
    cmd/zir/zir_host_native_aarch64.c cmd/zir/zir_host_native_armv7.c cmd/zir/zir_zx_wasm.c
ENGINE_NAMES := $(ENGINE_C)/engine_names.h
ifeq ($(BOOTSTRAP),1)
ENGINE_OBJECTS :=
$(call obj,$(HOST_EXEC_SOURCES)): CFLAGS += -DZIRAN_NO_ENGINE
else
ENGINE_OBJECTS := $(patsubst %,$(BUILD_DIR)/obj/engine/%.o,$(ENGINE_MODULES))
# Per-object flags, not a target-specific CFLAGS: that would also reach the
# prerequisites make builds for these objects (zi2c, through zir_host_native's
# generated assembly headers), and engine_names.h does not exist yet there.
$(foreach object,$(call obj,$(HOST_EXEC_SOURCES)),$(eval $(object)_CFLAGS += -iquote $(ENGINE_C) -include $(ENGINE_NAMES)))
$(call obj,$(HOST_EXEC_SOURCES)): $(ENGINE_NAMES)
$(call obj,cmd/zx2wasm/main.c)_CFLAGS += -iquote $(ENGINE_C) -include $(ENGINE_NAMES)
$(call obj,cmd/zx2wasm/main.c): $(ENGINE_NAMES)
endif
LIB_OBJECTS := $(call obj,$(LIB_SOURCES) $(HOST_EXEC_SOURCES)) $(ENGINE_OBJECTS) $(call compiler-port-object,host_api) $(BUNDLE_MODULE_OBJECT) $(SCANNER_OBJECT) $(TEXT_OBJECT) $(SOURCE_OBJECT) $(DECLARATION_OBJECT) $(ENUM_OBJECT) $(TYPE_OBJECT) $(EXPRESSION_OBJECT) $(STATEMENT_OBJECT) $(PROOF_KERNEL_OBJECT) $(PROOF_OBJECT) $(IR_OBJECT) $(LAW_OBJECT) $(SERVICES_OBJECT) $(LINK_OBJECT) $(CHECK_PORT_OBJECTS) \
    $(BUILD_DIR)/obj/parse.o $(RANGE_OBJECTS) $(NATIVE_NAMES_OBJECT) \
    $(call obj,cmd/zir/zir_zx_wasm_web.c)
NATIVE_HOST_ENABLED := 1
ifneq ($(findstring emcc,$(notdir $(firstword $(CC)))),)
NATIVE_HOST_ENABLED := 0
endif
ifneq ($(BOOTSTRAP),1)
ifeq ($(NATIVE_HOST_ENABLED),1)
LIB_OBJECTS += $(BUILD_DIR)/obj/native-core.o
endif
endif
FRONTEND_OBJECTS := $(call obj,$(FRONTEND)) $(SCANNER_OBJECT) $(TEXT_OBJECT) $(SOURCE_OBJECT) $(DECLARATION_OBJECT) $(ENUM_OBJECT) $(TYPE_OBJECT) $(EXPRESSION_OBJECT) $(STATEMENT_OBJECT) $(PROOF_KERNEL_OBJECT) $(PROOF_OBJECT) $(IR_OBJECT) $(LAW_OBJECT) $(SERVICES_OBJECT) $(LINK_OBJECT) $(CHECK_PORT_OBJECTS) \
    $(BUILD_DIR)/obj/parse.o $(RANGE_OBJECTS) $(NATIVE_NAMES_OBJECT)
# The expression parser, cmd/compiler_parse.zi.
override CFLAGS += -I$(PARSE_C)
LIB_OBJECTS += $(PARSE_OBJECT) $(BODY_OBJECT) $(SCALAR_OBJECT) $(C_EMIT_OBJECT) $(C_EMIT_VALUE_OBJECT) $(C_EMIT_STATEMENT_OBJECT) $(C_EMIT_CALL_OBJECT) $(C_EMIT_EXPR_OBJECT) $(SIMD_METADATA_OBJECT) $(C_EMIT_SIMD_OBJECT) $(GO_SIMD_OBJECT)
FRONTEND_OBJECTS += $(PARSE_OBJECT) $(BODY_OBJECT) $(SCALAR_OBJECT) $(C_EMIT_OBJECT) $(C_EMIT_VALUE_OBJECT) $(C_EMIT_STATEMENT_OBJECT) $(C_EMIT_CALL_OBJECT) $(C_EMIT_EXPR_OBJECT) $(SIMD_METADATA_OBJECT) $(C_EMIT_SIMD_OBJECT) $(GO_SIMD_OBJECT)
BUNDLE_OBJECT := $(BUNDLE_MODULE_OBJECT)
RUNTIME_EMIT_OBJECTS := $(addprefix $(BUILD_DIR)/obj/runtime-emit/,compiler_runtime_emit.o file.o file_linux.o c_string.o vec.o)
BACKEND_PROCESS_SOURCES := compiler_backend_process.c byte_text_linux.c environment_linux.c \
    file_tree_linux.c number_text.c process_run_linux.c sort.c text_buffer.c text_tools.c timer_linux.c
BACKEND_PROCESS_OBJECTS := $(addprefix $(BUILD_DIR)/obj/backend-process/,$(addsuffix .o,$(basename $(BACKEND_PROCESS_SOURCES))))
BACKEND_CACHE_OBJECT := $(BUILD_DIR)/obj/backend-cache/compiler_backend_cache.o
$(BACKEND_PROCESS_OBJECTS): FRAMEFLAGS := -Wframe-larger-than=1048576
RUNTIME_OBJECTS := $(RUNTIME_EMIT_OBJECTS) $(BACKEND_CACHE_OBJECT) $(BACKEND_PROCESS_OBJECTS) $(BUILD_DIR)/obj/runtime_headers.o

.PHONY: all check curl-http-test clean install-user package-objects package-link force-compiler-flags
.PHONY: sanitize fuzz
.PHONY: proof-model
LEAN ?= lean
# make check runs CHECK_JOBS checks at once (default: the build queue's
# BUILD_QUEUE_JOBS, else every CPU); CHECK_FILTER runs only checks whose name
# contains it. Every ordinary run executes the complete retained suite.
CHECK_JOBS ?=
CHECK_FILTER ?=
all: $(BIN_DIR)/zi-edit $(BIN_DIR)/ziran $(BIN_DIR)/zi-fmt $(BIN_DIR)/zi2zir $(BIN_DIR)/zi-api $(BIN_DIR)/zi-inspect $(BIN_DIR)/zi2c $(BIN_DIR)/zi2go $(BIN_DIR)/zi2zx $(BIN_DIR)/zx2wasm $(BIN_DIR)/zx2native $(BUILD_DIR)/libziran.a

proof-model: $(PROOF_KERNEL_OBJECT)
	mkdir -p $(BUILD_DIR)/proofs
	$(CC) $(CFLAGS) -o $(BUILD_DIR)/proofs/kernel tests/compiler_proof_kernel_test.c $(PROOF_KERNEL_OBJECT)
	env -u DISPLAY -u WAYLAND_DISPLAY $(BUILD_DIR)/proofs/kernel $(BUILD_DIR)/proofs/corpus.txt
	$(LEAN) -DwarningAsError=true -o $(BUILD_DIR)/proofs/Scalar.olean proofs/Scalar.lean
	LEAN_PATH=$(abspath $(BUILD_DIR)/proofs) $(LEAN) -DwarningAsError=true --run proofs/Oracle.lean $(BUILD_DIR)/proofs/corpus.txt

USER_BIN ?= $(HOME)/.local/bin
USER_SHARE ?= $(HOME)/.local/share/ziran/bootstrap
install-user: all
	mkdir -p $(USER_BIN) $(USER_SHARE)/build/bin
	@set -e; for tool in ziran zi-fmt zi-edit zi2zir zi-api zi-inspect zi2c zi2go zi2zx zx2wasm zx2native; do \
		temporary=$$(mktemp "$(USER_SHARE)/build/bin/.$$tool.XXXXXX"); \
		trap 'rm -f "$$temporary"' EXIT; \
		cp "$(BIN_DIR)/$$tool" "$$temporary"; chmod 755 "$$temporary"; \
		mv -f "$$temporary" "$(USER_SHARE)/build/bin/$$tool"; \
	done
	rm -rf $(USER_SHARE)/std $(USER_SHARE)/include $(USER_SHARE)/templates
	cp -r std include templates $(USER_SHARE)/
	@# Another tool's launcher, such as a build queue wrapper, stays in place.
	@set -e; for tool in ziran zi2zx; do \
		launcher="exec \"$(USER_SHARE)/build/bin/$$tool\" \"\$$@\""; \
		if test -e "$(USER_BIN)/$$tool" && ! grep -qxF "$$launcher" "$(USER_BIN)/$$tool"; then \
			echo "install-user: kept $(USER_BIN)/$$tool, which launches something else"; \
		else \
			printf '%s\n' '#!/bin/sh' 'set -eu' "$$launcher" > "$(USER_BIN)/$$tool"; \
			chmod 755 "$(USER_BIN)/$$tool"; \
		fi; \
	done

# A reused build directory must not mix sanitized and ordinary objects.
# Preserve the stamp's mtime unless the actual compiler configuration changes.
quote = '$(subst ','"'"',$(1))'
$(BUILD_DIR)/.compiler-flags: force-compiler-flags
	@mkdir -p $(BUILD_DIR)
# Dependency files name their exact object target. Changing BUILD_DIR's
# spelling (absolute versus relative) must rebuild before trusting those files.
	@printf '%s\n' $(call quote,$(CC)) $(call quote,$(CFLAGS)) \
		$(call quote,$(FRAMEFLAGS)) $(call quote,$(OBJCOPY)) \
		$(call quote,$(BUILD_DIR)) > $@.tmp
	@if cmp -s $@.tmp $@; then rm $@.tmp; else mv $@.tmp $@; fi

force-compiler-flags:

# zir.h includes the IR records generated from cmd/compiler_ir.zi, and
# zir_simd.h the runtime-emit records, and zir_token.h the scanner kinds.
$(BUILD_DIR)/obj/%.o: %.c $(BUILD_DIR)/.compiler-flags $(IR_READY) $(NATIVE_NAMES_READY) $(RUNTIME_EMIT_READY) $(SCALAR_READY) | $(SIMD_HEADERS) $(SCANNER_READY)
	@mkdir -p $(dir $@)
	$(NICE) $(CC) $(CFLAGS) $($@_CFLAGS) $(FRAMEFLAGS) $(DEPFLAGS) -c -o $@ $<

$(call obj,cmd/zi2c/main.c): $(BACKEND_PROCESS_READY)
$(SCANNER_OBJECT): $(SCANNER_READY) $(BUILD_DIR)/.compiler-flags
	@mkdir -p $(dir $@)
	$(NICE) $(CC) $(CFLAGS) $(FRAMEFLAGS) $(DEPFLAGS) -c -o $@ $(SCANNER_C)/compiler_scan.c

$(TEXT_OBJECT): $(TEXT_READY) $(BUILD_DIR)/.compiler-flags
	@mkdir -p $(dir $@)
	$(NICE) $(CC) $(CFLAGS) $(FRAMEFLAGS) $(DEPFLAGS) -c -o $@ $(TEXT_C)/compiler_text.c

$(call obj,cmd/zir/zir_parse.c): $(SOURCE_READY)
$(SOURCE_OBJECT): $(SOURCE_READY) $(BUILD_DIR)/.compiler-flags
	@mkdir -p $(dir $@)
	$(NICE) $(CC) $(CFLAGS) $(FRAMEFLAGS) $(DEPFLAGS) -c -o $@ $(SOURCE_C)/compiler_source.c

$(DECLARATION_OBJECT): $(DECLARATION_READY) $(TEXT_READY) $(SOURCE_READY) $(SCANNER_READY) $(BUILD_DIR)/.compiler-flags
	@mkdir -p $(dir $@)
	$(NICE) $(CC) $(CFLAGS) $(FRAMEFLAGS) $(DEPFLAGS) -c -o $@ $(DECLARATION_C)/compiler_declaration.c

$(ENUM_OBJECT): $(ENUM_READY) $(TEXT_READY) $(BUILD_DIR)/.compiler-flags
	@mkdir -p $(dir $@)
	$(NICE) $(CC) $(CFLAGS) $(FRAMEFLAGS) $(DEPFLAGS) -c -o $@ $(ENUM_C)/compiler_enum.c

$(TYPE_OBJECT): $(TYPE_READY) $(TEXT_READY) $(BUILD_DIR)/.compiler-flags
	@mkdir -p $(dir $@)
	$(NICE) $(CC) $(CFLAGS) $(FRAMEFLAGS) $(DEPFLAGS) -c -o $@ $(TYPE_C)/compiler_type.c

$(EXPRESSION_OBJECT): $(EXPRESSION_READY) $(SCANNER_READY) $(SOURCE_READY) $(TEXT_READY) $(BUILD_DIR)/.compiler-flags
	@mkdir -p $(dir $@)
	$(NICE) $(CC) $(CFLAGS) $(FRAMEFLAGS) $(DEPFLAGS) -c -o $@ $(EXPRESSION_C)/compiler_expression.c
$(PARSE_OBJECT): $(PARSE_READY) $(EXPRESSION_READY) $(IR_READY) $(SERVICES_READY) $(SCANNER_READY) $(SOURCE_READY) $(TEXT_READY) $(TYPE_READY) $(BUILD_DIR)/.compiler-flags
	@mkdir -p $(dir $@)
	$(NICE) $(CC) $(CFLAGS) $(FRAMEFLAGS) $(DEPFLAGS) -c -o $@ $(PARSE_C)/compiler_parse.c

$(STATEMENT_OBJECT): $(STATEMENT_READY) $(SCANNER_READY) $(SOURCE_READY) $(TEXT_READY) $(BUILD_DIR)/.compiler-flags
	@mkdir -p $(dir $@)
	$(NICE) $(CC) $(CFLAGS) $(FRAMEFLAGS) $(DEPFLAGS) -c -o $@ $(STATEMENT_C)/compiler_statement.c

$(PROOF_OBJECT): $(PROOF_READY) $(PROOF_KERNEL_READY) $(SERVICES_READY) $(SOURCE_READY) $(TEXT_READY) $(BUILD_DIR)/.compiler-flags
	@mkdir -p $(dir $@)
	$(NICE) $(CC) $(CFLAGS) $(FRAMEFLAGS) $(DEPFLAGS) -c -o $@ $(PROOF_C)/compiler_proof.c
$(IR_OBJECT): $(IR_READY) $(BUILD_DIR)/.compiler-flags
	@mkdir -p $(dir $@)
	$(NICE) $(CC) $(CFLAGS) $(FRAMEFLAGS) $(DEPFLAGS) -c -o $@ $(IR_C)/compiler_ir.c
$(LAW_OBJECT): $(LAW_READY) $(IR_READY) $(SERVICES_READY) $(TEXT_READY) $(BUILD_DIR)/.compiler-flags
	@mkdir -p $(dir $@)
	$(NICE) $(CC) $(CFLAGS) $(FRAMEFLAGS) $(DEPFLAGS) -c -o $@ $(LAW_C)/compiler_law.c
$(call obj,cmd/zir/zir_services.c): $(BODY_READY)
$(BODY_OBJECT): $(BODY_READY) $(IR_READY) $(NATIVE_NAMES_READY) $(BUILD_DIR)/.compiler-flags
	@mkdir -p $(dir $@)
	$(NICE) $(CC) $(CFLAGS) $(FRAMEFLAGS) $(DEPFLAGS) -c -o $@ $(BODY_C)/compiler_portable_body.c
$(SCALAR_OBJECT): $(SCALAR_READY) $(IR_READY) $(BUILD_DIR)/.compiler-flags
	@mkdir -p $(dir $@)
	$(NICE) $(CC) $(CFLAGS) $(FRAMEFLAGS) $(DEPFLAGS) -c -o $@ $(SCALAR_C)/compiler_scalar.c
$(C_EMIT_OBJECT): $(C_EMIT_READY) $(IR_READY) $(NATIVE_NAMES_READY) $(SCALAR_READY) $(BUILD_DIR)/.compiler-flags
	@mkdir -p $(dir $@)
	$(NICE) $(CC) $(CFLAGS) $(FRAMEFLAGS) $(DEPFLAGS) -c -o $@ $(C_EMIT_C)/compiler_c_emit.c
$(C_EMIT_VALUE_OBJECT): $(C_EMIT_VALUE_READY) $(IR_READY) $(NATIVE_NAMES_READY) $(RANGE_READY) $(C_EMIT_READY) $(BUILD_DIR)/.compiler-flags
	@mkdir -p $(dir $@)
	$(NICE) $(CC) $(CFLAGS) $(FRAMEFLAGS) $(DEPFLAGS) -c -o $@ $(C_EMIT_VALUE_C)/compiler_c_emit_value.c
$(C_EMIT_STATEMENT_OBJECT): $(C_EMIT_STATEMENT_READY) $(IR_READY) $(NATIVE_NAMES_READY) $(RANGE_READY) $(C_EMIT_READY) $(C_EMIT_VALUE_READY) $(BUILD_DIR)/.compiler-flags
	@mkdir -p $(dir $@)
	$(NICE) $(CC) $(CFLAGS) $(FRAMEFLAGS) $(DEPFLAGS) -c -o $@ $(C_EMIT_STATEMENT_C)/compiler_c_emit_statement.c
$(C_EMIT_CALL_OBJECT): $(C_EMIT_CALL_READY) $(IR_READY) $(NATIVE_NAMES_READY) $(RANGE_READY) $(C_EMIT_READY) $(C_EMIT_VALUE_READY) $(BUILD_DIR)/.compiler-flags
	@mkdir -p $(dir $@)
	$(NICE) $(CC) $(CFLAGS) $(FRAMEFLAGS) $(DEPFLAGS) -c -o $@ $(C_EMIT_CALL_C)/compiler_c_emit_call.c
$(C_EMIT_EXPR_OBJECT): $(C_EMIT_EXPR_READY) $(IR_READY) $(NATIVE_NAMES_READY) $(RANGE_READY) $(C_EMIT_READY) $(C_EMIT_VALUE_READY) $(BUILD_DIR)/.compiler-flags
	@mkdir -p $(dir $@)
	$(NICE) $(CC) $(CFLAGS) $(FRAMEFLAGS) $(DEPFLAGS) -c -o $@ $(C_EMIT_EXPR_C)/compiler_c_emit_expr.c
$(SIMD_METADATA_OBJECT): $(SIMD_METADATA_READY) $(IR_READY) $(SCALAR_READY) $(RUNTIME_EMIT_READY) $(BUILD_DIR)/.compiler-flags
	@mkdir -p $(dir $@)
	$(NICE) $(CC) $(CFLAGS) $(FRAMEFLAGS) $(DEPFLAGS) -c -o $@ $(SIMD_METADATA_C)/compiler_simd_metadata.c
$(C_EMIT_SIMD_OBJECT): $(C_EMIT_SIMD_READY) $(IR_READY) $(NATIVE_NAMES_READY) $(RANGE_READY) $(C_EMIT_VALUE_READY) $(SIMD_METADATA_READY) $(RUNTIME_EMIT_READY) $(BUILD_DIR)/.compiler-flags
	@mkdir -p $(dir $@)
	$(NICE) $(CC) $(CFLAGS) $(FRAMEFLAGS) $(DEPFLAGS) -c -o $@ $(C_EMIT_SIMD_C)/compiler_c_emit_simd.c
$(GO_SIMD_OBJECT): $(GO_SIMD_READY) $(IR_READY) $(NATIVE_NAMES_READY) $(SIMD_METADATA_READY) $(RUNTIME_EMIT_READY) $(BUILD_DIR)/.compiler-flags
	@mkdir -p $(dir $@)
	$(NICE) $(CC) $(CFLAGS) $(FRAMEFLAGS) $(DEPFLAGS) -c -o $@ $(GO_SIMD_C)/compiler_go_simd.c
$(C_BACKEND_OBJECT): $(C_BACKEND_READY) $(IR_READY) $(NATIVE_NAMES_READY) $(RANGE_READY) $(C_EMIT_READY) $(C_EMIT_VALUE_READY) $(C_EMIT_CALL_READY) $(SIMD_METADATA_READY) $(RUNTIME_EMIT_READY) $(BUILD_DIR)/.compiler-flags
	@mkdir -p $(dir $@)
	$(NICE) $(CC) $(CFLAGS) $(FRAMEFLAGS) $(DEPFLAGS) -c -o $@ $(C_BACKEND_C)/compiler_c_backend.c
$(call obj,cmd/zir/zir_services.c): $(SIMD_DIR)/zir_simd_go_bodies.h
$(SERVICES_OBJECT): $(SERVICES_READY) $(IR_READY) $(BUILD_DIR)/.compiler-flags
	@mkdir -p $(dir $@)
	$(NICE) $(CC) $(CFLAGS) $(FRAMEFLAGS) $(DEPFLAGS) -c -o $@ $(SERVICES_C)/compiler_services.c
$(LINK_OBJECT): $(LINK_READY) $(SERVICES_READY) $(SCANNER_READY) $(TEXT_READY) $(IR_READY) $(BUILD_DIR)/.compiler-flags
	@mkdir -p $(dir $@)
	$(NICE) $(CC) $(CFLAGS) $(FRAMEFLAGS) $(DEPFLAGS) -c -o $@ $(LINK_C)/compiler_link.c
$(PROOF_KERNEL_OBJECT): $(PROOF_KERNEL_READY) $(BUILD_DIR)/.compiler-flags
	@mkdir -p $(dir $@)
	$(NICE) $(CC) $(CFLAGS) $(FRAMEFLAGS) $(DEPFLAGS) -c -o $@ $(PROOF_KERNEL_C)/compiler_proof_kernel.c

$(call obj,cmd/zi2c/main.c cmd/zi2go/main.c cmd/zi2zir/main.c cmd/zi2zx/main.c tests/bundle_link_test.c): $(BUNDLE_READY)
$(BUNDLE_MODULE_OBJECT): $(BUNDLE_READY) $(BUILD_DIR)/.compiler-flags
	@mkdir -p $(dir $@)
	$(NICE) $(CC) $(CFLAGS) $(FRAMEFLAGS) $(DEPFLAGS) -c -o $@ $(BUNDLE_C)/compiler_bundle.c

$(BUILD_DIR)/obj/runtime_headers.o: $(BUILD_DIR)/runtime_headers.c $(BUILD_DIR)/.compiler-flags $(IR_READY) $(RUNTIME_EMIT_READY)
	@mkdir -p $(dir $@)
	$(NICE) $(CC) $(CFLAGS) $(DEPFLAGS) -c -o $@ $<

$(RUNTIME_EMIT_OBJECTS): $(BUILD_DIR)/obj/runtime-emit/%.o: $(RUNTIME_EMIT_C)/%.c \
    $(BUILD_DIR)/.compiler-flags $(RUNTIME_EMIT_READY)
	@mkdir -p $(dir $@)
	$(NICE) $(CC) $(CFLAGS) $(DEPFLAGS) -c -o $@ $<

$(BACKEND_PROCESS_OBJECTS): $(BUILD_DIR)/obj/backend-process/%.o: $(BACKEND_PROCESS_C)/%.c \
    $(BUILD_DIR)/.compiler-flags $(BACKEND_PROCESS_READY)
	@mkdir -p $(dir $@)
	$(NICE) $(CC) $(CFLAGS) $(DEPFLAGS) -c -o $@ $<

$(BACKEND_CACHE_OBJECT): $(BACKEND_CACHE_C)/compiler_backend_cache.c \
    $(BUILD_DIR)/.compiler-flags $(BACKEND_CACHE_READY)
	@mkdir -p $(dir $@)
	$(NICE) $(CC) $(CFLAGS) $(FRAMEFLAGS) $(DEPFLAGS) -c -o $@ $<

$(C_PRUNE_OBJECTS): $(BUILD_DIR)/obj/c-prune/%.o: $(C_PRUNE_C)/%.c $(BUILD_DIR)/.compiler-flags $(C_PRUNE_READY)
	@mkdir -p $(dir $@)
	$(NICE) $(CC) $(CFLAGS) -I$(C_PRUNE_C) $(DEPFLAGS) -c -o $@ $<
$(RANGE_OBJECTS): $(BUILD_DIR)/obj/c-range/%.o: $(RANGE_C)/%.c $(BUILD_DIR)/.compiler-flags $(RANGE_READY) $(IR_READY)
	@mkdir -p $(dir $@)
	$(NICE) $(CC) $(CFLAGS) $(FRAMEFLAGS) $(DEPFLAGS) -c -o $@ $<

$(NATIVE_NAMES_OBJECT): $(NATIVE_NAMES_C)/compiler_native_names.c \
    $(BUILD_DIR)/.compiler-flags $(NATIVE_NAMES_READY) $(TYPE_READY)
	@mkdir -p $(dir $@)
	$(NICE) $(CC) $(CFLAGS) $(DEPFLAGS) -c -o $@ $<

PARSE_PARTS := cmd/zir/zir_parse.c

# The C and Ziran parser parts agree on the names of their shared private
# calls. Keep the prefix on Wasm, where objcopy cannot localize them.
ifneq ($(findstring wasm,$(TARGET_MACHINE)),)
WASM_PRIVATE_HEADERS ?= $(BUILD_DIR)/wasm-private
else
PARSE_LOCALIZE := --wildcard --localize-symbol='private_parse_*' --localize-symbol='compiler_parse_*' \
    --localize-symbol='compiler_frontend_parse_*' --localize-symbol='compiler_ir_cleanup_*' \
    $(foreach module,$(PERSISTENCE_PORT_MODULES) zx_link inline_asm,--localize-symbol='compiler_$(module)_*')
endif
PARSE_PRIVATE_HEADERS := $(or $(WASM_PRIVATE_HEADERS),$(BUILD_DIR)/private-symbols)
$(PARSE_PRIVATE_HEADERS)/%.h: cmd/zir/zir_%_internal.h scripts/private-symbols.py
	python3 scripts/private-symbols.py $< $(basename $@).txt
	sed 's/.*/#define & private_$*_&/' $(basename $@).txt > $@
$(call obj,$(PARSE_PARTS)): CFLAGS += -include $(PARSE_PRIVATE_HEADERS)/parse.h
$(call obj,$(PARSE_PARTS)): $(PARSE_PRIVATE_HEADERS)/parse.h

# Frontend services also use private parser scratch and text helpers.
$(BUILD_DIR)/obj/parse.o: $(call obj,$(PARSE_PARTS) $(PERSISTENCE_HOST)) $(PARSE_PORT_OBJECTS) $(FRONTEND_PORT_OBJECTS) Makefile
	$(CC) -r -nostdlib -o $@ $(filter %.o,$^)
	$(OBJCOPY) --localize-hidden $(PARSE_LOCALIZE) $@

-include $(shell find $(BUILD_DIR)/obj -name '*.d' 2>/dev/null)

$(BUILD_DIR)/libziran.a: $(LIB_OBJECTS) Makefile
	$(RM) $@
	$(AR) rcs $@ $(LIB_OBJECTS)

$(BIN_DIR):
	mkdir -p $@

PACKAGE_C := $(BUILD_DIR)/package-c
PACKAGE_SOURCES := cmd/package_artifact.zi cmd/package_zx.zi cmd/package_checked.zi cmd/package.zi cmd/package_entry.zi cmd/package_process.zi cmd/package_script.zi cmd/package_bundle_run.zi cmd/diagnostics.zi \
    cmd/package_add.zi cmd/package_guide.zi \
    cmd/package_capabilities.zi cmd/package_features.zi cmd/package_explain.zi cmd/package_learn.zi cmd/package_learn_teacher.zi cmd/package_learn_tuning.zi cmd/compiler_learning_tuning.zi cmd/compiler_bundle.zi cmd/compiler_lowered_ir.zi std/sort.zi cmd/compiler_learning_policy.zi cmd/compiler_learning_fixture.zi cmd/package_common.zi cmd/compiler_target.zi \
    cmd/package_manifest.zi cmd/package_source.zi cmd/package_lock.zi cmd/package_map.zi cmd/package_options.zi cmd/package_compile_commands.zi cmd/package_template.zi \
    std/byte_text_linux.zi std/c_string.zi std/file_linux.zi std/number_text.zi std/process_run_linux.zi \
    std/json_scan.zi std/json_quote.zi std/json_bytes.zi std/json_document.zi std/float_text.zi std/text_tools.zi std/simd.zi std/date_time_linux.zi std/date_time_types.zi \
    std/toml_document.zi std/utf8.zi std/text.zi std/vec.zi VERSION
# Generated files are listed when the ziran recipe expands, after generation.
PACKAGE_OBJECTS = $(patsubst $(PACKAGE_C)/%.c,$(BUILD_DIR)/obj/package-c/%.o,$(wildcard $(PACKAGE_C)/*.c))

# Regenerate beside the old output and copy over only files whose text
# changed, so a compiler edit that leaves ziran's C alone recompiles nothing.
$(PACKAGE_C)/.generated: $(PACKAGE_SOURCES) $(GEN_ZI2C)
	rm -rf $(PACKAGE_C).next
	mkdir -p $(BUILD_DIR)/package-source
	printf 'Version :: "%s";\n' '$(ZIRAN_VERSION)' > $(BUILD_DIR)/package-source/version.zi
	$(GEN_ZI2C) --no-main $(ZIRAN_DEFINES) --root cmd --module-path std \
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
$(call obj,$(PERSISTENCE_HOST))_CFLAGS += -DZIRAN_STD_DIR='"$(abspath std)"' \
    -DZIRAN_INCLUDE_DIR='"$(abspath include)"'

# The native command imports a generated Ziran constant from VERSION.
ZIRAN_VERSION := $(shell cat VERSION)

package-objects: $(PACKAGE_OBJECTS)

# Make expands a whole recipe before running its first line, so the object
# list is only correct in a fresh make started after generation. That sub-make
# both compiles and links; linking in this recipe would miss the generated
# objects on a clean build.
package-link: $(PACKAGE_OBJECTS) | $(BIN_DIR)
	@set -e; \
	linked_bin=$$(mktemp "$(BIN_DIR)/.ziran.XXXXXX"); \
	trap 'rm -f "$$linked_bin"' EXIT; \
	trap 'exit 1' HUP INT TERM; \
	$(CC) $(CFLAGS) -o "$$linked_bin" $(PACKAGE_OBJECTS) $(PACKAGE_LIBS); \
	chmod 755 "$$linked_bin"; \
	mv -f "$$linked_bin" "$(BIN_DIR)/ziran"

$(BIN_DIR)/ziran: $(PACKAGE_C)/.generated $(HEADERS) | $(BIN_DIR)
	+$(MAKE) --no-print-directory package-link

FORMAT_C := $(BUILD_DIR)/format-c
FORMAT_SOURCES := cmd/format.zi cmd/format_source.zi cmd/diagnostics.zi std/vec.zi std/option.zi \
    std/byte_text_linux.zi std/c_string.zi std/file_linux.zi
$(BIN_DIR)/zi-fmt: $(FORMAT_SOURCES) $(BIN_DIR)/zi2c $(BUILD_DIR)/.compiler-flags | $(BIN_DIR)
	$(BIN_DIR)/zi2c --entry format:main $(ZIRAN_DEFINES) --root cmd --module-path std \
	    -o $(FORMAT_C) cmd/format.zi
	$(NICE) $(CC) $(CFLAGS) -I$(FORMAT_C) -o $@ $(FORMAT_C)/*.c -lm

EDIT_C := $(BUILD_DIR)/edit-c
$(BIN_DIR)/zi-edit: cmd/edit.zi cmd/transport.zi cmd/format_source.zi cmd/diagnostics.zi std/json_document.zi cmd/zir/zir_edit.c cmd/zir/zir_edit.h $(BIN_DIR)/zi2c $(FRONTEND_OBJECTS) $(BUNDLE_MODULE_OBJECT) | $(BIN_DIR)
	$(BIN_DIR)/zi2c --entry edit:main $(ZIRAN_DEFINES) --root cmd --module-path std -o $(EDIT_C) cmd/edit.zi
	$(NICE) $(CC) $(CFLAGS) -I$(EDIT_C) -o $@ $(EDIT_C)/*.c cmd/zir/zir_edit.c $(FRONTEND_OBJECTS) $(BUNDLE_MODULE_OBJECT) -lm

$(BIN_DIR)/zi2zir: $(call obj,cmd/zi2zir/main.c) $(BUNDLE_OBJECT) $(FRONTEND_OBJECTS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ -lm
	chmod 755 $@

$(BIN_DIR)/zi-api: $(call obj,cmd/zir-api/main.c) $(FRONTEND_OBJECTS) $(BUNDLE_MODULE_OBJECT) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ -lm

INSPECT_C := $(BUILD_DIR)/inspect-c
$(BIN_DIR)/zi-inspect: cmd/inspect.zi cmd/compiler_ir.zi $(BIN_DIR)/zi2c $(FRONTEND_OBJECTS) $(BUNDLE_OBJECT) | $(BIN_DIR)
	$(BIN_DIR)/zi2c --entry inspect:main $(ZIRAN_DEFINES) --root cmd -o $(INSPECT_C) cmd/inspect.zi
	$(NICE) $(CC) $(CFLAGS) -I$(INSPECT_C) -o $@ $(INSPECT_C)/inspect.c $(FRONTEND_OBJECTS) $(BUNDLE_OBJECT) -lm

RUNTIME_HEADERS := include/zir_bounds.h include/zir_string_types.h include/zir_string.h include/zir_slice.h \
    include/zir_vec.h include/ziran_parallel.h include/ziran_simd.h \
    $(SIMD_DIR)/ziran_simd_types.h $(SIMD_DIR)/ziran_simd_ops.h

$(SIMD_DIR)/simd-header: $(wildcard bootstrap/simd/*.c bootstrap/simd/*.h)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(BOOTSTRAP_CFLAGS) -std=c11 -Iinclude -o $@ $(filter %.c,$^) -lm

# Text that did not change keeps its timestamp, so nothing that includes it rebuilds.
simd_header = @$(SIMD_DIR)/simd-header $(1) > $@.tmp && { cmp -s $@.tmp $@ && rm $@.tmp || mv -f $@.tmp $@; }
$(SIMD_DIR)/ziran_simd_ops.h: $(SIMD_DIR)/simd-header
	$(call simd_header,)
$(SIMD_DIR)/ziran_simd_types.h: $(SIMD_DIR)/simd-header
	$(call simd_header,--types)
$(SIMD_DIR)/zir_simd_go_bodies.h: $(SIMD_DIR)/simd-header
	$(call simd_header,--go)

# Reused builds can be newer than restored or exported source headers.
# Compare embedded bytes too, retaining timestamps when nothing changed.
BOOTSTRAP_BIN := $(BUILD_DIR)/bootstrap/bin/zi2c
$(BUILD_DIR)/runtime_headers.c: scripts/embed_headers.sh $(RUNTIME_HEADERS) force-compiler-flags
	mkdir -p $(dir $@)
	@set -e; temporary="$@.$$$$.tmp"; trap 'rm -f "$$temporary"' EXIT HUP INT TERM; \
	    sh scripts/embed_headers.sh $(RUNTIME_HEADERS) > "$$temporary"; \
	    if cmp -s "$$temporary" "$@"; then rm -f "$$temporary"; \
	    else mv -f "$$temporary" "$@"; fi

$(RUNTIME_EMIT_C)/.generated: cmd/compiler_runtime_emit.zi std/file.zi std/file_linux.zi \
    std/c_string.zi std/result.zi std/vec.zi $(BOOTSTRAP_BIN)
	$(BOOTSTRAP_BIN) --no-main $(ZIRAN_DEFINES) --root cmd --module-path std \
	    -o $(RUNTIME_EMIT_C) cmd/compiler_runtime_emit.zi
	touch $@

$(BACKEND_CACHE_C)/.generated: cmd/compiler_backend_cache.zi std/file.zi std/file_linux.zi \
    std/file_tree_linux.zi std/environment.zi std/environment_linux.zi std/c_string.zi std/vec.zi \
    std/text_tools.zi std/sort.zi std/process_run_linux.zi std/byte_text_linux.zi $(BOOTSTRAP_BIN)
	$(BOOTSTRAP_BIN) --no-main $(ZIRAN_DEFINES) --root cmd --module-path std \
	    -o $(BACKEND_CACHE_C) cmd/compiler_backend_cache.zi
	touch $@

$(BACKEND_PROCESS_C)/.generated: cmd/compiler_backend_process.zi cmd/compiler_backend_cache.zi \
    std/file.zi std/file_linux.zi std/file_tree_linux.zi std/environment.zi std/environment_linux.zi \
    std/c_string.zi std/vec.zi std/text_tools.zi std/sort.zi std/timer_linux.zi \
    std/process_run_linux.zi std/byte_text_linux.zi $(BOOTSTRAP_BIN)
	$(BOOTSTRAP_BIN) --no-main $(ZIRAN_DEFINES) --root cmd --module-path std \
	    -o $(BACKEND_PROCESS_C) cmd/compiler_backend_process.zi
	touch $@

$(NATIVE_NAMES_C)/.generated: cmd/compiler_native_names.zi cmd/compiler_ir.zi cmd/compiler_services.zi cmd/compiler_type.zi $(BOOTSTRAP_BIN)
	$(BOOTSTRAP_BIN) --no-main --root cmd --module-path std \
	    -o $(NATIVE_NAMES_C) cmd/compiler_native_names.zi
	touch $@

$(C_PRUNE_C)/.generated: cmd/compiler_c_prune.zi std/vec.zi std/hash_map.zi std/text_tools.zi $(BOOTSTRAP_BIN)
	env -u DISPLAY -u WAYLAND_DISPLAY $(BOOTSTRAP_BIN) --no-main --root cmd --module-path std \
	    -o $(C_PRUNE_C) cmd/compiler_c_prune.zi
	touch $@

$(RANGE_C)/.generated: $(patsubst %,cmd/%.zi,$(RANGE_MODULES)) cmd/compiler_ir.zi $(BOOTSTRAP_BIN)
	env -u DISPLAY -u WAYLAND_DISPLAY $(BOOTSTRAP_BIN) --no-main --root cmd \
	    -o $(RANGE_C) cmd/compiler_c_range.zi
	touch $@

$(call obj,cmd/zi2c/main.c): $(C_BACKEND_READY) $(C_EMIT_READY) $(C_EMIT_VALUE_READY) $(C_EMIT_CALL_READY) $(SIMD_METADATA_READY) $(RANGE_READY)
$(BIN_DIR)/zi2c: $(call obj,cmd/zi2c/main.c) $(C_BACKEND_OBJECT) $(C_PRUNE_OBJECTS) \
    $(BUNDLE_OBJECT) $(FRONTEND_OBJECTS) $(RUNTIME_OBJECTS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ -lm
	chmod 755 $@

$(call obj,cmd/zi2go/main.c): $(foreach module,$(GO_PORT_MODULES),$(call compiler-port-ready,$(module)))
$(BIN_DIR)/zi2go: $(call obj,cmd/zi2go/main.c) $(GO_PORT_OBJECTS) $(BUNDLE_OBJECT) $(FRONTEND_OBJECTS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ -lm

# zi2zx runs revision-28 executable ZX on the portable instruction engine.
$(BIN_DIR)/zi2zx: $(call obj,cmd/zi2zx/main.c cmd/zi2zx/zx_exec_run.c) $(BUILD_DIR)/obj/assembly-core.o \
    $(BUILD_DIR)/libziran.a | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(RUNTIME_LIBS)
	chmod 755 $@

# zx2wasm accepts only verified revision-28 ZX and uses the same lowerer object
# as the embedding Wasm tier in libziran.
$(BIN_DIR)/zx2wasm: $(call obj,cmd/zx2wasm/main.c) $(BUILD_DIR)/libziran.a | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(RUNTIME_LIBS)
	chmod 755 $@

ifneq ($(BOOTSTRAP),1)
$(BOOTSTRAP_BIN): $(BUILD_DIR)/runtime_headers.c
$(BOOTSTRAP_BIN): $(FRONTEND) $(PARSE_PARTS) $(PERSISTENCE_HOST) \
    $(foreach module,$(CHECK_PORT_MODULES) $(PARSE_PORT_MODULES) $(FRONTEND_PORT_MODULES),$(call compiler-port-seed,$(module))) \
    cmd/compiler_runtime_emit.zi std/file.zi std/file_linux.zi std/c_string.zi std/result.zi std/vec.zi \
    cmd/zi2c/main.c $(HEADERS) $(RUNTIME_HEADERS) scripts/embed_headers.sh \
    $(BACKEND_BOOTSTRAP_FILES) cmd/compiler_backend_cache.zi cmd/compiler_backend_process.zi \
    cmd/compiler_native_names.zi cmd/compiler_portable_body.zi \
    bootstrap/compiler_portable_body/compiler_portable_body.c \
    bootstrap/compiler_portable_body/compiler_portable_body.h \
    bootstrap/compiler_scalar/compiler_scalar.c \
    bootstrap/compiler_scalar/compiler_scalar.h \
    bootstrap/compiler_c_emit/compiler_c_emit.c \
    bootstrap/compiler_c_emit/compiler_c_emit.h \
    bootstrap/compiler_c_emit_value/compiler_c_emit_value.c \
    bootstrap/compiler_c_emit_value/compiler_c_emit_value.h \
    bootstrap/compiler_c_emit_statement/compiler_c_emit_statement.c \
    bootstrap/compiler_c_emit_statement/compiler_c_emit_statement.h \
    bootstrap/compiler_c_emit_call/compiler_c_emit_call.c \
    bootstrap/compiler_c_emit_call/compiler_c_emit_call.h \
    bootstrap/compiler_c_emit_expr/compiler_c_emit_expr.c \
    bootstrap/compiler_c_emit_expr/compiler_c_emit_expr.h \
    bootstrap/compiler_simd_metadata/compiler_simd_metadata.c \
    bootstrap/compiler_simd_metadata/compiler_simd_metadata.h \
    bootstrap/compiler_c_emit_simd/compiler_c_emit_simd.c \
    bootstrap/compiler_c_emit_simd/compiler_c_emit_simd.h \
    bootstrap/compiler_go_simd/compiler_go_simd.c \
    bootstrap/compiler_go_simd/compiler_go_simd.h \
    bootstrap/compiler_c_backend/compiler_c_backend.c \
    bootstrap/compiler_c_backend/compiler_c_backend.h \
    bootstrap/compiler_scan/compiler_scan.c bootstrap/compiler_scan/compiler_scan.h \
    bootstrap/compiler_text/compiler_text.c bootstrap/compiler_text/compiler_text.h \
    bootstrap/compiler_source/compiler_source.c bootstrap/compiler_source/compiler_source.h \
    bootstrap/compiler_declaration/compiler_declaration.c bootstrap/compiler_declaration/compiler_declaration.h \
    bootstrap/compiler_enum/compiler_enum.c bootstrap/compiler_enum/compiler_enum.h \
    bootstrap/compiler_type/compiler_type.c bootstrap/compiler_type/compiler_type.h \
    bootstrap/compiler_expression/compiler_expression.c bootstrap/compiler_expression/compiler_expression.h \
    bootstrap/compiler_parse/compiler_parse.c bootstrap/compiler_parse/compiler_parse.h \
    bootstrap/compiler_statement/compiler_statement.c bootstrap/compiler_statement/compiler_statement.h \
    bootstrap/compiler_proof_kernel/compiler_proof_kernel.c bootstrap/compiler_proof_kernel/compiler_proof_kernel.h \
    bootstrap/compiler_proof_kernel/vec.h \
    bootstrap/compiler_proof/compiler_proof.c bootstrap/compiler_proof/compiler_proof.h \
    bootstrap/compiler_proof/vec.h \
    bootstrap/compiler_bundle/compiler_bundle.c bootstrap/compiler_bundle/compiler_bundle.h \
    bootstrap/compiler_ir/compiler_ir.c bootstrap/compiler_ir/compiler_ir.h \
    bootstrap/compiler_law/compiler_law.c bootstrap/compiler_law/compiler_law.h \
    bootstrap/compiler_services/compiler_services.c bootstrap/compiler_services/compiler_services.h \
    bootstrap/compiler_link/compiler_link.c bootstrap/compiler_link/compiler_link.h \
    bootstrap/compiler_link/vec.h bootstrap/compiler_runtime_emit/compiler_runtime_emit.c \
    bootstrap/compiler_runtime_emit/compiler_runtime_emit.h bootstrap/compiler_runtime_emit/file.c \
    bootstrap/compiler_runtime_emit/file.h bootstrap/compiler_runtime_emit/file_linux.c \
    bootstrap/compiler_runtime_emit/file_linux.h bootstrap/compiler_runtime_emit/c_string.c \
    bootstrap/compiler_runtime_emit/c_string.h bootstrap/compiler_runtime_emit/result.h bootstrap/compiler_runtime_emit/vec.c \
    bootstrap/compiler_runtime_emit/vec.h Makefile \
    bootstrap/compiler_native_names/compiler_native_names.c \
    bootstrap/compiler_native_names/compiler_native_names.h \
    bootstrap/compiler_lowered_ir/compiler_lowered_ir.c bootstrap/compiler_lowered_ir/compiler_lowered_ir.h \
    bootstrap/compiler_zir_lower/compiler_zir_lower.c bootstrap/compiler_zir_lower/compiler_zir_lower.h \
    $(wildcard bootstrap/compiler_c_prune/*.c bootstrap/compiler_c_prune/*.h) \
    $(wildcard bootstrap/compiler_c_range/*.c bootstrap/compiler_c_range/*.h)
	+$(MAKE) --no-print-directory BOOTSTRAP=1 BUILD_DIR=$(BUILD_DIR)/bootstrap SIMD_DIR=$(SIMD_DIR) \
	    CC=$(call quote,$(HOST_CC)) AR=ar OBJCOPY=$(call quote,$(HOST_OBJCOPY)) CFLAGS=$(call quote,$(BOOTSTRAP_CFLAGS)) \
	    SANITIZE_FLAGS= WASM_PRIVATE_HEADERS= $(BOOTSTRAP_BIN)
endif

define compiler-port-rules
$(call compiler-port-object,$(1)): $(call compiler-port-ready,$(1)) $(2) $(BUILD_DIR)/.compiler-flags
	@mkdir -p $$(dir $$@)
	$$(NICE) $$(CC) $$(CFLAGS) $$(FRAMEFLAGS) $$(DEPFLAGS) -c -o $$@ $(call compiler-port-dir,$(1))/compiler_$(1).c

ifneq ($$(BOOTSTRAP),1)
$(call compiler-port-dir,$(1))/.generated: cmd/compiler_$(1).zi $(3) $$(BOOTSTRAP_BIN)
	env -u DISPLAY -u WAYLAND_DISPLAY $$(BOOTSTRAP_BIN) --no-main --root cmd --module-path std \
	    -o $(call compiler-port-dir,$(1)) cmd/compiler_$(1).zi
	touch $$@
endif
endef

CHECK_PORT_IMPORTS := cmd/compiler_ir.zi cmd/compiler_check_types.zi
CHECK_PORT_HEADERS := $(IR_READY) $(call compiler-port-ready,check_types)
GO_PORT_IMPORTS := cmd/compiler_ir.zi cmd/compiler_native_names.zi
$(eval $(call compiler-port-rules,check_types,$(IR_READY),cmd/compiler_ir.zi))
$(foreach module,$(CHECK_CORE_MODULES),\
    $(eval $(call compiler-port-rules,$(module),$(CHECK_PORT_HEADERS),$(CHECK_PORT_IMPORTS))))
$(eval $(call compiler-port-rules,check_s12_types,$(CHECK_PORT_HEADERS),$(CHECK_PORT_IMPORTS)))
$(foreach module,$(CHECK_PROGRAM_MODULES),\
    $(eval $(call compiler-port-rules,$(module),$(CHECK_PORT_HEADERS) $(call compiler-port-ready,check_s12_types),$(CHECK_PORT_IMPORTS) cmd/compiler_check_s12_types.zi)))
PARSE_PORT_IMPORTS := cmd/compiler_ir.zi cmd/compiler_parse_types.zi
PARSE_PORT_HEADERS := $(IR_READY) $(call compiler-port-ready,parse_types)
$(eval $(call compiler-port-rules,parse_types,$(IR_READY),cmd/compiler_ir.zi))
$(eval $(call compiler-port-rules,parse_condition,$(PARSE_PORT_HEADERS) $(SOURCE_READY) $(TEXT_READY),$(PARSE_PORT_IMPORTS) cmd/compiler_source.zi cmd/compiler_text.zi cmd/compiler_scan.zi))
$(eval $(call compiler-port-rules,parse_discover,$(PARSE_PORT_HEADERS) $(TYPE_READY),$(PARSE_PORT_IMPORTS) cmd/compiler_type.zi cmd/compiler_source.zi cmd/compiler_text.zi cmd/compiler_scan.zi))
$(eval $(call compiler-port-rules,parse_source,$(PARSE_PORT_HEADERS),$(PARSE_PORT_IMPORTS)))
$(eval $(call compiler-port-rules,frontend_parse,$(PARSE_PORT_HEADERS) $(SOURCE_READY),$(PARSE_PORT_IMPORTS) cmd/compiler_source.zi cmd/compiler_scan.zi))
S14_PARSE_IMPORTS := $(PARSE_PORT_IMPORTS) cmd/compiler_statement.zi cmd/compiler_declaration.zi \
    cmd/compiler_source.zi cmd/compiler_text.zi cmd/compiler_scan.zi
S14_PARSE_HEADERS := $(PARSE_PORT_HEADERS) $(STATEMENT_READY) $(DECLARATION_READY) \
    $(SOURCE_READY) $(TEXT_READY) $(SCANNER_READY)
$(eval $(call compiler-port-rules,parse_s14_types,$(S14_PARSE_HEADERS),$(S14_PARSE_IMPORTS)))
$(foreach module,$(S14_PARSE_MODULES),\
    $(eval $(call compiler-port-rules,$(module),$(S14_PARSE_HEADERS) $(call compiler-port-ready,parse_s14_types),$(S14_PARSE_IMPORTS) cmd/compiler_parse_s14_types.zi)))
FRONTEND_SUPPORT_IMPORTS := $(S14_PARSE_IMPORTS) cmd/compiler_parse_s14_types.zi cmd/compiler_check_types.zi
FRONTEND_SUPPORT_HEADERS := $(S14_PARSE_HEADERS) $(call compiler-port-ready,parse_s14_types) $(call compiler-port-ready,check_types)
$(eval $(call compiler-port-rules,context_types,$(FRONTEND_SUPPORT_HEADERS),$(FRONTEND_SUPPORT_IMPORTS)))
$(foreach module,$(SUPPORT_PORT_MODULES),\
    $(eval $(call compiler-port-rules,$(module),$(FRONTEND_SUPPORT_HEADERS) $(call compiler-port-ready,context_types),$(FRONTEND_SUPPORT_IMPORTS) cmd/compiler_context_types.zi)))
# The generated source store also includes the standard Vec record header.
$(call compiler-port-object,source_store): $(PROOF_KERNEL_READY)
$(eval $(call compiler-port-rules,frontend_ir,$(FRONTEND_SUPPORT_HEADERS) $(call compiler-port-ready,context_types) $(TYPE_READY) $(ENUM_READY),$(FRONTEND_SUPPORT_IMPORTS) cmd/compiler_context_types.zi cmd/compiler_type.zi cmd/compiler_enum.zi))
$(eval $(call compiler-port-rules,law_runtime,$(PARSE_PORT_HEADERS) $(CHECK_PORT_HEADERS),$(PARSE_PORT_IMPORTS) cmd/compiler_check_types.zi))
$(eval $(call compiler-port-rules,target_bridge,$(PARSE_PORT_HEADERS) $(CHECK_PORT_HEADERS) $(TYPE_READY),$(PARSE_PORT_IMPORTS) cmd/compiler_check_types.zi cmd/compiler_type.zi))
$(call obj,cmd/zir/zir_memory.c): $(FRONTEND_SUPPORT_HEADERS) $(call compiler-port-ready,context_types)

# The embedding API (include/ziran_host.h): one module read from two files,
# linked into libziran beside the engine, native and wasm layers it calls.
$(eval $(call compiler-port-rules,host_api,,cmd/compiler_host_api_state.zi))

# Inline assembly: one module read from three source files.
$(eval $(call compiler-port-rules,inline_asm,$(IR_READY) $(NATIVE_NAMES_READY),$(GO_PORT_IMPORTS) \
    cmd/compiler_inline_asm_check.zi cmd/compiler_inline_asm_emit.zi))

# Entry linking: one module read from three source files.
$(eval $(call compiler-port-rules,zx_link,$(IR_READY),cmd/compiler_ir.zi cmd/compiler_zx_link_prune.zi cmd/compiler_zx_link_reach.zi))

PARSE_COMPILER_SOURCES := cmd/compiler_parse.zi cmd/compiler_token.zi cmd/compiler_expr_ir.zi cmd/compiler_expression.zi \
    cmd/compiler_ir.zi cmd/compiler_services.zi cmd/compiler_scan.zi cmd/compiler_source.zi cmd/compiler_text.zi cmd/compiler_type.zi
SERIAL_IMPORTS := $(PARSE_COMPILER_SOURCES) $(PARSE_PORT_IMPORTS) cmd/compiler_check_types.zi
SERIAL_HEADERS := $(PARSE_PORT_HEADERS) $(CHECK_PORT_HEADERS) $(PARSE_READY) \
    $(EXPRESSION_READY) $(SERVICES_READY) $(SCANNER_READY) $(SOURCE_READY) $(TEXT_READY) $(TYPE_READY) $(PROOF_KERNEL_READY)
PERSISTENCE_FILES_IMPORTS := $(FRONTEND_SUPPORT_IMPORTS) cmd/compiler_context_types.zi cmd/compiler_files.zi
PERSISTENCE_FILES_HEADERS := $(FRONTEND_SUPPORT_HEADERS) $(call compiler-port-ready,context_types) $(call compiler-port-ready,files)
PACKAGE_RESOLVE_IMPORTS := $(PERSISTENCE_FILES_IMPORTS) cmd/compiler_source_store.zi std/vec.zi
PACKAGE_RESOLVE_HEADERS := $(PERSISTENCE_FILES_HEADERS) $(call compiler-port-ready,source_store) $(PROOF_KERNEL_READY)
$(eval $(call compiler-port-rules,zir_serial,$(SERIAL_HEADERS),$(SERIAL_IMPORTS)))
$(eval $(call compiler-port-rules,package_resolve,$(PACKAGE_RESOLVE_HEADERS),$(PACKAGE_RESOLVE_IMPORTS)))
$(eval $(call compiler-port-rules,load,$(SERIAL_HEADERS) $(PACKAGE_RESOLVE_HEADERS) $(call compiler-port-ready,zir_serial) $(call compiler-port-ready,package_resolve),$(SERIAL_IMPORTS) $(PACKAGE_RESOLVE_IMPORTS) cmd/compiler_zir_serial.zi cmd/compiler_package_resolve.zi))
$(eval $(call compiler-port-rules,zx_assets,$(PERSISTENCE_FILES_HEADERS) $(BUNDLE_READY),$(PERSISTENCE_FILES_IMPORTS) cmd/compiler_bundle.zi))
$(call obj,$(PERSISTENCE_HOST)): $(SERIAL_HEADERS) $(PACKAGE_RESOLVE_HEADERS) $(BUNDLE_READY) \
    $(foreach module,$(PERSISTENCE_PORT_MODULES),$(call compiler-port-ready,$(module)))

$(eval $(call compiler-port-rules,go_asm_x64,$(IR_READY) $(NATIVE_NAMES_READY),$(GO_PORT_IMPORTS)))
$(eval $(call compiler-port-rules,go_asm,$(IR_READY) $(NATIVE_NAMES_READY) $(call compiler-port-ready,go_asm_x64),$(GO_PORT_IMPORTS) cmd/compiler_go_asm_x64.zi))
$(eval $(call compiler-port-rules,go_backend,$(IR_READY) $(NATIVE_NAMES_READY) $(call compiler-port-ready,go_asm) $(call compiler-port-ready,go_asm_x64),$(GO_PORT_IMPORTS) cmd/compiler_go_asm.zi cmd/compiler_go_asm_x64.zi))

ifneq ($(BOOTSTRAP),1)
$(BODY_C)/.generated: cmd/compiler_portable_body.zi cmd/compiler_ir.zi cmd/compiler_native_names.zi cmd/compiler_services.zi cmd/compiler_type.zi $(BOOTSTRAP_BIN)
	$(BOOTSTRAP_BIN) --no-main --root cmd --module-path std \
	    -o $(BODY_C) cmd/compiler_portable_body.zi
	touch $@

$(SCALAR_C)/.generated: cmd/compiler_scalar.zi cmd/compiler_ir.zi $(BOOTSTRAP_BIN)
	env -u DISPLAY -u WAYLAND_DISPLAY $(BOOTSTRAP_BIN) --no-main --root cmd \
	    -o $(SCALAR_C) cmd/compiler_scalar.zi
	touch $@

$(C_EMIT_C)/.generated: cmd/compiler_c_emit.zi cmd/compiler_ir.zi cmd/compiler_native_names.zi cmd/compiler_scalar.zi cmd/compiler_services.zi cmd/compiler_type.zi $(BOOTSTRAP_BIN)
	env -u DISPLAY -u WAYLAND_DISPLAY $(BOOTSTRAP_BIN) --no-main --root cmd --module-path std \
	    -o $(C_EMIT_C) cmd/compiler_c_emit.zi
	touch $@

$(C_EMIT_VALUE_C)/.generated: cmd/compiler_c_emit_value.zi cmd/compiler_c_emit.zi cmd/compiler_ir.zi cmd/compiler_native_names.zi $(patsubst %,cmd/%.zi,$(RANGE_MODULES)) $(BOOTSTRAP_BIN)
	env -u DISPLAY -u WAYLAND_DISPLAY $(BOOTSTRAP_BIN) --no-main --root cmd --module-path std \
	    -o $(C_EMIT_VALUE_C) cmd/compiler_c_emit_value.zi
	touch $@

$(C_EMIT_STATEMENT_C)/.generated: cmd/compiler_c_emit_statement.zi cmd/compiler_c_emit_value.zi cmd/compiler_c_emit.zi cmd/compiler_ir.zi cmd/compiler_native_names.zi $(patsubst %,cmd/%.zi,$(RANGE_MODULES)) $(BOOTSTRAP_BIN)
	env -u DISPLAY -u WAYLAND_DISPLAY $(BOOTSTRAP_BIN) --no-main --root cmd --module-path std \
	    -o $(C_EMIT_STATEMENT_C) cmd/compiler_c_emit_statement.zi
	touch $@

$(C_EMIT_CALL_C)/.generated: cmd/compiler_c_emit_call.zi cmd/compiler_c_emit_value.zi cmd/compiler_c_emit.zi cmd/compiler_ir.zi cmd/compiler_native_names.zi $(patsubst %,cmd/%.zi,$(RANGE_MODULES)) $(BOOTSTRAP_BIN)
	env -u DISPLAY -u WAYLAND_DISPLAY $(BOOTSTRAP_BIN) --no-main --root cmd --module-path std \
	    -o $(C_EMIT_CALL_C) cmd/compiler_c_emit_call.zi
	touch $@

$(C_EMIT_EXPR_C)/.generated: cmd/compiler_c_emit_expr.zi cmd/compiler_c_emit_value.zi cmd/compiler_c_emit.zi cmd/compiler_ir.zi cmd/compiler_native_names.zi $(patsubst %,cmd/%.zi,$(RANGE_MODULES)) $(BOOTSTRAP_BIN)
	env -u DISPLAY -u WAYLAND_DISPLAY $(BOOTSTRAP_BIN) --no-main --root cmd --module-path std \
	    -o $(C_EMIT_EXPR_C) cmd/compiler_c_emit_expr.zi
	touch $@

$(SIMD_METADATA_C)/.generated: cmd/compiler_simd_metadata.zi cmd/compiler_ir.zi cmd/compiler_scalar.zi cmd/compiler_runtime_emit.zi $(BOOTSTRAP_BIN)
	env -u DISPLAY -u WAYLAND_DISPLAY $(BOOTSTRAP_BIN) --no-main --root cmd --module-path std \
	    -o $(SIMD_METADATA_C) cmd/compiler_simd_metadata.zi
	touch $@

$(C_EMIT_SIMD_C)/.generated: cmd/compiler_c_emit_simd.zi cmd/compiler_simd_metadata.zi cmd/compiler_c_emit_value.zi cmd/compiler_ir.zi cmd/compiler_native_names.zi $(patsubst %,cmd/%.zi,$(RANGE_MODULES)) $(BOOTSTRAP_BIN)
	env -u DISPLAY -u WAYLAND_DISPLAY $(BOOTSTRAP_BIN) --no-main --root cmd --module-path std \
	    -o $(C_EMIT_SIMD_C) cmd/compiler_c_emit_simd.zi
	touch $@

$(GO_SIMD_C)/.generated: cmd/compiler_go_simd.zi cmd/compiler_simd_metadata.zi cmd/compiler_ir.zi cmd/compiler_native_names.zi $(BOOTSTRAP_BIN)
	env -u DISPLAY -u WAYLAND_DISPLAY $(BOOTSTRAP_BIN) --no-main --root cmd --module-path std \
	    -o $(GO_SIMD_C) cmd/compiler_go_simd.zi
	touch $@

$(C_BACKEND_C)/.generated: cmd/compiler_c_backend.zi cmd/compiler_ir.zi cmd/compiler_native_names.zi cmd/compiler_c_emit.zi cmd/compiler_c_emit_value.zi cmd/compiler_c_emit_call.zi cmd/compiler_simd_metadata.zi cmd/compiler_runtime_emit.zi $(BOOTSTRAP_BIN)
	env -u DISPLAY -u WAYLAND_DISPLAY $(BOOTSTRAP_BIN) --no-main --root cmd --module-path std \
	    -o $(C_BACKEND_C) cmd/compiler_c_backend.zi
	touch $@

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

$(DECLARATION_C)/.generated: cmd/compiler_declaration.zi cmd/compiler_text.zi cmd/compiler_source.zi cmd/compiler_scan.zi $(BOOTSTRAP_BIN)
	env -u DISPLAY -u WAYLAND_DISPLAY $(BOOTSTRAP_BIN) --no-main --root cmd \
	    -o $(DECLARATION_C) cmd/compiler_declaration.zi
	touch $@

$(ENUM_C)/.generated: cmd/compiler_enum.zi cmd/compiler_text.zi $(BOOTSTRAP_BIN)
	env -u DISPLAY -u WAYLAND_DISPLAY $(BOOTSTRAP_BIN) --no-main --root cmd \
	    -o $(ENUM_C) cmd/compiler_enum.zi
	touch $@

$(TYPE_C)/.generated: cmd/compiler_type.zi cmd/compiler_text.zi $(BOOTSTRAP_BIN)
	env -u DISPLAY -u WAYLAND_DISPLAY $(BOOTSTRAP_BIN) --no-main --root cmd \
	    -o $(TYPE_C) cmd/compiler_type.zi
	touch $@

$(EXPRESSION_C)/.generated: cmd/compiler_expression.zi cmd/compiler_scan.zi cmd/compiler_source.zi cmd/compiler_text.zi $(BOOTSTRAP_BIN)
	env -u DISPLAY -u WAYLAND_DISPLAY $(BOOTSTRAP_BIN) --no-main --root cmd \
	    -o $(EXPRESSION_C) cmd/compiler_expression.zi
	touch $@

$(PARSE_C)/.generated: $(PARSE_COMPILER_SOURCES) $(BOOTSTRAP_BIN)
	env -u DISPLAY -u WAYLAND_DISPLAY $(BOOTSTRAP_BIN) --no-main --root cmd \
	    -o $(PARSE_C) cmd/compiler_parse.zi
	touch $@

$(STATEMENT_C)/.generated: cmd/compiler_statement.zi cmd/compiler_scan.zi cmd/compiler_source.zi cmd/compiler_text.zi $(BOOTSTRAP_BIN)
	env -u DISPLAY -u WAYLAND_DISPLAY $(BOOTSTRAP_BIN) --no-main --root cmd \
	    -o $(STATEMENT_C) cmd/compiler_statement.zi
	touch $@

$(PROOF_KERNEL_C)/.generated: cmd/compiler_proof_kernel.zi std/vec.zi $(BOOTSTRAP_BIN)
	env -u DISPLAY -u WAYLAND_DISPLAY $(BOOTSTRAP_BIN) --no-main --root cmd \
	    -o $(PROOF_KERNEL_C) cmd/compiler_proof_kernel.zi
	touch $@

$(PROOF_C)/.generated: cmd/compiler_proof.zi cmd/compiler_ir.zi cmd/compiler_services.zi cmd/compiler_proof_kernel.zi cmd/compiler_source.zi cmd/compiler_text.zi std/vec.zi $(BOOTSTRAP_BIN)
	env -u DISPLAY -u WAYLAND_DISPLAY $(BOOTSTRAP_BIN) --no-main --root cmd \
	    -o $(PROOF_C) cmd/compiler_proof.zi
	touch $@

$(ENGINE_C)/.generated: cmd/compiler_runtime.zi cmd/compiler_exec_link.zi cmd/compiler_exec_debug.zi cmd/compiler_exec.zi cmd/compiler_opt.zi cmd/compiler_opt_ssa.zi cmd/compiler_opt_rotate.zi cmd/compiler_opt_loop.zi cmd/compiler_opt_licm.zi cmd/compiler_opt_induction.zi cmd/compiler_opt_layout.zi cmd/compiler_opt_inline.zi cmd/compiler_opt_specialize.zi cmd/compiler_opt_thread.zi cmd/compiler_opt_accumulate.zi cmd/compiler_opt_dom.zi cmd/compiler_learning.zi cmd/compiler_learning_policy.zi cmd/compiler_learning_table.zi \
    cmd/compiler_exec_serial.zi cmd/compiler_lowered_verify.zi cmd/compiler_lowered_proven.zi cmd/compiler_lowered_ir.zi \
    cmd/compiler_bundle.zi std/math.zi std/finite.zi std/text_buffer.zi std/number_text.zi $(wildcard cmd/compiler_zx_wasm*.zi) $(BOOTSTRAP_BIN)
	env -u DISPLAY -u WAYLAND_DISPLAY $(BOOTSTRAP_BIN) --no-main --root cmd \
	    --define ZIRAN_EXEC_NATIVE_PLAN_INDEXES --define ZIRAN_EXEC_PACKED_INSTRUCTIONS \
	    -o $(ENGINE_C) cmd/compiler_runtime.zi
	touch $@

$(ENGINE_NAMES): $(ENGINE_C)/.generated scripts/engine-names.sh
	sh scripts/engine-names.sh $(ENGINE_C) $(ENGINE_MODULES) > $@

$(BUILD_DIR)/obj/engine/%.o: Makefile $(ENGINE_NAMES) $(BUILD_DIR)/.compiler-flags
	@mkdir -p $(dir $@)
	$(NICE) $(CC) $(CFLAGS) $(FRAMEFLAGS) -fno-math-errno -fvisibility=hidden -iquote $(ENGINE_C) -include $(ENGINE_NAMES) \
	    -DZIRAN_ENGINE_SOURCE_$* -c -o $@ $(ENGINE_C)/$*.c


$(BUNDLE_C)/.generated: cmd/compiler_bundle.zi $(BOOTSTRAP_BIN)
	env -u DISPLAY -u WAYLAND_DISPLAY $(BOOTSTRAP_BIN) --no-main --root cmd \
	    -o $(BUNDLE_C) cmd/compiler_bundle.zi
	touch $@

$(IR_C)/.generated: cmd/compiler_ir.zi $(BOOTSTRAP_BIN)
	env -u DISPLAY -u WAYLAND_DISPLAY $(BOOTSTRAP_BIN) --no-main --root cmd \
	    -o $(IR_C) cmd/compiler_ir.zi
	touch $@

$(LAW_C)/.generated: cmd/compiler_law.zi cmd/compiler_ir.zi cmd/compiler_services.zi cmd/compiler_text.zi $(BOOTSTRAP_BIN)
	env -u DISPLAY -u WAYLAND_DISPLAY $(BOOTSTRAP_BIN) --no-main --root cmd \
	    -o $(LAW_C) cmd/compiler_law.zi
	touch $@

$(SERVICES_C)/.generated: cmd/compiler_services.zi cmd/compiler_ir.zi $(BOOTSTRAP_BIN)
	env -u DISPLAY -u WAYLAND_DISPLAY $(BOOTSTRAP_BIN) --no-main --root cmd \
	    -o $(SERVICES_C) cmd/compiler_services.zi
	touch $@

$(LINK_C)/.generated: cmd/compiler_link.zi cmd/compiler_services.zi cmd/compiler_ir.zi cmd/compiler_scan.zi cmd/compiler_text.zi std/vec.zi $(BOOTSTRAP_BIN)
	env -u DISPLAY -u WAYLAND_DISPLAY $(BOOTSTRAP_BIN) --no-main --root cmd \
	    -o $(LINK_C) cmd/compiler_link.zi
	touch $@

.PHONY: check-bootstrap update-bootstrap
# The checked-in bootstrap C must match what the current compiler generates
# from cmd/compiler_*.zi; update-bootstrap refreshes it. A seed also keeps the
# generated headers of standard modules it uses, such as vec.h.
BOOTSTRAP_MODULES := scan text source declaration enum type expression statement proof_kernel proof bundle ir law services link runtime_emit backend_cache backend_process native_names lowered_ir zir_lower c_prune c_range
GENERATE_BOOTSTRAP = env -u DISPLAY -u WAYLAND_DISPLAY $(BIN_DIR)/zi2c --no-main --root cmd \
    -o $(BUILD_DIR)/bootstrap-check cmd/compiler_$$module.zi
BOOTSTRAP_MODULES += parse portable_body scalar c_emit c_emit_value c_emit_statement c_emit_call c_emit_expr simd_metadata c_emit_simd go_simd c_backend
BOOTSTRAP_MODULES += $(GO_PORT_MODULES) $(CHECK_PORT_MODULES) $(PARSE_PORT_MODULES) $(FRONTEND_PORT_MODULES) host_api
# Darwin's seed copies of the platform modules, generated with the defines a
# Darwin host passes; one copy serves Apple Silicon and Intel.
DARWIN_BOOTSTRAP_MODULES := runtime_emit backend_cache backend_process
GENERATE_DARWIN_BOOTSTRAP = env -u DISPLAY -u WAYLAND_DISPLAY $(BIN_DIR)/zi2c --no-main \
    --target-os macos --define __APPLE__ --define __aarch64__ --root cmd \
    -o $(BUILD_DIR)/bootstrap-check-darwin cmd/compiler_$$module.zi
# The SIMD header generator's seed serves every platform; it compiles against
# include/'s runtime headers, so it keeps none of zi2c's copies of them.
GENERATE_SIMD_BOOTSTRAP = rm -rf $(BUILD_DIR)/bootstrap-check-simd && env -u DISPLAY -u WAYLAND_DISPLAY \
    $(BIN_DIR)/zi2c --root scripts/simd -o $(BUILD_DIR)/bootstrap-check-simd scripts/simd/header.zi
check-bootstrap: $(BIN_DIR)/zi2c
	@set -e; for module in $(BOOTSTRAP_MODULES); do \
	    $(GENERATE_BOOTSTRAP); \
	    for file in $$(find bootstrap/compiler_$$module -maxdepth 1 -type f ! -name '*.o' ! -name '*.d' ! -name '.build.lock' -printf '%f\n' | sort); do \
	        cmp bootstrap/compiler_$$module/$$file $(BUILD_DIR)/bootstrap-check/$$file; \
	    done; \
	done; \
	for module in $(DARWIN_BOOTSTRAP_MODULES); do \
	    $(GENERATE_DARWIN_BOOTSTRAP); \
	    for file in $$(find bootstrap/compiler_$$module -maxdepth 1 -type f ! -name '*.o' ! -name '*.d' ! -name '.build.lock' -printf '%f\n' | sort); do \
	        cmp bootstrap/darwin/compiler_$$module/$$file $(BUILD_DIR)/bootstrap-check-darwin/$$file; \
	    done; \
	done; \
	$(GENERATE_SIMD_BOOTSTRAP); \
	diff -rq -x 'zir_*.h' -x .build.lock bootstrap/simd $(BUILD_DIR)/bootstrap-check-simd; \
	echo "bootstrap matches: $(BOOTSTRAP_MODULES); darwin: $(DARWIN_BOOTSTRAP_MODULES); simd"

update-bootstrap: $(BIN_DIR)/zi2c
	@set -e; for module in $(BOOTSTRAP_MODULES); do \
	    $(GENERATE_BOOTSTRAP); \
	    files="compiler_$$module.c compiler_$$module.h"; \
	    case "$$module" in \
	        proof_kernel|proof|link) files="$$files vec.h";; \
	        c_range) files="$(foreach m,$(RANGE_MODULES),$(m).c $(m).h)";; \
	        runtime_emit) files="compiler_runtime_emit.c compiler_runtime_emit.h \
	            file.c file.h file_linux.c file_linux.h \
	            c_string.c c_string.h result.h vec.c vec.h";; \
	        backend_process) files="compiler_backend_process.c compiler_backend_process.h \
	            byte_text_linux.c byte_text_linux.h environment.h environment_linux.c environment_linux.h \
	            file_tree_linux.c file_tree_linux.h number_text.c number_text.h \
	            text_buffer.c text_buffer.h \
	            process_run_linux.c process_run_linux.h \
	            sort.c sort.h text_tools.c text_tools.h timer_linux.c timer_linux.h";; \
	    esac; \
	    for file in $$files; do \
	        cp $(BUILD_DIR)/bootstrap-check/$$file bootstrap/compiler_$$module/; \
	    done; \
	done; \
	for module in $(DARWIN_BOOTSTRAP_MODULES); do \
	    $(GENERATE_DARWIN_BOOTSTRAP); \
	    mkdir -p bootstrap/darwin/compiler_$$module; \
	    for file in $$(find bootstrap/compiler_$$module -maxdepth 1 -type f ! -name '*.o' ! -name '*.d' ! -name '.build.lock' -printf '%f\n'); do \
	        cp $(BUILD_DIR)/bootstrap-check-darwin/$$file bootstrap/darwin/compiler_$$module/; \
	    done; \
	done; \
	$(GENERATE_SIMD_BOOTSTRAP); \
	rm -rf bootstrap/simd; mkdir bootstrap/simd; \
	cp $(BUILD_DIR)/bootstrap-check-simd/*.c $(BUILD_DIR)/bootstrap-check-simd/*.h bootstrap/simd/; \
	rm -f bootstrap/simd/zir_*.h

endif

$(BIN_DIR)/bundle-link-test: $(call obj,tests/bundle_link_test.c) $(FRONTEND_OBJECTS) $(BUNDLE_OBJECT) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(RUNTIME_LIBS)

$(BIN_DIR)/host-capability-test: tests/host_capability_test.c $(BUILD_DIR)/libziran.a | $(BIN_DIR)
	$(CC) $(SANITIZE_FLAGS) -D_GNU_SOURCE -std=c11 -Iinclude -Icmd/zir -o $@ tests/host_capability_test.c $(BUILD_DIR)/libziran.a $(RUNTIME_LIBS)

$(BIN_DIR)/record-host-test: tests/record_host_test.c $(BUILD_DIR)/libziran.a | $(BIN_DIR)
	$(CC) $(SANITIZE_FLAGS) -D_GNU_SOURCE -std=c11 -Iinclude -o $@ tests/record_host_test.c $(BUILD_DIR)/libziran.a $(RUNTIME_LIBS)

$(BIN_DIR)/record-fields-by-name-test: tests/record_fields_by_name_test.c $(BUILD_DIR)/libziran.a | $(BIN_DIR)
	$(CC) $(SANITIZE_FLAGS) -D_GNU_SOURCE -std=c11 -Iinclude -o $@ tests/record_fields_by_name_test.c $(BUILD_DIR)/libziran.a $(RUNTIME_LIBS)

$(BIN_DIR)/array-host-test: tests/host_array_test.c $(BUILD_DIR)/libziran.a | $(BIN_DIR)
	$(CC) $(SANITIZE_FLAGS) -D_GNU_SOURCE -std=c11 -Iinclude -o $@ tests/host_array_test.c $(BUILD_DIR)/libziran.a $(RUNTIME_LIBS)

$(BIN_DIR)/startup-graph-test: tests/startup_graph_test.c $(BUILD_DIR)/libziran.a | $(BIN_DIR)
	$(CC) $(SANITIZE_FLAGS) -D_GNU_SOURCE -std=c11 -Iinclude -Icmd/zir -I$(IR_C) -I$(NATIVE_NAMES_C) -o $@ \
		tests/startup_graph_test.c $(BUILD_DIR)/libziran.a $(RUNTIME_LIBS)

$(BIN_DIR)/process-host-test: tests/process_host_test.c $(BUILD_DIR)/libziran.a | $(BIN_DIR)
	$(CC) $(SANITIZE_FLAGS) -D_GNU_SOURCE -std=c11 -Iinclude -o $@ tests/process_host_test.c $(BUILD_DIR)/libziran.a $(RUNTIME_LIBS)

$(BIN_DIR)/slice-host-test: tests/host_slice_test.c $(BUILD_DIR)/libziran.a | $(BIN_DIR)
	$(CC) $(SANITIZE_FLAGS) -D_GNU_SOURCE -std=c11 -Iinclude -o $@ tests/host_slice_test.c $(BUILD_DIR)/libziran.a $(RUNTIME_LIBS)

$(BIN_DIR)/native-wx-test: tests/native_wx_test.c cmd/zir/zir_native_memory.h $(BUILD_DIR)/libziran.a | $(BIN_DIR)
	$(CC) $(SANITIZE_FLAGS) -D_GNU_SOURCE -std=c11 -Icmd/zir -o $@ tests/native_wx_test.c $(BUILD_DIR)/libziran.a $(RUNTIME_LIBS)

$(BIN_DIR)/native-load-test: tests/native_load_test.c $(BUILD_DIR)/libziran.a | $(BIN_DIR)
	$(CC) $(SANITIZE_FLAGS) -D_GNU_SOURCE -std=c11 -Iinclude -o $@ tests/native_load_test.c $(BUILD_DIR)/libziran.a $(RUNTIME_LIBS)

CHECK_RUNNER_SOURCES := tests/run_check.zi tests/check_support.zi tests/check_plan.zi \
    tests/check_go.zi tests/check_cache.zi tests/check_runner_test.zi
CHECK_RUNNER_C := $(BUILD_DIR)/check-runner-c
check_quote = '$(subst ','"'"',$(1))'

$(BIN_DIR)/check-runner: $(CHECK_RUNNER_SOURCES) $(wildcard std/*.zi) $(BIN_DIR)/zi2c | $(BIN_DIR)
	$(BIN_DIR)/zi2c --entry run_check:main $(ZIRAN_DEFINES) --root tests --module-path std \
	    -o $(CHECK_RUNNER_C) tests/run_check.zi
	$(NICE) $(CC) $(CFLAGS) $(SANITIZE_FLAGS) -I$(CHECK_RUNNER_C) -o $@ $(CHECK_RUNNER_C)/*.c -lm

check: all $(BIN_DIR)/check-runner $(BIN_DIR)/bundle-link-test $(BIN_DIR)/host-capability-test $(BIN_DIR)/record-host-test $(BIN_DIR)/record-fields-by-name-test $(BIN_DIR)/array-host-test $(BIN_DIR)/slice-host-test $(BIN_DIR)/process-host-test $(BIN_DIR)/startup-graph-test $(BIN_DIR)/native-wx-test $(BIN_DIR)/native-load-test
	env -u DISPLAY -u WAYLAND_DISPLAY $(BIN_DIR)/startup-graph-test
	$(BIN_DIR)/check-runner --bin-dir $(BIN_DIR) $(if $(CHECK_JOBS),--jobs $(CHECK_JOBS)) \
	    $(if $(CHECK_FILTER),--filter $(call check_quote,$(CHECK_FILTER)))

zx-wasm-test: all
	env -u DISPLAY -u WAYLAND_DISPLAY tests/zx_wasm.sh $(BIN_DIR)/ziran

# Optional decoder for std/audio_mp3; no SDL or other UI dependency.
.PHONY: audio-mp3
audio-mp3:
	$(CMAKE) -S std/native -B $(BUILD_DIR)/audio/native -DCMAKE_C_COMPILER=$(CC)
	$(CMAKE) --build $(BUILD_DIR)/audio/native --target ziran_mp3

# Native ZX on an Android device: ANDROID_ARCH is aarch64 or armv7.
# android-build cross-builds libziran.a and zi2zx with the NDK for
# ANDROID_API; the compilers that run during that build are this host's.
# android-parity runs the revision-28 parity bundles on ANDROID_SERIAL, on
# its engine and natively, against this host's engine and writes
# ANDROID_PARITY_RECEIPT. ANDROID_ADB_HOST is the ssh destination whose adb
# reaches the device, or empty for this host's adb.
ANDROID_NDK ?= $(ANDROID_NDK_HOME)
ANDROID_ARCH ?= aarch64
ANDROID_API ?= 33
ANDROID_SERIAL ?=
ANDROID_ADB_HOST ?=
ANDROID_BUILD_DIR ?= $(BUILD_DIR)/android-$(ANDROID_ARCH)
ANDROID_PARITY_RECEIPT ?= $(BUILD_DIR)/android-parity.json
ANDROID_TOOLCHAIN = $(ANDROID_NDK)/toolchains/llvm/prebuilt/linux-x86_64/bin
ANDROID_TRIPLE_aarch64 = aarch64-linux-android
ANDROID_TRIPLE_armv7 = armv7a-linux-androideabi
ANDROID_OUT = $(abspath $(ANDROID_BUILD_DIR))
# Real-device streaming PCM test, separate from native ZX parity.
.PHONY: audio-android-test
audio-android-test: $(BIN_DIR)/ziran
	@test -n "$(ANDROID_SERIAL)" || { echo 'set ANDROID_SERIAL' >&2; exit 1; }
	env -u DISPLAY -u WAYLAND_DISPLAY $(BIN_DIR)/ziran run --target=c --root tests tests/audio_android_cases.zi -- \
	    $(abspath $(BIN_DIR)/ziran) $(BUILD_DIR)/audio/android-test '$(ANDROID_NDK)' '$(ANDROID_SERIAL)'

.PHONY: android-build android-parity
android-build: $(BIN_DIR)/zi2c
	@test -n "$(ANDROID_NDK)" || { echo 'set ANDROID_NDK or ANDROID_NDK_HOME' >&2; exit 1; }
	@test -n "$(ANDROID_TRIPLE_$(ANDROID_ARCH))" || { echo 'ANDROID_ARCH is aarch64 or armv7' >&2; exit 1; }
	$(MAKE) BUILD_DIR=$(ANDROID_OUT) CC=$(ANDROID_TOOLCHAIN)/$(ANDROID_TRIPLE_$(ANDROID_ARCH))$(ANDROID_API)-clang \
	    AR=$(ANDROID_TOOLCHAIN)/llvm-ar OBJCOPY=$(ANDROID_TOOLCHAIN)/llvm-objcopy \
	    GEN_ZI2C=$(abspath $(BIN_DIR)/zi2c) $(ANDROID_OUT)/libziran.a $(ANDROID_OUT)/bin/zi2zx

android-parity: $(BIN_DIR)/ziran $(BIN_DIR)/zi2zx android-build
	@test -n "$(ANDROID_SERIAL)" || { echo 'set ANDROID_SERIAL' >&2; exit 1; }
	env -u DISPLAY -u WAYLAND_DISPLAY $(BIN_DIR)/ziran run --target=c --root tests tests/zx_android_parity_cases.zi -- \
	    $(abspath $(BIN_DIR)/ziran) '' $(ANDROID_OUT)/bin '$(ANDROID_SERIAL)' '$(ANDROID_ADB_HOST)' $(ANDROID_PARITY_RECEIPT)

# Address and undefined-behavior sanitizers over the whole toolchain and
# test suite. It also checks every kept type/procedure lookup against a fresh one
# (ZIRAN_VERIFY_SYMBOL_LOOKUPS), so a change that forgets SymbolLookupsChanged
# fails here. Instrumentation grows stack frames past the frame limit, so
# the limit is off for this build. The compiler frees little on exit by
# design, so leak reports are off. Tests that link libziran.a add
# VM_CFLAGS to their compile.
SANITIZE_DIR ?= build/sanitize
SANITIZE_OPTIONS = -g -fsanitize=address,undefined \
    -fno-sanitize-recover=undefined -fno-omit-frame-pointer
SANITIZE_ENV = ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=print_stacktrace=1 \
    ZIRAN_VERIFY_SYMBOL_LOOKUPS=1 VM_CFLAGS="$(SANITIZE_OPTIONS)"
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

curl-http-test: $(BIN_DIR)/ziran $(BIN_DIR)/zi2c
	$(BIN_DIR)/ziran run --target=c --root tests tests/net_http_curl_cases.zi -- $(BIN_DIR)/zi2c

.PHONY: gzip-test
gzip-test: $(BIN_DIR)/ziran $(BIN_DIR)/zi2c
	$(BIN_DIR)/ziran run --target=c --root tests tests/gzip_cases.zi -- $(BIN_DIR)/zi2c

clean:
	rm -rf $(BUILD_DIR)

include assembly.mk

# Explicit MinGW/Wine gate; window setup is confined to private Xvfb.
.PHONY: audio-windows-test
audio-windows-test: all
	env -u DISPLAY -u WAYLAND_DISPLAY $(BIN_DIR)/ziran run --target=c --root tests tests/audio_windows_cases.zi -- $(abspath $(BIN_DIR)/ziran) $(abspath $(BUILD_DIR)/audio/windows)

# Headless consumers can reuse both core native codec archives.
.PHONY: audio-codecs
audio-codecs:
	$(CMAKE) -S std/native -B $(BUILD_DIR)/audio/native -DCMAKE_C_COMPILER=$(CC) -DZIRAN_AUDIO_VORBIS=ON
	$(CMAKE) --build $(BUILD_DIR)/audio/native --target ziran_mp3 vorbisfile
