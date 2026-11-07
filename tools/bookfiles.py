"""Общие функции для tools/check.py и tools/build_pdf.py."""
import pathlib
import re

ROOT = pathlib.Path(__file__).resolve().parent.parent
BOOK = ROOT / "book"
SCRIPTS = ROOT / "scripts"

FRONTMATTER = re.compile(r"\A---\n.*?\n---\n", re.S)
FOOTER = re.compile(r"\n---\n\n(?:Назад|Далее|В начало):[^\n]*\n*\Z")
LINK = re.compile(r"(!?)\[([^\]]*)\]\(([^)\s]+)\)")
# Заголовок «## `имя.sh`» или «### `имя.patch`» и первый блок bash/diff под ним.
SCRIPT_BLOCK = re.compile(r"^#{2,3} `([\w.-]+\.(?:sh|patch))`[^\n]*\n.*?```(?:bash|diff)\n(.*?)```", re.S | re.M)


def chapters():
    """Файлы книги в порядке чтения: NN-имя.md и вставки вида NNa-имя.md (00a идёт после 00)."""
    return sorted(BOOK.glob("[0-9][0-9]*-*.md"))


def strip_frontmatter(text):
    return FRONTMATTER.sub("", text, count=1).lstrip("\n")


def strip_footer(text):
    return FOOTER.sub("\n", text)
