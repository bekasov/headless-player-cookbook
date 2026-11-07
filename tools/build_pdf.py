#!/usr/bin/env python3
"""Сборка книги в один PDF: Markdown → (pandoc) HTML → (Chromium) PDF.

    python3 tools/build_pdf.py            # dist/headless-player-cookbook.pdf
    python3 tools/build_pdf.py --html     # только HTML, без PDF

Нужны pandoc 3+ и Chromium/Chrome 131+ (поля страницы с номерами через @page).
"""
import datetime
import os
import pathlib
import re
import shutil
import subprocess
import sys

from bookfiles import BOOK, LINK, ROOT, chapters, strip_footer, strip_frontmatter

DIST = ROOT / "dist"
PDFDIR = pathlib.Path(__file__).resolve().parent / "pdf"
NAME = "headless-player-cookbook"
CALLOUT = re.compile(r"^> \[!(\w+)\][+-]? ?(.*)$")
CALLOUT_TITLES = {"warning": "Внимание", "important": "Важно", "note": "Заметка", "tip": "Совет"}


def callouts_to_divs(text):
    """Callout Obsidian (> [!warning] Заголовок) → div pandoc с классом."""
    out, lines, i = [], text.split("\n"), 0
    while i < len(lines):
        m = CALLOUT.match(lines[i])
        if not m:
            out.append(lines[i])
            i += 1
            continue
        kind, title = m.group(1).lower(), m.group(2).strip() or CALLOUT_TITLES.get(m.group(1).lower(), "")
        body = []
        i += 1
        while i < len(lines) and lines[i].startswith(">"):
            body.append(lines[i][2:] if lines[i].startswith("> ") else lines[i][1:])
            i += 1
        out += [f"::: {{.callout .callout-{kind}}}", f"[{title}]{{.callout-title}}", ""] + body + [":::"]
    return "\n".join(out)


def preface_for_print(text):
    """Из предисловия убираем титул, оглавление и список схем: в PDF они свои."""
    text = re.sub(r"\A# .*?\n---\n\n## Предисловие\n", "", text, flags=re.S)
    text = re.sub(r"\n## Оглавление\n.*\Z", "\n", text, flags=re.S)
    return "# Предисловие {#00-preface .unnumbered}\n\n" + text


def chapter_for_print(path):
    text = strip_footer(strip_frontmatter(path.read_text()))
    if path.name.startswith("00-"):
        text = preface_for_print(text)
    else:
        text = re.sub(r"\A# (.*)$", lambda m: f"# {m.group(1)} {{#{path.stem}}}", text, count=1, flags=re.M)

    def relink(m):
        bang, label, target = m.groups()
        if target.startswith(("http://", "https://", "#")):
            return m.group(0)
        file_part, _, _anchor = target.partition("#")
        if bang:  # картинки: абсолютный путь, pandoc встроит их в HTML
            return f"{bang}[{label}]({(path.parent / file_part).resolve()})"
        if file_part.endswith(".md"):
            return f"[{label}](#{pathlib.Path(file_part).stem})"
        return m.group(0)

    text = LINK.sub(relink, text)
    # служебные разделы глав и подзаголовки приложений В и Г не нужны в оглавлении
    text = re.sub(r"^(## (?:Вопросы для самопроверки|Где почитать))$", r"\1 {.unlisted}", text, flags=re.M)
    if path.name.startswith(("15-", "16-")):
        text = re.sub(r"^(## .*)$", r"\1 {.unlisted}", text, flags=re.M)
    return callouts_to_divs(text)


def pandoc_highlight_flag():
    """pandoc 3.8+ понимает --syntax-highlighting, старые версии только --highlight-style."""
    out = subprocess.run(["pandoc", "--version"], capture_output=True, text=True).stdout
    major, minor = (int(x) for x in re.search(r"pandoc (\d+)\.(\d+)", out).groups())
    return "--syntax-highlighting=tango" if (major, minor) >= (3, 8) else "--highlight-style=tango"


def find_chromium():
    for name in ("chromium", "chromium-browser", "google-chrome-stable", "google-chrome"):
        if shutil.which(name):
            return name
    sys.exit("Не найден Chromium/Chrome: поставьте его или соберите только HTML (--html).")


def main():
    DIST.mkdir(exist_ok=True)
    combined = DIST / f"{NAME}.md"
    combined.write_text("\n\n".join(chapter_for_print(p) for p in chapters()))
    date = subprocess.run(["git", "-C", str(ROOT), "log", "-1", "--format=%cs"],
                          capture_output=True, text=True).stdout.strip() or datetime.date.today().isoformat()
    html = DIST / f"{NAME}.html"
    cover = DIST / "cover.html"
    cover.write_text(f'<section class="cover"><img src="{BOOK / "img" / "cover.svg"}" alt="Обложка">'
                     f'<div class="cover-version">Версия {date}</div></section>\n')
    subprocess.run([
        "pandoc", str(combined), "-f", "markdown-smart", "-t", "html5",
        "--standalone", "--embed-resources", "--toc", "--toc-depth=2",
        "--metadata-file", str(PDFDIR / "metadata.yaml"), "--metadata", f"date={date}",
        "--css", str(PDFDIR / "print.css"), pandoc_highlight_flag(),
        "--include-before-body", str(cover),
        "-o", str(html),
    ], check=True)
    print("HTML:", html.relative_to(ROOT))
    if "--html" in sys.argv:
        return
    pdf = DIST / f"{NAME}.pdf"
    subprocess.run([
        find_chromium(), "--headless", "--disable-gpu", "--no-pdf-header-footer",
        *(["--no-sandbox"] if os.environ.get("CI") else []),
        "--virtual-time-budget=10000", f"--print-to-pdf={pdf}", html.as_uri(),
    ], check=True, stderr=subprocess.DEVNULL)
    print("PDF: ", pdf.relative_to(ROOT))


if __name__ == "__main__":
    main()
