#ifndef UPP_COMMON_H
#define UPP_COMMON_H

#include <stddef.h>

/* ------------------------------------------------------------------ */
/* Виділення памʼяті з перевіркою                                      */
/*                                                                     */
/* Увесь код користується цими обгортками: вони не повертають NULL,    */
/* тож жодне місце в інтерпретаторі не мусить це перевіряти.           */
/* ------------------------------------------------------------------ */

void* upp_xmalloc(size_t size);
void* upp_xrealloc(void* ptr, size_t size);
char* upp_xstrdup(const char* s);
char* upp_xstrndup(const char* s, size_t n);

/* ------------------------------------------------------------------ */
/* Діагностика                                                         */
/* ------------------------------------------------------------------ */

#define UPP_STAGE_LEX     "Лексична помилка"
#define UPP_STAGE_PARSE   "Синтаксична помилка"
#define UPP_STAGE_RUNTIME "Помилка виконання"

/* Повідомляє про помилку і збільшує лічильник. */
void upp_error_at(const char* stage, int line, int col, const char* fmt, ...);
void upp_error(const char* stage, const char* fmt, ...);

int  upp_error_count(void);
void upp_reset_errors(void);

/* ------------------------------------------------------------------ */
/* Динамічний рядковий буфер                                           */
/* ------------------------------------------------------------------ */

typedef struct {
    char*  data;
    size_t len;
    size_t cap;
} StrBuf;

void  upp_sb_init(StrBuf* sb);
void  upp_sb_append(StrBuf* sb, const char* s);
void  upp_sb_append_len(StrBuf* sb, const char* s, size_t n);
void  upp_sb_append_char(StrBuf* sb, char c);
void  upp_sb_appendf(StrBuf* sb, const char* fmt, ...);
/* Віддає володіння буфером; StrBuf стає порожнім. */
char* upp_sb_take(StrBuf* sb, size_t* out_len);
void  upp_sb_free(StrBuf* sb);

/* Українська форма множини: 1 аргумент, 2 аргументи, 5 аргументів. */
const char* upp_plural(long long n, const char* one, const char* few, const char* many);

/* ------------------------------------------------------------------ */
/* UTF-8                                                               */
/* ------------------------------------------------------------------ */

/* Довжина послідовності UTF-8 за першим байтом (1..4, або 1 якщо байт
   некоректний). */
int upp_utf8_seq_len(unsigned char first);

/* Чи є байт продовженням послідовності UTF-8 (10xxxxxx). */
int upp_utf8_is_continuation(unsigned char c);

/* Кількість символів (а не байтів) у рядку UTF-8. */
size_t upp_utf8_length(const char* s, size_t byte_len);

#endif /* UPP_COMMON_H */
