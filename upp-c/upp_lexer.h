#ifndef UPP_LEXER_H
#define UPP_LEXER_H

#include "upp_tokens.h"

typedef struct {
    UppToken* tokens;
    int count;
    int capacity;
} UppTokenArray;

typedef struct {
    const char* source;
    size_t pos;
    int line;
    int col;
    int indents[64];
    int indent_depth;
    int at_line_start;
} Lexer;

UppTokenArray upp_tokenize(const char* source);
void upp_free_token_array(UppTokenArray* arr);

#endif // UPP_LEXER_H
