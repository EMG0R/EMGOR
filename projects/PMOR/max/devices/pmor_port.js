// pmor_port.js -- creates / removes the port object for the PMOR CDC link ON REQUEST ONLY.
// Max's port object opens its port the moment it is created, so it is not part of the saved patcher:
// loading a PMOR device never touches a port. Messages: openport <letter>, closeport (NOT open/close: those are [js] built-ins, see the end of the file).
//
// TEST HOOK (not for performing): `openport udp<port>` (e.g. `openport udp47000`) does NOT create a [serial]. It creates
//   [prepend rawbytes] -> [udpsend 127.0.0.1 <port>]   and   [udpreceive <port+1> @outputformat rawbytes] -> [route rawbytes]
// wired to/from proto exactly like the serial port (bytes as a list), so the devices can be driven headless against
// tools/simpmor_udp (the simulated PMOR on 127.0.0.1:<port>, replies to <port+1>); see tests/max_e2e/run.sh.
// `close` on a udp link sends the datagram PORTCLOSE first (the sim treats it as the port closing), then removes the objects.
// Letters a..f keep the old behaviour (serial).
inlets = 1; outlets = 0;
var port = null, objs = [], udpPort = 0, closer = null;
function remove_all() {
    for (var i = 0; i < objs.length; i++) this.patcher.remove(objs[i]);
    objs = []; port = null;
}
// debug probes: OFF unless `set on 1` was stored in [js]-global "pmor_dbg" (Max: send `pmor_dbg 1` via pmor_session.js `debug 1`)
function dbgOn() { try { return new Global("pmor_dbg").on == 1; } catch (e) { return false; } }
function dbg(t) { if (!dbgOn()) return; try { var f = new File("/tmp/pmor_m4l_debug.log", "write"); f.position = f.eof; f.writeline(new Date().toISOString() + " port " + t); f.close(); } catch (e) {} }
function loadbang() { dbg("loaded"); }
function openport(letter) {
    dbg("openport " + letter);
    closeport(true);
    var proto = this.patcher.getnamed("proto");
    if (!proto) { error("pmor_port: no object named proto\n"); return; }
    var s = String(letter || "a"), m = /^udp(\d+)$/.exec(s);
    if (m) {
        udpPort = parseInt(m[1], 10);
        var pre = this.patcher.newdefault(160, 300, "prepend", "rawbytes");
        var snd = this.patcher.newdefault(160, 330, "udpsend", "127.0.0.1", udpPort);
        var rcv = this.patcher.newdefault(260, 300, "udpreceive", udpPort + 1, "@outputformat", "rawbytes");
        var rt = this.patcher.newdefault(260, 330, "route", "rawbytes");     // udpreceive @outputformat rawbytes emits `rawbytes <bytes...>`
        objs = [pre, snd, rcv, rt]; port = snd;
        this.patcher.connect(proto, 0, pre, 0);
        this.patcher.connect(pre, 0, snd, 0);
        this.patcher.connect(rcv, 0, rt, 0);
        this.patcher.connect(rt, 0, proto, 0);
        return;
    }
    dbg("creating serial " + s); port = this.patcher.newdefault(160, 300, "serial", s, 115200); dbg("serial created " + (port ? "ok" : "null"));
    if (port) { port.message("dtr", 1); port.message("rts", 1); dbg("dtr+rts on"); }   // PMOR (TinyUSB CDC) drops bytes until the host raises DTR
    // [serial] only outputs received bytes when banged: a [metro 5] polls it, created and removed with the port.
    var poll = this.patcher.newdefault(60, 270, "metro", 5);
    objs = [port, poll];
    this.patcher.connect(poll, 0, port, 0);
    this.patcher.connect(port, 0, proto, 0);
    this.patcher.connect(proto, 0, port, 0);
    poll.message("int", 1); dbg("serial poll on");
}
function closeport(now) {
    if (closer) { closer.cancel(); closer = null; }
    if (!port) return;
    if (objs.length === 4) {                                   // udp test link: tell the sim, give the datagram time to leave
        var pre = objs[0], bytes = [], t = "PORTCLOSE";
        for (var i = 0; i < t.length; i++) bytes.push(t.charCodeAt(i));
        pre.message.apply(pre, ["list"].concat(bytes));        // feed the prepend directly (proto's wire stays until removal)
        if (now === true) { remove_all.call(this); return; }
        closer = new Task(function () { closer = null; remove_all.call(this); }, this);
        closer.schedule(60);
        return;
    }
    remove_all.call(this);
}
// [js] has built-in messages named `open` (opens its editor) and `compile`/`read`/...: a patcher message `open <letter>` NEVER reaches a js function
// called open (measured in Max 9.1.4: "extra arguments for message open"). The device must send `openport <letter>` / `closeport`.
// open/close are kept as aliases for anything that can reach them (e.g. called from another js), the built-ins win for patcher messages.
function open(letter) { openport(letter); }
function close(now) { closeport(now); }
