"""Windows desktop score editor for Paradigm: Origin."""

from __future__ import annotations

import math
from pathlib import Path
import tkinter as tk
from tkinter import filedialog, messagebox, ttk

from score_data import EDGES, GRID_HEIGHT, GRID_WIDTH, ChartNote, ScoreChart, edge_position

BG = "#101715"
PANEL = "#18211d"
RAISED = "#202c26"
GRID = "#34453b"
TEXT = "#f1f0e6"
MUTED = "#91a497"
MINT = "#83f0b2"
CYAN = "#84cde0"
AMBER = "#eeae6a"


class OriginScoreEditor(tk.Tk):
    def __init__(self) -> None:
        super().__init__()
        self.title("范式：起源 · 制谱器")
        self.geometry("1180x800")
        self.minsize(900, 610)
        self.configure(bg=BG)
        self.option_add("*Font", ("Microsoft YaHei UI", 10))

        self.chart = ScoreChart()
        self.file_path: Path | None = None
        self.selected_index: int | None = None
        self.tool = tk.StringVar(value="select")
        self.note_kind = tk.StringVar(value="tap")
        self.edge = tk.IntVar(value=0)
        self.tick = tk.StringVar(value="0")
        self.fake = tk.BooleanVar(value=False)
        self.status = tk.StringVar(value="就绪　·　12 × 9 判面")
        self.dirty = False
        self._dragging = False
        self._drag_moved = False
        self._drag_index: int | None = None

        self._style_widgets()
        self._build_header()
        self._build_layout()
        self._bind_shortcuts()
        self.protocol("WM_DELETE_WINDOW", self._close)
        self.after(60, self._draw_grid)

    def _style_widgets(self) -> None:
        style = ttk.Style(self)
        style.theme_use("clam")
        style.configure("Editor.TCombobox", fieldbackground=RAISED, background=RAISED, foreground=TEXT, arrowcolor=MINT)
        style.map("Editor.TCombobox", fieldbackground=[("readonly", RAISED)], foreground=[("readonly", TEXT)])
        style.configure("Note.Treeview", background=PANEL, foreground=TEXT, fieldbackground=PANEL, rowheight=33, borderwidth=0)
        style.configure("Note.Treeview.Heading", background=RAISED, foreground=MUTED, relief="flat")
        style.map("Note.Treeview", background=[("selected", "#2d4939")], foreground=[("selected", TEXT)])

    def _label(self, parent: tk.Misc, text: str, *, fg: str = MUTED, size: int = 9) -> tk.Label:
        return tk.Label(parent, text=text, bg=PANEL, fg=fg, font=("Microsoft YaHei UI", size))

    def _button(self, parent: tk.Misc, text: str, command, *, primary: bool = False) -> tk.Button:
        button = tk.Button(
            parent,
            text=text,
            command=command,
            bg=MINT if primary else RAISED,
            fg="#122019" if primary else TEXT,
            activebackground="#a5f8c8" if primary else "#34473a",
            activeforeground="#122019" if primary else TEXT,
            relief="flat",
            bd=0,
            padx=13,
            pady=9,
            cursor="hand2",
            font=("Microsoft YaHei UI", 9, "bold"),
        )
        return button

    def _build_header(self) -> None:
        header = tk.Frame(self, bg=BG, padx=25, pady=15)
        header.pack(fill="x")
        title = tk.Label(header, text="范式：起源", fg=TEXT, bg=BG, font=("Microsoft YaHei UI", 19, "bold"))
        title.pack(side="left")
        tk.Label(header, text="/  SCORE EDITOR", fg=MINT, bg=BG, font=("Consolas", 9, "bold")).pack(
            side="left", padx=(10, 0), pady=(8, 0)
        )
        tk.Label(header, textvariable=self.status, fg=MUTED, bg=BG, font=("Consolas", 9)).pack(
            side="right", pady=(8, 0)
        )

    def _build_layout(self) -> None:
        toolbar = tk.Frame(self, bg=PANEL, padx=13, pady=10)
        toolbar.pack(fill="x", padx=17)
        self._button(toolbar, "新建", self.new_chart).pack(side="left", padx=(0, 7))
        self._button(toolbar, "打开 JSON", self.open_chart).pack(side="left", padx=4)
        self._button(toolbar, "另存为", self.save_chart, primary=True).pack(side="left", padx=4)
        tk.Frame(toolbar, bg=GRID, width=1, height=26).pack(side="left", padx=13)
        tk.Label(toolbar, text="谱面", bg=PANEL, fg=MUTED, font=("Microsoft YaHei UI", 9)).pack(side="left", padx=(0, 8))
        self.title_var = tk.StringVar(value=self.chart.title)
        title_entry = tk.Entry(
            toolbar,
            textvariable=self.title_var,
            width=22,
            bg=RAISED,
            fg=TEXT,
            insertbackground=MINT,
            relief="flat",
            highlightthickness=1,
            highlightbackground=GRID,
            highlightcolor=MINT,
        )
        title_entry.pack(side="left", ipady=6)
        title_entry.bind("<KeyRelease>", self._title_changed)

        body = tk.Frame(self, bg=BG, padx=17, pady=15)
        body.pack(fill="both", expand=True)
        work = tk.Frame(body, bg=PANEL, padx=16, pady=14)
        work.pack(side="left", fill="both", expand=True)
        side = tk.Frame(body, bg=PANEL, width=300, padx=14, pady=14)
        side.pack(side="right", fill="y", padx=(13, 0))
        side.pack_propagate(False)

        caption = tk.Frame(work, bg=PANEL)
        caption.pack(fill="x", pady=(0, 10))
        tk.Label(caption, text="JUDGE PLANE", bg=PANEL, fg=MUTED, font=("Consolas", 10, "bold")).pack(side="left")
        tk.Label(caption, text="12 × 9  /  半开矩形判定", bg=PANEL, fg=MUTED, font=("Microsoft YaHei UI", 9)).pack(side="right")
        grid_frame = tk.Frame(work, bg=BG)
        grid_frame.pack(fill="both", expand=True)
        self.canvas = tk.Canvas(grid_frame, bg=BG, highlightthickness=0, cursor="crosshair")
        self.canvas.pack(fill="both", expand=True)
        self.canvas.bind("<Configure>", lambda _event: self._draw_grid())
        self.canvas.bind("<Button-1>", self._canvas_press)
        self.canvas.bind("<B1-Motion>", self._canvas_drag)
        self.canvas.bind("<ButtonRelease-1>", self._canvas_release)
        legend = tk.Frame(work, bg=PANEL, pady=10)
        legend.pack(fill="x")
        tk.Label(legend, text="●", fg=CYAN, bg=PANEL).pack(side="left")
        tk.Label(legend, text="判面音符", fg=MUTED, bg=PANEL).pack(side="left", padx=(4, 14))
        tk.Label(legend, text="◆", fg=MINT, bg=PANEL).pack(side="left")
        tk.Label(legend, text="边线音符", fg=MUTED, bg=PANEL).pack(side="left", padx=(4, 14))
        tk.Label(legend, text="◇ 假音符　·　点击画布放置 / 拖动选中音符", fg=MUTED, bg=PANEL).pack(side="right")

        self._label(side, "TOOLS  /  工具", size=9).pack(anchor="w")
        self._tool_button(side, "↖  选取 / 移动", "select").pack(fill="x", pady=(8, 4))
        self._tool_button(side, "●  判面音符", "space").pack(fill="x", pady=4)
        self._tool_button(side, "◆  边线音符", "edge").pack(fill="x", pady=4)

        self._label(side, "NOTE  /  音符属性", size=9).pack(anchor="w", pady=(18, 9))
        self._label(side, "音符种类").pack(anchor="w", pady=(0, 5))
        self.kind_combo = ttk.Combobox(
            side, textvariable=self.note_kind, values=("tap", "link", "slider"), state="readonly", style="Editor.TCombobox"
        )
        self.kind_combo.pack(fill="x", ipady=4)
        self._label(side, "边线位置", fg=MUTED).pack(anchor="w", pady=(10, 5))
        self.edge_combo = ttk.Combobox(side, values=EDGES, state="readonly", style="Editor.TCombobox")
        self.edge_combo.current(0)
        self.edge_combo.pack(fill="x", ipady=4)
        self.edge_combo.bind("<<ComboboxSelected>>", self._edge_selected)
        self._label(side, "tick  /  谱面原始刻度", fg=MUTED).pack(anchor="w", pady=(10, 5))
        self.tick_entry = tk.Entry(side, textvariable=self.tick, bg=RAISED, fg=TEXT, insertbackground=MINT, relief="flat")
        self.tick_entry.pack(fill="x", ipady=8)
        self.fake_check = tk.Checkbutton(
            side,
            text="假音符（不参与判定）",
            variable=self.fake,
            bg=PANEL,
            fg=TEXT,
            activebackground=PANEL,
            activeforeground=MINT,
            selectcolor=RAISED,
            highlightthickness=0,
        )
        self.fake_check.pack(anchor="w", pady=(8, 0))
        self._button(side, "应用属性到选中音符", self.apply_properties, primary=True).pack(fill="x", pady=(11, 5))

        row = tk.Frame(side, bg=PANEL)
        row.pack(fill="x", pady=(7, 7))
        self._label(row, "NOTES  /  音符列表", size=9).pack(side="left")
        self.count_label = self._label(row, "0 NOTES", fg=MINT, size=9)
        self.count_label.pack(side="right")
        columns = ("type", "time", "position")
        self.note_list = ttk.Treeview(side, columns=columns, show="headings", height=9, style="Note.Treeview", selectmode="browse")
        self.note_list.heading("type", text="种类")
        self.note_list.heading("time", text="tick")
        self.note_list.heading("position", text="位置")
        self.note_list.column("type", width=78, stretch=True, anchor="w")
        self.note_list.column("time", width=52, stretch=False, anchor="center")
        self.note_list.column("position", width=102, stretch=False, anchor="center")
        self.note_list.pack(fill="x")
        self.note_list.bind("<<TreeviewSelect>>", self._list_selected)
        self.note_list.bind("<Delete>", lambda _event: self.delete_selected())
        actions = tk.Frame(side, bg=PANEL)
        actions.pack(fill="x", pady=(8, 0))
        self._button(actions, "复制", self.duplicate_selected).pack(side="left", fill="x", expand=True, padx=(0, 4))
        self._button(actions, "删除", self.delete_selected).pack(side="left", fill="x", expand=True, padx=(4, 0))

    def _tool_button(self, parent: tk.Misc, text: str, tool: str) -> tk.Radiobutton:
        return tk.Radiobutton(
            parent,
            text=text,
            variable=self.tool,
            value=tool,
            command=self._tool_changed,
            bg=RAISED,
            fg=TEXT,
            selectcolor="#294333",
            activebackground=RAISED,
            activeforeground=MINT,
            indicatoron=False,
            anchor="w",
            padx=11,
            pady=8,
            cursor="hand2",
            relief="flat",
        )

    def _bind_shortcuts(self) -> None:
        self.bind("<Control-o>", lambda _event: self.open_chart())
        self.bind("<Control-s>", lambda _event: self.save_chart())
        self.bind("<Control-d>", lambda _event: self.duplicate_selected())
        self.bind("<Delete>", lambda _event: self.delete_selected())

    def _tool_changed(self) -> None:
        self.canvas.configure(cursor="crosshair" if self.tool.get() == "select" else "plus")

    def _edge_selected(self, _event=None) -> None:
        self.edge.set(self.edge_combo.current())

    def _title_changed(self, _event=None) -> None:
        title = self.title_var.get().strip()
        if title:
            self.chart.title = title
            self._mark_dirty()

    def _mark_dirty(self) -> None:
        self.dirty = True
        self.status.set(f"未保存　·　12 × 9 判面　·　{len(self.chart.notes)} 个音符")

    def _position(self) -> tuple[float, float, float, float]:
        width = max(100, self.canvas.winfo_width())
        height = max(100, self.canvas.winfo_height())
        unit = min((width - 76) / GRID_WIDTH, (height - 66) / GRID_HEIGHT)
        left = (width - unit * GRID_WIDTH) / 2
        top = (height - unit * GRID_HEIGHT) / 2
        return left, top, unit, unit

    def _draw_grid(self) -> None:
        if not hasattr(self, "canvas"):
            return
        canvas = self.canvas
        canvas.delete("all")
        left, top, unit_x, unit_y = self._position()
        right, bottom = left + GRID_WIDTH * unit_x, top + GRID_HEIGHT * unit_y
        for column in range(GRID_WIDTH + 1):
            x = left + column * unit_x
            major = column % 3 == 0
            canvas.create_line(x, top, x, bottom, fill=GRID if major else "#24332a", width=1.2 if major else 0.7)
            if column < GRID_WIDTH and column % 3 == 0:
                canvas.create_text(x + unit_x, bottom + 15, text=f"{column}", fill=MUTED, font=("Consolas", 9), anchor="n")
        for row in range(GRID_HEIGHT + 1):
            y = top + row * unit_y
            major = row % 3 == 0
            canvas.create_line(left, y, right, y, fill=GRID if major else "#24332a", width=1.2 if major else 0.7)
            if row < GRID_HEIGHT and row % 3 == 0:
                canvas.create_text(left - 12, y, text=f"{GRID_HEIGHT - row}", fill=MUTED, font=("Consolas", 9), anchor="e")
        canvas.create_rectangle(left, top, right, bottom, outline="#788f7c", width=1.5)
        for index, note in enumerate(self.chart.notes):
            self._draw_note(index, note, left, top, unit_x, unit_y)

    def _draw_note(self, index: int, note: ChartNote, left: float, top: float, unit_x: float, unit_y: float) -> None:
        x, y = note.coordinates()
        px, py = left + x * unit_x, top + (GRID_HEIGHT - y) * unit_y
        radius = max(6, min(unit_x, unit_y) * 0.19)
        color = MINT if note.event_type == "EdgeNote" else CYAN
        is_selected = index == self.selected_index
        if note.is_fake:
            self.canvas.create_oval(px - radius, py - radius, px + radius, py + radius, outline=AMBER, width=2, dash=(3, 2))
        else:
            self.canvas.create_oval(
                px - radius, py - radius, px + radius, py + radius,
                fill=color, outline=TEXT if is_selected else color, width=3 if is_selected else 1,
            )
        if note.event_type == "EdgeNote":
            self.canvas.create_polygon(
                px, py - radius * 0.8, px + radius * 0.8, py,
                px, py + radius * 0.8, px - radius * 0.8, py,
                fill="" if note.is_fake else color, outline=AMBER if note.is_fake else color, width=2,
            )
        if is_selected:
            self.canvas.create_text(px + radius + 5, py - radius - 5, text=f"{note.kind} · {note.tick}", fill=TEXT, anchor="sw", font=("Microsoft YaHei UI", 9))

    def _canvas_to_grid(self, px: float, py: float) -> tuple[float, float] | None:
        left, top, unit_x, unit_y = self._position()
        x, y = (px - left) / unit_x, GRID_HEIGHT - (py - top) / unit_y
        if not (0 <= x <= GRID_WIDTH and 0 <= y <= GRID_HEIGHT):
            return None
        return x, y

    def _nearest_note(self, px: float, py: float) -> int | None:
        left, top, unit_x, unit_y = self._position()
        best: tuple[float, int] | None = None
        hit_radius = max(11.0, min(unit_x, unit_y) * 0.30)
        for index, note in enumerate(self.chart.notes):
            x, y = note.coordinates()
            distance = math.hypot(left + x * unit_x - px, top + (GRID_HEIGHT - y) * unit_y - py)
            if distance <= hit_radius and (best is None or distance < best[0]):
                best = (distance, index)
        return best[1] if best else None

    def _canvas_press(self, event: tk.Event) -> None:
        self._drag_moved = False
        self._drag_index = None
        index = self._nearest_note(event.x, event.y)
        if self.tool.get() == "select":
            if index is None:
                self.selected_index = None
                self._sync_list()
                self._draw_grid()
                return
            self.selected_index = index
            self._drag_index = index
            self._dragging = True
            self._load_properties(index)
            self._sync_list()
            self._draw_grid()
            return
        position = self._canvas_to_grid(event.x, event.y)
        if position is not None:
            self._place_note(*position)

    def _canvas_drag(self, event: tk.Event) -> None:
        if not self._dragging or self._drag_index is None:
            return
        position = self._canvas_to_grid(event.x, event.y)
        if position is None:
            return
        note = self.chart.notes[self._drag_index]
        if note.event_type == "EdgeNote":
            note.pos = edge_position(note.edge or 0, *position)
        else:
            note.x, note.y = position
        self._drag_moved = True
        self._draw_grid()

    def _canvas_release(self, _event: tk.Event) -> None:
        if self._dragging:
            self._dragging = False
            if self._drag_moved and self._drag_index is not None:
                self._mark_dirty()
                self._sync_list()
                self._draw_grid()

    def _place_note(self, x: float, y: float) -> None:
        tick = self._read_tick()
        if tick is None:
            return
        if self.tool.get() == "edge":
            edge = self.edge.get()
            pos = edge_position(edge, x, y)
            note = ChartNote("EdgeNote", self.note_kind.get(), tick, self.fake.get(), edge=edge, pos=pos)
        else:
            note = ChartNote("SpaceNote", self.note_kind.get(), tick, self.fake.get(), x=x, y=y)
        self.chart.notes.append(note)
        self.selected_index = len(self.chart.notes) - 1
        self._mark_dirty()
        self._load_properties(self.selected_index)
        self._sync_list()
        self._draw_grid()

    def _read_tick(self) -> int | None:
        value = self.tick.get().strip()
        try:
            return int(value)
        except ValueError:
            messagebox.showerror("无法放置音符", "tick 必须为整数。", parent=self)
            self.tick_entry.focus_set()
            return None

    def _load_properties(self, index: int) -> None:
        note = self.chart.notes[index]
        self.note_kind.set(note.kind)
        self.tick.set(str(note.tick))
        self.fake.set(note.is_fake)
        if note.event_type == "EdgeNote":
            self.edge.set(note.edge or 0)
            self.edge_combo.current(self.edge.get())

    def apply_properties(self) -> None:
        if self.selected_index is None or self.selected_index >= len(self.chart.notes):
            self.status.set("先在判面或音符列表中选择一个音符")
            return
        tick = self._read_tick()
        if tick is None:
            return
        note = self.chart.notes[self.selected_index]
        note.kind = self.note_kind.get()
        note.tick = tick
        note.is_fake = self.fake.get()
        if note.event_type == "EdgeNote":
            note.edge = self.edge.get()
        self._mark_dirty()
        self._sync_list()
        self._draw_grid()

    def _sync_list(self) -> None:
        if not hasattr(self, "note_list"):
            return
        self.note_list.delete(*self.note_list.get_children())
        for index, note in enumerate(self.chart.notes):
            x, y = note.coordinates()
            position = EDGES[note.edge] + f" {note.pos:g}" if note.event_type == "EdgeNote" else f"({x:g}, {y:g})"
            kind = f"{'◇' if note.is_fake else ''}{'边线' if note.event_type == 'EdgeNote' else '判面'}·{note.kind}"
            self.note_list.insert("", "end", iid=str(index), values=(kind, note.tick, position))
        self.count_label.configure(text=f"{len(self.chart.notes)} NOTES")
        if self.selected_index is not None and self.selected_index < len(self.chart.notes):
            self.note_list.selection_set(str(self.selected_index))
            self.note_list.see(str(self.selected_index))
        self._draw_grid()

    def _list_selected(self, _event=None) -> None:
        selection = self.note_list.selection()
        if not selection:
            return
        try:
            index = int(selection[0])
        except ValueError:
            return
        if 0 <= index < len(self.chart.notes):
            self.selected_index = index
            self._load_properties(index)
            self._draw_grid()

    def delete_selected(self) -> None:
        if self.selected_index is None or not (0 <= self.selected_index < len(self.chart.notes)):
            return
        del self.chart.notes[self.selected_index]
        self.selected_index = None
        self._mark_dirty()
        self._sync_list()

    def duplicate_selected(self) -> None:
        if self.selected_index is None or not (0 <= self.selected_index < len(self.chart.notes)):
            return
        original = self.chart.notes[self.selected_index]
        note = ChartNote(**vars(original))
        if note.event_type == "EdgeNote":
            note.pos = min(note.pos + 0.5, GRID_HEIGHT if note.edge in (0, 1) else GRID_WIDTH)
        else:
            note.x = min(note.x + 0.5, GRID_WIDTH)
            note.y = min(note.y + 0.5, GRID_HEIGHT)
        self.chart.notes.append(note)
        self.selected_index = len(self.chart.notes) - 1
        self._mark_dirty()
        self._load_properties(self.selected_index)
        self._sync_list()

    def new_chart(self) -> None:
        if not self._confirm_discard():
            return
        self.chart = ScoreChart()
        self.file_path = None
        self.selected_index = None
        self.title_var.set(self.chart.title)
        self._clear_dirty()
        self._sync_list()

    def open_chart(self) -> None:
        if not self._confirm_discard():
            return
        filename = filedialog.askopenfilename(
            parent=self,
            title="打开谱面 JSON",
            filetypes=(("JSON 谱面", "*.json"), ("所有文件", "*.*")),
        )
        if not filename:
            return
        try:
            chart = ScoreChart.from_json(Path(filename).read_text(encoding="utf-8-sig"))
        except (OSError, ValueError) as exc:
            messagebox.showerror("打开失败", str(exc), parent=self)
            return
        self.chart = chart
        self.file_path = Path(filename)
        self.selected_index = None
        self.title_var.set(chart.title)
        self._clear_dirty()
        self._sync_list()

    def save_chart(self) -> bool:
        self._title_changed()
        if self.file_path is None:
            filename = filedialog.asksaveasfilename(
                parent=self,
                title="保存谱面 JSON",
                defaultextension=".json",
                filetypes=(("JSON 谱面", "*.json"), ("所有文件", "*.*")),
                initialfile=f"{self.chart.title}.json",
            )
            if not filename:
                return False
            self.file_path = Path(filename)
        try:
            self.file_path.write_text(self.chart.to_json(), encoding="utf-8")
        except OSError as exc:
            messagebox.showerror("保存失败", str(exc), parent=self)
            return False
        self._clear_dirty()
        self.status.set(f"已保存　·　{self.file_path.name}　·　{len(self.chart.notes)} 个音符")
        return True

    def _clear_dirty(self) -> None:
        self.dirty = False
        self.status.set(f"就绪　·　12 × 9 判面　·　{len(self.chart.notes)} 个音符")

    def _confirm_discard(self) -> bool:
        if not self.dirty:
            return True
        answer = messagebox.askyesnocancel("谱面尚未保存", "是否先保存当前谱面？", parent=self)
        if answer is None:
            return False
        if answer:
            return self.save_chart()
        return True

    def _close(self) -> None:
        if self._confirm_discard():
            self.destroy()


if __name__ == "__main__":
    OriginScoreEditor().mainloop()
