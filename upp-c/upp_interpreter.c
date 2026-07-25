#include "upp_interpreter.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Value make_val_number(long long n) {
    Value v;
    v.type = VAL_NUMBER;
    v.as.number = n;
    return v;
}

static Value make_val_string(const char* s) {
    Value v;
    v.type = VAL_STRING;
    v.as.string = _strdup(s);
    return v;
}

static Value make_val_bool(int b) {
    Value v;
    v.type = VAL_BOOL;
    v.as.boolean = b != 0;
    return v;
}

static void free_value(Value v) {
    if (v.type == VAL_STRING && v.as.string) {
        free(v.as.string);
    }
}

static Value copy_value(Value v) {
    if (v.type == VAL_STRING) {
        return make_val_string(v.as.string);
    }
    return v;
}

static int is_truthy(Value v) {
    if (v.type == VAL_BOOL) return v.as.boolean;
    if (v.type == VAL_NUMBER) return v.as.number != 0;
    if (v.type == VAL_STRING) return v.as.string && strlen(v.as.string) > 0;
    return 0;
}

Interpreter upp_make_interpreter(void) {
    Interpreter interp;
    interp.env.bindings = NULL;
    interp.env.count = 0;
    interp.env.capacity = 0;
    return interp;
}

void upp_free_interpreter(Interpreter* interp) {
    if (interp->env.bindings) {
        for (int i = 0; i < interp->env.count; i++) {
            free(interp->env.bindings[i].name);
            free_value(interp->env.bindings[i].val);
        }
        free(interp->env.bindings);
    }
}

static void env_set(Environment* env, const char* name, Value val) {
    for (int i = 0; i < env->count; i++) {
        if (strcmp(env->bindings[i].name, name) == 0) {
            free_value(env->bindings[i].val);
            env->bindings[i].val = copy_value(val);
            return;
        }
    }
    if (env->count + 1 > env->capacity) {
        env->capacity = env->capacity < 8 ? 8 : env->capacity * 2;
        env->bindings = (VarBinding*)realloc(env->bindings, sizeof(VarBinding) * env->capacity);
    }
    env->bindings[env->count].name = _strdup(name);
    env->bindings[env->count].val = copy_value(val);
    env->count++;
}

static Value env_get(Environment* env, const char* name) {
    for (int i = 0; i < env->count; i++) {
        if (strcmp(env->bindings[i].name, name) == 0) {
            return copy_value(env->bindings[i].val);
        }
    }
    fprintf(stderr, "Помилка виконання: Невідома змінна '%s'\n", name);
    return make_val_number(0);
}

static Value eval_expr(Interpreter* interp, Expr* expr) {
    if (!expr) return make_val_number(0);

    switch (expr->type) {
        case EXPR_NUMBER:
            return make_val_number(expr->as.number_val);

        case EXPR_STRING: {
            // Simple string interpolation "Text {var}"
            const char* str = expr->as.string_val;
            size_t len = strlen(str);
            char* result = (char*)malloc(len * 4 + 1);
            result[0] = '\0';

            size_t i = 0;
            while (i < len) {
                if (str[i] == '{') {
                    i++;
                    char var_name[128];
                    int vn_len = 0;
                    while (i < len && str[i] != '}') {
                        if (vn_len < 127) var_name[vn_len++] = str[i];
                        i++;
                    }
                    var_name[vn_len] = '\0';
                    if (str[i] == '}') i++;

                    Value v = env_get(&interp->env, var_name);
                    if (v.type == VAL_NUMBER) {
                        char buf[64];
                        sprintf(buf, "%lld", v.as.number);
                        strcat(result, buf);
                    } else if (v.type == VAL_STRING) {
                        strcat(result, v.as.string);
                    } else if (v.type == VAL_BOOL) {
                        strcat(result, v.as.boolean ? "правда" : "брехня");
                    }
                    free_value(v);
                } else {
                    size_t cur_len = strlen(result);
                    result[cur_len] = str[i++];
                    result[cur_len + 1] = '\0';
                }
            }
            Value res_val = make_val_string(result);
            free(result);
            return res_val;
        }

        case EXPR_BOOL:
            return make_val_bool(expr->as.bool_val);

        case EXPR_VARIABLE:
            return env_get(&interp->env, expr->as.var_name);

        case EXPR_UNARY: {
            Value val = eval_expr(interp, expr->as.unary.expr);
            if (strcmp(expr->as.unary.op, "не") == 0) {
                Value res = make_val_bool(!is_truthy(val));
                free_value(val);
                return res;
            }
            return val;
        }

        case EXPR_BINARY: {
            const char* op = expr->as.binary.op;

            if (strcmp(op, "і") == 0) {
                Value left = eval_expr(interp, expr->as.binary.left);
                if (!is_truthy(left)) {
                    free_value(left);
                    return make_val_bool(0);
                }
                free_value(left);
                Value right = eval_expr(interp, expr->as.binary.right);
                Value res = make_val_bool(is_truthy(right));
                free_value(right);
                return res;
            }

            if (strcmp(op, "або") == 0) {
                Value left = eval_expr(interp, expr->as.binary.left);
                if (is_truthy(left)) {
                    free_value(left);
                    return make_val_bool(1);
                }
                free_value(left);
                Value right = eval_expr(interp, expr->as.binary.right);
                Value res = make_val_bool(is_truthy(right));
                free_value(right);
                return res;
            }

            Value left = eval_expr(interp, expr->as.binary.left);
            Value right = eval_expr(interp, expr->as.binary.right);

            if (strcmp(op, "додати") == 0) {
                if (left.type == VAL_NUMBER && right.type == VAL_NUMBER) {
                    Value res = make_val_number(left.as.number + right.as.number);
                    free_value(left); free_value(right);
                    return res;
                }
                if (left.type == VAL_STRING || right.type == VAL_STRING) {
                    char buf[1024];
                    buf[0] = '\0';
                    if (left.type == VAL_STRING) strcat(buf, left.as.string);
                    else sprintf(buf + strlen(buf), "%lld", left.as.number);

                    if (right.type == VAL_STRING) strcat(buf, right.as.string);
                    else sprintf(buf + strlen(buf), "%lld", right.as.number);

                    free_value(left); free_value(right);
                    return make_val_string(buf);
                }
            }

            if (strcmp(op, "відняти") == 0 && left.type == VAL_NUMBER && right.type == VAL_NUMBER) {
                Value res = make_val_number(left.as.number - right.as.number);
                free_value(left); free_value(right);
                return res;
            }

            if (strcmp(op, "помножити") == 0 && left.type == VAL_NUMBER && right.type == VAL_NUMBER) {
                Value res = make_val_number(left.as.number * right.as.number);
                free_value(left); free_value(right);
                return res;
            }

            if (strcmp(op, "поділити") == 0 && left.type == VAL_NUMBER && right.type == VAL_NUMBER) {
                if (right.as.number == 0) {
                    fprintf(stderr, "Помилка виконання: Ділення на нуль\n");
                    free_value(left); free_value(right);
                    return make_val_number(0);
                }
                Value res = make_val_number(left.as.number / right.as.number);
                free_value(left); free_value(right);
                return res;
            }

            if (strcmp(op, "більше") == 0 && left.type == VAL_NUMBER && right.type == VAL_NUMBER) {
                Value res = make_val_bool(left.as.number > right.as.number);
                free_value(left); free_value(right);
                return res;
            }

            if (strcmp(op, "менше") == 0 && left.type == VAL_NUMBER && right.type == VAL_NUMBER) {
                Value res = make_val_bool(left.as.number < right.as.number);
                free_value(left); free_value(right);
                return res;
            }

            if (strcmp(op, "дорівнює") == 0) {
                int eq = 0;
                if (left.type == right.type) {
                    if (left.type == VAL_NUMBER) eq = left.as.number == right.as.number;
                    else if (left.type == VAL_BOOL) eq = left.as.boolean == right.as.boolean;
                    else if (left.type == VAL_STRING) eq = strcmp(left.as.string, right.as.string) == 0;
                }
                free_value(left); free_value(right);
                return make_val_bool(eq);
            }

            free_value(left); free_value(right);
            return make_val_number(0);
        }
    }
    return make_val_number(0);
}

static void execute_stmt(Interpreter* interp, Stmt* stmt) {
    if (!stmt) return;

    switch (stmt->type) {
        case STMT_VAR_DECL: {
            Value val = eval_expr(interp, stmt->as.var_decl.expr);
            env_set(&interp->env, stmt->as.var_decl.name, val);
            free_value(val);
            break;
        }
        case STMT_PRINT: {
            Value val = eval_expr(interp, stmt->as.print.expr);
            if (val.type == VAL_NUMBER) {
                printf("%lld\n", val.as.number);
            } else if (val.type == VAL_STRING) {
                printf("%s\n", val.as.string);
            } else if (val.type == VAL_BOOL) {
                printf("%s\n", val.as.boolean ? "правда" : "брехня");
            }
            free_value(val);
            break;
        }
        case STMT_BLOCK: {
            for (int i = 0; i < stmt->as.block.count; i++) {
                execute_stmt(interp, stmt->as.block.statements[i]);
            }
            break;
        }
        case STMT_IF: {
            Value cond = eval_expr(interp, stmt->as.if_stmt.condition);
            if (is_truthy(cond)) {
                for (int i = 0; i < stmt->as.if_stmt.then_branch.count; i++) {
                    execute_stmt(interp, stmt->as.if_stmt.then_branch.statements[i]);
                }
            } else if (stmt->as.if_stmt.has_else) {
                for (int i = 0; i < stmt->as.if_stmt.else_branch.count; i++) {
                    execute_stmt(interp, stmt->as.if_stmt.else_branch.statements[i]);
                }
            }
            free_value(cond);
            break;
        }
        case STMT_WHILE: {
            while (1) {
                Value cond = eval_expr(interp, stmt->as.while_stmt.condition);
                int truth = is_truthy(cond);
                free_value(cond);
                if (!truth) break;

                for (int i = 0; i < stmt->as.while_stmt.body.count; i++) {
                    execute_stmt(interp, stmt->as.while_stmt.body.statements[i]);
                }
            }
            break;
        }
    }
}

void upp_interpret(Interpreter* interp, Program* program) {
    for (int i = 0; i < program->statements.count; i++) {
        execute_stmt(interp, program->statements.statements[i]);
    }
}
