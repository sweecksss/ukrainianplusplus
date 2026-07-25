#include "upp_ast.h"
#include <stdlib.h>
#include <string.h>

Expr* upp_make_number_expr(long long val) {
    Expr* e = (Expr*)malloc(sizeof(Expr));
    e->type = EXPR_NUMBER;
    e->as.number_val = val;
    return e;
}

Expr* upp_make_string_expr(const char* val) {
    Expr* e = (Expr*)malloc(sizeof(Expr));
    e->type = EXPR_STRING;
    e->as.string_val = _strdup(val);
    return e;
}

Expr* upp_make_bool_expr(int val) {
    Expr* e = (Expr*)malloc(sizeof(Expr));
    e->type = EXPR_BOOL;
    e->as.bool_val = val;
    return e;
}

Expr* upp_make_var_expr(const char* name) {
    Expr* e = (Expr*)malloc(sizeof(Expr));
    e->type = EXPR_VARIABLE;
    e->as.var_name = _strdup(name);
    return e;
}

Expr* upp_make_binary_expr(Expr* left, const char* op, Expr* right) {
    Expr* e = (Expr*)malloc(sizeof(Expr));
    e->type = EXPR_BINARY;
    e->as.binary.left = left;
    e->as.binary.op = _strdup(op);
    e->as.binary.right = right;
    return e;
}

Expr* upp_make_unary_expr(const char* op, Expr* expr) {
    Expr* e = (Expr*)malloc(sizeof(Expr));
    e->type = EXPR_UNARY;
    e->as.unary.op = _strdup(op);
    e->as.unary.expr = expr;
    return e;
}

Stmt* upp_make_var_decl_stmt(const char* name, Expr* expr) {
    Stmt* s = (Stmt*)malloc(sizeof(Stmt));
    s->type = STMT_VAR_DECL;
    s->as.var_decl.name = _strdup(name);
    s->as.var_decl.expr = expr;
    return s;
}

Stmt* upp_make_print_stmt(Expr* expr) {
    Stmt* s = (Stmt*)malloc(sizeof(Stmt));
    s->type = STMT_PRINT;
    s->as.print.expr = expr;
    return s;
}

Stmt* upp_make_if_stmt(Expr* cond, StmtList then_b, StmtList else_b, int has_else) {
    Stmt* s = (Stmt*)malloc(sizeof(Stmt));
    s->type = STMT_IF;
    s->as.if_stmt.condition = cond;
    s->as.if_stmt.then_branch = then_b;
    s->as.if_stmt.else_branch = else_b;
    s->as.if_stmt.has_else = has_else;
    return s;
}

Stmt* upp_make_while_stmt(Expr* cond, StmtList body) {
    Stmt* s = (Stmt*)malloc(sizeof(Stmt));
    s->type = STMT_WHILE;
    s->as.while_stmt.condition = cond;
    s->as.while_stmt.body = body;
    return s;
}

void upp_stmt_list_init(StmtList* list) {
    list->statements = NULL;
    list->count = 0;
    list->capacity = 0;
}

void upp_stmt_list_append(StmtList* list, Stmt* stmt) {
    if (list->count + 1 > list->capacity) {
        list->capacity = list->capacity < 8 ? 8 : list->capacity * 2;
        list->statements = (Stmt**)realloc(list->statements, sizeof(Stmt*) * list->capacity);
    }
    list->statements[list->count++] = stmt;
}

void upp_free_expr(Expr* expr) {
    if (!expr) return;
    switch (expr->type) {
        case EXPR_STRING:
            free(expr->as.string_val);
            break;
        case EXPR_VARIABLE:
            free(expr->as.var_name);
            break;
        case EXPR_BINARY:
            upp_free_expr(expr->as.binary.left);
            free(expr->as.binary.op);
            upp_free_expr(expr->as.binary.right);
            break;
        case EXPR_UNARY:
            free(expr->as.unary.op);
            upp_free_expr(expr->as.unary.expr);
            break;
        default:
            break;
    }
    free(expr);
}

void upp_free_stmt(Stmt* stmt) {
    if (!stmt) return;
    switch (stmt->type) {
        case STMT_VAR_DECL:
            free(stmt->as.var_decl.name);
            upp_free_expr(stmt->as.var_decl.expr);
            break;
        case STMT_PRINT:
            upp_free_expr(stmt->as.print.expr);
            break;
        case STMT_BLOCK:
            for (int i = 0; i < stmt->as.block.count; i++) {
                upp_free_stmt(stmt->as.block.statements[i]);
            }
            free(stmt->as.block.statements);
            break;
        case STMT_IF:
            upp_free_expr(stmt->as.if_stmt.condition);
            for (int i = 0; i < stmt->as.if_stmt.then_branch.count; i++) {
                upp_free_stmt(stmt->as.if_stmt.then_branch.statements[i]);
            }
            free(stmt->as.if_stmt.then_branch.statements);
            if (stmt->as.if_stmt.has_else) {
                for (int i = 0; i < stmt->as.if_stmt.else_branch.count; i++) {
                    upp_free_stmt(stmt->as.if_stmt.else_branch.statements[i]);
                }
                free(stmt->as.if_stmt.else_branch.statements);
            }
            break;
        case STMT_WHILE:
            upp_free_expr(stmt->as.while_stmt.condition);
            for (int i = 0; i < stmt->as.while_stmt.body.count; i++) {
                upp_free_stmt(stmt->as.while_stmt.body.statements[i]);
            }
            free(stmt->as.while_stmt.body.statements);
            break;
    }
    free(stmt);
}

void upp_free_program(Program* prog) {
    for (int i = 0; i < prog->statements.count; i++) {
        upp_free_stmt(prog->statements.statements[i]);
    }
    free(prog->statements.statements);
}
