#include "../headers/codegen/main.h"
#include "registers.c"

//======================MZ_CODEGEN_UTILS BEGINS======================
void write_bytes(char *code, FILE *outfile) {
    if (!outfile || !code) {
        return;
    }
    if (fwrite(code, 1, strlen(code), outfile) != strlen(code)) {
        Error err;
        ERROR_PREP(err, ERROR_GENERIC,
                   "write_bytes(): Cannot write '%s' bytes to asm file.", code);
        log_error(&err);
        exit(0);
    }
}
void write_line(char *code, FILE *outfile) {
    if (!outfile || !code) {
        return;
    }
    if (fwrite(code, 1, strlen(code), outfile) != strlen(code)) {
        Error err;
        ERROR_PREP(err, ERROR_GENERIC,
                   "write_line(): Cannot write '%s' bytes to asm file.", code);
        log_error(&err);
        exit(0);
    }
    if (fwrite("\n", 1, 1, outfile) != 1) {
        Error err;
        ERROR_PREP(
            err, ERROR_GENERIC,
            "write_line(): Cannot write '<NEW_LINE>' bytes to asm file.");
        log_error(&err);
        exit(0);
    }
}
void write_integer(long long integer, FILE *outfile) {
    if (!outfile) {
        return;
    }
    if (sprintf(CG_INT_BUFFER, "%lld", integer) < 0) {
        Error err;
        ERROR_PREP(err, ERROR_GENERIC,
                   "write_integer(): Cannot integer %lld to buffer.", integer);
        log_error(&err);
        exit(0);
    }
    if (fwrite(CG_INT_BUFFER, 1, strlen(CG_INT_BUFFER), outfile) !=
        strlen(CG_INT_BUFFER)) {
        Error err;
        ERROR_PREP(err, ERROR_GENERIC,
                   "write_line(): Cannot write '%s' bytes to asm file.",
                   CG_INT_BUFFER);
        log_error(&err);
        exit(0);
    }
}

CodegenContext *codegen_context_create(CodegenContext *parent) {
    CodegenContext *cg_ctx = calloc(1, sizeof(CodegenContext));
    cg_ctx->parent = parent;
    cg_ctx->locals = environment_create(NULL);
    return cg_ctx;
}

//======================MZ_CODEGEN_UTILS ENDS========================

char *label_generate() {
    char *label = CG_LABEL_BUFFER + label_index;
    label_index += snprintf(label, CG_LABEL_BUFFER_LENGTH - label_index,
                            ".L%ld:\n", label_count);
    label_index++;
    if (label_index >= CG_LABEL_BUFFER_LENGTH) {
        label_index = 0;
        return label_generate();
    }
    label_count++;
    return label;
}

//======================MZ_CODEGEN_COMMON BEGINS=====================

//======================MZ_CODEGEN_COMMON ENDS=======================

//======================MZ_CODEGEN_OUTPUT_FORMAT_x86_64_ASM_MSWIN
// BEGINS======================

char *symbol_to_addr(Node *symbol) {
    char *symbol_str = CG_SYMBOL_BUFFER + symbol_index;
    // For global variables
    symbol_index += snprintf(symbol_str, CG_SYMBOL_BUFFER_LENGTH - symbol_index,
                             "%s(%%rip)", symbol->value.symbol);
    // TODO: Local Variables
    symbol_index++;
    if (symbol_index >= CG_SYMBOL_BUFFER_LENGTH) {
        symbol_index = 0;
        return symbol_to_addr(symbol);
    }
    return symbol_str;
}

Error codegen_function_x86_64_att_mswin(ParsingContext *context,
                                        CodegenContext *cg_context, Register *r,
                                        char *name, Node *function,
                                        FILE *outfile);

Error codegen_expression_x86_64_mswin(ParsingContext *context,
                                      CodegenContext *cg_context, Register *r,
                                      Node *expression, FILE *outfile) {
    Error err = OK;
    switch (expression->type) {
    case NODE_TYPE_FUNCTION_DECLARATION:
        if (!cg_context->parent) {
            break;
        }
        char *buffer = (char *)malloc(256 * sizeof(char));
        buffer = label_generate();
        err = codegen_function_x86_64_att_mswin(context, cg_context, r, buffer,
                                                expression, outfile);
        ret;
        break;
    case NODE_TYPE_DEBUG_PRINT_INTEGER:
        // LINE(";#; DEBUG INTEGER PRINTING");
        LINE("mov %%rax, %%rbx");
        LINE("lea fmt(%%rip), %%rcx");
        LINE("lea %s, %%r12", symbol_to_addr(expression->children));
        LINE("mov (%%r12), %%rdx");
        LINE("call printf");
        LINE("mov %%rax, %%rbx");
        // LINE(";#; DEBUG INTEGER PRINTING");

        break;
    case NODE_TYPE_VARIABLE_DECLARATION:
        if (!cg_context->parent) {
            LINE("lea %s, %%rax", symbol_to_addr(expression->children));
            if (expression->children->next_child->type == NODE_TYPE_NULL) {
                LINE("movq $0, (%%rax)");
            } else {
                BYTES("movq $");
                write_integer(
                    expression->children->next_child->value.MZ_integer,
                    outfile);
                LINE(", (%%rax)");
            }
        }
        log_message("[TODO] Local Variable Declaration");
        break;
    case NODE_TYPE_INTEGER:
        expression->result_register = register_allocate(r);
        LINE("mov $%lld, %s", expression->value.MZ_integer,
             register_name(r, expression->result_register));
        ;
        break;
    case NODE_TYPE_VARIABLE_REASSIGNMENT:
        if (cg_context->parent) {
            log_message("[TODO] Local Variable Reassignment");
        } else {
            if (expression->children->next_child->type == NODE_TYPE_INTEGER) {
                BYTES("movq $");
                write_integer(
                    expression->children->next_child->value.MZ_integer,
                    outfile);
                LINE(", %s", symbol_to_addr(expression->children));
            } else {
                err = codegen_expression_x86_64_mswin(
                    context, cg_context, r, expression->children->next_child,
                    outfile);
                ret;
                char *result_register = register_name(
                    r, expression->children->next_child->result_register);
                LINE("mov %s, %s", result_register,
                     symbol_to_addr(expression->children));
                register_deallocate(
                    r, expression->children->next_child->result_register);
            }
        }
        break;

    default:
        break;
    }
    return err;
}

void codegen_header_x86_64_att_mswin(FILE *outfile) {
    char *lines[5] = {";#; ==== ALIGN HEADER ====", "push %rbp",
                      "mov %rsp, %rbp", "sub $32, %rsp", ""};
    for (int i = 0; i < 5; i++) {
        write_line(lines[i], outfile);
    }
}

void codegen_footer_x86_64_att_mswin(FILE *outfile, bool exit_code) {
    if (!exit_code) {
        char *lines[5] = {"", "add $32, %rsp", "pop %rbp", "ret",
                          ";#; ==== ALIGN FOOTER ===="};
        for (int i = 0; i < 5; i++) {
            write_line(lines[i], outfile);
        }
    } else {
        char *lines[6] = {
            "",         "mov $0, %rax", "add $32, %rsp",
            "pop %rbp", "ret",          ";#; ==== ALIGN FOOTER ===="};
        for (int i = 0; i < 6; i++) {
            write_line(lines[i], outfile);
        }
    }
}

Error codegen_function_x86_64_att_mswin(ParsingContext *context,
                                        CodegenContext *cg_context, Register *r,
                                        char *name, Node *function,
                                        FILE *outfile) {
    Error err = OK;
    cg_context = codegen_context_create(cg_context);

    size_t param_count = 1;
    Node *parameter = function->children->children;
    while (parameter) {
        Node *result = node_allocate();
        // TODO: Try to pass args to register
        environment_get(*context->types, parameter->children->next_child,
                        result);
        environment_set(cg_context->locals, parameter->children,
                        node_integer(-param_count * result->value.MZ_integer));
        parameter = parameter->next_child;
    }

    LINE("jmp MZ_fn_after%s", name);
    LINE("MZ_fn_%s:", name);

    codegen_header_x86_64_att_mswin(outfile);
    Node *expression = function->children->next_child->next_child->children;
    while (expression) {
        err = codegen_expression_x86_64_mswin(context, cg_context, r,
                                              expression, outfile);
        ret;
        expression = expression->next_child;
    }
    cg_context = cg_context->parent;
    codegen_footer_x86_64_att_mswin(outfile, false);

    LINE("MZ_fn_after%s:", name);

    return err;
}

Error codegen_program_x86_64_mswin(ParsingContext *context,
                                   CodegenContext *cg_context, Node *program,
                                   FILE *outfile) {
    Error err = OK;

    Register *r = register_create("%rax");
    register_add(r, "%r10");
    register_add(r, "%r11");

    LINE(";#; This assembly was generated for x86_64 Windows by the MZ "
         "Compiler");

    LINE(".data");

    write_line("\n;#; -------------------------Global Variables "
               "START-------------------------\n",
               outfile);

#ifdef DEBUG_COMPILER
    LINE("fmt: .asciz \"%%lld\\n\"");
#endif // DEBUG

    Binding *var_it = context->variables->binding;

    while (var_it) {
        Node *var_id = var_it->id;
        Node *type = var_it->value;
        Node *type_info = node_allocate();
        environment_get(*context->types, type, type_info);
        LINE("%s: .space %lld", var_id->value.symbol,
             type_info->value.MZ_integer);
        var_it = var_it->next;
    }

    write_line("\n;#; -------------------------Global Variables "
               "END-------------------------\n",
               outfile);

    LINE(".global _start");

    LINE(".text");
    write_line("\n;#; -------------------------Global Functions "
               "START-------------------------\n",
               outfile);

    Binding *func_it = context->functions->binding;

    while (func_it) {
        Node *func_id = func_it->id;
        Node *function = func_it->value;
        func_it = func_it->next;

        char *name = func_id->value.symbol;
        err = codegen_function_x86_64_att_mswin(context, cg_context, r, name,
                                                function, outfile);
        ret;
    }

    write_line("\n;#; -------------------------Global Functions "
               "END-------------------------\n",
               outfile);

    LINE("_start:");
    codegen_header_x86_64_att_mswin(outfile);

    Node *expression = program->children;
    while (expression) {
        codegen_expression_x86_64_mswin(context, cg_context, r, expression,
                                        outfile);
        expression = expression->next_child;
    }

    codegen_footer_x86_64_att_mswin(outfile, true);
    register_free(r);
    return err;
}

//======================MZ_CODEGEN_OUTPUT_FORMAT_x86_64_ASM_MSWIN
// ENDS======================

Error codegen_program(ParsingContext *context, Node *program,
                      CodegenOutputFormat format, char *filepath) {
    Error err = OK;
    FILE *outfile = fopen(filepath, "wb");

    CodegenContext *cg_context = codegen_context_create(NULL);

    if (!outfile) {
        ERROR_PREP(err, ERROR_GENERIC, "Cannot open file '%s' for codegen! ",
                   filepath);
        return err;
    }

    switch (format) {
    case MZ_CODEGEN_OUTPUT_FORMAT_x86_64_ASM_MSWIN:
        err =
            codegen_program_x86_64_mswin(context, cg_context, program, outfile);
        ret;
        break;

    default:
        err =
            codegen_program_x86_64_mswin(context, cg_context, program, outfile);
        ret;
        break;
    }

    fclose(outfile);
    return err;
}