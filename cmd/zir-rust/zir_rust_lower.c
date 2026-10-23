#include "zir_rust_internal.h"

static FILE *open_output(ZirSourceSpan span, const char *directory,
                         const char *relative)
{
    char path[1024];
    char parent[1024];
    FILE *file;
    snprintf(path, sizeof(path), "%s/%s", directory, relative);
    snprintf(parent, sizeof(parent), "%s", path);
    char *separator = strrchr(parent, '/');
    if(separator != NULL)
        *separator = '\0';
    make_directories(parent);
    file = fopen(path, "wb");
    if(file == NULL) {
        Diagnostic(span, "zir_rust.output",
                   "cannot create Rust output: %s", path);
        exit(1);
    }
    return file;
}

/* Buffers rust_lower keeps on the heap so deep nesting fits the stack;
 * freed blocks are kept for reuse, one per nesting level. */
typedef struct RustLowerBuffers {
    RustEmitter emitter;
    char symbol[ZIR_RUST_NAME_MAX * 2];
} RustLowerBuffers;

int rust_lower(const ZirProgram *const *programs, int program_count,
               const char *output_directory, const char *entry_module,
               const char *entry_function, int executable);

static int
rust_lower_with_buffers(const ZirProgram *const *programs, int program_count,
               const char *output_directory, const char *entry_module,
               const char *entry_function, int executable, RustLowerBuffers *buffers)
{
    FILE *output;
    FILE *cargo;
    memset(&buffers->emitter, 0, sizeof(buffers->emitter));
    const ZirModule *entry_owner = NULL;
    const ZirFunction *entry = NULL;
    if(programs == NULL || program_count <= 0 || output_directory == NULL ||
       output_directory[0] == '\0') {
        Diagnostic(Span("<command>", 1, 1), "zir_rust.input",
                   "no checked programs were supplied");
        return 1;
    }
    for(int program_index = 0; program_index < program_count; program_index++)
        for(int module_index = 0;
            module_index < programs[program_index]->module_count;
            module_index++)
            validate_module(&programs[program_index]->modules[module_index]);
    if(executable) {
        for(int program_index = 0; program_index < program_count; program_index++)
            for(int module_index = 0;
                module_index < programs[program_index]->module_count;
                module_index++) {
                const ZirModule *module =
                    &programs[program_index]->modules[module_index];
                if(strcmp(module->name, entry_module) != 0)
                    continue;
                for(int function_index = 0;
                    function_index < module->function_count; function_index++)
                    if(strcmp(module->functions[function_index].name,
                              entry_function) == 0) {
                        entry_owner = module;
                        entry = &module->functions[function_index];
                    }
            }
        if(entry == NULL) {
            Diagnostic(Span("<command>", 1, 1), "zir_rust.entry",
                       "executable entry is missing: %s:%s", entry_module,
                       entry_function);
            return 1;
        }
        if(FunctionArgs(entry)[0] || (strcmp(entry->return_type, "void") != 0 &&
           !integer_type(entry->return_type) &&
           strcmp(entry->return_type, "bool") != 0)) {
            Diagnostic(entry->span, "zir_rust.entry",
                       "executable entry must take no arguments and return void, bool, or an integer");
            return 1;
        }
    }
    output = open_output(programs[0]->modules[0].span, output_directory,
                         executable ? "src/main.rs" : "src/lib.rs");
    cargo = open_output(programs[0]->modules[0].span, output_directory,
                        "Cargo.toml");
    fprintf(cargo,
            "[package]\n"
            "name = \"ziran_generated\"\n"
            "version = \"0.1.0\"\n"
            "edition = \"2021\"\n"
            "publish = false\n\n"
            "[profile.dev]\npanic = \"abort\"\n\n"
            "[profile.release]\npanic = \"abort\"\n");
    fclose(cargo);
    /* Keep native link flags with the generated Cargo project. */
    FILE *build_script = open_output(programs[0]->modules[0].span,
                                    output_directory, "build.rs");
    fputs("fn main() {\n", build_script);
    fputs("    println!(\"cargo:rustc-check-cfg=cfg(ziran_no_asm)\");\n", build_script);
    const char *link_flags[] = {getenv("LDFLAGS"), getenv("LDLIBS")};
    for(int i = 0; i < 2; i++) {
        if(link_flags[i] == NULL) continue;
        char *words = strdup(link_flags[i]), *save = NULL;
        if(words == NULL) exit(1);
        for(char *word = strtok_r(words, " \t\r\n", &save); word;
            word = strtok_r(NULL, " \t\r\n", &save)) {
            fputs("    println!(\"cargo:rustc-link-arg={}\", \"", build_script);
            for(const char *p = word; *p; p++) {
                if(*p == '"' || *p == '\\') fputc('\\', build_script);
                fputc(*p, build_script);
            }
            fputs("\");\n", build_script);
        }
        free(words);
    }
    fputs("}\n", build_script);
    fclose(build_script);
    char *runtime_text = NULL, *body_text = NULL;
    size_t runtime_size = 0, body_size = 0;
    TextStream runtime_stream, body_stream;
    FILE *runtime = TextStreamOpen(&runtime_stream, &runtime_text, &runtime_size);
    FILE *body = TextStreamOpen(&body_stream, &body_text, &body_size);
    if(runtime == NULL || body == NULL) {
        if(runtime != NULL) TextStreamClose(&runtime_stream);
        if(body != NULL) TextStreamClose(&body_stream);
        free(runtime_text);
        free(body_text);
        Diagnostic(Span(output_directory, 1, 1), "zir_rust.output",
                   "cannot buffer Rust output");
        return 1;
    }
    buffers->emitter.output = body;
    buffers->emitter.programs = programs;
    buffers->emitter.program_count = program_count;
    fputs("pub fn ZiranNew<T>() -> *mut T {\n"
          "    let layout = std::alloc::Layout::new::<T>();\n"
          "    let layout = std::alloc::Layout::from_size_align(layout.size().max(1), layout.align()).unwrap();\n"
          "    unsafe { std::alloc::alloc_zeroed(layout) as *mut T }\n"
          "}\n\n"
          "pub unsafe fn ZiranFree<T>(pointer: *mut T) {\n"
          "    if !pointer.is_null() {\n"
          "        let layout = std::alloc::Layout::new::<T>();\n"
          "        let layout = std::alloc::Layout::from_size_align(layout.size().max(1), layout.align()).unwrap();\n"
          "        std::alloc::dealloc(pointer as *mut u8, layout);\n"
          "    }\n"
          "}\n\n", runtime);
    fputs("#[repr(C)]\npub struct ZiranSlice<T> {\n"
          "    pub data: *mut T,\n    pub len: usize,\n}\n\n"
          "impl<T> Clone for ZiranSlice<T> {\n"
          "    fn clone(&self) -> Self { *self }\n"
          "}\n\n"
          "impl<T> Copy for ZiranSlice<T> {}\n\n"
          "impl<T> ZiranSlice<T> {\n"
          "    pub fn view(source: Self, low: i64, high: i64) -> Self {\n"
          "        assert!(low >= 0 && low <= high && high as usize <= source.len, \"slice range out of bounds\");\n"
          "        Self { data: if low == 0 { source.data } else { unsafe { source.data.offset(low as isize) } }, len: (high - low) as usize }\n"
          "    }\n"
          "    pub fn view_from(source: Self, low: i64) -> Self {\n"
          "        Self::view(source, low, source.len as i64)\n"
          "    }\n"
          "    pub fn element(self, index: i64) -> *mut T {\n"
          "        assert!(index >= 0 && (index as usize) < self.len, \"slice index out of bounds\");\n"
          "        unsafe { self.data.offset(index as isize) }\n"
          "    }\n"
          "    pub fn get(self, index: i64) -> T {\n"
          "        unsafe { self.element(index).read() }\n"
          "    }\n"
          "}\n\n", runtime);
    emit_ziran_vec_runtime(runtime);
    fputs("#[repr(C)]\n#[derive(Clone, Copy)]\npub struct Source_Code_Location {\n"
          "    pub fully_pathed_filename: ZiranText,\n    pub line_number: i64,\n}\n\n", runtime);
    fputs("#[repr(C)]\n#[derive(Clone, Copy)]\npub struct ZiranText {\n"
          "    pub data: *const u8,\n    pub len: usize,\n"
          "}\n\n"
          "unsafe impl Sync for ZiranText {}\n\n"
          "impl ZiranText {\n"
          "    pub const fn new(value: &'static str) -> Self {\n"
          "        Self { data: value.as_ptr(), len: value.len() }\n"
          "    }\n"
          "    pub fn eq(left: Self, right: Self) -> bool {\n"
          "        if left.len != right.len { return false; }\n"
          "        if left.len == 0 { return true; }\n"
          "        unsafe {\n"
          "            core::slice::from_raw_parts(left.data, left.len) ==\n"
          "            core::slice::from_raw_parts(right.data, right.len)\n"
          "        }\n"
          "    }\n"
          "    pub fn as_str(self) -> std::borrow::Cow<'static, str> {\n"
          "        if self.len == 0 { return std::borrow::Cow::Borrowed(\"\"); }\n"
          "        String::from_utf8_lossy(unsafe { core::slice::from_raw_parts(self.data, self.len) })\n"
          "    }\n"
          "    pub fn at(self, index: i64) -> u8 {\n"
          "        assert!(index >= 0 && (index as usize) < self.len, \"string index out of bounds\");\n"
          "        unsafe { *self.data.offset(index as isize) }\n"
          "    }\n"
          "    pub fn slice(value: Self, low: isize, high: isize) -> Self {\n"
          "        assert!(low >= 0 && low <= high && high as usize <= value.len);\n"
          "        Self { data: if low == 0 { value.data } else { unsafe { value.data.offset(low) } }, len: (high - low) as usize }\n"
          "    }\n"
          "    pub fn slice_from(value: Self, low: isize) -> Self {\n"
          "        Self::slice(value, low, value.len as isize)\n"
          "    }\n"
          "}\n\n", runtime);
    emit_type_definitions(&buffers->emitter, body);
    emit_extern_definitions(&buffers->emitter, body);
    emit_global_definitions(&buffers->emitter, body);
    for(int program_index = 0; program_index < program_count; program_index++) {
        const ZirProgram *program = programs[program_index];
        for(int module_index = 0; module_index < program->module_count;
            module_index++) {
            const ZirModule *module = &program->modules[module_index];
            if(program_count != 1 || programs[0]->module_count != 1)
                fprintf(body,
                        "// Code generated by zi2rust from %s. DO NOT EDIT.\n",
                        module->source_path);
            for(int function_index = 0; function_index < module->function_count;
                function_index++)
                lower_function(&buffers->emitter, module,
                               &module->functions[function_index]);
        }
    }
    int has_startup = emit_startup(&buffers->emitter, body);
    if(executable) {
        function_symbol(&buffers->emitter, entry_owner, entry, buffers->symbol, sizeof(buffers->symbol));
        fputs("fn main() {\n", body);
        if(has_startup)
            fputs("    ziran_startup();\n", body);
        if(strcmp(entry->return_type, "void") == 0) {
            fprintf(body, "    %s();\n", buffers->symbol);
        } else if(strcmp(entry->return_type, "bool") == 0) {
            fprintf(body, "    let failed = %s();\n", buffers->symbol);
            fprintf(body, "    if failed { std::process::exit(1); }\n");
        } else {
            fprintf(body, "    let code = %s() as i32;\n", buffers->symbol);
            fprintf(body, "    std::process::exit(code);\n");
        }
        fputs("}\n", body);
    }
    char *kept_text = NULL;
    size_t kept_size = 0;
    TextStream kept_stream;
    FILE *kept = TextStreamOpen(&kept_stream, &kept_text, &kept_size);
    int buffer_failed = kept == NULL;
    if(TextStreamClose(&runtime_stream) != 0) buffer_failed = 1;
    if(TextStreamClose(&body_stream) != 0) buffer_failed = 1;
    if(!buffer_failed && !write_used_runtime(kept, runtime_text, runtime_size, body_text, body_size))
        buffer_failed = 1;
    if(kept != NULL && TextStreamClose(&kept_stream) != 0) buffer_failed = 1;
    if(buffer_failed) {
        free(runtime_text);
        free(body_text);
        free(kept_text);
        Diagnostic(Span(output_directory, 1, 1), "zir_rust.output",
                   "cannot buffer Rust output");
        return 1;
    }
    if(program_count == 1 && programs[0]->module_count == 1)
        fprintf(output, "// Code generated by zi2rust from %s. DO NOT EDIT.\n",
                programs[0]->modules[0].source_path);
    write_rust_lint_allowances(output, kept_text, kept_size, body_text, body_size,
                               executable);
    fwrite(kept_text, 1, kept_size, output);
    fwrite(body_text, 1, body_size, output);
    free(kept_text);
    free(runtime_text);
    free(body_text);
    if(fclose(output) != 0) {
        Diagnostic(Span(output_directory, 1, 1), "zir_rust.output",
                   "cannot finish Rust output");
        return 1;
    }
    return 0;
}

static int
rust_lower_in_context(const ZirProgram *const *programs, int program_count,
               const char *output_directory, const char *entry_module,
               const char *entry_function, int executable)
{
    static const char scratch_key;
    static _Thread_local CompilerScratchCache scratch_cache;
    CompilerScratch *scratch_hint = CompilerScratchCached(&scratch_cache,
        &scratch_key, free);
    RustLowerBuffers *buffers = CompilerScratchTake(scratch_hint);
    if(!buffers) buffers = AllocateOrExit(sizeof(*buffers));
    uint64_t profile_started = ProfileStart();
    int returned = rust_lower_with_buffers(programs, program_count, output_directory, entry_module, entry_function, executable, buffers);
    ProfileEnd("emit.rust", profile_started);
    if(!CompilerScratchPut(scratch_hint, buffers))
        free(buffers);
    return returned;
}

int rust_lower(const ZirProgram *const *programs, int program_count, const char *output_directory, const char *entry_module, const char *entry_function, int executable)
{
    CompilerContext *previous = CompilerContextSelect(program_count > 0 && programs && programs[0] ? programs[0]->context : NULL);
    int result = rust_lower_in_context(programs, program_count, output_directory, entry_module, entry_function, executable);
    CompilerContextSelect(previous);
    return result;
}
