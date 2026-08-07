#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#  include <windows.h>
#endif

#include "upp_common.h"
#include "upp_interpreter.h"
#include "upp_lexer.h"
#include "upp_parser.h"

#define UPP_VERSION "0.2.0"

static char* read_file(const char* path) {
    FILE* file = fopen(path, "rb");
    if (!file) {
        upp_error(UPP_STAGE_RUNTIME, "Не вдалося відкрити файл '%s'", path);
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        upp_error(UPP_STAGE_RUNTIME, "Не вдалося прочитати файл '%s'", path);
        fclose(file);
        return NULL;
    }

    long size = ftell(file);
    if (size < 0) {
        upp_error(UPP_STAGE_RUNTIME, "Не вдалося визначити розмір файлу '%s'", path);
        fclose(file);
        return NULL;
    }
    rewind(file);

    char*  buffer = (char*)upp_xmalloc((size_t)size + 1);
    size_t read = fread(buffer, 1, (size_t)size, file);
    fclose(file);

    /* Прочитати менше, ніж очікували, — нормально для текстових файлів,
       але буфер усе одно має завершуватись нулем саме на цій позиції. */
    buffer[read] = '\0';
    return buffer;
}

static void print_version(void) {
    printf("U++ (Ukrainian Plus Plus) %s\n", UPP_VERSION);
}

static void print_usage(void) {
    print_version();
    printf("\n");
    printf("Використання:\n");
    printf("  upp <файл.upp>     виконати програму\n");
    printf("  upp --version      показати версію\n");
    printf("  upp --help         показати цю довідку\n");
}

int main(int argc, char* argv[]) {
#ifdef _WIN32
    /* Без цього українські літери в консолі Windows перетворюються на
       нечитабельні символи. */
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    if (argc < 2) {
        print_usage();
        return 1;
    }

    const char* arg = argv[1];

    if (strcmp(arg, "--version") == 0 || strcmp(arg, "-v") == 0) {
        print_version();
        return 0;
    }

    if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
        print_usage();
        return 0;
    }

    /* Раніше будь-який аргумент із дефісом мовчки друкував довідку й
       повертав 0, тож одруківка у прапорці виглядала як успіх. */
    if (arg[0] == '-') {
        fprintf(stderr, "Невідомий аргумент '%s'\n\n", arg);
        print_usage();
        return 1;
    }

    char* source = read_file(argv[1]);
    if (!source) return 1;

    UppTokenArray tokens = upp_tokenize(source);

    int status = 0;
    Program program;
    upp_stmt_list_init(&program.statements);

    if (upp_error_count() > 0) {
        /* Розбирати текст із лексичними помилками немає сенсу:
           повідомлення парсера будуть похідними й лише заплутають. */
        status = 1;
    } else {
        program = upp_parse(tokens);

        if (upp_error_count() > 0) {
            status = 1;
        } else {
            Interpreter interp = upp_make_interpreter();
            status = upp_interpret(&interp, &program);
            upp_free_interpreter(&interp);
        }
    }

    upp_free_program(&program);
    upp_free_token_array(&tokens);
    free(source);

    return status;
}
