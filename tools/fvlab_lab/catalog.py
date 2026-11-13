"""Manual interactivo: concepto -> ecuacion -> algoritmo -> archivo real.

Los fragmentos se leen del repositorio. No hay pseudocodigo inventado que
contradiga la implementacion.
"""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]


@dataclass(frozen=True)
class SourceAnchor:
    relpath: str
    start: int
    end: int


@dataclass
class Topic:
    id: str
    title: str
    summary: str
    why: str
    equations: str
    algorithm: str
    observe: str
    source: SourceAnchor
    experiment: str


def read_source(anchor: SourceAnchor, max_chars: int = 3500) -> str:
    path = REPO_ROOT / anchor.relpath
    if not path.is_file():
        return f"(no se encontro {anchor.relpath})"
    lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
    start = max(1, anchor.start)
    end = min(len(lines), anchor.end)
    body = "\n".join(f"{i:4d}| {lines[i - 1]}" for i in range(start, end + 1))
    header = f"{anchor.relpath}:{start}-{end}\n"
    text = header + body
    if len(text) > max_chars:
        return text[:max_chars] + "\n… (archivo recortado; abre el fuente en el editor)"
    return text


TOPICS: dict[str, Topic] = {
    "euler": Topic(
        id="euler",
        title="Ecuaciones de Euler",
        summary="Gas ideal compresible, no viscoso, sin conduccion de calor.",
        why="Son el modelo fisico de este laboratorio. Toda la numerica conserva masa, momento y energia.",
        equations=(
            "∂U/∂t + ∂F(U)/∂x = S\n"
            "U = (ρ, ρu, E)\n"
            "E = p/(γ-1) + ρ u²/2\n"
            "F = (ρu, ρu²+p, u(E+p))"
        ),
        algorithm="El nucleo nunca avanza primitivas sueltas: convierte a conservadas, calcula flujos y vuelve a primitivas con la EOS.",
        observe="Un choque es un salto en p y ρ; un contacto es un salto en ρ con p y u continuos.",
        source=SourceAnchor("include/euler1d/physics/ideal_gas.hpp", 1, 80),
        experiment="Carga Toro 1 y mira densidad, velocidad y presion a la vez.",
    ),
    "conserved": Topic(
        id="conserved",
        title="Variables conservadas y primitivas",
        summary="Conservadas U=(ρ,ρu,E) para el avance; primitivas W=(ρ,u,p) para reconstruir y pintar.",
        why="El esquema de Godunov reconstruye sobre primitivas pero actualiza conservadas. Si se desincronizan, el resultado es falso y puede no dar NaN.",
        equations="U = EOS(W),   W = EOS⁻¹(U)\nc = √(γ p / ρ)",
        algorithm="Un unico synchronize() restablece W a partir de U y rellena fantasmas.",
        observe="Si un piso de presion dispara, W deja de ser la imagen exacta de U. El nucleo lo cuenta.",
        source=SourceAnchor("include/euler1d/physics/state.hpp", 1, 40),
        experiment="Ejecuta Toro 2 (casi vacio) y mira si el aviso de pisos aparece.",
    ),
    "fvm": Topic(
        id="fvm",
        title="Volumenes finitos / flujo numerico",
        summary="Cada celda se actualiza con lo que entra menos lo que sale por las caras.",
        why="La conservacion es estructural: un solo flujo por cara, compartido por las dos celdas vecinas.",
        equations="U_i^{n+1} = U_i^n - (Δt/Δx) (F_{i+1/2} - F_{i-1/2}) + Δt S_i",
        algorithm="1) reconstruir W en caras  2) Riemann → F  3) divergencia  4) fuentes",
        observe="Si calcularas dos flujos distintos por cara, la masa se crearia o destruiria.",
        source=SourceAnchor("include/euler1d/numerical/finite_volume_operator.hpp", 1, 60),
        experiment="Compara N=50 y N=400 en Toro 1: el choque se afila al refinar.",
    ),
    "ghost": Topic(
        id="ghost",
        title="Celdas fantasma",
        summary="Dos celdas ficticias a cada lado para que el stencil MUSCL no se salga.",
        why="Sin ellas el limitador no tiene vecinos en el borde.",
        equations="indices k = -1 … N+2,   fisicas = 1 … N   (convencion Fortran)",
        algorithm="Transmisiva copia; periodica envuelve; reflectiva invierte u.",
        observe="Una frontera mal puesta refleja ondas que no deberian existir.",
        source=SourceAnchor("include/euler1d/boundary/boundary_condition.hpp", 1, 55),
        experiment="Toro 1 usa transmisiva. El test de entropia impone periodica.",
    ),
    "muscl": Topic(
        id="muscl",
        title="Reconstruccion MUSCL",
        summary="Dentro de cada celda se ajusta una recta limitada y se extrapolan los estados de cara.",
        why="El orden 1 (constante) es robusto pero borroso. MUSCL sube a orden 2 en zonas suaves.",
        equations="q_L = W_i + ½ σ_i ,   q_R = W_{i+1} - ½ σ_{i+1}\nσ = limitador(Δatras, Δadelante)",
        algorithm="Un barrido completo, no una llamada virtual por celda. El limitador es un tipo-politica.",
        observe="Con fo (pendiente 0) el contacto se ensancha. Con vl/sb se afila, a costa de mas oscilacion.",
        source=SourceAnchor("include/euler1d/numerical/reconstruction/reconstruction.hpp", 18, 57),
        experiment="Mismo Toro 1: limiter fo vs vl.",
    ),
    "fo": Topic(
        id="fo",
        title="Limitador first-order (fo)",
        summary="Pendiente nula: Godunov de primer orden.",
        why="Referencia disipativa. Si un esquema 'de alto orden' se parece a fo, algo esta mal.",
        equations="σ = 0",
        algorithm="FirstOrder::slope ignora las diferencias y devuelve 0.",
        observe="Todo se suaviza: contacto y choque ocupan muchas celdas.",
        source=SourceAnchor("include/euler1d/numerical/reconstruction/slope_limiter.hpp", 21, 29),
        experiment="Toro 5 con fo frente a vl: el contacto casi no se mueve y fo lo borra.",
    ),
    "mm": Topic(
        id="mm",
        title="Limitador minmod (mm)",
        summary="Toma la pendiente de menor magnitud si tienen el mismo signo; si no, cero.",
        why="El mas disipativo de los TVD clasicos. Muy seguro, poco nítido.",
        equations="σ = ½ (sign(a)+sign(b)) min(|a|,|b|)\ncon sign(0)=+1 como Fortran",
        algorithm="Replica reconstructor del Fortran, incluido sign(1,0)=+1.",
        observe="Menos oscilacion que superbee; mas borroso que van Leer.",
        source=SourceAnchor("include/euler1d/numerical/reconstruction/slope_limiter.hpp", 31, 36),
        experiment="Toro 1: mm vs sb.",
    ),
    "vl": Topic(
        id="vl",
        title="Limitador van Leer (vl)",
        summary="Media harmonica de las diferencias si no hay cambio de signo.",
        why="Compromiso habitual en este codigo (es el default del Fortran).",
        equations="si a·b>0:  σ = (a|b| + b|a|) / (|a|+|b|)\nsi no: σ = 0",
        algorithm="VanLeer::slope en slope_limiter.hpp, instanciado por plantilla.",
        observe="Buen contacto sin el ringing agresivo de superbee.",
        source=SourceAnchor("include/euler1d/numerical/reconstruction/slope_limiter.hpp", 38, 48),
        experiment="Ejemplo guiado 'Toro 1: HLL vs HLLC' usa vl.",
    ),
    "mc": Topic(
        id="mc",
        title="Limitador monotonized central (mc)",
        summary="Media centrada recortada a 2 veces cada diferencia unilateral.",
        why="Mas nítido que minmod, mas controlado que superbee.",
        equations="si a·b>0: σ = sign(c) min(|c|, 2|a|, 2|b|),  c=(a+b)/2",
        algorithm="Misma rama 'mc' del Fortran.",
        observe="Util cuando quieres segundo orden sin el overshoot de sb.",
        source=SourceAnchor("include/euler1d/numerical/reconstruction/slope_limiter.hpp", 49, 58),
        experiment="Toro 1: mc vs vl.",
    ),
    "sb": Topic(
        id="sb",
        title="Limitador superbee (sb)",
        summary="El mas agresivo de la familia TVD clasica.",
        why="Resuelve contactos muy finos; puede marcar 'escalones' en zonas suaves.",
        equations="σ = maxmod(minmod(2a,b), minmod(a,2b))  (forma Fortran con sign)",
        algorithm="Superbee::slope; usa fortranSign, no copysign de C++.",
        observe="En Toro 5 el contacto queda muy fino. En la onda de entropia puede verse 'cuadricula'.",
        source=SourceAnchor("include/euler1d/numerical/reconstruction/slope_limiter.hpp", 60, 72),
        experiment="Entropia suave: sb vs vl. Busca escalones.",
    ),
    "hll": Topic(
        id="hll",
        title="Solver HLL (Davis)",
        summary="Dos ondas SL, SR. Toda la estructura intermedia se promedia.",
        why="Robusto y barato. Difumina el contacto porque no lo modela.",
        equations=(
            "SL = min(uL-cL, uR-cR),  SR = max(uL+cL, uR+cR)   (Davis)\n"
            "si 0≤SL: F=FL;  si 0≥SR: F=FR\n"
            "si no: F = (SR FL - SL FR + SL SR (UR-UL)) / (SR-SL)"
        ),
        algorithm="hllFlux en hll.hpp. Velocidades Davis, no Einfeldt (HLLE aun no esta).",
        observe="El salto de densidad del contacto es mas ancho que con HLLC.",
        source=SourceAnchor("include/euler1d/numerical/riemann/hll.hpp", 13, 37),
        experiment="Toro 5: HLL vs HLLC. Es la demo central de la tesis.",
    ),
    "hllc": Topic(
        id="hllc",
        title="Solver HLLC",
        summary="Tres ondas: SL, S* (contacto), SR. La tangencial se advecta con el contacto.",
        why="Recupera la onda que HLL promedia. Coste apenas mayor.",
        equations=(
            "S* = (pR-pL + ρL uL (SL-uL) - ρR uR (SR-uR))\n"
            "     / (ρL(SL-uL) - ρR(SR-uR))     Toro (10.37)\n"
            "U* se reconstruye y F* = FK + SK (U*-UK)"
        ),
        algorithm="hllcFlux en hllc.hpp, mismas operaciones que el Fortran.",
        observe="El contacto de densidad se resuelve en menos celdas.",
        source=SourceAnchor("include/euler1d/numerical/riemann/hllc.hpp", 13, 51),
        experiment="Misma malla y limiter: cambia solo solver hll → hllc.",
    ),
    "exact": Topic(
        id="exact",
        title="Riemann exacto (referencia)",
        summary="Solucion semianalitica de Toro cap. 4. NO avanza el fluido.",
        why="Oraculo de verificacion. Comparar HLLC contra HLLC seria circular.",
        equations="Se itera p* con las funciones fK; luego se muestrea x/t.",
        algorithm="ExactRiemannSolver::solveStar + sample. Cuelga de Simulation, no del operador.",
        observe="La curva negra de la grafica es este solver, no otro esquema FV.",
        source=SourceAnchor("include/euler1d/numerical/riemann/exact_riemann_solver.hpp", 1, 51),
        experiment="Toro 1: la linea exacta debe cruzar el choque y el contacto en el sitio correcto.",
    ),
    "cfl": Topic(
        id="cfl",
        title="Paso de tiempo CFL",
        summary="dt = CFL · Δx / max(|u|+c). Si te pasas, el esquema explota.",
        why="Estabilidad, no precision. Un CFL alto no 'mejora' el orden.",
        equations="dt = CFL Δx / max_k (|u_k| + √(γ p_k/ρ_k))",
        algorithm="CflController: un barrido, mas un checksum para no tragar NaN.",
        observe="Bajar CFL aumenta el numero de pasos; el perfil final cambia poco si el esquema es correcto.",
        source=SourceAnchor("include/euler1d/numerical/cfl_controller.hpp", 9, 44),
        experiment="Toro 1, CFL 0.3 vs 0.9. Mira el recuento de pasos.",
    ),
    "rk2": Topic(
        id="rk2",
        title="Integrador SSP-RK2",
        summary="Heun / SSP-RK2: dos evaluaciones del operador espacial.",
        why="Es el integrador del Fortran. Los destinos de etapa estan separados (sin aliasing).",
        equations="U1 = U + dt L(U)\nU2 = U1 + dt L(U1)\nU^{n+1} = (U + U2)/2",
        algorithm="SspRk2::advance. Entre etapas obliga a synchronize (primitivas + BCs).",
        observe="El CLI no deja elegir Euler adelante; el nucleo si lo tiene para tests.",
        source=SourceAnchor("include/euler1d/numerical/time/time_integrator.hpp", 37, 50),
        experiment="Cualquier caso: el log imprime el numero de pasos de este integrador.",
    ),
    "bc": Topic(
        id="bc",
        title="Condiciones de frontera",
        summary="trans = outflow, per = periodica, refl = pared (u → −u).",
        why="Algunos tests las imponen (entropia=per, atmosfera/Sedov=refl).",
        equations="Fantasmas se rellenan antes de cada reconstruccion.",
        algorithm="BoundaryCondition::apply sobre U y W.",
        observe="Si pones transmisiva en la onda de entropia, la masa se escapa.",
        source=SourceAnchor("include/euler1d/boundary/boundary_condition.hpp", 25, 54),
        experiment="Carga 'Entropia' y no cambies bc: el test la fuerza a periodica.",
    ),
    "source": Topic(
        id="source",
        title="Terminos fuente y geometria",
        summary="La gravedad es aditiva. La geometria radial cambia la divergencia, no es una fuente inventada.",
        why="Mezclar geometria como -(α/r)F no conserva. El nucleo usa areas y volumenes exactos.",
        equations="S_grav = (0, −ρ g, −ρ g u)\nV = (r₊^{α+1} − r₋^{α+1}) / (α+1)",
        algorithm="CartesianDivergence o RadialDivergence, luego GravitySource::accumulate.",
        observe="Atmosfera (test 7): max|u| debe ser numerico-cero. Sedov (test 8) usa α=2.",
        source=SourceAnchor("include/euler1d/numerical/source/source_term.hpp", 48, 97),
        experiment="Test 7 con N=100. Si ves viento, algo rompio el balance.",
    ),
}


LIMITER_TO_TOPIC = {"fo": "fo", "mm": "mm", "vl": "vl", "mc": "mc", "sb": "sb"}
SOLVER_TO_TOPIC = {"hll": "hll", "hllc": "hllc"}

TOPIC_ORDER = [
    "euler",
    "conserved",
    "fvm",
    "ghost",
    "muscl",
    "fo",
    "mm",
    "vl",
    "mc",
    "sb",
    "hll",
    "hllc",
    "exact",
    "cfl",
    "rk2",
    "bc",
    "source",
]


def topic_for_selection(solver: str, limiter: str) -> str:
    return SOLVER_TO_TOPIC.get(solver, "hllc")
