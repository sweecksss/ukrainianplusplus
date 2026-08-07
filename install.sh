#!/usr/bin/env bash
# Скрипт автоматичного встановлення U++ для Linux та macOS.
#
# Працює у два способи:
#   1) з клону репозиторію:  ./install.sh
#   2) без клону:            curl -fsSL .../install.sh | bash
#
# У другому випадку BASH_SOURCE вказує не на файл, а на поточну теку,
# тому початкові коди доводиться завантажувати окремо — інакше скрипт
# шукав би upp-c/ там, де його немає.
set -euo pipefail

REPO_URL="https://github.com/sweecksss/ukrainianplusplus"
BRANCH="main"
PREFIX="${PREFIX:-/usr/local}"
BINDIR="$PREFIX/bin"

SOURCES="main.c upp_common.c upp_tokens.c upp_lexer.c upp_value.c upp_ast.c upp_parser.c upp_interpreter.c"

echo "=== Встановлення мови U++ (Ukrainian Plus Plus) ==="

# --- Компілятор ---------------------------------------------------

if command -v gcc >/dev/null 2>&1; then
    CC="gcc"
elif command -v clang >/dev/null 2>&1; then
    CC="clang"
elif command -v cc >/dev/null 2>&1; then
    CC="cc"
else
    echo "Помилка: не знайдено C-компілятор (gcc, clang або cc)." >&2
    echo "Встановіть build-essential (Debian/Ubuntu), gcc (Fedora)" >&2
    echo "або Command Line Tools (macOS: xcode-select --install)." >&2
    exit 1
fi

# --- Початкові коди -----------------------------------------------

find_local_sources() {
    # ${BASH_SOURCE[0]} під конвеєром дорівнює "bash", тож перевіряємо
    # не сам шлях, а наявність поряд теки upp-c з потрібними файлами.
    local dir
    dir="$(cd "$(dirname "${BASH_SOURCE[0]:-.}")" 2>/dev/null && pwd || true)"
    if [ -n "$dir" ] && [ -f "$dir/upp-c/main.c" ] && [ -f "$dir/upp-c/upp_value.c" ]; then
        printf '%s' "$dir/upp-c"
    fi
}

SRC_DIR="$(find_local_sources)"
WORKDIR=""

if [ -n "$SRC_DIR" ]; then
    echo "Використовую початкові коди поряд зі скриптом."
else
    echo "Завантажую початкові коди з $REPO_URL ..."

    WORKDIR="$(mktemp -d)"
    trap 'rm -rf "$WORKDIR"' EXIT

    TARBALL="$REPO_URL/archive/refs/heads/$BRANCH.tar.gz"

    if command -v curl >/dev/null 2>&1; then
        curl -fsSL "$TARBALL" -o "$WORKDIR/upp.tar.gz"
    elif command -v wget >/dev/null 2>&1; then
        wget -qO "$WORKDIR/upp.tar.gz" "$TARBALL"
    else
        echo "Помилка: потрібен curl або wget, щоб завантажити початкові коди." >&2
        exit 1
    fi

    tar -xzf "$WORKDIR/upp.tar.gz" -C "$WORKDIR"

    SRC_DIR="$(find "$WORKDIR" -type d -name upp-c -maxdepth 2 | head -n 1)"
    if [ -z "$SRC_DIR" ] || [ ! -f "$SRC_DIR/main.c" ]; then
        echo "Помилка: в архіві не знайдено початкових кодів (upp-c/)." >&2
        exit 1
    fi
fi

# --- Складання ----------------------------------------------------

echo "Складаю інтерпретатор за допомогою $CC ..."
cd "$SRC_DIR"

# shellcheck disable=SC2086
$CC -std=c11 -Wall -Wextra -O2 -o upp $SOURCES

# --- Перевірка ----------------------------------------------------

echo "Перевіряю зібраний інтерпретатор ..."
printf 'показати "U++ працює"\n' > ./_install_check.upp
if ! ./upp ./_install_check.upp > /dev/null; then
    rm -f ./_install_check.upp
    echo "Помилка: зібраний інтерпретатор не пройшов перевірку." >&2
    exit 1
fi
rm -f ./_install_check.upp

# --- Встановлення -------------------------------------------------

echo "Встановлюю у $BINDIR/upp ..."
if [ "$(id -u)" -eq 0 ]; then
    install -d "$BINDIR"
    install -m 755 upp "$BINDIR/upp"
elif command -v sudo >/dev/null 2>&1; then
    sudo install -d "$BINDIR"
    sudo install -m 755 upp "$BINDIR/upp"
else
    echo "Помилка: потрібні права root або sudo, щоб писати в $BINDIR." >&2
    echo "Можна встановити в іншу теку: PREFIX=\$HOME/.local bash install.sh" >&2
    exit 1
fi

echo
echo "Успішно! Мову U++ встановлено: $BINDIR/upp"
echo "Перевірка:  upp --version"
echo "Запуск:     upp програма.upp"

case ":$PATH:" in
    *":$BINDIR:"*) ;;
    *) echo
       echo "Увага: $BINDIR немає у PATH. Додайте рядок у ~/.bashrc або ~/.zshrc:"
       echo "  export PATH=\"$BINDIR:\$PATH\"" ;;
esac
