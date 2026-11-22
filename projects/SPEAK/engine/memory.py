from __future__ import annotations
import json
import re
from datetime import datetime
from pathlib import Path

MEMORY_DIR = Path("memory")


def load_all() -> dict:
    ctx: dict = {}
    cfg = load_voice_config()
    if cfg:
        ctx["voice_config"] = cfg
    facts_p = MEMORY_DIR / "facts.md"
    if facts_p.exists():
        text = facts_p.read_text().strip()
        if text:
            ctx["facts"] = text
    return ctx


def write_memory(key: str, value: str) -> None:
    MEMORY_DIR.mkdir(exist_ok=True)
    p = MEMORY_DIR / "facts.md"
    text = p.read_text() if p.exists() else ""
    pattern = re.compile(rf"^- \*\*{re.escape(key)}:\*\*.*$", re.MULTILINE)
    new_line = f"- **{key}:** {value}"
    if pattern.search(text):
        text = pattern.sub(new_line, text)
    else:
        text = text.rstrip("\n") + f"\n{new_line}\n"
    p.write_text(text)


def delete_memory(key: str) -> None:
    p = MEMORY_DIR / "facts.md"
    if not p.exists():
        return
    text = p.read_text()
    pattern = re.compile(rf"^- \*\*{re.escape(key)}:\*\*.*\n?", re.MULTILINE)
    p.write_text(pattern.sub("", text))


def list_memory() -> list[str]:
    p = MEMORY_DIR / "facts.md"
    if not p.exists():
        return []
    keys = []
    for line in p.read_text().splitlines():
        m = re.match(r"- \*\*(.+?):\*\*", line)
        if m:
            keys.append(m.group(1))
    return keys


def read_memory(key: str) -> str:
    p = MEMORY_DIR / "facts.md"
    if not p.exists():
        return ""
    for line in p.read_text().splitlines():
        if line.lstrip().startswith(f"- **{key}:**"):
            return line.split(":**", 1)[-1].strip()
    return ""


def write_lore(entry: str) -> None:
    MEMORY_DIR.mkdir(exist_ok=True)
    p = MEMORY_DIR / "lore.md"
    timestamp = datetime.now().strftime("%Y-%m-%d")
    existing = p.read_text().rstrip("\n") if p.exists() else ""
    p.write_text(existing + f"\n\n[{timestamp}] {entry}\n")


def load_lore() -> str:
    p = MEMORY_DIR / "lore.md"
    if p.exists():
        text = p.read_text().strip()
        if text:
            return text
    return ""


def log_turn(role: str, content: str, session_file: Path) -> None:
    session_file.parent.mkdir(parents=True, exist_ok=True)
    with session_file.open("a") as f:
        f.write(json.dumps({"role": role, "content": content, "ts": datetime.now().isoformat()}) + "\n")


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
