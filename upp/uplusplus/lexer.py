from __future__ import annotations

from dataclasses import dataclass
from typing import List

from .tokens import KEYWORDS, Token, TokenType


@dataclass
class LexerError(Exception):
    message: str
    line: int
    column: int

    def __str__(self) -> str:
        return f"Лексична помилка на рядку {self.line}, позиція {self.column}: {self.message}"


class Lexer:
    def __init__(self, source: str) -> None:
        self.source = source
        self.pos = 0
        self.line = 1
        self.col = 1
        self.indents: List[int] = [0]
        self.at_line_start = True

    def tokenize(self) -> List[Token]:
        tokens: List[Token] = []

        while not self._is_at_end():
            if self.at_line_start:
                self._handle_indentation(tokens)

            ch = self._peek()

            # Коментарі: все після "#" до кінця рядка ігнорується
            if ch == "#":
                self._skip_comment()
                continue

            if ch in (" ", "\t", "\r"):
                self._advance()
                continue

            if ch == "\n":
                tokens.append(self._make_token(TokenType.NEWLINE, "\n"))
                self._advance_line()
                self.at_line_start = True
                continue

            if ch == '"':
                tokens.append(self._string())
                continue

            if ch.isdigit():
                tokens.append(self._number())
                continue

            # Простий варіант: ідентифікатори та ключові слова = послідовність літер підкреслення та цифр (крім першого)
            if ch.isalpha() or ch == "_":
                tokens.append(self._identifier_or_keyword())
                continue

            raise LexerError(f"Невідомий символ: {ch!r}", self.line, self.col)

        # Закриваємо всі відступи в кінці файлу
        while len(self.indents) > 1:
            self.indents.pop()
            tokens.append(self._make_token(TokenType.DEDENT, ""))

        tokens.append(self._make_token(TokenType.EOF, ""))
        return tokens

    def _is_at_end(self) -> bool:
        return self.pos >= len(self.source)

    def _peek(self) -> str:
        return self.source[self.pos]

    def _advance(self) -> str:
        ch = self.source[self.pos]
        self.pos += 1
        self.col += 1
        return ch

    def _advance_line(self) -> None:
        self.pos += 1
        self.line += 1
        self.col = 1

    def _handle_indentation(self, tokens: List[Token]) -> None:
        # Обробка відступів на початку рядка (Python-подібна)
        # Порожні рядки та рядки-коментарі не впливають на відступи.
        self.at_line_start = False

        start_pos = self.pos
        indent = 0

        while not self._is_at_end():
            ch = self._peek()
            if ch == " ":
                indent += 1
                self._advance()
                continue
            if ch == "\t":
                # Таб = 4 пробіли (просте правило)
                indent += 4
                self._advance()
                continue
            if ch == "\r":
                self._advance()
                continue
            break

        # Якщо рядок порожній або тільки коментар — не змінюємо відступ
        if self._is_at_end():
            return
        nxt = self._peek()
        if nxt == "\n":
            # Відкотимося: пробіли на порожньому рядку не важливі
            self.pos = start_pos
            self.col = 1
            self.at_line_start = True
            return
        if nxt == "#":
            # Коментар-рядок: також не впливає
            self.pos = start_pos
            self.col = 1
            self.at_line_start = True
            return

        current = self.indents[-1]
        if indent > current:
            self.indents.append(indent)
            tokens.append(Token(TokenType.INDENT, "", self.line, 1))
        elif indent < current:
            while len(self.indents) > 1 and indent < self.indents[-1]:
                self.indents.pop()
                tokens.append(Token(TokenType.DEDENT, "", self.line, 1))
            if indent != self.indents[-1]:
                raise LexerError("Неправильний відступ", self.line, 1)

    def _make_token(self, ttype: TokenType, lexeme: str) -> Token:
        # Для простоти: початкова колонка = поточна колонка - (len(lexeme) - 1)
        start_col = max(1, self.col - max(len(lexeme) - 1, 0))
        return Token(ttype, lexeme, self.line, start_col)

    def _string(self) -> Token:
        # Початкові лапки вже на поточній позиції
        start_col = self.col
        self._advance()  # пропустити початкові "
        start_pos = self.pos

        while not self._is_at_end() and self._peek() != '"':
            if self._peek() == "\n":
                raise LexerError("Незавершений рядковий літерал", self.line, self.col)
            self._advance()

        if self._is_at_end():
            raise LexerError("Незавершений рядковий літерал", self.line, self.col)

        # self._peek() == '"'
        lexeme = self.source[start_pos:self.pos]
        self._advance()  # закриваючі "
        return Token(TokenType.STRING, lexeme, self.line, start_col)

    def _number(self) -> Token:
        start_pos = self.pos
        start_col = self.col
        while not self._is_at_end() and self._peek().isdigit():
            self._advance()
        lexeme = self.source[start_pos:self.pos]
        return Token(TokenType.NUMBER, lexeme, self.line, start_col)

    def _identifier_or_keyword(self) -> Token:
        start_pos = self.pos
        start_col = self.col
        while not self._is_at_end() and (self._peek().isalnum() or self._peek() == "_"):
            self._advance()
        lexeme = self.source[start_pos:self.pos]
        ttype = KEYWORDS.get(lexeme, TokenType.IDENT)
        return Token(ttype, lexeme, self.line, start_col)

    def _skip_comment(self) -> None:
        # Пропустити всі символи до кінця рядка (але не споживати сам "\n")
        while not self._is_at_end() and self._peek() != "\n":
            self._advance()

