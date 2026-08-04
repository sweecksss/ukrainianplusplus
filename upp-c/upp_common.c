#include "upp_common.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Памʼять                                                             */
/* ------------------------------------------------------------------ */

static void out_of_memory(void) {
    fprintf(stderr, "Критична помилка: недостатньо памʼяті\n");
    exit(70);
}

void* upp_xmalloc(size_t size) {
    void* p = malloc(size);
    if (!p) out_of_memory();
    return p;
}

void* upp_xrealloc(void* ptr, size_t size) {
    void* p = realloc(ptr, size);
    if (!p) out_of_memory();
    return p;
}

char* upp_xstrdup(const char* s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char* copy = (char*)upp_xmalloc(n + 1);
    memcpy(copy, s, n + 1);
    return copy;
}

char* upp_xstrndup(const char* s, size_t n) {
    char* copy = (char*)upp_xmalloc(n + 1);
    if (n > 0) memcpy(copy, s, n);
    copy[n] = '\0';
    return copy;
}

/* ------------------------------------------------------------------ */
/* Діагностика                                                         */
/* ------------------------------------------------------------------ */

static int error_count = 0;

/* Понад цю межу повідомлення більше не друкуються — інакше один зсув
   у відступах породжує сотні однакових рядків. */
#define UPP_MAX_REPORTED_ERRORS 25

static void report(const char* stage, int line, int col, const char* fmt, va_list args) {
    error_count++;

    if (error_count > UPP_MAX_REPORTED_ERRORS) {
        if (error_count == UPP_MAX_REPORTED_ERRORS + 1) {
            fprintf(stderr, "... забагато помилок, подальші повідомлення приховано\n");
        }
        return;
    }

    if (line > 0 && col > 0) {
        fprintf(stderr, "%s (рядок %d, колонка %d): ", stage, line, col);
    } else if (line > 0) {
        fprintf(stderr, "%s (рядок %d): ", stage, line);
    } else {
        fprintf(stderr, "%s: ", stage);
    }

    vfprintf(stderr, fmt, args);
    fputc('\n', stderr);
}

void upp_error_at(const char* stage, int line, int col, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    report(stage, line, col, fmt, args);
    va_end(args);
}

void upp_error(const char* stage, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    report(stage, 0, 0, fmt, args);
    va_end(args);
}

int upp_error_count(void) {
    return error_count;
}

void upp_reset_errors(void) {
    error_count = 0;
}

const char* upp_plural(long long n, const char* one, const char* few, const char* many) {
    long long abs_n = n < 0 ? -n : n;
    long long last = abs_n % 10;
    long long last_two = abs_n % 100;

    if (last == 1 && last_two != 11) return one;
    if (last >= 2 && last <= 4 && (last_two < 12 || last_two > 14)) return few;
    return many;
}

/* ------------------------------------------------------------------ */
/* Рядковий буфер                                                      */
/* ------------------------------------------------------------------ */

void upp_sb_init(StrBuf* sb) {
    sb->data = NULL;
    sb->len = 0;
    sb->cap = 0;
}

static void sb_reserve(StrBuf* sb, size_t extra) {
    size_t needed = sb->len + extra + 1;
    if (needed <= sb->cap) return;

    size_t cap = sb->cap < 32 ? 32 : sb->cap;
    while (cap < needed) cap *= 2;

    sb->data = (char*)upp_xrealloc(sb->data, cap);
    sb->cap = cap;
}

void upp_sb_append_len(StrBuf* sb, const char* s, size_t n) {
    if (n == 0) {
        /* Навіть порожній додаток має гарантувати коректний C-рядок. */
        sb_reserve(sb, 0);
        sb->data[sb->len] = '\0';
        return;
    }
    sb_reserve(sb, n);
    memcpy(sb->data + sb->len, s, n);
    sb->len += n;
    sb->data[sb->len] = '\0';
}

void upp_sb_append(StrBuf* sb, const char* s) {
    if (!s) return;
    upp_sb_append_len(sb, s, strlen(s));
}

void upp_sb_append_char(StrBuf* sb, char c) {
    sb_reserve(sb, 1);
    sb->data[sb->len++] = c;
    sb->data[sb->len] = '\0';
}

void upp_sb_appendf(StrBuf* sb, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    va_list probe;
    va_copy(probe, args);
    int needed = vsnprintf(NULL, 0, fmt, probe);
    va_end(probe);

    if (needed > 0) {
        sb_reserve(sb, (size_t)needed);
        vsnprintf(sb->data + sb->len, sb->cap - sb->len, fmt, args);
        sb->len += (size_t)needed;
    }

    va_end(args);
}

char* upp_sb_take(StrBuf* sb, size_t* out_len) {
    if (!sb->data) {
        /* Порожній буфер усе одно має повернути валідний рядок. */
        sb->data = (char*)upp_xmalloc(1);
        sb->data[0] = '\0';
        sb->cap = 1;
    }
    char* result = sb->data;
    if (out_len) *out_len = sb->len;
    upp_sb_init(sb);
    return result;
}

void upp_sb_free(StrBuf* sb) {
    free(sb->data);
    upp_sb_init(sb);
}

/* ------------------------------------------------------------------ */
/* UTF-8                                                               */
/* ------------------------------------------------------------------ */

int upp_utf8_seq_len(unsigned char first) {
    if (first < 0x80) return 1;
    if ((first & 0xE0) == 0xC0) return 2;
    if ((first & 0xF0) == 0xE0) return 3;
    if ((first & 0xF8) == 0xF0) return 4;
    return 1; /* некоректний провідний байт */
}

int upp_utf8_is_continuation(unsigned char c) {
    return (c & 0xC0) == 0x80;
}

size_t upp_utf8_length(const char* s, size_t byte_len) {
    size_t chars = 0;
    for (size_t i = 0; i < byte_len; i++) {
        if (!upp_utf8_is_continuation((unsigned char)s[i])) chars++;
    }
    return chars;
}
