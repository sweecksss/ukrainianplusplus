#include "upp_ast.h"
#include "upp_common.h"

#include <stdlib.h>
#include <string.h>

const char* upp_bin_op_name(BinOp op) {
    switch (op) {
        case BIN_ADD: return "додати";
        case BIN_SUB: return "відняти";
        case BIN_MUL: return "помножити";
        case BIN_DIV: return "поділити";
        case BIN_GT:  return "більше";
        case BIN_LT:  return "менше";
        case BIN_EQ:  return "дорівнює";
        case BIN_NE:  return "не дорівнює";
        case BIN_GE:  return "не менше";
        case BIN_LE:  return "не більше";
        case BIN_AND: return "і";
        case BIN_OR:  return "або";
    }
    return "?";
}

const char* upp_un_op_name(UnOp op) {
    switch (op) {
        case UN_NOT: return "не";
        case UN_NEG: return "-";
    }
    return "?";
}

/* ------------------------------------------------------------------ */
/* Вирази                                                              */
/* ------------------------------------------------------------------ */

static Expr* alloc_expr(ExprType type, int line) {
    Expr* e = (Expr*)upp_xmalloc(sizeof(Expr));
    memset(e, 0, sizeof(Expr));
    e->type = type;
    e->line = line;
    return e;
}

Expr* upp_make_number_expr(long long val, int line) {
    Expr* e = alloc_expr(EXPR_NUMBER, line);
    e->as.number_val = val;
    return e;
}

Expr* upp_make_real_expr(double val, int line) {
    Expr* e = alloc_expr(EXPR_REAL, line);
    e->as.real_val = val;
    return e;
}

Expr* upp_make_string_expr(const char* val, int line) {
    Expr* e = alloc_expr(EXPR_STRING, line);
    e->as.string_val = upp_xstrdup(val);
    return e;
}

Expr* upp_make_bool_expr(int val, int line) {
    Expr* e = alloc_expr(EXPR_BOOL, line);
    e->as.bool_val = val;
    return e;
}

Expr* upp_make_var_expr(const char* name, int line) {
    Expr* e = alloc_expr(EXPR_VARIABLE, line);
    e->as.var_name = upp_xstrdup(name);
    return e;
}

Expr* upp_make_binary_expr(Expr* left, BinOp op, Expr* right, int line) {
    Expr* e = alloc_expr(EXPR_BINARY, line);
    e->as.binary.left = left;
    e->as.binary.op = op;
    e->as.binary.right = right;
    return e;
}

Expr* upp_make_unary_expr(UnOp op, Expr* expr, int line) {
    Expr* e = alloc_expr(EXPR_UNARY, line);
    e->as.unary.op = op;
    e->as.unary.expr = expr;
    return e;
}

Expr* upp_make_call_expr(const char* name, Expr** args, int arg_count, int line) {
    Expr* e = alloc_expr(EXPR_CALL, line);
    e->as.call.name = upp_xstrdup(name);
    e->as.call.args = args;
    e->as.call.arg_count = arg_count;
    return e;
}

Expr* upp_make_list_expr(Expr** items, int count, int line) {
    Expr* e = alloc_expr(EXPR_LIST, line);
    e->as.list.items = items;
    e->as.list.count = count;
    return e;
}

Expr* upp_make_index_expr(Expr* target, Expr* index, int line) {
    Expr* e = alloc_expr(EXPR_INDEX, line);
    e->as.index.target = target;
    e->as.index.index = index;
    return e;
}

/* ------------------------------------------------------------------ */
/* Інструкції                                                          */
/* ------------------------------------------------------------------ */

static Stmt* alloc_stmt(StmtType type, int line) {
    Stmt* s = (Stmt*)upp_xmalloc(sizeof(Stmt));
    memset(s, 0, sizeof(Stmt));
    s->type = type;
    s->line = line;
    return s;
}

Stmt* upp_make_var_decl_stmt(const char* name, Expr* expr, int line) {
    Stmt* s = alloc_stmt(STMT_VAR_DECL, line);
    s->as.var_decl.name = upp_xstrdup(name);
    s->as.var_decl.expr = expr;
    return s;
}

Stmt* upp_make_assign_stmt(const char* name, Expr* expr, int line) {
    Stmt* s = alloc_stmt(STMT_ASSIGN, line);
    s->as.assign.name = upp_xstrdup(name);
    s->as.assign.expr = expr;
    return s;
}

Stmt* upp_make_index_assign_stmt(Expr* target, Expr* index, Expr* value, int line) {
    Stmt* s = alloc_stmt(STMT_INDEX_ASSIGN, line);
    s->as.index_assign.target = target;
    s->as.index_assign.index = index;
    s->as.index_assign.value = value;
    return s;
}

Stmt* upp_make_print_stmt(Expr* expr, int line) {
    Stmt* s = alloc_stmt(STMT_PRINT, line);
    s->as.print.expr = expr;
    return s;
}

Stmt* upp_make_if_stmt(Expr* cond, StmtList then_b, StmtList else_b, int has_else, int line) {
    Stmt* s = alloc_stmt(STMT_IF, line);
    s->as.if_stmt.condition = cond;
    s->as.if_stmt.then_branch = then_b;
    s->as.if_stmt.else_branch = else_b;
    s->as.if_stmt.has_else = has_else;
    return s;
}

Stmt* upp_make_while_stmt(Expr* cond, StmtList body, int line) {
    Stmt* s = alloc_stmt(STMT_WHILE, line);
    s->as.while_stmt.condition = cond;
    s->as.while_stmt.body = body;
    return s;
}

Stmt* upp_make_func_stmt(const char* name, char** params, int param_count, StmtList body, int line) {
    Stmt* s = alloc_stmt(STMT_FUNC, line);
    s->as.func.name = upp_xstrdup(name);
    s->as.func.params = params;
    s->as.func.param_count = param_count;
    s->as.func.body = body;
    return s;
}

Stmt* upp_make_return_stmt(Expr* expr, int line) {
    Stmt* s = alloc_stmt(STMT_RETURN, line);
    s->as.ret.expr = expr;
    return s;
}

Stmt* upp_make_expr_stmt(Expr* expr, int line) {
    Stmt* s = alloc_stmt(STMT_EXPR, line);
    s->as.expr_stmt.expr = expr;
    return s;
}

/* ------------------------------------------------------------------ */
/* Список інструкцій                                                   */
/* ------------------------------------------------------------------ */

void upp_stmt_list_init(StmtList* list) {
    list->statements = NULL;
    list->count = 0;
    list->capacity = 0;
}

void upp_stmt_list_append(StmtList* list, Stmt* stmt) {
    if (!stmt) return; /* парсер повертає NULL після помилки */
    if (list->count + 1 > list->capacity) {
        list->capacity = list->capacity < 8 ? 8 : list->capacity * 2;
        list->statements = (Stmt**)upp_xrealloc(list->statements, sizeof(Stmt*) * list->capacity);
    }
    list->statements[list->count++] = stmt;
}

void upp_stmt_list_free(StmtList* list) {
    for (int i = 0; i < list->count; i++) {
        upp_free_stmt(list->statements[i]);
    }
    free(list->statements);
    upp_stmt_list_init(list);
}

/* ------------------------------------------------------------------ */
/* Звільнення                                                          */
/* ------------------------------------------------------------------ */

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
            upp_free_expr(expr->as.binary.right);
            break;
        case EXPR_UNARY:
            upp_free_expr(expr->as.unary.expr);
            break;
        case EXPR_CALL:
            free(expr->as.call.name);
            for (int i = 0; i < expr->as.call.arg_count; i++) {
                upp_free_expr(expr->as.call.args[i]);
            }
            free(expr->as.call.args);
            break;
        case EXPR_LIST:
            for (int i = 0; i < expr->as.list.count; i++) {
                upp_free_expr(expr->as.list.items[i]);
            }
            free(expr->as.list.items);
            break;
        case EXPR_INDEX:
            upp_free_expr(expr->as.index.target);
            upp_free_expr(expr->as.index.index);
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
        case STMT_ASSIGN:
            free(stmt->as.assign.name);
            upp_free_expr(stmt->as.assign.expr);
            break;
        case STMT_INDEX_ASSIGN:
            upp_free_expr(stmt->as.index_assign.target);
            upp_free_expr(stmt->as.index_assign.index);
            upp_free_expr(stmt->as.index_assign.value);
            break;
        case STMT_PRINT:
            upp_free_expr(stmt->as.print.expr);
            break;
        case STMT_IF:
            upp_free_expr(stmt->as.if_stmt.condition);
            upp_stmt_list_free(&stmt->as.if_stmt.then_branch);
            upp_stmt_list_free(&stmt->as.if_stmt.else_branch);
            break;
        case STMT_WHILE:
            upp_free_expr(stmt->as.while_stmt.condition);
            upp_stmt_list_free(&stmt->as.while_stmt.body);
            break;
        case STMT_FUNC:
            free(stmt->as.func.name);
            for (int i = 0; i < stmt->as.func.param_count; i++) {
                free(stmt->as.func.params[i]);
            }
            free(stmt->as.func.params);
            upp_stmt_list_free(&stmt->as.func.body);
            break;
        case STMT_RETURN:
            upp_free_expr(stmt->as.ret.expr);
            break;
        case STMT_EXPR:
            upp_free_expr(stmt->as.expr_stmt.expr);
            break;
    }
    free(stmt);
}

void upp_free_program(Program* prog) {
    upp_stmt_list_free(&prog->statements);
}
