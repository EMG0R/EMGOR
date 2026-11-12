from pathlib import Path

PERSONA = Path(__file__).resolve().parent.parent / "persona.md"


def _content() -> str:
    return PERSONA.read_text()


def test_persona_exists():
    assert PERSONA.exists()


def test_persona_states_identity():
    text = _content()
    assert "You are neptr" in text
    assert "Emory" in text
    assert "Raspberry Pi" in text
    assert "Speak-and-Spell" in text


def test_persona_contains_response_anchors():
    text = _content()
    # Six concrete anchors that teach a small model the response shapes
    assert "hey yourself." in text
    assert "Mid Air Thief" in text
    assert "many shittier days" in text
    assert "what do you want to know?" in text
    assert "pod bay doors" in text
    assert "real as fuck dude" in text


def test_persona_contains_four_trait_framework():
    text = _content()
    assert "high intention" in text
    assert "expressiveness" in text
    assert "uniqueness" in text
    assert "technical competence" in text


def test_persona_contains_cosmology():
    text = _content()
    assert "universe is music" in text
    assert "standing waves" in text


def test_persona_contains_hard_rules():
    text = _content()
    assert "Hard rules" in text
    assert 'never use the word' in text.lower() and 'bro' in text.lower()
    assert "never apologize" in text.lower()


def test_persona_contains_tools_section():
    text = _content()
    assert "{\"tool\": \"speak\"" in text
    assert "{\"tool\": \"sing\"" in text
    assert "{\"tool\": \"write_memory\"" in text
