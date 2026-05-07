#!/usr/bin/env python3

from pathlib import Path
import re
import sys


def main() -> int:
    if len(sys.argv) != 4:
        print("usage: update_homebrew_formula.py <formula-path> <version> <sha256>", file=sys.stderr)
        return 1

    formula_path = Path(sys.argv[1])
    version = sys.argv[2]
    sha256 = sys.argv[3]

    text = formula_path.read_text()
    text = re.sub(
        r'url "https://github\.com/lxrzlyr/mtop/releases/download/v[^/]+/mtop-[^"]+-source\.tar\.gz"',
        f'url "https://github.com/lxrzlyr/mtop/releases/download/v{version}/mtop-{version}-source.tar.gz"',
        text,
    )
    text = re.sub(r'sha256 "[0-9a-f]+"', f'sha256 "{sha256}"', text)
    formula_path.write_text(text)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
