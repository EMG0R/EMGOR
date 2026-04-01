(function () {
    'use strict';

    var WORDS = ['sound', 'signal', 'rhythm', 'noise', 'pulse', 'wave', 'flux', 'drift', 'echo', 'void'];
    var currentView = 'home';
    var isOrbitPage = false;
    var transitionOverlay = null;
    var BG = '#0D0221';

    // Three.js globals
    var scene, camera, renderer, clock;
    var planetMeshes = [];      // { mesh, label, orbitRadius, orbitSpeed, angle, orbitTilt, view, entering, leaving, flyDir, flyProgress }
    var starField;
    var orbitLines = [];
    var cameraAngle = 0;
    var cameraTilt = 0.3;
    var cameraRadius = 18;
    var targetCameraAngle = 0;
    var targetCameraTilt = 0.3;

    // Planet color palette — diverse, not just purple
    var PLANET_COLORS = [
        { base: 0x00CED1, emissive: 0x006666 },  // deep teal
        { base: 0xE8A87C, emissive: 0x8B5A3C },  // rose gold
        { base: 0x4169E1, emissive: 0x1a2d6b },  // electric blue
        { base: 0xFFB347, emissive: 0x995500 },  // amber
        { base: 0xFF6F61, emissive: 0x8B2500 },  // coral
        { base: 0x88D8B0, emissive: 0x2d6b4a },  // mint
        { base: 0xB388FF, emissive: 0x5C2D91 },  // violet
        { base: 0xFF69B4, emissive: 0x8B1A5C },  // hot pink
        { base: 0x7FFFD4, emissive: 0x2E8B57 },  // aquamarine
        { base: 0xDDA0DD, emissive: 0x6B3A6B },  // plum
        { base: 0x20B2AA, emissive: 0x0E5E5A },  // light sea green
        { base: 0xF0E68C, emissive: 0x8B8000 },  // khaki gold
    ];

    // ─── THREE.JS SETUP ────────────────────────────────────────
    function initThree() {
        var canvas = document.getElementById('spaceCanvas');
        scene = new THREE.Scene();
        clock = new THREE.Clock();

        camera = new THREE.PerspectiveCamera(60, window.innerWidth / window.innerHeight, 0.1, 1000);
        camera.position.set(0, 5, cameraRadius);
        camera.lookAt(0, 0, 0);

        renderer = new THREE.WebGLRenderer({ canvas: canvas, alpha: true, antialias: true });
        renderer.setSize(window.innerWidth, window.innerHeight);
        renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
        renderer.setClearColor(0x000000, 0);

        // Point light at center (the black hole emits light from accretion disk)
        var centerLight = new THREE.PointLight(0xB388FF, 2.5, 100);
        centerLight.position.set(0, 0, 0);
        scene.add(centerLight);

        // Secondary warm light for depth
        var warmLight = new THREE.PointLight(0xFF6633, 0.8, 80);
        warmLight.position.set(0, 3, 0);
        scene.add(warmLight);

        // Subtle ambient so dark sides aren't pure black
        var ambient = new THREE.AmbientLight(0x1a0a2e, 0.4);
        scene.add(ambient);

        // Rim light from behind camera for that cinematic feel
        var rimLight = new THREE.DirectionalLight(0x4400AA, 0.3);
        rimLight.position.set(0, 10, 20);
        scene.add(rimLight);

        createStarField();
        createNebulaParticles();
    }

    // ─── STARS ─────────────────────────────────────────────────
    function createStarField() {
        var starCount = 2500;
        var geometry = new THREE.BufferGeometry();
        var positions = new Float32Array(starCount * 3);
        var colors = new Float32Array(starCount * 3);
        var sizes = new Float32Array(starCount);

        for (var i = 0; i < starCount; i++) {
            // Distribute stars on a large sphere
            var theta = Math.random() * Math.PI * 2;
            var phi = Math.acos(2 * Math.random() - 1);
            var r = 80 + Math.random() * 120;

            positions[i * 3] = r * Math.sin(phi) * Math.cos(theta);
            positions[i * 3 + 1] = r * Math.sin(phi) * Math.sin(theta);
            positions[i * 3 + 2] = r * Math.cos(phi);

            // Color variety: purple, cyan, white
            var roll = Math.random();
            if (roll < 0.12) {
                // Cyan
                colors[i * 3] = 0.0; colors[i * 3 + 1] = 0.9; colors[i * 3 + 2] = 1.0;
            } else if (roll < 0.4) {
                // Purple tint
                colors[i * 3] = 0.7; colors[i * 3 + 1] = 0.5; colors[i * 3 + 2] = 1.0;
            } else {
                // Warm white
                colors[i * 3] = 1.0; colors[i * 3 + 1] = 0.95; colors[i * 3 + 2] = 0.9;
            }

            sizes[i] = 0.3 + Math.random() * 1.8;
        }

        geometry.setAttribute('position', new THREE.BufferAttribute(positions, 3));
        geometry.setAttribute('color', new THREE.BufferAttribute(colors, 3));
        geometry.setAttribute('size', new THREE.BufferAttribute(sizes, 1));

        var starMaterial = new THREE.PointsMaterial({
            size: 0.8,
            vertexColors: true,
            transparent: true,
            opacity: 0.85,
            sizeAttenuation: true,
            blending: THREE.AdditiveBlending,
            depthWrite: false
        });

        starField = new THREE.Points(geometry, starMaterial);
        scene.add(starField);
    }

    // ─── NEBULA DUST ───────────────────────────────────────────
    function createNebulaParticles() {
        var count = 600;
        var geometry = new THREE.BufferGeometry();
        var positions = new Float32Array(count * 3);
        var colors = new Float32Array(count * 3);

        for (var i = 0; i < count; i++) {
            positions[i * 3] = (Math.random() - 0.5) * 100;
            positions[i * 3 + 1] = (Math.random() - 0.5) * 60;
            positions[i * 3 + 2] = (Math.random() - 0.5) * 100;

            var roll = Math.random();
            if (roll < 0.3) {
                colors[i * 3] = 0.3; colors[i * 3 + 1] = 0.1; colors[i * 3 + 2] = 0.5;
            } else if (roll < 0.6) {
                colors[i * 3] = 0.1; colors[i * 3 + 1] = 0.05; colors[i * 3 + 2] = 0.3;
            } else {
                colors[i * 3] = 0.0; colors[i * 3 + 1] = 0.15; colors[i * 3 + 2] = 0.25;
            }
        }

        geometry.setAttribute('position', new THREE.BufferAttribute(positions, 3));
        geometry.setAttribute('color', new THREE.BufferAttribute(colors, 3));

        var material = new THREE.PointsMaterial({
            size: 2.5,
            vertexColors: true,
            transparent: true,
            opacity: 0.12,
            sizeAttenuation: true,
            blending: THREE.AdditiveBlending,
            depthWrite: false
        });

        var nebula = new THREE.Points(geometry, material);
        scene.add(nebula);
    }

    // ─── ORBIT RINGS (3D) ──────────────────────────────────────
    function createOrbitRing(radius, tilt) {
        var segments = 128;
        var geometry = new THREE.BufferGeometry();
        var positions = new Float32Array(segments * 3);

        for (var i = 0; i < segments; i++) {
            var angle = (i / segments) * Math.PI * 2;
            positions[i * 3] = Math.cos(angle) * radius;
            positions[i * 3 + 1] = 0;
            positions[i * 3 + 2] = Math.sin(angle) * radius;
        }

        geometry.setAttribute('position', new THREE.BufferAttribute(positions, 3));

        var material = new THREE.LineBasicMaterial({
            color: 0xA855F7,
            transparent: true,
            opacity: 0.08,
            blending: THREE.AdditiveBlending
        });

        var line = new THREE.LineLoop(geometry, material);
        line.rotation.x = tilt;
        return line;
    }

    function rebuildOrbitRings() {
        orbitLines.forEach(function (line) { scene.remove(line); });
        orbitLines = [];

        planetMeshes.forEach(function (p) {
            if (p.leaving) return;
            var ring = createOrbitRing(p.orbitRadius, p.orbitTilt);
            scene.add(ring);
            orbitLines.push(ring);
        });
    }

    // ─── PLANET CREATION ───────────────────────────────────────
    function createPlanetMesh(colorIndex, size) {
        var col = PLANET_COLORS[colorIndex % PLANET_COLORS.length];

        var geometry = new THREE.SphereGeometry(size, 32, 32);
        var material = new THREE.MeshPhongMaterial({
            color: col.base,
            emissive: col.emissive,
            emissiveIntensity: 0.3,
            shininess: 80,
            specular: 0x444444
        });

        var mesh = new THREE.Mesh(geometry, material);

        // Add glow sprite
        var spriteMaterial = new THREE.SpriteMaterial({
            color: col.base,
            transparent: true,
            opacity: 0.15,
            blending: THREE.AdditiveBlending
        });
        var glow = new THREE.Sprite(spriteMaterial);
        glow.scale.set(size * 4, size * 4, 1);
        mesh.add(glow);

        return mesh;
    }

    function getOrbitRadius(index) {
        return 3.5 + index * 2.2;
    }

    function getOrbitSpeed(index) {
        return 0.15 / (1 + index * 0.25);
    }

    function getOrbitTilt(index) {
        // Slight unique tilt per orbit for 3D feel
        var tilts = [0.1, -0.15, 0.08, -0.2, 0.12, -0.05, 0.18, -0.1, 0.06, -0.14, 0.09, -0.07];
        return tilts[index % tilts.length];
    }

    function getPlanetSize(el) {
        if (el.classList.contains('icon-planet')) return 0.4;
        return 0.7 + Math.random() * 0.3;
    }

    // ─── BUILD ORBIT SYSTEM ────────────────────────────────────
    function buildOrbit(fromCenter) {
        // Remove existing planet meshes
        planetMeshes.forEach(function (p) {
            scene.remove(p.mesh);
        });
        planetMeshes = [];

        var visible = getVisiblePlanets();

        visible.forEach(function (el, i) {
            el.style.position = 'fixed';
            el.style.display = 'flex';

            var radius = getOrbitRadius(i);
            var speed = getOrbitSpeed(i);
            var angle = (i / visible.length) * Math.PI * 2 + i * 0.7;
            var tilt = getOrbitTilt(i);
            var size = getPlanetSize(el);

            var mesh = createPlanetMesh(i, size);
            scene.add(mesh);

            var p = {
                mesh: mesh,
                label: el,
                orbitRadius: radius,
                orbitSpeed: speed,
                angle: angle,
                orbitTilt: tilt,
                view: el.getAttribute('data-view'),
                entering: false,
                leaving: false,
                flyDir: new THREE.Vector3(),
                flyProgress: 0,
                flyStartPos: new THREE.Vector3(),
                spinSpeed: 0.5 + Math.random() * 1.5
            };

            if (fromCenter) {
                p.entering = true;
                p.flyProgress = 0;
                p.flyStartPos.set(0, 0, 0);
                mesh.position.set(0, 0, 0);
                mesh.scale.set(0.01, 0.01, 0.01);
            }

            planetMeshes.push(p);
        });

        rebuildOrbitRings();
    }

    function getVisiblePlanets() {
        return Array.from(document.querySelectorAll('[data-orbit="planet"]')).filter(function (el) {
            var view = el.getAttribute('data-view');
            return view === currentView || view === 'always';
        });
    }

    // ─── PROJECT 3D → SCREEN ───────────────────────────────────
    function projectToScreen(position) {
        var vector = position.clone();
        vector.project(camera);
        return {
            x: (vector.x * 0.5 + 0.5) * window.innerWidth,
            y: (-vector.y * 0.5 + 0.5) * window.innerHeight
        };
    }

    // ─── ANIMATION LOOP ────────────────────────────────────────
    function animate() {
        requestAnimationFrame(animate);
        var dt = clock.getDelta();
        var elapsed = clock.getElapsedTime();

        // Slow camera drift
        targetCameraAngle += dt * 0.03;
        targetCameraTilt = 0.3 + Math.sin(elapsed * 0.08) * 0.15;
        cameraAngle += (targetCameraAngle - cameraAngle) * 0.02;
        cameraTilt += (targetCameraTilt - cameraTilt) * 0.02;

        camera.position.x = Math.sin(cameraAngle) * cameraRadius;
        camera.position.z = Math.cos(cameraAngle) * cameraRadius;
        camera.position.y = Math.sin(cameraTilt) * 6;
        camera.lookAt(0, 0, 0);

        // Rotate star field very slowly with camera for parallax
        if (starField) {
            starField.rotation.y = elapsed * 0.005;
            starField.rotation.x = Math.sin(elapsed * 0.003) * 0.02;
        }

        // Animate planets
        for (var i = planetMeshes.length - 1; i >= 0; i--) {
            var p = planetMeshes[i];

            if (p.leaving) {
                p.flyProgress += dt * 1.8;
                var accel = 1 + p.flyProgress * 3;
                p.mesh.position.add(p.flyDir.clone().multiplyScalar(dt * 15 * accel));
                // Spin dramatically while flying away
                p.mesh.rotation.x += dt * 8;
                p.mesh.rotation.z += dt * 5;

                var screenPos = projectToScreen(p.mesh.position);
                var ew = p.label.offsetWidth;
                var eh = p.label.offsetHeight;
                p.label.style.left = (screenPos.x - ew / 2) + 'px';
                p.label.style.top = (screenPos.y - eh / 2) + 'px';
                p.label.style.opacity = Math.max(0, 1 - p.flyProgress * 1.5);

                if (p.flyProgress > 1.5) {
                    p.label.style.display = 'none';
                    scene.remove(p.mesh);
                    planetMeshes.splice(i, 1);
                }
                continue;
            }

            if (p.entering) {
                p.flyProgress = Math.min(1, p.flyProgress + dt * 0.8);
                // Elastic ease out for bouncy arrival
                var t = p.flyProgress;
                var ease = 1 - Math.pow(1 - t, 3);
                // Add a bounce at the end
                if (t > 0.7) {
                    var bounceT = (t - 0.7) / 0.3;
                    ease += Math.sin(bounceT * Math.PI * 2) * 0.05 * (1 - bounceT);
                }

                var targetX = Math.cos(p.angle) * p.orbitRadius;
                var targetZ = Math.sin(p.angle) * p.orbitRadius;
                var targetY = Math.sin(p.orbitTilt) * Math.sin(p.angle) * p.orbitRadius * 0.3;

                p.mesh.position.x = p.flyStartPos.x + (targetX - p.flyStartPos.x) * ease;
                p.mesh.position.y = p.flyStartPos.y + (targetY - p.flyStartPos.y) * ease;
                p.mesh.position.z = p.flyStartPos.z + (targetZ - p.flyStartPos.z) * ease;
                p.mesh.scale.setScalar(ease);

                // Spin while entering
                p.mesh.rotation.y += dt * 3;

                if (p.flyProgress >= 1) {
                    p.entering = false;
                    p.mesh.scale.set(1, 1, 1);
                }
            } else {
                // Normal orbit
                p.angle += p.orbitSpeed * dt;
                p.mesh.position.x = Math.cos(p.angle) * p.orbitRadius;
                p.mesh.position.z = Math.sin(p.angle) * p.orbitRadius;
                p.mesh.position.y = Math.sin(p.orbitTilt) * Math.sin(p.angle) * p.orbitRadius * 0.3;

                // Gentle self-rotation
                p.mesh.rotation.y += dt * p.spinSpeed;
            }

            // Project 3D position to screen for HTML label
            var screenPos = projectToScreen(p.mesh.position);
            var ew = p.label.offsetWidth;
            var eh = p.label.offsetHeight;
            p.label.style.left = (screenPos.x - ew / 2) + 'px';
            p.label.style.top = (screenPos.y - eh / 2) + 'px';
            p.label.style.opacity = '1';
            p.label.style.zIndex = '15';

            // Scale label based on distance to camera for depth
            var dist = p.mesh.position.distanceTo(camera.position);
            var labelScale = Math.max(0.5, Math.min(1.3, 12 / dist));
            p.label.style.transform = 'scale(' + labelScale.toFixed(3) + ')';
        }

        renderer.render(scene, camera);
    }

    // ─── VIEW SWITCHING (hip fly-off/fly-on) ───────────────────
    function switchView(newView) {
        if (newView === currentView) return;

        // Eject current non-always planets with funny trajectories
        planetMeshes.forEach(function (p) {
            if (p.view !== 'always' && !p.leaving) {
                p.leaving = true;
                p.flyProgress = 0;

                // Random dramatic ejection direction
                var ejections = [
                    new THREE.Vector3(1, 2, 0.5),    // up-right
                    new THREE.Vector3(-1.5, -0.5, 1), // down-left-forward
                    new THREE.Vector3(0, -2, -1),     // down-back
                    new THREE.Vector3(1, 0.3, -1.5),  // right-back
                    new THREE.Vector3(-0.8, 1.5, 0.8), // up-left-forward
                ];
                var dir = ejections[Math.floor(Math.random() * ejections.length)].clone();
                dir.normalize();
                p.flyDir = dir;
            }
        });

        currentView = newView;
        if (newView !== 'home') {
            history.replaceState(null, '', location.pathname + '#' + newView);
        } else {
            history.replaceState(null, '', location.pathname);
        }

        // Bring in new planets from random 3D edges after a beat
        setTimeout(function () {
            var newEls = Array.from(document.querySelectorAll('[data-orbit="planet"]')).filter(function (el) {
                return el.getAttribute('data-view') === currentView;
            });

            var alwaysCount = planetMeshes.filter(function (p) {
                return p.view === 'always' && !p.leaving;
            }).length;

            newEls.forEach(function (el, i) {
                el.style.display = 'flex';
                el.style.position = 'fixed';

                var idx = alwaysCount + i;
                var radius = getOrbitRadius(idx);
                var speed = getOrbitSpeed(idx);
                var angle = (idx / (alwaysCount + newEls.length)) * Math.PI * 2 + idx * 0.7;
                var tilt = getOrbitTilt(idx);
                var size = getPlanetSize(el);

                var mesh = createPlanetMesh(i + alwaysCount, size);

                // Start from random 3D edge — dramatic entrances
                var entrances = [
                    new THREE.Vector3(30, 15, 0),
                    new THREE.Vector3(-25, -10, 20),
                    new THREE.Vector3(0, 25, -15),
                    new THREE.Vector3(-30, 5, -10),
                    new THREE.Vector3(15, -20, 25),
                ];
                var startPos = entrances[Math.floor(Math.random() * entrances.length)].clone();
                mesh.position.copy(startPos);
                scene.add(mesh);

                planetMeshes.push({
                    mesh: mesh,
                    label: el,
                    orbitRadius: radius,
                    orbitSpeed: speed,
                    angle: angle,
                    orbitTilt: tilt,
                    view: el.getAttribute('data-view'),
                    entering: true,
                    leaving: false,
                    flyDir: new THREE.Vector3(),
                    flyProgress: 0,
                    flyStartPos: startPos.clone(),
                    spinSpeed: 0.5 + Math.random() * 1.5
                });
            });

            rebuildOrbitRings();
        }, 500);
    }

    // ─── ASTEROIDS (3D) ────────────────────────────────────────
    function spawnAsteroids3D() {
        var asteroids = [];

        function createAsteroid() {
            var size = 0.05 + Math.random() * 0.15;
            var geometry = new THREE.DodecahedronGeometry(size, 0);
            var isCyan = Math.random() < 0.15;
            var material = new THREE.MeshPhongMaterial({
                color: isCyan ? 0x00C8DC : 0x8C64C8,
                emissive: isCyan ? 0x004455 : 0x2a1a44,
                emissiveIntensity: 0.4,
                transparent: true,
                opacity: 0.4 + Math.random() * 0.3
            });

            var mesh = new THREE.Mesh(geometry, material);

            // Start from random edge of the 3D space
            var angle = Math.random() * Math.PI * 2;
            var r = 30 + Math.random() * 20;
            mesh.position.set(
                Math.cos(angle) * r,
                (Math.random() - 0.5) * 15,
                Math.sin(angle) * r
            );

            var speed = 0.5 + Math.random() * 1.5;
            var dir = new THREE.Vector3(-mesh.position.x, (Math.random() - 0.5) * 5, -mesh.position.z).normalize();

            scene.add(mesh);
            asteroids.push({
                mesh: mesh,
                dir: dir,
                speed: speed,
                rotSpeed: new THREE.Vector3(
                    (Math.random() - 0.5) * 3,
                    (Math.random() - 0.5) * 3,
                    (Math.random() - 0.5) * 3
                )
            });
        }

        function scheduleSpawn() {
            setTimeout(function () {
                if (asteroids.length < 15) createAsteroid();
                scheduleSpawn();
            }, 2000 + Math.random() * 4000);
        }
        scheduleSpawn();
        for (var k = 0; k < 5; k++) createAsteroid();

        function animateAsteroids() {
            for (var j = asteroids.length - 1; j >= 0; j--) {
                var a = asteroids[j];
                a.mesh.position.add(a.dir.clone().multiplyScalar(a.speed * 0.016));
                a.mesh.rotation.x += a.rotSpeed.x * 0.016;
                a.mesh.rotation.y += a.rotSpeed.y * 0.016;
                a.mesh.rotation.z += a.rotSpeed.z * 0.016;

                if (a.mesh.position.length() > 60) {
                    scene.remove(a.mesh);
                    asteroids.splice(j, 1);
                }
            }
            requestAnimationFrame(animateAsteroids);
        }
        animateAsteroids();
    }

    // ─── FLOATING WORDS ────────────────────────────────────────
    function spawnFloatingWords() {
        var wordEls = [];
        WORDS.forEach(function (word) {
            var el = document.createElement('span');
            el.className = 'floating-word';
            el.textContent = word;
            document.body.appendChild(el);
            wordEls.push({
                el: el,
                x: Math.random() * window.innerWidth,
                y: Math.random() * window.innerHeight,
                vx: (Math.random() - 0.5) * 0.25,
                vy: (Math.random() - 0.5) * 0.25,
                baseOpacity: 0.08 + Math.random() * 0.12
            });
        });

        function animateWords() {
            var W = window.innerWidth;
            var H = window.innerHeight;
            wordEls.forEach(function (w) {
                w.x += w.vx; w.y += w.vy;
                if (w.x < -80) w.x = W + 20;
                if (w.x > W + 80) w.x = -20;
                if (w.y < -20) w.y = H + 20;
                if (w.y > H + 20) w.y = -20;
                w.el.style.left = w.x + 'px';
                w.el.style.top = w.y + 'px';
                w.el.style.opacity = w.baseOpacity;
            });
            requestAnimationFrame(animateWords);
        }
        animateWords();
    }

    // ─── BIG BANG (3D version) ─────────────────────────────────
    function doBigBang() {
        sessionStorage.setItem('bigbang', '1');

        transitionOverlay.style.transition = 'none';
        transitionOverlay.style.opacity = '1';

        document.querySelectorAll('[data-orbit="planet"]').forEach(function (el) {
            el.style.display = 'none';
        });

        // Create explosion particles in 3D
        var particles = [];
        var particleGeom = new THREE.BufferGeometry();
        var particleCount = 800;
        var positions = new Float32Array(particleCount * 3);
        var colors = new Float32Array(particleCount * 3);
        var velocities = [];

        for (var i = 0; i < particleCount; i++) {
            positions[i * 3] = 0;
            positions[i * 3 + 1] = 0;
            positions[i * 3 + 2] = 0;

            var roll = Math.random();
            if (roll < 0.12) {
                colors[i * 3] = 0; colors[i * 3 + 1] = 0.94; colors[i * 3 + 2] = 1;
            } else if (roll < 0.5) {
                colors[i * 3] = 0.66; colors[i * 3 + 1] = 0.33; colors[i * 3 + 2] = 0.97;
            } else {
                colors[i * 3] = 0.75; colors[i * 3 + 1] = 0.52; colors[i * 3 + 2] = 0.99;
            }

            var theta = Math.random() * Math.PI * 2;
            var phi = Math.acos(2 * Math.random() - 1);
            var speed = 3 + Math.random() * 20;
            velocities.push({
                x: Math.sin(phi) * Math.cos(theta) * speed,
                y: Math.sin(phi) * Math.sin(theta) * speed,
                z: Math.cos(phi) * speed,
                life: 1
            });
        }

        particleGeom.setAttribute('position', new THREE.BufferAttribute(positions, 3));
        particleGeom.setAttribute('color', new THREE.BufferAttribute(colors, 3));

        var particleMat = new THREE.PointsMaterial({
            size: 0.3,
            vertexColors: true,
            transparent: true,
            opacity: 1,
            blending: THREE.AdditiveBlending,
            depthWrite: false,
            sizeAttenuation: true
        });

        var particleMesh = new THREE.Points(particleGeom, particleMat);
        scene.add(particleMesh);

        // Seed glow
        var seedGeom = new THREE.SphereGeometry(0.1, 16, 16);
        var seedMat = new THREE.MeshBasicMaterial({ color: 0xA855F7, transparent: true, opacity: 1 });
        var seed = new THREE.Mesh(seedGeom, seedMat);
        scene.add(seed);

        var seedGlow = new THREE.PointLight(0xA855F7, 5, 30);
        scene.add(seedGlow);

        var frame = 0;
        var SEED_FRAMES = 40;
        var exploded = false;
        var overlayFaded = false;
        var planetsLaunched = false;
        var extrasLaunched = false;

        function bangTick() {
            frame++;

            // Seed glow phase
            if (!exploded && frame < SEED_FRAMES) {
                var progress = frame / SEED_FRAMES;
                seed.scale.setScalar(1 + progress * 3);
                seedGlow.intensity = 5 + progress * 20;
                seedMat.opacity = 0.5 + progress * 0.5;
            }

            // Explode
            if (!exploded && frame >= SEED_FRAMES) {
                exploded = true;
                scene.remove(seed);
                seedGlow.intensity = 30;
            }

            // Animate explosion particles
            if (exploded) {
                var posAttr = particleGeom.getAttribute('position');
                var allDead = true;
                for (var j = 0; j < particleCount; j++) {
                    var v = velocities[j];
                    v.life -= 0.008;
                    if (v.life <= 0) continue;
                    allDead = false;

                    posAttr.array[j * 3] += v.x * 0.016;
                    posAttr.array[j * 3 + 1] += v.y * 0.016;
                    posAttr.array[j * 3 + 2] += v.z * 0.016;
                    v.x *= 0.985;
                    v.y *= 0.985;
                    v.z *= 0.985;
                }
                posAttr.needsUpdate = true;
                particleMat.opacity = Math.max(0, velocities[0].life);

                // Fade seed glow
                if (seedGlow.intensity > 0) seedGlow.intensity *= 0.97;

                if (allDead) {
                    scene.remove(particleMesh);
                    scene.remove(seedGlow);
                }
            }

            // Fade overlay
            if (!overlayFaded && frame >= SEED_FRAMES + 8) {
                overlayFaded = true;
                transitionOverlay.style.transition = 'opacity 1.5s ease';
                transitionOverlay.style.opacity = '0';
            }

            // Launch planets from center
            if (!planetsLaunched && frame >= SEED_FRAMES + 50) {
                planetsLaunched = true;
                buildOrbit(true);
            }

            // Extras
            if (!extrasLaunched && frame >= SEED_FRAMES + 80) {
                extrasLaunched = true;
                spawnAsteroids3D();
                if (document.querySelector('.icon-planet')) spawnFloatingWords();
            }

            if (!extrasLaunched || (exploded && particleMat.opacity > 0.01)) {
                requestAnimationFrame(bangTick);
            }
        }

        requestAnimationFrame(bangTick);
    }

    // ─── NAVIGATION ────────────────────────────────────────────
    function setupNavigation() {
        transitionOverlay = document.createElement('div');
        transitionOverlay.style.cssText = 'position:fixed;top:0;left:0;width:100vw;height:100vh;background:' + BG + ';z-index:9999;pointer-events:none;opacity:0;transition:opacity 0.35s ease;';
        document.body.appendChild(transitionOverlay);

        var fromNav = sessionStorage.getItem('space-nav');
        if (fromNav) {
            transitionOverlay.style.opacity = '1';
            transitionOverlay.style.transition = 'none';
            requestAnimationFrame(function () {
                requestAnimationFrame(function () {
                    transitionOverlay.style.transition = 'opacity 0.4s ease';
                    transitionOverlay.style.opacity = '0';
                });
            });
            sessionStorage.removeItem('space-nav');
        }

        document.addEventListener('click', function (e) {
            var link = e.target.closest('a[href], a[data-nav]');
            if (!link) return;
            if (link.target === '_blank') return;

            var nav = link.getAttribute('data-nav');
            if (nav) {
                e.preventDefault();
                switchView(nav);
                return;
            }

            var href = link.getAttribute('href');
            if (!href || href.startsWith('http') || href.startsWith('#')) return;

            e.preventDefault();
            sessionStorage.setItem('space-nav', '1');
            transitionOverlay.style.opacity = '1';
            setTimeout(function () { window.location.href = href; }, 350);
        });
    }

    // ─── INIT ──────────────────────────────────────────────────
    window.addEventListener('load', function () {
        isOrbitPage = document.querySelectorAll('[data-orbit="planet"]').length > 0;

        if (isOrbitPage) {
            var hash = location.hash.replace('#', '');
            currentView = (hash === 'synth' || hash === 'resources') ? hash : 'home';
            document.querySelectorAll('[data-orbit="planet"]').forEach(function (el) {
                var view = el.getAttribute('data-view');
                if (view !== currentView && view !== 'always') el.style.display = 'none';
            });
        }

        setupNavigation();
        initThree();
        animate();

        var shouldBigBang = isOrbitPage && !sessionStorage.getItem('bigbang') && currentView === 'home';

        if (shouldBigBang) {
            doBigBang();
        } else {
            spawnAsteroids3D();
            if (isOrbitPage) {
                if (document.querySelector('.icon-planet')) spawnFloatingWords();
                buildOrbit(false);
            }
        }
    });

    window.addEventListener('hashchange', function () {
        if (!isOrbitPage) return;
        var h = location.hash.replace('#', '');
        var newView = (h === 'synth' || h === 'resources') ? h : 'home';
        switchView(newView);
    });

    window.addEventListener('resize', function () {
        if (!camera || !renderer) return;
        camera.aspect = window.innerWidth / window.innerHeight;
        camera.updateProjectionMatrix();
        renderer.setSize(window.innerWidth, window.innerHeight);

        if (!isOrbitPage || planetMeshes.length === 0) return;
        planetMeshes.forEach(function (p, i) { p.orbitRadius = getOrbitRadius(i); });
        rebuildOrbitRings();
    });
})();
