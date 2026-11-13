"""Puerta unica entre la GUI y el nucleo C++.

Escribe un namelist, lanza xEuler1D y lee los .xl. No implementa HLL/HLLC.
"""

from __future__ import annotations

import os
import re
import subprocess
import tempfile
from dataclasses import dataclass, field
from pathlib import Path

from xl_io import XlBlock, read_last_block

REPO_ROOT = Path(__file__).resolve().parents[2]


@dataclass
class RunConfig:
    test: int = 1
    n: int = 200
    cfl: float = 0.9
    gamma: float = 1.4
    limiter: str = "vl"
    solver: str = "hllc"
    bc: str = "trans"
    xmin: float = 0.0
    xmax: float = 1.0
    x0: float = -1.0
    tf: float = -1.0
    every_out: int = 0
    grav: float = 0.0
    alpha_geo: int = 0
    rho_floor: float = 1.0e-12
    p_floor: float = 1.0e-12
    label: str = ""


@dataclass
class FieldSeries:
    x: list[float]
    numerical: list[float]
    exact: list[float] | None = None


@dataclass
class RunResult:
    config: RunConfig
    label: str
    stdout: str
    stderr: str
    returncode: int
    output_dir: Path
    steps: int | None = None
    final_time: float | None = None
    star_line: str = ""
    fields: dict[str, FieldSeries] = field(default_factory=dict)
    errors: dict[str, dict[str, float]] = field(default_factory=dict)


def find_solver() -> Path:
    env = os.environ.get("EULER1D_BIN")
    candidates = []
    if env:
        candidates.append(Path(env))
    candidates.extend(
        [
            REPO_ROOT / "build" / "xEuler1D.exe",
            REPO_ROOT / "build" / "xEuler1D",
            REPO_ROOT / "build-debug" / "xEuler1D.exe",
            REPO_ROOT / "build-debug" / "xEuler1D",
        ]
    )
    for path in candidates:
        if path.is_file():
            return path
    raise FileNotFoundError(
        "No se encontro xEuler1D. Compila el nucleo:\n"
        "  cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release\n"
        "  cmake --build build\n"
        "O define EULER1D_BIN con la ruta del ejecutable."
    )


def _solver_environment() -> dict[str, str]:
    """El .exe de MSYS2 UCRT necesita libstdc++/libgcc en el PATH."""
    env = os.environ.copy()
    extras = [
        Path(r"C:\msys64\ucrt64\bin"),
        Path(r"C:\msys64\mingw64\bin"),
    ]
    prefix = [str(p) for p in extras if p.is_dir()]
    if prefix:
        env["PATH"] = os.pathsep.join(prefix + [env.get("PATH", "")])
    return env


def write_namelist(config: RunConfig, path: Path) -> None:
    lines = [
        "&euler",
        f" test = {int(config.test)}",
        f" N = {int(config.n)}",
        f" cfl = {config.cfl}",
        f" gamma = {config.gamma}",
        f" limiter = '{config.limiter}'",
        f" solver = '{config.solver}'",
        f" bc = '{config.bc}'",
        f" xmin = {config.xmin}",
        f" xmax = {config.xmax}",
        f" x0 = {config.x0}",
        f" tf = {config.tf}",
        f" every_out = {int(config.every_out)}",
        f" grav = {config.grav}",
        f" alpha_geo = {int(config.alpha_geo)}",
        f" rho_floor = {config.rho_floor}",
        f" p_floor = {config.p_floor}",
        "/",
        "",
    ]
    path.write_text("\n".join(lines), encoding="utf-8")


def _metrics(reference: list[float], candidate: list[float], dx: float) -> dict[str, float]:
    n = min(len(reference), len(candidate))
    if n == 0:
        return {}
    max_abs = 0.0
    l1 = 0.0
    l2 = 0.0
    ref_max = 0.0
    for i in range(n):
        diff = abs(candidate[i] - reference[i])
        max_abs = max(max_abs, diff)
        l1 += diff * dx
        l2 += diff * diff * dx
        ref_max = max(ref_max, abs(reference[i]))
    return {
        "max_abs": max_abs,
        "l1": l1,
        "l2": l2 ** 0.5,
        "ref_max": ref_max,
    }


def _load_fields(
    output_dir: Path, gamma: float = 1.4
) -> tuple[dict[str, FieldSeries], dict[str, dict[str, float]]]:
    mapping = {"density": "rho", "velocity": "vel", "pressure": "press"}
    fields: dict[str, FieldSeries] = {}
    errors: dict[str, dict[str, float]] = {}
    for key, stem in mapping.items():
        num_path = output_dir / f"{stem}_1.xl"
        if not num_path.is_file():
            continue
        numerical = read_last_block(num_path)
        exact_block: XlBlock | None = None
        exact_path = output_dir / f"{stem}_exact.xl"
        if exact_path.is_file():
            try:
                exact_block = read_last_block(exact_path)
            except ValueError:
                exact_block = None
        exact_values = exact_block.value if exact_block and len(exact_block.value) == len(numerical.value) else None
        fields[key] = FieldSeries(x=numerical.x, numerical=numerical.value, exact=exact_values)
        if exact_values:
            dx = numerical.x[1] - numerical.x[0] if len(numerical.x) > 1 else 1.0
            errors[key] = _metrics(exact_values, numerical.value, dx)
            err_vals = [abs(numerical.value[i] - exact_values[i]) for i in range(len(exact_values))]
            fields[f"{key}_error"] = FieldSeries(x=numerical.x, numerical=err_vals, exact=None)
    if "density" in fields and "velocity" in fields and "pressure" in fields:
        dens = fields["density"]
        vel = fields["velocity"]
        pres = fields["pressure"]
        n = min(len(dens.numerical), len(vel.numerical), len(pres.numerical))
        energy = [
            pres.numerical[i] / (gamma - 1.0) + 0.5 * dens.numerical[i] * vel.numerical[i] ** 2
            for i in range(n)
        ]
        energy_exact = None
        if dens.exact and vel.exact and pres.exact:
            energy_exact = [
                pres.exact[i] / (gamma - 1.0) + 0.5 * dens.exact[i] * vel.exact[i] ** 2
                for i in range(n)
            ]
        fields["energy"] = FieldSeries(x=dens.x[:n], numerical=energy, exact=energy_exact)
        if energy_exact:
            dx = dens.x[1] - dens.x[0] if len(dens.x) > 1 else 1.0
            errors["energy"] = _metrics(energy_exact, energy, dx)
    return fields, errors


def run_simulation(config: RunConfig, solver_path: Path | None = None) -> RunResult:
    solver = solver_path or find_solver()
    label = config.label or f"{config.solver.upper()}+{config.limiter} N={config.n}"
    work = Path(tempfile.mkdtemp(prefix="fvlab_"))
    namelist = work / "input.par"
    write_namelist(config, namelist)

    completed = subprocess.run(
        [str(solver), "-i", str(namelist), "-o", str(work)],
        capture_output=True,
        text=True,
        cwd=str(work),
        timeout=300,
        env=_solver_environment(),
    )
    stdout = completed.stdout or ""
    stderr = completed.stderr or ""

    steps = None
    final_time = None
    match = re.search(r"pasos:\s+(\d+)\s+t final:\s+([0-9.eE+-]+)", stdout)
    if match:
        steps = int(match.group(1))
        final_time = float(match.group(2))
    star = ""
    star_match = re.search(r"exacto:.*", stdout)
    if star_match:
        star = star_match.group(0).strip()

    fields: dict[str, FieldSeries] = {}
    errors: dict[str, dict[str, float]] = {}
    if completed.returncode == 0:
        fields, errors = _load_fields(work, gamma=config.gamma)

    return RunResult(
        config=config,
        label=label,
        stdout=stdout,
        stderr=stderr,
        returncode=completed.returncode,
        output_dir=work,
        steps=steps,
        final_time=final_time,
        star_line=star,
        fields=fields,
        errors=errors,
    )
