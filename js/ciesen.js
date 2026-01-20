// ciesen.js

async function setup() {
    // Keep screen awake
    async function keepAwake() {
        try {
            if (document.visibilityState === 'visible') {
                const wakeLock = await navigator.wakeLock.request('screen');
                console.log('Wake Lock is active');
            }
        } catch (err) {
            console.error(`${err.name}, ${err.message}`);
        }
    }
    
    document.addEventListener("visibilitychange", keepAwake);
    keepAwake();

    // Create AudioContext
    const WAContext = window.AudioContext || window.webkitAudioContext;
    const context = new WAContext();

    // Resume AudioContext on visibility change
    document.addEventListener("visibilitychange", function() {
        if (document.visibilityState === 'visible') {
            if (context.state === 'suspended') {
                context.resume().then(() => {
                    console.log('AudioContext resumed');
                });
            }
        }
    });

    // Load the ciesen patch
    const patchExportURL = "export/ciesen.patch.export.json";
    const outputNode = context.createGain();
    outputNode.connect(context.destination);

    let response, patcher;
    try {
        response = await fetch(patchExportURL);
        patcher = await response.json();
        if (!window.RNBO) {
            await loadRNBOScript(patcher.desc.meta.rnboversion);
        }
    } catch (err) {
        console.error("Error loading patch:", err);
        return;
    }

    // Load dependencies if any
    let dependencies = [];
    try {
        const dependenciesResponse = await fetch("export/ciesen.dependencies.json");
        dependencies = await dependenciesResponse.json();
        dependencies = dependencies.map(d => d.file ? { ...d, file: "export/" + d.file } : d);
    } catch (e) { 
        // No dependencies file, that's fine
    }

    // Create RNBO device
    let device;
    try {
        device = await RNBO.createDevice({ context, patcher });
    } catch (err) {
        console.error("Error creating RNBO device:", err);
        return;
    }

    if (dependencies.length) await device.loadDataBufferDependencies(dependencies);
    device.node.connect(outputNode);

    // Setup both XY controls
    setupCircleControl(device);  // Blue circular pad
    setupGridControl(device);    // Purple square pad

    // Auto-start: resume audio context on first user interaction anywhere
    const startAudio = async () => {
        if (context.state === 'suspended') {
            await context.resume();
            console.log('AudioContext started');
        }
        // Remove listeners after first interaction
        document.removeEventListener('click', startAudio);
        document.removeEventListener('touchstart', startAudio);
    };
    
    document.addEventListener('click', startAudio);
    document.addEventListener('touchstart', startAudio);
    
    // Also try to start immediately (will work if page already had interaction)
    context.resume();
}

// Load RNBO script dynamically
function loadRNBOScript(version) {
    return new Promise((resolve, reject) => {
        if (/^\d+\.\d+\.\d+-dev$/.test(version)) {
            throw new Error("Patcher exported with a Debug Version! Specify correct RNBO version.");
        }
        const el = document.createElement("script");
        el.src = `https://c74-public.nyc3.digitaloceanspaces.com/rnbo/${encodeURIComponent(version)}/rnbo.min.js`;
        el.onload = resolve;
        el.onerror = () => reject(new Error(`Failed to load rnbo.js v${version}`));
        document.body.append(el);
    });
}

// Circular Blue XY Pad - constrained to circle bounds
// Maps to circleX and circleY parameters (0-1 range)
function setupCircleControl(device) {
    const circleContainer = document.getElementById('circle-container');
    let isDragging = false;

    function calculateCircleXY(clientX, clientY) {
        const rect = circleContainer.getBoundingClientRect();
        const centerX = rect.left + rect.width / 2;
        const centerY = rect.top + rect.height / 2;
        const radius = rect.width / 2;

        // Get position relative to center
        let dx = clientX - centerX;
        let dy = clientY - centerY;
        
        // Calculate distance from center
        const distance = Math.sqrt(dx * dx + dy * dy);
        
        // If outside circle, clamp to edge
        if (distance > radius) {
            const scale = radius / distance;
            dx *= scale;
            dy *= scale;
        }
        
        // Convert to 0-1 range (center is 0.5, 0.5)
        const x = (dx / radius + 1) / 2;
        const y = (dy / radius + 1) / 2;
        
        return { x, y };
    }

    function updateRNBOCircleValues(x, y) {
        // Change these parameter names to match your RNBO patch
        const paramX = device.parameters.find(param => param.name === "circleX");
        const paramY = device.parameters.find(param => param.name === "circleY");
        if (paramX) paramX.value = x;
        if (paramY) paramY.value = y;
    }

    function createSpark(pageX, pageY, isBlue = true) {
        const spark = document.createElement("div");
        spark.classList.add("spark");
        spark.classList.add(isBlue ? "blue" : "purple");
        
        const size = Math.random() * 8 + 3;
        spark.style.width = `${size}px`;
        spark.style.height = `${size}px`;

        spark.style.left = `${pageX}px`;
        spark.style.top = `${pageY}px`;

        const angle = Math.random() * 360;
        const speed = Math.random() * 30 + 10;
        spark.style.transform = `translate(${Math.cos(angle) * speed}px, ${Math.sin(angle) * speed}px)`;

        document.body.appendChild(spark);

        setTimeout(() => {
            spark.remove();
        }, 1000);
    }

    function updateCoordinates(event) {
        if (!isDragging) return;
        event.preventDefault();
        
        let clientX, clientY, pageX, pageY;

        if (event.touches && event.touches.length > 0) {
            clientX = event.touches[0].clientX;
            clientY = event.touches[0].clientY;
            pageX = event.touches[0].pageX;
            pageY = event.touches[0].pageY;
        } else {
            clientX = event.clientX;
            clientY = event.clientY;
            pageX = event.pageX;
            pageY = event.pageY;
        }

        const { x, y } = calculateCircleXY(clientX, clientY);
        updateRNBOCircleValues(x, y);
        createSpark(pageX, pageY, true); // Blue sparks
    }

    // Mouse events
    circleContainer.addEventListener('mousedown', (event) => {
        isDragging = true;
        updateCoordinates(event);
    });

    circleContainer.addEventListener('mousemove', (event) => {
        if (isDragging) {
            updateCoordinates(event);
        }
    });

    document.addEventListener('mouseup', () => {
        isDragging = false;
    });

    // Touch events
    circleContainer.addEventListener('touchstart', (event) => {
        isDragging = true;
        updateCoordinates(event);
    });

    circleContainer.addEventListener('touchmove', (event) => {
        if (isDragging) {
            updateCoordinates(event);
        }
    });

    circleContainer.addEventListener('touchend', () => {
        isDragging = false;
    });
}

// Square Purple XY Pad - maps to wavesX and wavesY (0-1 range)
function setupGridControl(device) {
    const gridContainer = document.getElementById('grid-container');
    let isDragging = false;

    function calculateXY(clientX, clientY) {
        const rect = gridContainer.getBoundingClientRect();
        // Map to 0-1 range for wavesX and wavesY
        const x = Math.min(1, Math.max(0, (clientX - rect.left) / rect.width));
        const y = Math.min(1, Math.max(0, (clientY - rect.top) / rect.height));
        return { x, y };
    }

    function updateRNBOValues(x, y) {
        const paramX = device.parameters.find(param => param.name === "wavesX");
        const paramY = device.parameters.find(param => param.name === "wavesY");
        if (paramX) paramX.value = x;
        if (paramY) paramY.value = y;
    }

    function createSpark(pageX, pageY) {
        const spark = document.createElement("div");
        spark.classList.add("spark");
        spark.classList.add("purple");
        
        const size = Math.random() * 8 + 3;
        spark.style.width = `${size}px`;
        spark.style.height = `${size}px`;

        spark.style.left = `${pageX}px`;
        spark.style.top = `${pageY}px`;

        const angle = Math.random() * 360;
        const speed = Math.random() * 30 + 10;
        spark.style.transform = `translate(${Math.cos(angle) * speed}px, ${Math.sin(angle) * speed}px)`;

        document.body.appendChild(spark);

        setTimeout(() => {
            spark.remove();
        }, 1000);
    }

    function updateCoordinates(event) {
        if (!isDragging) return;
        event.preventDefault();
        
        let clientX, clientY, pageX, pageY;

        if (event.touches && event.touches.length > 0) {
            clientX = event.touches[0].clientX;
            clientY = event.touches[0].clientY;
            pageX = event.touches[0].pageX;
            pageY = event.touches[0].pageY;
        } else {
            clientX = event.clientX;
            clientY = event.clientY;
            pageX = event.pageX;
            pageY = event.pageY;
        }

        const { x, y } = calculateXY(clientX, clientY);
        updateRNBOValues(x, y);
        createSpark(pageX, pageY); // Purple sparks
    }

    // Mouse events
    gridContainer.addEventListener('mousedown', (event) => {
        isDragging = true;
        updateCoordinates(event);
    });

    gridContainer.addEventListener('mousemove', (event) => {
        if (isDragging) {
            updateCoordinates(event);
        }
    });

    document.addEventListener('mouseup', () => {
        isDragging = false;
    });

    // Touch events
    gridContainer.addEventListener('touchstart', (event) => {
        isDragging = true;
        updateCoordinates(event);
    });

    gridContainer.addEventListener('touchmove', (event) => {
        if (isDragging) {
            updateCoordinates(event);
        }
    });

    gridContainer.addEventListener('touchend', () => {
        isDragging = false;
    });
}

// Initialize on DOM ready
document.addEventListener("DOMContentLoaded", setup);
