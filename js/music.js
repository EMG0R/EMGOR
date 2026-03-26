document.addEventListener('DOMContentLoaded', () => {
    const toggleImage = document.getElementById('toggle-image');
    const trackList = document.getElementById('track-list');
    const rows = trackList.querySelectorAll('.track-row');
    const videoRow = trackList.querySelector('.video-track-row');

    let activeAudio = null;
    let activeRow = null;

    if (toggleImage && trackList) {
        toggleImage.addEventListener('click', () => {
            const isVisible = trackList.style.display !== 'none';
            trackList.style.display = isVisible ? 'none' : 'flex';
            if (!isVisible) {
                setTimeout(() => trackList.scrollIntoView({ behavior: 'smooth' }), 100);
            }
        });
    }

    function formatTime(s) {
        if (!s || !isFinite(s)) return '0:00';
        const m = Math.floor(s / 60);
        const sec = Math.floor(s % 60);
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

    rows.forEach(row => {
        const src = row.dataset.src;
        const btn = row.querySelector('.track-play');
        const scrubber = row.querySelector('.track-scrubber');
        const progress = row.querySelector('.track-progress');
        const timeEl = row.querySelector('.track-time');
        const audio = new Audio(src);

        btn.addEventListener('click', () => {
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

        audio.addEventListener('timeupdate', () => {
            if (!audio.duration) return;
            const pct = (audio.currentTime / audio.duration) * 100;
            progress.style.width = pct + '%';
            timeEl.textContent = formatTime(audio.currentTime);
        });

        audio.addEventListener('ended', () => {
            stopAll();
        });

        function scrub(e) {
            const rect = scrubber.getBoundingClientRect();
            const clientX = e.touches ? e.touches[0].clientX : e.clientX;
            const x = Math.max(0, Math.min(clientX - rect.left, rect.width));
            if (audio.duration) {
                audio.currentTime = (x / rect.width) * audio.duration;
                progress.style.width = ((x / rect.width) * 100) + '%';
            }
        }

        let scrubbing = false;

        scrubber.addEventListener('mousedown', (e) => {
            scrubbing = true;
            scrub(e);
        });
        document.addEventListener('mousemove', (e) => {
            if (scrubbing) scrub(e);
        });
        document.addEventListener('mouseup', () => {
            scrubbing = false;
        });

        scrubber.addEventListener('touchstart', (e) => {
            scrubbing = true;
            scrub(e);
        }, { passive: true });
        document.addEventListener('touchmove', (e) => {
            if (scrubbing) scrub(e);
        }, { passive: true });
        document.addEventListener('touchend', () => {
            scrubbing = false;
        });
    });

    if (videoRow) {
        const videoBtn = videoRow.querySelector('.track-play');
        const videoEl = videoRow.querySelector('video');

        videoBtn.addEventListener('click', () => {
            stopAll();
            if (videoEl.paused) {
                videoEl.style.display = 'block';
                videoEl.play();
                videoBtn.textContent = '\u23F9 spiderverse';
            } else {
                videoEl.pause();
                videoEl.style.display = 'none';
                videoBtn.textContent = '\u25B6 spiderverse';
            }
        });

        videoEl.addEventListener('ended', () => {
            videoEl.style.display = 'none';
            videoBtn.textContent = '\u25B6 spiderverse';
        });
    }
});
