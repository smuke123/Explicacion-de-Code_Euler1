"""Laboratorio interactivo: aprender → modificar → ejecutar → ver → comparar."""

from __future__ import annotations

import os
import queue
import threading
import tkinter as tk
from tkinter import messagebox, ttk
import matplotlib

matplotlib.use("TkAgg")
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg, NavigationToolbar2Tk
from matplotlib.figure import Figure

import catalog
import examples
from controller import RunConfig, RunResult, find_solver, run_simulation

FIELD_LABELS = {
    "density": "Densidad ρ",
    "velocity": "Velocidad u",
    "pressure": "Presión p",
    "energy": "Energía E (EOS)",
    "density_error": "Error |ρ_num − ρ_ex|",
    "velocity_error": "Error |u_num − u_ex|",
    "pressure_error": "Error |p_num − p_ex|",
}

CASE_CHOICES = [
    "1 — Toro 1 (Sod modificado)",
    "2 — Toro 2 (casi vacío)",
    "3 — Toro 3 (blast izquierdo)",
    "4 — Toro 4 (colisión de choques)",
    "5 — Toro 5 (contacto estacionario)",
    "6 — Onda de entropía (periódica)",
    "7 — Atmósfera hidrostática",
    "8 — Sedov esférico",
]


def _case_id(choice: str) -> int:
    return int(choice.split("—", 1)[0].strip())


class LabApp(tk.Tk):
    def __init__(self) -> None:
        super().__init__()
        self.title("FVLab — laboratorio Euler 1D (prototipo)")
        self.geometry("1400x820")
        self.minsize(1100, 680)

        self.history: list[RunResult] = []
        self.compare: RunResult | None = None
        self.current: RunResult | None = None
        self.queue: queue.Queue = queue.Queue()
        self.busy = False
        self.solver_path = self._detect_solver()

        self._build()
        self._load_example(examples.GUIDED[0])
        self.show_topic("hllc")
        self.after(120, self._poll)
        self.protocol("WM_DELETE_WINDOW", self.destroy)

    def _detect_solver(self) -> str:
        try:
            return str(find_solver())
        except FileNotFoundError as error:
            return f"NO ENCONTRADO. {error}"

    def _build(self) -> None:
        self.columnconfigure(0, weight=1)
        self.rowconfigure(0, weight=1)

        root = ttk.Panedwindow(self, orient=tk.HORIZONTAL)
        root.grid(row=0, column=0, sticky="nsew")

        left = ttk.Frame(root, padding=6)
        center = ttk.Frame(root, padding=4)
        right = ttk.Frame(root, padding=6)
        root.add(left, weight=1)
        root.add(center, weight=3)
        root.add(right, weight=2)

        self._build_left(left)
        self._build_center(center)
        self._build_right(right)

        status = ttk.Frame(self, padding=(8, 2))
        status.grid(row=1, column=0, sticky="ew")
        self.status_var = tk.StringVar(value=f"Núcleo: {self.solver_path}")
        ttk.Label(status, textvariable=self.status_var, wraplength=1300).pack(anchor="w")

    def _build_left(self, parent: ttk.Frame) -> None:
        parent.columnconfigure(0, weight=1)
        parent.rowconfigure(1, weight=1)

        ttk.Label(parent, text="Experimento", font=("Segoe UI", 11, "bold")).grid(
            row=0, column=0, sticky="w"
        )
        notebook = ttk.Notebook(parent)
        notebook.grid(row=1, column=0, sticky="nsew", pady=(4, 0))

        cfg = ttk.Frame(notebook, padding=6)
        ex = ttk.Frame(notebook, padding=6)
        notebook.add(cfg, text="Métodos")
        notebook.add(ex, text="Ejemplos")
        self._build_config(cfg)
        self._build_examples(ex)

        btns = ttk.Frame(parent)
        btns.grid(row=2, column=0, sticky="ew", pady=8)
        self.run_btn = ttk.Button(btns, text="Ejecutar", command=self._on_run)
        self.run_btn.pack(fill="x", pady=2)
        ttk.Button(btns, text="Guardar para comparar", command=self._pin_compare).pack(
            fill="x", pady=2
        )
        ttk.Button(btns, text="Quitar comparación", command=self._clear_compare).pack(
            fill="x", pady=2
        )
        ttk.Button(btns, text="Abrir carpeta de salida", command=self._open_output).pack(
            fill="x", pady=2
        )

    def _entry(self, parent: ttk.Frame, row: int, label: str, var: tk.Variable) -> None:
        ttk.Label(parent, text=label).grid(row=row, column=0, sticky="w", pady=1)
        ttk.Entry(parent, textvariable=var, width=14).grid(row=row, column=1, sticky="ew", pady=1)

    def _build_config(self, parent: ttk.Frame) -> None:
        parent.columnconfigure(1, weight=1)
        self.case_var = tk.StringVar(value=CASE_CHOICES[0])
        self.solver_var = tk.StringVar(value="hllc")
        self.limiter_var = tk.StringVar(value="vl")
        self.bc_var = tk.StringVar(value="trans")
        self.n_var = tk.StringVar(value="200")
        self.cfl_var = tk.StringVar(value="0.9")
        self.gamma_var = tk.StringVar(value="1.4")
        self.tf_var = tk.StringVar(value="0.2")
        self.x0_var = tk.StringVar(value="0.3")
        self.xmin_var = tk.StringVar(value="0.0")
        self.xmax_var = tk.StringVar(value="1.0")
        self.grav_var = tk.StringVar(value="0.0")
        self.alpha_var = tk.StringVar(value="0")
        self.every_var = tk.StringVar(value="0")
        self.rho_floor_var = tk.StringVar(value="1e-12")
        self.p_floor_var = tk.StringVar(value="1e-12")

        row = 0
        ttk.Label(parent, text="Caso de prueba").grid(row=row, column=0, sticky="w")
        case = ttk.Combobox(
            parent, textvariable=self.case_var, values=CASE_CHOICES, state="readonly", width=28
        )
        case.grid(row=row, column=1, sticky="ew")
        case.bind("<<ComboboxSelected>>", lambda _e: self._on_case_changed())
        row += 1

        ttk.Label(parent, text="Riemann").grid(row=row, column=0, sticky="w")
        sol = ttk.Combobox(
            parent, textvariable=self.solver_var, values=["hll", "hllc"], state="readonly"
        )
        sol.grid(row=row, column=1, sticky="ew")
        sol.bind("<<ComboboxSelected>>", lambda _e: self.show_topic(self.solver_var.get()))
        row += 1

        ttk.Label(parent, text="Limitador").grid(row=row, column=0, sticky="w")
        lim = ttk.Combobox(
            parent,
            textvariable=self.limiter_var,
            values=["fo", "mm", "vl", "mc", "sb"],
            state="readonly",
        )
        lim.grid(row=row, column=1, sticky="ew")
        lim.bind("<<ComboboxSelected>>", lambda _e: self.show_topic(self.limiter_var.get()))
        row += 1

        ttk.Label(parent, text="Frontera").grid(row=row, column=0, sticky="w")
        ttk.Combobox(
            parent, textvariable=self.bc_var, values=["trans", "per", "refl"], state="readonly"
        ).grid(row=row, column=1, sticky="ew")
        row += 1

        ttk.Label(parent, text="Integrador").grid(row=row, column=0, sticky="w")
        ttk.Label(parent, text="SSP-RK2 (fijo en el CLI)").grid(row=row, column=1, sticky="w")
        row += 1
        ttk.Label(parent, text="Reconstrucción").grid(row=row, column=0, sticky="w")
        ttk.Label(parent, text="MUSCL; fo = orden 1").grid(row=row, column=1, sticky="w")
        row += 1

        for label, var in (
            ("Celdas N", self.n_var),
            ("CFL", self.cfl_var),
            ("gamma", self.gamma_var),
            ("t final", self.tf_var),
            ("diafragma x0", self.x0_var),
            ("xmin", self.xmin_var),
            ("xmax", self.xmax_var),
        ):
            self._entry(parent, row, label, var)
            row += 1

        adv = ttk.LabelFrame(parent, text="Avanzado", padding=4)
        adv.grid(row=row, column=0, columnspan=2, sticky="ew", pady=(8, 0))
        adv.columnconfigure(1, weight=1)
        arow = 0
        for label, var in (
            ("gravedad", self.grav_var),
            ("alpha_geo", self.alpha_var),
            ("every_out", self.every_var),
            ("rho_floor", self.rho_floor_var),
            ("p_floor", self.p_floor_var),
        ):
            ttk.Label(adv, text=label).grid(row=arow, column=0, sticky="w")
            ttk.Entry(adv, textvariable=var, width=12).grid(row=arow, column=1, sticky="ew")
            arow += 1

        row += 1
        self.ic_text = tk.Text(parent, height=7, wrap="word", font=("Consolas", 9))
        self.ic_text.grid(row=row, column=0, columnspan=2, sticky="nsew", pady=(8, 0))
        parent.rowconfigure(row, weight=1)

    def _build_examples(self, parent: ttk.Frame) -> None:
        parent.columnconfigure(0, weight=1)
        ttk.Label(
            parent,
            text="Carga un experimento, lee qué observar, ejecuta y cambia un solo método.",
            wraplength=280,
        ).grid(row=0, column=0, sticky="w")
        self.example_list = tk.Listbox(parent, height=8, exportselection=False)
        self.example_list.grid(row=1, column=0, sticky="nsew", pady=6)
        parent.rowconfigure(1, weight=1)
        for item in examples.GUIDED:
            self.example_list.insert("end", item.title)
        self.example_list.selection_set(0)
        ttk.Button(parent, text="Cargar ejemplo", command=self._on_load_example).grid(
            row=2, column=0, sticky="ew"
        )
        self.example_help = tk.Text(parent, height=12, wrap="word", font=("Segoe UI", 9))
        self.example_help.grid(row=3, column=0, sticky="nsew", pady=(6, 0))
        parent.rowconfigure(3, weight=1)

    def _build_center(self, parent: ttk.Frame) -> None:
        parent.columnconfigure(0, weight=1)
        parent.rowconfigure(1, weight=3)
        parent.rowconfigure(3, weight=1)

        top = ttk.Frame(parent)
        top.grid(row=0, column=0, sticky="ew")
        ttk.Label(top, text="Variable").pack(side="left")
        self.field_var = tk.StringVar(value="density")
        fields = ttk.Combobox(
            top,
            textvariable=self.field_var,
            values=list(FIELD_LABELS.keys()),
            state="readonly",
            width=22,
        )
        fields.pack(side="left", padx=6)
        fields.bind("<<ComboboxSelected>>", lambda _e: self._redraw())
        self.field_caption = ttk.Label(top, text=FIELD_LABELS["density"])
        self.field_caption.pack(side="left")

        self.fig = Figure(figsize=(6.4, 3.6), dpi=100)
        self.ax = self.fig.add_subplot(111)
        self.ax.set_xlabel("x")
        self.ax.grid(True, alpha=0.3)
        self.canvas = FigureCanvasTkAgg(self.fig, master=parent)
        self.canvas.get_tk_widget().grid(row=1, column=0, sticky="nsew")
        toolbar_host = ttk.Frame(parent)
        toolbar_host.grid(row=2, column=0, sticky="ew")
        NavigationToolbar2Tk(self.canvas, toolbar_host)

        bottom = ttk.Panedwindow(parent, orient=tk.HORIZONTAL)
        bottom.grid(row=3, column=0, sticky="nsew", pady=(6, 0))
        metrics = ttk.LabelFrame(bottom, text="Métricas", padding=4)
        logf = ttk.LabelFrame(bottom, text="Log del núcleo", padding=4)
        bottom.add(metrics, weight=1)
        bottom.add(logf, weight=2)
        self.metrics_text = tk.Text(metrics, height=8, wrap="word", font=("Consolas", 9))
        self.metrics_text.pack(fill="both", expand=True)
        self.log_text = tk.Text(logf, height=8, wrap="word", font=("Consolas", 8))
        self.log_text.pack(fill="both", expand=True)

    def _build_right(self, parent: ttk.Frame) -> None:
        parent.columnconfigure(0, weight=1)
        parent.rowconfigure(2, weight=1)
        ttk.Label(parent, text="Qué está haciendo el solver", font=("Segoe UI", 11, "bold")).grid(
            row=0, column=0, sticky="w"
        )
        titles = [catalog.TOPICS[tid].title for tid in catalog.TOPIC_ORDER]
        self.topic_var = tk.StringVar(value=titles[0])
        combo = ttk.Combobox(parent, textvariable=self.topic_var, values=titles, state="readonly")
        combo.grid(row=1, column=0, sticky="ew", pady=4)
        combo.bind("<<ComboboxSelected>>", lambda _e: self._on_topic_combo())

        self.manual = tk.Text(parent, wrap="word", font=("Segoe UI", 9))
        self.manual.grid(row=2, column=0, sticky="nsew")
        self.manual.tag_configure("h", font=("Segoe UI", 10, "bold"))
        self.manual.tag_configure("code", font=("Consolas", 8))
        self.manual.tag_configure("eq", font=("Consolas", 9))

    def _on_topic_combo(self) -> None:
        title = self.topic_var.get()
        for tid in catalog.TOPIC_ORDER:
            if catalog.TOPICS[tid].title == title:
                self.show_topic(tid)
                return

    def show_topic(self, topic_id: str) -> None:
        topic = catalog.TOPICS.get(topic_id)
        if topic is None:
            return
        self.topic_var.set(topic.title)
        src = catalog.read_source(topic.source)
        self.manual.delete("1.0", "end")
        blocks: list[tuple[str, str]] = [
            ("h", topic.title + "\n\n"),
            ("", topic.summary + "\n\n"),
            ("h", "Por qué se usa\n"),
            ("", topic.why + "\n\n"),
            ("h", "Ecuaciones\n"),
            ("eq", topic.equations + "\n\n"),
            ("h", "Algoritmo (el del código)\n"),
            ("", topic.algorithm + "\n\n"),
            ("h", "Qué observar\n"),
            ("", topic.observe + "\n\n"),
            ("h", "Experimento sugerido\n"),
            ("", topic.experiment + "\n\n"),
            ("h", f"Implementación real  {topic.source.relpath}\n"),
            ("code", src + "\n"),
        ]
        for tag, text in blocks:
            self.manual.insert("end", text, tag if tag else ())

    def _on_case_changed(self) -> None:
        cid = _case_id(self.case_var.get())
        info = examples.CASES[cid]
        self.x0_var.set(str(info["x0"]))
        self.tf_var.set(str(info["tf"]))
        if cid == 6:
            self.bc_var.set("per")
            self.show_topic("muscl")
        elif cid == 7:
            self.bc_var.set("refl")
            self.grav_var.set("1.0")
            self.show_topic("source")
        elif cid == 8:
            self.bc_var.set("refl")
            self.alpha_var.set("2")
            self.xmax_var.set("1.2")
            self.show_topic("source")
        else:
            self.show_topic(self.solver_var.get())
        self._refresh_ic()

    def _refresh_ic(self) -> None:
        cid = _case_id(self.case_var.get())
        text = examples.format_state(cid)
        text += (
            "\n\nLos estados L/R los fija el caso en test_case.cpp. "
            "El prototipo no inventa un segundo inicializador."
        )
        self.ic_text.delete("1.0", "end")
        self.ic_text.insert("1.0", text)

    def _on_load_example(self) -> None:
        sel = self.example_list.curselection()
        if not sel:
            return
        self._load_example(examples.GUIDED[sel[0]])

    def _load_example(self, item: examples.GuidedExample) -> None:
        cfg = item.config
        self.case_var.set(CASE_CHOICES[cfg.test - 1])
        self.solver_var.set(cfg.solver)
        self.limiter_var.set(cfg.limiter)
        self.bc_var.set(cfg.bc)
        self.n_var.set(str(cfg.n))
        self.cfl_var.set(str(cfg.cfl))
        self.gamma_var.set(str(cfg.gamma))
        self.tf_var.set(str(cfg.tf if cfg.tf > 0 else examples.CASES[cfg.test]["tf"]))
        self.x0_var.set(str(cfg.x0 if cfg.x0 > 0 else examples.CASES[cfg.test]["x0"]))
        self.xmin_var.set(str(cfg.xmin))
        self.xmax_var.set(str(cfg.xmax))
        self._refresh_ic()
        self.example_help.delete("1.0", "end")
        self.example_help.insert(
            "1.0",
            f"{item.title}\n\n"
            f"Problema:\n{item.problem}\n\n"
            f"Condición inicial:\n{item.initial}\n\n"
            f"Experimento recomendado:\n{item.recommended}\n\n"
            f"Observa:\n{item.observe}\n",
        )
        self.show_topic(item.open_topic)
        self.status_var.set(f"Ejemplo cargado: {item.title}")

    def _parse_config(self) -> RunConfig:
        try:
            return RunConfig(
                test=_case_id(self.case_var.get()),
                n=int(float(self.n_var.get())),
                cfl=float(self.cfl_var.get()),
                gamma=float(self.gamma_var.get()),
                limiter=self.limiter_var.get(),
                solver=self.solver_var.get(),
                bc=self.bc_var.get(),
                xmin=float(self.xmin_var.get()),
                xmax=float(self.xmax_var.get()),
                x0=float(self.x0_var.get()),
                tf=float(self.tf_var.get()),
                every_out=int(float(self.every_var.get())),
                grav=float(self.grav_var.get()),
                alpha_geo=int(float(self.alpha_var.get())),
                rho_floor=float(self.rho_floor_var.get()),
                p_floor=float(self.p_floor_var.get()),
            )
        except ValueError as error:
            raise ValueError(f"parámetro no numérico: {error}") from error

    def _on_run(self) -> None:
        if self.busy:
            return
        if "NO ENCONTRADO" in self.solver_path:
            messagebox.showerror("Núcleo ausente", self.solver_path)
            return
        try:
            config = self._parse_config()
        except ValueError as error:
            messagebox.showerror("Configuración", str(error))
            return
        self.busy = True
        self.run_btn.configure(state="disabled", text="Ejecutando…")
        self.status_var.set("Lanzando xEuler1D (el núcleo C++, no un solver Python)…")

        def worker() -> None:
            try:
                result = run_simulation(config)
                self.queue.put(("ok", result))
            except Exception as error:  # noqa: BLE001 — la GUI debe mostrar cualquier fallo
                self.queue.put(("err", str(error)))

        threading.Thread(target=worker, daemon=True).start()

    def _poll(self) -> None:
        try:
            while True:
                kind, payload = self.queue.get_nowait()
                if kind == "ok":
                    self._accept_result(payload)
                else:
                    messagebox.showerror("Fallo al ejecutar", payload)
                    self.status_var.set(payload)
                self.busy = False
                self.run_btn.configure(state="normal", text="Ejecutar")
        except queue.Empty:
            pass
        self.after(120, self._poll)

    def _accept_result(self, result: RunResult) -> None:
        self.current = result
        self.history.append(result)
        self.log_text.delete("1.0", "end")
        text = result.stdout
        if result.stderr:
            text += "\n--- stderr ---\n" + result.stderr
        self.log_text.insert("1.0", text or "(sin salida)")
        if result.returncode != 0:
            messagebox.showerror(
                "El núcleo rechazó la corrida",
                f"código {result.returncode}\n\n{result.stderr or result.stdout}",
            )
            self.status_var.set(f"Error {result.returncode}")
            return
        self._write_metrics(result)
        self._redraw()
        extra = f"  {result.star_line}" if result.star_line else ""
        self.status_var.set(
            f"Listo: {result.label}   pasos={result.steps}   t={result.final_time}{extra}"
        )

    def _write_metrics(self, result: RunResult) -> None:
        lines = [
            f"etiqueta: {result.label}",
            f"pasos: {result.steps}    t final: {result.final_time}",
            f"salida: {result.output_dir}",
        ]
        if result.star_line:
            lines.append(result.star_line)
        if result.errors:
            lines.append("error vs Riemann exacto:")
            for key, mets in result.errors.items():
                lines.append(
                    f"  {key:10s}  max|Δ|={mets['max_abs']:.4e}  L1={mets['l1']:.4e}  L2={mets['l2']:.4e}"
                )
        else:
            lines.append("sin referencia exacta en esta corrida (o no se escribió *_exact.xl).")
        if self.compare is not None:
            lines.append(f"comparando contra: {self.compare.label}")
        self.metrics_text.delete("1.0", "end")
        self.metrics_text.insert("1.0", "\n".join(lines))

    def _pin_compare(self) -> None:
        if self.current is None:
            messagebox.showinfo("Comparar", "Primero ejecuta una simulación.")
            return
        self.compare = self.current
        self.status_var.set(f"Ancla de comparación: {self.compare.label}. Cambia un método y vuelve a ejecutar.")
        self._redraw()

    def _clear_compare(self) -> None:
        self.compare = None
        self._redraw()

    def _open_output(self) -> None:
        if self.current is None:
            return
        path = self.current.output_dir
        if os.name == "nt":
            os.startfile(path)  # type: ignore[attr-defined]
        else:
            os.system(f'xdg-open "{path}"')

    def _series(self, result: RunResult) -> tuple[list[float], list[float], list[float] | None]:
        key = self.field_var.get()
        field = result.fields.get(key)
        if field is None:
            return [], [], None
        return field.x, field.numerical, field.exact

    def _redraw(self) -> None:
        key = self.field_var.get()
        self.field_caption.configure(text=FIELD_LABELS.get(key, key))
        self.ax.clear()
        self.ax.grid(True, alpha=0.3)
        self.ax.set_xlabel("x")
        self.ax.set_ylabel(FIELD_LABELS.get(key, key))
        plotted = False
        if self.compare is not None:
            x, y, exact = self._series(self.compare)
            if x:
                self.ax.plot(x, y, color="C1", lw=1.6, label=self.compare.label)
                plotted = True
            if exact:
                self.ax.plot(x, exact, color="0.2", ls="--", lw=1.0, label="exacta (ancla)")
        if self.current is not None:
            x, y, exact = self._series(self.current)
            if x:
                self.ax.plot(x, y, color="C0", lw=1.8, label=self.current.label)
                plotted = True
            if exact and self.compare is None:
                self.ax.plot(x, exact, color="0.15", ls="--", lw=1.1, label="Riemann exacto")
        if plotted:
            self.ax.legend(loc="best", fontsize=8)
        else:
            self.ax.set_title("Ejecuta una simulación para ver el resultado")
        self.fig.tight_layout()
        self.canvas.draw_idle()


def launch() -> None:
    app = LabApp()
    app.mainloop()
