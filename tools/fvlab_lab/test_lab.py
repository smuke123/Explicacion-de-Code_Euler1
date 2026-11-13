"""Pruebas del laboratorio: parser .xl y namelist. No tocan el nucleo numerico."""

from __future__ import annotations

import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
if str(HERE) not in sys.path:
    sys.path.insert(0, str(HERE))

from controller import RunConfig, find_solver, run_simulation, write_namelist
from xl_io import read_last_block, read_xl_file


def test_xl_parser() -> None:
    text = (
        "#Time = 0.0000000000E+00\n"
        "  0.1000000000E+00  0.1000000000E+01\n"
        "  0.2000000000E+00  0.1250000000E+00\n"
        "\n"
        "#Time = 0.2000000000E+00\n"
        "  0.1000000000E+00  0.4263190000E+00\n"
        "  0.2000000000E+00  0.1250000000E+00\n"
    )
    path = Path(tempfile.mkdtemp()) / "rho_1.xl"
    path.write_text(text, encoding="utf-8")
    blocks = read_xl_file(path)
    assert len(blocks) == 2
    last = read_last_block(path)
    assert last.time == 0.2
    assert last.x == [0.1, 0.2]
    assert last.value[0] == 0.426319


def test_namelist_keys() -> None:
    path = Path(tempfile.mkdtemp()) / "input.par"
    write_namelist(RunConfig(test=5, n=80, solver="hll", limiter="sb"), path)
    text = path.read_text(encoding="utf-8").lower()
    for key in (
        "test",
        "n =",
        "cfl",
        "gamma",
        "limiter",
        "solver",
        "bc",
        "xmin",
        "xmax",
        "x0",
        "tf",
        "grav",
        "alpha_geo",
        "rho_floor",
        "p_floor",
    ):
        assert key in text, key
    assert "hll" in text
    assert "sb" in text


def test_solver_smoke() -> None:
    try:
        solver = find_solver()
    except FileNotFoundError:
        print("SMOKE omitido: no hay xEuler1D")
        return
    result = run_simulation(
        RunConfig(test=1, n=50, solver="hllc", limiter="vl", tf=0.2, x0=0.3, label="smoke"),
        solver_path=solver,
    )
    assert result.returncode == 0, result.stderr or result.stdout
    assert "density" in result.fields
    assert len(result.fields["density"].numerical) == 50
    assert result.errors["density"]["max_abs"] > 0.0
    print(
        "SMOKE ok  pasos="
        + str(result.steps)
        + "  max_abs_rho="
        + f"{result.errors['density']['max_abs']:.4e}"
    )


if __name__ == "__main__":
    test_xl_parser()
    test_namelist_keys()
    test_solver_smoke()
    print("test_lab: OK")
