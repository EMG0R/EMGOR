(function () {
    'use strict';

    var WORDS = ['sound', 'signal', 'rhythm', 'noise', 'pulse', 'wave', 'flux', 'drift', 'echo', 'void'];
    var currentView = 'home';
    var isOrbitPage = false;
    var transitionOverlay = null;
    var BG = '#0D0221';

    var scene, camera, renderer, clock;
    var planetMeshes = [];
    var starField;
    var orbitLines = [];
    var cameraAngle = 0;

    var cameraRadius = 30;
    var cameraFOV = 55;
    var cameraHeight = 12;

    // Deep vivid jewel tones — saturated but dark
    var PLANET_COLORS = [
        { base: 0x0C5C54, emissive: 0x062E2A },  // deep jade
        { base: 0x8C2840, emissive: 0x461420 },  // crimson rose
        { base: 0x15406E, emissive: 0x0A2037 },  // midnight blue
        { base: 0x8E5010, emissive: 0x472808 },  // burnt sienna
        { base: 0x3E2888, emissive: 0x1F1444 },  // royal indigo
        { base: 0x106848, emissive: 0x083424 },  // forest emerald
        { base: 0x6E3080, emissive: 0x371840 },  // deep plum
        { base: 0x882818, emissive: 0x44140C },  // oxblood
        { base: 0x0E5868, emissive: 0x072C34 },  // deep teal
        { base: 0x685010, emissive: 0x342808 },  // dark bronze
        { base: 0x283868, emissive: 0x141C34 },  // navy steel
        { base: 0x6E3848, emissive: 0x371C24 },  // wine
    ];

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
        ctx.fillStyle = g;
        ctx.fillRect(0, 0, 64, 64);
        _circTex = new THREE.CanvasTexture(c);
        return _circTex;
    }

    // ─── THREE.JS SETUP ────────────────────────────────────────
    function initThree() {
        var canvas = document.getElementById('spaceCanvas');
        if (!canvas || typeof THREE === 'undefined') return;

        scene = new THREE.Scene();
        clock = new THREE.Clock();

        camera = new THREE.PerspectiveCamera(cameraFOV, window.innerWidth / window.innerHeight, 0.1, 1000);
        camera.position.set(0, cameraHeight, cameraRadius);
        camera.lookAt(0, 0, 0);

        renderer = new THREE.WebGLRenderer({ canvas: canvas, alpha: true, antialias: true });
        renderer.setSize(window.innerWidth, window.innerHeight);
        renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
        renderer.setClearColor(0x000000, 0);

        // Soft ambient fill
        scene.add(new THREE.AmbientLight(0x443355, 0.5));

        // Hemisphere for general fill
        scene.add(new THREE.HemisphereLight(0x554466, 0x110822, 0.4));

        // Gentle directional from above-right
        var dl1 = new THREE.DirectionalLight(0x887799, 0.3);
        dl1.position.set(10, 8, 12);
        scene.add(dl1);

        // Fill from left
        var dl2 = new THREE.DirectionalLight(0x665577, 0.2);
        dl2.position.set(-10, 4, -8);
        scene.add(dl2);

        // Faint accretion glow at center
        var accretion = new THREE.PointLight(0x5511AA, 0.5, 18);
        accretion.position.set(0, 0.3, 0);
        scene.add(accretion);

        // Black hole occluder — matches the shader's dark event horizon
        var bhOccluder = new THREE.Mesh(
            new THREE.SphereGeometry(2.0, 32, 32),
            new THREE.MeshBasicMaterial({ color: 0x0D0221 })
        );
        scene.add(bhOccluder);

        createStarField();
    }

    // ─── STARS ─────────────────────────────────────────────────
    function createStarField() {
        var tex = circTex();
        var n = 3500;
        var geom = new THREE.BufferGeometry();
        var pos = new Float32Array(n * 3);
        var col = new Float32Array(n * 3);

        for (var i = 0; i < n; i++) {
            var th = Math.random() * Math.PI * 2;
            var ph = Math.acos(2 * Math.random() - 1);
            var r = 60 + Math.random() * 180;
            pos[i*3] = r * Math.sin(ph) * Math.cos(th);
            pos[i*3+1] = r * Math.sin(ph) * Math.sin(th);
            pos[i*3+2] = r * Math.cos(ph);

            var roll = Math.random();
            if (roll < 0.08) {
                col[i*3] = 0.3; col[i*3+1] = 0.9; col[i*3+2] = 1.0;
            } else if (roll < 0.25) {
                col[i*3] = 0.7; col[i*3+1] = 0.5; col[i*3+2] = 1.0;
            } else {
                var b = 0.7 + Math.random() * 0.3;
                col[i*3] = b; col[i*3+1] = b; col[i*3+2] = b;
            }
        }
        geom.setAttribute('position', new THREE.BufferAttribute(pos, 3));
        geom.setAttribute('color', new THREE.BufferAttribute(col, 3));

        starField = new THREE.Points(geom, new THREE.PointsMaterial({
            size: 0.6, map: tex, vertexColors: true, transparent: true,
            opacity: 0.9, sizeAttenuation: true, blending: THREE.AdditiveBlending,
            depthWrite: false
        }));
        scene.add(starField);
    }

    // ─── ORBIT RINGS ───────────────────────────────────────────
    function makeRing(radius, inclination) {
        var seg = 128, geom = new THREE.BufferGeometry();
        var pos = new Float32Array(seg * 3);
        for (var i = 0; i < seg; i++) {
            var a = (i / seg) * Math.PI * 2;
            pos[i*3] = Math.cos(a) * radius;
            pos[i*3+1] = Math.sin(a) * radius * Math.sin(inclination) * 0.06;
            pos[i*3+2] = Math.sin(a) * radius;
        }
        geom.setAttribute('position', new THREE.BufferAttribute(pos, 3));
        return new THREE.LineLoop(geom, new THREE.LineBasicMaterial({
            color: 0xA855F7, transparent: true, opacity: 0.035,
            blending: THREE.AdditiveBlending
        }));
    }

    function rebuildRings() {
        orbitLines.forEach(function (l) { scene.remove(l); });
        orbitLines = [];
        planetMeshes.forEach(function (p) {
            if (p.leaving) return;
            var ring = makeRing(p.orbitRadius, p.inclination);
            scene.add(ring);
            orbitLines.push(ring);
        });
    }

    // ─── PLANET MESH ───────────────────────────────────────────
    function createPlanetMesh(colorIndex, size) {
        var c = PLANET_COLORS[colorIndex % PLANET_COLORS.length];
        var mesh = new THREE.Mesh(
            new THREE.SphereGeometry(size, 48, 48),
            new THREE.MeshPhongMaterial({
                color: c.base,
                emissive: c.emissive,
                emissiveIntensity: 0.3,
                shininess: 20,
                specular: 0x222222
            })
        );
        return mesh;
    }

    // ─── ORBIT SIZING — guaranteed to fit screen ───────────────
    function computeMaxVisibleRadius() {
        var fovRad = (cameraFOV / 2) * Math.PI / 180;
        var dist = Math.sqrt(cameraRadius * cameraRadius + cameraHeight * cameraHeight);
        var visibleHeight = Math.tan(fovRad) * dist;
        var visibleWidth = visibleHeight * (window.innerWidth / window.innerHeight);
        return Math.min(visibleWidth, visibleHeight) * 0.80;
    }

    function getOrbitRadius(index, total) {
        var maxR = computeMaxVisibleRadius();
        var minR = maxR * 0.22;
        if (total <= 1) return (minR + maxR) / 2;
        return minR + (maxR - minR) * (index / (total - 1));
    }

    function getOrbitSpeed(index) {
        return 0.15 / (1 + index * 0.35);
    }

    function getInclination(index) {
        // Subtle — thin galactic disc
        var vals = [0.08, -0.12, 0.1, -0.07, 0.11, -0.06, 0.09, -0.1, 0.07, -0.08, 0.1, -0.09];
        return vals[index % vals.length];
    }

    function getPlanetSize(el) {
        if (el.classList.contains('icon-planet')) return 0.65;
        return 0.85;
    }

    // ─── SPRING ────────────────────────────────────────────────
    function spr(cur, tgt, vel, k, d, dt) {
        var f = (tgt - cur) * k - vel * d;
        vel += f * dt;
        cur += vel * dt;
        return { v: cur, vel: vel };
    }

    // ─── ORBITAL POSITION ──────────────────────────────────────
    function orbitalPos(angle, radius, inclination) {
        return {
            x: Math.cos(angle) * radius,
            y: Math.sin(angle) * radius * Math.sin(inclination) * 0.06,
            z: Math.sin(angle) * radius
        };
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

        visible.forEach(function (el, i) {
            el.style.position = 'fixed';
            el.style.display = 'flex';

            var radius = getOrbitRadius(i, total);
            var speed = getOrbitSpeed(i);
            var angle = (i / total) * Math.PI * 2;
            var incl = getInclination(i);
            var size = getPlanetSize(el);

            var mesh = createPlanetMesh(i, size);
            scene.add(mesh);

            var tgt = orbitalPos(angle, radius, incl);

            var p = {
                mesh: mesh,
                label: el,
                orbitRadius: radius,
                orbitSpeed: speed,
                angle: angle,
                inclination: incl,
                planetSize: size,
                view: el.getAttribute('data-view'),
                entering: false,
                leaving: false,
                flyDir: new THREE.Vector3(),
                flyProgress: 0,
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
            return view === currentView || view === 'always';
        });
    }

    // ─── PROJECT 3D → SCREEN ───────────────────────────────────
    function proj(pos) {
        var v = pos.clone().project(camera);
        return {
            x: (v.x * 0.5 + 0.5) * window.innerWidth,
            y: (-v.y * 0.5 + 0.5) * window.innerHeight
        };
    }

    // ─── COMPUTE APPARENT SIZE ON SCREEN ───────────────────────
    function apparentSize(worldRadius, worldPos) {
        var dist = worldPos.distanceTo(camera.position);
        if (dist < 0.1) dist = 0.1;
        var fovRad = (cameraFOV / 2) * Math.PI / 180;
        var screenHeight = window.innerHeight;
        var projectedSize = (worldRadius / (dist * Math.tan(fovRad))) * (screenHeight / 2);
        return projectedSize;
    }

    // ─── ANIMATION ─────────────────────────────────────────────
    function animate() {
        if (!renderer) return;
        requestAnimationFrame(animate);
        var dt = Math.min(clock.getDelta(), 0.05);
        var elapsed = clock.getElapsedTime();

        // Slow, majestic camera drift
        cameraAngle += dt * 0.02;

        camera.position.x = Math.sin(cameraAngle) * cameraRadius;
        camera.position.z = Math.cos(cameraAngle) * cameraRadius;
        camera.position.y = cameraHeight + Math.sin(elapsed * 0.04) * 0.5;
        camera.lookAt(0, 0, 0);

        if (starField) {
            starField.rotation.y = elapsed * 0.003;
        }

        var K = 3.5, D = 3.8;
        var camDist = camera.position.length();

        for (var i = planetMeshes.length - 1; i >= 0; i--) {
            var p = planetMeshes[i];

            if (p.leaving) {
                p.flyProgress += dt * 2.5;
                var acc = 1 + p.flyProgress * 5;
                p.mesh.position.add(p.flyDir.clone().multiplyScalar(dt * 20 * acc));
                p.mesh.rotation.x += dt * 5;

                var sp2 = proj(p.mesh.position);
                var appSz = apparentSize(p.planetSize, p.mesh.position);
                var flyScale = Math.max(0.1, appSz / 60);
                p.label.style.left = (sp2.x - p.label.offsetWidth / 2) + 'px';
                p.label.style.top = (sp2.y - p.label.offsetHeight / 2) + 'px';
                p.label.style.transform = 'scale(' + flyScale.toFixed(3) + ')';
                p.label.style.opacity = Math.max(0, 1 - p.flyProgress * 3).toFixed(3);

                if (p.flyProgress > 0.7) {
                    p.label.style.display = 'none';
                    scene.remove(p.mesh);
                    planetMeshes.splice(i, 1);
                }
                continue;
            }

            // Orbital target
            p.angle += p.orbitSpeed * dt;
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

                if (Math.abs(p.px - tgt.x) < 0.05 &&
                    Math.abs(p.py - tgt.y) < 0.05 &&
                    Math.abs(p.pz - tgt.z) < 0.05 &&
                    Math.abs(p.sc - 1) < 0.02) {
                    p.entering = false;
                    p.mesh.scale.set(1, 1, 1);
                }
            } else {
                p.mesh.position.set(tgt.x, tgt.y, tgt.z);
                p.mesh.rotation.y += dt * p.spinSpeed;
            }

            // Emissive intensity based on distance from center
            var distFromCenter = p.mesh.position.length();
            var darkenFactor = Math.max(0.1, Math.min(0.4, distFromCenter / 20));
            p.mesh.material.emissiveIntensity = darkenFactor;

            // ─── LABEL POSITIONING (FIXED) ─────────────────────
            var sp = proj(p.mesh.position);

            // Depth-based subtle scale and opacity
            var dist = p.mesh.position.distanceTo(camera.position);
            var maxDist = camDist + computeMaxVisibleRadius();
            var minDist = Math.max(1, camDist - computeMaxVisibleRadius());
            var t = (dist - minDist) / (maxDist - minDist);
            t = Math.max(0, Math.min(1, t));
            var labelScale = 1.0 - t * 0.2;   // 1.0 near → 0.8 far
            var labelOpacity = 1.0 - t * 0.3;  // 1.0 near → 0.7 far

            var ew = p.label.offsetWidth;
            var eh = p.label.offsetHeight;
            // Center label on projected planet position
            p.label.style.left = (sp.x - ew / 2) + 'px';
            p.label.style.top = (sp.y - eh / 2) + 'px';
            p.label.style.transform = 'scale(' + labelScale.toFixed(3) + ')';
            p.label.style.transformOrigin = 'center center';
            p.label.style.opacity = labelOpacity.toFixed(3);

            // Behind the black hole? Lower z-index so overlay occludes label
            var isBehind = dist > camDist + 0.5;
            p.label.style.zIndex = isBehind ? '3' : '15';
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
                var pos = p.mesh.position.clone();
                if (pos.length() < 0.1) pos.set(1, 0, 0);
                var dir = pos.clone().normalize();
                dir.y += (Math.random() - 0.5) * 0.5;
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
            var aC = alwaysPlanets.length;
            var totalNew = aC + newEls.length;

            newEls.forEach(function (el, i) {
                el.style.display = 'flex';
                el.style.position = 'fixed';

                var idx = aC + i;
                var radius = getOrbitRadius(idx, totalNew);
                var speed = getOrbitSpeed(idx);
                var angle = (idx / totalNew) * Math.PI * 2;
                var incl = getInclination(idx);
                var size = getPlanetSize(el);

                var mesh = createPlanetMesh(i + aC, size);

                var sA = Math.random() * Math.PI * 2;
                var sR = computeMaxVisibleRadius() * 2.5;
                var startPos = new THREE.Vector3(
                    Math.cos(sA) * sR,
                    (Math.random() - 0.5) * 3,
                    Math.sin(sA) * sR
                );
                mesh.position.copy(startPos);
                mesh.scale.set(0.2, 0.2, 0.2);
                scene.add(mesh);

                planetMeshes.push({
                    mesh: mesh, label: el,
                    orbitRadius: radius, orbitSpeed: speed,
                    angle: angle, inclination: incl,
                    planetSize: size,
                    view: el.getAttribute('data-view'),
                    entering: true, leaving: false,
                    flyDir: new THREE.Vector3(), flyProgress: 0,
                    px: startPos.x, py: startPos.y, pz: startPos.z,
                    vx: 0, vy: 0, vz: 0,
                    sc: 0.2, vsc: 0,
                    spinSpeed: 0.3 + Math.random() * 0.8
                });
            });

            alwaysPlanets.forEach(function (p, i) {
                p.orbitRadius = getOrbitRadius(i, totalNew);
            });

            rebuildRings();
        }, 350);
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
            var a = Math.random() * Math.PI * 2, r = 22 + Math.random() * 15;
            m.position.set(Math.cos(a)*r, (Math.random()-0.5)*6, Math.sin(a)*r);
            var d = new THREE.Vector3(-m.position.x, (Math.random()-0.5)*2, -m.position.z).normalize();
            scene.add(m);
            arr.push({ m:m, d:d, s:0.3+Math.random()*1, rs:new THREE.Vector3((Math.random()-0.5)*2,(Math.random()-0.5)*2,(Math.random()-0.5)*2) });
        }
        (function loop(){ setTimeout(function(){ if(arr.length<8) spawn(); loop(); }, 3000+Math.random()*4000); })();
        for(var k=0;k<3;k++) spawn();
        (function tick(){
            for(var j=arr.length-1;j>=0;j--){
                var a=arr[j];
                a.m.position.add(a.d.clone().multiplyScalar(a.s*0.016));
                a.m.rotation.x+=a.rs.x*0.016; a.m.rotation.y+=a.rs.y*0.016;
                if(a.m.position.length()>50){ scene.remove(a.m); arr.splice(j,1); }
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
            w.push({ el:el, x:Math.random()*window.innerWidth, y:Math.random()*window.innerHeight,
                vx:(Math.random()-0.5)*0.25, vy:(Math.random()-0.5)*0.25, op:0.08+Math.random()*0.12 });
        });
        (function tick(){
            var W=window.innerWidth, H=window.innerHeight;
            w.forEach(function(o){
                o.x+=o.vx; o.y+=o.vy;
                if(o.x<-80) o.x=W+20; if(o.x>W+80) o.x=-20;
                if(o.y<-20) o.y=H+20; if(o.y>H+20) o.y=-20;
                o.el.style.left=o.x+'px'; o.el.style.top=o.y+'px'; o.el.style.opacity=o.op;
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
            var th=Math.random()*Math.PI*2, ph=Math.acos(2*Math.random()-1), sp=2+Math.random()*18;
            vels.push({x:Math.sin(ph)*Math.cos(th)*sp,y:Math.sin(ph)*Math.sin(th)*sp,z:Math.cos(ph)*sp,life:1});
        }
        geom.setAttribute('position',new THREE.BufferAttribute(pos,3));
        geom.setAttribute('color',new THREE.BufferAttribute(col,3));
        var mat=new THREE.PointsMaterial({size:0.2,map:tex,vertexColors:true,transparent:true,opacity:1,blending:THREE.AdditiveBlending,depthWrite:false,sizeAttenuation:true});
        var pts=new THREE.Points(geom,mat); scene.add(pts);

        var seedM=new THREE.Mesh(new THREE.SphereGeometry(0.1,16,16),new THREE.MeshBasicMaterial({color:0xA855F7,transparent:true,opacity:1}));
        scene.add(seedM);
        var seedL=new THREE.PointLight(0xA855F7,5,30); scene.add(seedL);

        var fr=0,SF=40,expl=false,oFade=false,pDone=false,eDone=false;
        (function tick(){
            fr++;
            if(!expl&&fr<SF){ var pr=fr/SF; seedM.scale.setScalar(1+pr*3); seedL.intensity=5+pr*20; }
            if(!expl&&fr>=SF){ expl=true; scene.remove(seedM); seedL.intensity=30; }
            if(expl){
                var pa=geom.getAttribute('position');
                for(var j=0;j<n;j++){
                    var v=vels[j]; v.life-=0.008; if(v.life<=0) continue;
                    pa.array[j*3]+=v.x*0.016; pa.array[j*3+1]+=v.y*0.016; pa.array[j*3+2]+=v.z*0.016;
                    v.x*=0.985; v.y*=0.985; v.z*=0.985;
                }
                pa.needsUpdate=true; mat.opacity=Math.max(0,vels[0].life);
                if(seedL.intensity>0) seedL.intensity*=0.97;
                if(mat.opacity<=0.01){scene.remove(pts);scene.remove(seedL);}
            }
            if(!oFade&&fr>=SF+8){ oFade=true; transitionOverlay.style.transition='opacity 1.5s ease'; transitionOverlay.style.opacity='0'; }
            if(!pDone&&fr>=SF+50){ pDone=true; buildOrbit(true); }
            if(!eDone&&fr>=SF+80){ eDone=true; spawnAsteroids3D(); if(document.querySelector('.icon-planet')) spawnFloatingWords(); }
            if(!eDone||(expl&&mat.opacity>0.01)) requestAnimationFrame(tick);
        })();
    }

    // ─── NAV ───────────────────────────────────────────────────
    function setupNavigation() {
        transitionOverlay = document.createElement('div');
        transitionOverlay.style.cssText = 'position:fixed;top:0;left:0;width:100vw;height:100vh;background:'+BG+';z-index:9999;pointer-events:none;opacity:0;transition:opacity 0.35s ease;';
        document.body.appendChild(transitionOverlay);

        var fn = sessionStorage.getItem('space-nav');
        if(fn){
            transitionOverlay.style.opacity='1'; transitionOverlay.style.transition='none';
            requestAnimationFrame(function(){ requestAnimationFrame(function(){
                transitionOverlay.style.transition='opacity 0.4s ease'; transitionOverlay.style.opacity='0';
            }); });
            sessionStorage.removeItem('space-nav');
        }

        document.addEventListener('click', function(e){
            var link=e.target.closest('a[href], a[data-nav]');
            if(!link||link.target==='_blank') return;
            var nav=link.getAttribute('data-nav');
            if(nav){ e.preventDefault(); switchView(nav); return; }
            var href=link.getAttribute('href');
            if(!href||href.startsWith('http')||href.startsWith('#')) return;
            e.preventDefault();
            sessionStorage.setItem('space-nav','1');
            transitionOverlay.style.opacity='1';
            setTimeout(function(){ window.location.href=href; },350);
        });
    }

    // ─── INIT ──────────────────────────────────────────────────
    window.addEventListener('load', function(){
        isOrbitPage = document.querySelectorAll('[data-orbit="planet"]').length > 0;
        if(isOrbitPage){
            var hash=location.hash.replace('#','');
            currentView=(hash==='synth'||hash==='resources')?hash:'home';
            document.querySelectorAll('[data-orbit="planet"]').forEach(function(el){
                var view=el.getAttribute('data-view');
                if(view!==currentView&&view!=='always') el.style.display='none';
            });
            createBlackHoleOverlay();
        }
        setupNavigation();
        initThree();
        animate();
        if(isOrbitPage&&!sessionStorage.getItem('bigbang')&&currentView==='home'){
            doBigBang();
        } else {
            if (typeof THREE !== 'undefined' && scene) spawnAsteroids3D();
            if(isOrbitPage){ if(document.querySelector('.icon-planet')) spawnFloatingWords(); buildOrbit(false); }
        }
    });

    window.addEventListener('hashchange', function(){
        if(!isOrbitPage) return;
        var h=location.hash.replace('#','');
        switchView((h==='synth'||h==='resources')?h:'home');
    });

    window.addEventListener('resize', function(){
        if(!camera||!renderer) return;
        camera.aspect=window.innerWidth/window.innerHeight;
        camera.updateProjectionMatrix();
        renderer.setSize(window.innerWidth,window.innerHeight);
        updateBlackHoleOverlay();
        if(isOrbitPage && planetMeshes.length > 0) {
            var total = planetMeshes.length;
            planetMeshes.forEach(function(p, i) {
                p.orbitRadius = getOrbitRadius(i, total);
            });
            rebuildRings();
        }
    });
})();
