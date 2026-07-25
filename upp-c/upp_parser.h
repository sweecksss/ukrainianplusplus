#ifndef UPP_PARSER_H
#define UPP_PARSER_H

#include "upp_lexer.h"
#include "upp_ast.h"

typedef struct {
    UppTokenArray tokens;
    int pos;
} Parser;

Program upp_parse(UppTokenArray tokens);

#endif // UPP_PARSER_H
