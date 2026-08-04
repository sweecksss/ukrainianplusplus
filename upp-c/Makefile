# Складання інтерпретатора U++ на Linux, macOS та в MinGW/MSYS.
# Для MSVC у Windows є build.bat.

CC      ?= cc
CFLAGS  ?= -std=c11 -Wall -Wextra -Wpedantic -O2
LDFLAGS ?=

BIN     := upp
SRC     := main.c upp_common.c upp_tokens.c upp_lexer.c upp_value.c \
           upp_ast.c upp_parser.c upp_interpreter.c
OBJ     := $(SRC:.c=.o)
DEPS    := upp_common.h upp_tokens.h upp_lexer.h upp_value.h upp_ast.h \
           upp_parser.h upp_interpreter.h

.PHONY: all clean test debug install

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ) $(LDFLAGS)

%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c $< -o $@

# Складання з санітайзерами — саме так ловляться помилки роботи з памʼяттю.
debug: CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -g -O0 -fsanitize=address,undefined
debug: LDFLAGS := -fsanitize=address,undefined
debug: clean $(BIN)

test: $(BIN)
	python3 ../tests/run_tests.py --exe ./$(BIN)

clean:
	rm -f $(OBJ) $(BIN)
