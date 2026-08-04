#ifndef UPP_INTERPRETER_H
#define UPP_INTERPRETER_H

#include "upp_ast.h"
#include "upp_value.h"

typedef struct {
    char* name;
    Value val;
} VarBinding;

typedef struct Environment {
    struct Environment* parent;   /* NULL лише у глобального оточення */
    VarBinding*         bindings;
    int                 count;
    int                 capacity;
} Environment;

typedef struct {
    Environment* globals;
    Environment* env;        /* поточне оточення */
    Value        ret_value;  /* значення останнього «повернути» */
    int          had_error;
    int          depth;      /* глибина викликів — захист від нескінченної рекурсії */
} Interpreter;

Interpreter upp_make_interpreter(void);
void        upp_free_interpreter(Interpreter* interp);

/* Повертає 0, якщо програма виконалась без помилок, інакше 1. */
int upp_interpret(Interpreter* interp, Program* program);

#endif /* UPP_INTERPRETER_H */
