from __future__ import annotations

import argparse
import sys
from pathlib import Path

from .interpreter import Interpreter, RuntimeErrorUPlusPlus
from .lexer import Lexer, LexerError
from .parser import ParseError, Parser


def run_file(path: Path) -> int:
    try:
        source = path.read_text(encoding="utf-8")
    except OSError as e:
        print(f"Помилка читання файлу {path}: {e}", file=sys.stderr)
        return 1

    try:
        lexer = Lexer(source)
        tokens = lexer.tokenize()

        parser = Parser(tokens)
        program = parser.parse()

        interpreter = Interpreter()
        interpreter.run(program)
        return 0

    except (LexerError, ParseError, RuntimeErrorUPlusPlus) as e:
        print(e, file=sys.stderr)
        return 1


def build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="uplusplus",
        description="u++ (Ukrainian Plus Plus) — маленька інтерпретована мова з українським синтаксисом.",
    )
    subparsers = parser.add_subparsers(dest="command", required=True)

    run_parser = subparsers.add_parser("run", help="запустити .upp програму")
    run_parser.add_argument("file", type=str, help="шлях до .upp файлу")

    return parser


def main(argv: list[str] | None = None) -> int:
    if argv is None:
        argv = sys.argv[1:]

    parser = build_arg_parser()
    args = parser.parse_args(argv)

    if args.command == "run":
        path = Path(args.file)
        return run_file(path)

    print("Невідома команда", file=sys.stderr)
    return 1


if __name__ == "__main__":
    raise SystemExit(main())

