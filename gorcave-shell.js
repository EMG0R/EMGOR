// gorcave page shell: nothing real is in this file or in the page that loads it.
// Knock -> phone approves -> token -> the real page is fetched from the gate and
// written over this document (same URL, same origin, same localStorage).
(function () {
  "use strict";
  var NAME = window.GORCAVE_PAGE;
  // The gate address is not in this public repo. It is typed (or arrives as
  // #api=https://host from the phone app) once per browser and kept in localStorage.
  var API = "";
  var $ = function (id) { return document.getElementById(id); };
  var LS = {
    get: function (k) { try { return localStorage.getItem(k); } catch (e) { return null; } },
    set: function (k, v) { try { localStorage.setItem(k, v); } catch (e) {} },
    del: function (k) { try { localStorage.removeItem(k); } catch (e) {} }
  };
  var timer = null;

  function okBase(v) { return /^https:\/\/[a-z0-9.-]+\.ts\.net$/i.test(v); }
  function findBase() {
    var m = /[#&]api=([^&]+)/.exec(location.hash || "");
    if (m) {
      var v = decodeURIComponent(m[1]).replace(/\/+$/, "");
      if (okBase(v)) LS.set("gorcave_api", v);
      try { history.replaceState(null, "", location.pathname + location.search); } catch (e) {}
    }
    var b = LS.get("gorcave_api") || "";
    API = okBase(b) ? b : "";
    return !!API;
  }

  function say(msg, cls) { var m = $("sMsg"); m.textContent = msg || ""; m.className = "msg" + (cls ? " " + cls : ""); }
  function saveToken(tok, ttl) {
    LS.set("gorcave_token", tok);
    LS.set("gorcave_token_exp", String(Math.floor(Date.now() / 1000) + (ttl || 31536000)));
  }
  function clearToken() { LS.del("gorcave_token"); LS.del("gorcave_token_exp"); }

  async function load() {
    if (!findBase()) { $("sBase").classList.remove("hidden"); say("enter the gorcave address (once)"); return; }
    $("sBase").classList.add("hidden");
    var tok = LS.get("gorcave_token");
    if (!tok) return knock();
    say("loading...");
    var r;
    try { r = await fetch(API + "/api/gate/page/" + NAME, { headers: { Authorization: "Bearer " + tok }, cache: "no-store" }); }
    catch (e) { say("cannot reach gorcave", "err"); $("sRetry").classList.remove("hidden"); return; }
    if (r.status === 401) { clearToken(); return knock(); }
    if (!r.ok) { say("gorcave said " + r.status, "err"); $("sRetry").classList.remove("hidden"); return; }
    var html = await r.text();
    document.open(); document.write(html); document.close();
  }

  async function knock() {
    $("sRetry").classList.add("hidden");
    say("asking your phone...");
    var k, r;
    try {
      r = await fetch(API + "/api/gate/approve/request", {
        method: "POST", headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ what: NAME + " page" })
      });
      k = await r.json();
    } catch (e) { say("cannot reach gorcave", "err"); $("sRetry").classList.remove("hidden"); return; }
    if (!r.ok) { say(k.detail || "could not ask", "err"); $("sRetry").classList.remove("hidden"); return; }
    $("sCode").textContent = k.code;
    $("sCodeBox").classList.remove("hidden");
    var left = k.expires_in || 120;
    say("approve on your phone, match the code");
    clearInterval(timer);
    timer = setInterval(async function () {
      left -= 2;
      var d;
      try {
        var c = await fetch(API + "/api/gate/approve/claim", {
          method: "POST", headers: { "Content-Type": "application/json" },
          body: JSON.stringify({ request_id: k.request_id, claim: k.claim })
        });
        d = await c.json();
      } catch (e) { return; }
      if (d.state === "approved") { clearInterval(timer); saveToken(d.token, d.ttl); $("sCodeBox").classList.add("hidden"); load(); return; }
      if (d.state === "denied" || d.state === "expired" || left <= 0) {
        clearInterval(timer);
        $("sCodeBox").classList.add("hidden");
        say(d.state === "denied" ? "denied on your phone" : "timed out", "err");
        $("sRetry").classList.remove("hidden");
        return;
      }
      say("waiting for approval... " + Math.max(0, left) + "s");
    }, 2000);
  }

  $("sBaseGo").onclick = function () {
    var v = $("sBaseIn").value.trim().replace(/\/+$/, "");
    if (v && !/^https?:\/\//.test(v)) v = "https://" + v;
    if (!okBase(v)) { say("not a gorcave address", "err"); return; }
    LS.set("gorcave_api", v); load();
  };
  $("sRetry").onclick = function () { load(); };
  $("sLostGo").onclick = async function () {
    var p = $("sPass").value, c = $("sTotp").value.trim();
    if (!p || !c) { say("passphrase and 6-digit code", "err"); return; }
    say("...");
    try {
      var r = await fetch(API + "/api/gate/login", {
        method: "POST", headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ passphrase: p, code: c })
      });
      var d = await r.json();
      if (!r.ok) throw new Error(d.detail || "denied");
      $("sPass").value = ""; $("sTotp").value = "";
      clearInterval(timer);
      saveToken(d.token, d.ttl);
      load();
    } catch (e) { say(e.message, "err"); }
  };
  load();
})();
