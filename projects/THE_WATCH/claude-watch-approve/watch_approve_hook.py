#!/usr/bin/env python3
"""
claude-watch-approve  -- Claude Code PreToolUse hook.

On a permission-needing tool call, ask the user's smartwatch (via the Mac
companion HTTP bridge) to approve/deny. Falls back to the normal terminal
permission prompt if the bridge or watch is unreachable. Never blocks forever,
never silently auto-denies on infrastructure failure.

Stdlib only. No pip installs. Python 3.7+.

PreToolUse stdin contract (verified against code.claude.com/docs/en/hooks):
  {
    "session_id": "...",
    "transcript_path": "...",
    "cwd": "/abs/project/dir",
    "permission_mode": "default",
    "hook_event_name": "PreToolUse",
    "tool_name": "Bash",
    "tool_input": { "command": "npm test" }
  }

Decision output written to stdout (exit 0):
  {
    "hookSpecificOutput": {
      "hookEventName": "PreToolUse",
      "permissionDecision": "allow" | "deny" | "ask",
      "permissionDecisionReason": "..."
    }
  }
  - allow  : tool runs, no terminal prompt
  - deny   : tool blocked
  - ask    : defer to Claude Code's normal terminal permission prompt (fail-safe)
"""

import sys
import os
import json
import time
import socket
import contextlib
import urllib.request
import urllib.error

try:
    import fcntl  # POSIX file locking (present on macOS/Linux)
except Exception:
    fcntl = None


@contextlib.contextmanager
def _file_lock(path):
    """Serialize read-modify-write on a shared JSON file across concurrent hook
    processes (parallel Claude windows). Uses a sibling <path>.lock + flock so a
    burst of simultaneous session/cache writes can't clobber each other's entries.
    Degrades to a no-op lock if fcntl is unavailable (writes still atomic via
    os.replace; only the merge could race)."""
    if fcntl is None:
        yield
        return
    lockpath = path + ".lock"
    try:
        os.makedirs(os.path.dirname(path), exist_ok=True)
    except Exception:
        pass
    f = None
    try:
        f = open(lockpath, "w")
    except Exception:
        f = None
    if f is not None:
        try:
            fcntl.flock(f.fileno(), fcntl.LOCK_EX)
        except Exception:
            pass
    try:
        yield
    finally:
        if f is not None:
            try:
                fcntl.flock(f.fileno(), fcntl.LOCK_UN)
            except Exception:
                pass
            f.close()

HERE = os.path.dirname(os.path.abspath(__file__))

# ---- defaults (overridden by config.json next to this script) --------------
DEFAULTS = {
    "bridge_host": "127.0.0.1",
    "bridge_port": 8765,
    "health_timeout_sec": 0.75,
    "request_timeout_sec": 65,
    "route_tools": ["Bash", "Write", "Edit", "MultiEdit", "NotebookEdit",
                    "WebFetch", "WebSearch"],
    "auto_allow": {
        "bash_command_prefixes": [
            "ls", "cat", "pwd", "echo", "which", "type",
            "git status", "git diff", "git log", "git show", "git branch",
            "grep", "rg", "find", "head", "tail", "wc", "file", "stat",
        ],
        "tool_names": [],
    },
    # Cod approval broker (local, only listening while the Cod app runs).
    #
    # SCOPE: this whole step engages ONLY for Claude Code sessions spawned by
    # the Cod app itself, identified by the `cod_pane_env_var` environment
    # variable that Cod exports in each pane (see cod/src-tauri/src/lib.rs,
    # spawn_pane). Hooks are plain subprocesses of the Claude Code session, so
    # they inherit it. Every other `claude` on this machine never sees this
    # code path at all -- no health check, no network call, behaviour identical
    # to before the Cod feature existed.
    #
    # For a Cod-spawned session the broker can only ever say SAFE (-> allow) or
    # UNSAFE/no-answer (-> terminal `ask`). It never denies. Cod is
    # self-contained and deliberately does NOT use the watch bridge as a
    # fallback; the watch path below is untouched and still serves every
    # non-Cod session.
    #
    # Setting "cod_broker_enabled": false restores fully original behaviour for
    # Cod panes too (watch bridge included).
    "cod_broker_enabled": True,
    "cod_pane_env_var": "COD_PANE",
    "cod_broker_host": "127.0.0.1",
    "cod_broker_port": 8766,
    "cod_broker_health_timeout_sec": 0.75,
    "cod_broker_request_timeout_sec": 10,
    "log_file": "~/.claude/claude-watch-approve.log",
    "cache_file": "~/.claude/claude-watch-approve-cache.json",
    "fail_open_on_bridge_down": False,
}

# Bash first-tokens that carry a meaningful subcommand; for these the cache /
# match signature uses the first TWO tokens (e.g. "git push", "npm run") so
# "always allow" is not dangerously broad (does not blanket-allow all of git).
SUBCOMMAND_TOOLS = {
    "git", "npm", "yarn", "pnpm", "docker", "cargo", "pip", "pip3",
    "kubectl", "brew", "apt", "apt-get", "gh", "make", "go", "bun",
}


def load_config():
    cfg = json.loads(json.dumps(DEFAULTS))  # deep copy
    path = os.path.join(HERE, "config.json")
    try:
        with open(path, "r") as f:
            user = json.load(f)
        for k, v in user.items():
            if k.startswith("//"):
                continue
            if k == "auto_allow" and isinstance(v, dict):
                for kk, vv in v.items():
                    if not kk.startswith("//"):
                        cfg["auto_allow"][kk] = vv
            else:
                cfg[k] = v
    except FileNotFoundError:
        pass
    except Exception as e:
        _raw_log(cfg, "config load error: %r" % e)
    return cfg


def expand(p):
    return os.path.expanduser(os.path.expandvars(p))


def _raw_log(cfg, msg):
    try:
        lf = expand(cfg.get("log_file", DEFAULTS["log_file"]))
        os.makedirs(os.path.dirname(lf), exist_ok=True)
        with open(lf, "a") as f:
            f.write("%s  %s\n" % (time.strftime("%Y-%m-%d %H:%M:%S"), msg))
    except Exception:
        pass


# ---- decision emitters -----------------------------------------------------
def emit(decision, reason):
    print(json.dumps({
        "hookSpecificOutput": {
            "hookEventName": "PreToolUse",
            "permissionDecision": decision,
            "permissionDecisionReason": reason,
        }
    }))
    sys.exit(0)


# ---- signature / matching --------------------------------------------------
# Substrings that turn one command into several, redirect I/O, or substitute
# shell output. If a Bash command contains ANY of these it is NOT eligible for
# auto-allow and does not collapse to a coarse cache signature -- otherwise
# "git status && rm -rf /" would ride in on the "git status" allow-prefix.
SHELL_CHAINING = ("&&", "||", ";", "|", "`", "$(", "${", ">", "<", "\n", "\r")


def has_shell_chaining(cmd):
    return any(tok in (cmd or "") for tok in SHELL_CHAINING)


def tokenize(cmd):
    try:
        import shlex
        return shlex.split(cmd)
    except Exception:
        return cmd.split()


def bash_signature(command):
    """Normalized signature for a Bash command used for auto-allow prefix
    matching and for the 'always' cache key. Uses first token, or first two
    tokens for subcommand-style tools."""
    command = command or ""
    # A command that chains/redirects must NOT collapse to a coarse "first token"
    # signature (that would let an "always" on plain `git status` also match
    # `git status && rm -rf`). Give it a stable, command-specific key instead.
    if has_shell_chaining(command):
        import hashlib
        return "chain#" + hashlib.sha1(command.strip().encode("utf-8", "replace")).hexdigest()[:16]
    toks = tokenize(command)
    # skip leading env assignments like FOO=bar
    while toks and "=" in toks[0] and not toks[0].startswith("/") and " " not in toks[0].split("=")[0]:
        toks = toks[1:]
    if not toks:
        return ""
    first = os.path.basename(toks[0])
    if first in SUBCOMMAND_TOOLS and len(toks) > 1:
        return "%s %s" % (first, toks[1])
    return first


def match_key(tool_name, tool_input):
    """Stable key identifying 'this kind of call' for the always-cache.
    Documented in README.
      Bash        -> "Bash::<sig>"           (sig = first token / first two)
      Write/Edit  -> "<Tool>::<abspath>"     (exact file)
      Web*        -> "<Tool>::<host>"        (domain)
      other       -> "<Tool>::*"
    """
    if tool_name == "Bash":
        return "Bash::" + bash_signature(tool_input.get("command", ""))
    if tool_name in ("Write", "Edit", "MultiEdit", "NotebookEdit"):
        p = tool_input.get("file_path") or tool_input.get("notebook_path") or ""
        return "%s::%s" % (tool_name, p)
    if tool_name in ("WebFetch", "WebSearch"):
        url = tool_input.get("url") or tool_input.get("query") or ""
        host = url
        try:
            from urllib.parse import urlparse
            if "://" in url:
                host = urlparse(url).netloc or url
        except Exception:
            pass
        return "%s::%s" % (tool_name, host)
    return "%s::*" % tool_name


def is_auto_allowed(cfg, tool_name, tool_input):
    aa = cfg.get("auto_allow", {})
    if tool_name in aa.get("tool_names", []):
        return True
    if tool_name == "Bash":
        cmd = (tool_input.get("command") or "").strip()
        # Never auto-allow a chained/redirected command: the safe read-only
        # prefix only guarantees the FIRST command is safe. Route it to the watch.
        if has_shell_chaining(cmd):
            return False
        toks = tokenize(cmd)
        norm = " ".join(os.path.basename(t) if i == 0 else t
                        for i, t in enumerate(toks))
        for pref in aa.get("bash_command_prefixes", []):
            pt = pref.split()
            if toks and [os.path.basename(toks[0])] + toks[1:len(pt)] == pt:
                return True
            # token-boundary prefix match
            if norm == pref or norm.startswith(pref + " "):
                return True
    return False


# ---- cache -----------------------------------------------------------------
def cache_path(cfg):
    return expand(cfg.get("cache_file", DEFAULTS["cache_file"]))


def cache_has(cfg, key):
    try:
        with open(cache_path(cfg), "r") as f:
            data = json.load(f)
        return key in data.get("allow", {})
    except FileNotFoundError:
        return False
    except Exception:
        return False


def cache_add(cfg, key, meta):
    path = cache_path(cfg)
    with _file_lock(path):
        data = {"allow": {}}
        try:
            with open(path, "r") as f:
                data = json.load(f)
        except Exception:
            pass
        data.setdefault("allow", {})[key] = {
            "added": time.strftime("%Y-%m-%d %H:%M:%S"),
            **meta,
        }
        try:
            os.makedirs(os.path.dirname(path), exist_ok=True)
            tmp = path + ".tmp"
            with open(tmp, "w") as f:
                json.dump(data, f, indent=2)
            os.replace(tmp, path)
        except Exception as e:
            _raw_log(cfg, "cache write error: %r" % e)


# ---- per-session "approve for the whole session" cache ----------------------
# A single watch tap returns scope:"session" -> that Claude Code window (keyed by
# session_id) auto-approves everything else for the rest of the session, with NO
# further watch buzzing. Entries are pruned after 24h so the file can't grow
# unbounded (session_ids are unique, so old ones never match a live session).
def session_cache_path(cfg):
    return expand(cfg.get("session_cache_file",
                          "~/.claude/claude-watch-approve-session.json"))


def session_has(cfg, session_id):
    if not session_id:
        return False
    try:
        with open(session_cache_path(cfg), "r") as f:
            data = json.load(f)
        return session_id in data.get("sessions", {})
    except Exception:
        return False


def session_add(cfg, session_id):
    if not session_id:
        return
    path = session_cache_path(cfg)
    with _file_lock(path):
        data = {"sessions": {}}
        try:
            with open(path, "r") as f:
                data = json.load(f)
        except Exception:
            pass
        sessions = data.setdefault("sessions", {})
        sessions[session_id] = {"added": int(time.time())}
        # prune entries older than 24h
        cutoff = int(time.time()) - 24 * 3600
        for sid in list(sessions):
            try:
                if int(sessions[sid].get("added", 0)) < cutoff:
                    del sessions[sid]
            except Exception:
                pass
        try:
            os.makedirs(os.path.dirname(path), exist_ok=True)
            tmp = path + ".tmp"
            with open(tmp, "w") as f:
                json.dump(data, f, indent=2)
            os.replace(tmp, path)
        except Exception as e:
            _raw_log(cfg, "session cache write error: %r" % e)


# ---- human summary for the watch card --------------------------------------
def human_summary(tool_name, tool_input):
    if tool_name == "Bash":
        c = (tool_input.get("command") or "").strip().replace("\n", " ")
        return "Run: " + (c[:120] + ("..." if len(c) > 120 else ""))
    if tool_name in ("Write", "Edit", "MultiEdit"):
        return "%s %s" % (tool_name, tool_input.get("file_path", "?"))
    if tool_name == "NotebookEdit":
        return "Edit notebook %s" % tool_input.get("notebook_path", "?")
    if tool_name == "WebFetch":
        return "Fetch %s" % tool_input.get("url", "?")
    if tool_name == "WebSearch":
        return "Search: %s" % tool_input.get("query", "?")
    return "%s call" % tool_name


# ---- bridge client ---------------------------------------------------------
def bridge_healthy(cfg):
    url = "http://%s:%s/health" % (cfg["bridge_host"], cfg["bridge_port"])
    try:
        req = urllib.request.Request(url, method="GET")
        with urllib.request.urlopen(req, timeout=cfg["health_timeout_sec"]) as r:
            return 200 <= r.status < 300
    except Exception:
        return False


def is_cod_pane_session(cfg):
    """True only for a Claude Code session that the Cod app spawned.

    Cod's spawn_pane exports COD_PANE=1 in the pane's shell before exec'ing
    `claude`, so the Claude Code process and every subprocess it starts --
    including this PreToolUse hook -- carry it. Any `claude` started from a
    normal terminal, VS Code, a launchd job, etc. does not, and for those this
    returns False and the Cod code path is never entered.

    The classifier child spawned by the broker itself has COD_PANE cleared and
    COD_BROKER_CLASSIFY=1 set (double guard against recursion), so it is also
    treated as a non-Cod session here.
    """
    if not cfg.get("cod_broker_enabled", True):
        return False
    if os.environ.get("COD_BROKER_CLASSIFY"):
        return False
    var = cfg.get("cod_pane_env_var", "COD_PANE") or "COD_PANE"
    return bool(os.environ.get(var))


def cod_broker_verdict(cfg, tool_name, tool_input, session_id, cwd, summary):
    """Ask the Cod broker whether this call is trivially safe.

    Returns "SAFE" only when the broker positively says so; returns None for
    every other outcome (disabled, recursion guard, broker down, HTTP error,
    timeout, bad JSON, UNSAFE, anything unrecognized). None means "no opinion" —
    the caller then continues into the unchanged watch-bridge path. The broker
    has no way to express a deny.
    """
    if not cfg.get("cod_broker_enabled", True):
        return None
    # The broker classifies by shelling out to `claude -p`, which is itself a
    # Claude Code session and would re-enter this hook. That child sets
    # COD_BROKER_CLASSIFY=1; seeing it here means "don't call the broker again".
    if os.environ.get("COD_BROKER_CLASSIFY"):
        return None
    host = cfg.get("cod_broker_host", "127.0.0.1")
    port = cfg.get("cod_broker_port", 8766)
    try:
        req = urllib.request.Request("http://%s:%s/health" % (host, port), method="GET")
        with urllib.request.urlopen(
                req, timeout=cfg.get("cod_broker_health_timeout_sec", 0.75)) as r:
            if not (200 <= r.status < 300):
                return None
    except Exception:
        return None  # Cod not running -> behave exactly as before.
    try:
        payload = json.dumps({
            "tool_name": tool_name,
            "tool_input": tool_input,
            "session_id": session_id,
            "cwd": cwd,
            "summary": summary,
        }).encode()
        req = urllib.request.Request("http://%s:%s/classify" % (host, port),
                                     data=payload, method="POST",
                                     headers={"Content-Type": "application/json"})
        with urllib.request.urlopen(
                req, timeout=cfg.get("cod_broker_request_timeout_sec", 10)) as r:
            resp = json.loads(r.read().decode())
        verdict = str(resp.get("verdict", "")).strip().upper()
        return "SAFE" if verdict == "SAFE" else None
    except Exception as e:
        _raw_log(cfg, "COD-BROKER-ERR err=%r -> fall through to watch" % (e,))
        return None


def ask_watch(cfg, title, body, origin):
    url = "http://%s:%s/permission" % (cfg["bridge_host"], cfg["bridge_port"])
    payload = json.dumps({"title": title, "body": body, "origin": origin}).encode()
    req = urllib.request.Request(url, data=payload, method="POST",
                                 headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=cfg["request_timeout_sec"]) as r:
        return json.loads(r.read().decode())


# ---- main ------------------------------------------------------------------
def main():
    cfg = load_config()
    if is_cod_pane_session(cfg):
        # Cod owns approvals for its own panes (bundled cod_hook.py + policy).
        # Exit silently so this hook never overrides Cod's decision.
        sys.exit(0)
    try:
        stdin = json.load(sys.stdin)
    except Exception as e:
        # Can't parse input -> defer to normal flow, never break the session.
        emit("ask", "watch-approve: bad hook input (%r)" % e)

    tool_name = stdin.get("tool_name", "")
    tool_input = stdin.get("tool_input", {}) or {}
    cwd = stdin.get("cwd", "")
    session_id = stdin.get("session_id", "")
    project = os.path.basename(cwd.rstrip("/")) if cwd else "?"

    # 1. Not a routed tool -> auto-allow, never buzz the watch.
    if tool_name not in cfg.get("route_tools", []):
        emit("allow", "watch-approve: %s not routed to watch" % tool_name)

    # 2. Auto-allow rule -> skip the watch.
    if is_auto_allowed(cfg, tool_name, tool_input):
        emit("allow", "watch-approve: %s matched auto-allow" % tool_name)

    key = match_key(tool_name, tool_input)

    # 3. This session already got a "yes for the whole session" -> auto-allow,
    #    no watch buzz.
    if session_has(cfg, session_id):
        _raw_log(cfg, "SESSION-ALLOW %s (%s)" % (session_id[:8], project))
        emit("allow", "watch-approve: session already approved on watch")

    # 3b. Previously approved "always" (persisted forever) -> auto-allow.
    if cache_has(cfg, key):
        _raw_log(cfg, "CACHE-ALLOW %s (%s)" % (key, project))
        emit("allow", "watch-approve: remembered approval for '%s'" % key)

    title = "Claude Code: %s" % tool_name
    body = human_summary(tool_name, tool_input)

    # 3c. Cod-spawned sessions ONLY (COD_PANE in this hook's environment).
    #     Cod is self-contained: ask its local broker, and if the answer is not
    #     a positive SAFE -- UNSAFE, unparseable, timeout, broker down, Cod not
    #     running -- go straight to the normal terminal permission prompt. The
    #     watch bridge is intentionally not consulted for these sessions.
    #     Every non-Cod session skips this block entirely (no /health probe, no
    #     network call) and continues to step 4 exactly as it always has.
    if is_cod_pane_session(cfg):
        if cod_broker_verdict(cfg, tool_name, tool_input, session_id, cwd, body) == "SAFE":
            _raw_log(cfg, "COD-ALLOW key=%s proj=%s" % (key, project))
            emit("allow", "cod-broker: classified safe, auto-approved")
        _raw_log(cfg, "COD-ASK key=%s proj=%s -> terminal prompt" % (key, project))
        emit("ask", "cod-broker: no safe verdict, use terminal prompt")

    # 4. Need a decision. Check the bridge is up (fast) before we block on it.
    if not bridge_healthy(cfg):
        _raw_log(cfg, "BRIDGE-DOWN key=%s proj=%s -> %s" %
                 (key, project, "allow" if cfg.get("fail_open_on_bridge_down") else "ask"))
        if cfg.get("fail_open_on_bridge_down"):
            emit("allow", "watch-approve: bridge down, fail-open configured")
        emit("ask", "watch-approve: bridge unreachable, use terminal prompt")

    # title/body computed above (step 3c) and reused unchanged here.
    origin = "%s  (session %s)" % (project or cwd, session_id[:8])

    try:
        resp = ask_watch(cfg, title, body, origin)
    except (urllib.error.URLError, socket.timeout, TimeoutError) as e:
        _raw_log(cfg, "BRIDGE-ERR key=%s proj=%s err=%r -> ask" % (key, project, e))
        emit("ask", "watch-approve: bridge error, use terminal prompt")
    except Exception as e:
        _raw_log(cfg, "BRIDGE-EXC key=%s proj=%s err=%r -> ask" % (key, project, e))
        emit("ask", "watch-approve: bridge exception, use terminal prompt")

    if resp.get("timedOut"):
        _raw_log(cfg, "TIMEOUT key=%s proj=%s -> ask" % (key, project))
        emit("ask", "watch-approve: watch did not respond, use terminal prompt")

    approve = bool(resp.get("approve"))
    scope = resp.get("scope", "once")

    if approve and scope == "session":
        session_add(cfg, session_id)
        _raw_log(cfg, "WATCH-ALLOW-SESSION sid=%s proj=%s" % (session_id[:8], project))
        emit("allow", "watch-approve: approved on watch (whole session)")
    if approve and scope == "always":
        cache_add(cfg, key, {"tool": tool_name, "summary": body, "cwd": cwd})
        _raw_log(cfg, "WATCH-ALLOW-ALWAYS key=%s proj=%s" % (key, project))
        emit("allow", "watch-approve: approved on watch (remembered as always)")
    if approve:
        _raw_log(cfg, "WATCH-ALLOW-ONCE key=%s proj=%s" % (key, project))
        emit("allow", "watch-approve: approved on watch")

    _raw_log(cfg, "WATCH-DENY key=%s proj=%s" % (key, project))
    emit("deny", "watch-approve: denied on watch")


if __name__ == "__main__":
    main()
