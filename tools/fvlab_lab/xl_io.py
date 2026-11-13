"""Lector del formato .xl del nucleo C++ / Fortran.

No calcula flujos ni avanza el fluido: solo lee lo que ya escribio OutputManager.
La logica replica include/euler1d/io/xl_reader.hpp.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path


@dataclass
class XlBlock:
    time: float
    x: list[float] = field(default_factory=list)
    value: list[float] = field(default_factory=list)


def read_xl_file(path: Path) -> list[XlBlock]:
    blocks: list[XlBlock] = []
    text = Path(path).read_text(encoding="utf-8", errors="replace")
    for raw in text.splitlines():
        line = raw.rstrip("\r")
        marker = line.find("#Time")
        if marker != -1:
            equals = line.find("=", marker)
            if equals == -1:
                raise ValueError(f"cabecera #Time malformada en {path}")
            blocks.append(XlBlock(time=float(line[equals + 1 :].strip())))
            continue
        parts = line.split()
        if len(parts) >= 2:
            if not blocks:
                raise ValueError(f"datos antes de la primera cabecera en {path}")
            blocks[-1].x.append(float(parts[0]))
            blocks[-1].value.append(float(parts[1]))
    return blocks


def read_last_block(path: Path) -> XlBlock:
    blocks = read_xl_file(path)
    if not blocks:
        raise ValueError(f"archivo .xl sin bloques: {path}")
    return blocks[-1]
