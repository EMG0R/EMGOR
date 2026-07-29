/**
 * WebChuGL Service Worker
 *
 * COOP/COEP header injection for cross-origin isolation (SharedArrayBuffer).
 * Based on coi-serviceworker by Guido Zuidhof (MIT).
 *
 * Also provides dynamic caching (stale-while-revalidate) for same-origin
 * assets for PWA / offline support. No hardcoded asset list — caching
 * happens on first fetch regardless of deployment layout.
 */

var CACHE_NAME = 'webchugl-v23';

// This worker must ONLY control /ciesen.html (registered with
// { scope: './ciesen.html' }). Legacy versions were registered at root
// scope and controlled the whole site; when such a registration updates
// to this script, it unregisters itself.
function isCiesenScope() {
    try {
        return new URL(self.registration.scope).pathname.indexOf('ciesen') !== -1;
    } catch (e) {
        return false;
    }
}

// ── Install ─────────────────────────────────────────────────────────
self.addEventListener('install', function() { self.skipWaiting(); });

// ── Activate: clean old caches, claim clients ────────────────────────
self.addEventListener('activate', function(event) {
    event.waitUntil(
        caches.keys().then(function(keys) {
            return Promise.all(
                keys.filter(function(k) { return k !== CACHE_NAME; })
                    .map(function(k) { return caches.delete(k); })
            );
        }).then(function() {
            // Legacy root-scope registration: self-destruct instead of claiming.
            if (!isCiesenScope()) {
                return self.registration.unregister();
            }
            return self.clients.claim();
        })
    );
});

// ── Message handling ────────────────────────────────────────────────
self.addEventListener('message', function(ev) {
    if (!ev.data) return;
    // Only accept messages from same-origin clients
    if (ev.source && new URL(ev.source.url).origin !== self.location.origin) return;
    if (ev.data.type === 'deregister') {
        self.registration.unregister().then(function() {
            return self.clients.matchAll();
        }).then(function(clients) {
            clients.forEach(function(client) { client.navigate(client.url); });
        });
    }
});

// ── Fetch: COOP/COEP headers + dynamic caching ──────────────────────
self.addEventListener('fetch', function(event) {
    var r = event.request;

    if (r.method !== 'GET') return;

    // Chrome bug: only-if-cached with mode !== same-origin causes fetch to fail
    if (r.cache === 'only-if-cached' && r.mode !== 'same-origin') return;

    // Don't intercept SSE / EventSource streams (causes CORS preflight issues)
    if (r.headers.get('accept') === 'text/event-stream') return;

    var isSameOrigin = new URL(r.url).origin === self.location.origin;

    // Cross-origin requests: need CORS headers for require-corp COEP.
    // jsdelivr and other major CDNs send Access-Control-Allow-Origin: *
    // so dynamic import() (which uses CORS mode) works fine.
    // Don't intercept cross-origin — browser handles CORS natively.
    if (!isSameOrigin) return;

    // Legacy root-scope registration: don't touch anything while the
    // activate self-unregister is pending.
    if (!isCiesenScope()) return;

    var url = new URL(r.url);
    var path = url.pathname;

    // Site data files must never be served stale: galaxy.json, universe/**,
    // and any markdown. Network-first, no cache writes.
    if (path.indexOf('/universe/') !== -1 ||
        /\/galaxy\.json$/.test(path) ||
        /\.(json|md)$/.test(path)) {
        event.respondWith(
            fetch(r).catch(function() { return caches.match(r); })
                .then(function(resp) { return resp || fetch(r); })
        );
        return;
    }

    // ChucK sources are fetched with a Date.now() cache-buster — caching
    // them would grow the cache unboundedly. Pass through.
    if (/\.ck$/.test(path)) return;

    // Same-origin: stale-while-revalidate cache + COI headers
    event.respondWith(
        caches.match(r).then(function(cached) {
            if (cached) {
                // Serve from cache now, refresh in background
                fetch(r).then(function(response) {
                    if (response.ok) {
                        caches.open(CACHE_NAME).then(function(cache) {
                            cache.put(r, response);
                        });
                    }
                }).catch(function() {});
                return addCoiHeaders(cached, r);
            }

            // Not cached — fetch, cache, serve
            return fetch(r).then(function(response) {
                if (response.ok) {
                    var clone = response.clone();
                    caches.open(CACHE_NAME).then(function(cache) {
                        cache.put(r, clone);
                    });
                }
                return addCoiHeaders(response, r);
            });
        })
    );
});

// ── Helpers ──────────────────────────────────────────────────────────

function addCoiHeaders(response, request) {
    if (response.type === 'opaque' || response.type === 'opaqueredirect' || !response.headers) {
        return response;
    }

    // COEP/COOP are only needed by ciesen (WebChuGL needs SharedArrayBuffer).
    // Other pages (NEPTR, etc.) must NOT get these headers — COEP require-corp
    // would block the RNBO CDN script which has no Cross-Origin-Resource-Policy.
    if (!request || !request.url.includes('ciesen')) {
        return response;
    }

    var headers = new Headers(response.headers);
    headers.set('Cross-Origin-Opener-Policy', 'same-origin');
    headers.set('Cross-Origin-Embedder-Policy', 'require-corp');

    return new Response(response.body, {
        status: response.status,
        statusText: response.statusText,
        headers: headers
    });
}
