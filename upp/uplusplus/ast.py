from __future__ import annotations

from dataclasses import dataclass
from typing import List, Optional, Protocol, runtime_checkable


@runtime_checkable
class Node(Protocol):
    pass


@dataclass
class Program(Node):
    statements: List["Stmt"]


@runtime_checkable
class Stmt(Node, Protocol):
    pass


@runtime_checkable
class Expr(Node, Protocol):
    pass


@dataclass
class VarDecl(Stmt):
    name: str
    expr: Expr


@dataclass
class PrintStmt(Stmt):
    expr: Expr


@dataclass
class BlockStmt(Stmt):
    statements: List["Stmt"]


@dataclass
class NumberExpr(Expr):
    value: int


@dataclass
class VariableExpr(Expr):
    name: str


@dataclass
class BinaryExpr(Expr):
    left: Expr
    op: str  # арифметичні та порівняння ("додати", "більше", ...)
    right: Expr


@dataclass
class StringExpr(Expr):
    value: str


@dataclass
class BoolExpr(Expr):
    value: bool


@dataclass
class UnaryExpr(Expr):
    op: str  # "не"
    expr: Expr


@dataclass
class IfStmt(Stmt):
    condition: Expr
    then_branch: BlockStmt
    else_branch: Optional[BlockStmt] = None


@dataclass
class WhileStmt(Stmt):
    condition: Expr
    body: BlockStmt

