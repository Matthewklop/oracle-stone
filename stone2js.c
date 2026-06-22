/* ============================================================================
 * stone2js.c — Stone → JavaScript Transpiler
 *
 * Converts Stone source to JavaScript. No compile step needed.
 * Usage: ./stone2js < input.st > output.js
 *        ./stone2js file.st > file.js
 *
 * Properties:
 *   - Same Stone grammar (stack-based, comma-free, paren-free)
 *   - Outputs valid JavaScript (Node.js)
 *   - print → process.stdout.write(), printn → console.log()
 *   - loop → for-loop, if/else → if/else with braces
 *   - fn → function, var → let
 * ============================================================================
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

/* ─── Reuse Stone's tokenizer ─── */
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

typedef struct {
    TokenType type;
    char text[64];
    int line;
    int col;
} Token;

typedef struct {
    const char *src;
    const char *pos;
    int line;
    int col;
    Token curr;
    int has_curr;
} Lexer;

static void lex_init(Lexer *l, const char *src) {
    l->src = src; l->pos = src; l->line = 1; l->col = 1; l->has_curr = 0;
}

static int lex_next(Lexer *l, Token *t) {
    if (l->has_curr) {
        *t = l->curr;
        l->has_curr = 0;
        return 1;
    }
    const char *p = l->pos;
    while (*p == ' ' || *p == '\t' || *p == '\r') { p++; l->col++; }
    t->line = l->line;
    t->col = l->col;
    t->text[0] = 0;
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
        t->text[i] = 0; t->type = TOK_STRING;
        l->pos = p; return 1;
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
        t->text[i] = 0; t->type = TOK_NUMBER;
        l->pos = p; return 1;
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
    t->text[0] = *p; t->text[1] = 0;
    l->pos = p + 1; return 1;
}

static int lex_peek(Lexer *l, Token *t) {
    if (!l->has_curr) { lex_next(l, &l->curr); l->has_curr = 1; }
    *t = l->curr; return 1;
}

static int lex_expect(Lexer *l, TokenType type, Token *t) {
    lex_next(l, t);
    if (t->type != type) {
        fprintf(stderr, "Error at line %d: expected %s, got %s\n",
                t->line, token_names[type], token_names[t->type]);
        return 0;
    }
    return 1;
}

static void lex_skip_newlines(Lexer *l) {
    Token t;
    while (1) { lex_peek(l, &t); if (t.type == TOK_NEWLINE) lex_next(l, &t); else break; }
}

/* ─── JavaScript Emitter ─── */
typedef struct { char *buf; size_t cap, len; int indent; } Emitter;

static void emit_init(Emitter *e) {
    e->cap = 65536; e->buf = malloc(e->cap); e->len = 0; e->indent = 0; e->buf[0] = 0;
}
static void emit_str(Emitter *e, const char *s) {
    size_t sl = strlen(s);
    if (e->len + sl + 1 >= e->cap) { e->cap *= 2; e->buf = realloc(e->buf, e->cap); }
    memcpy(e->buf + e->len, s, sl); e->len += sl; e->buf[e->len] = 0;
}
static void emit_indent_only(Emitter *e) {
    for (int i = 0; i < e->indent; i++) emit_str(e, "    ");
}
static void emit_line(Emitter *e, const char *s) {
    emit_indent_only(e);
    emit_str(e, s); emit_str(e, "\n");
}

/* ─── Parser → JavaScript ─── */
typedef struct { Lexer *lex; Emitter *e; int errors; int ndef; } Parser;

static void parse_expr(Parser *p, int as_string);
static void parse_stmt(Parser *p);

/* expr → JavaScript expression string */
static void parse_expr(Parser *p, int as_string) {
    Lexer *l = p->lex; Emitter *e = p->e; Token t;
    lex_peek(l, &t);
    if (t.type == TOK_IDENT) { lex_next(l, &t); emit_str(e, t.text); }
    else if (t.type == TOK_NUMBER) { lex_next(l, &t); emit_str(e, t.text); }
    else if (t.type == TOK_STRING) {
        lex_next(l, &t);
        if (as_string) { /* already a string literal, use raw */
            emit_str(e, t.text);
        } else { /* strip quotes, treat as value */
            emit_str(e, t.text);
        }
    } else { fprintf(stderr, "Error L%d: expected value\n", t.line); p->errors++; return; }
    while (1) {
        lex_peek(l, &t);
        const char *op = NULL;
        switch (t.type) {
            case TOK_PLUS:  op = " + "; break;
            case TOK_MINUS: op = " - "; break;
            case TOK_MUL:   op = " * "; break;
            case TOK_DIV:   op = " / "; break;
            case TOK_GT:    op = " > "; break;
            case TOK_LT:    op = " < "; break;
            case TOK_EQ:    op = " == "; break;
            case TOK_ASSIGN: op = " == "; break;
            default: return;
        }
        lex_next(l, &t); emit_str(e, op);
        lex_peek(l, &t);
        if (t.type == TOK_IDENT || t.type == TOK_NUMBER || t.type == TOK_STRING) {
            lex_next(l, &t); emit_str(e, t.text);
        } else { fprintf(stderr, "Error L%d: expected value after op\n", t.line); p->errors++; return; }
    }
}

static void parse_cond(Parser *p) {
    Lexer *l = p->lex; Emitter *e = p->e; Token t;
    parse_expr(p, 0);
    lex_peek(l, &t);
    while (t.type == TOK_AND || t.type == TOK_OR) {
        lex_next(l, &t);
        emit_str(e, t.type == TOK_AND ? " && " : " || ");
        parse_expr(p, 0);
        lex_peek(l, &t);
    }
}

static void parse_stmt(Parser *p) {
    Lexer *l = p->lex; Emitter *e = p->e; Token t;
    lex_skip_newlines(l);
    lex_peek(l, &t);

    switch (t.type) {
        case TOK_EOF: case TOK_END: return;

        case TOK_VAR: {
            lex_next(l, &t); Token name;
            lex_expect(l, TOK_IDENT, &name);
            emit_indent_only(e); emit_str(e, "let "); emit_str(e, name.text);
            lex_peek(l, &t);
            if (t.type == TOK_IDENT || t.type == TOK_NUMBER || t.type == TOK_STRING) {
                emit_str(e, " = "); parse_expr(p, 0);
            }
            emit_str(e, ";\n");
            break;
        }

        case TOK_IDENT: {
            Token name; lex_next(l, &name); lex_peek(l, &t);
            if (t.type == TOK_ASSIGN) {
                lex_next(l, &t);
                emit_indent_only(e); emit_str(e, name.text); emit_str(e, " = "); parse_expr(p, 0); emit_str(e, ";\n");
            } else {
                emit_indent_only(e); emit_str(e, name.text); emit_str(e, "(");
                int ac = 0;
                while (1) {
                    lex_peek(l, &t);
                    if (t.type == TOK_NEWLINE || t.type == TOK_EOF || t.type == TOK_END) break;
                    if (t.type == TOK_IDENT || t.type == TOK_NUMBER || t.type == TOK_STRING) {
                        if (ac++) emit_str(e, ", "); parse_expr(p, 0);
                    } else break;
                }
                emit_str(e, ");\n");
            }
            break;
        }

        case TOK_PRINT: case TOK_PRINTN: {
            lex_next(l, &t);
            int nl = (t.type == TOK_PRINTN);
            lex_peek(l, &t);
            emit_indent_only(e);
            if (nl) {
                emit_str(e, "console.log("); parse_expr(p, 0); emit_str(e, ");\n");
            } else {
                emit_str(e, "process.stdout.write("); parse_expr(p, 0); emit_str(e, ");\n");
            }
            break;
        }

        case TOK_RET: {
            lex_next(l, &t);
            emit_indent_only(e); emit_str(e, "return");
            lex_peek(l, &t);
            if (t.type == TOK_IDENT || t.type == TOK_NUMBER || t.type == TOK_STRING) {
                emit_str(e, " "); parse_expr(p, 0);
            }
            emit_str(e, ";\n"); break;
        }

        case TOK_IF: {
            lex_next(l, &t);
            emit_indent_only(e); emit_str(e, "if ("); parse_cond(p); emit_str(e, ") {\n");
            e->indent++;
            while (1) { lex_skip_newlines(l); lex_peek(l, &t);
                if (t.type == TOK_END || t.type == TOK_ELSE || t.type == TOK_EOF) break; parse_stmt(p); }
            e->indent--;
            lex_skip_newlines(l); lex_peek(l, &t);
            if (t.type == TOK_ELSE) {
                lex_next(l, &t); emit_line(e, "} else {");
                e->indent++;
                while (1) { lex_skip_newlines(l); lex_peek(l, &t);
                    if (t.type == TOK_END || t.type == TOK_EOF) break; parse_stmt(p); }
                e->indent--;
            }
            emit_line(e, "}");
            lex_skip_newlines(l); lex_expect(l, TOK_END, &t); break;
        }

        case TOK_LOOP: {
            lex_next(l, &t); Token v; lex_expect(l, TOK_IDENT, &v);
            emit_indent_only(e); emit_str(e, "for (let "); emit_str(e, v.text); emit_str(e, " = 0; ");
            emit_str(e, v.text); emit_str(e, " < ");
            parse_expr(p, 0);
            emit_str(e, "; "); emit_str(e, v.text); emit_str(e, "++) {\n");
            e->indent++;
            while (1) { lex_skip_newlines(l); lex_peek(l, &t);
                if (t.type == TOK_END || t.type == TOK_EOF) break; parse_stmt(p); }
            e->indent--;
            emit_line(e, "}");
            lex_skip_newlines(l); lex_expect(l, TOK_END, &t); break;
        }

        default:
            fprintf(stderr, "Error L%d: unexpected '%s'\n", t.line, t.text);
            p->errors++;
            while (t.type != TOK_NEWLINE && t.type != TOK_EOF) lex_next(l, &t);
    }
}

static void parse_fn(Parser *p) {
    Lexer *l = p->lex; Emitter *e = p->e; Token t;
    lex_expect(l, TOK_FN, &t);
    Token name; lex_expect(l, TOK_IDENT, &name);
    if (p->ndef++ == 0) emit_str(e, "\n");
    emit_str(e, "function "); emit_str(e, name.text); emit_str(e, "(");
    int pc = 0;
    while (1) {
        lex_peek(l, &t);
        if (t.type == TOK_NEWLINE || t.type == TOK_EOF) break;
        if (t.type == TOK_IDENT) {
            if (pc++) emit_str(e, ", "); lex_next(l, &t); emit_str(e, t.text);
        } else break;
    }
    emit_str(e, ") {\n");
    e->indent++;
    lex_skip_newlines(l);
    while (1) { lex_peek(l, &t); if (t.type == TOK_END || t.type == TOK_EOF) break; parse_stmt(p); }
    e->indent--;
    emit_line(e, "}");
    lex_expect(l, TOK_END, &t);
    emit_str(e, "\n");
}

/* ─── Main ─── */
int main(int argc, char **argv) {
    const char *input_path = NULL;
    if (argc > 1) input_path = argv[1];

    char *src = NULL; size_t src_len = 0;
    if (input_path) {
        FILE *f = fopen(input_path, "rb"); if (!f) { perror("fopen"); return 1; }
        fseek(f, 0, SEEK_END); src_len = ftell(f); fseek(f, 0, SEEK_SET);
        src = malloc(src_len + 1); fread(src, 1, src_len, f); fclose(f);
        src[src_len] = 0;
    } else {
        char buf[4096]; size_t cap = 4096, total = 0; src = malloc(cap);
        while (fgets(buf, sizeof(buf), stdin)) {
            size_t n = strlen(buf);
            if (total + n >= cap) { cap *= 2; src = realloc(src, cap); }
            memcpy(src + total, buf, n); total += n;
        }
        src[total] = 0; src_len = total;
    }
    if (!src || src_len == 0) { fprintf(stderr, "Empty\n"); return 1; }

    Lexer l; Lexer *lex = &l;
    Emitter e; emit_init(&e);
    Parser p = {lex, &e, 0, 0};
    lex_init(lex, src);

    /* Header */
    emit_line(&e, "// Generated by stone2js \u2014 Stone \u2192 JavaScript");
    emit_line(&e, "");

    Token t;
    while (1) {
        lex_skip_newlines(lex); lex_peek(lex, &t);
        if (t.type == TOK_EOF) break;
        if (t.type == TOK_FN) parse_fn(&p);
        else { fprintf(stderr, "Error: expected 'fn' at top level, got '%s'\n", t.text); p.errors++; lex_next(lex, &t); }
    }

    /* If a main function exists, call it */
    if (strstr(e.buf, "function main(")) {
        emit_line(&e, "");
        emit_line(&e, "main();");
    }

    if (p.errors == 0) {
        printf("%s", e.buf);
    } else {
        fprintf(stderr, "%d errors\n", p.errors);
    }
    free(e.buf); free(src);
    return p.errors ? 1 : 0;
}
