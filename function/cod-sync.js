// =============================================================================
// cod-sync — persistent relay endpoint between the Cod desktop app (Mac) and
// the Cod Remote mobile page (phone). Neither side talks to the other
// directly; both talk only to this site.
//
// Storage: Netlify Blobs (@netlify/blobs), a serverless KV store. Netlify
// Functions run in ephemeral containers in production, so plain filesystem
// writes (see log-responses.js) do NOT reliably persist between invocations
// — blobs do.
//
// WIRE FORMAT (source of truth for the Mac-side Rust client and the Android
// shell — keep both in sync with this file if you change it):
//
//   POST /.netlify/functions/cod-sync
//     Called by the Mac (Cod app) to push its latest state and, in the same
//     round trip, pull any commands the phone queued up while it was away.
//     Request body (JSON):
//       {
//         "deviceId": "<string, required>",
//         "secret":   "<string, required, must match COD_RELAY_SECRET>",
//         "snapshot": { ...arbitrary JSON, Mac-defined shape... }
//       }
//     Response 200 (JSON):
//       {
//         "ok": true,
//         "commands": [ ...array of command objects that were queued for
//                       this deviceId, now delivered and cleared... ]
//       }
//     Response 400: malformed JSON / missing deviceId or snapshot.
//     Response 401: missing/incorrect secret.
//
//   GET /.netlify/functions/cod-sync?deviceId=...&secret=...
//     Called by the phone (Cod Remote) to read the Mac's latest pushed
//     snapshot.
//     Response 200 (JSON):
//       { "ok": true, "snapshot": {...} | null }
//       snapshot is null if the Mac has never pushed one for this deviceId.
//     Response 400: missing deviceId/secret query params.
//     Response 401: missing/incorrect secret.
//
// Blob keys used (in the default, unnamed store "cod-relay"):
//   cod-snapshot:${deviceId}   -> JSON string of the last snapshot pushed by the Mac
//   cod-commands:${deviceId}   -> JSON string of an array of queued command objects
//                                 (owned/written by cod-command.js, drained here)
//
// Commands are delivered at-most-once: a POST here reads cod-commands:${id}
// and immediately clears it, so the same command is never handed out twice.
// =============================================================================

const { getStore } = require('@netlify/blobs');

const SNAPSHOT_PREFIX = 'cod-snapshot:';
const COMMANDS_PREFIX = 'cod-commands:';

function json(statusCode, obj) {
    return {
        statusCode,
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(obj),
    };
}

function checkSecret(provided) {
    const expected = process.env.COD_RELAY_SECRET;
    return typeof expected === 'string' && expected.length > 0 &&
        typeof provided === 'string' && provided === expected;
}

exports.handler = async function (event) {
    try {
        const store = getStore('cod-relay');

        if (event.httpMethod === 'GET') {
            const params = event.queryStringParameters || {};
            const deviceId = params.deviceId;
            const secret = params.secret;

            if (!deviceId || !secret) {
                return json(400, { error: 'deviceId and secret query params are required' });
            }
            if (!checkSecret(secret)) {
                return json(401, { error: 'unauthorized' });
            }

            const raw = await store.get(SNAPSHOT_PREFIX + deviceId, { type: 'text' });
            let snapshot = null;
            if (raw) {
                try { snapshot = JSON.parse(raw); } catch (e) { snapshot = null; }
            }
            return json(200, { ok: true, snapshot });
        }

        if (event.httpMethod === 'POST') {
            let payload;
            try {
                payload = JSON.parse(event.body || '{}');
            } catch (e) {
                return json(400, { error: 'malformed JSON body' });
            }

            const { deviceId, secret, snapshot } = payload || {};

            if (!deviceId || typeof deviceId !== 'string') {
                return json(400, { error: 'deviceId is required' });
            }
            if (snapshot === undefined || snapshot === null || typeof snapshot !== 'object') {
                return json(400, { error: 'snapshot (object) is required' });
            }
            if (!checkSecret(secret)) {
                return json(401, { error: 'unauthorized' });
            }

            // Store the new snapshot.
            await store.set(SNAPSHOT_PREFIX + deviceId, JSON.stringify(snapshot));

            // Drain any queued outbound commands for this device (at-most-once
            // delivery: read, then delete, before responding).
            const rawCommands = await store.get(COMMANDS_PREFIX + deviceId, { type: 'text' });
            let commands = [];
            if (rawCommands) {
                try {
                    const parsed = JSON.parse(rawCommands);
                    if (Array.isArray(parsed)) commands = parsed;
                } catch (e) { commands = []; }
            }
            if (commands.length > 0) {
                await store.delete(COMMANDS_PREFIX + deviceId);
            }

            return json(200, { ok: true, commands });
        }

        return json(405, { error: 'method not allowed' });
    } catch (err) {
        // Never throw uncaught — always degrade to a clean error response.
        console.error('cod-sync error:', err);
        return json(500, { error: 'internal error' });
    }
};
