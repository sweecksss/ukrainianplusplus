#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include "upp_lexer.h"
#include "upp_parser.h"
#include "upp_interpreter.h"

static char* read_file(const char* path) {
    FILE* file = fopen(path, "rb");
    if (!file) {
        fprintf(stderr, "Помилка відкриття файлу: %s\n", path);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char* buffer = (char*)malloc(size + 1);
    if (!buffer) {
        fclose(file);
        return NULL;
    }

    fread(buffer, 1, size, file);
    buffer[size] = '\0';
    fclose(file);
    return buffer;
}

int main(int argc, char* argv[]) {
    // Set console output to UTF-8 on Windows
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    if (argc < 2) {
        printf("Використання: upp <файл.upp>\n");
        return 1;
    }

    const char* filename = argv[1];
    char* source = read_file(filename);
    if (!source) return 1;

    UppTokenArray tokens = upp_tokenize(source);
    Program program = upp_parse(tokens);

    Interpreter interp = upp_make_interpreter();
    upp_interpret(&interp, &program);

    upp_free_interpreter(&interp);
    upp_free_program(&program);
    upp_free_token_array(&tokens);
    free(source);

    return 0;
}
