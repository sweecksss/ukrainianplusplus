#include "upp_interpreter.h"
#include "upp_common.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Глибше цієї межі — майже напевно нескінченна рекурсія. Без перевірки
   програма кладе стек процесу й падає без пояснень. */
#define UPP_MAX_CALL_DEPTH 200

typedef enum {
    EXEC_NORMAL,
    EXEC_RETURN,
    EXEC_ERROR
} ExecResult;

static ExecResult execute_stmt(Interpreter* it, Stmt* stmt);
static ExecResult execute_list(Interpreter* it, StmtList* list);
static Value      eval_expr(Interpreter* it, Expr* expr);

/* ------------------------------------------------------------------ */
/* Помилки виконання                                                   */
/* ------------------------------------------------------------------ */

static void runtime_error(Interpreter* it, int line, const char* fmt, ...) {
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

    upp_error_at(UPP_STAGE_RUNTIME, line, 0, "%s", msg);
    free(msg);

    it->had_error = 1;
}

/* ------------------------------------------------------------------ */
/* Оточення                                                            */
/* ------------------------------------------------------------------ */

static Environment* env_new(Environment* parent) {
    Environment* env = (Environment*)upp_xmalloc(sizeof(Environment));
    env->parent = parent;
    env->bindings = NULL;
    env->count = 0;
    env->capacity = 0;
    return env;
}

static void env_free(Environment* env) {
    for (int i = 0; i < env->count; i++) {
        free(env->bindings[i].name);
        upp_release(env->bindings[i].val);
    }
    free(env->bindings);
    free(env);
}

/* Оголошує змінну в ПОТОЧНОМУ оточенні (повторне «нехай» перезаписує). */
static void env_define(Environment* env, const char* name, Value val) {
    for (int i = 0; i < env->count; i++) {
        if (strcmp(env->bindings[i].name, name) == 0) {
            upp_release(env->bindings[i].val);
            env->bindings[i].val = upp_retain(val);
            return;
        }
    }

    if (env->count + 1 > env->capacity) {
        env->capacity = env->capacity < 8 ? 8 : env->capacity * 2;
        env->bindings = (VarBinding*)upp_xrealloc(env->bindings, sizeof(VarBinding) * env->capacity);
    }

    env->bindings[env->count].name = upp_xstrdup(name);
    env->bindings[env->count].val = upp_retain(val);
    env->count++;
}

static VarBinding* env_find(Environment* env, const char* name) {
    for (Environment* e = env; e != NULL; e = e->parent) {
        for (int i = 0; i < e->count; i++) {
            if (strcmp(e->bindings[i].name, name) == 0) {
                return &e->bindings[i];
            }
        }
    }
    return NULL;
}

/* Присвоєння шукає найближчу наявну змінну; створювати нову не можна —
   це ловить помилки в іменах. */
static int env_assign(Environment* env, const char* name, Value val) {
    VarBinding* binding = env_find(env, name);
    if (!binding) return 0;

    upp_release(binding->val);
    binding->val = upp_retain(val);
    return 1;
}

/* ------------------------------------------------------------------ */
/* Життєвий цикл                                                       */
/* ------------------------------------------------------------------ */

Interpreter upp_make_interpreter(void) {
    Interpreter it;
    it.globals = env_new(NULL);
    it.env = it.globals;
    it.ret_value = upp_nil();
    it.had_error = 0;
    it.depth = 0;
    return it;
}

void upp_free_interpreter(Interpreter* it) {
    upp_release(it->ret_value);
    it->ret_value = upp_nil();
    if (it->globals) {
        env_free(it->globals);
        it->globals = NULL;
        it->env = NULL;
    }
}

/* ------------------------------------------------------------------ */
/* Арифметика та порівняння                                            */
/* ------------------------------------------------------------------ */

static Value concat_values(Value left, Value right) {
    char* ls = upp_value_to_display(left);
    char* rs = upp_value_to_display(right);

    StrBuf sb;
    upp_sb_init(&sb);
    upp_sb_append(&sb, ls);
    upp_sb_append(&sb, rs);

    free(ls);
    free(rs);

    size_t len = 0;
    char*  joined = upp_sb_take(&sb, &len);
    return upp_string_take(joined, len);
}

static Value concat_lists(Value left, Value right) {
    Value result = upp_list();
    for (int i = 0; i < left.as.list->count; i++) {
        upp_list_append(result.as.list, left.as.list->items[i]);
    }
    for (int i = 0; i < right.as.list->count; i++) {
        upp_list_append(result.as.list, right.as.list->items[i]);
    }
    return result;
}

static int compare_ordered(Interpreter* it, int line, BinOp op, Value left, Value right, int* out) {
    if (upp_is_numeric(left) && upp_is_numeric(right)) {
        if (left.type == VAL_NUMBER && right.type == VAL_NUMBER) {
            long long a = left.as.number, b = right.as.number;
            switch (op) {
                case BIN_GT: *out = a > b;  return 1;
                case BIN_LT: *out = a < b;  return 1;
                case BIN_GE: *out = a >= b; return 1;
                case BIN_LE: *out = a <= b; return 1;
                default: break;
            }
        }
        double a = upp_as_double(left), b = upp_as_double(right);
        switch (op) {
            case BIN_GT: *out = a > b;  return 1;
            case BIN_LT: *out = a < b;  return 1;
            case BIN_GE: *out = a >= b; return 1;
            case BIN_LE: *out = a <= b; return 1;
            default: break;
        }
    }

    if (left.type == VAL_STRING && right.type == VAL_STRING) {
        size_t min_len = left.as.string->len < right.as.string->len
                       ? left.as.string->len : right.as.string->len;
        int cmp = memcmp(left.as.string->chars, right.as.string->chars, min_len);
        if (cmp == 0) {
            cmp = left.as.string->len < right.as.string->len ? -1
                : left.as.string->len > right.as.string->len ? 1 : 0;
        }
        switch (op) {
            case BIN_GT: *out = cmp > 0;  return 1;
            case BIN_LT: *out = cmp < 0;  return 1;
            case BIN_GE: *out = cmp >= 0; return 1;
            case BIN_LE: *out = cmp <= 0; return 1;
            default: break;
        }
    }

    runtime_error(it, line, "Не можна порівняти '%s' і '%s' операцією '%s'",
                  upp_type_name(left), upp_type_name(right), upp_bin_op_name(op));
    return 0;
}

static Value eval_arithmetic(Interpreter* it, int line, BinOp op, Value left, Value right) {
    if (op == BIN_ADD) {
        if (left.type == VAL_STRING || right.type == VAL_STRING) {
            return concat_values(left, right);
        }
        if (left.type == VAL_LIST && right.type == VAL_LIST) {
            return concat_lists(left, right);
        }
    }

    if (!upp_is_numeric(left) || !upp_is_numeric(right)) {
        runtime_error(it, line, "Не можна застосувати '%s' до '%s' і '%s'",
                      upp_bin_op_name(op), upp_type_name(left), upp_type_name(right));
        return upp_nil();
    }

    int both_int = (left.type == VAL_NUMBER && right.type == VAL_NUMBER);

    if (op == BIN_DIV) {
        if (both_int) {
            if (right.as.number == 0) {
                runtime_error(it, line, "Ділення на нуль");
                return upp_nil();
            }
            /* Цілі числа діляться націло, з відкиданням дробової частини. */
            return upp_number(left.as.number / right.as.number);
        }
        double divisor = upp_as_double(right);
        if (divisor == 0.0) {
            runtime_error(it, line, "Ділення на нуль");
            return upp_nil();
        }
        return upp_real(upp_as_double(left) / divisor);
    }

    if (both_int) {
        long long a = left.as.number, b = right.as.number;
        switch (op) {
            case BIN_ADD: return upp_number(a + b);
            case BIN_SUB: return upp_number(a - b);
            case BIN_MUL: return upp_number(a * b);
            default: break;
        }
    }

    double a = upp_as_double(left), b = upp_as_double(right);
    switch (op) {
        case BIN_ADD: return upp_real(a + b);
        case BIN_SUB: return upp_real(a - b);
        case BIN_MUL: return upp_real(a * b);
        default: break;
    }

    runtime_error(it, line, "Невідома операція '%s'", upp_bin_op_name(op));
    return upp_nil();
}

/* ------------------------------------------------------------------ */
/* Вбудовані функції                                                   */
/* ------------------------------------------------------------------ */

static Value builtin_dovzhyna(Interpreter* it, int line, Value* args, int count) {
    if (count != 1) {
        runtime_error(it, line, "'довжина' очікує 1 аргумент, отримано %d", count);
        return upp_nil();
    }
    int ok = 0;
    long long len = upp_value_length(args[0], &ok);
    if (!ok) {
        runtime_error(it, line, "'довжина' працює з рядком або списком, а не з '%s'",
                      upp_type_name(args[0]));
        return upp_nil();
    }
    return upp_number(len);
}

static Value builtin_vvesty(Interpreter* it, int line, Value* args, int count) {
    if (count > 1) {
        runtime_error(it, line, "'ввести' очікує 0 або 1 аргумент, отримано %d", count);
        return upp_nil();
    }

    if (count == 1) {
        char* prompt = upp_value_to_display(args[0]);
        fputs(prompt, stdout);
        fflush(stdout);
        free(prompt);
    }

    StrBuf sb;
    upp_sb_init(&sb);

    int c;
    while ((c = fgetc(stdin)) != EOF && c != '\n') {
        upp_sb_append_char(&sb, (char)c);
    }

    /* Файли з Windows-переносами лишають '\r' у кінці. */
    if (sb.len > 0 && sb.data[sb.len - 1] == '\r') {
        sb.len--;
        sb.data[sb.len] = '\0';
    }

    size_t len = 0;
    char*  text = upp_sb_take(&sb, &len);
    return upp_string_take(text, len);
}

static Value builtin_chyslo(Interpreter* it, int line, Value* args, int count) {
    if (count != 1) {
        runtime_error(it, line, "'число' очікує 1 аргумент, отримано %d", count);
        return upp_nil();
    }

    Value v = args[0];
    if (upp_is_numeric(v)) return v.type == VAL_NUMBER ? upp_number(v.as.number) : upp_real(v.as.real);
    if (v.type == VAL_BOOL) return upp_number(v.as.boolean ? 1 : 0);

    if (v.type != VAL_STRING) {
        runtime_error(it, line, "'число' не вміє перетворювати '%s'", upp_type_name(v));
        return upp_nil();
    }

    const char* text = v.as.string->chars;
    char*       end = NULL;

    if (strchr(text, '.') != NULL) {
        double d = strtod(text, &end);
        if (end == text || *end != '\0') {
            runtime_error(it, line, "Рядок '%s' не є числом", text);
            return upp_nil();
        }
        return upp_real(d);
    }

    long long n = strtoll(text, &end, 10);
    if (end == text || *end != '\0') {
        runtime_error(it, line, "Рядок '%s' не є числом", text);
        return upp_nil();
    }
    return upp_number(n);
}

static Value builtin_tekst(Interpreter* it, int line, Value* args, int count) {
    if (count != 1) {
        runtime_error(it, line, "'текст' очікує 1 аргумент, отримано %d", count);
        return upp_nil();
    }
    size_t len = 0;
    char*  text = upp_value_to_display(args[0]);
    len = strlen(text);
    return upp_string_take(text, len);
}

static Value builtin_dodaty_do(Interpreter* it, int line, Value* args, int count) {
    if (count != 2) {
        runtime_error(it, line, "'додати_до' очікує 2 аргументи, отримано %d", count);
        return upp_nil();
    }
    if (args[0].type != VAL_LIST) {
        runtime_error(it, line, "'додати_до' очікує список першим аргументом, а не '%s'",
                      upp_type_name(args[0]));
        return upp_nil();
    }
    upp_list_append(args[0].as.list, args[1]);
    return upp_retain(args[0]);
}

typedef Value (*BuiltinFn)(Interpreter*, int, Value*, int);

typedef struct {
    const char* name;
    BuiltinFn   fn;
} Builtin;

static const Builtin builtins[] = {
    {"довжина",   builtin_dovzhyna},
    {"ввести",    builtin_vvesty},
    {"число",     builtin_chyslo},
    {"текст",     builtin_tekst},
    {"додати_до", builtin_dodaty_do},
    {NULL, NULL}
};

static BuiltinFn find_builtin(const char* name) {
    for (int i = 0; builtins[i].name != NULL; i++) {
        if (strcmp(name, builtins[i].name) == 0) return builtins[i].fn;
    }
    return NULL;
}

/* ------------------------------------------------------------------ */
/* Виклик функції                                                      */
/* ------------------------------------------------------------------ */

static Value call_function(Interpreter* it, int line, const FuncDecl* decl, Value* args, int arg_count) {
    if (decl->param_count != arg_count) {
        runtime_error(it, line, "Функція '%s' очікує %d %s, отримано %d",
                      decl->name, decl->param_count,
                      upp_plural(decl->param_count, "аргумент", "аргументи", "аргументів"),
                      arg_count);
        return upp_nil();
    }

    if (it->depth >= UPP_MAX_CALL_DEPTH) {
        runtime_error(it, line, "Перевищено глибину рекурсії (%d) у функції '%s'",
                      UPP_MAX_CALL_DEPTH, decl->name);
        return upp_nil();
    }

    /* Тіло функції бачить власні параметри й глобальні змінні, але не
       локальні змінні того, хто її викликав. */
    Environment* frame = env_new(it->globals);
    for (int i = 0; i < arg_count; i++) {
        env_define(frame, decl->params[i], args[i]);
    }

    Environment* saved_env = it->env;
    it->env = frame;
    it->depth++;

    ExecResult result = execute_list(it, (StmtList*)&decl->body);

    it->depth--;
    it->env = saved_env;
    env_free(frame);

    if (result == EXEC_RETURN) {
        Value ret = it->ret_value;
        it->ret_value = upp_nil();
        return ret; /* володіння переходить викликачеві */
    }

    return upp_nil();
}

static Value eval_call(Interpreter* it, Expr* expr) {
    int         line = expr->line;
    const char* name = expr->as.call.name;
    int         arg_count = expr->as.call.arg_count;

    Value* args = arg_count > 0 ? (Value*)upp_xmalloc(sizeof(Value) * arg_count) : NULL;
    int    evaluated = 0;

    for (; evaluated < arg_count; evaluated++) {
        args[evaluated] = eval_expr(it, expr->as.call.args[evaluated]);
        if (it->had_error) {
            evaluated++;
            break;
        }
    }

    Value result = upp_nil();

    if (!it->had_error) {
        VarBinding* binding = env_find(it->env, name);

        if (binding && binding->val.type == VAL_FUNCTION) {
            result = call_function(it, line, binding->val.as.func->decl, args, arg_count);
        } else if (binding) {
            runtime_error(it, line, "'%s' не є функцією (це %s)", name, upp_type_name(binding->val));
        } else {
            BuiltinFn fn = find_builtin(name);
            if (fn) {
                result = fn(it, line, args, arg_count);
            } else {
                runtime_error(it, line, "Невідома функція '%s'", name);
            }
        }
    }

    for (int i = 0; i < evaluated; i++) {
        upp_release(args[i]);
    }
    free(args);

    return result;
}

/* ------------------------------------------------------------------ */
/* Індексація                                                          */
/* ------------------------------------------------------------------ */

/* Повертає зміщення в байтах для i-го символу UTF-8 (або довжину рядка). */
static size_t utf8_offset(const char* s, size_t byte_len, long long char_index) {
    long long seen = -1;
    for (size_t i = 0; i < byte_len; i++) {
        if (!upp_utf8_is_continuation((unsigned char)s[i])) {
            seen++;
            if (seen == char_index) return i;
        }
    }
    return byte_len;
}

static Value eval_index(Interpreter* it, Expr* expr) {
    Value target = eval_expr(it, expr->as.index.target);
    if (it->had_error) {
        upp_release(target);
        return upp_nil();
    }

    Value index = eval_expr(it, expr->as.index.index);
    if (it->had_error) {
        upp_release(target);
        upp_release(index);
        return upp_nil();
    }

    Value result = upp_nil();

    if (index.type != VAL_NUMBER) {
        runtime_error(it, expr->line, "Індекс має бути цілим числом, а не '%s'", upp_type_name(index));
    } else if (target.type == VAL_LIST) {
        long long i = index.as.number;
        if (i < 0 || i >= target.as.list->count) {
            runtime_error(it, expr->line, "Індекс %lld поза межами списку (довжина %d)",
                          i, target.as.list->count);
        } else {
            result = upp_retain(target.as.list->items[i]);
        }
    } else if (target.type == VAL_STRING) {
        long long chars = (long long)upp_utf8_length(target.as.string->chars, target.as.string->len);
        long long i = index.as.number;
        if (i < 0 || i >= chars) {
            runtime_error(it, expr->line, "Індекс %lld поза межами рядка (довжина %lld)", i, chars);
        } else {
            size_t start = utf8_offset(target.as.string->chars, target.as.string->len, i);
            size_t end = utf8_offset(target.as.string->chars, target.as.string->len, i + 1);
            result = upp_string_len(target.as.string->chars + start, end - start);
        }
    } else {
        runtime_error(it, expr->line, "Індексувати можна лише список або рядок, а не '%s'",
                      upp_type_name(target));
    }

    upp_release(target);
    upp_release(index);
    return result;
}

/* ------------------------------------------------------------------ */
/* Вирази                                                              */
/* ------------------------------------------------------------------ */

static Value eval_expr(Interpreter* it, Expr* expr) {
    if (!expr || it->had_error) return upp_nil();

    switch (expr->type) {
        case EXPR_NUMBER: return upp_number(expr->as.number_val);
        case EXPR_REAL:   return upp_real(expr->as.real_val);
        case EXPR_STRING: return upp_string(expr->as.string_val);
        case EXPR_BOOL:   return upp_bool(expr->as.bool_val);

        case EXPR_VARIABLE: {
            VarBinding* binding = env_find(it->env, expr->as.var_name);
            if (!binding) {
                runtime_error(it, expr->line, "Невідома змінна '%s'", expr->as.var_name);
                return upp_nil();
            }
            return upp_retain(binding->val);
        }

        case EXPR_LIST: {
            Value list = upp_list();
            for (int i = 0; i < expr->as.list.count; i++) {
                Value item = eval_expr(it, expr->as.list.items[i]);
                if (it->had_error) {
                    upp_release(item);
                    upp_release(list);
                    return upp_nil();
                }
                upp_list_append(list.as.list, item);
                upp_release(item);
            }
            return list;
        }

        case EXPR_INDEX: return eval_index(it, expr);
        case EXPR_CALL:  return eval_call(it, expr);

        case EXPR_UNARY: {
            Value operand = eval_expr(it, expr->as.unary.expr);
            if (it->had_error) {
                upp_release(operand);
                return upp_nil();
            }

            Value result = upp_nil();
            if (expr->as.unary.op == UN_NOT) {
                result = upp_bool(!upp_truthy(operand));
            } else if (operand.type == VAL_NUMBER) {
                result = upp_number(-operand.as.number);
            } else if (operand.type == VAL_REAL) {
                result = upp_real(-operand.as.real);
            } else {
                runtime_error(it, expr->line, "Не можна взяти '-' від '%s'", upp_type_name(operand));
            }

            upp_release(operand);
            return result;
        }

        case EXPR_BINARY: {
            BinOp op = expr->as.binary.op;

            /* Логічні операції обчислюють правий бік лише за потреби. */
            if (op == BIN_AND || op == BIN_OR) {
                Value left = eval_expr(it, expr->as.binary.left);
                if (it->had_error) {
                    upp_release(left);
                    return upp_nil();
                }

                int left_truthy = upp_truthy(left);
                upp_release(left);

                if (op == BIN_AND && !left_truthy) return upp_bool(0);
                if (op == BIN_OR && left_truthy)   return upp_bool(1);

                Value right = eval_expr(it, expr->as.binary.right);
                if (it->had_error) {
                    upp_release(right);
                    return upp_nil();
                }

                Value result = upp_bool(upp_truthy(right));
                upp_release(right);
                return result;
            }

            Value left = eval_expr(it, expr->as.binary.left);
            if (it->had_error) {
                upp_release(left);
                return upp_nil();
            }

            Value right = eval_expr(it, expr->as.binary.right);
            if (it->had_error) {
                upp_release(left);
                upp_release(right);
                return upp_nil();
            }

            Value result = upp_nil();

            switch (op) {
                case BIN_EQ: result = upp_bool(upp_equals(left, right));  break;
                case BIN_NE: result = upp_bool(!upp_equals(left, right)); break;
                case BIN_GT: case BIN_LT: case BIN_GE: case BIN_LE: {
                    int cmp = 0;
                    if (compare_ordered(it, expr->line, op, left, right, &cmp)) {
                        result = upp_bool(cmp);
                    }
                    break;
                }
                default:
                    result = eval_arithmetic(it, expr->line, op, left, right);
                    break;
            }

            upp_release(left);
            upp_release(right);
            return result;
        }
    }

    return upp_nil();
}

/* ------------------------------------------------------------------ */
/* Інструкції                                                          */
/* ------------------------------------------------------------------ */

static ExecResult execute_index_assign(Interpreter* it, Stmt* stmt) {
    Value target = eval_expr(it, stmt->as.index_assign.target);
    if (it->had_error) {
        upp_release(target);
        return EXEC_ERROR;
    }

    Value index = eval_expr(it, stmt->as.index_assign.index);
    if (it->had_error) {
        upp_release(target);
        upp_release(index);
        return EXEC_ERROR;
    }

    Value value = eval_expr(it, stmt->as.index_assign.value);
    if (it->had_error) {
        upp_release(target);
        upp_release(index);
        upp_release(value);
        return EXEC_ERROR;
    }

    ExecResult result = EXEC_NORMAL;

    if (target.type != VAL_LIST) {
        runtime_error(it, stmt->line, "Присвоювати за індексом можна лише елементу списку, а не '%s'",
                      upp_type_name(target));
        result = EXEC_ERROR;
    } else if (index.type != VAL_NUMBER) {
        runtime_error(it, stmt->line, "Індекс має бути цілим числом, а не '%s'", upp_type_name(index));
        result = EXEC_ERROR;
    } else if (index.as.number < 0 || index.as.number >= target.as.list->count) {
        runtime_error(it, stmt->line, "Індекс %lld поза межами списку (довжина %d)",
                      index.as.number, target.as.list->count);
        result = EXEC_ERROR;
    } else {
        Value* slot = &target.as.list->items[index.as.number];
        upp_release(*slot);
        *slot = upp_retain(value);
    }

    upp_release(target);
    upp_release(index);
    upp_release(value);
    return result;
}

static ExecResult execute_stmt(Interpreter* it, Stmt* stmt) {
    if (!stmt || it->had_error) return it->had_error ? EXEC_ERROR : EXEC_NORMAL;

    switch (stmt->type) {
        case STMT_VAR_DECL: {
            Value val = eval_expr(it, stmt->as.var_decl.expr);
            if (it->had_error) {
                upp_release(val);
                return EXEC_ERROR;
            }
            env_define(it->env, stmt->as.var_decl.name, val);
            upp_release(val);
            return EXEC_NORMAL;
        }

        case STMT_ASSIGN: {
            Value val = eval_expr(it, stmt->as.assign.expr);
            if (it->had_error) {
                upp_release(val);
                return EXEC_ERROR;
            }
            if (!env_assign(it->env, stmt->as.assign.name, val)) {
                runtime_error(it, stmt->line,
                              "Змінна '%s' не оголошена. Спершу напишіть: нехай %s буде ...",
                              stmt->as.assign.name, stmt->as.assign.name);
                upp_release(val);
                return EXEC_ERROR;
            }
            upp_release(val);
            return EXEC_NORMAL;
        }

        case STMT_INDEX_ASSIGN:
            return execute_index_assign(it, stmt);

        case STMT_PRINT: {
            Value val = eval_expr(it, stmt->as.print.expr);
            if (it->had_error) {
                upp_release(val);
                return EXEC_ERROR;
            }
            char* text = upp_value_to_display(val);
            fputs(text, stdout);
            fputc('\n', stdout);
            free(text);
            upp_release(val);
            return EXEC_NORMAL;
        }

        case STMT_EXPR: {
            Value val = eval_expr(it, stmt->as.expr_stmt.expr);
            upp_release(val);
            return it->had_error ? EXEC_ERROR : EXEC_NORMAL;
        }

        case STMT_IF: {
            Value cond = eval_expr(it, stmt->as.if_stmt.condition);
            if (it->had_error) {
                upp_release(cond);
                return EXEC_ERROR;
            }
            int truthy = upp_truthy(cond);
            upp_release(cond);

            if (truthy) return execute_list(it, &stmt->as.if_stmt.then_branch);
            if (stmt->as.if_stmt.has_else) return execute_list(it, &stmt->as.if_stmt.else_branch);
            return EXEC_NORMAL;
        }

        case STMT_WHILE: {
            for (;;) {
                Value cond = eval_expr(it, stmt->as.while_stmt.condition);
                if (it->had_error) {
                    upp_release(cond);
                    return EXEC_ERROR;
                }
                int truthy = upp_truthy(cond);
                upp_release(cond);
                if (!truthy) return EXEC_NORMAL;

                ExecResult result = execute_list(it, &stmt->as.while_stmt.body);
                if (result != EXEC_NORMAL) return result;
            }
        }

        case STMT_FUNC: {
            Value fn = upp_function(&stmt->as.func);
            env_define(it->env, stmt->as.func.name, fn);
            upp_release(fn);
            return EXEC_NORMAL;
        }

        case STMT_RETURN: {
            Value val = stmt->as.ret.expr ? eval_expr(it, stmt->as.ret.expr) : upp_nil();
            if (it->had_error) {
                upp_release(val);
                return EXEC_ERROR;
            }
            upp_release(it->ret_value);
            it->ret_value = val; /* володіння переходить інтерпретатору */
            return EXEC_RETURN;
        }
    }

    return EXEC_NORMAL;
}

static ExecResult execute_list(Interpreter* it, StmtList* list) {
    for (int i = 0; i < list->count; i++) {
        ExecResult result = execute_stmt(it, list->statements[i]);
        if (result != EXEC_NORMAL) return result;
    }
    return EXEC_NORMAL;
}

/* ------------------------------------------------------------------ */
/* Точка входу                                                         */
/* ------------------------------------------------------------------ */

int upp_interpret(Interpreter* it, Program* program) {
    /* Функції видно з будь-якого місця файлу, а не лише нижче за
       оголошенням — інакше взаємна рекурсія неможлива. */
    for (int i = 0; i < program->statements.count; i++) {
        Stmt* stmt = program->statements.statements[i];
        if (stmt->type == STMT_FUNC) {
            Value fn = upp_function(&stmt->as.func);
            env_define(it->globals, stmt->as.func.name, fn);
            upp_release(fn);
        }
    }

    for (int i = 0; i < program->statements.count; i++) {
        Stmt* stmt = program->statements.statements[i];
        if (stmt->type == STMT_FUNC) continue;

        ExecResult result = execute_stmt(it, stmt);
        if (result == EXEC_ERROR) return 1;
        if (result == EXEC_RETURN) break; /* «повернути» на верхньому рівні завершує програму */
    }

    return it->had_error ? 1 : 0;
}
