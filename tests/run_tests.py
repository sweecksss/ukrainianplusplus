#!/usr/bin/env python3
"""Запуск золотих тестів U++.

Кожен тест — це пара файлів у теці tests/:

    NNN-назва.upp        програма
    NNN-назва.expected   очікуваний результат
    NNN-назва.stdin      (необовʼязково) дані для «ввести»

Формат .expected:

    ===exit===
    0
    ===stdout===
    ...очікуваний вивід...
    ===stderr===
    ...очікувані повідомлення про помилки...

Секції stdout і stderr можна пропускати, якщо вони порожні.

Використання:
    python tests/run_tests.py                    # запустити всі тести
    python tests/run_tests.py --exe path/upp.exe # інший інтерпретатор
    python tests/run_tests.py --update           # перезаписати .expected
    python tests/run_tests.py 012                # лише тести з «012» у назві
"""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path

TESTS_DIR = Path(__file__).resolve().parent
ROOT = TESTS_DIR.parent

EXIT_MARK = "===exit==="
OUT_MARK = "===stdout==="
ERR_MARK = "===stderr==="


def find_interpreter() -> Path | None:
    candidates = [
        ROOT / "upp-c" / "upp.exe",
        ROOT / "upp-c" / "upp",
        ROOT / "upp.exe",
        ROOT / "upp",
    ]
    for path in candidates:
        if path.exists():
            return path
    return None


def parse_expected(text: str) -> tuple[int, str, str]:
    exit_code = 0
    stdout: list[str] = []
    stderr: list[str] = []
    section = None

    for line in text.splitlines():
        if line == EXIT_MARK:
            section = "exit"
            continue
        if line == OUT_MARK:
            section = "stdout"
            continue
        if line == ERR_MARK:
            section = "stderr"
            continue

        if section == "exit":
            if line.strip():
                exit_code = int(line.strip())
        elif section == "stdout":
            stdout.append(line)
        elif section == "stderr":
            stderr.append(line)

    return exit_code, "\n".join(stdout), "\n".join(stderr)


def format_expected(exit_code: int, stdout: str, stderr: str) -> str:
    parts = [EXIT_MARK, str(exit_code)]
    if stdout:
        parts.append(OUT_MARK)
        parts.append(stdout)
    if stderr:
        parts.append(ERR_MARK)
        parts.append(stderr)
    return "\n".join(parts) + "\n"


def run_case(exe: Path, upp_file: Path) -> tuple[int, str, str]:
    stdin_file = upp_file.with_suffix(".stdin")
    stdin_data = stdin_file.read_bytes() if stdin_file.exists() else b""

    proc = subprocess.run(
        [str(exe), str(upp_file)],
        input=stdin_data,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        timeout=20,
    )

    stdout = proc.stdout.decode("utf-8", errors="replace").replace("\r\n", "\n").rstrip("\n")
    stderr = proc.stderr.decode("utf-8", errors="replace").replace("\r\n", "\n").rstrip("\n")
    return proc.returncode, stdout, stderr


# Поведінка самих аргументів командного рядка золотими файлами не
# покривається, а помилятися там легко: колись будь-який прапорець
# мовчки друкував довідку й повертав 0.
CLI_CHECKS = [
    (["--version"], 0, "U++ (Ukrainian Plus Plus)", None),
    (["-v"], 0, "U++ (Ukrainian Plus Plus)", None),
    (["--help"], 0, "Використання:", None),
    (["-h"], 0, "Використання:", None),
    ([], 1, "Використання:", None),
    (["--невідомий"], 1, None, "Невідомий аргумент"),
    (["нема-такого-файлу.upp"], 1, None, "Не вдалося відкрити файл"),
]


def run_cli_checks(exe: Path) -> tuple[int, list[str]]:
    passed = 0
    failed: list[str] = []

    for argv, want_code, want_out, want_err in CLI_CHECKS:
        label = "upp " + (" ".join(argv) if argv else "(без аргументів)")
        proc = subprocess.run([str(exe), *argv], capture_output=True, timeout=20)

        out = proc.stdout.decode("utf-8", errors="replace")
        err = proc.stderr.decode("utf-8", errors="replace")

        problems = []
        if proc.returncode != want_code:
            problems.append(f"код виходу: очікувалось {want_code}, отримано {proc.returncode}")
        if want_out and want_out not in out:
            problems.append(f"у виводі немає {want_out!r}")
        if want_err and want_err not in err:
            problems.append(f"у помилках немає {want_err!r}")

        if problems:
            failed.append(label)
            print(f"ПРОВАЛ  {label}")
            for problem in problems:
                print(f"  {problem}")
        else:
            passed += 1
            print(f"ОК      {label}")

    return passed, failed


def diff_block(title: str, expected: str, actual: str) -> list[str]:
    if expected == actual:
        return []
    lines = [f"  {title}:"]
    lines.append("    очікувалось:")
    lines.extend(f"      | {line}" for line in (expected.splitlines() or ["<порожньо>"]))
    lines.append("    отримано:")
    lines.extend(f"      | {line}" for line in (actual.splitlines() or ["<порожньо>"]))
    return lines


def main() -> int:
    parser = argparse.ArgumentParser(description="Золоті тести U++")
    parser.add_argument("filter", nargs="?", default="", help="підрядок у назві тесту")
    parser.add_argument("--exe", help="шлях до інтерпретатора")
    parser.add_argument("--update", action="store_true", help="перезаписати .expected")
    args = parser.parse_args()

    exe = Path(args.exe) if args.exe else find_interpreter()
    if not exe or not exe.exists():
        print("Не знайдено інтерпретатор. Зберіть його: upp-c/build.bat або make -C upp-c",
              file=sys.stderr)
        return 2

    cases = sorted(p for p in TESTS_DIR.glob("*.upp") if args.filter in p.name)
    if not cases:
        print("Тестів не знайдено", file=sys.stderr)
        return 2

    passed = 0
    failed: list[str] = []

    for case in cases:
        expected_file = case.with_suffix(".expected")
        code, out, err = run_case(exe, case)

        if args.update:
            expected_file.write_text(format_expected(code, out, err), encoding="utf-8")
            print(f"ОНОВЛЕНО {case.name}")
            passed += 1
            continue

        if not expected_file.exists():
            failed.append(case.name)
            print(f"НЕМАЄ ОЧІКУВАНОГО  {case.name}")
            continue

        want_code, want_out, want_err = parse_expected(
            expected_file.read_text(encoding="utf-8")
        )

        problems: list[str] = []
        if code != want_code:
            problems.append(f"  код виходу: очікувалось {want_code}, отримано {code}")
        problems.extend(diff_block("вивід", want_out, out))
        problems.extend(diff_block("помилки", want_err, err))

        if problems:
            failed.append(case.name)
            print(f"ПРОВАЛ  {case.name}")
            print("\n".join(problems))
        else:
            passed += 1
            print(f"ОК      {case.name}")

    # Прапорці командного рядка перевіряємо лише під час повного
    # прогону: під час --update і фільтрації вони ні до чого.
    if not args.update and not args.filter:
        print()
        cli_passed, cli_failed = run_cli_checks(exe)
        passed += cli_passed
        failed.extend(cli_failed)

    print()
    print(f"Пройдено: {passed}, провалено: {len(failed)}")
    if failed:
        print("Провалені: " + ", ".join(failed))
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
