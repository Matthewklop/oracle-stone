/* ============================================================================
 * stone_meta.c — Meta-Emitter: One binary, any target
 *
 * The parser stays the same. The emitter reads a target descriptor.
 * Adding a new language = writing a target descriptor, not a C program.
 *
 * Usage:
 *   ./stone_meta input.st --target python
 *   ./stone_meta input.st --target javascript
 *   ./stone_meta input.st --target lua
 *   ./stone_meta input.st --target go
 *   ./stone_meta input.st --target rust
 *
 * Target descriptors are embedded. Each one is ~50 lines of struct init.
 * Total: 1 parser + N descriptors = ~500 + 50N lines.
 * With 10 targets: ~1,000 lines. Half of what we had before.
 * ============================================================================
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

/* ─── Stone tokenizer (same grammar, reused) ─── */
typedef enum {
    TOK_IDENT, TOK_NUMBER, TOK_STRING,
    TOK_FN, TOK_END, TOK_VAR, TOK_IF, TOK_ELSE, TOK_LOOP,
    TOK_PRINT, TOK_PRINTN, TOK_RET,
    TOK_ASSIGN, TOK_PLUS, TOK_MINUS, TOK_MUL, TOK_DIV,
    TOK_GT, TOK_LT, TOK_EQ, TOK_AND, TOK_OR,
    TOK_NEWLINE, TOK_EOF
} TokenType;

static const char *token_names[] = {
    "ident", "number", "string",
    "fn", "end", "var", "if", "else", "loop",
    "print", "printn", "ret",
    "=", "+", "-", "*", "/",
    ">", "<", "=", "and", "or",
    "newline", "eof"
};

typedef struct { TokenType type; char text[64]; int line; int col; } Token;
typedef struct { const char *src; const char *pos; int line; int col; Token curr; int has_curr; } Lexer;

static void lex_init(Lexer *l, const char *src) {
    l->src = src; l->pos = src; l->line = 1; l->col = 1; l->has_curr = 0;
}

static int lex_next(Lexer *l, Token *t) {
    if (l->has_curr) { *t = l->curr; l->has_curr = 0; return 1; }
    const char *p = l->pos;
    while (*p == ' ' || *p == '\t' || *p == '\r') { p++; l->col++; }
    t->line = l->line; t->col = l->col; t->text[0] = 0;
    if (*p == 0) { t->type = TOK_EOF; l->pos = p; return 1; }
    if (*p == '\n') {
        t->type = TOK_NEWLINE; t->text[0] = '\n'; t->text[1] = 0;
        l->line++; l->col = 1; l->pos = p + 1; return 1;
    }
    if (*p == '#') { while (*p && *p != '\n') p++; l->pos = p; return lex_next(l, t); }
    if (*p == '"') {
        int i = 0; t->text[i++] = *p++;
        while (*p && *p != '"' && i < 62) { t->text[i++] = *p++; }
        if (*p == '"') { t->text[i++] = *p++; }
        t->text[i] = 0; t->type = TOK_STRING; l->pos = p; return 1;
    }
    if (isalpha(*p) || *p == '_') {
        int i = 0;
        while ((isalnum(*p) || *p == '_') && i < 63) t->text[i++] = *p++;
        t->text[i] = 0;
        if (strcmp(t->text, "fn") == 0) t->type = TOK_FN;
        else if (strcmp(t->text, "end") == 0) t->type = TOK_END;
        else if (strcmp(t->text, "var") == 0) t->type = TOK_VAR;
        else if (strcmp(t->text, "if") == 0) t->type = TOK_IF;
        else if (strcmp(t->text, "else") == 0) t->type = TOK_ELSE;
        else if (strcmp(t->text, "loop") == 0) t->type = TOK_LOOP;
        else if (strcmp(t->text, "print") == 0) t->type = TOK_PRINT;
        else if (strcmp(t->text, "printn") == 0) t->type = TOK_PRINTN;
        else if (strcmp(t->text, "ret") == 0) t->type = TOK_RET;
        else if (strcmp(t->text, "and") == 0) t->type = TOK_AND;
        else if (strcmp(t->text, "or") == 0) t->type = TOK_OR;
        else t->type = TOK_IDENT;
        l->pos = p; return 1;
    }
    if (*p >= '0' && *p <= '9') {
        int i = 0;
        while ((*p >= '0' && *p <= '9') && i < 63) t->text[i++] = *p++;
        t->text[i] = 0; t->type = TOK_NUMBER; l->pos = p; return 1;
    }
    t->type = TOK_EOF;
    switch (*p) {
        case '=': t->type = TOK_ASSIGN; break;
        case '+': t->type = TOK_PLUS; break;
        case '-': t->type = TOK_MINUS; break;
        case '*': t->type = TOK_MUL; break;
        case '/': t->type = TOK_DIV; break;
        case '>': t->type = TOK_GT; break;
        case '<': t->type = TOK_LT; break;
        default: t->type = TOK_EOF; return 0;
    }
    t->text[0] = *p; t->text[1] = 0; l->pos = p + 1; return 1;
}

static int lex_peek(Lexer *l, Token *t) {
    if (!l->has_curr) { lex_next(l, &l->curr); l->has_curr = 1; }
    *t = l->curr; return 1;
}

static int lex_expect(Lexer *l, TokenType type, Token *t) {
    lex_next(l, t);
    if (t->type != type) { fprintf(stderr, "Error L%d: expected %s, got %s\n", t->line, token_names[type], token_names[t->type]); return 0; }
    return 1;
}

static void lex_skip_newlines(Lexer *l) {
    Token t; while (1) { lex_peek(l, &t); if (t.type == TOK_NEWLINE) lex_next(l, &t); else break; }
}

/* ─── Target Descriptor ─── */
/* Each target maps every Stone construct to its syntax.
 * This is the COMPLETE specification of one language target. */
typedef struct {
    /* Function definition */
    const char *fn_prefix;     /* "def " or "function " or "fn " */
    const char *fn_suffix;     /* "):" or ") {" or ")" */
    const char *fn_close;      /* "" or "end" or "}" */
    const char *fn_call_open;  /* "(" */
    const char *fn_call_close; /* ")" */
    const char *fn_call_sep;   /* ", " */
    
    /* Variable */
    const char *var_prefix;    /* "local " or "let " or "" */
    const char *var_assign;    /* " = " */
    const char *var_suffix;    /* "" or ";" */
    
    /* Assignment */
    const char *assign_op;     /* " = " */
    const char *assign_suffix; /* "" or ";" */
    
    /* If/else */
    const char *if_prefix;     /* "if " */
    const char *if_suffix;     /* ":" or " then" or " {" */
    const char *if_close;      /* "end" or "}" */
    const char *else_prefix;   /* "else" */
    const char *else_suffix;   /* ":" or " {" */
    
    /* Loop */
    const char *loop_prefix;   /* "for " */
    const char *loop_var;      /* " = 0, " in Lua, " in range(" in Python */
    const char *loop_mid;      /* "):" or " do" or " {" */
    const char *loop_close;    /* "end" or "}" */
    
    /* Print */
    const char *print_prefix;  /* "print(" or "io.write(" or "console.log(" or "fmt.Printf(" */
    const char *print_no_nl;   /* ", end=\"\"" or "" or ")", just for print without newline */
    const char *print_with_nl; /* ")" or ");" */
    
    /* Return */
    const char *ret_prefix;    /* "return " */
    const char *ret_suffix;    /* "" or ";" */
    
    /* Expression operators (same across all targets) */
    const char *op_add;  const char *op_sub;  const char *op_mul;
    const char *op_div;  const char *op_gt;   const char *op_lt;
    const char *op_eq;
    const char *op_and;  const char *op_or;
    
    /* Structural */
    const char *line_end;      /* "" or ";" */
    const char *indent_str;    /* "    " or "\t" */
    const char *comment;       /* "# " or "// " or "-- " */
    const char *block_open;    /* "{" or "" */
    const char *block_close;   /* "}" or "end" */
    
    /* File header */
    const char *header;        /* "#!/usr/bin/env python3\n" or "// Generated\n" */
    const char *main_call;     /* "if __name__ == \"__main__\":\n    main()" or "main()" */
} TargetDesc;

/* ─── Embedded target descriptors ─── */
static const TargetDesc TARGET_PYTHON = {
    .fn_prefix = "def ", .fn_suffix = "):", .fn_close = "",
    .fn_call_open = "(", .fn_call_close = ")", .fn_call_sep = ", ",
    .var_prefix = "", .var_assign = " = ", .var_suffix = "",
    .assign_op = " = ", .assign_suffix = "",
    .if_prefix = "if ", .if_suffix = ":", .if_close = "",
    .else_prefix = "else", .else_suffix = ":",
    .loop_prefix = "for ", .loop_var = " in range(", .loop_mid = "):", .loop_close = "",
    .print_prefix = "print(", .print_no_nl = ", end=\"\"", .print_with_nl = ")",
    .ret_prefix = "return ", .ret_suffix = "",
    .op_add = " + ", .op_sub = " - ", .op_mul = " * ", .op_div = " / ",
    .op_gt = " > ", .op_lt = " < ", .op_eq = " == ",
    .op_and = " and ", .op_or = " or ",
    .line_end = "", .indent_str = "    ", .comment = "# ",
    .block_open = "", .block_close = "",
    .header = "#!/usr/bin/env python3\n# Generated by Stone (meta-emitter)\n\n",
    .main_call = "if __name__ == \"__main__\":\n    main()",
};

static const TargetDesc TARGET_JAVASCRIPT = {
    .fn_prefix = "function ", .fn_suffix = ") {", .fn_close = "}",
    .fn_call_open = "(", .fn_call_close = ")", .fn_call_sep = ", ",
    .var_prefix = "let ", .var_assign = " = ", .var_suffix = ";",
    .assign_op = " = ", .assign_suffix = ";",
    .if_prefix = "if (", .if_suffix = ") {", .if_close = "}",
    .else_prefix = "} else {", .else_suffix = "",
    .loop_prefix = "for (let ", .loop_var = " = 0; ", .loop_mid = " < ", .loop_close = ") {",
    .print_prefix = "console.log(", .print_no_nl = ")", .print_with_nl = ")",
    .ret_prefix = "return ", .ret_suffix = ";",
    .op_add = " + ", .op_sub = " - ", .op_mul = " * ", .op_div = " / ",
    .op_gt = " > ", .op_lt = " < ", .op_eq = " === ",
    .op_and = " && ", .op_or = " || ",
    .line_end = ";", .indent_str = "    ", .comment = "// ",
    .block_open = "{", .block_close = "}",
    .header = "// Generated by Stone (meta-emitter)\n\n",
    .main_call = "main();",
};

static const TargetDesc TARGET_LUA = {
    .fn_prefix = "function ", .fn_suffix = ")", .fn_close = "end",
    .fn_call_open = "(", .fn_call_close = ")", .fn_call_sep = ", ",
    .var_prefix = "local ", .var_assign = " = ", .var_suffix = "",
    .assign_op = " = ", .assign_suffix = "",
    .if_prefix = "if ", .if_suffix = " then", .if_close = "end",
    .else_prefix = "else", .else_suffix = "",
    .loop_prefix = "for ", .loop_var = " = 0, ", .loop_mid = "-1 do", .loop_close = "end",
    .print_prefix = "io.write(", .print_no_nl = ")", .print_with_nl = ")",
    .ret_prefix = "return ", .ret_suffix = "",
    .op_add = " + ", .op_sub = " - ", .op_mul = " * ", .op_div = " / ",
    .op_gt = " > ", .op_lt = " < ", .op_eq = " == ",
    .op_and = " and ", .op_or = " or ",
    .line_end = "", .indent_str = "    ", .comment = "-- ",
    .block_open = "", .block_close = "",
    .header = "-- Generated by Stone (meta-emitter)\n\n",
    .main_call = "main()",
};

static const TargetDesc TARGET_RUST = {
    .fn_prefix = "fn ", .fn_suffix = ") {", .fn_close = "}",
        .fn_call_open = "(", .fn_call_close = ")", .fn_call_sep = ", ",
        .var_prefix = "let mut ", .var_assign = " = ", .var_suffix = ";",
        .assign_op = " = ", .assign_suffix = ";",
        .if_prefix = "if ", .if_suffix = " {", .if_close = "}",
        .else_prefix = "} else {", .else_suffix = "",
        .loop_prefix = "for ", .loop_var = " in 0..", .loop_mid = " {", .loop_close = "}",
        .print_prefix = "print!(\"{}\", ", .print_no_nl = ")", .print_with_nl = ");\n",
        .ret_prefix = "return ", .ret_suffix = ";",
        .op_add = " + ", .op_sub = " - ", .op_mul = " * ", .op_div = " / ",
        .op_gt = " > ", .op_lt = " < ", .op_eq = " == ",
        .op_and = " && ", .op_or = " || ",
        .line_end = ";", .indent_str = "    ", .comment = "// ",
        .block_open = "", .block_close = "",
        .header = "// Generated by Stone for Rust\n\n",
        .main_call = "fn main() {\n    hello();\n}",
};

/* ─── Emitter ─── */
typedef struct { char *buf; size_t cap, len; int indent; } Emitter;

static void emit_init(Emitter *e) {
    e->cap = 65536; e->buf = malloc(e->cap); e->len = 0; e->indent = 0; e->buf[0] = 0;
}
static void emit_str(Emitter *e, const char *s) {
    if (!s) return;
    size_t sl = strlen(s);
    if (e->len + sl + 1 >= e->cap) { e->cap *= 2; e->buf = realloc(e->buf, e->cap); }
    memcpy(e->buf + e->len, s, sl); e->len += sl; e->buf[e->len] = 0;
}
static void emit_indent(Emitter *e) {
    for (int i = 0; i < e->indent; i++) emit_str(e, "    ");
}
static void emit_line(Emitter *e, const char *s) {
    emit_indent(e); emit_str(e, s); emit_str(e, "\n");
}

/* ─── Meta-Parser: uses target descriptor for ALL output ─── */
typedef struct { Lexer *lex; Emitter *e; const TargetDesc *tgt; int errors; int ndef; } Parser;

static void parse_value(Parser *p);
static void parse_expr(Parser *p);
static void parse_stmt(Parser *p);

static void parse_value(Parser *p) {
    Lexer *l = p->lex; Emitter *e = p->e; const TargetDesc *t = p->tgt;
    Token tk, nt;
    lex_peek(l, &tk);
    switch (tk.type) {
        case TOK_NUMBER: lex_next(l, &tk); emit_str(e, tk.text); break;
        case TOK_STRING: lex_next(l, &tk); emit_str(e, tk.text); break;
        case TOK_IDENT: {
            lex_next(l, &tk);
            lex_peek(l, &nt);
            if (nt.type == TOK_IDENT || nt.type == TOK_NUMBER || nt.type == TOK_STRING) {
                emit_str(e, tk.text); emit_str(e, t->fn_call_open);
                int ac = 0;
                while (1) {
                    lex_peek(l, &nt);
                    if (nt.type == TOK_NEWLINE || nt.type == TOK_EOF || nt.type == TOK_END
                        || nt.type == TOK_PLUS || nt.type == TOK_MINUS || nt.type == TOK_MUL
                        || nt.type == TOK_DIV || nt.type == TOK_GT || nt.type == TOK_LT
                        || nt.type == TOK_EQ || nt.type == TOK_ASSIGN
                        || nt.type == TOK_AND || nt.type == TOK_OR) break;
                    if (ac++) emit_str(e, t->fn_call_sep);
                    lex_next(l, &nt); emit_str(e, nt.text);
                }
                emit_str(e, t->fn_call_close);
            } else {
                emit_str(e, tk.text);
            }
            break;
        }
        default: fprintf(stderr, "Error L%d: expected value\n", tk.line); p->errors++;
    }
}

static void parse_expr(Parser *p) {
    Lexer *l = p->lex; Emitter *e = p->e; const TargetDesc *t = p->tgt;
    Token tk;
    parse_value(p);
    while (1) {
        lex_peek(l, &tk);
        const char *op = NULL;
        switch (tk.type) {
            case TOK_PLUS:   op = t->op_add; break;
            case TOK_MINUS:  op = t->op_sub; break;
            case TOK_MUL:    op = t->op_mul; break;
            case TOK_DIV:    op = t->op_div; break;
            case TOK_GT:     op = t->op_gt; break;
            case TOK_LT:     op = t->op_lt; break;
            case TOK_EQ:     op = t->op_eq; break;
            case TOK_ASSIGN: op = t->op_eq; break;
            default: return;
        }
        lex_next(l, &tk); emit_str(e, op); parse_value(p);
    }
}

static void parse_cond(Parser *p) {
    Lexer *l = p->lex; Emitter *e = p->e; const TargetDesc *t = p->tgt;
    Token tk;
    parse_expr(p);
    lex_peek(l, &tk);
    while (tk.type == TOK_AND || tk.type == TOK_OR) {
        lex_next(l, &tk);
        emit_str(e, tk.type == TOK_AND ? t->op_and : t->op_or);
        parse_expr(p);
        lex_peek(l, &tk);
    }
}

static void parse_stmt(Parser *p) {
    Lexer *l = p->lex; Emitter *e = p->e; const TargetDesc *t = p->tgt;
    Token tk;
    lex_skip_newlines(l);
    lex_peek(l, &tk);
    switch (tk.type) {
        case TOK_EOF: case TOK_END: return;
        case TOK_VAR: {
            lex_next(l, &tk); Token name; lex_expect(l, TOK_IDENT, &name);
            emit_indent(e); emit_str(e, t->var_prefix); emit_str(e, name.text);
            lex_peek(l, &tk);
            if (tk.type == TOK_IDENT || tk.type == TOK_NUMBER || tk.type == TOK_STRING) {
                emit_str(e, t->var_assign); parse_expr(p);
            }
            emit_str(e, t->var_suffix); emit_str(e, "\n");
            break;
        }
        case TOK_IDENT: {
            Token name; lex_next(l, &name); lex_peek(l, &tk);
            if (tk.type == TOK_ASSIGN) {
                lex_next(l, &tk);
                emit_indent(e); emit_str(e, name.text); emit_str(e, t->assign_op); parse_expr(p);
                emit_str(e, t->assign_suffix); emit_str(e, "\n");
            } else {
                emit_indent(e); emit_str(e, name.text); emit_str(e, t->fn_call_open);
                int ac = 0;
                while (1) {
                    lex_peek(l, &tk);
                    if (tk.type == TOK_NEWLINE || tk.type == TOK_EOF || tk.type == TOK_END) break;
                    if (tk.type == TOK_IDENT || tk.type == TOK_NUMBER || tk.type == TOK_STRING) {
                        if (ac++) emit_str(e, t->fn_call_sep);
                        lex_next(l, &tk); emit_str(e, tk.text);
                    } else break;
                }
                emit_str(e, t->fn_call_close);
                if (strcmp(t->line_end, ";") == 0) emit_str(e, ";");
                emit_str(e, "\n");
            }
            break;
        }
        case TOK_PRINT: case TOK_PRINTN: {
            lex_next(l, &tk); int nl = (tk.type == TOK_PRINTN);
            lex_peek(l, &tk);
            emit_indent(e);
            if (tk.type == TOK_STRING) {
                emit_str(e, t->print_prefix);
                lex_next(l, &tk); emit_str(e, tk.text);
                emit_str(e, nl ? t->print_with_nl : t->print_no_nl);
                if (strcmp(t->line_end, ";") == 0) emit_str(e, ";");
                emit_str(e, "\n");
            } else {
                emit_str(e, t->print_prefix); parse_expr(p);
                emit_str(e, nl ? t->print_with_nl : t->print_no_nl);
                if (strcmp(t->line_end, ";") == 0) emit_str(e, ";");
                emit_str(e, "\n");
            }
            break;
        }
        case TOK_RET: {
            lex_next(l, &tk);
            emit_indent(e); emit_str(e, t->ret_prefix);
            lex_peek(l, &tk);
            if (tk.type == TOK_IDENT || tk.type == TOK_NUMBER || tk.type == TOK_STRING) parse_expr(p);
            emit_str(e, t->ret_suffix); emit_str(e, "\n");
            break;
        }
        case TOK_IF: {
            lex_next(l, &tk);
            emit_indent(e); emit_str(e, t->if_prefix); parse_cond(p); emit_str(e, t->if_suffix); emit_str(e, "\n");
            if (strlen(t->block_open) > 0) { emit_indent(e); emit_str(e, t->block_open); emit_str(e, "\n"); }
            e->indent++;
            while (1) { lex_skip_newlines(l); lex_peek(l, &tk);
                if (tk.type == TOK_END || tk.type == TOK_ELSE || tk.type == TOK_EOF) break; parse_stmt(p); }
            e->indent--;
            if (strlen(t->block_close) > 0 && strcmp(t->if_close, t->block_close) == 0) { emit_indent(e); emit_str(e, t->block_close); emit_str(e, "\n"); }
            lex_skip_newlines(l); lex_peek(l, &tk);
            if (tk.type == TOK_ELSE) {
                lex_next(l, &tk); emit_indent(e); emit_str(e, t->else_prefix); emit_str(e, t->else_suffix); emit_str(e, "\n");
                if (strlen(t->block_open) > 0) { emit_indent(e); emit_str(e, t->block_open); emit_str(e, "\n"); }
                e->indent++;
                while (1) { lex_skip_newlines(l); lex_peek(l, &tk);
                    if (tk.type == TOK_END || tk.type == TOK_EOF) break; parse_stmt(p); }
                e->indent--;
                if (strlen(t->block_close) > 0 && strcmp(t->if_close, t->block_close) == 0) { emit_indent(e); emit_str(e, t->block_close); emit_str(e, "\n"); }
            }
            lex_skip_newlines(l); lex_expect(l, TOK_END, &tk);
            if (strlen(t->if_close) > 0 && strcmp(t->if_close, t->block_close) != 0) { emit_indent(e); emit_str(e, t->if_close); emit_str(e, "\n"); }
            break;
        }
        case TOK_LOOP: {
            lex_next(l, &tk); Token v; lex_expect(l, TOK_IDENT, &v);
            emit_indent(e); emit_str(e, t->loop_prefix); emit_str(e, v.text);
            if (strcmp(t->loop_var, " = 0, ") == 0) {
                /* Lua-style: for i = 0, N-1 do */
                emit_str(e, " = 0, "); parse_expr(p); emit_str(e, "-1 do\n");
            } else if (strcmp(t->loop_var, " in range(") == 0) {
                /* Python-style: for i in range(N): */
                emit_str(e, " in range("); parse_expr(p); emit_str(e, "):\n");
            } else {
                /* JavaScript-style: for (let i = 0; i < N; i++) */
                emit_str(e, " = 0; "); emit_str(e, v.text); emit_str(e, " < "); parse_expr(p); emit_str(e, "; ");
                emit_str(e, v.text); emit_str(e, "++) {\n");
            }
            e->indent++;
            while (1) { lex_skip_newlines(l); lex_peek(l, &tk);
                if (tk.type == TOK_END || tk.type == TOK_EOF) break; parse_stmt(p); }
            e->indent--;
            lex_skip_newlines(l); lex_expect(l, TOK_END, &tk);
            if (strlen(t->loop_close) > 0) { emit_indent(e); emit_str(e, t->loop_close); emit_str(e, "\n"); }
            break;
        }
        default:
            fprintf(stderr, "Error L%d: unexpected '%s'\n", tk.line, tk.text);
            p->errors++;
            while (tk.type != TOK_NEWLINE && tk.type != TOK_EOF) lex_next(l, &tk);
    }
}

static void parse_fn(Parser *p) {
    Lexer *l = p->lex; Emitter *e = p->e; const TargetDesc *t = p->tgt;
    Token tk; lex_expect(l, TOK_FN, &tk);
    Token name; lex_expect(l, TOK_IDENT, &name);
    if (p->ndef++ == 0) emit_str(e, "\n");
    emit_str(e, t->fn_prefix); emit_str(e, name.text);
    /* Parameters */
    emit_str(e, "(");
    int pc = 0;
    while (1) {
        lex_peek(l, &tk);
        if (tk.type == TOK_NEWLINE || tk.type == TOK_EOF) break;
        if (tk.type == TOK_IDENT) { if (pc++) emit_str(e, ", "); lex_next(l, &tk); emit_str(e, tk.text); }
        else break;
    }
    emit_str(e, t->fn_suffix); emit_str(e, "\n");
    if (strlen(t->block_open) > 0) { emit_indent(e); emit_str(e, t->block_open); emit_str(e, "\n"); }
    e->indent++;
    lex_skip_newlines(l);
    while (1) { lex_peek(l, &tk); if (tk.type == TOK_END || tk.type == TOK_EOF) break; parse_stmt(p); }
    e->indent--;
    lex_expect(l, TOK_END, &tk);
    if (strlen(t->block_close) > 0) { emit_indent(e); emit_str(e, t->block_close); emit_str(e, "\n"); }
    if (strlen(t->fn_close) > 0 && strcmp(t->fn_close, t->block_close) != 0) { emit_line(e, t->fn_close); }
    emit_str(e, "\n");
}

/* ─── Main ─── */
int main(int argc, char **argv) {
    const char *input_path = NULL;
    const char *target_name = "python";

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--target") == 0 && i + 1 < argc) target_name = argv[++i];
        else if (!input_path) input_path = argv[i];
    }
        if (!input_path) { fprintf(stderr, "Usage: %s <input.st> --target <target>\nTargets: python, javascript, lua, rust\n", argv[0]); return 1; }

    const TargetDesc *tgt = NULL;
    if (strcmp(target_name, "python") == 0) tgt = &TARGET_PYTHON;
    else if (strcmp(target_name, "javascript") == 0) tgt = &TARGET_JAVASCRIPT;
    else if (strcmp(target_name, "lua") == 0) tgt = &TARGET_LUA;
    else if (strcmp(target_name, "rust") == 0) tgt = &TARGET_RUST;
    else { fprintf(stderr, "Unknown target: %s\n", target_name); return 1; }

    char *src = NULL; size_t src_len = 0;
    FILE *f = fopen(input_path, "rb"); if (!f) { perror("fopen"); return 1; }
    fseek(f, 0, SEEK_END); src_len = ftell(f); fseek(f, 0, SEEK_SET);
    src = malloc(src_len + 1); fread(src, 1, src_len, f); fclose(f);
    src[src_len] = 0;

    Lexer l; Lexer *lex = &l;
    Emitter e; emit_init(&e);
    Parser p = {lex, &e, tgt, 0, 0};
    lex_init(lex, src);

    emit_line(&e, tgt->header);
    Token tk;
    while (1) {
        lex_skip_newlines(lex); lex_peek(lex, &tk);
        if (tk.type == TOK_EOF) break;
        if (tk.type == TOK_FN) parse_fn(&p);
        else { fprintf(stderr, "Error: expected 'fn', got '%s'\n", tk.text); p.errors++; lex_next(lex, &tk); }
    }

    if (strstr(e.buf, "main(")) {
        emit_str(&e, "\n");
        /* Parse main_call for multi-line patterns */
        const char *mc = tgt->main_call;
        if (strstr(mc, "\\n")) {
            /* Python-style multi-line */
            while (*mc) {
                const char *nl = strstr(mc, "\\n");
                if (nl) {
                    char buf[256]; size_t n = nl - mc;
                    memcpy(buf, mc, n); buf[n] = 0;
                    emit_line(&e, buf);
                    mc = nl + 2;
                } else { emit_line(&e, mc); break; }
            }
        } else {
            emit_line(&e, mc);
        }
    }

    if (p.errors == 0) { printf("%s", e.buf); }
    else { fprintf(stderr, "%d errors\n", p.errors); }
    free(e.buf); free(src);
    return p.errors ? 1 : 0;
}
