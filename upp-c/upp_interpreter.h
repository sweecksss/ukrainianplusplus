#ifndef UPP_INTERPRETER_H
#define UPP_INTERPRETER_H

#include "upp_ast.h"

typedef enum {
    VAL_NUMBER,
    VAL_STRING,
    VAL_BOOL
} ValueType;

typedef struct {
    ValueType type;
    union {
        long long number;
        char* string;
        int boolean;
    } as;
} Value;

typedef struct {
    char* name;
    Value val;
} VarBinding;

typedef struct {
    VarBinding* bindings;
    int count;
    int capacity;
} Environment;

typedef struct {
    Environment env;
} Interpreter;

Interpreter upp_make_interpreter(void);
void upp_free_interpreter(Interpreter* interp);
void upp_interpret(Interpreter* interp, Program* program);

#endif // UPP_INTERPRETER_H
