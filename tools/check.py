#!/usr/bin/env python3
"""Проверка книги: ссылки, картинки, блоки кода, синхронность скриптов.

    python3 tools/check.py             # локальные проверки
    python3 tools/check.py --external  # плюс доступность внешних ссылок (нужен curl)
"""
import re
import subprocess
import sys
import urllib.parse

from bookfiles import BOOK, FRONTMATTER, LINK, ROOT, SCRIPT_BLOCK, SCRIPTS, chapters, strip_frontmatter

errors = []
LIST_ITEM = re.compile(r"(?:[-*] |\d+\. )")


def err(path, line, msg):
    errors.append(f"{path.relative_to(ROOT)}:{line}: {msg}")


def unquote(line):
    """Строка без префикса цитаты/callout («> »), отступы сохраняются."""
    return line[2:] if line.startswith("> ") else line[1:] if line.startswith(">") else line


def headings(path):
    """Заголовки файла в двух видах: как пишет Obsidian и как слаг GitHub."""
    out = set()
    for line in path.read_text().splitlines():
        if line.startswith("#"):
            h = line.lstrip("#").strip()
            out.add(h)
            slug = "".join(c for c in h.lower() if c.isalnum() or c in " -_").replace(" ", "-")
            out.add(slug)
    return out


def check_file(path, external):
    text = path.read_text()
    lines = text.splitlines()
    in_code = False
    skip = len(FRONTMATTER.match(text).group(0).splitlines()) if FRONTMATTER.match(text) else 0
    for n, line in enumerate(lines, 1):
        if n <= skip:
            continue
        bare = unquote(line)
        if bare.lstrip().startswith("```"):
            if not in_code and bare.strip() == "```":
                err(path, n, "блок кода без языка (```bash, ```fish, ```text)")
            in_code = not in_code
            continue
        if in_code:
            continue
        prev = unquote(lines[n - 2]) if n > 1 else ""
        if (LIST_ITEM.match(bare) and prev.strip() and not LIST_ITEM.match(prev)
                and not prev.startswith((" ", "|", "[!", "---"))):
            err(path, n, "перед списком нужна пустая строка (иначе pandoc склеит его с абзацем)")
        for bang, _label, target in LINK.findall(re.sub(r"`[^`]*`", "", line)):
            if target.startswith(("http://", "https://")):
                external.add(target)
                continue
            file_part, _, anchor = target.partition("#")
            dest = (path.parent / urllib.parse.unquote(file_part)).resolve() if file_part else path
            if not dest.exists():
                err(path, n, f"нет файла {target}")
            elif anchor and dest.suffix == ".md":
                if urllib.parse.unquote(anchor) not in headings(dest):
                    err(path, n, f"нет заголовка #{anchor} в {dest.name}")
            if bang and dest.suffix not in (".svg", ".png", ".jpg", ".jpeg", ".webp"):
                err(path, n, f"картинка неожиданного типа: {target}")
    if in_code:
        err(path, len(lines), "незакрытый блок кода")


def check_chapter_structure(path):
    body = strip_frontmatter(path.read_text())
    if not path.read_text().startswith("---\n"):
        err(path, 1, "нет frontmatter (aliases, tags)")
    if not body.startswith("# "):
        err(path, 1, "файл должен начинаться с заголовка первого уровня")


def check_scripts():
    """Скрипты и патчи из приложения А должны совпадать с файлами в scripts/ (в любой подпапке)."""
    appendix = BOOK / "12-appendix-a-scripts.md"
    found = dict(SCRIPT_BLOCK.findall(appendix.read_text()))
    files = {}
    for f in sorted(SCRIPTS.rglob("*")):
        if f.suffix in (".sh", ".patch"):
            if f.name in files:
                err(f, 1, f"имя совпадает с {files[f.name].relative_to(ROOT)}")
            files[f.name] = f
    for name, body in found.items():
        f = files.get(name)
        if f is None:
            err(appendix, 1, f"{name} есть в приложении, но нет в scripts/")
        elif f.read_text() != body:
            err(f, 1, f"отличается от текста в {appendix.name}")
    for name, f in files.items():
        if name not in found:
            err(f, 1, "файла нет в приложении А")


def check_external(urls):
    for url in sorted(urls):
        r = subprocess.run(
            ["curl", "-s", "-o", "/dev/null", "-L", "-m", "20", "-r", "0-0",
             "-A", "Mozilla/5.0 (book link check)", "-w", "%{http_code}", url],
            capture_output=True, text=True)
        code = r.stdout.strip()
        mark = "ok " if code in ("200", "206") else "!! "
        print(f"{mark}{code} {url}")
        if mark == "!! ":
            errors.append(f"внешняя ссылка {code}: {url}")


def main():
    external = set()
    files = chapters() + [ROOT / "README.md"]
    for f in files:
        if f.exists():
            check_file(f, external)
    for f in chapters():
        check_chapter_structure(f)
    check_scripts()
    if "--external" in sys.argv:
        check_external(external)
    for e in errors:
        print(e)
    print(f"{len(files)} файлов, {len(external)} внешних ссылок, ошибок: {len(errors)}")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
