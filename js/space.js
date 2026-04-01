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
    var cameraTilt = 0.35;
    var cameraRadius = 35;

    // Dark, moody jewel tones — rich but not bright
    var PLANET_COLORS = [
        { base: 0x0E6B62, emissive: 0x053530 },  // dark jade
        { base: 0x8B3A4A, emissive: 0x451D25 },  // dark rose
        { base: 0x1E4A7A, emissive: 0x0F253D },  // deep ocean
        { base: 0x8A5A20, emissive: 0x452D10 },  // dark amber
        { base: 0x4A3A8E, emissive: 0x251D47 },  // deep indigo
        { base: 0x1A7A55, emissive: 0x0D3D2B },  // dark emerald
        { base: 0x7A4A8A, emissive: 0x3D2545 },  // dark orchid
        { base: 0x8A4030, emissive: 0x452018 },  // dark terracotta
        { base: 0x1A6A7A, emissive: 0x0D353D },  // dark teal
        { base: 0x6A5A20, emissive: 0x352D10 },  // dark gold
        { base: 0x3A4A6A, emissive: 0x1D2535 },  // dark steel
        { base: 0x7A4A50, emissive: 0x3D2528 },  // dark mauve
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
        scene = new THREE.Scene();
        clock = new THREE.Clock();

        camera = new THREE.PerspectiveCamera(50, window.innerWidth / window.innerHeight, 0.1, 1000);
        camera.position.set(0, 8, cameraRadius);
        camera.lookAt(0, 0, 0);

        renderer = new THREE.WebGLRenderer({ canvas: canvas, alpha: true, antialias: true });
        renderer.setSize(window.innerWidth, window.innerHeight);
        renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
        renderer.setClearColor(0x000000, 0);

        // Gentle ambient — subtle overall fill
        scene.add(new THREE.AmbientLight(0x443355, 0.6));

        // Soft hemisphere for natural sky/ground
        scene.add(new THREE.HemisphereLight(0x665588, 0x1a0a2e, 0.4));

        // Subtle directional from above-right
        var dl1 = new THREE.DirectionalLight(0x9988AA, 0.35);
        dl1.position.set(8, 6, 10);
        scene.add(dl1);

        // Faint fill from opposite side
        var dl2 = new THREE.DirectionalLight(0x776699, 0.2);
        dl2.position.set(-8, 3, -8);
        scene.add(dl2);

        // Faint purple glow from accretion disk
        var accretionGlow = new THREE.PointLight(0x6622AA, 0.6, 20);
        accretionGlow.position.set(0, 0.3, 0);
        scene.add(accretionGlow);

        createStarField();
    }

    // ─── STARS ─────────────────────────────────────────────────
    function createStarField() {
        var tex = circTex();
        var n = 2500;
        var geom = new THREE.BufferGeometry();
        var pos = new Float32Array(n * 3);
        var col = new Float32Array(n * 3);

        for (var i = 0; i < n; i++) {
            var th = Math.random() * Math.PI * 2;
            var ph = Math.acos(2 * Math.random() - 1);
            var r = 50 + Math.random() * 150;
            pos[i*3] = r * Math.sin(ph) * Math.cos(th);
            pos[i*3+1] = r * Math.sin(ph) * Math.sin(th);
            pos[i*3+2] = r * Math.cos(ph);

            var roll = Math.random();
            if (roll < 0.1) {
                col[i*3] = 0.2; col[i*3+1] = 1.0; col[i*3+2] = 1.0;
            } else if (roll < 0.3) {
                col[i*3] = 0.8; col[i*3+1] = 0.6; col[i*3+2] = 1.0;
            } else {
                col[i*3] = 1.0; col[i*3+1] = 1.0; col[i*3+2] = 1.0;
            }
        }
        geom.setAttribute('position', new THREE.BufferAttribute(pos, 3));
        geom.setAttribute('color', new THREE.BufferAttribute(col, 3));

        starField = new THREE.Points(geom, new THREE.PointsMaterial({
            size: 0.7,
            map: tex,
            vertexColors: true,
            transparent: true,
            opacity: 1.0,
            sizeAttenuation: true,
            blending: THREE.AdditiveBlending,
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
            pos[i*3+1] = Math.sin(a) * radius * Math.sin(inclination) * 0.15;
            pos[i*3+2] = Math.sin(a) * radius;
        }
        geom.setAttribute('position', new THREE.BufferAttribute(pos, 3));
        return new THREE.LineLoop(geom, new THREE.LineBasicMaterial({
            color: 0xA855F7, transparent: true, opacity: 0.05,
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
        return new THREE.Mesh(
            new THREE.SphereGeometry(size, 48, 48),
            new THREE.MeshPhongMaterial({
                color: c.base,
                emissive: c.emissive,
                emissiveIntensity: 0.35,
                shininess: 25,
                specular: 0x333333
            })
        );
    }

    // ─── ORBIT PARAMS ──────────────────────────────────────────
    function getOrbitRadius(index, total) {
        // Each orbit spaced 4 units apart minimum so planets never overlap
        // Planet diameter is ~3, so 4 units gap is safe
        var baseR = 5.0;
        var spacing = 4.0;
        return baseR + index * spacing;
    }

    function getOrbitSpeed(index) {
        return 0.2 / (1 + index * 0.3);
    }

    // Each orbit has a slight inclination for Y-axis modulation
    function getInclination(index) {
        var vals = [0.15, -0.25, 0.2, -0.18, 0.28, -0.12, 0.22, -0.2, 0.17, -0.15, 0.25, -0.22];
        return vals[index % vals.length];
    }

    function getPlanetSize(el) {
        if (el.classList.contains('icon-planet')) return 1.2;
        return 1.5;
    }

    // ─── SPRING ────────────────────────────────────────────────
    function spr(cur, tgt, vel, k, d, dt) {
        var f = (tgt - cur) * k - vel * d;
        vel += f * dt;
        cur += vel * dt;
        return { v: cur, vel: vel };
    }

    // ─── ORBITAL POSITION WITH Y MODULATION ────────────────────
    function orbitalPos(angle, radius, inclination) {
        return {
            x: Math.cos(angle) * radius,
            y: Math.sin(angle) * radius * Math.sin(inclination) * 0.15,
            z: Math.sin(angle) * radius
        };
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

    // ─── ANIMATION ─────────────────────────────────────────────
    function animate() {
        requestAnimationFrame(animate);
        var dt = Math.min(clock.getDelta(), 0.05);
        var elapsed = clock.getElapsedTime();

        // Camera drift — 25% faster rotation
        cameraAngle += dt * 0.0375;
        cameraTilt = 0.35 + Math.sin(elapsed * 0.07) * 0.1;

        camera.position.x = Math.sin(cameraAngle) * cameraRadius;
        camera.position.z = Math.cos(cameraAngle) * cameraRadius;
        camera.position.y = 10 + Math.sin(cameraTilt) * 3;
        camera.lookAt(0, 0, 0);

        if (starField) {
            starField.rotation.y = elapsed * 0.005;
        }

        var K = 3.5, D = 3.8;

        for (var i = planetMeshes.length - 1; i >= 0; i--) {
            var p = planetMeshes[i];

            if (p.leaving) {
                p.flyProgress += dt * 2.5;
                var acc = 1 + p.flyProgress * 5;
                p.mesh.position.add(p.flyDir.clone().multiplyScalar(dt * 20 * acc));
                p.mesh.rotation.x += dt * 5;

                var sp2 = proj(p.mesh.position);
                p.label.style.left = (sp2.x - p.label.offsetWidth / 2) + 'px';
                p.label.style.top = (sp2.y - p.label.offsetHeight / 2) + 'px';
                p.label.style.opacity = Math.max(0, 1 - p.flyProgress * 3).toFixed(3);

                if (p.flyProgress > 0.7) {
                    p.label.style.display = 'none';
                    scene.remove(p.mesh);
                    planetMeshes.splice(i, 1);
                }
                continue;
            }

            // Orbital target with Y modulation
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

            // Project to screen — ALL planets get same scaling logic
            var sp = proj(p.mesh.position);
            var ew = p.label.offsetWidth;
            var eh = p.label.offsetHeight;
            p.label.style.left = (sp.x - ew / 2) + 'px';
            p.label.style.top = (sp.y - eh / 2) + 'px';
            p.label.style.opacity = '1';
            p.label.style.zIndex = '15';

            // Depth-based scale — same formula for ALL planets including icons
            var dist = p.mesh.position.distanceTo(camera.position);
            var labelScale = Math.max(0.5, Math.min(1.3, 16 / dist));
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
                var pos = p.mesh.position.clone();
                if (pos.length() < 0.1) pos.set(1, 0, 0);
                var dir = pos.clone().normalize();
                dir.y += (Math.random() - 0.5) * 0.6;
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
                var sR = 35;
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
            arr.push({ m: m, d: d, s: 0.3+Math.random()*1, rs: new THREE.Vector3((Math.random()-0.5)*2,(Math.random()-0.5)*2,(Math.random()-0.5)*2) });
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
        }
        setupNavigation();
        initThree();
        animate();
        if(isOrbitPage&&!sessionStorage.getItem('bigbang')&&currentView==='home'){
            doBigBang();
        } else {
            spawnAsteroids3D();
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
    });
})();
