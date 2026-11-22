from __future__ import annotations
import json
from pathlib import Path

MEMORY_DIR = Path("memory")


def load_all() -> dict:
    ctx: dict = {}
    cfg = load_voice_config()
    if cfg:
        ctx["voice_config"] = cfg
    for name in ("facts", "lore"):
        p = MEMORY_DIR / f"{name}.md"
        if p.exists():
            text = p.read_text().strip()
            if text:
                ctx[name] = text
    return ctx


def write_memory(key: str, value: str) -> None:
    MEMORY_DIR.mkdir(exist_ok=True)
    p = MEMORY_DIR / "facts.md"
    with p.open("a") as f:
        f.write(f"\n- **{key}:** {value}")


def read_memory(key: str) -> str:
    p = MEMORY_DIR / "facts.md"
    if not p.exists():
        return ""
    for line in p.read_text().splitlines():
        if line.lstrip().startswith(f"- **{key}:**"):
            return line.split(":**", 1)[-1].strip()
    return ""


def save_voice_config(mode: str, ti_amount: float) -> None:
    MEMORY_DIR.mkdir(exist_ok=True)
    (MEMORY_DIR / "voice_config.json").write_text(
        json.dumps({"mode": mode, "ti_amount": ti_amount}, indent=2)
    )


def load_voice_config() -> dict | None:
    p = MEMORY_DIR / "voice_config.json"
    if p.exists():
        return json.loads(p.read_text())
    return None


def write_song(filename: str, content: str) -> None:
    songs_dir = MEMORY_DIR / "songs"
    songs_dir.mkdir(parents=True, exist_ok=True)
    (songs_dir / filename).write_text(content)
