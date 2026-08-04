#!/usr/bin/env python3
"""Запуск програм U++.

Обгортка над інтерпретатором upp: шукає зібраний бінарник і передає
йому файл. Саму мову реалізовано на C у теці upp-c/.

    python main.py програма.upp
"""

import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.abspath(__file__))

# Порядок важливий: свіжозібраний бінарник у upp-c/ має перевагу над
# тим, що лежить у корені після встановлення.
CANDIDATES = [
    os.path.join(ROOT, "upp-c", "upp.exe"),
    os.path.join(ROOT, "upp-c", "upp"),
    os.path.join(ROOT, "upp.exe"),
    os.path.join(ROOT, "upp"),
]


def find_interpreter():
    for path in CANDIDATES:
        if os.path.isfile(path):
            return path
    return None


def main(argv):
    if not argv:
        print("Використання: python main.py <файл.upp>", file=sys.stderr)
        return 1

    interpreter = find_interpreter()
    if interpreter is None:
        print("Інтерпретатор U++ не знайдено.", file=sys.stderr)
        print("Зберіть його:", file=sys.stderr)
        print("    Windows: upp-c\\build.bat", file=sys.stderr)
        print("    Linux/macOS: make -C upp-c", file=sys.stderr)
        return 1

    # Зайві аргументи не вигадуємо: передаємо шлях до програми як є.
    return subprocess.run([interpreter, argv[-1]]).returncode


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
