from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any, Dict
import re

from . import ast


class RuntimeErrorUPlusPlus(Exception):
    pass


@dataclass
class Environment:
    values: Dict[str, Any] = field(default_factory=dict)

    def define(self, name: str, value: Any) -> None:
        self.values[name] = value

    def get(self, name: str) -> Any:
        if name not in self.values:
            raise RuntimeErrorUPlusPlus(f"Невідома змінна '{name}'")
        return self.values[name]


class Interpreter:
    def __init__(self) -> None:
        self.env = Environment()

    def run(self, program: ast.Program) -> None:
        for stmt in program.statements:
            self._execute(stmt)

    # ---------------------------
    # Statements

    def _execute(self, stmt: ast.Stmt) -> None:
        if isinstance(stmt, ast.VarDecl):
            value = self._eval_expr(stmt.expr)
            self.env.define(stmt.name, value)
        elif isinstance(stmt, ast.PrintStmt):
            value = self._eval_expr(stmt.expr)
            print(value)
        elif isinstance(stmt, ast.BlockStmt):
            self._execute_block(stmt)
        elif isinstance(stmt, ast.IfStmt):
            self._execute_if(stmt)
        elif isinstance(stmt, ast.WhileStmt):
            self._execute_while(stmt)
        else:
            raise RuntimeErrorUPlusPlus(f"Невідома інструкція: {stmt!r}")

    # ---------------------------
    # Expressions

    def _eval_expr(self, expr: ast.Expr) -> Any:
        if isinstance(expr, ast.NumberExpr):
            return expr.value
        if isinstance(expr, ast.StringExpr):
            # Підтримка простих шаблонів "Текст {імʼя}"
            value = expr.value

            def replace(match: re.Match[str]) -> str:
                name = match.group(1).strip()
                try:
                    v = self.env.get(name)
                except RuntimeErrorUPlusPlus:
                    # Якщо змінної немає — залишаємо як є
                    return match.group(0)
                return str(v)

            return re.sub(r"\{([^}]+)\}", replace, value)
        if isinstance(expr, ast.BoolExpr):
            return expr.value
        if isinstance(expr, ast.VariableExpr):
            return self.env.get(expr.name)
        if isinstance(expr, ast.BinaryExpr):
            # Логічні оператори з коротким замиканням
            if expr.op == "і":
                left = bool(self._eval_expr(expr.left))
                if not left:
                    return False
                return bool(self._eval_expr(expr.right))
            if expr.op == "або":
                left = bool(self._eval_expr(expr.left))
                if left:
                    return True
                return bool(self._eval_expr(expr.right))

            left = self._eval_expr(expr.left)
            right = self._eval_expr(expr.right)
            if expr.op == "додати":
                return left + right
            if expr.op == "відняти":
                return left - right
            if expr.op == "помножити":
                return left * right
            if expr.op == "поділити":
                if right == 0:
                    raise RuntimeErrorUPlusPlus("Ділення на нуль")
                return left // right
            if expr.op == "більше":
                return left > right
            if expr.op == "менше":
                return left < right
            if expr.op == "дорівнює":
                return left == right
            raise RuntimeErrorUPlusPlus(f"Невідома операція '{expr.op}'")
        if isinstance(expr, ast.UnaryExpr):
            value = self._eval_expr(expr.expr)
            if expr.op == "не":
                return not bool(value)
            raise RuntimeErrorUPlusPlus(f"Невідома унарна операція '{expr.op}'")
        raise RuntimeErrorUPlusPlus(f"Невідомий вираз: {expr!r}")

    # ---------------------------
    # Additional statement handlers

    def _execute_if(self, stmt: ast.IfStmt) -> None:
        condition_value = self._eval_expr(stmt.condition)
        if bool(condition_value):
            self._execute_block(stmt.then_branch)
        elif stmt.else_branch is not None:
            self._execute_block(stmt.else_branch)

    def _execute_while(self, stmt: ast.WhileStmt) -> None:
        # Проста реалізація: поки умова істинна — виконуємо блок тіла
        while bool(self._eval_expr(stmt.condition)):
            self._execute_block(stmt.body)

    def _execute_block(self, block: ast.BlockStmt) -> None:
        for s in block.statements:
            self._execute(s)

