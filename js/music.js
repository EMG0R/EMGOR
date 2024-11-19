document.addEventListener('DOMContentLoaded', () => {
    // DOM Elements
    const playButton = document.querySelector('.custom-play-button');
    const scrubControl = document.getElementById('scrub-control');
    const audioElement = document.getElementById('audio-player');

    // Ensure AudioContext exists
    const context = window.context || new (window.AudioContext || window.webkitAudioContext)();
    const audioSource = context.createMediaElementSource(audioElement);
    const outputNode = context.destination;

    // Connect audio source to output node
    audioSource.connect(outputNode);

    function togglePlay() {
        if (audioElement.paused) {
            audioElement.play();
            playButton.textContent = '⏸'; // Update button to pause icon
            playButton.style.width = '57.5px'; // Increase width by 15% (50px * 1.15)
            playButton.style.height = '42.5px'; // Decrease height by 15% (50px * 0.85)
            playButton.style.fontSize = '135%'; // Make pause icon 15% larger
            scrubControl.style.display = 'block'; // Show scrub bar
        } else {
            audioElement.pause();
            playButton.textContent = '▶'; // Update button to play icon
            playButton.style.width = '57.5px'; // Increase width by 15% (50px * 1.15)
            playButton.style.height = '42.5px'; // Decrease height by 15% (50px * 0.85)
            playButton.style.fontSize = '100%'; // Make pause icon 15% larger
            scrubControl.style.display = 'none'; // Hide scrub bar
        }
    }

    // Reset play button and hide scrub bar when audio ends
    function handleAudioEnd() {
        audioElement.pause(); // Ensure audio stops
        audioElement.currentTime = 0; // Reset playback position
        playButton.textContent = '▶'; // Reset button to play state
        scrubControl.style.display = 'none'; // Hide scrub bar
    }

    // Update scrub bar indicator as audio plays
    function updateScrubIndicator() {
        if (audioElement.duration) {
            const progress = (audioElement.currentTime / audioElement.duration) * 100;
            scrubControl.style.setProperty('--indicator-position-scrub', `${progress}%`);
        }
    }

    // Set audio position on scrub
    function setAudioPosition(event) {
        const rect = scrubControl.getBoundingClientRect();
        const clientX = event.touches ? event.touches[0].clientX : event.clientX;
        let x = clientX - rect.left;
        x = Math.max(0, Math.min(x, rect.width));

        if (audioElement.duration) {
            const newTime = (x / rect.width) * audioElement.duration;
            audioElement.currentTime = newTime;

            const progress = (x / rect.width) * 100;
            scrubControl.style.setProperty('--indicator-position-scrub', `${progress}%`);
        }
    }

    // Event listeners for scrub control
    scrubControl.addEventListener('mousedown', (event) => {
        setAudioPosition(event); // Update position on initial click
        document.addEventListener('mousemove', setAudioPosition);
    });

    document.addEventListener('mouseup', () => {
        document.removeEventListener('mousemove', setAudioPosition);
    });

    scrubControl.addEventListener('touchstart', (event) => {
        setAudioPosition(event); // Update position on initial touch
        document.addEventListener('touchmove', setAudioPosition);
    });

    document.addEventListener('touchend', () => {
        document.removeEventListener('touchmove', setAudioPosition);
    });

    // Sync scrub bar with playback
    audioElement.addEventListener('timeupdate', updateScrubIndicator);

    // Reset play button and scrub bar when audio ends
    audioElement.addEventListener('ended', handleAudioEnd);

    // Play button functionality
    playButton.addEventListener('click', togglePlay);
});