"""Ejemplos guiados. Los estados coinciden con src/initial/test_case.cpp."""

from __future__ import annotations

from dataclasses import dataclass

from controller import RunConfig


@dataclass(frozen=True)
class GuidedExample:
    id: str
    title: str
    problem: str
    initial: str
    recommended: str
    observe: str
    config: RunConfig
    open_topic: str


CASES = {
    1: {
        "name": "Toro 1 — Sod modificado",
        "left": (1.0, 0.75, 1.0),
        "right": (0.125, 0.0, 0.1),
        "x0": 0.3,
        "tf": 0.2,
        "note": "Rarefaccion sonica. p*=0.46629, u*=1.36091 (Toro).",
    },
    2: {
        "name": "Toro 2 — dos rarefacciones (123)",
        "left": (1.0, -2.0, 0.4),
        "right": (1.0, 2.0, 0.4),
        "x0": 0.5,
        "tf": 0.15,
        "note": "Region central casi vacia. Prueba de positividad.",
    },
    3: {
        "name": "Toro 3 — blast izquierdo",
        "left": (1.0, 0.0, 1000.0),
        "right": (1.0, 0.0, 0.01),
        "x0": 0.5,
        "tf": 0.012,
        "note": "Choque muy fuerte. Robustez.",
    },
    4: {
        "name": "Toro 4 — colision de choques",
        "left": (5.99924, 19.5975, 460.894),
        "right": (5.99242, -6.19633, 46.0950),
        "x0": 0.4,
        "tf": 0.035,
        "note": "Dos choques que chocan.",
    },
    5: {
        "name": "Toro 5 — contacto casi estacionario",
        "left": (1.0, -19.59745, 1000.0),
        "right": (1.0, -19.59745, 0.01),
        "x0": 0.8,
        "tf": 0.012,
        "note": "Donde HLL mas pierde frente a HLLC.",
    },
    6: {
        "name": "Onda de entropia (suave)",
        "left": (1.0, 1.0, 1.0),
        "right": (1.0, 1.0, 1.0),
        "x0": 0.5,
        "tf": 1.0,
        "note": "Perfil sinusoidal. Sirve para orden de convergencia. BC periodica.",
    },
    7: {
        "name": "Atmosfera hidrostatica",
        "left": (1.0, 0.0, 1.0),
        "right": (1.0, 0.0, 1.0),
        "x0": 0.5,
        "tf": 10.0,
        "note": "Equilibrio con gravedad. max|u| debe ser ~1e-15.",
    },
    8: {
        "name": "Sedov esferico",
        "left": (1.0, 0.0, 1.0),
        "right": (1.0, 0.0, 1.0),
        "x0": 0.5,
        "tf": 0.5,
        "note": "Geometria esferica impuesta. Mas caro; usa N pequeño para el demo.",
    },
}


def format_state(case_id: int) -> str:
    info = CASES[case_id]
    l = info["left"]
    r = info["right"]
    return (
        f"{info['name']}\n"
        f"  Izquierda  ρ={l[0]}, u={l[1]}, p={l[2]}\n"
        f"  Derecha    ρ={r[0]}, u={r[1]}, p={r[2]}\n"
        f"  x0={info['x0']}   tf={info['tf']}\n"
        f"  {info['note']}"
    )


GUIDED = [
    GuidedExample(
        id="t1_hllc",
        title="1. Toro 1 — primer experimento",
        problem="Tubo de choque (Sod modificado). Rarefaccion + contacto + choque.",
        initial=format_state(1),
        recommended="Deja HLLC + van Leer. Luego cambia solo el solver a HLL.",
        observe="Tres ondas. La exacta (linea) ancla el choque y el contacto.",
        config=RunConfig(test=1, n=200, solver="hllc", limiter="vl", tf=0.2, x0=0.3, label="Toro1 HLLC"),
        open_topic="hllc",
    ),
    GuidedExample(
        id="t5_compare",
        title="2. Toro 5 — HLL vs HLLC (demo de tesis)",
        problem="Contacto casi estacionario. HLL lo promedia; HLLC lo captura.",
        initial=format_state(5),
        recommended="Ejecuta HLLC, luego cambia a HLL y compara en densidad.",
        observe="El salto de ρ queda mucho mas ancho con HLL. Ese es el punto.",
        config=RunConfig(test=5, n=200, solver="hllc", limiter="vl", tf=0.012, x0=0.8, label="Toro5 HLLC"),
        open_topic="hllc",
    ),
    GuidedExample(
        id="t1_limiters",
        title="3. Toro 1 — fo vs van Leer",
        problem="Mismo Riemann; solo cambia la reconstruccion.",
        initial=format_state(1),
        recommended="Ejecuta fo, luego vl, compara densidad.",
        observe="fo borra el contacto. vl lo recupera sin tocar el solver.",
        config=RunConfig(test=1, n=200, solver="hllc", limiter="fo", tf=0.2, x0=0.3, label="Toro1 fo"),
        open_topic="fo",
    ),
    GuidedExample(
        id="t2_vacuum",
        title="4. Toro 2 — positividad",
        problem="Dos rarefacciones, centro casi vacio.",
        initial=format_state(2),
        recommended="N=200, HLLC, vl. Si subes N mucho, tarda mas pero sigue estable.",
        observe="ρ y p deben permanecer positivos. Mira el log por avisos de piso.",
        config=RunConfig(test=2, n=200, solver="hllc", limiter="vl", tf=0.15, x0=0.5, label="Toro2 HLLC"),
        open_topic="conserved",
    ),
    GuidedExample(
        id="entropy",
        title="5. Entropia suave — orden, no choques",
        problem="Sinusoide advectada. Frontera periodica impuesta.",
        initial=format_state(6),
        recommended="N=100 vs N=200. El error debe bajar al refinar.",
        observe="No hay discontinuidades. Superbee puede 'escalonar' la onda.",
        config=RunConfig(test=6, n=100, solver="hllc", limiter="vl", tf=1.0, label="Entropia N=100"),
        open_topic="muscl",
    ),
]
