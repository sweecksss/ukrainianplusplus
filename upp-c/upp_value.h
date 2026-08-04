#ifndef UPP_VALUE_H
#define UPP_VALUE_H

#include <stddef.h>
#include "upp_ast.h"

typedef enum {
    VAL_NIL,
    VAL_NUMBER,    /* ціле  */
    VAL_REAL,      /* дробове */
    VAL_BOOL,
    VAL_STRING,
    VAL_LIST,
    VAL_FUNCTION
} ValueType;

typedef struct UppString   UppString;
typedef struct UppList     UppList;
typedef struct UppFunction UppFunction;

typedef struct {
    ValueType type;
    union {
        long long    number;
        double       real;
        int          boolean;
        UppString*   string;
        UppList*     list;
        UppFunction* func;
    } as;
} Value;

/* Рядки незмінні, тому спільне володіння непомітне ззовні. */
struct UppString {
    int    refcount;
    size_t len;      /* у байтах */
    char*  chars;
};

/* Списки — посилальний тип: передача у функцію не копіює вміст. */
struct UppList {
    int    refcount;
    Value* items;
    int    count;
    int    capacity;
};

/* Функція лише посилається на оголошення в AST, який живе довше. */
struct UppFunction {
    int             refcount;
    const FuncDecl* decl;
};

/* ------------------------------------------------------------------ */
/* Конструктори                                                        */
/* ------------------------------------------------------------------ */

Value upp_nil(void);
Value upp_number(long long n);
Value upp_real(double d);
Value upp_bool(int b);
Value upp_string(const char* chars);
Value upp_string_len(const char* chars, size_t len);
/* Приймає володіння вже виділеним буфером. */
Value upp_string_take(char* chars, size_t len);
Value upp_list(void);
Value upp_function(const FuncDecl* decl);

/* ------------------------------------------------------------------ */
/* Володіння                                                           */
/* ------------------------------------------------------------------ */

Value upp_retain(Value v);
void  upp_release(Value v);

/* ------------------------------------------------------------------ */
/* Операції                                                            */
/* ------------------------------------------------------------------ */

int         upp_truthy(Value v);
int         upp_equals(Value a, Value b);
const char* upp_type_name(Value v);

/* Обидві повертають щойно виділений рядок, який звільняє викликач.
   display — для «показати», repr — для елементів усередині списку
   (рядки в лапках, щоб було видно межі). */
char* upp_value_to_display(Value v);
char* upp_value_to_repr(Value v);

void  upp_list_append(UppList* list, Value v);
/* Довжина рядка в символах UTF-8 або кількість елементів списку. */
long long upp_value_length(Value v, int* ok);

/* Числове значення як double — для змішаної арифметики. */
double upp_as_double(Value v);
int    upp_is_numeric(Value v);

#endif /* UPP_VALUE_H */
