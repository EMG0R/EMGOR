import pytest
from unittest.mock import patch, MagicMock


def test_read_cpu_temp_returns_none_on_mac():
    with patch("engine.thermal._IS_PI", False):
        import engine.thermal as thermal
        assert thermal.read_cpu_temp() is None


def test_read_cpu_temp_parses_sysfs(tmp_path):
    import platform
    temp_file = tmp_path / "temp"
    temp_file.write_text("72500\n")
    with patch.object(platform, "machine", return_value="aarch64"), \
         patch("engine.thermal.TEMP_PATH", temp_file), \
         patch("engine.thermal._IS_PI", True):
        import engine.thermal as thermal
        assert thermal.read_cpu_temp() == pytest.approx(72.5)


def test_check_temp_no_op_on_mac():
    mock_llm = MagicMock()
    with patch("engine.thermal._IS_PI", False):
        import engine.thermal as thermal
        warned = thermal.check_and_throttle(mock_llm)
    assert not warned


def test_check_temp_warns_above_80(tmp_path):
    temp_file = tmp_path / "temp"
    temp_file.write_text("81000\n")
    mock_llm = MagicMock()
    mock_llm.n_batch = 512
    with patch("engine.thermal.TEMP_PATH", temp_file), \
         patch("engine.thermal._IS_PI", True):
        import engine.thermal as thermal
        warned = thermal.check_and_throttle(mock_llm)
    assert warned
    assert mock_llm.n_batch == 64


def test_check_temp_restores_normal_batch(tmp_path):
    temp_file = tmp_path / "temp"
    temp_file.write_text("70000\n")
    mock_llm = MagicMock()
    mock_llm.n_batch = 64
    with patch("engine.thermal.TEMP_PATH", temp_file), \
         patch("engine.thermal._IS_PI", True):
        import engine.thermal as thermal
        warned = thermal.check_and_throttle(mock_llm)
    assert not warned
    assert mock_llm.n_batch == 512


def test_check_temp_pauses_above_85_until_cool(tmp_path):
    """Verify blocking pause: n_batch throttled, then restored after cooling."""
    temp_file = tmp_path / "temp"
    # Start hot, cool down on second read
    call_count = 0

    def fake_read_temp():
        nonlocal call_count
        call_count += 1
        return 86.0 if call_count == 1 else 72.0

    mock_llm = MagicMock()
    mock_llm.n_batch = 512
    import engine.thermal as thermal
    with patch("engine.thermal._IS_PI", True), \
         patch("engine.thermal.read_cpu_temp", side_effect=fake_read_temp), \
         patch("engine.thermal.time.sleep"):
        warned = thermal.check_and_throttle(mock_llm)
    assert warned
    # After cooling, n_batch should be restored to 512
    assert mock_llm.n_batch == 512
