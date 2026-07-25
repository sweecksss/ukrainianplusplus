#include "upp_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static UppToken* parser_peek(Parser* p) {
    return &p->tokens.tokens[p->pos];
}

static UppToken* parser_previous(Parser* p) {
    return &p->tokens.tokens[p->pos - 1];
}

static int parser_is_at_end(Parser* p) {
    return parser_peek(p)->type == TOKEN_EOF;
}

static int parser_check(Parser* p, UppTokenType type) {
    if (parser_is_at_end(p)) return type == TOKEN_EOF;
    return parser_peek(p)->type == type;
}

static UppToken* parser_advance(Parser* p) {
    if (!parser_is_at_end(p)) p->pos++;
    return parser_previous(p);
}

static int parser_match(Parser* p, UppTokenType type) {
    if (parser_check(p, type)) {
        parser_advance(p);
        return 1;
    }
    return 0;
}

static UppToken* parser_consume(Parser* p, UppTokenType type, const char* msg) {
    if (parser_check(p, type)) return parser_advance(p);
    UppToken* tok = parser_peek(p);
    fprintf(stderr, "Синтаксична помилка (рядок %d): %s. Знайдено '%s'\n", tok->line, msg, tok->lexeme);
    return tok;
}

// Forward declarations
static Expr* parse_expression(Parser* p);
static Stmt* parse_statement(Parser* p);
static StmtList parse_suite(Parser* p);

static Expr* parse_factor(Parser* p) {
    if (parser_match(p, TOKEN_NOT)) {
        char* op = _strdup(parser_previous(p)->lexeme);
        Expr* expr = parse_factor(p);
        Expr* res = upp_make_unary_expr(op, expr);
        free(op);
        return res;
    }

    UppToken* tok = parser_peek(p);
    if (tok->type == TOKEN_NUMBER) {
        parser_advance(p);
        return upp_make_number_expr(tok->number_value);
    }
    if (tok->type == TOKEN_STRING) {
        parser_advance(p);
        return upp_make_string_expr(tok->lexeme);
    }
    if (tok->type == TOKEN_TRUE) {
        parser_advance(p);
        return upp_make_bool_expr(1);
    }
    if (tok->type == TOKEN_FALSE) {
        parser_advance(p);
        return upp_make_bool_expr(0);
    }
    if (tok->type == TOKEN_IDENT) {
        parser_advance(p);
        return upp_make_var_expr(tok->lexeme);
    }

    fprintf(stderr, "Синтаксична помилка (рядок %d): Очікувався вираз, натомість '%s'\n", tok->line, tok->lexeme);
    return upp_make_number_expr(0);
}

static Expr* parse_term(Parser* p) {
    Expr* expr = parse_factor(p);
    while (parser_check(p, TOKEN_POMNOZHYTY) || parser_check(p, TOKEN_PODILYTY)) {
        parser_advance(p);
        char* op = _strdup(parser_previous(p)->lexeme);
        Expr* right = parse_factor(p);
        expr = upp_make_binary_expr(expr, op, right);
        free(op);
    }
    return expr;
}

static Expr* parse_additive(Parser* p) {
    Expr* expr = parse_term(p);
    while (parser_check(p, TOKEN_DODATY) || parser_check(p, TOKEN_VIDNIATY)) {
        parser_advance(p);
        char* op = _strdup(parser_previous(p)->lexeme);
        Expr* right = parse_term(p);
        expr = upp_make_binary_expr(expr, op, right);
        free(op);
    }
    return expr;
}

static Expr* parse_comparison(Parser* p) {
    Expr* expr = parse_additive(p);
    while (parser_check(p, TOKEN_BILSHE) || parser_check(p, TOKEN_MENSHE) || parser_check(p, TOKEN_DORIVNYUE)) {
        parser_advance(p);
        char* op = _strdup(parser_previous(p)->lexeme);
        Expr* right = parse_additive(p);
        expr = upp_make_binary_expr(expr, op, right);
        free(op);
    }
    return expr;
}

static Expr* parse_and(Parser* p) {
    Expr* expr = parse_comparison(p);
    while (parser_match(p, TOKEN_AND)) {
        char* op = _strdup(parser_previous(p)->lexeme);
        Expr* right = parse_comparison(p);
        expr = upp_make_binary_expr(expr, op, right);
        free(op);
    }
    return expr;
}

static Expr* parse_or(Parser* p) {
    Expr* expr = parse_and(p);
    while (parser_match(p, TOKEN_OR)) {
        char* op = _strdup(parser_previous(p)->lexeme);
        Expr* right = parse_and(p);
        expr = upp_make_binary_expr(expr, op, right);
        free(op);
    }
    return expr;
}

static Expr* parse_expression(Parser* p) {
    return parse_or(p);
}

static Stmt* parse_var_decl(Parser* p) {
    parser_consume(p, TOKEN_NEHAI, "Очікувалось 'нехай'");
    UppToken* name_tok = parser_consume(p, TOKEN_IDENT, "Очікувалась назва змінної");
    char* name = _strdup(name_tok->lexeme);
    parser_consume(p, TOKEN_BUDE, "Очікувалось слово 'буде'");
    Expr* expr = parse_expression(p);
    parser_match(p, TOKEN_NEWLINE);
    Stmt* res = upp_make_var_decl_stmt(name, expr);
    free(name);
    return res;
}

static Stmt* parse_print(Parser* p) {
    parser_consume(p, TOKEN_POKAZATY, "Очікувалось 'показати'");
    Expr* expr = parse_expression(p);
    parser_match(p, TOKEN_NEWLINE);
    return upp_make_print_stmt(expr);
}

static StmtList parse_suite(Parser* p) {
    while (parser_match(p, TOKEN_NEWLINE)) {}

    StmtList list;
    upp_stmt_list_init(&list);

    if (parser_match(p, TOKEN_INDENT)) {
        while (!parser_check(p, TOKEN_DEDENT) && !parser_check(p, TOKEN_EOF)) {
            if (parser_match(p, TOKEN_NEWLINE)) continue;
            upp_stmt_list_append(&list, parse_statement(p));
        }
        parser_consume(p, TOKEN_DEDENT, "Очікувався кінець блоку (DEDENT)");
    } else {
        upp_stmt_list_append(&list, parse_statement(p));
    }

    return list;
}

static Stmt* parse_if(Parser* p) {
    parser_consume(p, TOKEN_IF, "Очікувалось 'якщо'");
    Expr* cond = parse_expression(p);

    parser_match(p, TOKEN_TO);
    StmtList then_b = parse_suite(p);

    while (parser_match(p, TOKEN_NEWLINE)) {}

    StmtList else_b;
    upp_stmt_list_init(&else_b);
    int has_else = 0;

    if (parser_match(p, TOKEN_ELSE)) {
        has_else = 1;
        parser_match(p, TOKEN_TO);
        else_b = parse_suite(p);
    }

    return upp_make_if_stmt(cond, then_b, else_b, has_else);
}

static Stmt* parse_while(Parser* p) {
    parser_consume(p, TOKEN_WHILE, "Очікувалось 'поки'");
    Expr* cond = parse_expression(p);

    parser_match(p, TOKEN_TO);
    StmtList body = parse_suite(p);

    return upp_make_while_stmt(cond, body);
}

static Stmt* parse_statement(Parser* p) {
    if (parser_check(p, TOKEN_NEHAI)) return parse_var_decl(p);
    if (parser_check(p, TOKEN_POKAZATY)) return parse_print(p);
    if (parser_check(p, TOKEN_IF)) return parse_if(p);
    if (parser_check(p, TOKEN_WHILE)) return parse_while(p);

    UppToken* tok = parser_peek(p);
    if (tok->type != TOKEN_INDENT && tok->type != TOKEN_DEDENT && tok->type != TOKEN_NEWLINE) {
        fprintf(stderr, "Синтаксична помилка (рядок %d): Неочікуваний початок інструкції '%s'\n", tok->line, tok->lexeme);
    }
    parser_advance(p);
    return upp_make_print_stmt(upp_make_number_expr(0));
}

Program upp_parse(UppTokenArray tokens) {
    Parser p;
    p.tokens = tokens;
    p.pos = 0;

    Program prog;
    upp_stmt_list_init(&prog.statements);

    while (!parser_check(&p, TOKEN_EOF)) {
        if (parser_match(&p, TOKEN_NEWLINE)) continue;
        upp_stmt_list_append(&prog.statements, parse_statement(&p));
    }

    return prog;
}
