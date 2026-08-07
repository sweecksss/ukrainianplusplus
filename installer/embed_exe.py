#!/usr/bin/env python3
"""Вшиває upp.exe у upp_bytes.inc.

Інсталятор носить інтерпретатор усередині себе як масив байтів. Якщо не
перегенерувати цей файл після перезбирання інтерпретатора, інсталятор
мовчки роздаватиме стару версію — тому крок вбудовано в build_setup.bat.
"""

from pathlib import Path
import sys

HERE = Path(__file__).resolve().parent
SOURCE = HERE / "upp.exe"
TARGET = HERE / "upp_bytes.inc"

BYTES_PER_LINE = 16


def main() -> int:
    if not SOURCE.exists():
        print(f"Не знайдено {SOURCE}", file=sys.stderr)
        print("Спершу зберіть інтерпретатор: ..\\upp-c\\build.bat", file=sys.stderr)
        return 1

    data = SOURCE.read_bytes()
    lines = [
        ", ".join(f"0x{b:02X}" for b in data[i:i + BYTES_PER_LINE])
        for i in range(0, len(data), BYTES_PER_LINE)
    ]

    # newline="\n" явно: інакше на Windows Python перетворив би кожен
    # перенос на CRLF і файл щоразу «змінювався» б цілком.
    with open(TARGET, "w", encoding="ascii", newline="\n") as handle:
        handle.write(",\n".join(lines) + "\n")
    print(f"{TARGET.name}: {len(data)} байт")
    return 0


if __name__ == "__main__":
    sys.exit(main())
