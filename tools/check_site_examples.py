#!/usr/bin/env python3
"""Перевіряє, що всі приклади коду на сайті справді виконуються.

Документація застаріває тихо: приклад лишається в HTML, а мова вже інша.
Цей скрипт витягає блоки <pre><code> з index.html і запускає кожен, який
є програмою на U++ (а не командою оболонки чи зразком виводу).

    python tools/check_site_examples.py
"""

from __future__ import annotations

import html as html_mod
import re
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PAGE = ROOT / "index.html"

CODE_BLOCK = re.compile(r"<pre><code([^>]*)>(.*?)</code></pre>", re.DOTALL)

# Блок, позначений data-expect="error", має завершитись помилкою — це
# приклад із розділу про помилки, і мовчазний успіх тут був би багом.
EXPECT_ERROR = 'data-expect="error"'

# Ключові слова, за якими блок вважається програмою на U++.
UPP_MARKERS = ("нехай ", "показати", "функція ", "поки ", "якщо ", "додати_до(")

# Приклади, які навмисно не є програмами.
SHELL_MARKERS = ("upp ", "echo ", "python ", "make ", "\\build.bat")


def find_interpreter() -> Path | None:
    for path in (ROOT / "upp-c" / "upp.exe", ROOT / "upp-c" / "upp",
                 ROOT / "upp.exe", ROOT / "upp"):
        if path.exists():
            return path
    return None


def strip_tags(text: str) -> str:
    return re.sub(r"<[^>]+>", "", text)


def main() -> int:
    exe = find_interpreter()
    if not exe:
        print("Не знайдено інтерпретатор", file=sys.stderr)
        return 2

    blocks = CODE_BLOCK.findall(PAGE.read_text(encoding="utf-8"))
    checked = 0
    failed: list[tuple[int, str, str]] = []

    for number, (attrs, raw) in enumerate(blocks, start=1):
        code = html_mod.unescape(strip_tags(raw)).strip()

        if not any(marker in code for marker in UPP_MARKERS):
            continue
        if any(code.startswith(marker) for marker in SHELL_MARKERS):
            continue

        expect_error = EXPECT_ERROR in attrs
        checked += 1

        with tempfile.NamedTemporaryFile("w", suffix=".upp", encoding="utf-8",
                                         delete=False) as handle:
            handle.write(code + "\n")
            temp = Path(handle.name)

        try:
            # Приклади з «ввести» чекають на введення — даємо їм щось.
            proc = subprocess.run([str(exe), str(temp)], input="21\n".encode("utf-8"),
                                  capture_output=True, timeout=15)
        finally:
            temp.unlink(missing_ok=True)

        stderr = proc.stderr.decode("utf-8", "replace").strip()

        if expect_error and proc.returncode == 0:
            failed.append((number, code, "очікувалась помилка, але програма відпрацювала успішно"))
        elif not expect_error and proc.returncode != 0:
            failed.append((number, code, stderr))

    for number, code, error in failed:
        print(f"ПРОВАЛ  блок №{number}")
        for line in code.splitlines():
            print(f"    | {line}")
        print(f"  {error}\n")

    print(f"Перевірено прикладів: {checked}, провалено: {len(failed)}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
