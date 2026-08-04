#!/usr/bin/env python3
"""Синхронізує опублікований сайт із джерелом.

Джерело сайту — файли в корені репозиторію. GitHub Pages може бути
налаштований або на корінь, або на теку docs/, тому docs/ має бути
точною копією. Раніше ці копії правили руками, і вони розʼїжджались.

    python tools/sync_site.py          # скопіювати корінь -> docs/
    python tools/sync_site.py --check  # лише перевірити, нічого не писати
"""

from __future__ import annotations

import argparse
import filecmp
import shutil
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DOCS = ROOT / "docs"

# Файли сайту, які живуть у корені.
PAGE_FILES = ["index.html", "style.css", "app.js", "favicon.svg", "logo.svg"]

# Файли для завантаження. Саме на них посилається index.html.
DOWNLOAD_FILES = [
    "upp.exe",
    "upp_setup.exe",
    "upp-installer.zip",
    "upp-vscode-extension.vsix",
]


def pairs() -> list[tuple[Path, Path]]:
    result = [(ROOT / name, DOCS / name) for name in PAGE_FILES]
    result += [
        (ROOT / "downloads" / name, DOCS / "downloads" / name)
        for name in DOWNLOAD_FILES
    ]
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description="Синхронізація docs/ із коренем")
    parser.add_argument("--check", action="store_true",
                        help="лише повідомити про розбіжності, код виходу 1")
    args = parser.parse_args()

    missing: list[str] = []
    differing: list[str] = []
    copied = 0

    for src, dst in pairs():
        if not src.exists():
            missing.append(str(src.relative_to(ROOT)))
            continue

        same = dst.exists() and filecmp.cmp(src, dst, shallow=False)
        if same:
            continue

        differing.append(str(dst.relative_to(ROOT)))
        if not args.check:
            dst.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(src, dst)
            copied += 1

    for name in missing:
        print(f"НЕМАЄ ДЖЕРЕЛА  {name}", file=sys.stderr)

    if args.check:
        for name in differing:
            print(f"РОЗБІЖНІСТЬ  {name}")
        if differing or missing:
            print(f"\nРозбіжностей: {len(differing)}, відсутніх джерел: {len(missing)}")
            return 1
        print("docs/ збігається з коренем")
        return 0

    for name in differing:
        print(f"ОНОВЛЕНО  {name}")
    print(f"\nСкопійовано файлів: {copied}")
    return 1 if missing else 0


if __name__ == "__main__":
    sys.exit(main())
