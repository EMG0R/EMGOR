(function () {
    'use strict';

    // shared "only one track plays at a time" state across every mounted player
    var active = null; // { audio, btn, row, fill, timeEl }

    function formatTime(s) {
        if (!s || !isFinite(s)) return '0:00';
        var m = Math.floor(s / 60);
        var sec = Math.floor(s % 60);
        return m + ':' + (sec < 10 ? '0' : '') + sec;
    }

    function stopActive() {
        if (!active) return;
        active.audio.pause();
        try { active.audio.currentTime = 0; } catch (e) {}
        active.btn.textContent = '▶';
        active.btn.classList.remove('playing');
        active.fill.style.width = '0%';
        active.bar.setAttribute('aria-valuenow', '0');
        active.timeEl.textContent = '0:00 / ' + formatTime(active.audio.duration);
        active = null;
    }

    function mount(container, tracks, opts) {
        if (typeof container === 'string') {
            container = document.querySelector(container);
        }
        if (!container || !tracks || !tracks.length) return;
        opts = opts || {};

        var wrap = document.createElement('div');
        wrap.className = 'ep';

        tracks.forEach(function (track) {
            var row = document.createElement('div');
            row.className = 'ep-row';

            var btn = document.createElement('button');
            btn.type = 'button';
            btn.className = 'ep-play';
            btn.textContent = '▶';
            btn.setAttribute('aria-label', 'play ' + track.name);

            var name = document.createElement('span');
            name.className = 'ep-name';
            name.textContent = track.name;

            var bar = document.createElement('div');
            bar.className = 'ep-bar';
            bar.setAttribute('role', 'slider');
            bar.setAttribute('tabindex', '0');
            bar.setAttribute('aria-label', track.name + ' scrubber');
            bar.setAttribute('aria-valuemin', '0');
            bar.setAttribute('aria-valuemax', '100');
            bar.setAttribute('aria-valuenow', '0');

            var fill = document.createElement('div');
            fill.className = 'ep-fill';
            bar.appendChild(fill);

            var timeEl = document.createElement('span');
            timeEl.className = 'ep-time';
            timeEl.textContent = '0:00';

            row.appendChild(btn);
            row.appendChild(name);
            row.appendChild(bar);
            row.appendChild(timeEl);

            if (track.download) {
                var dl = document.createElement('a');
                dl.className = 'ep-dl';
                dl.href = track.src;
                dl.setAttribute('download', '');
                dl.textContent = '⤓';
                dl.setAttribute('aria-label', 'download ' + track.name);
                row.appendChild(dl);
            }

            wrap.appendChild(row);

            var audio = null;
            var scrubbing = false;

            function ensureAudio() {
                if (!audio) {
                    audio = new Audio(track.src);
                    audio.preload = 'none';

                    audio.addEventListener('loadedmetadata', function () {
                        timeEl.textContent = formatTime(audio.currentTime) + ' / ' + formatTime(audio.duration);
                    });

                    audio.addEventListener('timeupdate', function () {
                        if (!audio.duration) return;
                        var pct = (audio.currentTime / audio.duration) * 100;
                        fill.style.width = pct + '%';
                        bar.setAttribute('aria-valuenow', String(Math.round(pct)));
                        timeEl.textContent = formatTime(audio.currentTime) + ' / ' + formatTime(audio.duration);
                    });

                    audio.addEventListener('ended', function () {
                        stopActive();
                    });
                }
                return audio;
            }

            function primeMetadata() {
                var a = ensureAudio();
                if (a.preload === 'none') {
                    a.preload = 'metadata';
                }
            }

            btn.addEventListener('mouseenter', primeMetadata);
            btn.addEventListener('focus', primeMetadata);
            bar.addEventListener('mouseenter', primeMetadata);
            bar.addEventListener('focus', primeMetadata);

            btn.addEventListener('click', function () {
                var a = ensureAudio();
                if (active && active.audio === a && !a.paused) {
                    stopActive();
                    return;
                }
                stopActive();
                active = { audio: a, btn: btn, row: row, fill: fill, bar: bar, timeEl: timeEl };
                a.play();
                btn.textContent = '⏹';
                btn.classList.add('playing');
            });

            function seekFromClientX(clientX) {
                var a = ensureAudio();
                var rect = bar.getBoundingClientRect();
                var x = Math.max(0, Math.min(clientX - rect.left, rect.width));
                var pct = rect.width ? (x / rect.width) : 0;
                if (a.duration) {
                    a.currentTime = pct * a.duration;
                    fill.style.width = (pct * 100) + '%';
                    bar.setAttribute('aria-valuenow', String(Math.round(pct * 100)));
                }
            }

            bar.addEventListener('pointerdown', function (e) {
                scrubbing = true;
                try { bar.setPointerCapture(e.pointerId); } catch (err) {}
                seekFromClientX(e.clientX);
            });
            bar.addEventListener('pointermove', function (e) {
                if (scrubbing) seekFromClientX(e.clientX);
            });
            bar.addEventListener('pointerup', function (e) {
                scrubbing = false;
                try { bar.releasePointerCapture(e.pointerId); } catch (err) {}
            });
            bar.addEventListener('pointercancel', function () {
                scrubbing = false;
            });

            bar.addEventListener('keydown', function (e) {
                var a = ensureAudio();
                if (!a.duration) return;
                if (e.key === 'ArrowLeft') {
                    a.currentTime = Math.max(0, a.currentTime - 5);
                    e.preventDefault();
                } else if (e.key === 'ArrowRight') {
                    a.currentTime = Math.min(a.duration, a.currentTime + 5);
                    e.preventDefault();
                }
            });
        });

        container.innerHTML = '';
        container.appendChild(wrap);
    }

    function autoMount() {
        var nodes = document.querySelectorAll('[data-player]');
        nodes.forEach(function (node) {
            var raw = node.getAttribute('data-tracks');
            if (!raw) return;
            var tracks;
            try {
                tracks = JSON.parse(raw);
            } catch (e) {
                return;
            }
            if (!Array.isArray(tracks)) return;
            mount(node, tracks, {});
        });
    }

    window.EmgorPlayer = { mount: mount };

    document.addEventListener('DOMContentLoaded', autoMount);
})();
