const C = "gorcave-console-v1";
const SHELL = ["gorcave-app.html","gorcave-app-manifest.json","gorcave-icon-192.png","gorcave-icon-512.png"];
self.addEventListener("install", e => { self.skipWaiting();
  e.waitUntil(caches.open(C).then(c => c.addAll(SHELL)).catch(()=>{})); });
self.addEventListener("activate", e => e.waitUntil(self.clients.claim()));
self.addEventListener("fetch", e => {
  const u = new URL(e.request.url);
  if (u.origin === location.origin && e.request.method === "GET") {
    e.respondWith(caches.match(e.request).then(r => r || fetch(e.request)));
  }
});
