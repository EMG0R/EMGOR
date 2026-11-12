"""
Full turn smoke tests: text input → dispatch → tool execution → no crash.
No audio hardware. No real LLM. Verifies companion module wiring.
"""
import numpy as np
import pytest
from unittest.mock import MagicMock, patch


def test_plain_text_reply_calls_speak():
    """Plain text reply from LLM is spoken directly."""
    fake_audio = np.zeros(100, dtype=np.float32)
    with patch("engine.tools.synthesize", return_value=(fake_audio, 22050)), \
         patch("engine.tools.play") as mock_play:
        from companion import _handle_reply
        results = _handle_reply("what do you want")
    mock_play.assert_called_once_with(fake_audio, 22050)
    assert results == []


def test_tool_call_speak_dispatches():
    """JSON tool call for speak is dispatched and played."""
    fake_audio = np.zeros(100, dtype=np.float32)
    with patch("engine.tools.synthesize", return_value=(fake_audio, 22050)), \
         patch("engine.tools.play") as mock_play:
        from companion import _handle_reply
        results = _handle_reply('{"tool": "speak", "args": {"text": "boo ya"}}')
    mock_play.assert_called_once_with(fake_audio, 22050)
    assert results == []


def test_tool_call_set_voice(tmp_path, monkeypatch):
    """set_voice tool updates module state and saves to memory."""
    import engine.memory as mem
    import engine.tools as t
    monkeypatch.setattr(mem, "MEMORY_DIR", tmp_path)
    monkeypatch.setattr(t, "_voice_mode", t._voice_mode)   # save + restore
    monkeypatch.setattr(t, "_ti_amount", t._ti_amount)     # save + restore
    from companion import _handle_reply
    _handle_reply('{"tool": "set_voice", "args": {"mode": "ti", "ti_amount": 0.9}}')
    assert t._voice_mode == "ti"
    assert t._ti_amount == 0.9


def test_tool_call_write_memory(tmp_path, monkeypatch):
    """write_memory tool persists key-value to facts.md."""
    import engine.memory as mem
    monkeypatch.setattr(mem, "MEMORY_DIR", tmp_path)
    from companion import _handle_reply
    _handle_reply('{"tool": "write_memory", "args": {"key": "fav_color", "value": "black"}}')
    assert mem.read_memory("fav_color") == "black"


def test_chained_tool_calls(tmp_path, monkeypatch):
    """LLM outputs two JSON lines — both execute in order."""
    import engine.memory as mem
    monkeypatch.setattr(mem, "MEMORY_DIR", tmp_path)
    fake_audio = np.zeros(100, dtype=np.float32)
    reply = (
        '{"tool": "write_memory", "args": {"key": "test_key", "value": "yes"}}\n'
        '{"tool": "speak", "args": {"text": "done"}}'
    )
    with patch("engine.tools.synthesize", return_value=(fake_audio, 22050)), \
         patch("engine.tools.play") as mock_play:
        from companion import _handle_reply
        results = _handle_reply(reply)
    assert mem.read_memory("test_key") == "yes"
    mock_play.assert_called_once_with(fake_audio, 22050)
    assert results == []
