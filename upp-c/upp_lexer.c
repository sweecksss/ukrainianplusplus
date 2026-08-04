#include "upp_lexer.h"
#include "upp_common.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char* source;
    size_t      pos;
    int         line;
    int         col;          /* у символах UTF-8, не в байтах */
    int*        indents;      /* стек рівнів відступу, без штучної межі */
    int         indent_count;
    int         indent_cap;
    int         at_line_start;
} Lexer;

/* ------------------------------------------------------------------ */
/* Масив токенів                                                       */
/* ------------------------------------------------------------------ */

static void token_array_init(UppTokenArray* arr) {
    arr->tokens = NULL;
    arr->count = 0;
    arr->capacity = 0;
}

static void token_array_append(UppTokenArray* arr, UppToken tok) {
    if (arr->count + 1 > arr->capacity) {
        arr->capacity = arr->capacity < 8 ? 8 : arr->capacity * 2;
        arr->tokens = (UppToken*)upp_xrealloc(arr->tokens, sizeof(UppToken) * arr->capacity);
    }
    arr->tokens[arr->count++] = tok;
}

void upp_free_token_array(UppTokenArray* arr) {
    if (arr->tokens) {
        for (int i = 0; i < arr->count; i++) {
            upp_free_token(arr->tokens[i]);
        }
        free(arr->tokens);
    }
    token_array_init(arr);
}

static UppToken make_token(UppTokenType type, char* lexeme, int line, int col) {
    UppToken tok;
    tok.type = type;
    tok.lexeme = lexeme;
    tok.number_value = 0;
    tok.real_value = 0.0;
    tok.line = line;
    tok.col = col;
    return tok;
}

/* ------------------------------------------------------------------ */
/* Основа                                                              */
/* ------------------------------------------------------------------ */

static int is_ident_start(unsigned char c) {
    /* Байти >= 0x80 — це літери кирилиці в UTF-8. */
    return isalpha(c) || c == '_' || c >= 0x80;
}

static int is_ident_body(unsigned char c) {
    return isalnum(c) || c == '_' || c >= 0x80;
}

static char lexer_peek(Lexer* lexer) {
    return lexer->source[lexer->pos];
}

static char lexer_peek_at(Lexer* lexer, size_t offset) {
    /* Безпечно: рядок завершується '\0', тож заглядання за кінець
       поверне '\0', а не вийде за межі буфера. */
    for (size_t i = 0; i < offset; i++) {
        if (lexer->source[lexer->pos + i] == '\0') return '\0';
    }
    return lexer->source[lexer->pos + offset];
}

static char lexer_advance(Lexer* lexer) {
    char c = lexer->source[lexer->pos++];
    /* Колонку рахуємо в символах: продовження UTF-8 не збільшують її. */
    if (!upp_utf8_is_continuation((unsigned char)c)) lexer->col++;
    return c;
}

static void lexer_advance_line(Lexer* lexer) {
    lexer->pos++;
    lexer->line++;
    lexer->col = 1;
}

/* ------------------------------------------------------------------ */
/* Стек відступів                                                      */
/* ------------------------------------------------------------------ */

static void indent_push(Lexer* lexer, int level) {
    if (lexer->indent_count + 1 > lexer->indent_cap) {
        lexer->indent_cap = lexer->indent_cap < 16 ? 16 : lexer->indent_cap * 2;
        lexer->indents = (int*)upp_xrealloc(lexer->indents, sizeof(int) * lexer->indent_cap);
    }
    lexer->indents[lexer->indent_count++] = level;
}

static int indent_top(Lexer* lexer) {
    return lexer->indents[lexer->indent_count - 1];
}

static void handle_indentation(Lexer* lexer, UppTokenArray* arr) {
    lexer->at_line_start = 0;

    size_t start_pos = lexer->pos;
    int    start_col = lexer->col;
    int    indent = 0;

    while (lexer_peek(lexer) != '\0') {
        char ch = lexer_peek(lexer);
        if (ch == ' ') {
            indent++;
            lexer_advance(lexer);
        } else if (ch == '\t') {
            indent += 4; /* табуляція = 4 пробіли */
            lexer_advance(lexer);
        } else if (ch == '\r') {
            lexer_advance(lexer);
        } else {
            break;
        }
    }

    if (lexer_peek(lexer) == '\0') return;

    /* Порожні рядки та рядки лише з коментарем не змінюють структуру блоків. */
    char nxt = lexer_peek(lexer);
    if (nxt == '\n' || nxt == '#') {
        lexer->pos = start_pos;
        lexer->col = start_col;
        lexer->at_line_start = 1;
        return;
    }

    int current = indent_top(lexer);

    if (indent > current) {
        indent_push(lexer, indent);
        token_array_append(arr, make_token(TOKEN_INDENT, upp_xstrdup(""), lexer->line, 1));
    } else if (indent < current) {
        while (lexer->indent_count > 1 && indent < indent_top(lexer)) {
            lexer->indent_count--;
            token_array_append(arr, make_token(TOKEN_DEDENT, upp_xstrdup(""), lexer->line, 1));
        }
        /* Рівень, на який ми вийшли, має точно збігтися з одним із
           попередніх — інакше відступи неузгоджені. */
        if (indent != indent_top(lexer)) {
            upp_error_at(UPP_STAGE_LEX, lexer->line, 1,
                         "Неузгоджений відступ: %d %s не відповідають жодному з відкритих блоків "
                         "(найближчий рівень — %d)",
                         indent, upp_plural(indent, "пробіл", "пробіли", "пробілів"),
                         indent_top(lexer));
        }
    }
}

/* ------------------------------------------------------------------ */
/* Літерали                                                            */
/* ------------------------------------------------------------------ */

/* Рядок зберігається СИРИМ: екрановані послідовності та підстановки
   {змінна} розбирає парсер, якому потрібен доступ до самих символів
   `\` і `{`. Тут ми лише знаходимо межі літерала. */
static void lex_string(Lexer* lexer, UppTokenArray* arr) {
    int start_line = lexer->line;
    int start_col = lexer->col;

    lexer_advance(lexer); /* відкривальна лапка */
    size_t start_p = lexer->pos;

    int terminated = 0;
    while (lexer_peek(lexer) != '\0') {
        char c = lexer_peek(lexer);

        if (c == '\n') break; /* літерал не може перетинати рядки */

        if (c == '\\') {
            lexer_advance(lexer);
            if (lexer_peek(lexer) == '\0' || lexer_peek(lexer) == '\n') break;
            lexer_advance(lexer);
            continue;
        }

        if (c == '"') {
            terminated = 1;
            break;
        }

        lexer_advance(lexer);
    }

    size_t len = lexer->pos - start_p;
    char*  raw = upp_xstrndup(lexer->source + start_p, len);

    if (terminated) {
        lexer_advance(lexer); /* закривальна лапка */
    } else {
        upp_error_at(UPP_STAGE_LEX, start_line, start_col,
                     "Незавершений рядковий літерал: бракує закривальної лапки");
    }

    token_array_append(arr, make_token(TOKEN_STRING, raw, start_line, start_col));
}

static void lex_number(Lexer* lexer, UppTokenArray* arr) {
    int    start_col = lexer->col;
    size_t start_p = lexer->pos;

    while (isdigit((unsigned char)lexer_peek(lexer))) {
        lexer_advance(lexer);
    }

    int is_real = 0;
    /* Крапка починає дробову частину лише якщо за нею йде цифра:
       так «1.» лишається помилкою, а не мовчазним 1.0 */
    if (lexer_peek(lexer) == '.' && isdigit((unsigned char)lexer_peek_at(lexer, 1))) {
        is_real = 1;
        lexer_advance(lexer); /* крапка */
        while (isdigit((unsigned char)lexer_peek(lexer))) {
            lexer_advance(lexer);
        }
    }

    size_t len = lexer->pos - start_p;
    char*  text = upp_xstrndup(lexer->source + start_p, len);

    UppToken tok = make_token(is_real ? TOKEN_REAL : TOKEN_NUMBER, text, lexer->line, start_col);
    if (is_real) {
        tok.real_value = strtod(text, NULL);
    } else {
        tok.number_value = strtoll(text, NULL, 10);
    }

    /* Зайва крапка одразу після числа — типова помилка на кшталт «1.2.3». */
    if (lexer_peek(lexer) == '.') {
        upp_error_at(UPP_STAGE_LEX, lexer->line, lexer->col,
                     "Некоректне число: зайва крапка після '%s'", text);
        lexer_advance(lexer);
    }

    token_array_append(arr, tok);
}

static void lex_identifier(Lexer* lexer, UppTokenArray* arr) {
    int    start_col = lexer->col;
    size_t start_p = lexer->pos;

    while (is_ident_body((unsigned char)lexer_peek(lexer))) {
        lexer_advance(lexer);
    }

    size_t len = lexer->pos - start_p;
    char*  text = upp_xstrndup(lexer->source + start_p, len);

    token_array_append(arr, make_token(upp_lookup_keyword(text), text, lexer->line, start_col));
}

/* ------------------------------------------------------------------ */
/* Головний цикл                                                       */
/* ------------------------------------------------------------------ */

static void lex_unknown_char(Lexer* lexer) {
    /* Друкуємо весь символ UTF-8, а не перший його байт — інакше
       в повідомленні буде «кракозябра». */
    int  start_col = lexer->col;
    int  seq = upp_utf8_seq_len((unsigned char)lexer_peek(lexer));
    char buf[5];
    int  n = 0;

    for (; n < seq && lexer_peek(lexer) != '\0'; n++) {
        buf[n] = lexer_advance(lexer);
    }
    buf[n] = '\0';

    upp_error_at(UPP_STAGE_LEX, lexer->line, start_col,
                 "Невідомий символ '%s'", buf);
}

UppTokenArray upp_tokenize(const char* source) {
    /* Блокнот та інші редактори Windows зберігають UTF-8 із BOM.
       Без цього рядка перше ключове слово злипається з невидимими
       байтами й програма не запускається взагалі. */
    if ((unsigned char)source[0] == 0xEF &&
        (unsigned char)source[1] == 0xBB &&
        (unsigned char)source[2] == 0xBF) {
        source += 3;
    }

    Lexer lexer;
    lexer.source = source;
    lexer.pos = 0;
    lexer.line = 1;
    lexer.col = 1;
    lexer.indents = NULL;
    lexer.indent_count = 0;
    lexer.indent_cap = 0;
    lexer.at_line_start = 1;
    indent_push(&lexer, 0);

    UppTokenArray arr;
    token_array_init(&arr);

    while (lexer_peek(&lexer) != '\0') {
        if (lexer.at_line_start) {
            handle_indentation(&lexer, &arr);
            if (lexer_peek(&lexer) == '\0') break;
        }

        char ch = lexer_peek(&lexer);

        if (ch == '#') {
            while (lexer_peek(&lexer) != '\0' && lexer_peek(&lexer) != '\n') {
                lexer_advance(&lexer);
            }
            continue;
        }

        if (ch == ' ' || ch == '\t' || ch == '\r') {
            lexer_advance(&lexer);
            continue;
        }

        if (ch == '\n') {
            token_array_append(&arr, make_token(TOKEN_NEWLINE, upp_xstrdup("\\n"), lexer.line, lexer.col));
            lexer_advance_line(&lexer);
            lexer.at_line_start = 1;
            continue;
        }

        if (ch == '"') {
            lex_string(&lexer, &arr);
            continue;
        }

        if (isdigit((unsigned char)ch)) {
            lex_number(&lexer, &arr);
            continue;
        }

        if (is_ident_start((unsigned char)ch)) {
            lex_identifier(&lexer, &arr);
            continue;
        }

        UppTokenType punct = TOKEN_EOF;
        switch (ch) {
            case '(': punct = TOKEN_LPAREN;   break;
            case ')': punct = TOKEN_RPAREN;   break;
            case '[': punct = TOKEN_LBRACKET; break;
            case ']': punct = TOKEN_RBRACKET; break;
            case ',': punct = TOKEN_COMMA;    break;
            case '-': punct = TOKEN_MINUS;    break;
            default:  break;
        }

        if (punct != TOKEN_EOF) {
            int start_col = lexer.col;
            char text[2] = {ch, '\0'};
            lexer_advance(&lexer);
            token_array_append(&arr, make_token(punct, upp_xstrdup(text), lexer.line, start_col));
            continue;
        }

        lex_unknown_char(&lexer);
    }

    /* Файл може закінчитися всередині блоків — закриваємо їх усі. */
    while (lexer.indent_count > 1) {
        lexer.indent_count--;
        token_array_append(&arr, make_token(TOKEN_DEDENT, upp_xstrdup(""), lexer.line, 1));
    }

    token_array_append(&arr, make_token(TOKEN_EOF, upp_xstrdup(""), lexer.line, lexer.col));

    free(lexer.indents);
    return arr;
}
