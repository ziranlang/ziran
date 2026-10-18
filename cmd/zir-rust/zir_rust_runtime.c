#include "zir_rust_internal.h"

void emit_ziran_vec_runtime(FILE *output)
{
    fputs("#[repr(C)]\n", output);
    fputs("pub struct ZiranOption<T> {\n", output);
    fputs("    pub has_value: bool,\n", output);
    fputs("    pub value: T,\n", output);
    fputs("}\n", output);
    fputs("\n", output);
    fputs("#[repr(C)]\n", output);
    fputs("pub struct ZiranVec<T> {\n", output);
    fputs("    pub data: *mut T,\n", output);
    fputs("    pub count: i64,\n", output);
    fputs("    pub capacity: i64,\n", output);
    fputs("}\n", output);
    fputs("\n", output);
    fputs("unsafe impl<T> Sync for ZiranVec<T> {}\n", output);
    fputs("\n", output);
    fputs("impl<T> ZiranVec<T> {\n", output);
    fputs("    pub const fn new() -> Self {\n", output);
    fputs("        Self { data: core::ptr::null_mut(), count: 0, capacity: 0 }\n", output);
    fputs("    }\n", output);
    fputs("    pub fn element(&self, index: i64) -> *mut T {\n", output);
    fputs("        assert!(index >= 0 && index < self.count, \"Vec index out of bounds\");\n", output);
    fputs("        unsafe { self.data.offset(index as isize) }\n", output);
    fputs("    }\n", output);
    fputs("    pub fn at(&self, index: i64) -> T {\n", output);
    fputs("        unsafe { self.element(index).read() }\n", output);
    fputs("    }\n", output);
    fputs("}\n", output);
    fputs("\n", output);
    fputs("impl<T> Drop for ZiranVec<T> {\n", output);
    fputs("    fn drop(&mut self) {\n", output);
    fputs("        ZiranVecFree(self);\n", output);
    fputs("    }\n", output);
    fputs("}\n", output);
    fputs("\n", output);
    fputs("pub fn ZiranVecPush<T>(vector: &mut ZiranVec<T>, value: T) -> bool {\n", output);
    fputs("    if vector.count == vector.capacity {\n", output);
    fputs("        let capacity = if vector.capacity == 0 {\n", output);
    fputs("            4\n", output);
    fputs("        } else {\n", output);
    fputs("            match (vector.capacity as usize).checked_mul(2) {\n", output);
    fputs("                Some(capacity) if capacity <= i64::MAX as usize => capacity,\n", output);
    fputs("                _ => return false,\n", output);
    fputs("            }\n", output);
    fputs("        };\n", output);
    fputs("        let layout = match std::alloc::Layout::array::<T>(capacity) {\n", output);
    fputs("            Ok(layout) => layout,\n", output);
    fputs("            Err(_) => return false,\n", output);
    fputs("        };\n", output);
    fputs("        let old_size = vector.capacity as usize * core::mem::size_of::<T>();\n", output);
    fputs("        let allocation = if vector.data.is_null() {\n", output);
    fputs("            unsafe { std::alloc::alloc(layout) }\n", output);
    fputs("        } else {\n", output);
    fputs("            let pointer = vector.data as *mut u8;\n", output);
    fputs("            let old_layout = match std::alloc::Layout::from_size_align(\n", output);
    fputs("                old_size, layout.align()) {\n", output);
    fputs("                Ok(layout) => layout,\n", output);
    fputs("                Err(_) => return false,\n", output);
    fputs("            };\n", output);
    fputs("            unsafe { std::alloc::realloc(pointer, old_layout, layout.size()) }\n", output);
    fputs("        };\n", output);
    fputs("        if allocation.is_null() {\n", output);
    fputs("            return false;\n", output);
    fputs("        }\n", output);
    fputs("        vector.data = allocation as *mut T;\n", output);
    fputs("        vector.capacity = capacity as i64;\n", output);
    fputs("    }\n", output);
    fputs("    unsafe { vector.data.offset(vector.count as isize).write(value); }\n", output);
    fputs("    vector.count += 1;\n", output);
    fputs("    true\n", output);
    fputs("}\n", output);
    fputs("\n", output);
    fputs("pub fn ZiranVecClone<T>(destination: &mut ZiranVec<T>, source: &ZiranVec<T>) -> bool {\n", output);
    fputs("    ZiranVecFree(destination);\n", output);
    fputs("    if source.count == 0 {\n", output);
    fputs("        return true;\n", output);
    fputs("    }\n", output);
    fputs("    let count = source.count as usize;\n", output);
    fputs("    let layout = match std::alloc::Layout::array::<T>(count) {\n", output);
    fputs("        Ok(layout) => layout,\n", output);
    fputs("        Err(_) => return false,\n", output);
    fputs("    };\n", output);
    fputs("    let allocation = unsafe { std::alloc::alloc(layout) };\n", output);
    fputs("    if allocation.is_null() {\n", output);
    fputs("        return false;\n", output);
    fputs("    }\n", output);
    fputs("    unsafe { core::ptr::copy_nonoverlapping(source.data, allocation as *mut T, count); }\n", output);
    fputs("    destination.data = allocation as *mut T;\n", output);
    fputs("    destination.count = source.count;\n", output);
    fputs("    destination.capacity = source.count;\n", output);
    fputs("    true\n", output);
    fputs("}\n", output);
    fputs("\n", output);
    fputs("pub fn ZiranVecPop<T>(vector: &mut ZiranVec<T>) -> ZiranOption<T> {\n", output);
    fputs("    if vector.count == 0 {\n", output);
    fputs("        return ZiranOption {\n", output);
    fputs("            has_value: false,\n", output);
    fputs("            value: unsafe { core::mem::zeroed() },\n", output);
    fputs("        };\n", output);
    fputs("    }\n", output);
    fputs("    vector.count -= 1;\n", output);
    fputs("    let value = unsafe { vector.data.offset(vector.count as isize).read() };\n", output);
    fputs("    ZiranOption { has_value: true, value }\n", output);
    fputs("}\n", output);
    fputs("\n", output);
    fputs("pub fn ZiranVecGet<T>(vector: &mut ZiranVec<T>, index: usize) -> ZiranOption<T> {\n", output);
    fputs("    if index >= vector.count as usize {\n", output);
    fputs("        return ZiranOption {\n", output);
    fputs("            has_value: false,\n", output);
    fputs("            value: unsafe { core::mem::zeroed() },\n", output);
    fputs("        };\n", output);
    fputs("    }\n", output);
    fputs("    let value = unsafe { vector.data.offset(index as isize).read() };\n", output);
    fputs("    ZiranOption { has_value: true, value }\n", output);
    fputs("}\n", output);
    fputs("\n", output);
    fputs("pub fn ZiranVecSlice<T>(vector: &ZiranVec<T>, low: isize, high: isize) -> ZiranSlice<T> {\n", output);
    fputs("    assert!(low >= 0 && low <= high && high as usize <= vector.count as usize);\n", output);
    fputs("    ZiranSlice {\n", output);
    fputs("        data: if low == 0 { vector.data } else { unsafe { vector.data.offset(low) } },\n", output);
    fputs("        len: (high - low) as usize,\n", output);
    fputs("    }\n", output);
    fputs("}\n", output);
    fputs("\n", output);
    fputs("pub fn ZiranVecClear<T>(vector: &mut ZiranVec<T>) {\n", output);
    fputs("    vector.count = 0;\n", output);
    fputs("}\n", output);
    fputs("\n", output);
    fputs("pub fn ZiranBuilderAppend(vector: &mut ZiranVec<u8>, text: ZiranText) -> bool {\n", output);
    fputs("    for index in 0..text.len {\n", output);
    fputs("        if !ZiranVecPush(vector, unsafe { *text.data.offset(index as isize) }) {\n", output);
    fputs("            return false;\n", output);
    fputs("        }\n", output);
    fputs("    }\n", output);
    fputs("    true\n", output);
    fputs("}\n", output);
    fputs("\n", output);
    fputs("pub fn ZiranBuilderFinish(vector: &mut ZiranVec<u8>) -> ZiranText {\n", output);
    fputs("    if vector.count == 0 {\n", output);
    fputs("        ZiranVecFree(vector);\n", output);
    fputs("        return ZiranText { data: core::ptr::null(), len: 0 };\n", output);
    fputs("    }\n", output);
    fputs("    let text = ZiranText { data: vector.data, len: vector.count as usize };\n", output);
    fputs("    vector.data = core::ptr::null_mut();\n", output);
    fputs("    vector.count = 0;\n", output);
    fputs("    vector.capacity = 0;\n", output);
    fputs("    text\n", output);
    fputs("}\n", output);
    fputs("\n", output);
    /* print! spells floats as the shortest round-trip decimal, like every
     * target; only NaN is written differently, as nan. */
    fputs("pub struct ZiranFloat<T>(pub T);\n", output);
    fputs("\n", output);
    fputs("impl<T: core::fmt::Display + PartialEq> core::fmt::Display for ZiranFloat<T> {\n", output);
    fputs("    fn fmt(&self, f: &mut core::fmt::Formatter) -> core::fmt::Result {\n", output);
    fputs("        if self.0 != self.0 { f.write_str(\"nan\") } else { self.0.fmt(f) }\n", output);
    fputs("    }\n", output);
    fputs("}\n", output);
    fputs("\n", output);
    fputs("pub fn ZiranVecFree<T>(vector: &mut ZiranVec<T>) {\n", output);
    fputs("    if !vector.data.is_null() {\n", output);
    fputs("        let size = vector.capacity as usize * core::mem::size_of::<T>();\n", output);
    fputs("        let align = core::mem::align_of::<T>();\n", output);
    fputs("        match std::alloc::Layout::from_size_align(size, align) {\n", output);
    fputs("            Ok(layout) => unsafe {\n", output);
    fputs("                std::alloc::dealloc(vector.data as *mut u8, layout)\n", output);
    fputs("            },\n", output);
    fputs("            Err(_) => unreachable!(\"invalid Ziran vector layout\"),\n", output);
    fputs("        }\n", output);
    fputs("    }\n", output);
    fputs("    vector.data = core::ptr::null_mut();\n", output);
    fputs("    vector.count = 0;\n", output);
    fputs("    vector.capacity = 0;\n", output);
    fputs("}\n", output);
}

/* The Rust runtime is written in full, then only the items the program
 * refers to are kept: a struct or function stays when the program or a kept
 * item names it, and an impl stays with its type. */
typedef struct RustRuntimeItem {
    const char *start;
    size_t length;
    char name[ZIR_NAME_MAX];
    char impl_type[ZIR_NAME_MAX];
    int kept;
    /* A method of a split impl, or the impl's opening or closing line. */
    int method;
    int impl_line;
    int block; /* index of the impl opening line for methods and the close */
} RustRuntimeItem;

static int rust_names(const char *text, size_t length, const char *name)
{
    size_t size = strlen(name);
    if(size == 0)
        return 0;
    for(size_t index = 0; index + size <= length; index++) {
        if(memcmp(text + index, name, size) != 0)
            continue;
        if(index > 0 && is_ident_char((unsigned char)text[index - 1]))
            continue;
        if(index + size < length && is_ident_char((unsigned char)text[index + size]))
            continue;
        return 1;
    }
    return 0;
}

static void rust_item_name(RustRuntimeItem *item)
{
    const char *end = item->start + item->length;
    const char *markers[] = {"pub struct ", "pub fn ", "pub const fn ", "pub unsafe fn ",
                             "    pub fn ", "    pub const fn ", "    pub unsafe fn "};
    for(const char *line = item->start; line < end;) {
        const char *next = memchr(line, '\n', (size_t)(end - line));
        size_t size = next != NULL ? (size_t)(next - line) : (size_t)(end - line);
        for(size_t m = 0; m < sizeof(markers) / sizeof(markers[0]); m++) {
            size_t marker = strlen(markers[m]);
            if(size > marker && memcmp(line, markers[m], marker) == 0) {
                size_t used = 0;
                for(const char *c = line + marker; c < line + size &&
                    is_ident_char((unsigned char)*c) && used + 1 < sizeof(item->name); c++)
                    item->name[used++] = *c;
                item->name[used] = '\0';
                return;
            }
        }
        if((size > 4 && memcmp(line, "impl", 4) == 0) ||
           (size > 11 && memcmp(line, "unsafe impl", 11) == 0)) {
            /* The implemented type follows " for " when present. */
            const char *type = NULL;
            for(const char *c = line; c + 5 <= line + size; c++)
                if(memcmp(c, " for ", 5) == 0) { type = c + 5; break; }
            if(type == NULL) {
                type = line + (line[0] == 'u' ? 11 : 4);
                if(*type == '<')
                    while(type < line + size && *type != '>') type++;
                while(type < line + size && !is_ident_char((unsigned char)*type)) type++;
            }
            size_t used = 0;
            while(type < line + size && is_ident_char((unsigned char)*type) &&
                  used + 1 < sizeof(item->impl_type))
                item->impl_type[used++] = *type++;
            item->impl_type[used] = '\0';
            return;
        }
        line = next != NULL ? next + 1 : end;
    }
}

static RustRuntimeItem *rust_add_item(RustRuntimeItem **items, int *count,
                                      int *capacity, const char *start,
                                      size_t length)
{
    if(*count == *capacity) {
        *capacity = *capacity ? *capacity * 2 : 64;
        RustRuntimeItem *grown = realloc(*items, (size_t)*capacity * sizeof(**items));
        if(grown == NULL)
            return NULL;
        *items = grown;
    }
    RustRuntimeItem *item = &(*items)[(*count)++];
    memset(item, 0, sizeof(*item));
    item->start = start;
    item->length = length;
    item->block = -1;
    return item;
}

/* An inherent impl is split into its opening line, one piece per method,
 * and its closing line, so each method is kept only when used. */
static int rust_add_impl(RustRuntimeItem **items, int *count, int *capacity,
                         const char *start, size_t length)
{
    const char *end = start + length;
    const char *first_line_end = memchr(start, '\n', length);
    int methods = 0;
    for(const char *line = start; line < end;) {
        const char *next = memchr(line, '\n', (size_t)(end - line));
        if(!strncmp(line, "    pub fn ", 11) || !strncmp(line, "    pub const fn ", 17) ||
           !strncmp(line, "    fn ", 7))
            methods++;
        line = next ? next + 1 : end;
    }
    RustRuntimeItem *whole = rust_add_item(items, count, capacity, start, length);
    if(whole == NULL)
        return 0;
    rust_item_name(whole);
    if(methods < 2 || !whole->impl_type[0] || first_line_end == NULL)
        return 1;
    /* Replace the whole impl with its pieces. */
    (*count)--;
    int opening = *count;
    RustRuntimeItem *head = rust_add_item(items, count, capacity, start,
                                          (size_t)(first_line_end + 1 - start));
    if(head == NULL)
        return 0;
    rust_item_name(head);
    head->impl_line = 1;
    const char *method_start = NULL;
    const char *close = end;
    while(close > start && close[-1] == '\n') close--;
    while(close > start && close[-1] != '\n') close--;
    for(const char *line = first_line_end + 1; line <= close;) {
        int starts = line == close || !strncmp(line, "    pub fn ", 11) ||
                     !strncmp(line, "    pub const fn ", 17) || !strncmp(line, "    fn ", 7);
        if(starts && method_start != NULL) {
            RustRuntimeItem *method = rust_add_item(items, count, capacity, method_start,
                                                    (size_t)(line - method_start));
            if(method == NULL)
                return 0;
            rust_item_name(method);
            method->method = 1;
            method->block = opening;
            snprintf(method->impl_type, sizeof(method->impl_type), "%s",
                     (*items)[opening].impl_type);
        }
        if(line == close)
            break;
        if(starts)
            method_start = line;
        const char *next = memchr(line, '\n', (size_t)(end - line));
        line = next ? next + 1 : end;
    }
    RustRuntimeItem *tail = rust_add_item(items, count, capacity, close,
                                          (size_t)(end - close));
    if(tail == NULL)
        return 0;
    tail->impl_line = 1;
    tail->block = opening;
    return 1;
}

/* A method is called as .name( or Type::name(; other items by name. */
static int rust_mentions(const char *text, size_t length, const char *name, int method)
{
    if(!method)
        return rust_names(text, length, name);
    size_t size = strlen(name);
    for(size_t index = 1; index + size < length; index++)
        if(memcmp(text + index, name, size) == 0 && text[index + size] == '(' &&
           (text[index - 1] == '.' || text[index - 1] == ':'))
            return 1;
    return 0;
}

static int rust_item_referenced(RustRuntimeItem *items, int count, int self,
                                const char *body, size_t body_size)
{
    int method = items[self].method;
    if(rust_mentions(body, body_size, items[self].name, method))
        return 1;
    for(int other = 0; other < count; other++)
        if(other != self && items[other].kept &&
           rust_mentions(items[other].start, items[other].length, items[self].name, method))
            return 1;
    return 0;
}

int write_used_runtime(FILE *output, const char *runtime, size_t runtime_size,
                              const char *body, size_t body_size)
{
    RustRuntimeItem *items = NULL;
    int count = 0, capacity = 0, changed = 1;
    const char *end = runtime + runtime_size;
    const char *item_start = NULL;
    int open = 0;
    for(const char *line = runtime; line < end;) {
        const char *next = memchr(line, '\n', (size_t)(end - line));
        const char *line_end = next != NULL ? next + 1 : end;
        size_t size = (size_t)(line_end - line);
        int blank = size <= 1;
        if(item_start == NULL && !blank) {
            item_start = line;
            open = 1;
        }
        if(item_start != NULL && open && line[0] == '}' )
            open = 0;
        if(item_start != NULL && open && size >= 3 && line[0] != ' ' &&
           (memcmp(line_end - 3, "{}\n", 3) == 0 || memcmp(line_end - 2, ";\n", 2) == 0))
            open = 0;
        if(item_start != NULL && !open) {
            if(!rust_add_impl(&items, &count, &capacity, item_start,
                              (size_t)(line_end - item_start))) {
                free(items);
                return 0;
            }
            item_start = NULL;
        }
        line = line_end;
    }
    for(int i = 0; i < count; i++)
        if(!items[i].method && !items[i].impl_line && items[i].name[0] &&
           rust_names(body, body_size, items[i].name))
            items[i].kept = 1;
    while(changed) {
        changed = 0;
        for(int i = 0; i < count; i++) {
            int keep = 0;
            if(items[i].kept || items[i].impl_line)
                continue;
            if(items[i].method) {
                /* A method stays when its type does and something calls it. */
                for(int j = 0; j < count && !keep; j++)
                    keep = items[j].kept && !items[j].method && !items[j].impl_line &&
                           !strcmp(items[j].name, items[i].impl_type);
                keep = keep && rust_item_referenced(items, count, i, body, body_size);
            } else if(items[i].impl_type[0]) {
                for(int j = 0; j < count && !keep; j++)
                    keep = items[j].kept && !items[j].method &&
                           !strcmp(items[i].impl_type, items[j].name);
            } else if(items[i].name[0])
                keep = rust_item_referenced(items, count, i, body, body_size);
            if(keep)
                items[i].kept = changed = 1;
        }
    }
    /* A split impl's opening and closing lines stay with any kept method. */
    for(int i = 0; i < count; i++)
        if(items[i].method && items[i].kept && items[i].block >= 0) {
            items[items[i].block].kept = 1;
            for(int j = items[i].block + 1; j < count; j++)
                if(items[j].impl_line && items[j].block == items[i].block) {
                    items[j].kept = 1;
                    break;
                }
        }
    for(int i = 0; i < count; i++)
        if(items[i].kept) {
            fwrite(items[i].start, 1, items[i].length, output);
            if(!items[i].method && !(items[i].impl_line && items[i].block < 0))
                fputc('\n', output);
        }
    free(items);
    return 1;
}

static int rust_text_has(const char *first, size_t first_size,
                         const char *second, size_t second_size, const char *needle)
{
    size_t length = strlen(needle);
    for(int part = 0; part < 2; part++) {
        const char *text = part ? second : first;
        size_t size = part ? second_size : first_size;
        for(size_t index = 0; index + length <= size; index++)
            if(!memcmp(text + index, needle, length))
                return 1;
    }
    return 0;
}

/* A definition keyword followed by a name containing an uppercase letter
 * or underscore, like fn Hello_Main or struct Source_Code_Location. */
static int rust_text_names(const char *first, size_t first_size,
                           const char *second, size_t second_size,
                           const char *keyword, int (*unusual)(int))
{
    size_t length = strlen(keyword);
    for(int part = 0; part < 2; part++) {
        const char *text = part ? second : first;
        size_t size = part ? second_size : first_size;
        for(size_t index = 0; index + length < size; index++) {
            if(memcmp(text + index, keyword, length) != 0 ||
               (index > 0 && is_ident_char((unsigned char)text[index - 1])))
                continue;
            for(size_t c = index + length; c < size && is_ident_char((unsigned char)text[c]); c++)
                if(unusual((unsigned char)text[c]))
                    return 1;
        }
    }
    return 0;
}

static int rust_upper(int c) { return isupper(c); }
static int rust_underscore(int c) { return c == '_'; }

/* Only the lints the generated names and code can trigger are allowed. */
void write_rust_lint_allowances(FILE *output, const char *runtime, size_t runtime_size,
                                       const char *body, size_t body_size, int executable)
{
    int any = 0;
    if(rust_text_names(runtime, runtime_size, body, body_size, "fn ", rust_upper))
        any = fputs("#![allow(non_snake_case)]\n", output) >= 0;
    if(rust_text_names(runtime, runtime_size, body, body_size, "struct ", rust_underscore) ||
       rust_text_names(runtime, runtime_size, body, body_size, "union ", rust_underscore) ||
       rust_text_names(runtime, runtime_size, body, body_size, "type ", rust_underscore))
        any = fputs("#![allow(non_camel_case_types)]\n", output) >= 0;
    if(rust_text_has(runtime, runtime_size, body, body_size, "static mut "))
        any = fputs("#![allow(non_upper_case_globals)]\n", output) >= 0;
    if(!executable || rust_text_has("", 0, body, body_size, "let ") ||
       rust_text_has(runtime, runtime_size, body, body_size, "static mut "))
        any = fputs("#![allow(unused)]\n", output) >= 0;
    if(rust_text_has(runtime, runtime_size, body, body_size, "extern \"C\""))
        any = fputs("#![allow(improper_ctypes_definitions)]\n", output) >= 0;
    if(rust_text_has(runtime, runtime_size, body, body_size, ".wrapping_") ||
       rust_text_has(runtime, runtime_size, body, body_size, ".checked_") ||
       rust_text_has(runtime, runtime_size, body, body_size, " as usize]")) {
        fputs("#![allow(arithmetic_overflow)]\n", output);
        any = fputs("#![allow(unconditional_panic)]\n", output) >= 0;
    }
    if(any)
        fputc('\n', output);
}
