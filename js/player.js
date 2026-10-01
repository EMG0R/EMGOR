// EMGOR in-site audio player. Title over a waveform you can scrub, one track at a time page-wide.
// Usage: <div data-player data-peaks="resources/peaks.json" data-tracks='[{"name":"..","src":".."}]'></div>
//        or EmgorPlayer.mount(el, tracks, { peaks: {src: [..]} })
(function () {
    'use strict';

    var active = null;
    // plain shapes, not text glyphs (no emoji fallback on phones, exact centering)
    var PLAY = '<svg viewBox="0 0 20 20" aria-hidden="true"><path d="M6 3.5v13l10.5-6.5z"/></svg>';
    var STOP = '<svg viewBox="0 0 20 20" aria-hidden="true"><rect x="5" y="5" width="10" height="10" rx="1.5"/></svg>';
    var DPR = Math.min(window.devicePixelRatio || 1, 2);
    var COL_DIM = 'rgba(241,236,248,0.22)', COL_PLAY = '#F5E663', COL_HOVER = 'rgba(143,217,255,0.45)';

    function formatTime(s) {
        if (!s || !isFinite(s)) return '0:00';
        var m = Math.floor(s / 60), sec = Math.floor(s % 60);
        return m + ':' + (sec < 10 ? '0' : '') + sec;
    }

    function flatPeaks() { var a = []; for (var i = 0; i < 160; i++) a.push(0.35 + 0.15 * Math.sin(i * 0.4)); return a; }

    function mount(container, tracks, opts) {
        if (typeof container === 'string') container = document.querySelector(container);
        if (!container || !tracks || !tracks.length) return;
        opts = opts || {};
        var peaksMap = opts.peaks || {};

        var wrap = document.createElement('div');
        wrap.className = 'ep';

        tracks.forEach(function (track) {
            var row = document.createElement('div');
            row.className = 'ep-row';

            var btn = document.createElement('button');
            btn.type = 'button'; btn.className = 'ep-play'; btn.innerHTML = PLAY;
            btn.setAttribute('aria-label', 'play ' + track.name);

            var body = document.createElement('div');
            body.className = 'ep-body';

            var head = document.createElement('div');
            head.className = 'ep-head';
            var name = document.createElement('span');
            name.className = 'ep-name'; name.textContent = track.name;
            var timeEl = document.createElement('span');
            timeEl.className = 'ep-time'; timeEl.textContent = '0:00';
            head.appendChild(name); head.appendChild(timeEl);
            if (track.download) {
                var dl = document.createElement('a');
                dl.className = 'ep-dl'; dl.href = track.src; dl.setAttribute('download', '');
                dl.innerHTML = '&#10515;'; dl.setAttribute('aria-label', 'download ' + track.name);
                head.appendChild(dl);
            }

            var wave = document.createElement('canvas');
            wave.className = 'ep-wave';
            wave.setAttribute('role', 'slider'); wave.setAttribute('tabindex', '0');
            wave.setAttribute('aria-label', track.name + ' scrubber');
            wave.setAttribute('aria-valuemin', '0'); wave.setAttribute('aria-valuemax', '100'); wave.setAttribute('aria-valuenow', '0');

            body.appendChild(head); body.appendChild(wave);
            row.appendChild(btn); row.appendChild(body);
            wrap.appendChild(row);

            var peaks = track.peaks || peaksMap[track.src] || null;
            var audio = null, frac = 0, hover = -1, pendingSeek = -1, scrubbing = false;
            var ctx = wave.getContext('2d'), W = 0, H = 0;

            function draw() {
                var p = peaks || flatPeaks();
                ctx.setTransform(DPR, 0, 0, DPR, 0, 0);
                ctx.clearRect(0, 0, W, H);
                var n = p.length, bw = W / n, gap = Math.min(1.5, bw * 0.3), mid = H / 2;
                for (var i = 0; i < n; i++) {
                    var x = i * bw, pos = (i + 0.5) / n;
                    var h = Math.max(2, p[i] * (H - 4));
                    ctx.fillStyle = pos <= frac ? COL_PLAY : (hover >= 0 && pos <= hover ? COL_HOVER : COL_DIM);
                    ctx.fillRect(x + gap / 2, mid - h / 2, bw - gap, h);
                }
            }
            function size() {
                var r = wave.getBoundingClientRect();
                if (!r.width) return;
                W = r.width; H = r.height;
                wave.width = Math.round(W * DPR); wave.height = Math.round(H * DPR);
                draw();
            }
            if (window.ResizeObserver) new ResizeObserver(size).observe(wave); else addEventListener('resize', size);
            setTimeout(size, 0);

            function setFrac(f) {
                frac = Math.max(0, Math.min(1, f));
                wave.setAttribute('aria-valuenow', String(Math.round(frac * 100)));
                draw();
            }
            function reset() {
                btn.innerHTML = PLAY; btn.classList.remove('playing');
                setFrac(0);
                timeEl.textContent = '0:00' + (audio && audio.duration ? ' / ' + formatTime(audio.duration) : '');
            }
            function ensureAudio() {
                if (audio) return audio;
                audio = new Audio();
                audio.preload = 'metadata';
                audio.src = track.src;
                audio.addEventListener('loadedmetadata', function () {
                    if (pendingSeek >= 0) { audio.currentTime = pendingSeek * audio.duration; pendingSeek = -1; }
                    timeEl.textContent = formatTime(audio.currentTime) + ' / ' + formatTime(audio.duration);
                });
                audio.addEventListener('timeupdate', function () {
                    if (!audio.duration || scrubbing) return;
                    setFrac(audio.currentTime / audio.duration);
                    timeEl.textContent = formatTime(audio.currentTime) + ' / ' + formatTime(audio.duration);
                });
                audio.addEventListener('ended', function () { if (active && active.audio === audio) { active = null; } reset(); });
                return audio;
            }
            function stopOthers() {
                if (active && active.audio !== audio) { active.audio.pause(); active.reset(); active = null; }
            }
            function play() {
                var a = ensureAudio();
                stopOthers();
                active = { audio: a, reset: reset };
                a.play();
                btn.innerHTML = STOP; btn.classList.add('playing');
            }
            btn.addEventListener('click', function () {
                var a = ensureAudio();
                if (!a.paused) { a.pause(); btn.innerHTML = PLAY; btn.classList.remove('playing'); return; }
                play();
            });

            function seekTo(f) {
                f = Math.max(0, Math.min(1, f));
                var a = ensureAudio();
                setFrac(f);
                if (a.duration) { a.currentTime = f * a.duration; timeEl.textContent = formatTime(a.currentTime) + ' / ' + formatTime(a.duration); }
                else pendingSeek = f;
            }
            function fracFromEvent(e) { var r = wave.getBoundingClientRect(); return r.width ? (e.clientX - r.left) / r.width : 0; }

            wave.addEventListener('pointerdown', function (e) {
                e.preventDefault(); scrubbing = true;
                try { wave.setPointerCapture(e.pointerId); } catch (err) {}
                seekTo(fracFromEvent(e));
            });
            wave.addEventListener('pointermove', function (e) {
                if (scrubbing) seekTo(fracFromEvent(e));
                else if (e.pointerType === 'mouse') { hover = fracFromEvent(e); draw(); }
            });
            function end(e) {
                if (!scrubbing) return;
                scrubbing = false;
                try { wave.releasePointerCapture(e.pointerId); } catch (err) {}
                var a = ensureAudio();
                if (a.paused) play();
            }
            wave.addEventListener('pointerup', end);
            wave.addEventListener('pointercancel', function () { scrubbing = false; });
            wave.addEventListener('pointerleave', function () { hover = -1; draw(); });
            wave.addEventListener('keydown', function (e) {
                var a = ensureAudio();
                if (e.key === 'ArrowLeft') { seekTo(frac - (a.duration ? 5 / a.duration : 0.02)); e.preventDefault(); }
                else if (e.key === 'ArrowRight') { seekTo(frac + (a.duration ? 5 / a.duration : 0.02)); e.preventDefault(); }
                else if (e.key === ' ' || e.key === 'Enter') { btn.click(); e.preventDefault(); }
            });
        });

        container.innerHTML = '';
        container.appendChild(wrap);
    }

    function autoMount() {
        var nodes = document.querySelectorAll('[data-player]');
        Array.prototype.forEach.call(nodes, function (node) {
            var tracks;
            try { tracks = JSON.parse(node.getAttribute('data-tracks') || '[]'); } catch (e) { return; }
            if (!Array.isArray(tracks) || !tracks.length) return;
            var src = node.getAttribute('data-peaks');
            if (src) {
                fetch(src).then(function (r) { return r.json(); })
                    .then(function (peaks) { mount(node, tracks, { peaks: peaks }); })
                    .catch(function () { mount(node, tracks, {}); });
            } else mount(node, tracks, {});
        });
    }

    window.EmgorPlayer = { mount: mount };
    document.addEventListener('DOMContentLoaded', autoMount);
})();
