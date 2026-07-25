#include "upp_tokens.h"
#include <string.h>
#include <stdlib.h>

typedef struct {
    const char* kw;
    UppTokenType type;
} KeywordMap;

static KeywordMap keywords[] = {
    {"нехай", TOKEN_NEHAI},
    {"буде", TOKEN_BUDE},
    {"показати", TOKEN_POKAZATY},
    {"правда", TOKEN_TRUE},
    {"брехня", TOKEN_FALSE},
    {"якщо", TOKEN_IF},
    {"інакше", TOKEN_ELSE},
    {"поки", TOKEN_WHILE},
    {"то", TOKEN_TO},
    {"і", TOKEN_AND},
    {"або", TOKEN_OR},
    {"не", TOKEN_NOT},
    {"додати", TOKEN_DODATY},
    {"відняти", TOKEN_VIDNIATY},
    {"помножити", TOKEN_POMNOZHYTY},
    {"поділити", TOKEN_PODILYTY},
    {"більше", TOKEN_BILSHE},
    {"менше", TOKEN_MENSHE},
    {"дорівнює", TOKEN_DORIVNYUE},
    {NULL, TOKEN_IDENT}
};

UppTokenType upp_lookup_keyword(const char* lexeme) {
    for (int i = 0; keywords[i].kw != NULL; i++) {
        if (strcmp(lexeme, keywords[i].kw) == 0) {
            return keywords[i].type;
        }
    }
    return TOKEN_IDENT;
}

void upp_free_token(UppToken token) {
    if (token.lexeme) {
        free(token.lexeme);
    }
}
