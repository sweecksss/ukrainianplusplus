from __future__ import annotations

from typing import List

from .ast import (
    BinaryExpr,
    BlockStmt,
    BoolExpr,
    Expr,
    IfStmt,
    NumberExpr,
    PrintStmt,
    Program,
    Stmt,
    StringExpr,
    UnaryExpr,
    VarDecl,
    VariableExpr,
    WhileStmt,
)
from .tokens import Token, TokenType


class ParseError(Exception):
    pass


class Parser:
    def __init__(self, tokens: List[Token]) -> None:
        self.tokens = tokens
        self.pos = 0

    def parse(self) -> Program:
        statements: List[Stmt] = []
        while not self._check(TokenType.EOF):
            if self._match(TokenType.NEWLINE):
                continue
            statements.append(self._statement())
        return Program(statements)

    # ---------------------------
    # Statements

    def _statement(self) -> Stmt:
        if self._check(TokenType.NEHAI):
            return self._var_decl()
        if self._check(TokenType.POKAZATY):
            return self._print_stmt()
        if self._check(TokenType.IF):
            return self._if_stmt()
        if self._check(TokenType.WHILE):
            return self._while_stmt()
        token = self._peek()
        raise ParseError(
            f"Неочікуваний початок інструкції: {token.lexeme!r} "
            f"(рядок {token.line}, позиція {token.column})"
        )

    def _var_decl(self) -> VarDecl:
        self._consume(TokenType.NEHAI, "Очікувалось 'нехай'")
        name_tok = self._consume(TokenType.IDENT, "Очікувалась назва змінної")
        self._consume(TokenType.BUDE, "Очікувалось слово 'буде'")
        expr = self._expression()
        # Необов'язковий NEWLINE у кінці рядка
        self._match(TokenType.NEWLINE)
        return VarDecl(name=name_tok.lexeme, expr=expr)

    def _print_stmt(self) -> PrintStmt:
        self._consume(TokenType.POKAZATY, "Очікувалось 'показати'")
        expr = self._expression()
        self._match(TokenType.NEWLINE)
        return PrintStmt(expr=expr)

    def _if_stmt(self) -> IfStmt:
        self._consume(TokenType.IF, "Очікувалось 'якщо'")
        condition = self._expression()
        # Блок через "то" (рекомендовано) або fallback на одну інструкцію
        if self._match(TokenType.TO):
            then_branch = self._suite()
        else:
            self._match(TokenType.NEWLINE)
            then_branch = BlockStmt([self._statement()])

        # Можливі порожні рядки перед "інакше"
        while self._match(TokenType.NEWLINE):
            pass

        else_branch: BlockStmt | None = None
        if self._match(TokenType.ELSE):
            if self._match(TokenType.TO):
                else_branch = self._suite()
            else:
                self._match(TokenType.NEWLINE)
                else_branch = BlockStmt([self._statement()])

        return IfStmt(condition=condition, then_branch=then_branch, else_branch=else_branch)

    def _while_stmt(self) -> WhileStmt:
        # поки <умова> то\n
        #   <блок>
        self._consume(TokenType.WHILE, "Очікувалось 'поки'")
        condition = self._expression()
        if self._match(TokenType.TO):
            body = self._suite()
        else:
            self._match(TokenType.NEWLINE)
            body = BlockStmt([self._statement()])
        return WhileStmt(condition=condition, body=body)

    def _suite(self) -> BlockStmt:
        # очікуємо NEWLINE INDENT ... DEDENT
        self._match(TokenType.NEWLINE)
        self._consume(TokenType.INDENT, "Очікувався відступ (INDENT) після 'то'")

        statements: List[Stmt] = []
        while not self._check(TokenType.DEDENT) and not self._check(TokenType.EOF):
            if self._match(TokenType.NEWLINE):
                continue
            statements.append(self._statement())

        self._consume(TokenType.DEDENT, "Очікувався кінець блоку (DEDENT)")
        return BlockStmt(statements)

    # ---------------------------
    # Expressions
    #
    # expression      -> or
    # or              -> and ( "або" and )*
    # and             -> comparison ( "і" comparison )*
    # comparison      -> additive ( ( "більше" | "менше" | "дорівнює" ) additive )*
    # additive        -> term ( ( "додати" | "відняти" ) term )*
    # term            -> factor ( ( "помножити" | "поділити" ) factor )*
    # factor          -> "не" factor | NUMBER | STRING | TRUE | FALSE | IDENT

    def _expression(self) -> Expr:
        return self._or()

    def _or(self) -> Expr:
        expr = self._and()
        while self._match(TokenType.OR):
            op_token = self._previous()
            right = self._and()
            expr = BinaryExpr(left=expr, op=op_token.lexeme, right=right)
        return expr

    def _and(self) -> Expr:
        expr = self._comparison()
        while self._match(TokenType.AND):
            op_token = self._previous()
            right = self._comparison()
            expr = BinaryExpr(left=expr, op=op_token.lexeme, right=right)
        return expr

    def _comparison(self) -> Expr:
        expr = self._additive()
        while self._match(TokenType.BILSHE, TokenType.MENSHE, TokenType.DORIVNYUE):
            op_token = self._previous()
            right = self._additive()
            expr = BinaryExpr(left=expr, op=op_token.lexeme, right=right)
        return expr

    def _additive(self) -> Expr:
        expr = self._term()
        while self._match(TokenType.DODATY, TokenType.VIDNIATY):
            op_token = self._previous()
            right = self._term()
            expr = BinaryExpr(left=expr, op=op_token.lexeme, right=right)
        return expr

    def _term(self) -> Expr:
        expr = self._factor()
        while self._match(TokenType.POMNOZHYTY, TokenType.PODILYTY):
            op_token = self._previous()
            right = self._factor()
            expr = BinaryExpr(left=expr, op=op_token.lexeme, right=right)
        return expr

    def _factor(self) -> Expr:
        if self._match(TokenType.NOT):
            op_token = self._previous()
            right = self._factor()
            return UnaryExpr(op=op_token.lexeme, expr=right)
        if self._match(TokenType.NUMBER):
            token = self._previous()
            return NumberExpr(int(token.lexeme))
        if self._match(TokenType.STRING):
            token = self._previous()
            return StringExpr(token.lexeme)
        if self._match(TokenType.TRUE, TokenType.FALSE):
            token = self._previous()
            value = token.type == TokenType.TRUE
            return BoolExpr(value)
        if self._match(TokenType.IDENT):
            token = self._previous()
            return VariableExpr(token.lexeme)
        token = self._peek()
        raise ParseError(
            f"Очікувався вираз, натомість {token.lexeme!r} "
            f"(рядок {token.line}, позиція {token.column})"
        )

    # ---------------------------
    # Helpers

    def _match(self, *types: TokenType) -> bool:
        for t in types:
            if self._check(t):
                self._advance()
                return True
        return False

    def _consume(self, ttype: TokenType, message: str) -> Token:
        if self._check(ttype):
            return self._advance()
        token = self._peek()
        raise ParseError(f"{message}. Знайдено {token.lexeme!r} (рядок {token.line}, позиція {token.column})")

    def _check(self, ttype: TokenType) -> bool:
        if self._is_at_end():
            return ttype == TokenType.EOF
        return self._peek().type == ttype

    def _advance(self) -> Token:
        if not self._is_at_end():
            self.pos += 1
        return self._previous()

    def _previous(self) -> Token:
        return self.tokens[self.pos - 1]

    def _is_at_end(self) -> bool:
        return self.tokens[self.pos].type == TokenType.EOF

    def _peek(self) -> Token:
        return self.tokens[self.pos]

