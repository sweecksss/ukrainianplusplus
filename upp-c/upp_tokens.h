#ifndef UPP_TOKENS_H
#define UPP_TOKENS_H

typedef enum {
    TOKEN_EOF,
    TOKEN_NEWLINE,
    TOKEN_INDENT,
    TOKEN_DEDENT,

    TOKEN_NUMBER,     /* ціле число      */
    TOKEN_REAL,       /* дробове число   */
    TOKEN_IDENT,
    TOKEN_STRING,     /* lexeme = сирий текст між лапками */

    /* Розділові знаки */
    TOKEN_LPAREN,     /* (  */
    TOKEN_RPAREN,     /* )  */
    TOKEN_LBRACKET,   /* [  */
    TOKEN_RBRACKET,   /* ]  */
    TOKEN_COMMA,      /* ,  */
    TOKEN_MINUS,      /* -  (унарний мінус) */

    /* Оголошення та присвоєння */
    TOKEN_NEHAI,      /* "нехай"     */
    TOKEN_BUDE,       /* "буде"      */
    TOKEN_STAYE,      /* "стає"      */

    /* Інструкції */
    TOKEN_POKAZATY,   /* "показати"  */
    TOKEN_IF,         /* "якщо"      */
    TOKEN_ELSE,       /* "інакше"    */
    TOKEN_WHILE,      /* "поки"      */
    TOKEN_TO,         /* "то"        */
    TOKEN_FUNCTION,   /* "функція"   */
    TOKEN_RETURN,     /* "повернути" */

    /* Літерали */
    TOKEN_TRUE,       /* "правда"    */
    TOKEN_FALSE,      /* "брехня"    */

    /* Логічні операції */
    TOKEN_AND,        /* "і"         */
    TOKEN_OR,         /* "або"       */
    TOKEN_NOT,        /* "не"        */

    /* Арифметика */
    TOKEN_DODATY,     /* "додати"    */
    TOKEN_VIDNIATY,   /* "відняти"   */
    TOKEN_POMNOZHYTY, /* "помножити" */
    TOKEN_PODILYTY,   /* "поділити"  */

    /* Порівняння */
    TOKEN_BILSHE,     /* "більше"    */
    TOKEN_MENSHE,     /* "менше"     */
    TOKEN_DORIVNYUE   /* "дорівнює"  */
} UppTokenType;

typedef struct {
    UppTokenType type;
    char*        lexeme;
    long long    number_value;  /* для TOKEN_NUMBER */
    double       real_value;    /* для TOKEN_REAL   */
    int          line;
    int          col;           /* у символах, не в байтах */
} UppToken;

UppTokenType upp_lookup_keyword(const char* lexeme);

/* Людяна назва типу токена — для повідомлень про помилки. */
const char* upp_token_type_name(UppTokenType type);

/* Текст токена так, як його варто показати користувачеві. */
const char* upp_token_describe(const UppToken* token);

void upp_free_token(UppToken token);

#endif /* UPP_TOKENS_H */
