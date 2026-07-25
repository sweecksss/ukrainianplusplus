#ifndef UPP_TOKENS_H
#define UPP_TOKENS_H

typedef enum {
    TOKEN_EOF,
    TOKEN_NEWLINE,
    TOKEN_INDENT,
    TOKEN_DEDENT,

    TOKEN_NUMBER,
    TOKEN_IDENT,
    TOKEN_STRING,

    TOKEN_NEHAI,      // "нехай"
    TOKEN_BUDE,       // "буде"
    TOKEN_POKAZATY,   // "показати"
    TOKEN_TRUE,       // "правда"
    TOKEN_FALSE,      // "брехня"
    TOKEN_IF,         // "якщо"
    TOKEN_ELSE,       // "інакше"
    TOKEN_WHILE,      // "поки"
    TOKEN_TO,         // "то"

    TOKEN_AND,        // "і"
    TOKEN_OR,         // "або"
    TOKEN_NOT,        // "не"

    TOKEN_DODATY,     // "додати"
    TOKEN_VIDNIATY,   // "відняти"
    TOKEN_POMNOZHYTY, // "помножити"
    TOKEN_PODILYTY,   // "поділити"
    TOKEN_BILSHE,     // "більше"
    TOKEN_MENSHE,     // "менше"
    TOKEN_DORIVNYUE   // "дорівнює"
} UppTokenType;

typedef struct {
    UppTokenType type;
    char* lexeme;
    long long number_value;
    int line;
    int col;
} UppToken;

UppTokenType upp_lookup_keyword(const char* lexeme);
void upp_free_token(UppToken token);

#endif // UPP_TOKENS_H
