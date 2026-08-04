#ifndef UPP_PARSER_H
#define UPP_PARSER_H

#include "upp_ast.h"
#include "upp_lexer.h"

typedef struct {
    UppTokenArray tokens;
    int           pos;
} Parser;

/* Будує AST. Помилки рахуються через upp_error_count(); програму, у
   якій є помилки розбору, виконувати не можна. */
Program upp_parse(UppTokenArray tokens);

#endif /* UPP_PARSER_H */
