def test_npc_sounds_bank_present_and_non_empty():
    from engine.idle_sounds import NPC_SOUNDS
    assert isinstance(NPC_SOUNDS, list)
    assert len(NPC_SOUNDS) >= 10
    assert all(isinstance(s, str) and s for s in NPC_SOUNDS)
    # Spot check the named entries
    assert "hmmm" in NPC_SOUNDS
    assert "type shit" in NPC_SOUNDS
    assert "damn shawty" in NPC_SOUNDS


def test_verse_topics_present():
    from engine.idle_sounds import VERSE_TOPICS
    assert isinstance(VERSE_TOPICS, list)
    assert len(VERSE_TOPICS) >= 5
    # Cosmology pool
    assert any("space" in t for t in VERSE_TOPICS)
    assert any("time" in t for t in VERSE_TOPICS)
