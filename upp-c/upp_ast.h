#ifndef UPP_AST_H
#define UPP_AST_H

/* ------------------------------------------------------------------ */
/* Операції                                                            */
/* ------------------------------------------------------------------ */

typedef enum {
    BIN_ADD,   /* додати         */
    BIN_SUB,   /* відняти        */
    BIN_MUL,   /* помножити      */
    BIN_DIV,   /* поділити       */
    BIN_GT,    /* більше         */
    BIN_LT,    /* менше          */
    BIN_EQ,    /* дорівнює       */
    BIN_NE,    /* не дорівнює    */
    BIN_GE,    /* не менше       */
    BIN_LE,    /* не більше      */
    BIN_AND,   /* і              */
    BIN_OR     /* або            */
} BinOp;

typedef enum {
    UN_NOT,    /* не             */
    UN_NEG     /* -              */
} UnOp;

const char* upp_bin_op_name(BinOp op);
const char* upp_un_op_name(UnOp op);

/* ------------------------------------------------------------------ */
/* Вирази                                                              */
/* ------------------------------------------------------------------ */

typedef enum {
    EXPR_NUMBER,
    EXPR_REAL,
    EXPR_STRING,
    EXPR_BOOL,
    EXPR_VARIABLE,
    EXPR_BINARY,
    EXPR_UNARY,
    EXPR_CALL,
    EXPR_LIST,
    EXPR_INDEX
} ExprType;

typedef struct Expr Expr;

struct Expr {
    ExprType type;
    int      line;   /* потрібен, щоб помилки виконання мали позицію */
    union {
        long long number_val;
        double    real_val;
        char*     string_val;
        int       bool_val;
        char*     var_name;
        struct {
            Expr* left;
            BinOp op;
            Expr* right;
        } binary;
        struct {
            UnOp  op;
            Expr* expr;
        } unary;
        struct {
            char*  name;
            Expr** args;
            int    arg_count;
        } call;
        struct {
            Expr** items;
            int    count;
        } list;
        struct {
            Expr* target;
            Expr* index;
        } index;
    } as;
};

/* ------------------------------------------------------------------ */
/* Інструкції                                                          */
/* ------------------------------------------------------------------ */

typedef enum {
    STMT_VAR_DECL,      /* нехай x буде e   */
    STMT_ASSIGN,        /* x стає e         */
    STMT_INDEX_ASSIGN,  /* x[i] стає e      */
    STMT_PRINT,         /* показати e       */
    STMT_IF,
    STMT_WHILE,
    STMT_FUNC,          /* функція f(a, b)  */
    STMT_RETURN,        /* повернути e      */
    STMT_EXPR           /* виклик як інструкція */
} StmtType;

typedef struct Stmt Stmt;

typedef struct {
    Stmt** statements;
    int    count;
    int    capacity;
} StmtList;

/* Оголошення функції живе в AST; значення-функція лише посилається на
   нього, тому параметри й тіло не дублюються. */
typedef struct {
    char*    name;
    char**   params;
    int      param_count;
    StmtList body;
} FuncDecl;

struct Stmt {
    StmtType type;
    int      line;
    union {
        struct {
            char* name;
            Expr* expr;
        } var_decl;
        struct {
            char* name;
            Expr* expr;
        } assign;
        struct {
            Expr* target;
            Expr* index;
            Expr* value;
        } index_assign;
        struct {
            Expr* expr;
        } print;
        struct {
            Expr*    condition;
            StmtList then_branch;
            StmtList else_branch;
            int      has_else;
        } if_stmt;
        struct {
            Expr*    condition;
            StmtList body;
        } while_stmt;
        FuncDecl func;
        struct {
            Expr* expr;      /* може бути NULL: «повернути» без значення */
        } ret;
        struct {
            Expr* expr;
        } expr_stmt;
    } as;
};

typedef struct {
    StmtList statements;
} Program;

/* ------------------------------------------------------------------ */
/* Конструктори                                                        */
/* ------------------------------------------------------------------ */

Expr* upp_make_number_expr(long long val, int line);
Expr* upp_make_real_expr(double val, int line);
Expr* upp_make_string_expr(const char* val, int line);
Expr* upp_make_bool_expr(int val, int line);
Expr* upp_make_var_expr(const char* name, int line);
Expr* upp_make_binary_expr(Expr* left, BinOp op, Expr* right, int line);
Expr* upp_make_unary_expr(UnOp op, Expr* expr, int line);
Expr* upp_make_call_expr(const char* name, Expr** args, int arg_count, int line);
Expr* upp_make_list_expr(Expr** items, int count, int line);
Expr* upp_make_index_expr(Expr* target, Expr* index, int line);

Stmt* upp_make_var_decl_stmt(const char* name, Expr* expr, int line);
Stmt* upp_make_assign_stmt(const char* name, Expr* expr, int line);
Stmt* upp_make_index_assign_stmt(Expr* target, Expr* index, Expr* value, int line);
Stmt* upp_make_print_stmt(Expr* expr, int line);
Stmt* upp_make_if_stmt(Expr* cond, StmtList then_b, StmtList else_b, int has_else, int line);
Stmt* upp_make_while_stmt(Expr* cond, StmtList body, int line);
Stmt* upp_make_func_stmt(const char* name, char** params, int param_count, StmtList body, int line);
Stmt* upp_make_return_stmt(Expr* expr, int line);
Stmt* upp_make_expr_stmt(Expr* expr, int line);

void upp_stmt_list_init(StmtList* list);
void upp_stmt_list_append(StmtList* list, Stmt* stmt);
void upp_stmt_list_free(StmtList* list);

void upp_free_expr(Expr* expr);
void upp_free_stmt(Stmt* stmt);
void upp_free_program(Program* prog);

#endif /* UPP_AST_H */
