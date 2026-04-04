(function () {
    'use strict';

    // ─── CONSTANTS ─────────────────────────────────────────────
    var WORDS = ['sound', 'signal', 'rhythm', 'noise', 'pulse', 'wave', 'flux', 'drift', 'echo', 'void'];
    var BG = '#0D0221';
    var PLANET_COLORS = [
        { base: 0x0C5C54, emissive: 0x062E2A },
        { base: 0x8C2840, emissive: 0x461420 },
        { base: 0x15406E, emissive: 0x0A2037 },
        { base: 0x8E5010, emissive: 0x472808 },
        { base: 0x3E2888, emissive: 0x1F1444 },
        { base: 0x106848, emissive: 0x083424 },
        { base: 0x6E3080, emissive: 0x371840 },
        { base: 0x882818, emissive: 0x44140C },
        { base: 0x0E5868, emissive: 0x072C34 },
        { base: 0x685010, emissive: 0x342808 },
        { base: 0x283868, emissive: 0x141C34 },
        { base: 0x6E3848, emissive: 0x371C24 },
    ];

    // ─── STATE ─────────────────────────────────────────────────
    var currentView = 'home';
    var isOrbitPage = false;
    var transitionOverlay = null;
    var scene, camera, renderer, clock;
    var planetMeshes = [];
    var starField;
    var orbitLines = [];
    var cameraAngle = 0;

    // Camera defaults
    var CAM_R = 28;
    var CAM_H = 14;
    var CAM_FOV = 55;

    // Zoom / moon state
    var viewState = 'solar'; // solar | zooming_in | moons | zooming_out
    var zoomTimer = 0;
    var ZOOM_DUR = 1.0;
    var zoomedIdx = -1;
    var zoomStartPos = { x: 0, y: 0, z: 0 };
    var zoomEndPos = { x: 0, y: 0, z: 0 };
    var zoomStartLook = { x: 0, y: 0, z: 0 };
    var zoomEndLook = { x: 0, y: 0, z: 0 };
    var moonMeshes = [];
    var moonOrbitLines = [];

    // Cached safe orbit radius
    var _safeMaxR = null;

    // ─── UTILITIES ─────────────────────────────────────────────
    function lerp(a, b, t) { return a + (b - a) * t; }
    function easeInOut(t) { return t < 0.5 ? 4*t*t*t : 1 - Math.pow(-2*t+2, 3) / 2; }

    function spr(cur, tgt, vel, k, d, dt) {
        var f = (tgt - cur) * k - vel * d;
        vel += f * dt; cur += vel * dt;
        return { v: cur, vel: vel };
    }

    function orbitalPos(angle, radius, inclination) {
        return {
            x: Math.cos(angle) * radius,
            y: Math.sin(angle) * radius * Math.sin(inclination) * 0.06,
            z: Math.sin(angle) * radius
        };
    }

    // ─── CIRCLE TEXTURE ────────────────────────────────────────
    var _circTex;
    function circTex() {
        if (_circTex) return _circTex;
        var c = document.createElement('canvas');
        c.width = c.height = 64;
        var ctx = c.getContext('2d'), h = 32;
        var g = ctx.createRadialGradient(h, h, 0, h, h, h);
        g.addColorStop(0, 'rgba(255,255,255,1)');
        g.addColorStop(0.4, 'rgba(255,255,255,0.6)');
        g.addColorStop(0.8, 'rgba(255,255,255,0.1)');
        g.addColorStop(1, 'rgba(255,255,255,0)');
        ctx.fillStyle = g; ctx.fillRect(0, 0, 64, 64);
        _circTex = new THREE.CanvasTexture(c);
        return _circTex;
    }

    // ─── THREE.JS SETUP ────────────────────────────────────────
    function initThree() {
        var canvas = document.getElementById('spaceCanvas');
        if (!canvas || typeof THREE === 'undefined') return;

        scene = new THREE.Scene();
        clock = new THREE.Clock();

        camera = new THREE.PerspectiveCamera(CAM_FOV, window.innerWidth / window.innerHeight, 0.1, 1000);
        camera.position.set(0, CAM_H, CAM_R);
        camera.lookAt(0, 0, 0);

        renderer = new THREE.WebGLRenderer({ canvas: canvas, alpha: true, antialias: true });
        renderer.setSize(window.innerWidth, window.innerHeight);
        renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
        renderer.setClearColor(0x000000, 0);

        scene.add(new THREE.AmbientLight(0x443355, 0.5));
        scene.add(new THREE.HemisphereLight(0x554466, 0x110822, 0.4));

        var dl1 = new THREE.DirectionalLight(0x887799, 0.3);
        dl1.position.set(10, 8, 12); scene.add(dl1);
        var dl2 = new THREE.DirectionalLight(0x665577, 0.2);
        dl2.position.set(-10, 4, -8); scene.add(dl2);

        var accretion = new THREE.PointLight(0x5511AA, 0.5, 18);
        accretion.position.set(0, 0.3, 0); scene.add(accretion);

        var bhOccluder = new THREE.Mesh(
            new THREE.SphereGeometry(2.0, 32, 32),
            new THREE.MeshBasicMaterial({ color: 0x0D0221 })
        );
        scene.add(bhOccluder);

        createStarField();

        // Compute safe orbit radius now that camera exists
        camera.updateMatrixWorld(true);
        _safeMaxR = computeSafeMaxRadius();
    }

    // ─── STARS ─────────────────────────────────────────────────
    function createStarField() {
        var tex = circTex(), n = 3500;
        var geom = new THREE.BufferGeometry();
        var pos = new Float32Array(n * 3), col = new Float32Array(n * 3);
        for (var i = 0; i < n; i++) {
            var th = Math.random() * Math.PI * 2;
            var ph = Math.acos(2 * Math.random() - 1);
            var r = 60 + Math.random() * 180;
            pos[i*3] = r * Math.sin(ph) * Math.cos(th);
            pos[i*3+1] = r * Math.sin(ph) * Math.sin(th);
            pos[i*3+2] = r * Math.cos(ph);
            var roll = Math.random();
            if (roll < 0.08) { col[i*3] = 0.3; col[i*3+1] = 0.9; col[i*3+2] = 1.0; }
            else if (roll < 0.25) { col[i*3] = 0.7; col[i*3+1] = 0.5; col[i*3+2] = 1.0; }
            else { var b = 0.7 + Math.random()*0.3; col[i*3]=b; col[i*3+1]=b; col[i*3+2]=b; }
        }
        geom.setAttribute('position', new THREE.BufferAttribute(pos, 3));
        geom.setAttribute('color', new THREE.BufferAttribute(col, 3));
        starField = new THREE.Points(geom, new THREE.PointsMaterial({
            size: 0.6, map: tex, vertexColors: true, transparent: true,
            opacity: 0.9, sizeAttenuation: true, blending: THREE.AdditiveBlending, depthWrite: false
        }));
        scene.add(starField);
    }

    // ─── DASHED ORBIT RING ─────────────────────────────────────
    function makeDashedRing(radius, color, opacity) {
        var seg = 128, points = [];
        for (var i = 0; i <= seg; i++) {
            var a = (i / seg) * Math.PI * 2;
            points.push(new THREE.Vector3(Math.cos(a) * radius, 0, Math.sin(a) * radius));
        }
        var geom = new THREE.BufferGeometry().setFromPoints(points);
        var mat = new THREE.LineDashedMaterial({
            color: color || 0xA855F7, transparent: true,
            opacity: opacity || 0.15,
            dashSize: 0.5, gapSize: 0.3,
            blending: THREE.AdditiveBlending
        });
        var line = new THREE.Line(geom, mat);
        line.computeLineDistances();
        return line;
    }

    function rebuildRings() {
        orbitLines.forEach(function (l) { scene.remove(l); });
        orbitLines = [];
        planetMeshes.forEach(function (p) {
            if (p.leaving) return;
            var ring = makeDashedRing(p.orbitRadius, 0xA855F7, 0.15);
            scene.add(ring);
            orbitLines.push(ring);
        });
    }

    // ─── PLANET MESH ───────────────────────────────────────────
    function createPlanetMesh(colorIndex, size) {
        var c = PLANET_COLORS[colorIndex % PLANET_COLORS.length];
        return new THREE.Mesh(
            new THREE.SphereGeometry(size, 48, 48),
            new THREE.MeshPhongMaterial({
                color: c.base, emissive: c.emissive,
                emissiveIntensity: 0.3, shininess: 20, specular: 0x222222
            })
        );
    }

    // ─── SCREEN-SAFE ORBIT MATH ────────────────────────────────
    function computeSafeMaxRadius() {
        if (!camera) return 12;
        var saved = camera.position.clone();
        camera.position.set(0, CAM_H, CAM_R);
        camera.lookAt(0, 0, 0);
        camera.updateMatrixWorld(true);

        var W = window.innerWidth, H = window.innerHeight;
        var mx = 90, my = 70;
        var maxFrac = 0;
        for (var i = 0; i < 72; i++) {
            var a = (i / 72) * Math.PI * 2;
            var p = new THREE.Vector3(Math.cos(a), 0, Math.sin(a));
            var v = p.clone().project(camera);
            maxFrac = Math.max(maxFrac,
                Math.abs(v.x) * (W/2) / ((W/2) - mx),
                Math.abs(v.y) * (H/2) / ((H/2) - my)
            );
        }
        camera.position.copy(saved);
        camera.updateMatrixWorld(true);
        return maxFrac > 0 ? 1.0 / maxFrac : 12;
    }

    function getSafeMaxR() {
        if (_safeMaxR === null) _safeMaxR = computeSafeMaxRadius();
        return _safeMaxR;
    }

    function computeOrbits(count) {
        var maxR = getSafeMaxR();
        var minR = 3.8;
        if (count === 0) return [];
        if (count === 1) return [{ radius: maxR * 0.5, size: Math.min(2.2, maxR * 0.12) }];

        // Even spacing with slight exponential bias
        var radii = [];
        for (var i = 0; i < count; i++) {
            var t = i / (count - 1);
            var curve = t * 0.7 + t * t * 0.3;
            radii.push(minR + (maxR - minR) * curve);
        }

        // Min gap between adjacent orbits
        var minGap = Infinity;
        for (var i = 1; i < count; i++) minGap = Math.min(minGap, radii[i] - radii[i-1]);
        minGap = Math.min(minGap, radii[0] - 2.0);

        // Planet size: 38% of min gap (guaranteed no overlap)
        var baseSize = Math.min(2.2, Math.max(0.6, minGap * 0.38));

        var orbits = [];
        for (var i = 0; i < count; i++) {
            var t = count <= 1 ? 0.5 : i / (count - 1);
            orbits.push({ radius: radii[i], size: baseSize * (0.85 + t * 0.3) });
        }
        return orbits;
    }

    function getOrbitSpeed(index) {
        return 0.30 / (1 + index * 0.4);
    }

    function getInclination(index) {
        var vals = [0.08, -0.12, 0.1, -0.07, 0.11, -0.06, 0.09, -0.1, 0.07, -0.08, 0.1, -0.09];
        return vals[index % vals.length];
    }

    // ─── BLACK HOLE OVERLAY ────────────────────────────────────
    function createBlackHoleOverlay() {
        var overlay = document.createElement('div');
        overlay.className = 'black-hole-overlay';
        document.body.appendChild(overlay);
        updateBlackHoleOverlay();
    }

    function updateBlackHoleOverlay() {
        var overlay = document.querySelector('.black-hole-overlay');
        if (!overlay) return;
        var size = Math.min(window.innerWidth, window.innerHeight) * 0.22;
        overlay.style.width = size + 'px';
        overlay.style.height = size + 'px';
    }

    // ─── BUILD ORBITS ──────────────────────────────────────────
    function buildOrbit(fromCenter) {
        planetMeshes.forEach(function (p) { scene.remove(p.mesh); });
        planetMeshes = [];

        var visible = getVisiblePlanets();
        var total = visible.length;
        var orbits = computeOrbits(total);

        visible.forEach(function (el, i) {
            el.style.position = 'fixed';
            el.style.display = 'flex';

            var o = orbits[i];
            var speed = getOrbitSpeed(i);
            var angle = (i / total) * Math.PI * 2;
            var incl = getInclination(i);

            var mesh = createPlanetMesh(i, o.size);
            scene.add(mesh);

            var tgt = orbitalPos(angle, o.radius, incl);

            var p = {
                mesh: mesh, label: el,
                orbitRadius: o.radius, orbitSpeed: speed,
                angle: angle, inclination: incl,
                planetSize: o.size,
                view: el.getAttribute('data-view'),
                entering: false, leaving: false,
                flyDir: new THREE.Vector3(), flyProgress: 0,
                px: tgt.x, py: tgt.y, pz: tgt.z,
                vx: 0, vy: 0, vz: 0,
                sc: 1, vsc: 0,
                spinSpeed: 0.3 + Math.random() * 0.8
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

        rebuildRings();
    }

    function getVisiblePlanets() {
        return Array.from(document.querySelectorAll('[data-orbit="planet"]')).filter(function (el) {
            var view = el.getAttribute('data-view');
            return view === 'home' || view === 'always';
        });
    }

    // ─── PROJECTION ────────────────────────────────────────────
    function proj(pos) {
        var v = pos.clone().project(camera);
        return {
            x: (v.x * 0.5 + 0.5) * window.innerWidth,
            y: (-v.y * 0.5 + 0.5) * window.innerHeight
        };
    }

    function apparentSize(worldRadius, worldPos) {
        var dist = worldPos.distanceTo(camera.position);
        if (dist < 0.1) dist = 0.1;
        var fovRad = (CAM_FOV / 2) * Math.PI / 180;
        return (worldRadius / (dist * Math.tan(fovRad))) * (window.innerHeight / 2);
    }

    // ─── ZOOM STATE MACHINE ────────────────────────────────────
    function startZoomIn(idx) {
        if (viewState !== 'solar') return;
        var p = planetMeshes[idx];
        if (!p) return;

        viewState = 'zooming_in';
        zoomTimer = 0;
        zoomedIdx = idx;

        // Save current camera
        zoomStartPos = { x: camera.position.x, y: camera.position.y, z: camera.position.z };
        zoomStartLook = { x: 0, y: 0, z: 0 };

        // Target: 10 units from planet, above and behind
        var pp = p.mesh.position;
        var dir = new THREE.Vector3(0, CAM_H, CAM_R).normalize();
        zoomEndPos = {
            x: pp.x + dir.x * 10,
            y: pp.y + dir.y * 10 + 2,
            z: pp.z + dir.z * 10
        };
        zoomEndLook = { x: pp.x, y: pp.y, z: pp.z };
    }

    function completeZoomIn() {
        viewState = 'moons';

        // Hide non-zoomed planets + orbit rings
        planetMeshes.forEach(function (p, i) {
            if (i !== zoomedIdx) {
                p.label.style.display = 'none';
                p.mesh.visible = false;
            }
        });
        orbitLines.forEach(function (l) { l.visible = false; });

        // Hide black hole overlay
        var bh = document.querySelector('.black-hole-overlay');
        if (bh) bh.style.opacity = '0';

        spawnMoons();
    }

    function startZoomOut() {
        if (viewState !== 'moons') return;
        viewState = 'zooming_out';
        zoomTimer = 0;

        // Save current camera
        zoomStartPos = { x: camera.position.x, y: camera.position.y, z: camera.position.z };
        zoomStartLook = { x: zoomEndLook.x, y: zoomEndLook.y, z: zoomEndLook.z };

        // Target: default
        zoomEndPos = { x: Math.sin(cameraAngle) * CAM_R, y: CAM_H, z: Math.cos(cameraAngle) * CAM_R };
        zoomEndLook = { x: 0, y: 0, z: 0 };

        clearMoons();
    }

    function completeZoomOut() {
        viewState = 'solar';
        zoomedIdx = -1;

        // Show all planets + orbit rings
        planetMeshes.forEach(function (p) {
            p.label.style.display = 'flex';
            p.mesh.visible = true;
        });
        orbitLines.forEach(function (l) { l.visible = true; });

        var bh = document.querySelector('.black-hole-overlay');
        if (bh) bh.style.opacity = '1';

        // Rebuild to get correct sizes
        buildOrbit(false);
    }

    // ─── MOON SYSTEM ───────────────────────────────────────────
    var MOON_TILT = 0.25;

    function spawnMoons() {
        var parent = planetMeshes[zoomedIdx];
        if (!parent) return;
        var navTarget = parent.label.getAttribute('data-nav');
        if (!navTarget) return;

        var moonEls = Array.from(document.querySelectorAll('[data-orbit="planet"]')).filter(function(el) {
            return el.getAttribute('data-view') === navTarget;
        });

        var parentPos = parent.mesh.position;
        var moonCount = moonEls.length;

        moonEls.forEach(function (el, i) {
            el.style.display = 'flex';
            el.style.position = 'fixed';

            var moonRadius = 1.8 + i * 1.4;
            var moonSize = 0.35 + i * 0.06;
            var moonSpeed = 0.55 / (1 + i * 0.3);
            var angle = (i / moonCount) * Math.PI * 2;

            var mesh = createPlanetMesh(i + 5, moonSize);
            mesh.position.set(
                parentPos.x + Math.cos(angle) * moonRadius,
                parentPos.y,
                parentPos.z + Math.sin(angle) * moonRadius
            );
            scene.add(mesh);

            // Dashed moon orbit
            var ring = makeDashedRing(moonRadius, 0x8855CC, 0.12);
            ring.position.copy(parentPos);
            ring.rotation.x = MOON_TILT;
            scene.add(ring);
            moonOrbitLines.push(ring);

            moonMeshes.push({
                mesh: mesh, label: el,
                parentPos: parentPos.clone(),
                orbitRadius: moonRadius,
                orbitSpeed: moonSpeed,
                angle: angle,
                moonSize: moonSize
            });
        });
    }

    function clearMoons() {
        moonMeshes.forEach(function (m) {
            scene.remove(m.mesh);
            m.label.style.display = 'none';
        });
        moonMeshes = [];
        moonOrbitLines.forEach(function (l) { scene.remove(l); });
        moonOrbitLines = [];
    }

    // ─── ANIMATION ─────────────────────────────────────────────
    function animate() {
        if (!renderer) return;
        requestAnimationFrame(animate);
        var dt = Math.min(clock.getDelta(), 0.05);
        var elapsed = clock.getElapsedTime();

        // ── Camera ─────────────────────────────────────────
        if (viewState === 'zooming_in' || viewState === 'zooming_out') {
            zoomTimer += dt;
            var t = Math.min(1, zoomTimer / ZOOM_DUR);
            var e = easeInOut(t);

            camera.position.set(
                lerp(zoomStartPos.x, zoomEndPos.x, e),
                lerp(zoomStartPos.y, zoomEndPos.y, e),
                lerp(zoomStartPos.z, zoomEndPos.z, e)
            );
            camera.lookAt(
                lerp(zoomStartLook.x, zoomEndLook.x, e),
                lerp(zoomStartLook.y, zoomEndLook.y, e),
                lerp(zoomStartLook.z, zoomEndLook.z, e)
            );

            // Fade non-zoomed planets during zoom-in
            if (viewState === 'zooming_in') {
                var fadeT = Math.min(1, zoomTimer / (ZOOM_DUR * 0.6));
                planetMeshes.forEach(function (p, i) {
                    if (i !== zoomedIdx) {
                        p.label.style.opacity = (1 - fadeT).toFixed(3);
                        p.mesh.material.transparent = true;
                        p.mesh.material.opacity = 1 - fadeT;
                    }
                });
                orbitLines.forEach(function (l) {
                    l.material.opacity = 0.15 * (1 - fadeT);
                });
            }
            // Fade in planets during zoom-out
            if (viewState === 'zooming_out') {
                var fadeT = Math.min(1, zoomTimer / ZOOM_DUR);
                planetMeshes.forEach(function (p, i) {
                    if (i !== zoomedIdx) {
                        p.label.style.display = 'flex';
                        p.mesh.visible = true;
                        p.label.style.opacity = fadeT.toFixed(3);
                        p.mesh.material.transparent = true;
                        p.mesh.material.opacity = fadeT;
                    }
                });
                orbitLines.forEach(function (l) {
                    l.visible = true;
                    l.material.opacity = 0.15 * fadeT;
                });
            }

            if (t >= 1) {
                if (viewState === 'zooming_in') completeZoomIn();
                else completeZoomOut();
            }

        } else if (viewState === 'solar') {
            cameraAngle += dt * 0.012;
            camera.position.x = Math.sin(cameraAngle) * CAM_R;
            camera.position.z = Math.cos(cameraAngle) * CAM_R;
            camera.position.y = CAM_H + Math.sin(elapsed * 0.03) * 0.3;
            camera.lookAt(0, 0, 0);

        } else if (viewState === 'moons') {
            camera.position.set(
                zoomEndPos.x + Math.sin(elapsed * 0.06) * 0.3,
                zoomEndPos.y + Math.sin(elapsed * 0.04) * 0.2,
                zoomEndPos.z + Math.cos(elapsed * 0.05) * 0.2
            );
            camera.lookAt(zoomEndLook.x, zoomEndLook.y, zoomEndLook.z);
        }

        // ── Stars (5x faster) ──────────────────────────────
        if (starField) starField.rotation.y = elapsed * 0.015;

        // ── Spring constants ────────────────────────────────
        var K = 3.5, D = 3.8;
        var W = window.innerWidth, H = window.innerHeight;

        // ── Planets ─────────────────────────────────────────
        for (var i = planetMeshes.length - 1; i >= 0; i--) {
            var p = planetMeshes[i];

            if (p.leaving) {
                p.flyProgress += dt * 2.5;
                var acc = 1 + p.flyProgress * 5;
                p.mesh.position.add(p.flyDir.clone().multiplyScalar(dt * 20 * acc));
                p.mesh.rotation.x += dt * 5;
                p.label.style.opacity = Math.max(0, 1 - p.flyProgress * 3).toFixed(3);
                if (p.flyProgress > 0.7) {
                    p.label.style.display = 'none';
                    scene.remove(p.mesh);
                    planetMeshes.splice(i, 1);
                }
                continue;
            }

            // Orbital motion (only in solar)
            if (viewState === 'solar') {
                p.angle += p.orbitSpeed * dt;
            }

            var tgt = orbitalPos(p.angle, p.orbitRadius, p.inclination);

            if (p.entering) {
                var rx = spr(p.px, tgt.x, p.vx, K, D, dt);
                var ry = spr(p.py, tgt.y, p.vy, K, D, dt);
                var rz = spr(p.pz, tgt.z, p.vz, K, D, dt);
                var rs = spr(p.sc, 1, p.vsc, K, D, dt);
                p.px = rx.v; p.vx = rx.vel;
                p.py = ry.v; p.vy = ry.vel;
                p.pz = rz.v; p.vz = rz.vel;
                p.sc = rs.v; p.vsc = rs.vel;
                p.mesh.position.set(p.px, p.py, p.pz);
                var s = Math.max(0.01, p.sc);
                p.mesh.scale.set(s, s, s);
                p.mesh.rotation.y += dt * 2;
                if (Math.abs(p.px-tgt.x)<0.05 && Math.abs(p.py-tgt.y)<0.05 &&
                    Math.abs(p.pz-tgt.z)<0.05 && Math.abs(p.sc-1)<0.02) {
                    p.entering = false;
                    p.mesh.scale.set(1, 1, 1);
                }
            } else {
                p.mesh.position.set(tgt.x, tgt.y, tgt.z);
                p.mesh.rotation.y += dt * p.spinSpeed;
            }

            // Emissive based on distance from center
            var dfc = p.mesh.position.length();
            p.mesh.material.emissiveIntensity = Math.max(0.1, Math.min(0.4, dfc / 20));

            // ── Label positioning ───────────────────────────
            if (p.label.style.display === 'none') continue;

            var sp = proj(p.mesh.position);
            var appR = apparentSize(p.planetSize, p.mesh.position);

            // Dynamic label size — scales with apparent planet size
            var diam = Math.max(75, Math.min(220, appR * 2.8));
            p.label.style.width = diam + 'px';
            p.label.style.height = diam + 'px';

            // Scale text with planet apparent size
            if (!p.label.classList.contains('icon-planet')) {
                var fs = Math.max(9, Math.min(16, diam * 0.12));
                p.label.style.fontSize = fs + 'px';
                p.label.style.letterSpacing = Math.max(1, Math.min(3, diam * 0.025)) + 'px';
            }
            // Scale SVG icons
            var svg = p.label.querySelector('svg');
            if (svg) {
                var svgSz = Math.round(diam * 0.4);
                svg.setAttribute('width', svgSz);
                svg.setAttribute('height', svgSz);
            }

            // HARD CLAMP: never go off screen
            var pad = diam / 2 + 5;
            sp.x = Math.max(pad, Math.min(W - pad, sp.x));
            sp.y = Math.max(pad, Math.min(H - pad, sp.y));

            var ew = p.label.offsetWidth, eh = p.label.offsetHeight;
            p.label.style.left = (sp.x - ew / 2) + 'px';
            p.label.style.top = (sp.y - eh / 2) + 'px';

            // Depth-based subtle opacity
            var dist = p.mesh.position.distanceTo(camera.position);
            var camDist = camera.position.length();
            var maxD = camDist + getSafeMaxR();
            var minD = Math.max(1, camDist - getSafeMaxR());
            var dt2 = (dist - minD) / (maxD - minD);
            dt2 = Math.max(0, Math.min(1, dt2));
            if (viewState === 'solar') {
                p.label.style.opacity = (1.0 - dt2 * 0.25).toFixed(3);
            }

            p.label.style.transformOrigin = 'center center';
            p.label.style.zIndex = dist > camDist + 0.5 ? '3' : '15';
        }

        // ── Moons ───────────────────────────────────────────
        for (var j = 0; j < moonMeshes.length; j++) {
            var m = moonMeshes[j];
            m.angle += m.orbitSpeed * dt;

            var dx = Math.cos(m.angle) * m.orbitRadius;
            var dy = -Math.sin(m.angle) * m.orbitRadius * Math.sin(MOON_TILT);
            var dz = Math.sin(m.angle) * m.orbitRadius * Math.cos(MOON_TILT);
            m.mesh.position.set(m.parentPos.x + dx, m.parentPos.y + dy, m.parentPos.z + dz);
            m.mesh.rotation.y += dt * 0.5;

            var msp = proj(m.mesh.position);
            var mappR = apparentSize(m.moonSize, m.mesh.position);
            var mdiam = Math.max(60, Math.min(180, mappR * 3.0));
            m.label.style.width = mdiam + 'px';
            m.label.style.height = mdiam + 'px';

            if (!m.label.classList.contains('icon-planet')) {
                var mfs = Math.max(9, Math.min(14, mdiam * 0.13));
                m.label.style.fontSize = mfs + 'px';
                m.label.style.letterSpacing = Math.max(1, Math.min(3, mdiam * 0.025)) + 'px';
            }
            var msvg = m.label.querySelector('svg');
            if (msvg) {
                var ms = Math.round(mdiam * 0.4);
                msvg.setAttribute('width', ms);
                msvg.setAttribute('height', ms);
            }

            // Clamp moons to screen
            var mpad = mdiam / 2 + 5;
            msp.x = Math.max(mpad, Math.min(W - mpad, msp.x));
            msp.y = Math.max(mpad, Math.min(H - mpad, msp.y));

            m.label.style.left = (msp.x - m.label.offsetWidth / 2) + 'px';
            m.label.style.top = (msp.y - m.label.offsetHeight / 2) + 'px';
            m.label.style.opacity = '1';
            m.label.style.zIndex = '15';
        }

        renderer.render(scene, camera);
    }

    // ─── ASTEROIDS ─────────────────────────────────────────────
    function spawnAsteroids3D() {
        var arr = [];
        function spawn() {
            var sz = 0.03 + Math.random() * 0.08;
            var m = new THREE.Mesh(
                new THREE.DodecahedronGeometry(sz, 0),
                new THREE.MeshPhongMaterial({
                    color: Math.random() < 0.15 ? 0x00C8DC : 0x8C64C8,
                    emissive: Math.random() < 0.15 ? 0x004455 : 0x2a1a44,
                    emissiveIntensity: 0.4, transparent: true, opacity: 0.3
                })
            );
            var a = Math.random()*Math.PI*2, r = 22+Math.random()*15;
            m.position.set(Math.cos(a)*r, (Math.random()-0.5)*6, Math.sin(a)*r);
            var d = new THREE.Vector3(-m.position.x,(Math.random()-0.5)*2,-m.position.z).normalize();
            scene.add(m);
            arr.push({m:m,d:d,s:0.3+Math.random()*1,rs:new THREE.Vector3((Math.random()-0.5)*2,(Math.random()-0.5)*2,(Math.random()-0.5)*2)});
        }
        (function loop(){setTimeout(function(){if(arr.length<8)spawn();loop();},3000+Math.random()*4000);})();
        for(var k=0;k<3;k++) spawn();
        (function tick(){
            for(var j=arr.length-1;j>=0;j--){
                var a=arr[j];
                a.m.position.add(a.d.clone().multiplyScalar(a.s*0.016));
                a.m.rotation.x+=a.rs.x*0.016;a.m.rotation.y+=a.rs.y*0.016;
                if(a.m.position.length()>50){scene.remove(a.m);arr.splice(j,1);}
            }
            requestAnimationFrame(tick);
        })();
    }

    // ─── FLOATING WORDS ────────────────────────────────────────
    function spawnFloatingWords() {
        var w = [];
        WORDS.forEach(function (word) {
            var el = document.createElement('span');
            el.className = 'floating-word'; el.textContent = word;
            document.body.appendChild(el);
            w.push({el:el,x:Math.random()*window.innerWidth,y:Math.random()*window.innerHeight,
                vx:(Math.random()-0.5)*0.25,vy:(Math.random()-0.5)*0.25,op:0.08+Math.random()*0.12});
        });
        (function tick(){
            var W=window.innerWidth,H=window.innerHeight;
            w.forEach(function(o){
                o.x+=o.vx;o.y+=o.vy;
                if(o.x<-80)o.x=W+20;if(o.x>W+80)o.x=-20;
                if(o.y<-20)o.y=H+20;if(o.y>H+20)o.y=-20;
                o.el.style.left=o.x+'px';o.el.style.top=o.y+'px';o.el.style.opacity=o.op;
            });
            requestAnimationFrame(tick);
        })();
    }

    // ─── BIG BANG ──────────────────────────────────────────────
    function doBigBang() {
        sessionStorage.setItem('bigbang', '1');
        transitionOverlay.style.transition = 'none';
        transitionOverlay.style.opacity = '1';
        document.querySelectorAll('[data-orbit="planet"]').forEach(function(el){ el.style.display='none'; });

        var tex = circTex(), n = 600;
        var geom = new THREE.BufferGeometry();
        var pos = new Float32Array(n*3), col = new Float32Array(n*3), vels = [];
        for(var i=0;i<n;i++){
            pos[i*3]=pos[i*3+1]=pos[i*3+2]=0;
            var roll=Math.random();
            if(roll<0.12){col[i*3]=0;col[i*3+1]=0.94;col[i*3+2]=1;}
            else if(roll<0.5){col[i*3]=0.66;col[i*3+1]=0.33;col[i*3+2]=0.97;}
            else{col[i*3]=0.75;col[i*3+1]=0.52;col[i*3+2]=0.99;}
            var th=Math.random()*Math.PI*2,ph=Math.acos(2*Math.random()-1),sp=2+Math.random()*18;
            vels.push({x:Math.sin(ph)*Math.cos(th)*sp,y:Math.sin(ph)*Math.sin(th)*sp,z:Math.cos(ph)*sp,life:1});
        }
        geom.setAttribute('position',new THREE.BufferAttribute(pos,3));
        geom.setAttribute('color',new THREE.BufferAttribute(col,3));
        var mat=new THREE.PointsMaterial({size:0.2,map:tex,vertexColors:true,transparent:true,opacity:1,blending:THREE.AdditiveBlending,depthWrite:false,sizeAttenuation:true});
        var pts=new THREE.Points(geom,mat);scene.add(pts);

        var seedM=new THREE.Mesh(new THREE.SphereGeometry(0.1,16,16),new THREE.MeshBasicMaterial({color:0xA855F7,transparent:true,opacity:1}));
        scene.add(seedM);
        var seedL=new THREE.PointLight(0xA855F7,5,30);scene.add(seedL);

        var fr=0,SF=40,expl=false,oFade=false,pDone=false,eDone=false;
        (function tick(){
            fr++;
            if(!expl&&fr<SF){var pr=fr/SF;seedM.scale.setScalar(1+pr*3);seedL.intensity=5+pr*20;}
            if(!expl&&fr>=SF){expl=true;scene.remove(seedM);seedL.intensity=30;}
            if(expl){
                var pa=geom.getAttribute('position');
                for(var j=0;j<n;j++){
                    var v=vels[j];v.life-=0.008;if(v.life<=0)continue;
                    pa.array[j*3]+=v.x*0.016;pa.array[j*3+1]+=v.y*0.016;pa.array[j*3+2]+=v.z*0.016;
                    v.x*=0.985;v.y*=0.985;v.z*=0.985;
                }
                pa.needsUpdate=true;mat.opacity=Math.max(0,vels[0].life);
                if(seedL.intensity>0)seedL.intensity*=0.97;
                if(mat.opacity<=0.01){scene.remove(pts);scene.remove(seedL);}
            }
            if(!oFade&&fr>=SF+8){oFade=true;transitionOverlay.style.transition='opacity 1.5s ease';transitionOverlay.style.opacity='0';}
            if(!pDone&&fr>=SF+50){pDone=true;buildOrbit(true);}
            if(!eDone&&fr>=SF+80){eDone=true;spawnAsteroids3D();if(document.querySelector('.icon-planet'))spawnFloatingWords();}
            if(!eDone||(expl&&mat.opacity>0.01))requestAnimationFrame(tick);
        })();
    }

    // ─── NAV ───────────────────────────────────────────────────
    function setupNavigation() {
        transitionOverlay = document.createElement('div');
        transitionOverlay.style.cssText = 'position:fixed;top:0;left:0;width:100vw;height:100vh;background:'+BG+';z-index:9999;pointer-events:none;opacity:0;transition:opacity 0.35s ease;';
        document.body.appendChild(transitionOverlay);

        var fn = sessionStorage.getItem('space-nav');
        if(fn){
            transitionOverlay.style.opacity='1';transitionOverlay.style.transition='none';
            requestAnimationFrame(function(){requestAnimationFrame(function(){
                transitionOverlay.style.transition='opacity 0.4s ease';transitionOverlay.style.opacity='0';
            });});
            sessionStorage.removeItem('space-nav');
        }

        document.addEventListener('click', function(e){
            var link = e.target.closest('a[href], a[data-nav]');
            if (!link || link.target === '_blank') return;

            var nav = link.getAttribute('data-nav');

            // Home button — zoom out if in moon view
            if (nav === 'home') {
                e.preventDefault();
                if (viewState === 'moons') {
                    startZoomOut();
                }
                // In solar view, home does nothing (already home)
                return;
            }

            // Nav planet — zoom in
            if (nav && viewState === 'solar') {
                e.preventDefault();
                for (var i = 0; i < planetMeshes.length; i++) {
                    if (planetMeshes[i].label === link) {
                        startZoomIn(i);
                        break;
                    }
                }
                return;
            }

            // Regular navigation (moon links, portfolio, etc.)
            var href = link.getAttribute('href');
            if (!href || href.startsWith('http') || href.startsWith('#')) return;
            e.preventDefault();
            sessionStorage.setItem('space-nav', '1');
            transitionOverlay.style.opacity = '1';
            setTimeout(function(){ window.location.href = href; }, 350);
        });
    }

    // ─── INIT ──────────────────────────────────────────────────
    window.addEventListener('load', function(){
        isOrbitPage = document.querySelectorAll('[data-orbit="planet"]').length > 0;
        if(isOrbitPage){
            document.querySelectorAll('[data-orbit="planet"]').forEach(function(el){
                var view = el.getAttribute('data-view');
                if (view !== 'home' && view !== 'always') el.style.display = 'none';
            });
            createBlackHoleOverlay();
        }
        setupNavigation();
        initThree();
        animate();
        if(isOrbitPage && !sessionStorage.getItem('bigbang')){
            doBigBang();
        } else {
            if (typeof THREE !== 'undefined' && scene) spawnAsteroids3D();
            if(isOrbitPage){ if(document.querySelector('.icon-planet')) spawnFloatingWords(); buildOrbit(false); }
        }
    });

    window.addEventListener('resize', function(){
        if(!camera||!renderer) return;
        camera.aspect = window.innerWidth / window.innerHeight;
        camera.updateProjectionMatrix();
        renderer.setSize(window.innerWidth, window.innerHeight);
        updateBlackHoleOverlay();

        // Recompute safe orbit radius
        _safeMaxR = null;

        if(isOrbitPage && viewState === 'solar' && planetMeshes.length > 0) {
            buildOrbit(false);
        }
    });
})();
