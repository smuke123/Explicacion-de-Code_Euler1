"""L1 y L2 frente al Riemann exacto, aislando el solver (HLL vs HLLC).

Mismo caso, misma malla, mismo limitador, mismo CFL: la unica variable es el
solver de Riemann. Es la celda minima de la matriz de comparacion (FR-035).
"""

import math
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from convergencia import norms  # noqa: E402

CASES = {1: "Sod modificado", 2: "123 (dos rarefacciones)", 5: "contacto casi estacionario"}


def main():
    base = Path(sys.argv[1] if len(sys.argv) > 1 else ".")
    print(f"{'caso':>28} {'solver':>6} {'L1(rho)':>12} {'L2(rho)':>12}")
    for test, label in CASES.items():
        for solver in ("hll", "hllc"):
            d = base / f"t{test}_{solver}"
            if not (d / "rho_1.xl").exists():
                continue
            _, l1, l2 = norms(d, "rho")
            print(f"{label:>28} {solver:>6} {l1:>12.4e} {l2:>12.4e}")


if __name__ == "__main__":
    main()
