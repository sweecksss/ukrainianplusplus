#include "upp_parser.h"
#include "upp_common.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Дрібні помічники                                                    */
/* ------------------------------------------------------------------ */

static UppToken* parser_peek(Parser* p) {
    return &p->tokens.tokens[p->pos];
}

static UppToken* parser_peek_next(Parser* p) {
    if (p->tokens.tokens[p->pos].type == TOKEN_EOF) return &p->tokens.tokens[p->pos];
    return &p->tokens.tokens[p->pos + 1];
}

static UppToken* parser_previous(Parser* p) {
    return &p->tokens.tokens[p->pos > 0 ? p->pos - 1 : 0];
}

static int parser_is_at_end(Parser* p) {
    return parser_peek(p)->type == TOKEN_EOF;
}

static int parser_check(Parser* p, UppTokenType type) {
    return parser_peek(p)->type == type;
}

static UppToken* parser_advance(Parser* p) {
    if (!parser_is_at_end(p)) p->pos++;
    return parser_previous(p);
}

static int parser_match(Parser* p, UppTokenType type) {
    if (parser_check(p, type)) {
        parser_advance(p);
        return 1;
    }
    return 0;
}

/* Формулює повідомлення й додає до нього те, що реально трапилось. */
/* «і» — і сполучник, і найзвичніша назва для лічильника циклу. Оскільки
   в U++ два вирази ніколи не стоять поруч, позиція однозначно каже, що
   саме мається на увазі: на місці операції це «і», на місці значення —
   назва змінної. Тому «нехай і буде 0» і «а і б» працюють обидва. */
static int is_name_token(UppTokenType type) {
    return type == TOKEN_IDENT || type == TOKEN_AND;
}

static void error_at_token(Parser* p, const UppToken* tok, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    va_list probe;
    va_copy(probe, args);
    int needed = vsnprintf(NULL, 0, fmt, probe);
    va_end(probe);

    size_t size = (size_t)(needed > 0 ? needed : 0) + 1;
    char*  msg = (char*)upp_xmalloc(size);
    vsnprintf(msg, size, fmt, args);
    va_end(args);

    upp_error_at(UPP_STAGE_PARSE, tok->line, tok->col, "%s. Знайдено: '%s'",
                 msg, upp_token_describe(tok));

    free(msg);
    (void)p;
}

/* Після помилки пропускаємо решту рядка, щоб не сипати каскадом
   похідних повідомлень. INDENT/DEDENT не чіпаємо — вони тримають
   структуру блоків. */
static void synchronize(Parser* p) {
    while (!parser_is_at_end(p)) {
        if (parser_check(p, TOKEN_INDENT) || parser_check(p, TOKEN_DEDENT)) return;
        if (parser_check(p, TOKEN_NEWLINE)) {
            parser_advance(p);
            return;
        }
        parser_advance(p);
    }
}

static int parser_consume(Parser* p, UppTokenType type, const char* what) {
    if (parser_check(p, type)) {
        parser_advance(p);
        return 1;
    }
    error_at_token(p, parser_peek(p), "Очікувалось %s", what);
    return 0;
}

/* Інструкція має закінчуватись кінцем рядка (або блоку/файлу). */
static int consume_statement_end(Parser* p) {
    if (parser_check(p, TOKEN_NEWLINE)) {
        parser_advance(p);
        return 1;
    }
    if (parser_check(p, TOKEN_EOF) || parser_check(p, TOKEN_DEDENT)) {
        return 1;
    }
    error_at_token(p, parser_peek(p), "Очікувався кінець рядка");
    return 0;
}

/* ------------------------------------------------------------------ */
/* Попередні оголошення                                                */
/* ------------------------------------------------------------------ */

static Expr*    parse_expression(Parser* p);
static Stmt*    parse_statement(Parser* p);
static StmtList parse_suite(Parser* p);

/* ------------------------------------------------------------------ */
/* Рядкові літерали: екранування + підстановка {змінна}                */
/* ------------------------------------------------------------------ */

static int ident_start_byte(unsigned char c) {
    return isalpha(c) || c == '_' || c >= 0x80;
}

static int ident_body_byte(unsigned char c) {
    return isalnum(c) || c == '_' || c >= 0x80;
}

static int is_valid_ident(const char* s, size_t len) {
    if (len == 0) return 0;
    if (!ident_start_byte((unsigned char)s[0])) return 0;
    for (size_t i = 1; i < len; i++) {
        if (!ident_body_byte((unsigned char)s[i])) return 0;
    }
    return 1;
}

/* Перетворює сирий текст літерала на вираз. Якщо всередині є {змінна},
   будується ланцюжок конкатенацій, тож підстановка більше не потребує
   окремої логіки під час виконання. */
static Expr* build_string_expr(Parser* p, const char* raw, int line, int col) {
    StrBuf literal;
    upp_sb_init(&literal);

    Expr* result = NULL;
    int   interpolated = 0;

    size_t len = strlen(raw);
    size_t i = 0;

    while (i < len) {
        char c = raw[i];

        if (c == '\\' && i + 1 < len) {
            char esc = raw[i + 1];
            i += 2;
            switch (esc) {
                case 'n':  upp_sb_append_char(&literal, '\n'); break;
                case 't':  upp_sb_append_char(&literal, '\t'); break;
                case 'r':  upp_sb_append_char(&literal, '\r'); break;
                case '"':  upp_sb_append_char(&literal, '"');  break;
                case '\\': upp_sb_append_char(&literal, '\\'); break;
                case '{':  upp_sb_append_char(&literal, '{');  break;
                case '}':  upp_sb_append_char(&literal, '}');  break;
                default:
                    upp_error_at(UPP_STAGE_PARSE, line, col,
                                 "Невідома екранована послідовність '\\%c' у рядку", esc);
                    upp_sb_append_char(&literal, esc);
                    break;
            }
            continue;
        }

        if (c == '{') {
            size_t start = ++i;
            while (i < len && raw[i] != '}') i++;

            if (i >= len) {
                upp_error_at(UPP_STAGE_PARSE, line, col,
                             "Незакрита підстановка у рядку: бракує '}'");
                upp_sb_append_char(&literal, '{');
                upp_sb_append_len(&literal, raw + start, len - start);
                break;
            }

            /* Прибираємо пробіли навколо імені: "{ ім'я }" теж має працювати. */
            size_t name_start = start;
            size_t name_end = i;
            while (name_start < name_end && raw[name_start] == ' ') name_start++;
            while (name_end > name_start && raw[name_end - 1] == ' ') name_end--;

            size_t name_len = name_end - name_start;
            i++; /* пропускаємо '}' */

            if (!is_valid_ident(raw + name_start, name_len)) {
                upp_error_at(UPP_STAGE_PARSE, line, col,
                             "У підстановці має бути назва змінної, а не '%.*s'",
                             (int)name_len, raw + name_start);
                continue;
            }

            /* Замикаємо накопичений текст і додаємо змінну. */
            size_t lit_len = 0;
            char*  lit = upp_sb_take(&literal, &lit_len);
            Expr*  piece = upp_make_string_expr(lit, line);
            free(lit);

            result = result ? upp_make_binary_expr(result, BIN_ADD, piece, line) : piece;

            char* name = upp_xstrndup(raw + name_start, name_len);
            Expr* var = upp_make_var_expr(name, line);
            free(name);

            result = upp_make_binary_expr(result, BIN_ADD, var, line);
            interpolated = 1;
            continue;
        }

        upp_sb_append_char(&literal, c);
        i++;
    }

    size_t tail_len = 0;
    char*  tail = upp_sb_take(&literal, &tail_len);

    if (!interpolated) {
        Expr* e = upp_make_string_expr(tail, line);
        free(tail);
        upp_sb_free(&literal);
        return e;
    }

    if (tail_len > 0) {
        Expr* piece = upp_make_string_expr(tail, line);
        result = upp_make_binary_expr(result, BIN_ADD, piece, line);
    }
    free(tail);
    upp_sb_free(&literal);

    (void)p;
    return result;
}

/* ------------------------------------------------------------------ */
/* Вирази                                                              */
/* ------------------------------------------------------------------ */

static Expr** parse_expression_list(Parser* p, UppTokenType closer, const char* closer_name, int* out_count) {
    Expr** items = NULL;
    int    count = 0;
    int    capacity = 0;

    if (!parser_check(p, closer)) {
        do {
            Expr* item = parse_expression(p);
            if (!item) break;
            if (count + 1 > capacity) {
                capacity = capacity < 4 ? 4 : capacity * 2;
                items = (Expr**)upp_xrealloc(items, sizeof(Expr*) * capacity);
            }
            items[count++] = item;
        } while (parser_match(p, TOKEN_COMMA));
    }

    parser_consume(p, closer, closer_name);

    *out_count = count;
    return items;
}

static Expr* parse_primary(Parser* p) {
    UppToken* tok = parser_peek(p);

    switch (tok->type) {
        case TOKEN_NUMBER:
            parser_advance(p);
            return upp_make_number_expr(tok->number_value, tok->line);

        case TOKEN_REAL:
            parser_advance(p);
            return upp_make_real_expr(tok->real_value, tok->line);

        case TOKEN_STRING:
            parser_advance(p);
            return build_string_expr(p, tok->lexeme, tok->line, tok->col);

        case TOKEN_TRUE:
            parser_advance(p);
            return upp_make_bool_expr(1, tok->line);

        case TOKEN_FALSE:
            parser_advance(p);
            return upp_make_bool_expr(0, tok->line);

        case TOKEN_AND:   /* «і» на місці значення — це назва змінної */
        case TOKEN_IDENT: {
            parser_advance(p);
            char* name = upp_xstrdup(tok->lexeme);

            if (parser_match(p, TOKEN_LPAREN)) {
                int    arg_count = 0;
                Expr** args = parse_expression_list(p, TOKEN_RPAREN, "')' після аргументів", &arg_count);
                Expr*  call = upp_make_call_expr(name, args, arg_count, tok->line);
                free(name);
                return call;
            }

            Expr* var = upp_make_var_expr(name, tok->line);
            free(name);
            return var;
        }

        case TOKEN_LPAREN: {
            parser_advance(p);
            Expr* inner = parse_expression(p);
            parser_consume(p, TOKEN_RPAREN, "')' після виразу в дужках");
            return inner;
        }

        case TOKEN_LBRACKET: {
            parser_advance(p);
            int    count = 0;
            Expr** items = parse_expression_list(p, TOKEN_RBRACKET, "']' після елементів списку", &count);
            return upp_make_list_expr(items, count, tok->line);
        }

        default:
            error_at_token(p, tok, "Очікувався вираз");
            return NULL;
    }
}

static Expr* parse_postfix(Parser* p) {
    Expr* expr = parse_primary(p);
    if (!expr) return NULL;

    while (parser_check(p, TOKEN_LBRACKET)) {
        int line = parser_peek(p)->line;
        parser_advance(p);
        Expr* index = parse_expression(p);
        if (!index) {
            upp_free_expr(expr);
            return NULL;
        }
        parser_consume(p, TOKEN_RBRACKET, "']' після індексу");
        expr = upp_make_index_expr(expr, index, line);
    }

    return expr;
}

/* Унарний мінус живе на рівні арифметики: -а помножити б == (-а) * б */
static Expr* parse_unary(Parser* p) {
    if (parser_check(p, TOKEN_MINUS)) {
        int line = parser_peek(p)->line;
        parser_advance(p);
        Expr* operand = parse_unary(p);
        if (!operand) return NULL;
        return upp_make_unary_expr(UN_NEG, operand, line);
    }
    return parse_postfix(p);
}

static Expr* parse_term(Parser* p) {
    Expr* expr = parse_unary(p);
    if (!expr) return NULL;

    while (parser_check(p, TOKEN_POMNOZHYTY) || parser_check(p, TOKEN_PODILYTY)) {
        BinOp op = parser_check(p, TOKEN_POMNOZHYTY) ? BIN_MUL : BIN_DIV;
        int   line = parser_peek(p)->line;
        parser_advance(p);

        Expr* right = parse_unary(p);
        if (!right) {
            upp_free_expr(expr);
            return NULL;
        }
        expr = upp_make_binary_expr(expr, op, right, line);
    }
    return expr;
}

static Expr* parse_additive(Parser* p) {
    Expr* expr = parse_term(p);
    if (!expr) return NULL;

    while (parser_check(p, TOKEN_DODATY) || parser_check(p, TOKEN_VIDNIATY)) {
        BinOp op = parser_check(p, TOKEN_DODATY) ? BIN_ADD : BIN_SUB;
        int   line = parser_peek(p)->line;
        parser_advance(p);

        Expr* right = parse_term(p);
        if (!right) {
            upp_free_expr(expr);
            return NULL;
        }
        expr = upp_make_binary_expr(expr, op, right, line);
    }
    return expr;
}

/* Складені оператори порівняння: «не дорівнює», «не менше», «не більше».
   Слово «не» в цій позиції не може починати нічого іншого, бо унарне
   заперечення розбирається рівнем вище. */
static int match_comparison_op(Parser* p, BinOp* out_op, int* out_line) {
    UppToken* tok = parser_peek(p);
    *out_line = tok->line;

    switch (tok->type) {
        case TOKEN_BILSHE:    parser_advance(p); *out_op = BIN_GT; return 1;
        case TOKEN_MENSHE:    parser_advance(p); *out_op = BIN_LT; return 1;
        case TOKEN_DORIVNYUE: parser_advance(p); *out_op = BIN_EQ; return 1;
        case TOKEN_NOT: {
            UppTokenType next = parser_peek_next(p)->type;
            if (next == TOKEN_DORIVNYUE) { parser_advance(p); parser_advance(p); *out_op = BIN_NE; return 1; }
            if (next == TOKEN_MENSHE)    { parser_advance(p); parser_advance(p); *out_op = BIN_GE; return 1; }
            if (next == TOKEN_BILSHE)    { parser_advance(p); parser_advance(p); *out_op = BIN_LE; return 1; }
            return 0;
        }
        default:
            return 0;
    }
}

static Expr* parse_comparison(Parser* p) {
    Expr* expr = parse_additive(p);
    if (!expr) return NULL;

    BinOp op;
    int   line;
    while (match_comparison_op(p, &op, &line)) {
        Expr* right = parse_additive(p);
        if (!right) {
            upp_free_expr(expr);
            return NULL;
        }
        expr = upp_make_binary_expr(expr, op, right, line);
    }
    return expr;
}

/* «не» стоїть вище за порівняння, як і в більшості мов:
   «не а дорівнює б» читається як «не (а дорівнює б)». */
static Expr* parse_not(Parser* p) {
    if (parser_check(p, TOKEN_NOT)) {
        /* Але «не дорівнює» — це оператор порівняння, а не заперечення. */
        UppTokenType next = parser_peek_next(p)->type;
        if (next != TOKEN_DORIVNYUE && next != TOKEN_MENSHE && next != TOKEN_BILSHE) {
            int line = parser_peek(p)->line;
            parser_advance(p);
            Expr* operand = parse_not(p);
            if (!operand) return NULL;
            return upp_make_unary_expr(UN_NOT, operand, line);
        }
    }
    return parse_comparison(p);
}

static Expr* parse_and(Parser* p) {
    Expr* expr = parse_not(p);
    if (!expr) return NULL;

    while (parser_check(p, TOKEN_AND)) {
        int line = parser_peek(p)->line;
        parser_advance(p);
        Expr* right = parse_not(p);
        if (!right) {
            upp_free_expr(expr);
            return NULL;
        }
        expr = upp_make_binary_expr(expr, BIN_AND, right, line);
    }
    return expr;
}

static Expr* parse_or(Parser* p) {
    Expr* expr = parse_and(p);
    if (!expr) return NULL;

    while (parser_check(p, TOKEN_OR)) {
        int line = parser_peek(p)->line;
        parser_advance(p);
        Expr* right = parse_and(p);
        if (!right) {
            upp_free_expr(expr);
            return NULL;
        }
        expr = upp_make_binary_expr(expr, BIN_OR, right, line);
    }
    return expr;
}

static Expr* parse_expression(Parser* p) {
    return parse_or(p);
}

/* ------------------------------------------------------------------ */
/* Інструкції                                                          */
/* ------------------------------------------------------------------ */

static Stmt* parse_var_decl(Parser* p) {
    int line = parser_peek(p)->line;
    parser_advance(p); /* нехай */

    if (!is_name_token(parser_peek(p)->type)) {
        error_at_token(p, parser_peek(p), "Очікувалась назва змінної після 'нехай'");
        synchronize(p);
        return NULL;
    }

    char* name = upp_xstrdup(parser_advance(p)->lexeme);

    if (!parser_consume(p, TOKEN_BUDE, "слово 'буде'")) {
        free(name);
        synchronize(p);
        return NULL;
    }

    Expr* expr = parse_expression(p);
    if (!expr) {
        free(name);
        synchronize(p);
        return NULL;
    }

    consume_statement_end(p);

    Stmt* stmt = upp_make_var_decl_stmt(name, expr, line);
    free(name);
    return stmt;
}

static Stmt* parse_print(Parser* p) {
    int line = parser_peek(p)->line;
    parser_advance(p); /* показати */

    Expr* expr = parse_expression(p);
    if (!expr) {
        synchronize(p);
        return NULL;
    }

    consume_statement_end(p);
    return upp_make_print_stmt(expr, line);
}

static Stmt* parse_return(Parser* p) {
    int line = parser_peek(p)->line;
    parser_advance(p); /* повернути */

    Expr* expr = NULL;
    if (!parser_check(p, TOKEN_NEWLINE) && !parser_check(p, TOKEN_DEDENT) && !parser_check(p, TOKEN_EOF)) {
        expr = parse_expression(p);
        if (!expr) {
            synchronize(p);
            return NULL;
        }
    }

    consume_statement_end(p);
    return upp_make_return_stmt(expr, line);
}

/* Тіло блоку: або відступ на наступних рядках, або одна інструкція
   на тому ж рядку («якщо а більше б то показати а»). */
static StmtList parse_suite(Parser* p) {
    StmtList list;
    upp_stmt_list_init(&list);

    if (!parser_check(p, TOKEN_NEWLINE)) {
        /* Однорядковий варіант. */
        upp_stmt_list_append(&list, parse_statement(p));
        return list;
    }

    while (parser_match(p, TOKEN_NEWLINE)) {}

    if (!parser_match(p, TOKEN_INDENT)) {
        error_at_token(p, parser_peek(p),
                       "Очікувався блок з відступом (додайте пробіли на початку рядків тіла)");
        return list;
    }

    while (!parser_check(p, TOKEN_DEDENT) && !parser_check(p, TOKEN_EOF)) {
        if (parser_match(p, TOKEN_NEWLINE)) continue;
        upp_stmt_list_append(&list, parse_statement(p));
    }

    parser_match(p, TOKEN_DEDENT);
    return list;
}

static Stmt* parse_if(Parser* p) {
    int line = parser_peek(p)->line;
    parser_advance(p); /* якщо */

    Expr* cond = parse_expression(p);
    if (!cond) {
        synchronize(p);
        return NULL;
    }

    parser_match(p, TOKEN_TO);
    StmtList then_branch = parse_suite(p);

    StmtList else_branch;
    upp_stmt_list_init(&else_branch);
    int has_else = 0;

    /* «інакше» може стояти після порожніх рядків. */
    int save = p->pos;
    while (parser_check(p, TOKEN_NEWLINE)) parser_advance(p);

    if (parser_match(p, TOKEN_ELSE)) {
        has_else = 1;
        parser_match(p, TOKEN_TO);
        else_branch = parse_suite(p);
    } else {
        p->pos = save;
    }

    return upp_make_if_stmt(cond, then_branch, else_branch, has_else, line);
}

static Stmt* parse_while(Parser* p) {
    int line = parser_peek(p)->line;
    parser_advance(p); /* поки */

    Expr* cond = parse_expression(p);
    if (!cond) {
        synchronize(p);
        return NULL;
    }

    parser_match(p, TOKEN_TO);
    StmtList body = parse_suite(p);

    return upp_make_while_stmt(cond, body, line);
}

static Stmt* parse_function(Parser* p) {
    int line = parser_peek(p)->line;
    parser_advance(p); /* функція */

    if (!is_name_token(parser_peek(p)->type)) {
        error_at_token(p, parser_peek(p), "Очікувалась назва функції");
        synchronize(p);
        return NULL;
    }

    char* name = upp_xstrdup(parser_advance(p)->lexeme);

    if (!parser_consume(p, TOKEN_LPAREN, "'(' після назви функції")) {
        free(name);
        synchronize(p);
        return NULL;
    }

    char** params = NULL;
    int    param_count = 0;
    int    param_cap = 0;

    if (!parser_check(p, TOKEN_RPAREN)) {
        do {
            if (!is_name_token(parser_peek(p)->type)) {
                error_at_token(p, parser_peek(p), "Очікувалась назва параметра");
                break;
            }
            if (param_count + 1 > param_cap) {
                param_cap = param_cap < 4 ? 4 : param_cap * 2;
                params = (char**)upp_xrealloc(params, sizeof(char*) * param_cap);
            }
            params[param_count++] = upp_xstrdup(parser_advance(p)->lexeme);
        } while (parser_match(p, TOKEN_COMMA));
    }

    parser_consume(p, TOKEN_RPAREN, "')' після параметрів");
    parser_match(p, TOKEN_TO);

    StmtList body = parse_suite(p);

    Stmt* stmt = upp_make_func_stmt(name, params, param_count, body, line);
    free(name);
    return stmt;
}

/* Або присвоєння («x стає ...», «x[i] стає ...»), або вираз-інструкція
   (виклик функції заради побічного ефекту). Розбираємо вираз, а тоді
   дивимось, чи йде за ним «стає». */
static Stmt* parse_expr_or_assign(Parser* p) {
    int   line = parser_peek(p)->line;
    Expr* target = parse_expression(p);

    if (!target) {
        synchronize(p);
        return NULL;
    }

    if (parser_match(p, TOKEN_STAYE)) {
        Expr* value = parse_expression(p);
        if (!value) {
            upp_free_expr(target);
            synchronize(p);
            return NULL;
        }

        consume_statement_end(p);

        if (target->type == EXPR_VARIABLE) {
            Stmt* stmt = upp_make_assign_stmt(target->as.var_name, value, line);
            upp_free_expr(target);
            return stmt;
        }

        if (target->type == EXPR_INDEX) {
            Stmt* stmt = upp_make_index_assign_stmt(target->as.index.target,
                                                    target->as.index.index,
                                                    value, line);
            /* Вузол-обгортку звільняємо, а його частини перейшли в stmt. */
            free(target);
            return stmt;
        }

        upp_error_at(UPP_STAGE_PARSE, line, 1,
                     "Ліворуч від 'стає' має бути змінна або елемент списку");
        upp_free_expr(target);
        upp_free_expr(value);
        return NULL;
    }

    consume_statement_end(p);

    if (target->type != EXPR_CALL) {
        upp_error_at(UPP_STAGE_PARSE, line, 1,
                     "Цей вираз нічого не робить. Можливо, бракує 'показати' або 'стає'");
        upp_free_expr(target);
        return NULL;
    }

    return upp_make_expr_stmt(target, line);
}

static Stmt* parse_statement(Parser* p) {
    switch (parser_peek(p)->type) {
        case TOKEN_NEHAI:    return parse_var_decl(p);
        case TOKEN_POKAZATY: return parse_print(p);
        case TOKEN_IF:       return parse_if(p);
        case TOKEN_WHILE:    return parse_while(p);
        case TOKEN_FUNCTION: return parse_function(p);
        case TOKEN_RETURN:   return parse_return(p);

        case TOKEN_INDENT:
            error_at_token(p, parser_peek(p), "Зайвий відступ");
            parser_advance(p);
            return NULL;

        case TOKEN_ELSE:
            error_at_token(p, parser_peek(p), "'інакше' без відповідного 'якщо'");
            synchronize(p);
            return NULL;

        default:
            return parse_expr_or_assign(p);
    }
}

/* ------------------------------------------------------------------ */
/* Точка входу                                                         */
/* ------------------------------------------------------------------ */

Program upp_parse(UppTokenArray tokens) {
    Parser p;
    p.tokens = tokens;
    p.pos = 0;

    Program prog;
    upp_stmt_list_init(&prog.statements);

    while (!parser_check(&p, TOKEN_EOF)) {
        if (parser_match(&p, TOKEN_NEWLINE)) continue;
        if (parser_match(&p, TOKEN_DEDENT)) continue;

        int before = p.pos;
        upp_stmt_list_append(&prog.statements, parse_statement(&p));

        /* Страховка від зациклення: кожна ітерація має рухати позицію. */
        if (p.pos == before) parser_advance(&p);
    }

    return prog;
}
