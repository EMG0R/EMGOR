document.addEventListener('DOMContentLoaded', () => {
    const audioSystems = [];

    const toggleImage = document.getElementById('toggle-image');
    const playButtonContainer = document.getElementById('play-button-container');

    if (toggleImage && playButtonContainer) {
        toggleImage.addEventListener('click', () => {
            const isVisible = playButtonContainer.style.display === 'block';
            playButtonContainer.style.display = isVisible ? 'none' : 'block';
            playButtonContainer.style.justifyContent = 'flex-start';
            playButtonContainer.style.alignItems = 'flex-start';

            if (!isVisible) {
                setTimeout(() => {
                    playButtonContainer.scrollIntoView({ behavior: 'smooth' });
                }, 100);
            }
        });
    }

    function initializeAudioSystem(fileName, buttonId, scrubberId, color = 'rgba(75, 0, 130, 0.85)') {
        const playButton = document.getElementById(buttonId);
        const scrubControl = document.getElementById(scrubberId);
        const audioElement = new Audio(fileName);

        if (!playButton || !scrubControl) {
            console.error(`Missing elements for button: ${buttonId}, scrubber: ${scrubberId}`);
            return;
        }

        // Set default or custom colors
        playButton.style.backgroundColor = color;
        scrubControl.style.backgroundColor = color;

        function stopOthers() {
            audioSystems.forEach(({ audio, button, scrubber }) => {
                if (audio !== audioElement) {
                    audio.pause();
                    audio.currentTime = 0;
                    button.textContent = '▶';
                    scrubber.style.display = 'none';
                }
            });
        }

        function togglePlay() {
            stopOthers();
            if (audioElement.paused) {
                audioElement.play();
                playButton.textContent = '⏹';
                scrubControl.style.display = 'block';
            } else {
                audioElement.pause();
                playButton.textContent = '▶';
                scrubControl.style.display = 'none';
            }
        }

        function handleAudioEnd() {
            audioElement.pause();
            audioElement.currentTime = 0;
            playButton.textContent = '▶';
            scrubControl.style.display = 'none';
        }

        function updateScrubIndicator() {
            if (audioElement.duration) {
                const progress = (audioElement.currentTime / audioElement.duration) * 100;
                scrubControl.style.setProperty('--indicator-position-scrub', `${progress}%`);
            }
        }

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

        playButton.addEventListener('click', togglePlay);
        audioElement.addEventListener('ended', handleAudioEnd);
        audioElement.addEventListener('timeupdate', updateScrubIndicator);

        scrubControl.addEventListener('mousedown', (event) => {
            setAudioPosition(event);
            document.addEventListener('mousemove', setAudioPosition);
        });
        document.addEventListener('mouseup', () => {
            document.removeEventListener('mousemove', setAudioPosition);
        });
        scrubControl.addEventListener('touchstart', (event) => {
            setAudioPosition(event);
            document.addEventListener('touchmove', setAudioPosition);
        });
        document.addEventListener('touchend', () => {
            document.removeEventListener('touchmove', setAudioPosition);
        });

        audioSystems.push({ audio: audioElement, button: playButton, scrubber: scrubControl });
    
    }

    // Initialize audio systems
    initializeAudioSystem('../resources/anxiety.wav', 'play-button-anxiety', 'scrub-control-anxiety');
    initializeAudioSystem('../resources/fitmk.wav', 'play-button-fitmk', 'scrub-control-fitmk');
    initializeAudioSystem('../resources/mashup.wav', 'play-button-mashup', 'scrub-control-mashup');
    initializeAudioSystem('../resources/matthew.wav', 'play-button-matthew', 'scrub-control-matthew');
    initializeAudioSystem('../resources/okay.wav', 'play-button-okay', 'scrub-control-okay');
    initializeAudioSystem('../resources/sad.wav', 'play-button-sad', 'scrub-control-sad');





    function initializeVideoSystem(buttonId, videoId) {
        const playButton = document.getElementById(buttonId);
        const videoElement = document.getElementById(videoId);
    
        if (!playButton || !videoElement) {
            console.error(`Missing elements for button: ${buttonId}, video: ${videoId}`);
            return;
        }
    
        // Function to toggle video play/pause and visibility
        function toggleVideoPlay() {
            if (videoElement.paused) {
                videoElement.play();
                playButton.textContent = '⏹'; // Use || for pause
                videoElement.style.display = 'block';
            } else {
                videoElement.pause();
                videoElement.style.display = 'none';
                playButton.textContent = '▶'; // Reset to play symbol
            }
        }
    
        // Reset video and button state when video ends
        videoElement.addEventListener('ended', () => {
            videoElement.style.display = 'none';
            playButton.textContent = '▶';
        });
    
        // Add click event listener to the button
        playButton.addEventListener('click', toggleVideoPlay);
    }
    
    // Initialize the video system
    initializeVideoSystem('play-button-spiderverse', 'spiderverse-video');


});