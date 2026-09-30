document.addEventListener('DOMContentLoaded', function() {
    var rows = document.querySelectorAll('.track-row');
    var activeAudio = null;
    var activeRow = null;

    function formatTime(s) {
        if (!s || !isFinite(s)) return '0:00';
        var m = Math.floor(s / 60);
        var sec = Math.floor(s % 60);
        return m + ':' + (sec < 10 ? '0' : '') + sec;
    }

    function stopAll() {
        if (activeAudio) {
            activeAudio.pause();
            activeAudio.currentTime = 0;
        }
        if (activeRow) {
            activeRow.querySelector('.track-play').textContent = '\u25B6';
            activeRow.querySelector('.track-play').classList.remove('playing');
            activeRow.querySelector('.track-progress').style.width = '0%';
            activeRow.querySelector('.track-time').textContent = '0:00';
        }
        activeAudio = null;
        activeRow = null;
    }

    rows.forEach(function(row) {
        var src = row.dataset.src;
        var btn = row.querySelector('.track-play');
        var scrubber = row.querySelector('.track-scrubber');
        var progress = row.querySelector('.track-progress');
        var timeEl = row.querySelector('.track-time');
        var audio = new Audio(src);

        btn.addEventListener('click', function() {
            if (activeRow === row && !audio.paused) {
                stopAll();
                return;
            }
            stopAll();
            activeAudio = audio;
            activeRow = row;
            audio.play();
            btn.textContent = '\u23F9';
            btn.classList.add('playing');
        });

        audio.addEventListener('timeupdate', function() {
            if (!audio.duration) return;
            progress.style.width = ((audio.currentTime / audio.duration) * 100) + '%';
            timeEl.textContent = formatTime(audio.currentTime);
        });

        audio.addEventListener('ended', function() {
            stopAll();
        });

        function scrub(e) {
            var rect = scrubber.getBoundingClientRect();
            var clientX = e.touches ? e.touches[0].clientX : e.clientX;
            var x = Math.max(0, Math.min(clientX - rect.left, rect.width));
            if (audio.duration) {
                audio.currentTime = (x / rect.width) * audio.duration;
                progress.style.width = ((x / rect.width) * 100) + '%';
            }
        }

        var scrubbing = false;

        scrubber.addEventListener('mousedown', function(e) {
            scrubbing = true;
            scrub(e);
        });
        document.addEventListener('mousemove', function(e) {
            if (scrubbing) scrub(e);
        });
        document.addEventListener('mouseup', function() {
            scrubbing = false;
        });

        scrubber.addEventListener('touchstart', function(e) {
            scrubbing = true;
            scrub(e);
        }, { passive: true });
        document.addEventListener('touchmove', function(e) {
            if (scrubbing) scrub(e);
        }, { passive: true });
        document.addEventListener('touchend', function() {
            scrubbing = false;
        });
    });
});
