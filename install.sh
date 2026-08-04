#!/usr/bin/env bash
# Скрипт автоматичного встановлення U++ для Linux та macOS
set -e

echo "=== Встановлення мови U++ (Ukrainian Plus Plus) ==="

# Перевірка наявності компілятора
if command -v gcc >/dev/null 2>&1; then
    CC="gcc"
elif command -v clang >/dev/null 2>&1; then
    CC="clang"
elif command -v cc >/dev/null 2>&1; then
    CC="cc"
else
    echo "Помилка: не знайдено C-компілятор (gcc, clang або cc). Будь ласка, встановіть build-essential або clang." >&2
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
UPP_C_DIR="$SCRIPT_DIR/upp-c"

echo "Складання інтерпретатора за допомогою $CC..."
cd "$UPP_C_DIR"
$CC -std=c11 -Wall -Wextra -O2 -o upp \
    main.c upp_common.c upp_tokens.c upp_lexer.c upp_value.c \
    upp_ast.c upp_parser.c upp_interpreter.c

echo "Встановлення в /usr/local/bin/upp..."
if [ "$(id -u)" -eq 0 ]; then
    install -d /usr/local/bin
    install -m 755 upp /usr/local/bin/upp
else
    sudo install -d /usr/local/bin
    sudo install -m 755 upp /usr/local/bin/upp
fi

echo "Успішно! Мову U++ встановлено."
echo "Запуск: upp програма.upp"
