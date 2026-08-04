#ifndef UPP_LEXER_H
#define UPP_LEXER_H

#include <stddef.h>
#include "upp_tokens.h"

typedef struct {
    UppToken* tokens;
    int       count;
    int       capacity;
} UppTokenArray;

/* Розбиває вихідний текст на токени.

   Помилки не зупиняють роботу — вони рахуються через upp_error_count(),
   щоб за один прохід показати користувачеві всі проблеми одразу. */
UppTokenArray upp_tokenize(const char* source);

void upp_free_token_array(UppTokenArray* arr);

#endif /* UPP_LEXER_H */
