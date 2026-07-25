#include "upp_lexer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static void token_array_init(UppTokenArray* arr) {
    arr->tokens = NULL;
    arr->count = 0;
    arr->capacity = 0;
}

static void token_array_append(UppTokenArray* arr, UppToken tok) {
    if (arr->count + 1 > arr->capacity) {
        arr->capacity = arr->capacity < 8 ? 8 : arr->capacity * 2;
        arr->tokens = (UppToken*)realloc(arr->tokens, sizeof(UppToken) * arr->capacity);
    }
    arr->tokens[arr->count++] = tok;
}

void upp_free_token_array(UppTokenArray* arr) {
    if (arr->tokens) {
        for (int i = 0; i < arr->count; i++) {
            upp_free_token(arr->tokens[i]);
        }
        free(arr->tokens);
        arr->tokens = NULL;
    }
    arr->count = 0;
    arr->capacity = 0;
}

static int is_ident_start(unsigned char c) {
    return isalpha(c) || c == '_' || c >= 0x80;
}

static int is_ident_body(unsigned char c) {
    return isalnum(c) || c == '_' || c >= 0x80;
}

static char lexer_peek(Lexer* lexer) {
    return lexer->source[lexer->pos];
}

static char lexer_advance(Lexer* lexer) {
    char c = lexer->source[lexer->pos++];
    lexer->col++;
    return c;
}

static void lexer_advance_line(Lexer* lexer) {
    lexer->pos++;
    lexer->line++;
    lexer->col = 1;
}

static void handle_indentation(Lexer* lexer, UppTokenArray* arr) {
    lexer->at_line_start = 0;
    size_t start_pos = lexer->pos;
    int indent = 0;

    while (lexer->source[lexer->pos] != '\0') {
        char ch = lexer_peek(lexer);
        if (ch == ' ') {
            indent++;
            lexer_advance(lexer);
        } else if (ch == '\t') {
            indent += 4;
            lexer_advance(lexer);
        } else if (ch == '\r') {
            lexer_advance(lexer);
        } else {
            break;
        }
    }

    if (lexer->source[lexer->pos] == '\0') return;

    char nxt = lexer_peek(lexer);
    if (nxt == '\n' || nxt == '#') {
        lexer->pos = start_pos;
        lexer->col = 1;
        lexer->at_line_start = 1;
        return;
    }

    int current = lexer->indents[lexer->indent_depth];
    if (indent > current) {
        if (lexer->indent_depth < 63) {
            lexer->indent_depth++;
            lexer->indents[lexer->indent_depth] = indent;
            UppToken tok = {TOKEN_INDENT, _strdup(""), 0, lexer->line, 1};
            token_array_append(arr, tok);
        }
    } else if (indent < current) {
        while (lexer->indent_depth > 0 && indent < lexer->indents[lexer->indent_depth]) {
            lexer->indent_depth--;
            UppToken tok = {TOKEN_DEDENT, _strdup(""), 0, lexer->line, 1};
            token_array_append(arr, tok);
        }
    }
}

UppTokenArray upp_tokenize(const char* source) {
    Lexer lexer;
    lexer.source = source;
    lexer.pos = 0;
    lexer.line = 1;
    lexer.col = 1;
    lexer.indents[0] = 0;
    lexer.indent_depth = 0;
    lexer.at_line_start = 1;

    UppTokenArray arr;
    token_array_init(&arr);

    while (source[lexer.pos] != '\0') {
        if (lexer.at_line_start) {
            handle_indentation(&lexer, &arr);
        }

        if (source[lexer.pos] == '\0') break;

        char ch = lexer_peek(&lexer);

        if (ch == '#') {
            while (source[lexer.pos] != '\0' && lexer_peek(&lexer) != '\n') {
                lexer_advance(&lexer);
            }
            continue;
        }

        if (ch == ' ' || ch == '\t' || ch == '\r') {
            lexer_advance(&lexer);
            continue;
        }

        if (ch == '\n') {
            UppToken tok = {TOKEN_NEWLINE, _strdup("\n"), 0, lexer.line, lexer.col};
            token_array_append(&arr, tok);
            lexer_advance_line(&lexer);
            lexer.at_line_start = 1;
            continue;
        }

        if (ch == '"') {
            int start_col = lexer.col;
            lexer_advance(&lexer); // skip opening "
            size_t start_p = lexer.pos;

            while (source[lexer.pos] != '\0' && lexer_peek(&lexer) != '"') {
                if (lexer_peek(&lexer) == '\n') {
                    fprintf(stderr, "Лексична помилка на рядку %d: Незавершений рядковий літерал\n", lexer.line);
                    break;
                }
                lexer_advance(&lexer);
            }

            size_t len = lexer.pos - start_p;
            char* str_val = (char*)malloc(len + 1);
            strncpy(str_val, source + start_p, len);
            str_val[len] = '\0';

            if (lexer_peek(&lexer) == '"') {
                lexer_advance(&lexer);
            }

            UppToken tok = {TOKEN_STRING, str_val, 0, lexer.line, start_col};
            token_array_append(&arr, tok);
            continue;
        }

        if (isdigit((unsigned char)ch)) {
            int start_col = lexer.col;
            size_t start_p = lexer.pos;

            while (source[lexer.pos] != '\0' && isdigit((unsigned char)lexer_peek(&lexer))) {
                lexer_advance(&lexer);
            }

            size_t len = lexer.pos - start_p;
            char* num_str = (char*)malloc(len + 1);
            strncpy(num_str, source + start_p, len);
            num_str[len] = '\0';
            long long num_val = atoll(num_str);

            UppToken tok = {TOKEN_NUMBER, num_str, num_val, lexer.line, start_col};
            token_array_append(&arr, tok);
            continue;
        }

        if (is_ident_start((unsigned char)ch)) {
            int start_col = lexer.col;
            size_t start_p = lexer.pos;

            while (source[lexer.pos] != '\0' && is_ident_body((unsigned char)lexer_peek(&lexer))) {
                lexer_advance(&lexer);
            }

            size_t len = lexer.pos - start_p;
            char* ident_str = (char*)malloc(len + 1);
            strncpy(ident_str, source + start_p, len);
            ident_str[len] = '\0';

            UppTokenType type = upp_lookup_keyword(ident_str);
            UppToken tok = {type, ident_str, 0, lexer.line, start_col};
            token_array_append(&arr, tok);
            continue;
        }

        fprintf(stderr, "Лексична помилка на рядку %d: Невідомий символ '%c'\n", lexer.line, ch);
        lexer_advance(&lexer);
    }

    while (lexer.indent_depth > 0) {
        lexer.indent_depth--;
        UppToken tok = {TOKEN_DEDENT, _strdup(""), 0, lexer.line, 1};
        token_array_append(&arr, tok);
    }

    UppToken eof_tok = {TOKEN_EOF, _strdup(""), 0, lexer.line, lexer.col};
    token_array_append(&arr, eof_tok);

    return arr;
}
