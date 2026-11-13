"""Orden de convergencia observado sobre la onda de entropia (test 6).

Lee el ultimo bloque de rho_1.xl y rho_exact.xl de cada corrida y calcula
L1 = sum |e| dx  y  L2 = sqrt(sum e^2 dx), con dx = 1/N.
"""

import math
import sys
from pathlib import Path


def read_last_block(path):
    x, v = [], []
    for line in path.read_text().splitlines():
        line = line.strip()
        if line.startswith("#Time"):
            x, v = [], []
            continue
        if not line:
            continue
        a, b = line.split()
        x.append(float(a))
        v.append(float(b))
    return x, v


def norms(run_dir, field):
    _, num = read_last_block(run_dir / f"{field}_1.xl")
    _, exa = read_last_block(run_dir / f"{field}_exact.xl")
    n = len(num)
    dx = 1.0 / n
    l1 = sum(abs(a - b) for a, b in zip(num, exa)) * dx
    l2 = math.sqrt(sum((a - b) ** 2 for a, b in zip(num, exa)) * dx)
    return n, l1, l2


def main():
    base = Path(sys.argv[1] if len(sys.argv) > 1 else ".")
    rows = []
    for n in (50, 100, 200, 400, 800):
        d = base / f"conv_vl_N{n}"
        if not (d / "rho_1.xl").exists():
            continue
        rows.append(norms(d, "rho"))

    print(f"{'N':>5} {'L1':>12} {'p(L1)':>7} {'L2':>12} {'p(L2)':>7}")
    prev = None
    for n, l1, l2 in rows:
        if prev is None:
            print(f"{n:>5} {l1:>12.4e} {'-':>7} {l2:>12.4e} {'-':>7}")
        else:
            p1 = math.log(prev[1] / l1) / math.log(n / prev[0])
            p2 = math.log(prev[2] / l2) / math.log(n / prev[0])
            print(f"{n:>5} {l1:>12.4e} {p1:>7.3f} {l2:>12.4e} {p2:>7.3f}")
        prev = (n, l1, l2)


if __name__ == "__main__":
    main()
