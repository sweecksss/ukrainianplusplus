#include "upp_value.h"
#include "upp_common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Конструктори                                                        */
/* ------------------------------------------------------------------ */

Value upp_nil(void) {
    Value v;
    v.type = VAL_NIL;
    v.as.number = 0;
    return v;
}

Value upp_number(long long n) {
    Value v;
    v.type = VAL_NUMBER;
    v.as.number = n;
    return v;
}

Value upp_real(double d) {
    Value v;
    v.type = VAL_REAL;
    v.as.real = d;
    return v;
}

Value upp_bool(int b) {
    Value v;
    v.type = VAL_BOOL;
    v.as.boolean = b != 0;
    return v;
}

Value upp_string_take(char* chars, size_t len) {
    UppString* s = (UppString*)upp_xmalloc(sizeof(UppString));
    s->refcount = 1;
    s->len = len;
    s->chars = chars;

    Value v;
    v.type = VAL_STRING;
    v.as.string = s;
    return v;
}

Value upp_string_len(const char* chars, size_t len) {
    return upp_string_take(upp_xstrndup(chars, len), len);
}

Value upp_string(const char* chars) {
    if (!chars) chars = "";
    return upp_string_len(chars, strlen(chars));
}

Value upp_list(void) {
    UppList* l = (UppList*)upp_xmalloc(sizeof(UppList));
    l->refcount = 1;
    l->items = NULL;
    l->count = 0;
    l->capacity = 0;

    Value v;
    v.type = VAL_LIST;
    v.as.list = l;
    return v;
}

Value upp_function(const FuncDecl* decl) {
    UppFunction* f = (UppFunction*)upp_xmalloc(sizeof(UppFunction));
    f->refcount = 1;
    f->decl = decl;

    Value v;
    v.type = VAL_FUNCTION;
    v.as.func = f;
    return v;
}

/* ------------------------------------------------------------------ */
/* Володіння                                                           */
/* ------------------------------------------------------------------ */

Value upp_retain(Value v) {
    switch (v.type) {
        case VAL_STRING:   v.as.string->refcount++; break;
        case VAL_LIST:     v.as.list->refcount++;   break;
        case VAL_FUNCTION: v.as.func->refcount++;   break;
        default: break;
    }
    return v;
}

void upp_release(Value v) {
    switch (v.type) {
        case VAL_STRING:
            if (--v.as.string->refcount == 0) {
                free(v.as.string->chars);
                free(v.as.string);
            }
            break;
        case VAL_LIST:
            if (--v.as.list->refcount == 0) {
                for (int i = 0; i < v.as.list->count; i++) {
                    upp_release(v.as.list->items[i]);
                }
                free(v.as.list->items);
                free(v.as.list);
            }
            break;
        case VAL_FUNCTION:
            if (--v.as.func->refcount == 0) {
                /* decl належить AST — звільняти його тут не можна. */
                free(v.as.func);
            }
            break;
        default:
            break;
    }
}

/* ------------------------------------------------------------------ */
/* Операції                                                            */
/* ------------------------------------------------------------------ */

int upp_truthy(Value v) {
    switch (v.type) {
        case VAL_NIL:      return 0;
        case VAL_BOOL:     return v.as.boolean;
        case VAL_NUMBER:   return v.as.number != 0;
        case VAL_REAL:     return v.as.real != 0.0;
        case VAL_STRING:   return v.as.string->len > 0;
        case VAL_LIST:     return v.as.list->count > 0;
        case VAL_FUNCTION: return 1;
    }
    return 0;
}

int upp_is_numeric(Value v) {
    return v.type == VAL_NUMBER || v.type == VAL_REAL;
}

double upp_as_double(Value v) {
    if (v.type == VAL_NUMBER) return (double)v.as.number;
    if (v.type == VAL_REAL)   return v.as.real;
    return 0.0;
}

int upp_equals(Value a, Value b) {
    /* Числа порівнюються між собою незалежно від того, ціле це чи дріб. */
    if (upp_is_numeric(a) && upp_is_numeric(b)) {
        if (a.type == VAL_NUMBER && b.type == VAL_NUMBER) {
            return a.as.number == b.as.number;
        }
        return upp_as_double(a) == upp_as_double(b);
    }

    if (a.type != b.type) return 0;

    switch (a.type) {
        case VAL_NIL:  return 1;
        case VAL_BOOL: return a.as.boolean == b.as.boolean;
        case VAL_STRING:
            return a.as.string->len == b.as.string->len &&
                   memcmp(a.as.string->chars, b.as.string->chars, a.as.string->len) == 0;
        case VAL_LIST: {
            if (a.as.list == b.as.list) return 1;
            if (a.as.list->count != b.as.list->count) return 0;
            for (int i = 0; i < a.as.list->count; i++) {
                if (!upp_equals(a.as.list->items[i], b.as.list->items[i])) return 0;
            }
            return 1;
        }
        case VAL_FUNCTION: return a.as.func->decl == b.as.func->decl;
        default: return 0;
    }
}

const char* upp_type_name(Value v) {
    switch (v.type) {
        case VAL_NIL:      return "ніщо";
        case VAL_NUMBER:   return "число";
        case VAL_REAL:     return "дробове число";
        case VAL_BOOL:     return "булеве значення";
        case VAL_STRING:   return "рядок";
        case VAL_LIST:     return "список";
        case VAL_FUNCTION: return "функція";
    }
    return "невідоме";
}

/* ------------------------------------------------------------------ */
/* Текстове представлення                                              */
/* ------------------------------------------------------------------ */

/* Списки можуть містити самі себе (додати_до(с, с)), тож обмежуємо
   глибину, щоб друк не з'їв увесь стек. */
#define UPP_MAX_PRINT_DEPTH 32

static void format_real(StrBuf* sb, double d) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%.10g", d);

    /* Щоб дробове число залишалося візуально дробовим: 2 -> 2.0 */
    if (!strpbrk(buf, ".eEn")) { /* 'n' покриває nan/inf */
        size_t n = strlen(buf);
        if (n + 3 < sizeof(buf)) {
            buf[n] = '.';
            buf[n + 1] = '0';
            buf[n + 2] = '\0';
        }
    }
    upp_sb_append(sb, buf);
}

static void value_to_buf(StrBuf* sb, Value v, int quote_strings, int depth) {
    switch (v.type) {
        case VAL_NIL:
            upp_sb_append(sb, "ніщо");
            break;
        case VAL_NUMBER:
            upp_sb_appendf(sb, "%lld", v.as.number);
            break;
        case VAL_REAL:
            format_real(sb, v.as.real);
            break;
        case VAL_BOOL:
            upp_sb_append(sb, v.as.boolean ? "правда" : "брехня");
            break;
        case VAL_STRING:
            if (quote_strings) {
                upp_sb_append_char(sb, '"');
                upp_sb_append_len(sb, v.as.string->chars, v.as.string->len);
                upp_sb_append_char(sb, '"');
            } else {
                upp_sb_append_len(sb, v.as.string->chars, v.as.string->len);
            }
            break;
        case VAL_LIST: {
            if (depth >= UPP_MAX_PRINT_DEPTH) {
                upp_sb_append(sb, "[...]");
                break;
            }
            upp_sb_append_char(sb, '[');
            for (int i = 0; i < v.as.list->count; i++) {
                if (i > 0) upp_sb_append(sb, ", ");
                value_to_buf(sb, v.as.list->items[i], 1, depth + 1);
            }
            upp_sb_append_char(sb, ']');
            break;
        }
        case VAL_FUNCTION:
            upp_sb_appendf(sb, "<функція %s>", v.as.func->decl->name);
            break;
    }
}

char* upp_value_to_display(Value v) {
    StrBuf sb;
    upp_sb_init(&sb);
    value_to_buf(&sb, v, 0, 0);
    return upp_sb_take(&sb, NULL);
}

char* upp_value_to_repr(Value v) {
    StrBuf sb;
    upp_sb_init(&sb);
    value_to_buf(&sb, v, 1, 0);
    return upp_sb_take(&sb, NULL);
}

/* ------------------------------------------------------------------ */
/* Списки                                                              */
/* ------------------------------------------------------------------ */

void upp_list_append(UppList* list, Value v) {
    if (list->count + 1 > list->capacity) {
        list->capacity = list->capacity < 8 ? 8 : list->capacity * 2;
        list->items = (Value*)upp_xrealloc(list->items, sizeof(Value) * list->capacity);
    }
    list->items[list->count++] = upp_retain(v);
}

long long upp_value_length(Value v, int* ok) {
    *ok = 1;
    if (v.type == VAL_STRING) {
        return (long long)upp_utf8_length(v.as.string->chars, v.as.string->len);
    }
    if (v.type == VAL_LIST) {
        return v.as.list->count;
    }
    *ok = 0;
    return 0;
}
