import sys, pathlib, struct
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
from demiurge_web import (parse_throttled, derive_power, parse_temp,
                          parse_meminfo, parse_loadavg, parse_liveconf,
                          parse_sets, parse_interfaces, parse_sync, percore_cpu,
                          validate_sensors_config, validate_sensor_source,
                          validate_sensor_map, validate_target,
                          read_sensors_config, write_sensors_config,
                          read_expr_targets, osc_decode_multi, osc_decode,
                          _friendly_label)
FX = pathlib.Path(__file__).parent / "fixtures"


def test_throttled_decode_now_and_past():
    t = parse_throttled((FX / "throttled_0x50005.txt").read_text())
    assert t["hex"] == "0x50005"
    assert t["now"] == ["UNDERVOLT", "THROTTLE"]          # bits 0,2
    assert set(t["past"]) == {"UNDERVOLT", "THROTTLE"}    # bits 16,18


def test_throttled_clean():
    assert parse_throttled("throttled=0x0") == {"hex": "0x0", "now": [], "past": []}


def test_power_thresholds():
    assert derive_power(1_500_000) == "LOW"
    assert derive_power(2_400_000) == "MED"
    assert derive_power(2_800_000) == "HIGH"


def test_temp():
    assert parse_temp("temp=57.4'C") == 57.4


def test_meminfo():
    m = parse_meminfo((FX / "meminfo.txt").read_text())
    assert m == {"total_kb": 16602348, "avail_kb": 15991224}


def test_loadavg():
    l = parse_loadavg((FX / "loadavg.txt").read_text(), 4)
    assert l["load1"] == 0.42
    assert l["pct"] == 10.5


def test_liveconf_real_file():
    lc = parse_liveconf((FX / "live.conf").read_text())
    assert lc["set"] is not None
    assert lc["chains"], "chain block must parse to at least one row"
    assert lc["chains"][0] == ["neptrPhase4"]


def test_liveconf_input_head_and_sync_off():
    lc = parse_liveconf((FX / "sets/nam.conf").read_text())
    assert lc["has_input"] is True
    assert lc["chains"] == [["73_plexi_bright"]]
    off = parse_liveconf("# set: x\nsync_layer = off\nchain =\n  ~/a/b.csd\n")
    assert off["has_input"] is True


def test_sets_sorted_and_nam_hidden():
    files = {"neptr.conf": (FX / "sets/neptr.conf").read_text(),
             "nam.conf":   (FX / "sets/nam.conf").read_text()}
    out = parse_sets(files)
    assert [s["set"] for s in out] == ["neptr"] and out[0]["power"] == "high"


def test_interfaces_normal():
    out = parse_interfaces((FX / "interfaces.txt").read_text())
    assert out == [
        {"name": "Interface A", "id": "IfaceA", "display": "Interface A",
         "active": True, "chosen": True, "inuse": True},
        {"name": "Interface B", "id": "IfaceB", "display": "Interface B",
         "active": False, "chosen": False, "inuse": False},
        {"name": "snd_rpi_hifiberry_dacplusadcpro", "id": "sndrpihifiberry",
         "display": "HiFiBerry DAC+ADC Pro",
         "active": False, "chosen": False, "inuse": False},
    ]


def test_interfaces_flag_combos():
    out = parse_interfaces("A|0|chosen\nB|1|active,inuse\nC|2|inuse\n")
    assert [(i["active"], i["chosen"], i["inuse"]) for i in out] == \
        [(False, True, False), (True, False, True), (False, False, True)]


def test_interfaces_display_fourth_field():
    # 4th field is the display name; absent/empty falls back to the raw name.
    out = parse_interfaces("vc4-hdmi-0|vc4hdmi0|active|HDMI 0\nVolt 2|V2||\n")
    assert [(i["name"], i["display"]) for i in out] == \
        [("vc4-hdmi-0", "HDMI 0"), ("Volt 2", "Volt 2")]


def test_interfaces_malformed_lines_skipped():
    out = parse_interfaces("garbage line\nA|0\n|1|active\nB|2|active\n"
                           "way|too|many|fields|here\n")
    assert out == [{"name": "B", "id": "2", "display": "B",
                    "active": True, "chosen": False, "inuse": False}]


def test_interfaces_empty():
    assert parse_interfaces("") == []
    assert parse_interfaces(None) == []


def test_sync_normal():
    s = parse_sync((FX / "sync.txt").read_text())
    assert s == {"role": "opgorator", "bpm": 120.0, "source_alive": True}


def test_sync_demiurge_role_and_dead_source():
    s = parse_sync("demiurge|93.5|0\n")
    assert s == {"role": "demiurge", "bpm": 93.5, "source_alive": False}


def test_sync_bad_role_rejected():
    assert parse_sync("volca|120|1\n") is None


def test_sync_bad_bpm_rejected():
    assert parse_sync("opgorator|fast|1\n") is None


def test_sync_malformed_or_empty():
    assert parse_sync("") is None
    assert parse_sync(None) is None
    assert parse_sync("opgorator|120\n") is None


def test_liveconf_opgorator_chain_token_bare():
    lc = parse_liveconf("# set: x\nchain =\n  opgorator\n")
    assert lc["chains"] == [["opgorator"]]


def test_liveconf_opgorator_chain_single_line():
    lc = parse_liveconf("# set: x\nchain = opgorator\n")
    assert lc["chains"] == [["opgorator"]]


def test_percore_cpu():
    pcts = percore_cpu((FX / "stat.txt").read_text(),
                       (FX / "stat2.txt").read_text())
    assert len(pcts) == 4
    assert all(0.0 <= p <= 100.0 for p in pcts)
    assert pcts[0] == 60.1   # cpu0: dt=203, didle=81, busy=122 -> 60.1%


def test_touch_normal():
    from demiurge_web import parse_touch
    t = parse_touch((FX / "touch.txt").read_text())
    assert t == {"on": True, "detected": True,
                 "device": "/dev/input/event4",
                 "name": "ADS7846 Touchscreen",
                 "base": 80, "channel": 16, "slots": 1}


def test_touch_off_not_detected():
    from demiurge_web import parse_touch
    t = parse_touch("touch=off|detected=no|device=|name=|base=80|channel=16|slots=1\n")
    assert t["on"] is False and t["detected"] is False


def test_touch_malformed_or_empty():
    from demiurge_web import parse_touch
    assert parse_touch("") is None
    assert parse_touch(None) is None
    assert parse_touch("garbage line\n") is None
    assert parse_touch("touch=on|detected=yes|base=NaN|channel=16|slots=1\n") is None


# ───────────── demiurge-audio --list ─────────────

def test_audio_settings_normal():
    from demiurge_web import parse_audio
    a = parse_audio((FX / "audio.txt").read_text())
    assert set(a) == {"mode", "bits", "quantum", "rate"}
    assert a["bits"]["value"] == "32"
    assert a["bits"]["options"] == ["16", "32", "float"]
    assert a["quantum"]["options"][0] == "32"


def test_audio_settings_never_offers_24_bit():
    from demiurge_web import parse_audio
    a = parse_audio((FX / "audio.txt").read_text())
    assert "24" not in a["bits"]["options"]


def test_audio_settings_effect_is_carried_verbatim():
    # The mode-aware honesty string is the CLI's, not the UI's.
    from demiurge_web import parse_audio
    a = parse_audio(
        "quantum|128|128|32,64,128|CPU headroom only — in bypass mode latency "
        "is pinned at 1024 frames (21.3 ms) by the audio backend\n")
    assert a["quantum"]["effect"].startswith("CPU headroom only")
    assert "pinned" in a["quantum"]["effect"]


def test_audio_settings_malformed_or_empty():
    from demiurge_web import parse_audio
    assert parse_audio("") is None
    assert parse_audio(None) is None
    assert parse_audio("bits|32|32\n") is None          # too few fields
    assert parse_audio("ksmps|64|64|a,b|x\n") is None   # unknown key only


def test_audio_settings_skips_bad_lines_keeps_good():
    from demiurge_web import parse_audio
    a = parse_audio("garbage\nbits|16|32|16,32,float|fmt\n")
    assert list(a) == ["bits"] and a["bits"]["value"] == "16"


# ───────────── demiurge-wifi ─────────────

def test_wifi_status_normal():
    from demiurge_web import parse_wifi_status
    w = parse_wifi_status((FX / "wifi_status.txt").read_text())
    assert w == {"state": "connected", "ssid": "GOR_CAVE", "signal": 81,
                 "security": "wpa", "ip": "192.168.8.156", "iface": "wlan0"}


def test_wifi_status_disconnected_has_no_ssid():
    from demiurge_web import parse_wifi_status
    w = parse_wifi_status("disconnected|||||wlan0\n")
    assert w["state"] == "disconnected" and w["ssid"] == ""
    assert w["signal"] is None


def test_wifi_status_malformed():
    from demiurge_web import parse_wifi_status
    assert parse_wifi_status("") is None
    assert parse_wifi_status(None) is None
    assert parse_wifi_status("connected|GOR_CAVE\n") is None
    assert parse_wifi_status("weird|a|1|wpa|ip|wlan0\n") is None


def test_wifi_list_normal_order_and_flags():
    from demiurge_web import parse_wifi_list
    nets = parse_wifi_list((FX / "wifi_list.txt").read_text())
    assert [n["ssid"] for n in nets][0] == "GOR_CAVE"
    assert nets[0]["active"] and nets[0]["saved"]
    assert nets[1]["saved"] and not nets[1]["active"]
    assert nets[3]["security"] == "open" and not nets[3]["saved"]


def test_wifi_list_ssid_with_spaces_survives():
    from demiurge_web import parse_wifi_list
    nets = parse_wifi_list("CoffeeShop Guest|40|open|\n")
    assert nets[0]["ssid"] == "CoffeeShop Guest"


def test_wifi_list_skips_malformed_and_empty():
    from demiurge_web import parse_wifi_list
    assert parse_wifi_list("") == []
    assert parse_wifi_list(None) == []
    assert parse_wifi_list("junk\n|50|wpa|\nOK|50|wpa|saved\n") == [
        {"ssid": "OK", "signal": 50, "security": "wpa",
         "active": False, "saved": True}]


# ───────────────────────── graph (the patcher) ─────────────────────────
#
# These protect the two facts the graph exists to surface: DIRECT vs LOOPBACK
# per direction, and an engine port connected to nothing. The second one is
# the 2026-08-09 fault — the engine's inputs were wired to nothing and no UI
# could say so.

def test_graph_parses_nodes_ports_links_and_paths():
    from demiurge_web import parse_graph
    g = parse_graph((FX / "graph.txt").read_text())
    assert g["mode"] == "sync"
    assert g["in_path"] == "direct" and g["out_path"] == "direct"
    assert len(g["links"]) == 6
    assert {n["kind"] for n in g["nodes"]} == {"interface", "engine", "loopback"}


def test_graph_link_carries_its_path_class_and_flags():
    from demiurge_web import parse_graph
    g = parse_graph((FX / "graph.txt").read_text())
    direct = [l for l in g["links"] if l["dst"] == "csound6:input1"][0]
    assert direct["path"] == "direct"
    assert set(direct["flags"]) == {"engine-in", "managed"}
    loop = [l for l in g["links"]
            if l["dst"] == "demiurge-source-input:input_FL"][0]
    assert loop["path"] == "loopback"


def test_graph_port_carries_its_link_count():
    from demiurge_web import parse_graph
    g = parse_graph((FX / "graph.txt").read_text())
    p = [p for p in g["ports"] if p["name"] == "csound6:input1"][0]
    assert (p["dir"], p["kind"], p["display"], p["links"]) == \
        ("in", "engine", "IN 1", 1)


def test_graph_reports_an_unconnected_engine_port():
    from demiurge_web import parse_graph
    g = parse_graph(
        "mode|sync\n"
        "port|csound6:input1|csound6|in|engine|IN 1|0\n"
        "warn|no-input|The engine has no input — nothing you play reaches it.\n"
        "warn|dangling|csound6:input1 is connected to nothing (engine input).\n"
        "path|none|direct\n")
    assert g["in_path"] == "none"
    assert [w["code"] for w in g["warnings"]] == ["no-input", "dangling"]
    assert g["ports"][0]["links"] == 0


def test_graph_bypass_is_a_state_not_a_failure():
    """demiurge-graph exits 3 and prints mode|bypass when the sync layer is
    off. Reporting that as None would make the UI say "unavailable" where the
    honest answer is "there is no graph in bypass mode"."""
    from demiurge_web import parse_graph
    g = parse_graph("mode|bypass\n")
    assert g["mode"] == "bypass" and g["links"] == []


def test_graph_missing_or_unparseable_is_none():
    from demiurge_web import parse_graph
    assert parse_graph(None) is None
    assert parse_graph("") is None
    assert parse_graph("mode|nonsense\n") is None      # unknown mode
    assert parse_graph("junk\nnothing here\n") is None  # no mode line


def test_graph_skips_malformed_lines():
    from demiurge_web import parse_graph
    g = parse_graph("mode|sync\n"
                    "port|a:b|a|sideways|engine|X|1\n"   # bad direction
                    "port|a:b|a|in|engine|X|zz\n"        # bad link count
                    "port|too|few\n"
                    "link|only|three\n"
                    "path|weird|direct\n")
    assert g["ports"] == [] and g["links"] == []
    assert g["in_path"] is None and g["out_path"] == "direct"


# ───────────────────────── sensor mapping ─────────────────────────

def _source(**over):
    s = {"id": "tof1", "label": "LiDAR 1", "units": "mm", "in_lo": 20,
         "in_hi": 460, "dropout": "hold", "deadzone": 0.02, "smooth_ms": 40,
         "enabled": True}
    s.update(over)
    return s


def _map(**over):
    m = {"id": "m1", "source": "tof1", "enabled": True, "zone_lo": 0.0,
         "zone_hi": 0.5, "outside": "hold", "invert": False, "curve": "lin",
         "out_lo": 0.0, "out_hi": 1.0, "mode": "continuous",
         "combine": "replace", "slew_ms": 30,
         "target": {"sink": "csound", "kind": "expr", "id": 30}}
    m.update(over)
    return m


def test_validate_sensor_source_ok():
    assert validate_sensor_source(_source()) is None


def test_validate_sensor_source_bad_id_and_ranges():
    assert validate_sensor_source(_source(id="")) is not None
    assert validate_sensor_source(_source(id="has space")) is not None
    assert validate_sensor_source(_source(dropout="bogus")) is not None
    assert validate_sensor_source(_source(deadzone=1.5)) is not None
    assert validate_sensor_source(_source(smooth_ms=-1)) is not None
    assert validate_sensor_source(_source(enabled="yes")) is not None


def test_validate_target_expr_and_cc():
    assert validate_target({"sink": "csound", "kind": "expr", "id": 30},
                           {30, 1}) is None
    assert validate_target({"sink": "csound", "kind": "expr", "id": 999},
                           {30, 1}) is not None
    assert validate_target({"sink": "csound", "kind": "cc", "ch": 0, "cc": 20},
                           None) is None
    assert validate_target({"sink": "csound", "kind": "cc", "ch": 16, "cc": 20},
                           None) is not None
    assert validate_target({"sink": "midi", "kind": "expr", "id": 30},
                           None) is not None


def test_validate_sensor_map_ok_and_bad_zone():
    assert validate_sensor_map(_map(), {"tof1"}, {30}) is None
    assert validate_sensor_map(_map(zone_lo=0.8, zone_hi=0.2), {"tof1"},
                               {30}) is not None
    assert validate_sensor_map(_map(source="nope"), {"tof1"}, {30}) is not None
    assert validate_sensor_map(_map(curve="bogus"), {"tof1"}, {30}) is not None


def test_validate_sensors_config_full_roundtrip():
    cfg = {"sources": [_source()], "maps": [_map()]}
    assert validate_sensors_config(cfg, valid_expr_ids={30}) is None


def test_validate_sensors_config_rejects_malformed_shapes():
    assert validate_sensors_config(None) is not None
    assert validate_sensors_config({}) is not None
    assert validate_sensors_config({"sources": "nope", "maps": []}) is not None
    assert validate_sensors_config({"sources": [], "maps": "nope"}) is not None


def test_validate_sensors_config_rejects_map_to_unknown_source():
    cfg = {"sources": [_source()], "maps": [_map(source="ghost")]}
    assert validate_sensors_config(cfg) is not None


def test_validate_sensors_config_rejects_duplicate_ids():
    cfg = {"sources": [_source(), _source()], "maps": []}
    assert validate_sensors_config(cfg) is not None
    cfg2 = {"sources": [_source()], "maps": [_map(), _map()]}
    assert validate_sensors_config(cfg2) is not None


def test_validate_sensors_config_two_maps_can_share_one_source():
    # The headline feature: multiple maps slicing one sensor's travel.
    cfg = {"sources": [_source()],
          "maps": [_map(id="m1", zone_lo=0.0, zone_hi=0.5),
                   _map(id="m2", zone_lo=0.5, zone_hi=1.0)]}
    assert validate_sensors_config(cfg, valid_expr_ids={30}) is None


def test_read_sensors_config_missing_file_is_empty():
    cfg = read_sensors_config("/nonexistent/path/sensors.json")
    assert cfg == {"sources": [], "maps": []}


def test_read_sensors_config_malformed_json_is_empty(tmp_path):
    p = tmp_path / "sensors.json"
    p.write_text("{not json")
    assert read_sensors_config(p) == {"sources": [], "maps": []}


def test_write_sensors_config_roundtrips_and_is_atomic(tmp_path):
    p = tmp_path / "demiurge" / "sensors.json"
    cfg = {"sources": [_source()], "maps": [_map()]}
    write_sensors_config(cfg, p)
    assert read_sensors_config(p) == cfg
    # no leftover temp file after a successful write
    assert list(tmp_path.glob("demiurge/*.tmp")) == []


def test_read_expr_targets_from_fixture():
    targets = read_expr_targets(FX / "expr_targets.json")
    assert len(targets) == 4
    ids = {t["id"] for t in targets}
    assert ids == {30, 1, 21, 22}
    # id 30's label is empty in the source file -> friendly fallback fires
    t30 = next(t for t in targets if t["id"] == 30)
    assert t30["label"] == "Master Vol"
    # id 22 has a real label -> used verbatim
    t22 = next(t for t in targets if t["id"] == 22)
    assert t22["label"] == "WAH pedal position"


def test_read_expr_targets_missing_file_is_empty_list():
    assert read_expr_targets("/nonexistent/expr_targets.json") == []


def test_friendly_label_from_csound_global():
    assert _friendly_label("gkMasterVol") == "Master Vol"
    assert _friendly_label("gkBBcut_expr") == "BBcut expr"


# ───────────────────────── OSC decode ─────────────────────────

def _osc_string(s):
    b = s.encode("ascii") + b"\0"
    return b + b"\0" * (-len(b) % 4)


def test_osc_decode_multi_sensor_broadcast():
    addr = _osc_string("/sensor")
    tags = _osc_string(",sf")
    sid = _osc_string("tof1")
    val = struct.pack(">f", 0.375)
    msg = addr + tags + sid + val
    decoded = osc_decode_multi(msg)
    assert decoded[0] == "/sensor"
    assert decoded[1][0] == "tof1"
    assert abs(decoded[1][1] - 0.375) < 1e-6


def test_osc_decode_multi_rejects_garbage():
    assert osc_decode_multi(b"not an osc packet at all") is None
    assert osc_decode_multi(b"") is None


def test_osc_decode_still_handles_single_arg_engine_messages():
    # osc_decode (pre-existing) must be unaffected by the new multi-arg path.
    addr = _osc_string("/menu")
    tags = _osc_string(",i")
    val = struct.pack(">i", 3)
    assert osc_decode(addr + tags + val) == ("/menu", 3)
