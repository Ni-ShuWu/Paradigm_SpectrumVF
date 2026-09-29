"""Chart data and JSON serialization for the Paradigm: Origin score editor."""

from __future__ import annotations

from dataclasses import dataclass, field
import json
import math
from typing import Any

GRID_WIDTH = 12
GRID_HEIGHT = 9
FORMAT_NAME = "ParadigmOriginChart"
FORMAT_VERSION = 1
EDGES = ("左边线", "右边线", "上边线", "下边线")


@dataclass
class ChartNote:
    event_type: str
    kind: str = "tap"
    tick: int = 0
    is_fake: bool = False
    edge: int | None = None
    pos: float | None = None
    x: float | None = None
    y: float | None = None

    def coordinates(self) -> tuple[float, float]:
        if self.event_type == "EdgeNote":
            if self.edge == 0:
                return 0.0, float(self.pos)
            if self.edge == 1:
                return float(GRID_WIDTH), float(self.pos)
            if self.edge == 2:
                return float(self.pos), float(GRID_HEIGHT)
            return float(self.pos), 0.0
        return float(self.x), float(self.y)

    def to_dict(self) -> dict[str, Any]:
        result: dict[str, Any] = {
            "type": self.event_type,
            "kind": self.kind,
            "tick": self.tick,
            "isFake": self.is_fake,
        }
        if self.event_type == "EdgeNote":
            result.update(edge=self.edge, pos=self.pos)
        else:
            result.update(x=self.x, y=self.y)
        return result


@dataclass
class ScoreChart:
    title: str = "未命名谱面"
    notes: list[ChartNote] = field(default_factory=list)

    def to_dict(self) -> dict[str, Any]:
        return {
            "format": FORMAT_NAME,
            "version": FORMAT_VERSION,
            "title": self.title,
            "notes": [note.to_dict() for note in self.notes],
        }

    def to_json(self) -> str:
        return json.dumps(self.to_dict(), ensure_ascii=False, indent=2) + "\n"

    @classmethod
    def from_json(cls, text: str) -> "ScoreChart":
        try:
            data = json.loads(text)
        except json.JSONDecodeError as exc:
            raise ValueError(f"JSON 格式错误：第 {exc.lineno} 行，第 {exc.colno} 列") from exc
        if not isinstance(data, dict):
            raise ValueError("谱面内容必须是 JSON 对象。")
        if data.get("format", FORMAT_NAME) != FORMAT_NAME:
            raise ValueError(f"不支持的谱面格式：{data.get('format')}")
        if data.get("version", FORMAT_VERSION) != FORMAT_VERSION:
            raise ValueError(f"不支持的 JSON 版本：{data.get('version')}")

        title = data.get("title", "未命名谱面")
        if not isinstance(title, str):
            raise ValueError("title 必须是字符串。")
        entries = data.get("notes")
        if not isinstance(entries, list):
            raise ValueError("notes 必须是音符数组。")

        notes = [_parse_note(entry, index) for index, entry in enumerate(entries, start=1)]
        return cls(title=title, notes=notes)


def _parse_note(data: Any, index: int) -> ChartNote:
    prefix = f"第 {index} 个音符"
    if not isinstance(data, dict):
        raise ValueError(f"{prefix} 必须是 JSON 对象。")

    event_type = data.get("type")
    if event_type not in ("EdgeNote", "SpaceNote"):
        raise ValueError(f"{prefix} 的 type 必须是 EdgeNote 或 SpaceNote。")
    kind = data.get("kind", "tap")
    if not isinstance(kind, str) or not kind.strip():
        raise ValueError(f"{prefix} 的 kind 必须是非空字符串。")
    tick = data.get("tick", 0)
    if isinstance(tick, bool) or not isinstance(tick, int):
        raise ValueError(f"{prefix} 的 tick 必须是整数。")
    is_fake = data.get("isFake", False)
    if not isinstance(is_fake, bool):
        raise ValueError(f"{prefix} 的 isFake 必须是布尔值。")

    if event_type == "EdgeNote":
        edge = data.get("edge")
        if isinstance(edge, bool) or not isinstance(edge, int) or edge not in range(4):
            raise ValueError(f"{prefix} 的 edge 必须是 0～3。")
        limit = GRID_HEIGHT if edge in (0, 1) else GRID_WIDTH
        pos = _coordinate(data.get("pos"), "pos", limit, prefix)
        return ChartNote(event_type, kind.strip(), tick, is_fake, edge=edge, pos=pos)

    x = _coordinate(data.get("x"), "x", GRID_WIDTH, prefix)
    y = _coordinate(data.get("y"), "y", GRID_HEIGHT, prefix)
    return ChartNote(event_type, kind.strip(), tick, is_fake, x=x, y=y)


def _coordinate(value: Any, name: str, limit: int, prefix: str) -> float:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise ValueError(f"{prefix} 的 {name} 必须是数字。")
    if not math.isfinite(value) or not 0 <= value <= limit:
        raise ValueError(f"{prefix} 的 {name} 必须在 0～{limit} 范围内。")
    return float(value)


def edge_position(edge: int, x: float, y: float) -> float:
    """Project a judge-plane coordinate onto its selected edge."""
    return y if edge in (0, 1) else x
