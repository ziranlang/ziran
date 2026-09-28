#include "zir_parse_internal.h"

static ZirProgram *
parse_source(const char *path, const char *root, const char *source,
             ZirCompileImportResolver resolver, void *resolver_context,
             const char *const *defines, int define_count)
{
    ZirProgram *program;
    CompileParseContext compile_context = {0};
    ZirModule *module;
    ZirFunction *fn = NULL;
    char line[SOURCE_LINE_MAX];
    char module_name[ZIR_NAME_MAX];
    char rel[SOURCE_PATH_MAX];
    int line_no = 0;
    int physical_line_no = 0;
    enum { TOP, TYPE, FUNCTION } mode = TOP;
    int depth = 0;
    char pending[SOURCE_LINE_MAX * 4];
    /* Compile-time imports recurse into parse_source while this line is in
     * use. Keep each invocation's logical source separate. */
    char logical[SOURCE_LINE_MAX * 4];
    pending[0] = '\0';
    char lookahead[SOURCE_LINE_MAX];
    int have_look = 0;
    /* One-line control blocks ('if cond { body }') are split into header /
     * body / '}' logical lines; the body and closer re-enter the main loop
     * through this FIFO so they flow through the normal join machinery. */
    char onelineq[16][SOURCE_LINE_MAX * 2];
    int onelineq_count = 0;
    int from_queue = 0;
    int pending_len = 0;
    int pending_start_line = 1;
    int pending_start_column = 1;
    int pending_end_column = 1;
    int paren_depth = 0;
    int bracket_depth = 0;
    int in_string = 0;
    int expr_brace = 0;
    ZirCondFrame tframes[8];
    int tframe_count = 0;
    ZirCondFrame type_frames[8];
    int type_frame_count = 0;
    ZirConsts consts;
    ZirConsts future_constants = {0};
    ZirUsings future_usings = {0};
    ZirImports future_imports = {0};
    ZirTypes future_types = {0};
    ZirDeferredTypes deferred_types = {0};
    ZirFunctions future_functions = {0};
    ZirGlobals future_globals = {0};
    ZirDiscoveredFiles discovered_files = {0};
    int body_mdepth[8];
    int body_mselected[8];
    int body_mactive[8];
    int body_mparent_active[8];
    int body_mcount = 0;
    int in_block_comment = 0;
    int program_export = 0;
    int program_export_line = 0;
    char program_export_symbol[ZIR_NAME_MAX] = "";
    int scope_public = 1;
    int scope_file = 0;
    char foreign_library_names[32][ZIR_NAME_MAX];
    char foreign_library_targets[32][ZIR_PATH_MAX];
    char foreign_library_paths[32][SOURCE_PATH_MAX];
    int foreign_library_file_private[32] = {0};
    int foreign_library_count = 0;
    LoadFrame load_frames[32];
    int load_depth = 0;
    char *canonical = realpath(path, NULL);
    char *owned_source = NULL;
    if(canonical == NULL)
        canonical = strdup(path);
    if(canonical == NULL)
        die("out of memory tracking source path");
    memset(&consts, 0, sizeof(consts));
    if(define_count > 0) {
        consts.items = calloc((size_t)define_count, sizeof(*consts.items));
        if(consts.items == NULL)
            die("out of memory recording compiler definitions");
        consts.capacity = define_count;
        consts.count = define_count;
        for(int i = 0; i < define_count; i++) {
            copy_text(consts.items[i].name,
                      sizeof(consts.items[i].name), defines[i]);
            copy_text(consts.items[i].expr,
                      sizeof(consts.items[i].expr), "1");
            consts.items[i].is_public = 1;
        }
    }
    snprintf(rel, sizeof(rel), "%s", relative_path(root, path));
    const char *basename = strrchr(path, '/');
    basename = basename != NULL ? basename + 1 : path;
    size_t stem_length = strlen(basename);
    if(stem_length > 3 && strcmp(basename + stem_length - 3, ".zi") == 0)
        stem_length -= 3;
    if(stem_length == 0 || stem_length >= sizeof(module_name))
        die_at(Span(rel, 1, 1), "source filename cannot be used as a module name");
    memcpy(module_name, basename, stem_length);
    module_name[stem_length] = '\0';
    program = ProgramNew();
    if(program == NULL)
        die("out of memory");
    compile_context.program = program;
    compile_context.root = root;
    compile_context.resolver = resolver;
    compile_context.resolver_context = resolver_context;
    compile_context.future_constants = &future_constants;
    compile_context.future_usings = &future_usings;
    compile_context.future_imports = &future_imports;
    compile_context.future_types = &future_types;
    compile_context.future_functions = &future_functions;
    compile_context.future_globals = &future_globals;

    module = ProgramAddModule(program, module_name, rel, Span(rel, 1, 1));
    if(module == NULL)
        die("out of memory");
    if(root != NULL) {
        char *canonical_root = realpath(root, NULL);
        copy_text(module->source_root, sizeof(module->source_root),
                  canonical_root != NULL ? canonical_root : root);
        free(canonical_root);
    }
    discover_file_scope(source, path, rel, root, &future_constants,
                        &future_usings, &future_imports, &future_types,
                        &deferred_types,
                        &future_functions, &future_globals,
                        &discovered_files, 0);
    discover_conditional_types(&deferred_types, module, &consts,
                               &compile_context, &future_types);

    for(;;) {
        if(!have_look && onelineq_count == 0 &&
           read_source_line(line, sizeof(line), &source, path, line_no) == NULL) {
            if(in_block_comment)
                die_at(Span(rel, line_no, 1), "unterminated block comment");
            if(pending_len != 0 || paren_depth != 0 || bracket_depth != 0 ||
               in_string || expr_brace != 0)
                die_at(Span(rel, pending_start_line, pending_start_column),
                       "unterminated source declaration or expression");
            if(load_depth == 0)
                break;
            LoadFrame *frame = &load_frames[--load_depth];
            if(mode != TOP || tframe_count != frame->conditional_depth ||
               type_frame_count != 0 ||
               body_mcount != 0 || program_export)
                die_at(Span(rel, line_no, 1),
                       "unterminated declaration or directive in loaded file");
            free(owned_source);
            free(canonical);
            path = frame->path;
            source = frame->source;
            canonical = frame->canonical;
            owned_source = frame->owned_source;
            copy_text(rel, sizeof(rel), frame->rel);
            copy_text(lookahead, sizeof(lookahead), frame->lookahead);
            line_no = frame->line_no;
            physical_line_no = frame->physical_line_no;
            have_look = frame->have_look;
            scope_public = frame->scope_public;
            scope_file = frame->scope_file;
            in_block_comment = frame->in_block_comment;
            continue;
        }
        if(!have_look && onelineq_count == 0)
            physical_line_no++;
        char raw[SOURCE_LINE_MAX];
        char *t;
        int from_lookahead = 0;
        copy_text(module->lookup_path, sizeof(module->lookup_path), rel);

        /* Queued one-liner parts outrank the stashed lookahead: they belong
         * before the next source line, and have_look persists until the
         * queue drains. */
        from_queue = onelineq_count > 0;
        if(onelineq_count > 0) {
            copy_text(line, sizeof(line), onelineq[0]);
            memmove(onelineq[0], onelineq[1],
                    sizeof(onelineq[0]) * (size_t)(onelineq_count - 1));
            onelineq_count--;
        } else if(have_look) {
            snprintf(line, sizeof(line), "%s", lookahead);
            have_look = 0;
            from_lookahead = 1;
        }

        if(!from_queue)
            line_no++;
        if(!from_queue && !from_lookahead)
            strip_block_comments(line, &in_block_comment);
        if(contains_source_directive(line, "#else_if") ||
           contains_source_directive(line, "#else"))
            die_at(Span(rel, line_no, 1),
                   "Jai compile-time branches use 'else #if' or 'else', not #else_if/#else");
        int compile_condition_line = line_starts_compile_condition(line);
        int active_source =
            (tframe_count == 0 || tframes[tframe_count - 1].active) &&
            (type_frame_count == 0 ||
             type_frames[type_frame_count - 1].active) &&
            (body_mcount == 0 || body_mactive[body_mcount - 1]);
        if(!from_queue && active_source && !compile_condition_line)
            normalize_jai_source_tokens(line, rel, canonical,
                                        physical_line_no);
        snprintf(raw, sizeof(raw), "%s", line);
        {
            char *trimmed = trim(raw);
            int trimmed_column = from_queue ? 1 :
                                 source_column_for_trimmed(raw, trimmed);

            if(trimmed[0] == '\0' || strncmp(trimmed, "//", 2) == 0) {
                if(pending_len == 0)
                    continue;
                continue;
            }
            if(pending_len == 0) {
                pending_start_line = line_no;
                pending_start_column = trimmed_column;
            }
            pending_end_column = source_end_column_for_trimmed(raw, trimmed);
            if(pending_len > 0 && pending_len + 2 < (int)sizeof(pending)) {
                pending[pending_len++] = '\n';
                pending[pending_len] = '\0';
            }
            strncat(pending, trimmed, sizeof(pending) - pending_len - 1);
            pending_len = (int)strlen(pending);
            pending_len = (int)strlen(pending);
            /* Decide whether braces at paren-depth 0 on this logical line are
             * block braces (control/headers open scopes) or expression braces
             * (compound literals / initializers continue the statement). */
            {
                int header_line = 0;
                char w0[16];
                size_t wl = 0;

                for(const char *w = pending;
                    *w != '\0' && (isalnum((unsigned char)*w) || *w == '_') &&
                    wl + 1 < sizeof(w0); w++)
                    w0[wl++] = *w;
                w0[wl] = '\0';
                /* Keyword headers must be followed by ' ', '(' or '{':
                 * 'item->x = ...' / 'state.x' are member statements, not
                 * block headers (their compound-literal braces are
                 * expression braces). */
                {
                    char nc = pending[wl];
                    char block_callee[ZIR_NAME_MAX];
                    char block_name[ZIR_NAME_MAX];
                    header_line =
                        pending[0] == '#' ||
                        strcmp(pending, "{") == 0 ||   /* bare scope-open */
                        parse_block_call_header(pending, block_callee,
                                                sizeof(block_callee), block_name,
                                                sizeof(block_name)) ||
                        /* 'name :: Type = {' carries an initializer, not a
                         * body: its braces are expression braces so the
                         * logical line continues until they balance. Header
                         * forms ('name :: struct {', 'f :: (args) {', typedefs,
                         * externs) never contain ' = '. */
                        looks_like_function_header(pending) ||
                        (strstr(pending, " :: ") != NULL &&
                         strstr(pending, " = ") == NULL) ||
                        (nc != '\0' && nc != '-' && nc != '.' &&
                         (strchr(" ({", nc) != NULL || nc == ':') &&
                         (strcmp(w0, "if") == 0 ||
                          strcmp(w0, "else") == 0 ||
                          strcmp(w0, "while") == 0 ||
                          strcmp(w0, "for") == 0 ||
                          strcmp(w0, "defer") == 0 ||
                          strcmp(w0, "switch") == 0 ||
                          strcmp(w0, "match") == 0 ||
                          strcmp(w0, "do") == 0 ||
                          strcmp(w0, "case") == 0 ||
                          strcmp(w0, "struct") == 0 ||
                          strcmp(w0, "enum") == 0 ||
                          strcmp(w0, "state") == 0));
                }
                /* K&R "} else {" / "} else if (...) {" also covers Jai's
                 * "} else #if COND {" compile-time branch. The leading and
                 * trailing braces balance as one logical header line. */
                if(!header_line && pending[0] == '}') {
                    const char *eq = pending + 1;

                    while(*eq == ' ' || *eq == '\t')
                        eq++;
                    if(starts_word(eq, "else"))
                        header_line = 1;
                }
                {
                int in_chr = 0;

                for(const char *p = trimmed; *p != '\0'; p++) {
                    if(in_string) {
                        if(*p == '\\' && p[1] != '\0')
                            p++;
                        else if(*p == '"')
                            in_string = 0;
                    } else if(in_chr) {
                        if(*p == '\\' && p[1] != '\0')
                            p++;
                        else if(*p == '\'')
                            in_chr = 0;
                    } else if(*p == '"') {
                        in_string = 1;
                    } else if(*p == '\'') {
                        in_chr = 1;
                    } else if(*p == '(') {
                        paren_depth++;
                    } else if(*p == ')') {
                        paren_depth--;
                    } else if(*p == '[') {
                        bracket_depth++;
                    } else if(*p == ']') {
                        bracket_depth--;
                    } else if(paren_depth == 0 && bracket_depth == 0) {
                        if(*p == '{') {
                            if(!header_line)
                                expr_brace++;
                        } else if(*p == '}') {
                            if(!header_line && expr_brace > 0)
                                expr_brace--;
                        }
                    }
                }
                }
            }
            if(paren_depth > 0 || bracket_depth > 0 || in_string ||
               expr_brace > 0)
                continue;
            /* Continuation: a line ending in a binary operator or comma
             * continues onto the next. Exclude ++/-- (they end statements). */
            {
                size_t pl = (size_t)pending_len;
                char last;
                char prev;

                while(pl > 0 && (pending[pl - 1] == ' ' ||
                                 pending[pl - 1] == '\t'))
                    pl--;
                last = pl > 0 ? pending[pl - 1] : '\0';
                prev = pl > 1 ? pending[pl - 2] : '\0';
                /* Continue after a binary operator or comma. The previous
                 * character distinguishes an operator from a postfix form. */
                if(last == ',' || last == '=' || last == '%' ||
                   (last == '/' && prev != '>'))
                    continue;
                if((last == '+' || last == '-' || last == '*' ||
                    last == '<' || last == '>') &&
                   (prev == ' ' || prev == '\t') &&
                   !(prev == last))
                    continue;
                if((last == '&' || last == '|') &&
                   (prev == last || prev == ' ' || prev == '\t'))
                    continue;
                /* Look ahead for Jai continuation tokens. Skipped for queued
                 * one-liner parts and while a stash is pending: their "next
                 * line" is not the next physical source line. */
                if(!from_queue && !have_look) {
                    char la[SOURCE_LINE_MAX];

                    /* Keep consuming lookahead lines while they continue this
                     * statement; the first non-continuation line is stashed
                     * for the next iteration (appending it blindly here is
                     * how block-closing '}'s used to get swallowed). */
                    while(read_source_line(la, sizeof(la), &source, path, line_no) != NULL) {
                        const char *lt;
                        int cont;

                        physical_line_no++;
                        strip_block_comments(la, &in_block_comment);
                        if(contains_source_directive(la, "#else_if") ||
                           contains_source_directive(la, "#else"))
                            die_at(Span(rel, line_no + 1, 1),
                                   "Jai compile-time branches use 'else #if' or 'else', not #else_if/#else");
                        /* Decide continuation from raw source. A stashed line
                         * may become inactive after this line selects #if;
                         * normalize it only when the parser consumes it. */
                        lt = trim(la);
                        if(lt[0] == '\0' || strncmp(lt, "//", 2) == 0) {
                            line_no++;   /* comments and blank lines still count */
                            continue;
                        }
                        cont =
                            (last == '}' && starts_word(lt, "else")) ||
                            *lt == '.' || *lt == ',' || *lt == '+' ||
                            *lt == '/' || *lt == '%' ||
                            (*lt == '-' && lt[1] != '>') ||
                            ((*lt == '&' && lt[1] == '&') ||
                             (*lt == '|' && lt[1] == '|') ||
                             (*lt == '=' && lt[1] == '=') ||
                             (*lt == '!' && lt[1] == '=') ||
                             (*lt == '<' && lt[1] == '=') ||
                             (*lt == '>' && lt[1] == '='));
                        if(!cont) {
                            snprintf(lookahead, sizeof(lookahead), "%s", la);
                            have_look = 1;
                            break;
                        }
                        if(active_source && !compile_condition_line)
                            normalize_jai_source_tokens(la, rel, canonical,
                                                        physical_line_no);
                        lt = trim(la);
                        pending_end_column = source_end_column_for_trimmed(la, lt);
                        if(pending_len > 0 &&
                           pending_len + 2 < (int)sizeof(pending)) {
                            pending[pending_len++] =
                                last == '}' && starts_word(lt, "else") ?
                                ' ' : '\n';
                            pending[pending_len] = '\0';
                        }
                        strncat(pending, lt,
                                sizeof(pending) - pending_len - 1);
                        pending_len = (int)strlen(pending);
                        line_no++;
                    }
                }
            }
        }
        t = pending;
        int export_directive = 0;
        char export_directive_symbol[ZIR_NAME_MAX] = "";
        {
            snprintf(logical, sizeof(logical), "%s", pending);
            t = logical;
            char *separator = statement_separator(logical);
            if(separator != NULL) {
                prepend_logical_line(onelineq, &onelineq_count,
                                     skip_ws(separator + 1),
                                     Span(rel, line_no, 1));
                char *before = separator;
                while(before > logical && isspace((unsigned char)before[-1]))
                    before--;
                if(before > logical && before[-1] == '}')
                    *before = '\0'; /* A block ends at its closing brace. */
                else
                    separator[1] = '\0'; /* Keep declaration terminators. */
                trim_in_place(logical);
            }
            if(mode == TOP) {
                export_directive = strip_program_export(logical,
                    export_directive_symbol,
                    sizeof(export_directive_symbol),
                    Span(rel, line_no, 1));
                t = logical;
            }
            /* One-line control block: keep the header as this logical line
             * and queue the body + closer for the next iterations (nested
             * one-liners split again when their body is finalized). */
            if(mode == TOP) {
                char head[SOURCE_LINE_MAX * 2];
                char body[SOURCE_LINE_MAX * 2];

                if(split_oneline_function(t, head, sizeof(head),
                                          body, sizeof(body))) {
                    prepend_logical_line(onelineq, &onelineq_count, "}",
                                         Span(rel, line_no, 1));
                    if(body[0])
                        prepend_logical_line(onelineq, &onelineq_count,
                                             body, Span(rel, line_no, 1));
                    snprintf(logical, sizeof(logical), "%s", head);
                    t = logical;
                }
            } else {
                char head[SOURCE_LINE_MAX * 2];
                char body[SOURCE_LINE_MAX * 2];
                char tail[SOURCE_LINE_MAX * 2];

                if(split_oneline_block(t, head, sizeof(head),
                                       body, sizeof(body),
                                       tail, sizeof(tail))) {
                    if(tail[0])
                        prepend_logical_line(onelineq, &onelineq_count,
                                             tail, Span(rel, line_no, 1));
                    prepend_logical_line(onelineq, &onelineq_count, "}",
                                         Span(rel, line_no, 1));
                    if(body[0])
                        prepend_logical_line(onelineq, &onelineq_count,
                                             body, Span(rel, line_no, 1));
                    snprintf(logical, sizeof(logical), "%s", head);
                    t = logical;
                }
            }
            if(mode == FUNCTION) {
                split_jai_control_line(logical, sizeof(logical), onelineq,
                                       &onelineq_count,
                                       Span(rel, line_no, 1));
                t = logical;
            }
        }
        pending[0] = '\0';
        pending_len = 0;
        if(*t == '\0') continue;
        char *string_source = NULL;
        if(mode != FUNCTION &&
           contains_source_directive(t, "#procedure_name") &&
           !(mode == TOP && looks_like_function_header(t)))
            die_at(Span(rel, line_no, 1),
                   "#procedure_name() requires a procedure scope");
        if(mode != FUNCTION &&
           contains_source_directive(t, "#caller_location") &&
           !(mode == TOP && looks_like_function_header(t)))
            die_at(Span(rel, line_no, 1),
                   "#caller_location is only valid as a parameter default");
        if(contains_source_directive(t, "#slot"))
            die_at(Span(rel, line_no, 1),
                   "#slot is not Jai syntax; use a procedure type and a named function");
        if(mode == TOP && contains_source_directive(t, "#private"))
            die_at(Span(rel, line_no, 1),
                   "#private is not Jai syntax; use #scope_file");
        if(mode == TOP && looks_like_function_header(t)) {
            const char *parameters = strchr(t, '(');
            const char *closing = closing_parenthesis(parameters);
            const char *body_open = closing == NULL ? NULL :
                                    strchr(closing + 1, '{');
            for(const char *modifier = strchr(closing == NULL ? t :
                                              closing + 1, '#');
                modifier != NULL &&
                (body_open == NULL || modifier < body_open);
                modifier = strchr(modifier + 1, '#'))
                if(!starts_word(modifier, "#foreign") &&
                   !starts_word(modifier, "#must"))
                    die_at(Span(rel, line_no, 1),
                           "unknown function modifier: %s", modifier);
        }
        if(mode == TOP &&
           cond_top_step(t, tframes, &tframe_count,
                         module, &consts, &compile_context,
                         path, rel, line_no, NULL)) {
            continue;
        } else if(mode == TOP && tframe_count > 0 &&
                  !tframes[tframe_count - 1].active) {
            continue;
        }
        if(mode == TOP && export_directive) {
            if(program_export)
                die_at(Span(rel, line_no, 1),
                       "#program_export must precede exactly one function");
            program_export = 1;
            program_export_line = line_no;
            copy_text(program_export_symbol,
                      sizeof(program_export_symbol),
                      export_directive_symbol);
            if(export_directive == 1)
                continue;
        }
        if(mode == TOP &&
           (starts_word(t, "#law") || starts_word(t, "#law_waive"))) {
            if(starts_word(t, "#law_waive")) {
                const char *body = skip_ws(t + strlen("#law_waive"));
                char name[ZIR_NAME_MAX], reason[ZIR_TEXT_MAX];
                size_t length = 0;
                while((isalnum((unsigned char)*body) || *body == '_') &&
                      length + 1 < sizeof(name))
                    name[length++] = *body++;
                name[length] = '\0';
                body = skip_ws(body);
                if(length == 0 || *body != '"') {
                    body = skip_ws(body);
                    const char *semi = body[0] == '"' ? NULL : NULL;
                    (void)semi;
                }
                {
                    const char *quote = *body == '"' ? body + 1 : NULL;
                    const char *end = quote != NULL ?
                        strchr(quote, '"') : NULL;
                    const char *semi = end != NULL ?
                        skip_ws(end + 1) : NULL;
                    if(quote == NULL || end == NULL || semi == NULL ||
                        strcmp(semi, ";") != 0)
                        die_at(Span(rel, line_no, 1),
                               "#law_waive requires NAME \"reason\";");
                    if((size_t)(end - quote) >= sizeof(reason))
                        die_at(Span(rel, line_no, 1),
                               "#law_waive reason is too long");
                    memcpy(reason, quote, (size_t)(end - quote));
                    reason[end - quote] = '\0';
                    if(!ModuleAddLawWaiver(module, name, reason,
                                           Span(rel, line_no, 1)))
                        die_at(Span(rel, line_no, 1),
                               "out of memory recording law waiver");
                }
            } else {
                const char *body = skip_ws(t + strlen("#law"));
                char name[ZIR_NAME_MAX], kind[16];
                char payload[ZIR_TEXT_MAX];
                size_t length = 0;
                const char *semi;
                while((isalnum((unsigned char)*body) || *body == '_') &&
                      length + 1 < sizeof(name))
                    name[length++] = *body++;
                name[length] = '\0';
                body = skip_ws(body);
                length = 0;
                while((isalnum((unsigned char)*body) || *body == '_') &&
                      length + 1 < sizeof(kind))
                    kind[length++] = *body++;
                kind[length] = '\0';
                semi = body;
                while(*semi && *semi != ';') semi++;
                if(name[0] == '\0' || kind[0] == '\0' ||
                   (strcmp(kind, "type") != 0 &&
                    strcmp(kind, "effect") != 0 &&
                    strcmp(kind, "bounds") != 0 &&
                    strcmp(kind, "abi") != 0 &&
                    strcmp(kind, "custom") != 0 &&
                    strcmp(kind, "size") != 0) ||
                   *skip_ws(body) == '\0' || *semi != ';' ||
                   semi == body)
                    die_at(Span(rel, line_no, 1),
                           "#law requires NAME kind payload; with kind "
                           "type, effect, bounds, abi, custom, or size");
                {
                    const char *start = skip_ws(body);
                    size_t payload_length = (size_t)(semi - start);
                    if(payload_length == 0 ||
                       payload_length >= sizeof(payload))
                        die_at(Span(rel, line_no, 1),
                               "#law payload is empty or too long");
                    memcpy(payload, start, payload_length);
                    payload[payload_length] = '\0';
                    trim_in_place(payload);
                }
                if(!ModuleAddLaw(module, name, kind, payload,
                                 Span(rel, line_no, 1)))
                    die_at(Span(rel, line_no, 1),
                           "out of memory recording law");
            }
            continue;
        } else if(mode == TOP &&
                  parse_compile_check(module, rel, line_no, t, &consts)) {
            continue;
        } else if(mode == TOP &&
                  (strcmp(t, "#scope_file") == 0 ||
                   strcmp(t, "#scope_module") == 0 ||
                   strcmp(t, "#scope_export") == 0)) {
            scope_public = strcmp(t, "#scope_export") == 0;
            scope_file = strcmp(t, "#scope_file") == 0;
            continue;
        } else if(mode == TOP && program_export &&
                  !looks_like_function_header(t)) {
            die_at(Span(rel, program_export_line, 1),
                   "#program_export must precede a function declaration");
        } else if(mode == TOP &&
                  (starts_word(t, "using") || strncmp(t, "using,", 6) == 0) &&
                  strstr(t, "#import") == NULL) {
            char name[ZIR_NAME_MAX];
            char filter[160];
            const char *path = skip_ws(t + 5);
            filter[0] = '\0';
            parse_using_modifiers(&path, filter, sizeof(filter), rel, line_no);
            if(strchr(path, ':') != NULL) {
                parse_file_global(module, path, Span(rel, line_no, 1),
                                  scope_public, scope_file,
                                  name, sizeof(name));
            } else {
                size_t length = strlen(path);
                if(length < 2 || path[length - 1] != ';' ||
                   length >= sizeof(name))
                    die_at(Span(rel, line_no, 1),
                           "using requires a type or record binding and ';'");
                memcpy(name, path, length - 1);
                name[length - 1] = '\0';
                trim_in_place(name);
                if(!is_member_path_text(name))
                    die_at(Span(rel, line_no, 1),
                           "using requires a type or record binding name");
            }
            ZirUsing *using = ModuleAddUsing(module, name,
                Span(rel, line_no, 1));
            if(using == NULL)
                die_at(Span(rel, line_no, 1),
                       "out of memory recording using declaration");
            copy_text(using->filter, sizeof(using->filter), filter);
            using->is_file_private = scope_file;
            continue;
        } else if(mode == TOP && starts_word(t, "#load")) {
            char requested[SOURCE_PATH_MAX];
            char candidate[SOURCE_PATH_MAX * 2];
            const char *argument = skip_ws(t + strlen("#load"));
            const char *end = *argument == '"' ?
                              strchr(argument + 1, '"') : NULL;
            const char *slash = strrchr(path, '/');
            if(end == NULL || strcmp(skip_ws(end + 1), ";") != 0 ||
               (size_t)(end - argument - 1) >= sizeof(requested))
                die_at(Span(rel, line_no, 1),
                       "#load requires a quoted relative .zi path and ';'");
            memcpy(requested, argument + 1,
                   (size_t)(end - argument - 1));
            requested[end - argument - 1] = '\0';
            size_t length = strlen(requested);
            if(requested[0] == '/' || length < 4 ||
               strcmp(requested + length - 3, ".zi") != 0 ||
               strchr(requested, '\\') != NULL)
                die_at(Span(rel, line_no, 1),
                       "#load requires a quoted relative .zi path and ';'");
            if(load_depth >= (int)(sizeof(load_frames) / sizeof(load_frames[0])))
                die_at(Span(rel, line_no, 1), "#load nesting limit exceeded");
            if(slash != NULL) {
                if(snprintf(candidate, sizeof(candidate), "%.*s/%s",
                            (int)(slash - path), path, requested) >=
                   (int)sizeof(candidate))
                    die_at(Span(rel, line_no, 1), "#load path is too long");
            } else
                copy_text(candidate, sizeof(candidate), requested);
            char *next_path = realpath(candidate, NULL);
            if(next_path == NULL)
                die_at(Span(rel, line_no, 1),
                       "cannot find loaded file: %s", requested);
            if(strcmp(next_path, canonical) == 0)
                die_at(Span(rel, line_no, 1), "cyclic #load: %s", requested);
            for(int i = 0; i < load_depth; i++)
                if(strcmp(next_path, load_frames[i].canonical) == 0)
                    die_at(Span(rel, line_no, 1), "cyclic #load: %s", requested);
            LoadFrame *frame = &load_frames[load_depth++];
            frame->path = path;
            frame->source = source;
            frame->canonical = canonical;
            frame->owned_source = owned_source;
            copy_text(frame->rel, sizeof(frame->rel), rel);
            copy_text(frame->lookahead, sizeof(frame->lookahead), lookahead);
            frame->line_no = line_no;
            frame->physical_line_no = physical_line_no;
            frame->have_look = have_look;
            frame->scope_public = scope_public;
            frame->scope_file = scope_file;
            frame->in_block_comment = in_block_comment;
            frame->conditional_depth = tframe_count;
            canonical = next_path;
            path = canonical;
            owned_source = read_lowered_source(path);
            source = owned_source;
            copy_text(rel, sizeof(rel), relative_path(root, path));
            line_no = 0;
            physical_line_no = 0;
            have_look = 0;
            lookahead[0] = '\0';
            scope_public = 1;
            scope_file = 0;
            in_block_comment = 0;
            continue;
        } else if(mode == TOP &&
                  (string_source = import_string_source(
                       t, Span(rel, line_no, 1))) != NULL) {
            char alias[ZIR_NAME_MAX];
            char synthetic_name[ZIR_NAME_MAX];
            char synthetic_path[SOURCE_PATH_MAX * 2];
            uint32_t hash = 2166136261u;
            for(const unsigned char *p = (const unsigned char *)rel; *p; p++)
                hash = (hash ^ *p) * 16777619u;
            for(const unsigned char *p = (const unsigned char *)t; *p; p++)
                hash = (hash ^ *p) * 16777619u;
            hash = (hash ^ (uint32_t)line_no) * 16777619u;
            snprintf(synthetic_name, sizeof(synthetic_name),
                     "string_%08x", hash);
            const char *slash = strrchr(path, '/');
            int written = slash == NULL ?
                snprintf(synthetic_path, sizeof(synthetic_path), "%s.zi",
                         synthetic_name) :
                snprintf(synthetic_path, sizeof(synthetic_path),
                         "%.*s/%s.zi", (int)(slash - path), path,
                         synthetic_name);
            if(written < 0 || (size_t)written >= sizeof(synthetic_path))
                die_at(Span(rel, line_no, 1),
                       "#import, string source path is too long");
            int named = parse_symbol_before_colons(t, alias, sizeof(alias));
            ZirProgram *fragment = parse_source(synthetic_path, root,
                                                string_source, resolver,
                                                resolver_context,
                                                defines, define_count);
            free(string_source);
            if(fragment == NULL) {
                ProgramFree(program);
                free(consts.items);
                free(future_constants.items);
                free(future_usings.items);
                free(future_imports.items);
                free(future_types.items);
                free_deferred_types(&deferred_types);
                free_discovered_functions(&future_functions);
                free(future_globals.items);
                free_discovered_files(&discovered_files);
                free(canonical);
                return NULL;
            }
            for(int imported = 0; imported < fragment->module_count;
                imported++) {
                ZirModule *incoming = &fragment->modules[imported];
                for(int existing = 0; existing < program->module_count;
                    existing++)
                    if(strcmp(program->modules[existing].name,
                              incoming->name) == 0)
                        die_at(Span(rel, line_no, 1),
                               "duplicate #import, string module identity");
                ZirModule *added = ProgramAddModule(program, incoming->name,
                    incoming->source_path, incoming->span);
                if(added == NULL)
                    die("out of memory importing source string");
                *added = *incoming;
                *incoming = (ZirModule){0};
            }
            ProgramFree(fragment);
            module = &program->modules[0];
            ZirImport *import = ModuleAddImport(module,
                named ? ZIR_IMPORT_MODULE : ZIR_IMPORT_OPEN,
                named ? alias : synthetic_name, synthetic_name, "string",
                scope_public, Span(rel, line_no, 1));
            if(import == NULL)
                die("out of memory importing source string");
            import->is_file_private = scope_file;
            continue;
        } else if(mode == TOP && t[0] == '#' &&
                  !starts_word(t, "#import") &&
                  strncmp(t, "#import,", 8) != 0 &&
                  !starts_word(t, "#enum")) {
            die_at(Span(rel, line_no, 1),
                   "unknown directive: %s", t);
        } else if(mode == TOP &&
                  parse_foreign_library_line(rel, line_no, t,
                      foreign_library_names, foreign_library_targets,
                      &foreign_library_count)) {
            copy_text(foreign_library_paths[foreign_library_count - 1],
                      sizeof(foreign_library_paths[0]), rel);
            foreign_library_file_private[foreign_library_count - 1] =
                scope_file;
            continue;
        } else if(mode == TOP &&
                  (parse_import_line(module, rel, line_no, t,
                                     scope_public) ||
                   parse_foreign_line(module, rel, line_no, t,
                       scope_public, foreign_library_names,
                       foreign_library_targets, foreign_library_paths,
                       foreign_library_file_private,
                       foreign_library_count))) {
            program_export = 0;
            if(module->import_count > 0)
                module->imports[module->import_count - 1].is_file_private = scope_file;
            continue;
        } else if(mode == TOP && starts_word(t, "state") &&
                  *skip_ws(t + 5) == '{') {
            die_at(Span(rel, line_no, 1),
                   "state blocks are not Jai syntax; declare named globals");
        } else if(mode == TOP && strstr(t, "::") != NULL &&
                  starts_word(skip_ws(strstr(t, "::") + 2), "#type")) {
            char name[ZIR_NAME_MAX];
            char args[ZIR_TEXT_MAX];
            char ret[ZIR_NAME_MAX];
            const char *declaration = skip_ws(strstr(t, "::") + 2);
            const char *parameters = skip_ws(declaration + strlen("#type"));
            if(!parse_symbol_before_colons(t, name, sizeof(name)) ||
               *parameters != '(' || strchr(t, '{') != NULL ||
               t[strlen(t) - 1] != ';')
                die_at(Span(rel, line_no, 1),
                       "procedure type requires Name :: #type (Types) -> Result;");
            parse_function_header(name, sizeof(name), args, sizeof(args),
                                  ret, sizeof(ret), t);
            size_t return_length = strlen(ret);
            const char *c_call = strstr(t, "#c_call");
            int is_c_call = c_call != NULL;
            if(is_c_call) {
                if(*skip_ws(c_call + strlen("#c_call")) != ';' ||
                   skip_ws(c_call + strlen("#c_call"))[1] != '\0' ||
                   return_length == 0)
                    die_at(Span(rel, line_no, 1),
                           "procedure type requires Result #c_call;");
            } else {
                if(return_length == 0 || ret[return_length - 1] != ';')
                    die_at(Span(rel, line_no, 1),
                           "procedure type requires a result type and ';'");
                ret[--return_length] = '\0';
                while(return_length > 0 &&
                      isspace((unsigned char)ret[return_length - 1]))
                    ret[--return_length] = '\0';
            }
            if(strcmp(ret, "()") == 0)
                copy_text(ret, sizeof(ret), "void");
            if(program_export)
                die_at(Span(rel, program_export_line, 1),
                       "#program_export requires a function");
            ZirType *slot = ModuleAddType(module, name, Span(rel, line_no, 1));
            if(slot == NULL)
                die("out of memory declaring procedure type");
            slot->is_procedure_type = 1;
            slot->is_c_call = is_c_call;
            slot->is_public = scope_public;
            slot->is_file_private = scope_file;
            copy_text(slot->procedure_return_type,
                      sizeof(slot->procedure_return_type), ret);
            if(*skip_ws(args)) {
                char parts[16][ZIR_TEXT_MAX];
                int count = split_top_level(args, parts[0], 16,
                                            sizeof(parts[0]));
                size_t used = 0;
                if(count < 1 || count > 16)
                    die_at(slot->span, "invalid procedure type parameters");
                for(int parameter = 0; parameter < count; parameter++) {
                    char *part = trim(parts[parameter]);
                    char *colon = strchr(part, ':');
                    const char *type = colon == NULL ? part : skip_ws(colon + 1);
                    char parameter_name[ZIR_NAME_MAX];
                    int written;
                    if(colon != NULL) {
                        *colon = '\0';
                        trim_in_place(part);
                        if(!is_identifier_text(part))
                            die_at(slot->span,
                                   "procedure type parameter needs a name");
                        copy_text(parameter_name, sizeof(parameter_name), part);
                    } else
                        snprintf(parameter_name, sizeof(parameter_name),
                                 "arg%d", parameter);
                    if(*type == '\0' || strchr(type, ':') != NULL)
                        die_at(slot->span,
                               "procedure type parameter needs a type");
                    written = snprintf(slot->body + used,
                                       sizeof(slot->body) - used,
                                       "%s%s: %s", parameter ? ", " : "",
                                       parameter_name, type);
                    if(written < 0 || (size_t)written >=
                        sizeof(slot->body) - used)
                        die_at(slot->span,
                               "procedure type parameters exceed size limit");
                    used += (size_t)written;
                }
            }
            continue;
        } else if(mode == TOP && looks_like_function_header(t)) {
            char name[ZIR_NAME_MAX];
            char args[ZIR_TEXT_MAX];
            char defaults[ZIR_TEXT_MAX] = "";
            char ret[ZIR_NAME_MAX];
            int has_body = strchr(t, '{') != NULL;
            int conversion = 0;
            {
                const char *marker = strstr(t, "::");
                if(marker != NULL) {
                    marker = skip_ws(marker + 2);
                    if(starts_word(marker, "#as")) {
                        conversion = 1;
                        if(*skip_ws(marker + 3) != '(')
                            die_at(Span(rel, line_no, 1),
                                   "#as requires (source: Type) -> Result");
                    }
                }
            }

            parse_function_header(name, sizeof(name), args, sizeof(args),
                                  ret, sizeof(ret), t);
            if(!has_body)
                die_at(Span(rel, line_no, 1),
                       "procedure types require Jai #type syntax");
            if(name[0] != '\0') {
                separate_parameter_defaults(args, sizeof(args), defaults,
                                            sizeof(defaults),
                                            Span(rel, line_no, 1));
                uint64_t using_parameters = strip_using_parameters(
                    args, sizeof(args), Span(rel, line_no, 1));
                if(defaults[0] && strip_using_parameters(
                       defaults, sizeof(defaults),
                       Span(rel, line_no, 1)) != using_parameters)
                    die_at(Span(rel, line_no, 1),
                           "inconsistent using parameter defaults");
                fn = ModuleAddFunction(module, name, args, ret, 0,
                                          Span(rel, line_no, 1));
                if(conversion) {
                    char parameters[64][ZIR_TEXT_MAX];
                    int count = *skip_ws(args) ?
                        split_top_level(args, parameters[0], 1,
                                        sizeof(parameters[0])) : 0;
                    if(count != 1 || strchr(args, '$') != NULL ||
                       strcmp(ret, "void") == 0)
                        die_at(fn->span,
                               "#as requires one source parameter and a result");
                    fn->is_conversion = 1;
                }
                fn->using_parameters = using_parameters;
                fn->must_use = function_must_use(t, ret, fn->span);
                copy_text(fn->default_args, sizeof(fn->default_args), defaults);
                if(strchr(args, '$') != NULL) {
                    char parameters[64][ZIR_TEXT_MAX];
                    int count = split_top_level(args, parameters[0], 64,
                                                sizeof(parameters[0]));
                    if(count < 1)
                        die_at(fn->span, "invalid polymorphic procedure parameters");
                    for(int parameter = 0; parameter < count; parameter++) {
                        char *colon = strchr(parameters[parameter], ':');
                        char *type = colon == NULL ? NULL : trim(colon + 1);
                        if(type == NULL || strchr(type, '$') == NULL)
                            continue;
                        if(type[0] != '$' || !is_identifier_text(type + 1) ||
                           (fn->template_param[0] &&
                            strcmp(fn->template_param, type + 1)))
                            die_at(fn->span,
                                   "polymorphic procedure requires one $Type parameter");
                        copy_text(fn->template_param,
                                  sizeof(fn->template_param), type + 1);
                    }
                    if(fn->template_param[0] == '\0')
                        die_at(fn->span, "invalid polymorphic procedure parameter");
                    if(program_export)
                        die_at(fn->span,
                               "#program_export requires a concrete procedure");
                    fn->is_template = 1;
                }
                /* Preserve the Jai source name and optional linker symbol. */
                fn->exported = program_export;
                copy_text(fn->export_symbol,
                          sizeof(fn->export_symbol),
                          program_export_symbol);
                program_export = 0;
                program_export_symbol[0] = '\0';
                /* Public functions are emitted in headers. */
                fn->is_public = scope_public;
                fn->is_file_private = scope_file;
                mode = FUNCTION;
                depth = 1;
            }
        } else if(mode == TOP && contains_source_directive(t, "#global")) {
            die_at(Span(rel, line_no, 1),
                   "#global is not Jai syntax; declare a variable with name: Type");
        } else if(mode == TOP &&
                  (isalpha((unsigned char)t[0]) || t[0] == '_') &&
                  strchr(t, ':') != NULL && strstr(t, "::") == NULL) {
            /* A Jai file-scope variable uses the same typed declaration as
             * a local: name: Type; or name: Type = initializer; */
            parse_file_global(module, t, Span(rel, line_no, 1),
                              scope_public, scope_file, NULL, 0);
        } else if(mode == TOP && strstr(t, "::") != NULL &&
                  strstr(t, "#type") != NULL) {
            die_at(Span(rel, line_no, 1),
                   "C declarator #type is not Jai syntax; use Name :: #type (arguments) -> Result;");
        } else if(mode == TOP && strstr(t, "::") != NULL &&
                  starts_word(skip_ws(strstr(t, "::") + 2),
                              "specialize")) {
            die_at(Span(rel, line_no, 1),
                   "use Jai-style type application: Name :: Generic(Type)");
        } else if(mode == TOP && strstr(t, "::") != NULL &&
                  !starts_word(skip_ws(strstr(t, "::") + 2), "struct") &&
                  !starts_word(skip_ws(strstr(t, "::") + 2), "union") &&
                  !starts_word(skip_ws(strstr(t, "::") + 2), "enum") &&
                  !starts_word(skip_ws(strstr(t, "::") + 2), "enum_flags") &&
                  !starts_word(skip_ws(strstr(t, "::") + 2), "variant") &&
                  !looks_like_function_header(t)) {
            /* 'Name :: expr' is the Ziran constant declaration. Ordinary
             * constants are also emitted into generated interfaces so public
             * types can use them in array bounds. Platform predicates remain
             * frontend-only because #defined is not a C expression. */
            const char *colons = strstr(t, "::");
            char *expr = (char *)colons + 2;
            char cname[ZIR_NAME_MAX];
            size_t cn = 0;
            const char *q = t;

            while(q < colons && (isalnum((unsigned char)*q) || *q == '_') &&
                  cn + 1 < sizeof(cname))
                cname[cn++] = *q++;
            cname[cn] = '\0';
            while(*expr == ' ' || *expr == '\t')
                expr++;
            size_t expression_length = strlen(expr);
            while(expression_length > 0 &&
                  isspace((unsigned char)expr[expression_length - 1]))
                expr[--expression_length] = '\0';
            if(expression_length > 0 && expr[expression_length - 1] == ';')
                expr[--expression_length] = '\0';
            while(expression_length > 0 &&
                  isspace((unsigned char)expr[expression_length - 1]))
                expr[--expression_length] = '\0';
            if(find_unquoted_word(expr, "#this") != NULL)
                die_at(Span(rel, line_no, 1),
                       "#this requires a procedure or type scope");
            if(cname[0] != '\0' && *expr != '\0') {
                if(starts_word(expr, "#define"))
                    die_at(Span(rel, line_no, 1), "use '%s :: value'; #define is not Ziran syntax", cname);
                else {
                    char run_value[ZIR_TEXT_MAX];
                    char selected_value[ZIR_TEXT_MAX];
                    ZirDefine *def;
                    int deferred_run = 0;

                    if(consts.count == consts.capacity) {
                        int capacity = consts.capacity > 0 ? consts.capacity * 2 : 16;
                        ZirConst *items = realloc(consts.items,
                            (size_t)capacity * sizeof(*items));

                        if(items == NULL)
                            die("out of memory");
                        consts.items = items;
                        consts.capacity = capacity;
                    }
                    if(starts_word(expr, "#run")) {
                        char expanded[ZIR_TEXT_MAX];
                        long value = 0;
                        int folded;
                        ZirConsts visible = visible_compile_constants(
                            &consts, &future_constants);
                        ZirModule run_scope = visible_compile_module(
                            module, &compile_context, &visible);

                        expand_compile_expr(expanded, sizeof(expanded),
                                            &visible, trim((char *)(expr + 4)),
                                            rel);
                        folded = strstr(expanded, "size_of") == NULL &&
                                 eval_const_condition(expanded, &value,
                                                     &run_scope,
                                                     &visible, rel, line_no, 1);
                        if(folded)
                            snprintf(run_value, sizeof(run_value),
                                     "%ld", value);
                        else {
                            CompileValue typed = {0};
                            int fuel = 10000;
                            if(compile_context.resolver != NULL &&
                               !compile_context.resolver(
                                   compile_context.resolver_context, program,
                                   &run_scope, path, root, expanded))
                                die_at(Span(rel, line_no, 1),
                                       "cannot resolve imports for #run expression");
                            if(evaluate_typed_expression(&run_scope, &visible,
                                    expanded, Span(rel, line_no, 1), 1,
                                    &fuel, &typed) &&
                               typed.kind != COMPILE_INVALID) {
                                copy_text(run_value, sizeof(run_value),
                                          typed.literal);
                                folded = 1;
                            }
                        }
                        if(!folded) {
                            int written = snprintf(run_value,
                                sizeof(run_value), "#run %s",
                                trim((char *)(expr + 4)));
                            if(written < 0 ||
                               (size_t)written >= sizeof(run_value))
                                die_at(Span(rel, line_no, 1),
                                       "#run expression is too long");
                            deferred_run = 1;
                        }
                        free_visible_compile_module(&run_scope);
                        free(visible.items);
                        expr = run_value;
                    }
                    if(!deferred_run &&
                       find_unquoted_text(expr, "#ifx") != NULL) {
                        copy_text(selected_value, sizeof(selected_value), expr);
                        lower_compile_ifx_value(selected_value,
                                                sizeof(selected_value), module,
                                                &consts, Span(rel, line_no, 1),
                                                1);
                        expr = selected_value;
                    }
                    if(!deferred_run &&
                       find_unquoted_text(expr, "#ifx") == NULL &&
                       find_unquoted_word(expr, "size_of") != NULL &&
                       find_unquoted_word(expr, "type_of") == NULL) {
                        if(expr != selected_value)
                            copy_text(selected_value,
                                      sizeof(selected_value), expr);
                        lower_size_of_value(selected_value,
                                            sizeof(selected_value), module,
                                            Span(rel, line_no, 1));
                        expr = selected_value;
                    }
                    int known_alias = 0;
                    if(is_identifier_text(expr)) {
                        for(int i = 0; i < module->define_count; i++) {
                            if(!strcmp(module->defines[i].name, expr)) {
                                known_alias = 1;
                                break;
                            }
                        }
                        /* Platform predicates stay out of the define table,
                         * so also consult the local constant table: an alias
                         * of a declared constant is a define, not an open
                         * enum member reference. */
                        if(!known_alias) {
                            for(int i = 0; i < consts.count; i++) {
                                if(!strcmp(consts.items[i].name, expr)) {
                                    known_alias = 1;
                                    break;
                                }
                            }
                        }
                    }
                    int using_alias = module->using_count > 0 &&
                        is_identifier_text(expr) && !known_alias &&
                        strcmp(expr, "true") && strcmp(expr, "false") &&
                        strcmp(expr, "null");
                    /* A platform predicate may negate or group
                     * `#defined`, as in `!#defined(_WIN32) && ...`. */
                    if(find_unquoted_text(expr, "#defined") == NULL &&
                       (!is_identifier_text(expr) || known_alias ||
                        using_alias)) {
                        def = ModuleAddDefine(module, cname, expr,
                                                 Span(rel, line_no, 1));
                        if(def != NULL) {
                            def->is_public = scope_public;
                            def->is_file_private = scope_file;
                            def->requires_open_enum = using_alias;
                        }
                    }
                    memset(&consts.items[consts.count], 0,
                           sizeof(consts.items[consts.count]));
                    snprintf(consts.items[consts.count].name,
                             sizeof(consts.items[0].name), "%s", cname);
                    snprintf(consts.items[consts.count].expr,
                             sizeof(consts.items[0].expr), "%s", expr);
                    copy_text(consts.items[consts.count].path,
                              sizeof(consts.items[0].path), rel);
                    consts.items[consts.count].is_file_private = scope_file;
                    consts.items[consts.count].is_public = scope_public;
                    consts.items[consts.count].source_line = line_no;
                    consts.count++;
                }
            }
        } else if(mode == TOP && strstr(t, "::") != NULL) {
            /* Named records and enums. */
            const char *colons = strstr(t, "::");
            const char *after = colons + 2;
            char tname[ZIR_NAME_MAX];
            size_t tn = 0;

            after = skip_ws(after);
            if(starts_word(after, "variant"))
                die_at(Span(rel, line_no, 1),
                       "variant declarations are not Jai syntax; use a struct or union");
            int enum_flags = strncmp(after, "enum_flags", 10) == 0 &&
                (after[10] == '\0' || isspace((unsigned char)after[10]) ||
                 after[10] == '{');
            int named_enum = enum_flags ||
                (strncmp(after, "enum", 4) == 0 &&
                 (after[4] == '\0' || isspace((unsigned char)after[4]) ||
                  after[4] == '{'));
            int named_union = strncmp(after, "union", 5) == 0 &&
                (after[5] == '\0' || after[5] == ' ' ||
                 after[5] == '(' || after[5] == '{');
            if((strncmp(after, "struct", 6) == 0 &&
                (after[6] == '\0' || after[6] == ' ' ||
                 after[6] == '(' || after[6] == '{')) || named_union ||
               named_enum) {
                const char *q = t;
                ZirType *ty;

                while(q < colons && (isalnum((unsigned char)*q) || *q == '_') &&
                      tn + 1 < sizeof(tname))
                    tname[tn++] = *q++;
                tname[tn] = '\0';
                char parameters[ZIR_NAME_MAX] = "";
                if(skip_ws(q) != colons)
                    die_at(Span(rel, line_no, 1),
                           "type declarations require a plain name before ::");
                if(!is_identifier_text(tname))
                    die_at(Span(rel, line_no, 1), "invalid type name");
                if((strncmp(after, "struct", 6) == 0 || named_union) &&
                   !parse_type_parameters(after, named_union ? "union" : "struct", parameters,
                                          sizeof(parameters)))
                    die_at(Span(rel, line_no, 1),
                           "polymorphic type parameters require (T: Type, ...)");
                ty = ModuleAddType(module, tname,
                                      Span(rel, line_no, 1));
                if(ty != NULL) {
                    ty->is_public = scope_public;
                    ty->is_file_private = scope_file;
                    ty->is_enum = named_enum;
                    ty->is_union = named_union;
                    ty->is_enum_flags = enum_flags;
                    if(ty->is_enum)
                        parse_enum_backing(ty, after);
                    ty->is_record_template = parameters[0] != '\0' &&
                        (strncmp(after, "struct", 6) == 0 || named_union);
                    copy_text(ty->template_params,
                              sizeof(ty->template_params), parameters);
                    if(strstr(after, "#extern") != NULL)
                        die_at(Span(rel, line_no, 1),
                               "#extern is not Jai syntax for a type declaration");
                    const char *opening = strchr(after, '{');
                    const char *closing = opening ? strchr(opening + 1, '}') : NULL;
                    if(closing != NULL) {
                        size_t body_length = (size_t)(closing - opening - 1);
                        if(ty->is_record_template) {
                            if(body_length + 1 >= sizeof(ty->body))
                                die_at(ty->span, "generic record body exceeds size limit");
                            memcpy(ty->body, opening + 1, body_length);
                            int nesting = 0;
                            for(size_t byte = 0; byte < body_length; byte++) {
                                if(ty->body[byte] == '[' || ty->body[byte] == '(')
                                    nesting++;
                                if(ty->body[byte] == ']' || ty->body[byte] == ')')
                                    nesting--;
                                if((ty->body[byte] == ',' ||
                                    ty->body[byte] == ';') && nesting == 0)
                                    ty->body[byte] = '\n';
                            }
                            ty->body[body_length] = '\n';
                            ty->body[body_length + 1] = '\0';
                        } else {
                            if(body_length >= sizeof(ty->body))
                                die_at(ty->span, "enum body exceeds size limit");
                            memcpy(ty->body, opening + 1, body_length);
                            ty->body[body_length] = '\0';
                            for(size_t byte = 0; byte < body_length; byte++)
                                if(ty->body[byte] == ';') ty->body[byte] = '\n';
                            if(ty->is_enum)
                                lower_enum_values(ty);
                        }
                        if(!take_abi_incomplete(ty))
                            die_at(ty->span,
                                   "#abi_incomplete must appear alone in a record body");
                        if(!expand_type_this(ty))
                            die_at(ty->span, "#this type body exceeds size limit");
                    } else {
                        mode = TYPE;
                        type_frame_count = 0;
                    }
                }
                fn = NULL;
            }
        } else if((mode == TOP || mode == TYPE) &&
                  starts_word(t, "#enum")) {
            die_at(Span(rel, line_no, 1),
                   "#enum is not Jai syntax; use Name :: enum { ... }");
        } else if(mode == TYPE) {
            if(cond_top_step(t, type_frames, &type_frame_count,
                             module, &consts, &compile_context,
                             path, rel, line_no, NULL))
                continue;
            if(type_frame_count > 0 &&
               !type_frames[type_frame_count - 1].active)
                continue;
            if(t[0] == '}') {
                ZirType *ty = &module->types[module->type_count - 1];
                if(!take_abi_incomplete(ty))
                    die_at(ty->span,
                           "#abi_incomplete must appear alone in a record body");
                if(!expand_type_this(ty))
                    die_at(ty->span, "#this type body exceeds size limit");
                if(ty->is_enum)
                    lower_enum_values(ty);
                mode = TOP;
                cond_frame_settle(tframes, tframe_count);
            } else if(strcmp(t, "{") == 0 &&
                      module->types[module->type_count - 1].body[0] == '\0') {
                /* Jai commonly places a type's opening brace on the next line. */
            } else if(starts_word(t, "#abi_incomplete")) {
                ZirType *ty = &module->types[module->type_count - 1];
                size_t length = strlen(t);

                if(ty->is_enum || ty->is_abi_incomplete ||
                   !line_is_abi_incomplete(t, length))
                    die_at(Span(rel, line_no, 1),
                           "#abi_incomplete must appear alone in a record body");
                ty->is_abi_incomplete = 1;
            } else if(t[0] == '#') {
                die_at(Span(rel, line_no, 1),
                       "unknown type-body directive: %s", t);
            } else {
                ZirType *ty = &module->types[module->type_count - 1];
                char *body = ty->body;
                size_t capacity = sizeof(ty->body);
                size_t used = strlen(body);
                int written = snprintf(body + used, capacity - used, "%s\n", t);
                if(written < 0 || (size_t)written >= capacity - used)
                    die_at(ty->span, "type body exceeds size limit");
                if(!ty->is_record_template)
                    for(size_t byte = used; byte < used + (size_t)written; byte++)
                        if(body[byte] == ';') body[byte] = '\n';
            }
        } else if(mode == FUNCTION) {
            char *bcnd = NULL;
            int bck;
            int parallel_for = 0;
            int parallel_gpu = 0;
            char parallel_text[SOURCE_LINE_MAX * 2];
            if(starts_word(t, "#parallel_gpu")) {
                const char *after = skip_ws(t + strlen("#parallel_gpu"));
                if(!starts_word(after, "for"))
                    die_at(Span(rel, line_no, 1),
                           "#parallel_gpu requires a for region");
                parallel_for = 1;
                parallel_gpu = 1;
                snprintf(parallel_text, sizeof(parallel_text), "%s", after);
                t = trim(parallel_text);
            } else if(starts_word(t, "#parallel")) {
                const char *after = skip_ws(t + strlen("#parallel"));
                if(!starts_word(after, "for"))
                    die_at(Span(rel, line_no, 1),
                           "#parallel requires a for region");
                parallel_for = 1;
                snprintf(parallel_text, sizeof(parallel_text), "%s", after);
                t = trim(parallel_text);
            }
            bck = parse_cond_start(t, &bcnd);

            int compile_else = body_mcount > 0 &&
                depth == body_mdepth[body_mcount - 1] &&
                line_is_compile_else(t);
            int compile_else_if = bck == 2 && body_mcount > 0 &&
                depth == body_mdepth[body_mcount - 1];

            if(bck == 1) {
                int parent_active = body_mcount == 0 ||
                    body_mactive[body_mcount - 1];
                int chosen = parent_active &&
                    select_compile_condition(module, &consts, bcnd,
                                             Span(rel, line_no, 1),
                                             &compile_context, path, NULL);
                if(body_mcount >= 8)
                    die_at(Span(rel, line_no, 1), "too many nested #if blocks");
                body_mdepth[body_mcount] = depth;
                body_mselected[body_mcount] = chosen;
                body_mparent_active[body_mcount] = parent_active;
                body_mactive[body_mcount] = parent_active && chosen;
                body_mcount++;
            } else if(compile_else_if) {
                int i = body_mcount - 1;
                int chosen = body_mparent_active[i] &&
                    !body_mselected[i] &&
                    select_compile_condition(module, &consts, bcnd,
                                             Span(rel, line_no, 1),
                                             &compile_context, path, NULL);
                body_mactive[i] = body_mparent_active[i] &&
                    !body_mselected[i] && chosen;
                body_mselected[i] |= chosen;
            } else if(compile_else) {
                int i = body_mcount - 1;
                body_mactive[i] = body_mparent_active[i] &&
                    !body_mselected[i];
                body_mselected[i] = 1;
            } else if(bck == 2) {
                die_at(Span(rel, line_no, 1),
                       "else #if without a matching #if");
            } else if(t[0] == '}' && body_mcount > 0 &&
                      depth == body_mdepth[body_mcount - 1]) {
                /* this '}' closes a body-level '#if' region, not a block */
                body_mcount--;
            } else if(body_mcount > 0 &&
                      !body_mactive[body_mcount - 1]) {
                depth += net_block_braces(t);
            } else if(t[0] == '#' && strcmp(t, "#through") != 0 &&
                      strcmp(t, "#through;") != 0 &&
                      !starts_word(t, "#parallel")) {
                die_at(Span(rel, line_no, 1),
                       "unknown function-body directive: %s", t);
            } else if(t[0] == '}') {
                /* A leading brace closes the if-body. A braced else opens a
                 * new body here; a single-statement else is braced during
                 * normalization. Record the close and following else as
                 * separate checked statements. */
                const char *eq = t + 1;

                while(*eq == ' ' || *eq == '\t')
                    eq++;
                if(depth > 1 && starts_word(eq, "else")) {
                    split_jai_control_line((char *)eq,
                                           SOURCE_LINE_MAX * 4 - (size_t)(eq - t),
                                           onelineq, &onelineq_count,
                                           Span(rel, line_no, 1));
                    ZirSourceSpan span = SpanEnd(rel, line_no,
                                                    pending_start_column +
                                                    (int)(eq - t),
                                                    line_no,
                                                    pending_end_column);
                    FunctionAddStmt(fn, ZIR_STMT_BLOCK_CLOSE, "}",
                                       Span(rel, line_no, 1));
                    FunctionAddStmt(fn, ZIR_STMT_IF, eq, span);
                    depth += net_block_braces(t);
                } else {
                    if(depth > 0)
                        depth--;
                    if(depth == 0) {
                        mode = TOP;
                        cond_frame_settle(tframes, tframe_count);
                        fn = NULL;
                    } else {
                        FunctionAddStmt(fn, ZIR_STMT_BLOCK_CLOSE, t,
                                           Span(rel, line_no, 1));
                    }
                }
            } else {
                int using_binding = 0;
                char using_filter[160];
                char using_path_text[SOURCE_LINE_MAX * 2];
                using_filter[0] = '\0';
                using_path_text[0] = '\0';
                if(starts_word(t, "using") ||
                   strncmp(t, "using,", 6) == 0) {
                    if(!isspace((unsigned char)t[5]) && t[5] != ',')
                        die_at(Span(rel, line_no, 1),
                               "using modifiers are not supported");
                    const char *after = skip_ws(t + 5);
                    parse_using_modifiers(&after, using_filter,
                                          sizeof(using_filter), rel, line_no);
                    snprintf(using_path_text, sizeof(using_path_text), "%s",
                             after);
                    t = trim(using_path_text);
                    if(*t == '\0')
                        die_at(Span(rel, line_no, 1),
                               "using needs a record binding");
                    using_binding = 1;
                }
                if(starts_word(t, "match")) {
                    const char *after_match = skip_ws(t + 5);
                    if((t[5] == '!' || t[5] == '?' ||
                        (isspace((unsigned char)t[5]) &&
                         *after_match != '=' && *after_match != ':' &&
                         *after_match != '+' && *after_match != '-' &&
                         *after_match != '*' && *after_match != '/' &&
                         *after_match != '%' && *after_match != '&' &&
                         *after_match != '|' && *after_match != '^' &&
                         *after_match != '.' && *after_match != '(')) &&
                       strchr(t, '{') != NULL)
                        die_at(Span(rel, line_no, 1),
                               "match is not Jai syntax; use if value == { case ... }");
                }
                ZirStmtKind kind = classify_stmt(t);
                if(using_binding && kind == ZIR_STMT_UNKNOWN)
                    kind = ZIR_STMT_EXPR;
                if(using_binding && kind != ZIR_STMT_DECL &&
                   kind != ZIR_STMT_EXPR)
                    die_at(Span(rel, line_no, 1),
                           "using needs a record binding");
                int brace_delta = net_block_braces(t);
                char block_callee[ZIR_NAME_MAX];
                char block_name[ZIR_NAME_MAX];

                if(looks_like_non_jai_control(t, "guard"))
                    die_at(Span(rel, line_no, 1),
                           "guard is not Jai syntax; use if and return");
                if(t[0] == 'c' && isspace((unsigned char)t[1])) {
                    const char *rest = skip_ws(t + 1);
                    if(isalpha((unsigned char)*rest) || *rest == '_')
                        die_at(Span(rel, line_no, 1),
                               "raw C statements are not Jai syntax; use #foreign");
                }
                if(looks_like_non_jai_control(t, "switch"))
                    die_at(Span(rel, line_no, 1),
                           "switch is not Jai syntax; use if value == { case ... }");
                if(looks_like_non_jai_control(t, "goto") ||
                   looks_like_label(t))
                    die_at(Span(rel, line_no, 1),
                           "goto and labels are not Jai syntax; use structured control flow");
                if(kind == ZIR_STMT_DECL && strchr(t, ':') == NULL)
                    die_at(Span(rel, line_no, 1),
                           "C-style local declarations are not Jai syntax; use name: Type");

                if(parse_block_call_header(t, block_callee,
                                         sizeof(block_callee),
                                         block_name, sizeof(block_name))) {
                    die_at(Span(rel, line_no, 1),
                           "block calls are not Jai syntax; call with a record literal");
                }

                {
                    ZirSourceSpan span = SpanEnd(rel, pending_start_line,
                                                    pending_start_column,
                                                    line_no,
                                                    pending_end_column);
                    if(kind == ZIR_STMT_IF && starts_word(t, "if")) {
                        const char *condition = skip_ws(t + 2);
                        if(starts_word(condition, "#complete"))
                            condition = skip_ws(condition + strlen("#complete"));
                        const char *equals = strstr(condition, "==");
                        if(equals != NULL && *skip_ws(equals + 2) == '{' &&
                           *skip_ws(skip_ws(equals + 2) + 1) == '\0')
                            kind = ZIR_STMT_IF_CASE;
                    }
                    ZirStmt *statement = FunctionAddStmt(fn, kind, t, span);
                    if(statement != NULL && parallel_for) {
                        if(kind != ZIR_STMT_FOR)
                            die_at(span, "#parallel requires a for region");
                        statement->is_parallel = 1;
                        statement->is_gpu = parallel_gpu;
                    }
                    if(statement != NULL && using_binding) {
                        statement->is_using = 1;
                        if(kind == ZIR_STMT_EXPR) {
                            char name[ZIR_NAME_MAX];
                            if(strlen(t) >= sizeof(name))
                                die_at(span, "using field path is too long");
                            copy_text(name, sizeof(name), t);
                            size_t length = strlen(name);
                            if(length > 0 && name[length - 1] == ';')
                                name[--length] = '\0';
                            trim_in_place(name);
                            if(!is_member_path_text(name))
                                die_at(span,
                                       "using needs a record binding or field path");
                            copy_text(statement->name,
                                      sizeof(statement->name), name);
                            copy_text(statement->type,
                                      sizeof(statement->type), using_filter);
                        }
                    }
                }
                depth += brace_delta;
                if(depth < 0)
                    depth = 0;
            }
        } else if(mode == TOP && t[0] != '\0') {
            die_at(Span(rel, line_no, 1), "invalid top-level declaration");
        }
    }
    if(mode != TOP)
        die_at(Span(rel, line_no, 1),
               "unterminated declaration or function body");
    if(tframe_count > 0 || type_frame_count > 0 ||
       body_mcount > 0)
        die_at(Span(rel, line_no, 1), "unterminated #if block");
    if(program_export)
        die_at(Span(rel, program_export_line, 1),
               "#program_export must precede a function declaration");
    module = &program->modules[0];
    if(module->using_count > 0)
        for(int i = 0; i < consts.count; i++) {
            const ZirConst *constant = &consts.items[i];
            if(!is_identifier_text(constant->expr) ||
               !strcmp(constant->expr, "true") ||
               !strcmp(constant->expr, "false") ||
               !strcmp(constant->expr, "null")) continue;
            int emitted = 0;
            for(int d = 0; d < module->define_count; d++)
                if(!strcmp(module->defines[d].name, constant->name)) {
                    emitted = 1;
                    break;
                }
            if(emitted) continue;
            ZirDefine *def = ModuleAddDefine(module, constant->name,
                constant->expr, Span(constant->path,
                                      constant->source_line, 1));
            if(def == NULL)
                die_at(Span(constant->path, constant->source_line, 1),
                       "out of memory recording enum constant");
            def->is_public = constant->is_public;
            def->is_file_private = constant->is_file_private;
            def->requires_open_enum = 1;
        }
    for(int mi = 0; mi < program->module_count; mi++) {
        ZirModule *module = &program->modules[mi];
        add_default_helpers(program, module, path, root,
                            resolver, resolver_context);
        for(int gi = 0; gi < module->global_count; gi++) {
            ZirGlobal *global = &module->globals[gi];
            copy_text(module->lookup_path, sizeof(module->lookup_path),
                      global->span.path);
            if(find_unquoted_text(global->init, "#ifx") != NULL)
                lower_compile_ifx_value(global->init, sizeof(global->init),
                                        module, &consts, global->span, 1);
            if(find_unquoted_text(global->init, "#ifx") == NULL &&
               find_unquoted_word(global->init, "size_of") != NULL &&
               find_unquoted_word(global->init, "type_of") == NULL)
                lower_size_of_value(global->init, sizeof(global->init),
                                    module, global->span);
        }
        for(int fi = 0; fi < module->function_count; fi++) {
            ZirFunction *fn = &module->functions[fi];
            copy_text(module->lookup_path, sizeof(module->lookup_path),
                      fn->span.path);
            int has_compile_ifx = 0;
            for(int si = 0; si < fn->stmt_count; si++)
                if(find_unquoted_text(fn->stmts[si].text, "#ifx") != NULL) {
                    has_compile_ifx = 1;
                    break;
                }
            if(has_compile_ifx)
                lower_compile_ifx_function(fn, module, &consts, 1);
            if(!NormalizeJaiBodies(fn) || !BindJaiLoopControls(fn) ||
               !LowerCleanup(fn)) {
                ProgramFree(program);
                free(consts.items);
                free(future_constants.items);
                free(future_usings.items);
                free(future_imports.items);
                free(future_types.items);
                free_deferred_types(&deferred_types);
                free_discovered_functions(&future_functions);
                free(future_globals.items);
                free_discovered_files(&discovered_files);
                free(canonical);
                return NULL;
            }
            if(!LowerJaiFor(fn, module)) {
                ProgramFree(program);
                free(consts.items);
                free(future_constants.items);
                free(future_usings.items);
                free(future_imports.items);
                free(future_types.items);
                free_deferred_types(&deferred_types);
                free_discovered_functions(&future_functions);
                free(future_globals.items);
                free_discovered_files(&discovered_files);
                free(canonical);
                return NULL;
            }
            StructureFunction(fn, module);
        }
        module->lookup_path[0] = '\0';
    }
    free(consts.items);
    free(future_constants.items);
    free(future_usings.items);
    free(future_imports.items);
    free(future_types.items);
    free_deferred_types(&deferred_types);
    free_discovered_functions(&future_functions);
    free(future_globals.items);
    free_discovered_files(&discovered_files);
    free(canonical);
    return program;
}

ZirProgram *
parse_file_with_imports(const char *path, const char *root,
                        ZirCompileImportResolver resolver, void *context)
{
    return parse_file_with_imports_defined(path, root, resolver, context,
                                           NULL, 0);
}

ZirProgram *
parse_file_with_imports_defined(const char *path, const char *root,
                                ZirCompileImportResolver resolver,
                                void *context, const char *const *defines,
                                int define_count)
{
    char *lowered = read_lowered_source(path);
    ZirProgram *program = parse_source(path, root, lowered,
                                       resolver, context,
                                       defines, define_count);
    free(lowered);
    return program;
}

ZirProgram *
parse_file(const char *path, const char *root)
{
    return parse_file_with_imports(path, root, NULL, NULL);
}

ZirProgram *
parse_source_text(const char *path, const char *source)
{
    char *lowered = lower_jai_multiline_strings(source, path);
    ZirProgram *program = parse_source(path, ".", lowered, NULL, NULL,
                                       NULL, 0);
    free(lowered);
    return program;
}
