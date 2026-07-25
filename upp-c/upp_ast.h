#ifndef UPP_AST_H
#define UPP_AST_H

typedef enum {
    EXPR_NUMBER,
    EXPR_STRING,
    EXPR_BOOL,
    EXPR_VARIABLE,
    EXPR_BINARY,
    EXPR_UNARY
} ExprType;

typedef struct Expr Expr;

struct Expr {
    ExprType type;
    union {
        long long number_val;
        char* string_val;
        int bool_val;
        char* var_name;
        struct {
            Expr* left;
            char* op;
            Expr* right;
        } binary;
        struct {
            char* op;
            Expr* expr;
        } unary;
    } as;
};

typedef enum {
    STMT_VAR_DECL,
    STMT_PRINT,
    STMT_BLOCK,
    STMT_IF,
    STMT_WHILE
} StmtType;

typedef struct Stmt Stmt;

typedef struct {
    Stmt** statements;
    int count;
    int capacity;
} StmtList;

struct Stmt {
    StmtType type;
    union {
        struct {
            char* name;
            Expr* expr;
        } var_decl;
        struct {
            Expr* expr;
        } print;
        StmtList block;
        struct {
            Expr* condition;
            StmtList then_branch;
            StmtList else_branch;
            int has_else;
        } if_stmt;
        struct {
            Expr* condition;
            StmtList body;
        } while_stmt;
    } as;
};

typedef struct {
    StmtList statements;
} Program;

Expr* upp_make_number_expr(long long val);
Expr* upp_make_string_expr(const char* val);
Expr* upp_make_bool_expr(int val);
Expr* upp_make_var_expr(const char* name);
Expr* upp_make_binary_expr(Expr* left, const char* op, Expr* right);
Expr* upp_make_unary_expr(const char* op, Expr* expr);

Stmt* upp_make_var_decl_stmt(const char* name, Expr* expr);
Stmt* upp_make_print_stmt(Expr* expr);
Stmt* upp_make_if_stmt(Expr* cond, StmtList then_b, StmtList else_b, int has_else);
Stmt* upp_make_while_stmt(Expr* cond, StmtList body);

void upp_stmt_list_init(StmtList* list);
void upp_stmt_list_append(StmtList* list, Stmt* stmt);
void upp_free_expr(Expr* expr);
void upp_free_stmt(Stmt* stmt);
void upp_free_program(Program* prog);

#endif // UPP_AST_H
