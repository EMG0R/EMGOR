import threading
from unittest.mock import patch, MagicMock


def test_listener_accepts_list_of_wake_words():
    fake_model = MagicMock()
    with patch("engine.wakeword.Model", MagicMock(return_value=fake_model)) as m:
        from engine.wakeword import WakeWordListener
        listener = WakeWordListener(threading.Event(), wake_words=["computer", "neptr"])
    # Happy path: a single combined construction
    assert m.call_count == 1
    final_call = m.call_args_list[-1]
    assert final_call.kwargs["wakeword_models"] == ["computer", "neptr"]
    assert listener._loaded_words == ["computer", "neptr"]


def test_listener_falls_back_when_a_word_is_unavailable():
    call_log = []
    def model_factory(*args, **kwargs):
        names = kwargs.get("wakeword_models", [])
        call_log.append(names)
        if "computah" in names:
            raise ValueError("model not found")
        return MagicMock()
    with patch("engine.wakeword.Model", side_effect=model_factory):
        from engine.wakeword import WakeWordListener
        listener = WakeWordListener(threading.Event(), wake_words=["computer", "computah"])
    assert listener._loaded_words == ["computer"]


def test_listener_raises_when_zero_wake_words_load():
    with patch("engine.wakeword.Model", side_effect=ValueError("nope")):
        from engine.wakeword import WakeWordListener
        import pytest
        with pytest.raises(RuntimeError, match="no wake words"):
            WakeWordListener(threading.Event(), wake_words=["xxx", "yyy"])


def test_listener_raises_when_openwakeword_missing():
    with patch("engine.wakeword.Model", None):
        from engine.wakeword import WakeWordListener
        import pytest
        with pytest.raises(RuntimeError, match="openwakeword"):
            WakeWordListener(threading.Event(), wake_words=["computer"])
