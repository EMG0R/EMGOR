// =============================================================================
// cod-command — phone -> relay leg of the Cod sync relay. See cod-sync.js for
// the full picture and the storage rationale (Netlify Blobs, not filesystem).
//
// WIRE FORMAT (source of truth — keep the Mac-side Rust client and the
// Android shell in sync with this file if you change it):
//
//   POST /.netlify/functions/cod-command
//     Called by the phone (Cod Remote) to queue a command for the Mac to
//     pick up on its next cod-sync POST.
//     Request body (JSON):
//       {
//         "deviceId": "<string, required>",
//         "secret":   "<string, required, must match COD_RELAY_SECRET>",
//         "command":  {
//           "type": "approve" | "deny" | "inject" | "chat" | ...,
//           ...additional fields depend on type, e.g.:
//           //   approve/deny -> { "id": "<approval id>" }
//           //   chat         -> { "message": "<text>", "targetSession": "<optional>" }
//           //   inject       -> whatever the Mac-side agent expects
//         }
//       }
//     Response 200 (JSON): { "ok": true }
//     Response 400: malformed JSON / missing deviceId or command.
//     Response 401: missing/incorrect secret.
//
// Storage: commands are appended to a JSON array stored at
//   cod-commands:${deviceId}
// in the same "cod-relay" blob store cod-sync.js reads/writes. cod-sync.js's
// POST handler drains (reads + clears) this array on the Mac's next check-in,
// so delivery is at-most-once and commands do not pile up indefinitely.
// =============================================================================

const { getStore } = require('@netlify/blobs');

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
    if (event.httpMethod !== 'POST') {
        return json(405, { error: 'method not allowed' });
    }

    try {
        let payload;
        try {
            payload = JSON.parse(event.body || '{}');
        } catch (e) {
            return json(400, { error: 'malformed JSON body' });
        }

        const { deviceId, secret, command } = payload || {};

        if (!deviceId || typeof deviceId !== 'string') {
            return json(400, { error: 'deviceId is required' });
        }
        if (command === undefined || command === null || typeof command !== 'object') {
            return json(400, { error: 'command (object) is required' });
        }
        if (!checkSecret(secret)) {
            return json(401, { error: 'unauthorized' });
        }

        const store = getStore('cod-relay');
        const key = COMMANDS_PREFIX + deviceId;

        const raw = await store.get(key, { type: 'text' });
        let commands = [];
        if (raw) {
            try {
                const parsed = JSON.parse(raw);
                if (Array.isArray(parsed)) commands = parsed;
            } catch (e) { commands = []; }
        }

        commands.push(command);
        await store.set(key, JSON.stringify(commands));

        return json(200, { ok: true });
    } catch (err) {
        // Never throw uncaught — always degrade to a clean error response.
        console.error('cod-command error:', err);
        return json(500, { error: 'internal error' });
    }
};
