const C = "gorcave-console-v3";
const SHELL = ["gorcave-app.html","gorcave-app-manifest.json","gorcave-icon-192.png","gorcave-icon-512.png"];
self.addEventListener("install", e => { self.skipWaiting();
  e.waitUntil(caches.open(C).then(c => c.addAll(SHELL)).catch(()=>{})); });
self.addEventListener("activate", e => e.waitUntil((async () => {
  const keys = await caches.keys();
  await Promise.all(keys.filter(k => k !== C).map(k => caches.delete(k)));  // purge old versions
  await self.clients.claim();
})()));
self.addEventListener("fetch", e => {
  const u = new URL(e.request.url);
  if (u.origin !== location.origin || e.request.method !== "GET") return;
  // network-first for pages so updates land live; cache is only the offline fallback
  if (e.request.mode === "navigate" || u.pathname.endsWith(".html")) {
    e.respondWith(
      fetch(e.request).then(r => { const c = r.clone(); caches.open(C).then(x => x.put(e.request, c)); return r; })
                      .catch(() => caches.match(e.request)));
  } else {
    e.respondWith(caches.match(e.request).then(r => r || fetch(e.request)));
  }
});
