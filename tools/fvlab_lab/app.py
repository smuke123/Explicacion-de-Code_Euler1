"""Punto de entrada del laboratorio interactivo."""

from __future__ import annotations

import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
if str(HERE) not in sys.path:
    sys.path.insert(0, str(HERE))

from ui import launch  # noqa: E402


def main() -> None:
    launch()


if __name__ == "__main__":
    main()
