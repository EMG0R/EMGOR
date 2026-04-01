(function () {
    'use strict';

    var WORDS = ['sound', 'signal', 'rhythm', 'noise', 'pulse', 'wave', 'flux', 'drift', 'echo', 'void'];
    var currentView = 'home';
    var isOrbitPage = false;
    var transitionOverlay = null;
    var BG = '#0D0221';

    // Three.js globals
    var scene, camera, renderer, clock;
    var planetMeshes = [];
    var starField;
    var orbitLines = [];
    var cameraAngle = 0;
    var cameraTilt = 0.3;
    var cameraRadius = 22;
    var targetCameraAngle = 0;
    var targetCameraTilt = 0.3;

    // Planet color palette — vivid and diverse
    var PLANET_COLORS = [
        { base: 0x00CED1, emissive: 0x004D4D },  // deep teal
        { base: 0xE8A87C, emissive: 0x6B3A2C },  // rose gold
        { base: 0x4488FF, emissive: 0x1a3388 },  // electric blue
        { base: 0xFFB347, emissive: 0x774400 },  // amber
        { base: 0xFF6B6B, emissive: 0x6B1A1A },  // coral red
        { base: 0x66DDAA, emissive: 0x1a5533 },  // seafoam mint
        { base: 0xC084FC, emissive: 0x4C2D80 },  // lavender violet
        { base: 0xFF77AA, emissive: 0x6B2244 },  // bubblegum pink
        { base: 0x55EEDD, emissive: 0x1a5550 },  // aquamarine
        { base: 0xFFDD55, emissive: 0x665500 },  // golden yellow
        { base: 0x77AAFF, emissive: 0x2244aa },  // sky blue
        { base: 0xDD88CC, emissive: 0x553355 },  // orchid
    ];

    // ─── CIRCLE TEXTURE (prevents square particles) ────────────
    function createCircleTexture() {
        var size = 64;
        var canvas = document.createElement('canvas');
        canvas.width = size;
        canvas.height = size;
        var ctx = canvas.getContext('2d');
        var half = size / 2;
        var gradient = ctx.createRadialGradient(half, half, 0, half, half, half);
        gradient.addColorStop(0, 'rgba(255,255,255,1)');
        gradient.addColorStop(0.3, 'rgba(255,255,255,0.8)');
        gradient.addColorStop(0.7, 'rgba(255,255,255,0.15)');
        gradient.addColorStop(1, 'rgba(255,255,255,0)');
        ctx.fillStyle = gradient;
        ctx.fillRect(0, 0, size, size);
        var texture = new THREE.CanvasTexture(canvas);
        return texture;
    }

    // ─── THREE.JS SETUP ────────────────────────────────────────
    function initThree() {
        var canvas = document.getElementById('spaceCanvas');
        scene = new THREE.Scene();
        clock = new THREE.Clock();

        camera = new THREE.PerspectiveCamera(55, window.innerWidth / window.innerHeight, 0.1, 1000);
        camera.position.set(0, 5, cameraRadius);
        camera.lookAt(0, 0, 0);

        renderer = new THREE.WebGLRenderer({ canvas: canvas, alpha: true, antialias: true });
        renderer.setSize(window.innerWidth, window.innerHeight);
        renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
        renderer.setClearColor(0x000000, 0);

        // Point light at center (accretion disk glow)
        var centerLight = new THREE.PointLight(0xB388FF, 2.5, 100);
        centerLight.position.set(0, 0, 0);
        scene.add(centerLight);

        // Secondary warm light
        var warmLight = new THREE.PointLight(0xFF6633, 0.6, 80);
        warmLight.position.set(0, 3, 0);
        scene.add(warmLight);

        // Ambient
        var ambient = new THREE.AmbientLight(0x1a0a2e, 0.5);
        scene.add(ambient);

        // Rim light
        var rimLight = new THREE.DirectionalLight(0x4400AA, 0.3);
        rimLight.position.set(0, 10, 20);
        scene.add(rimLight);

        createStarField();
        createNebulaParticles();
    }

    // ─── STARS ─────────────────────────────────────────────────
    function createStarField() {
        var circleTexture = createCircleTexture();
        var starCount = 2500;
        var geometry = new THREE.BufferGeometry();
        var positions = new Float32Array(starCount * 3);
        var colors = new Float32Array(starCount * 3);

        for (var i = 0; i < starCount; i++) {
            var theta = Math.random() * Math.PI * 2;
            var phi = Math.acos(2 * Math.random() - 1);
            var r = 80 + Math.random() * 120;

            positions[i * 3] = r * Math.sin(phi) * Math.cos(theta);
            positions[i * 3 + 1] = r * Math.sin(phi) * Math.sin(theta);
            positions[i * 3 + 2] = r * Math.cos(phi);

            var roll = Math.random();
            if (roll < 0.12) {
                colors[i * 3] = 0.0; colors[i * 3 + 1] = 0.9; colors[i * 3 + 2] = 1.0;
            } else if (roll < 0.4) {
                colors[i * 3] = 0.7; colors[i * 3 + 1] = 0.5; colors[i * 3 + 2] = 1.0;
            } else {
                colors[i * 3] = 1.0; colors[i * 3 + 1] = 0.95; colors[i * 3 + 2] = 0.9;
            }
        }

        geometry.setAttribute('position', new THREE.BufferAttribute(positions, 3));
        geometry.setAttribute('color', new THREE.BufferAttribute(colors, 3));

        var starMaterial = new THREE.PointsMaterial({
            size: 0.6,
            map: circleTexture,
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
        var circleTexture = createCircleTexture();
        var count = 400;
        var geometry = new THREE.BufferGeometry();
        var positions = new Float32Array(count * 3);
        var colors = new Float32Array(count * 3);

        for (var i = 0; i < count; i++) {
            positions[i * 3] = (Math.random() - 0.5) * 120;
            positions[i * 3 + 1] = (Math.random() - 0.5) * 60;
            positions[i * 3 + 2] = (Math.random() - 0.5) * 120;

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
            size: 3.0,
            map: circleTexture,
            vertexColors: true,
            transparent: true,
            opacity: 0.08,
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
            opacity: 0.06,
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
            emissiveIntensity: 0.35,
            shininess: 90,
            specular: 0x555555
        });

        var mesh = new THREE.Mesh(geometry, material);

        // Soft glow sprite
        var spriteMaterial = new THREE.SpriteMaterial({
            color: col.base,
            transparent: true,
            opacity: 0.12,
            blending: THREE.AdditiveBlending
        });
        var glow = new THREE.Sprite(spriteMaterial);
        glow.scale.set(size * 3.5, size * 3.5, 1);
        mesh.add(glow);

        return mesh;
    }

    function getOrbitRadius(index) {
        // Wider spacing: starts at 4, each orbit 2.8 units further
        return 4.0 + index * 2.8;
    }

    function getOrbitSpeed(index) {
        return 0.18 / (1 + index * 0.3);
    }

    function getOrbitTilt(index) {
        var tilts = [0.08, -0.12, 0.06, -0.15, 0.1, -0.04, 0.14, -0.08, 0.05, -0.11, 0.07, -0.06];
        return tilts[index % tilts.length];
    }

    function getPlanetSize(el) {
        if (el.classList.contains('icon-planet')) return 0.35;
        return 0.55;
    }

    // ─── SPRING PHYSICS FOR SMOOTH ANIMATIONS ──────────────────
    // Critically damped spring — smooth deceleration, no stutter
    function springLerp(current, target, velocity, stiffness, damping, dt) {
        var diff = target - current;
        var springForce = diff * stiffness;
        var dampForce = -velocity * damping;
        var accel = springForce + dampForce;
        velocity += accel * dt;
        current += velocity * dt;
        return { value: current, velocity: velocity };
    }

    // ─── BUILD ORBIT SYSTEM ────────────────────────────────────
    function buildOrbit(fromCenter) {
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

            // Compute target orbital position
            var targetX = Math.cos(angle) * radius;
            var targetZ = Math.sin(angle) * radius;
            var targetY = Math.sin(tilt) * Math.sin(angle) * radius * 0.3;

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
                // Spring state for smooth fly-in
                sx: targetX, sy: targetY, sz: targetZ,
                vx: 0, vy: 0, vz: 0,
                sScale: 1,
                vScale: 0,
                spinSpeed: 0.5 + Math.random() * 1.5
            };

            if (fromCenter) {
                p.entering = true;
                // Start at center with zero velocity
                p.sx = 0; p.sy = 0; p.sz = 0;
                p.vx = 0; p.vy = 0; p.vz = 0;
                p.sScale = 0.01;
                p.vScale = 0;
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
        var dt = Math.min(clock.getDelta(), 0.05); // cap dt to prevent jumps
        var elapsed = clock.getElapsedTime();

        // Slow camera drift
        targetCameraAngle += dt * 0.03;
        targetCameraTilt = 0.3 + Math.sin(elapsed * 0.08) * 0.12;
        cameraAngle += (targetCameraAngle - cameraAngle) * 0.02;
        cameraTilt += (targetCameraTilt - cameraTilt) * 0.02;

        camera.position.x = Math.sin(cameraAngle) * cameraRadius;
        camera.position.z = Math.cos(cameraAngle) * cameraRadius;
        camera.position.y = Math.sin(cameraTilt) * 5;
        camera.lookAt(0, 0, 0);

        // Rotate star field slowly
        if (starField) {
            starField.rotation.y = elapsed * 0.005;
            starField.rotation.x = Math.sin(elapsed * 0.003) * 0.02;
        }

        // Animate planets
        var stiffness = 4.0;
        var damping = 4.0; // critically damped

        for (var i = planetMeshes.length - 1; i >= 0; i--) {
            var p = planetMeshes[i];

            if (p.leaving) {
                p.flyProgress += dt * 2.0;
                var accel = 1 + p.flyProgress * 4;
                p.mesh.position.add(p.flyDir.clone().multiplyScalar(dt * 18 * accel));
                p.mesh.rotation.x += dt * 6;
                p.mesh.rotation.z += dt * 4;

                var screenPos = projectToScreen(p.mesh.position);
                var ew = p.label.offsetWidth;
                var eh = p.label.offsetHeight;
                p.label.style.left = (screenPos.x - ew / 2) + 'px';
                p.label.style.top = (screenPos.y - eh / 2) + 'px';
                p.label.style.opacity = Math.max(0, 1 - p.flyProgress * 2).toFixed(3);

                if (p.flyProgress > 1.2) {
                    p.label.style.display = 'none';
                    scene.remove(p.mesh);
                    planetMeshes.splice(i, 1);
                }
                continue;
            }

            // Compute orbital target position
            p.angle += p.orbitSpeed * dt;
            var targetX = Math.cos(p.angle) * p.orbitRadius;
            var targetZ = Math.sin(p.angle) * p.orbitRadius;
            var targetY = Math.sin(p.orbitTilt) * Math.sin(p.angle) * p.orbitRadius * 0.3;

            if (p.entering) {
                // Spring physics for smooth arrival — no stutter
                var rx = springLerp(p.sx, targetX, p.vx, stiffness, damping, dt);
                var ry = springLerp(p.sy, targetY, p.vy, stiffness, damping, dt);
                var rz = springLerp(p.sz, targetZ, p.vz, stiffness, damping, dt);
                var rs = springLerp(p.sScale, 1, p.vScale, stiffness, damping, dt);

                p.sx = rx.value; p.vx = rx.velocity;
                p.sy = ry.value; p.vy = ry.velocity;
                p.sz = rz.value; p.vz = rz.velocity;
                p.sScale = rs.value; p.vScale = rs.velocity;

                p.mesh.position.set(p.sx, p.sy, p.sz);
                var s = Math.max(0.01, p.sScale);
                p.mesh.scale.set(s, s, s);

                p.mesh.rotation.y += dt * 2;

                // Check if settled (close enough to target)
                var dx = Math.abs(p.sx - targetX);
                var dy = Math.abs(p.sy - targetY);
                var dz = Math.abs(p.sz - targetZ);
                var ds = Math.abs(p.sScale - 1);
                if (dx < 0.01 && dy < 0.01 && dz < 0.01 && ds < 0.01) {
                    p.entering = false;
                    p.mesh.scale.set(1, 1, 1);
                }
            } else {
                // Normal orbit — direct position
                p.mesh.position.set(targetX, targetY, targetZ);
                p.mesh.rotation.y += dt * p.spinSpeed;
            }

            // Project to screen for HTML label
            var screenPos = projectToScreen(p.mesh.position);
            var ew = p.label.offsetWidth;
            var eh = p.label.offsetHeight;
            p.label.style.left = (screenPos.x - ew / 2) + 'px';
            p.label.style.top = (screenPos.y - eh / 2) + 'px';
            p.label.style.opacity = '1';
            p.label.style.zIndex = '15';

            // Depth-based label scaling
            var dist = p.mesh.position.distanceTo(camera.position);
            var labelScale = Math.max(0.55, Math.min(1.2, 14 / dist));
            p.label.style.transform = 'scale(' + labelScale.toFixed(3) + ')';
        }

        renderer.render(scene, camera);
    }

    // ─── VIEW SWITCHING ────────────────────────────────────────
    function switchView(newView) {
        if (newView === currentView) return;

        // Eject non-always planets
        planetMeshes.forEach(function (p) {
            if (p.view !== 'always' && !p.leaving) {
                p.leaving = true;
                p.flyProgress = 0;

                // Use current position to determine natural ejection direction
                var pos = p.mesh.position.clone();
                if (pos.length() < 0.1) pos.set(1, 0, 0);
                var dir = pos.clone().normalize();
                // Add some random vertical kick for fun
                dir.y += (Math.random() - 0.5) * 1.5;
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

                // Fly in from a random 3D edge position
                var theta = Math.random() * Math.PI * 2;
                var startR = 35 + Math.random() * 15;
                var startPos = new THREE.Vector3(
                    Math.cos(theta) * startR,
                    (Math.random() - 0.5) * 20,
                    Math.sin(theta) * startR
                );
                mesh.position.copy(startPos);
                mesh.scale.set(0.3, 0.3, 0.3);
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
                    sx: startPos.x, sy: startPos.y, sz: startPos.z,
                    vx: 0, vy: 0, vz: 0,
                    sScale: 0.3,
                    vScale: 0,
                    spinSpeed: 0.5 + Math.random() * 1.5
                });
            });

            rebuildOrbitRings();
        }, 400);
    }

    // ─── ASTEROIDS (3D) ────────────────────────────────────────
    function spawnAsteroids3D() {
        var asteroids = [];

        function createAsteroid() {
            var size = 0.04 + Math.random() * 0.12;
            var geometry = new THREE.DodecahedronGeometry(size, 0);
            var isCyan = Math.random() < 0.15;
            var material = new THREE.MeshPhongMaterial({
                color: isCyan ? 0x00C8DC : 0x8C64C8,
                emissive: isCyan ? 0x004455 : 0x2a1a44,
                emissiveIntensity: 0.4,
                transparent: true,
                opacity: 0.3 + Math.random() * 0.3
            });

            var mesh = new THREE.Mesh(geometry, material);
            var angle = Math.random() * Math.PI * 2;
            var r = 30 + Math.random() * 20;
            mesh.position.set(
                Math.cos(angle) * r,
                (Math.random() - 0.5) * 15,
                Math.sin(angle) * r
            );

            var dir = new THREE.Vector3(-mesh.position.x, (Math.random() - 0.5) * 5, -mesh.position.z).normalize();
            var speed = 0.5 + Math.random() * 1.5;

            scene.add(mesh);
            asteroids.push({
                mesh: mesh, dir: dir, speed: speed,
                rotSpeed: new THREE.Vector3((Math.random() - 0.5) * 3, (Math.random() - 0.5) * 3, (Math.random() - 0.5) * 3)
            });
        }

        function scheduleSpawn() {
            setTimeout(function () {
                if (asteroids.length < 12) createAsteroid();
                scheduleSpawn();
            }, 2500 + Math.random() * 4000);
        }
        scheduleSpawn();
        for (var k = 0; k < 4; k++) createAsteroid();

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

    // ─── BIG BANG (3D) ─────────────────────────────────────────
    function doBigBang() {
        sessionStorage.setItem('bigbang', '1');

        transitionOverlay.style.transition = 'none';
        transitionOverlay.style.opacity = '1';

        document.querySelectorAll('[data-orbit="planet"]').forEach(function (el) {
            el.style.display = 'none';
        });

        var circleTexture = createCircleTexture();
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
            size: 0.25,
            map: circleTexture,
            vertexColors: true,
            transparent: true,
            opacity: 1,
            blending: THREE.AdditiveBlending,
            depthWrite: false,
            sizeAttenuation: true
        });

        var particleMesh = new THREE.Points(particleGeom, particleMat);
        scene.add(particleMesh);

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

            if (!exploded && frame < SEED_FRAMES) {
                var progress = frame / SEED_FRAMES;
                seed.scale.setScalar(1 + progress * 3);
                seedGlow.intensity = 5 + progress * 20;
                seedMat.opacity = 0.5 + progress * 0.5;
            }

            if (!exploded && frame >= SEED_FRAMES) {
                exploded = true;
                scene.remove(seed);
                seedGlow.intensity = 30;
            }

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
                if (seedGlow.intensity > 0) seedGlow.intensity *= 0.97;
                if (allDead) {
                    scene.remove(particleMesh);
                    scene.remove(seedGlow);
                }
            }

            if (!overlayFaded && frame >= SEED_FRAMES + 8) {
                overlayFaded = true;
                transitionOverlay.style.transition = 'opacity 1.5s ease';
                transitionOverlay.style.opacity = '0';
            }

            if (!planetsLaunched && frame >= SEED_FRAMES + 50) {
                planetsLaunched = true;
                buildOrbit(true);
            }

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
