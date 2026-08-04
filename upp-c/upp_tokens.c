#include "upp_tokens.h"
#include "upp_common.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    const char*  kw;
    UppTokenType type;
} KeywordMap;

static const KeywordMap keywords[] = {
    {"нехай",     TOKEN_NEHAI},
    {"буде",      TOKEN_BUDE},
    {"стає",      TOKEN_STAYE},
    {"показати",  TOKEN_POKAZATY},
    {"правда",    TOKEN_TRUE},
    {"брехня",    TOKEN_FALSE},
    {"якщо",      TOKEN_IF},
    {"інакше",    TOKEN_ELSE},
    {"поки",      TOKEN_WHILE},
    {"то",        TOKEN_TO},
    {"функція",   TOKEN_FUNCTION},
    {"повернути", TOKEN_RETURN},
    {"і",         TOKEN_AND},
    {"та",        TOKEN_AND},
    {"або",       TOKEN_OR},
    {"не",        TOKEN_NOT},
    {"додати",    TOKEN_DODATY},
    {"відняти",   TOKEN_VIDNIATY},
    {"помножити", TOKEN_POMNOZHYTY},
    {"поділити",  TOKEN_PODILYTY},
    {"більше",    TOKEN_BILSHE},
    {"менше",     TOKEN_MENSHE},
    {"дорівнює",  TOKEN_DORIVNYUE},
    {NULL,        TOKEN_IDENT}
};

UppTokenType upp_lookup_keyword(const char* lexeme) {
    for (int i = 0; keywords[i].kw != NULL; i++) {
        if (strcmp(lexeme, keywords[i].kw) == 0) {
            return keywords[i].type;
        }
    }
    return TOKEN_IDENT;
}

const char* upp_token_type_name(UppTokenType type) {
    switch (type) {
        case TOKEN_EOF:      return "кінець файлу";
        case TOKEN_NEWLINE:  return "кінець рядка";
        case TOKEN_INDENT:   return "початок блоку";
        case TOKEN_DEDENT:   return "кінець блоку";
        case TOKEN_NUMBER:   return "число";
        case TOKEN_REAL:     return "дробове число";
        case TOKEN_IDENT:    return "назва";
        case TOKEN_STRING:   return "рядок";
        default:             return "слово";
    }
}

const char* upp_token_describe(const UppToken* token) {
    switch (token->type) {
        case TOKEN_EOF:     return "кінець файлу";
        case TOKEN_NEWLINE: return "кінець рядка";
        case TOKEN_INDENT:  return "початок блоку";
        case TOKEN_DEDENT:  return "кінець блоку";
        default:            return token->lexeme ? token->lexeme : "?";
    }
}

void upp_free_token(UppToken token) {
    free(token.lexeme);
}
