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
    var cameraTilt = 0.35;
    var cameraRadius = 28;

    // Planet color palette
    var PLANET_COLORS = [
        { base: 0x00CED1, emissive: 0x004D4D },
        { base: 0xE8A87C, emissive: 0x6B3A2C },
        { base: 0x4488FF, emissive: 0x1a3388 },
        { base: 0xFFB347, emissive: 0x774400 },
        { base: 0xFF6B6B, emissive: 0x6B1A1A },
        { base: 0x66DDAA, emissive: 0x1a5533 },
        { base: 0xC084FC, emissive: 0x4C2D80 },
        { base: 0xFF77AA, emissive: 0x6B2244 },
        { base: 0x55EEDD, emissive: 0x1a5550 },
        { base: 0xFFDD55, emissive: 0x665500 },
        { base: 0x77AAFF, emissive: 0x2244aa },
        { base: 0xDD88CC, emissive: 0x553355 },
    ];

    // ─── CIRCLE TEXTURE ────────────────────────────────────────
    var circleTexture;
    function getCircleTexture() {
        if (circleTexture) return circleTexture;
        var size = 64;
        var c = document.createElement('canvas');
        c.width = size; c.height = size;
        var ctx = c.getContext('2d');
        var half = size / 2;
        var g = ctx.createRadialGradient(half, half, 0, half, half, half);
        g.addColorStop(0, 'rgba(255,255,255,1)');
        g.addColorStop(0.4, 'rgba(255,255,255,0.6)');
        g.addColorStop(0.8, 'rgba(255,255,255,0.1)');
        g.addColorStop(1, 'rgba(255,255,255,0)');
        ctx.fillStyle = g;
        ctx.fillRect(0, 0, size, size);
        circleTexture = new THREE.CanvasTexture(c);
        return circleTexture;
    }

    // ─── THREE.JS SETUP ────────────────────────────────────────
    function initThree() {
        var canvas = document.getElementById('spaceCanvas');
        scene = new THREE.Scene();
        clock = new THREE.Clock();

        camera = new THREE.PerspectiveCamera(50, window.innerWidth / window.innerHeight, 0.1, 1000);
        camera.position.set(0, 8, cameraRadius);
        camera.lookAt(0, 0, 0);

        renderer = new THREE.WebGLRenderer({ canvas: canvas, alpha: true, antialias: true });
        renderer.setSize(window.innerWidth, window.innerHeight);
        renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
        renderer.setClearColor(0x000000, 0);

        // Center light (accretion disk)
        var centerLight = new THREE.PointLight(0xB388FF, 3, 80);
        centerLight.position.set(0, 0, 0);
        scene.add(centerLight);

        // Warm secondary
        var warmLight = new THREE.PointLight(0xFF6633, 0.5, 60);
        warmLight.position.set(0, 2, 0);
        scene.add(warmLight);

        // Ambient
        scene.add(new THREE.AmbientLight(0x2a1a4e, 0.6));

        // Rim light
        var rimLight = new THREE.DirectionalLight(0x6633CC, 0.25);
        rimLight.position.set(0, 10, 20);
        scene.add(rimLight);

        createStarField();
    }

    // ─── STARS (circles only, no nebula) ───────────────────────
    function createStarField() {
        var tex = getCircleTexture();
        var count = 2000;
        var geom = new THREE.BufferGeometry();
        var pos = new Float32Array(count * 3);
        var col = new Float32Array(count * 3);

        for (var i = 0; i < count; i++) {
            var theta = Math.random() * Math.PI * 2;
            var phi = Math.acos(2 * Math.random() - 1);
            var r = 60 + Math.random() * 140;
            pos[i * 3] = r * Math.sin(phi) * Math.cos(theta);
            pos[i * 3 + 1] = r * Math.sin(phi) * Math.sin(theta);
            pos[i * 3 + 2] = r * Math.cos(phi);

            var roll = Math.random();
            if (roll < 0.1) {
                col[i * 3] = 0.0; col[i * 3 + 1] = 0.9; col[i * 3 + 2] = 1.0;
            } else if (roll < 0.35) {
                col[i * 3] = 0.7; col[i * 3 + 1] = 0.5; col[i * 3 + 2] = 1.0;
            } else {
                col[i * 3] = 1.0; col[i * 3 + 1] = 0.95; col[i * 3 + 2] = 0.9;
            }
        }

        geom.setAttribute('position', new THREE.BufferAttribute(pos, 3));
        geom.setAttribute('color', new THREE.BufferAttribute(col, 3));

        starField = new THREE.Points(geom, new THREE.PointsMaterial({
            size: 0.5,
            map: tex,
            vertexColors: true,
            transparent: true,
            opacity: 0.8,
            sizeAttenuation: true,
            blending: THREE.AdditiveBlending,
            depthWrite: false
        }));
        scene.add(starField);
    }

    // ─── ORBIT RINGS ───────────────────────────────────────────
    function createOrbitRing(radius) {
        var seg = 128;
        var geom = new THREE.BufferGeometry();
        var pos = new Float32Array(seg * 3);
        for (var i = 0; i < seg; i++) {
            var a = (i / seg) * Math.PI * 2;
            pos[i * 3] = Math.cos(a) * radius;
            pos[i * 3 + 1] = 0;
            pos[i * 3 + 2] = Math.sin(a) * radius;
        }
        geom.setAttribute('position', new THREE.BufferAttribute(pos, 3));
        return new THREE.LineLoop(geom, new THREE.LineBasicMaterial({
            color: 0xA855F7, transparent: true, opacity: 0.06,
            blending: THREE.AdditiveBlending
        }));
    }

    function rebuildOrbitRings() {
        orbitLines.forEach(function (l) { scene.remove(l); });
        orbitLines = [];
        planetMeshes.forEach(function (p) {
            if (p.leaving) return;
            var ring = createOrbitRing(p.orbitRadius);
            scene.add(ring);
            orbitLines.push(ring);
        });
    }

    // ─── PLANET MESH (NO glow sprite — that caused the boxes) ──
    function createPlanetMesh(colorIndex, size) {
        var c = PLANET_COLORS[colorIndex % PLANET_COLORS.length];
        var mesh = new THREE.Mesh(
            new THREE.SphereGeometry(size, 32, 32),
            new THREE.MeshPhongMaterial({
                color: c.base,
                emissive: c.emissive,
                emissiveIntensity: 0.4,
                shininess: 80,
                specular: 0x555555
            })
        );
        return mesh;
    }

    // ─── ORBIT SIZING — fits viewport, solar system style ──────
    // Orbits are FLAT on the xz plane, camera looks from above-ish
    function getOrbitRadius(index, total) {
        // Scale orbits to fit screen: innermost ~30% of half-screen, outermost ~85%
        var minR = 3.5;
        var maxR = 12;
        if (total <= 1) return (minR + maxR) / 2;
        return minR + (maxR - minR) * (index / (total - 1));
    }

    function getOrbitSpeed(index) {
        // Kepler-ish: inner planets faster
        return 0.25 / (1 + index * 0.35);
    }

    function getPlanetSize(el) {
        // Icon planets slightly smaller, but same ballpark
        if (el.classList.contains('icon-planet')) return 0.45;
        return 0.6;
    }

    // ─── SPRING LERP ───────────────────────────────────────────
    function springStep(cur, target, vel, stiffness, damping, dt) {
        var force = (target - cur) * stiffness - vel * damping;
        vel += force * dt;
        cur += vel * dt;
        return { v: cur, vel: vel };
    }

    // ─── BUILD ORBITS ──────────────────────────────────────────
    function buildOrbit(fromCenter) {
        planetMeshes.forEach(function (p) { scene.remove(p.mesh); });
        planetMeshes = [];

        var visible = getVisiblePlanets();
        var total = visible.length;

        visible.forEach(function (el, i) {
            el.style.position = 'fixed';
            el.style.display = 'flex';

            var radius = getOrbitRadius(i, total);
            var speed = getOrbitSpeed(i);
            var angle = (i / total) * Math.PI * 2;
            var size = getPlanetSize(el);

            var mesh = createPlanetMesh(i, size);
            scene.add(mesh);

            // Target position (flat xz plane like solar system)
            var tx = Math.cos(angle) * radius;
            var tz = Math.sin(angle) * radius;

            var p = {
                mesh: mesh,
                label: el,
                orbitRadius: radius,
                orbitSpeed: speed,
                angle: angle,
                view: el.getAttribute('data-view'),
                entering: false,
                leaving: false,
                flyDir: new THREE.Vector3(),
                flyProgress: 0,
                // Spring state
                px: tx, py: 0, pz: tz,
                vx: 0, vy: 0, vz: 0,
                sc: 1, vsc: 0,
                spinSpeed: 0.4 + Math.random() * 1.0
            };

            if (fromCenter) {
                p.entering = true;
                p.px = 0; p.py = 0; p.pz = 0;
                p.sc = 0.01;
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
    function projectToScreen(pos) {
        var v = pos.clone().project(camera);
        return {
            x: (v.x * 0.5 + 0.5) * window.innerWidth,
            y: (-v.y * 0.5 + 0.5) * window.innerHeight
        };
    }

    // ─── MAIN ANIMATION LOOP ───────────────────────────────────
    function animate() {
        requestAnimationFrame(animate);
        var dt = Math.min(clock.getDelta(), 0.05);
        var elapsed = clock.getElapsedTime();

        // Slow camera drift — stays above, looking at center
        cameraAngle += dt * 0.025;
        cameraTilt = 0.35 + Math.sin(elapsed * 0.06) * 0.08;

        camera.position.x = Math.sin(cameraAngle) * cameraRadius;
        camera.position.z = Math.cos(cameraAngle) * cameraRadius;
        camera.position.y = 6 + Math.sin(cameraTilt) * 3;
        camera.lookAt(0, 0, 0);

        if (starField) {
            starField.rotation.y = elapsed * 0.004;
        }

        // Spring params
        var stiff = 3.5;
        var damp = 3.7;

        for (var i = planetMeshes.length - 1; i >= 0; i--) {
            var p = planetMeshes[i];

            if (p.leaving) {
                p.flyProgress += dt * 2.5;
                var acc = 1 + p.flyProgress * 5;
                p.mesh.position.add(p.flyDir.clone().multiplyScalar(dt * 20 * acc));
                p.mesh.rotation.x += dt * 5;
                p.mesh.rotation.z += dt * 3;

                var sp = projectToScreen(p.mesh.position);
                p.label.style.left = (sp.x - p.label.offsetWidth / 2) + 'px';
                p.label.style.top = (sp.y - p.label.offsetHeight / 2) + 'px';
                p.label.style.opacity = Math.max(0, 1 - p.flyProgress * 2.5).toFixed(3);

                if (p.flyProgress > 0.8) {
                    p.label.style.display = 'none';
                    scene.remove(p.mesh);
                    planetMeshes.splice(i, 1);
                }
                continue;
            }

            // Compute orbital target (flat xz plane)
            p.angle += p.orbitSpeed * dt;
            var tx = Math.cos(p.angle) * p.orbitRadius;
            var tz = Math.sin(p.angle) * p.orbitRadius;
            var ty = 0; // flat solar system plane

            if (p.entering) {
                var rx = springStep(p.px, tx, p.vx, stiff, damp, dt);
                var ry = springStep(p.py, ty, p.vy, stiff, damp, dt);
                var rz = springStep(p.pz, tz, p.vz, stiff, damp, dt);
                var rs = springStep(p.sc, 1, p.vsc, stiff, damp, dt);

                p.px = rx.v; p.vx = rx.vel;
                p.py = ry.v; p.vy = ry.vel;
                p.pz = rz.v; p.vz = rz.vel;
                p.sc = rs.v; p.vsc = rs.vel;

                p.mesh.position.set(p.px, p.py, p.pz);
                var s = Math.max(0.01, p.sc);
                p.mesh.scale.set(s, s, s);
                p.mesh.rotation.y += dt * 2;

                // Settled?
                if (Math.abs(p.px - tx) < 0.05 && Math.abs(p.pz - tz) < 0.05 && Math.abs(p.sc - 1) < 0.02) {
                    p.entering = false;
                    p.mesh.scale.set(1, 1, 1);
                }
            } else {
                p.mesh.position.set(tx, ty, tz);
                p.mesh.rotation.y += dt * p.spinSpeed;
            }

            // Project to screen for label
            var sp = projectToScreen(p.mesh.position);
            var ew = p.label.offsetWidth;
            var eh = p.label.offsetHeight;
            p.label.style.left = (sp.x - ew / 2) + 'px';
            p.label.style.top = (sp.y - eh / 2) + 'px';
            p.label.style.opacity = '1';
            p.label.style.zIndex = '15';

            // Depth-based label scaling — consistent for all planets
            var dist = p.mesh.position.distanceTo(camera.position);
            var labelScale = Math.max(0.6, Math.min(1.15, 18 / dist));
            p.label.style.transform = 'scale(' + labelScale.toFixed(3) + ')';
        }

        renderer.render(scene, camera);
    }

    // ─── VIEW SWITCHING ────────────────────────────────────────
    function switchView(newView) {
        if (newView === currentView) return;

        planetMeshes.forEach(function (p) {
            if (p.view !== 'always' && !p.leaving) {
                p.leaving = true;
                p.flyProgress = 0;
                // Eject outward from center
                var pos = p.mesh.position.clone();
                if (pos.length() < 0.1) pos.set(1, 0, 0);
                var dir = pos.clone().normalize();
                dir.y += (Math.random() - 0.5) * 0.8;
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

            var alwaysPlanets = planetMeshes.filter(function (p) {
                return p.view === 'always' && !p.leaving;
            });
            var alwaysCount = alwaysPlanets.length;
            var totalNew = alwaysCount + newEls.length;

            newEls.forEach(function (el, i) {
                el.style.display = 'flex';
                el.style.position = 'fixed';

                var idx = alwaysCount + i;
                var radius = getOrbitRadius(idx, totalNew);
                var speed = getOrbitSpeed(idx);
                var angle = (idx / totalNew) * Math.PI * 2;
                var size = getPlanetSize(el);

                var mesh = createPlanetMesh(i + alwaysCount, size);

                // Start from edge of scene but within a reasonable range
                var startAngle = Math.random() * Math.PI * 2;
                var startR = 25;
                var startPos = new THREE.Vector3(
                    Math.cos(startAngle) * startR,
                    (Math.random() - 0.5) * 4,
                    Math.sin(startAngle) * startR
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
                    view: el.getAttribute('data-view'),
                    entering: true,
                    leaving: false,
                    flyDir: new THREE.Vector3(),
                    flyProgress: 0,
                    px: startPos.x, py: startPos.y, pz: startPos.z,
                    vx: 0, vy: 0, vz: 0,
                    sc: 0.3, vsc: 0,
                    spinSpeed: 0.4 + Math.random() * 1.0
                });
            });

            // Update always-planets orbit radii for new total
            alwaysPlanets.forEach(function (p, i) {
                p.orbitRadius = getOrbitRadius(i, totalNew);
            });

            rebuildOrbitRings();
        }, 350);
    }

    // ─── ASTEROIDS ─────────────────────────────────────────────
    function spawnAsteroids3D() {
        var asteroids = [];

        function createAsteroid() {
            var size = 0.04 + Math.random() * 0.1;
            var mesh = new THREE.Mesh(
                new THREE.DodecahedronGeometry(size, 0),
                new THREE.MeshPhongMaterial({
                    color: Math.random() < 0.15 ? 0x00C8DC : 0x8C64C8,
                    emissive: Math.random() < 0.15 ? 0x004455 : 0x2a1a44,
                    emissiveIntensity: 0.4,
                    transparent: true,
                    opacity: 0.3 + Math.random() * 0.25
                })
            );
            var a = Math.random() * Math.PI * 2;
            var r = 25 + Math.random() * 20;
            mesh.position.set(Math.cos(a) * r, (Math.random() - 0.5) * 8, Math.sin(a) * r);
            var dir = new THREE.Vector3(-mesh.position.x, (Math.random() - 0.5) * 3, -mesh.position.z).normalize();
            scene.add(mesh);
            asteroids.push({
                mesh: mesh, dir: dir, speed: 0.4 + Math.random() * 1.2,
                rs: new THREE.Vector3((Math.random() - 0.5) * 2, (Math.random() - 0.5) * 2, (Math.random() - 0.5) * 2)
            });
        }

        (function loop() {
            setTimeout(function () {
                if (asteroids.length < 10) createAsteroid();
                loop();
            }, 3000 + Math.random() * 4000);
        })();
        for (var k = 0; k < 3; k++) createAsteroid();

        (function tick() {
            for (var j = asteroids.length - 1; j >= 0; j--) {
                var a = asteroids[j];
                a.mesh.position.add(a.dir.clone().multiplyScalar(a.speed * 0.016));
                a.mesh.rotation.x += a.rs.x * 0.016;
                a.mesh.rotation.y += a.rs.y * 0.016;
                if (a.mesh.position.length() > 55) {
                    scene.remove(a.mesh);
                    asteroids.splice(j, 1);
                }
            }
            requestAnimationFrame(tick);
        })();
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
        (function tick() {
            var W = window.innerWidth, H = window.innerHeight;
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
            requestAnimationFrame(tick);
        })();
    }

    // ─── BIG BANG ──────────────────────────────────────────────
    function doBigBang() {
        sessionStorage.setItem('bigbang', '1');
        transitionOverlay.style.transition = 'none';
        transitionOverlay.style.opacity = '1';

        document.querySelectorAll('[data-orbit="planet"]').forEach(function (el) {
            el.style.display = 'none';
        });

        var tex = getCircleTexture();
        var count = 600;
        var geom = new THREE.BufferGeometry();
        var pos = new Float32Array(count * 3);
        var col = new Float32Array(count * 3);
        var vels = [];

        for (var i = 0; i < count; i++) {
            pos[i * 3] = pos[i * 3 + 1] = pos[i * 3 + 2] = 0;
            var roll = Math.random();
            if (roll < 0.12) { col[i*3]=0; col[i*3+1]=0.94; col[i*3+2]=1; }
            else if (roll < 0.5) { col[i*3]=0.66; col[i*3+1]=0.33; col[i*3+2]=0.97; }
            else { col[i*3]=0.75; col[i*3+1]=0.52; col[i*3+2]=0.99; }

            var th = Math.random() * Math.PI * 2;
            var ph = Math.acos(2 * Math.random() - 1);
            var sp = 2 + Math.random() * 18;
            vels.push({ x: Math.sin(ph)*Math.cos(th)*sp, y: Math.sin(ph)*Math.sin(th)*sp, z: Math.cos(ph)*sp, life: 1 });
        }
        geom.setAttribute('position', new THREE.BufferAttribute(pos, 3));
        geom.setAttribute('color', new THREE.BufferAttribute(col, 3));

        var mat = new THREE.PointsMaterial({
            size: 0.2, map: tex, vertexColors: true, transparent: true, opacity: 1,
            blending: THREE.AdditiveBlending, depthWrite: false, sizeAttenuation: true
        });
        var pts = new THREE.Points(geom, mat);
        scene.add(pts);

        var seedMesh = new THREE.Mesh(
            new THREE.SphereGeometry(0.1, 16, 16),
            new THREE.MeshBasicMaterial({ color: 0xA855F7, transparent: true, opacity: 1 })
        );
        scene.add(seedMesh);
        var seedGlow = new THREE.PointLight(0xA855F7, 5, 30);
        scene.add(seedGlow);

        var frame = 0, SF = 40;
        var exploded = false, overlayDone = false, planetsDone = false, extrasDone = false;

        (function tick() {
            frame++;
            if (!exploded && frame < SF) {
                var pr = frame / SF;
                seedMesh.scale.setScalar(1 + pr * 3);
                seedGlow.intensity = 5 + pr * 20;
            }
            if (!exploded && frame >= SF) {
                exploded = true;
                scene.remove(seedMesh);
                seedGlow.intensity = 30;
            }
            if (exploded) {
                var pa = geom.getAttribute('position');
                for (var j = 0; j < count; j++) {
                    var v = vels[j]; v.life -= 0.008;
                    if (v.life <= 0) continue;
                    pa.array[j*3] += v.x*0.016; pa.array[j*3+1] += v.y*0.016; pa.array[j*3+2] += v.z*0.016;
                    v.x *= 0.985; v.y *= 0.985; v.z *= 0.985;
                }
                pa.needsUpdate = true;
                mat.opacity = Math.max(0, vels[0].life);
                if (seedGlow.intensity > 0) seedGlow.intensity *= 0.97;
                if (mat.opacity <= 0.01) { scene.remove(pts); scene.remove(seedGlow); }
            }
            if (!overlayDone && frame >= SF + 8) {
                overlayDone = true;
                transitionOverlay.style.transition = 'opacity 1.5s ease';
                transitionOverlay.style.opacity = '0';
            }
            if (!planetsDone && frame >= SF + 50) { planetsDone = true; buildOrbit(true); }
            if (!extrasDone && frame >= SF + 80) {
                extrasDone = true;
                spawnAsteroids3D();
                if (document.querySelector('.icon-planet')) spawnFloatingWords();
            }
            if (!extrasDone || (exploded && mat.opacity > 0.01)) requestAnimationFrame(tick);
        })();
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
            if (nav) { e.preventDefault(); switchView(nav); return; }

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

        if (isOrbitPage && !sessionStorage.getItem('bigbang') && currentView === 'home') {
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
        switchView((h === 'synth' || h === 'resources') ? h : 'home');
    });

    window.addEventListener('resize', function () {
        if (!camera || !renderer) return;
        camera.aspect = window.innerWidth / window.innerHeight;
        camera.updateProjectionMatrix();
        renderer.setSize(window.innerWidth, window.innerHeight);
    });
})();
