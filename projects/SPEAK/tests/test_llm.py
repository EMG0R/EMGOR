import json
import threading
from unittest.mock import MagicMock, patch


def _make_backend(mock_llama_cls):
    """Helper: build LLMBackend with mocked Llama class."""
    from engine.llm import LLMBackend
    return LLMBackend(model_path="fake.gguf")


def test_generate_returns_plain_text(monkeypatch):
    mock_llm = MagicMock()
    mock_llm.create_chat_completion.return_value = {
        "choices": [{"message": {"content": "hey what up"}}]
    }
    with patch("engine.llm.Llama", return_value=mock_llm):
        from engine.llm import LLMBackend
        backend = LLMBackend(model_path="fake.gguf")
        result = backend.generate("you are cool", [], "hello")
    assert result == "hey what up"


def test_generate_passes_history(monkeypatch):
    mock_llm = MagicMock()
    mock_llm.create_chat_completion.return_value = {
        "choices": [{"message": {"content": "ok"}}]
    }
    history = [
        {"role": "user", "content": "first"},
        {"role": "assistant", "content": "response"},
    ]
    with patch("engine.llm.Llama", return_value=mock_llm):
        from engine.llm import LLMBackend
        backend = LLMBackend(model_path="fake.gguf")
        backend.generate("sys", history, "second")

    call_args = mock_llm.create_chat_completion.call_args
    messages = call_args.kwargs["messages"]
    assert messages[0]["role"] == "system"
    assert messages[1]["content"] == "first"
    assert messages[3]["content"] == "second"


def test_generate_returns_tool_call_json(monkeypatch):
    tool_json = '{"tool": "speak", "args": {"text": "boo"}}'
    mock_llm = MagicMock()
    mock_llm.create_chat_completion.return_value = {
        "choices": [{"message": {"content": tool_json}}]
    }
    with patch("engine.llm.Llama", return_value=mock_llm):
        from engine.llm import LLMBackend
        backend = LLMBackend(model_path="fake.gguf")
        result = backend.generate("sys", [], "do something")
    assert result == tool_json


def test_trim_history_keeps_last_20(monkeypatch):
    mock_llm = MagicMock()
    mock_llm.create_chat_completion.return_value = {
        "choices": [{"message": {"content": "ok"}}]
    }
    long_history = [{"role": "user", "content": str(i)} for i in range(30)]
    with patch("engine.llm.Llama", return_value=mock_llm):
        from engine.llm import LLMBackend
        backend = LLMBackend(model_path="fake.gguf")
        backend.generate("sys", long_history, "new")

    call_args = mock_llm.create_chat_completion.call_args
    messages = call_args.kwargs["messages"]
    # system + 20 history + 1 user = 22
    assert len(messages) <= 22


def test_try_generate_returns_none_when_lock_held():
    from engine.llm import LLMBackend
    with patch("engine.llm.Llama", MagicMock()):
        backend = LLMBackend()
        backend._llm.create_chat_completion.return_value = {"choices": [{"message": {"content": "ok"}}]}
        backend._lock.acquire()
        try:
            result = backend.try_generate("sys", [], "hi")
            assert result is None
        finally:
            backend._lock.release()
