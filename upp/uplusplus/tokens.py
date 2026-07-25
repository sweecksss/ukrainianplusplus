from __future__ import annotations

from dataclasses import dataclass
from enum import Enum, auto


class TokenType(Enum):
    # Спеціальні
    EOF = auto()
    NEWLINE = auto()
    INDENT = auto()
    DEDENT = auto()

    # Базові
    NUMBER = auto()
    IDENT = auto()
    STRING = auto()

    # Ключові слова
    NEHAI = auto()  # "нехай"
    BUDE = auto()  # "буде"
    POKAZATY = auto()  # "показати"
    TRUE = auto()  # "правда"
    FALSE = auto()  # "брехня"
    IF = auto()  # "якщо"
    ELSE = auto()  # "інакше"
    WHILE = auto()  # "поки"
    TO = auto()  # "то"

    # Логічні оператори
    AND = auto()  # "і"
    OR = auto()  # "або"
    NOT = auto()  # "не"

    # Операції
    DODATY = auto()  # "додати"
    VIDNIATY = auto()  # "відняти"
    POMNOZHYTY = auto()  # "помножити"
    PODILYTY = auto()  # "поділити"
    BILSHE = auto()  # "більше"
    MENSHE = auto()  # "менше"
    DORIVNYUE = auto()  # "дорівнює"


KEYWORDS = {
    "нехай": TokenType.NEHAI,
    "буде": TokenType.BUDE,
    "показати": TokenType.POKAZATY,
    "правда": TokenType.TRUE,
    "брехня": TokenType.FALSE,
    "якщо": TokenType.IF,
    "інакше": TokenType.ELSE,
    "поки": TokenType.WHILE,
    "то": TokenType.TO,
    "і": TokenType.AND,
    "або": TokenType.OR,
    "не": TokenType.NOT,
    "додати": TokenType.DODATY,
    "відняти": TokenType.VIDNIATY,
    "помножити": TokenType.POMNOZHYTY,
    "поділити": TokenType.PODILYTY,
    "більше": TokenType.BILSHE,
    "менше": TokenType.MENSHE,
    "дорівнює": TokenType.DORIVNYUE,
}


@dataclass
class Token:
    type: TokenType
    lexeme: str
    line: int
    column: int

    def __repr__(self) -> str:
        return f"Token({self.type}, {self.lexeme!r}, line={self.line}, col={self.column})"

