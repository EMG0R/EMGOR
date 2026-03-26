(function () {
    var WORDS = ['sound', 'signal', 'rhythm', 'noise', 'pulse', 'wave', 'flux', 'drift', 'echo', 'void'];
    var currentView = 'home';
    var activePlanets = [];
    var orbitSvg = null;
    var isOrbitPage = false;
    var transitionOverlay = null;

    function spawnStars() {
        var container = document.createElement('div');
        container.id = 'bg-stars';
        container.style.cssText = 'position:fixed;top:0;left:0;width:100vw;height:100vh;z-index:0;pointer-events:none;overflow:hidden;';
        document.body.appendChild(container);

        if (!document.getElementById('star-pulse-style')) {
            var style = document.createElement('style');
            style.id = 'star-pulse-style';
            style.textContent =
                '@keyframes starPulse{0%,100%{opacity:0;transform:scale(0.5)}50%{opacity:0.8;transform:scale(1.1)}}';
            document.head.appendChild(style);
        }

        for (var i = 0; i < 200; i++) {
            var s = document.createElement('div');
            var size = 0.5 + Math.random() * 2.5;
            var dur = 2 + Math.random() * 8;
            var delay = Math.random() * 12;
            s.style.cssText = 'position:absolute;border-radius:50%;background:rgba(255,255,255,0.7);' +
                'width:' + size + 'px;height:' + size + 'px;' +
                'left:' + (Math.random() * 100) + '%;top:' + (Math.random() * 100) + '%;' +
                'animation:starPulse ' + dur + 's ease-in-out ' + delay + 's infinite;opacity:0;';
            container.appendChild(s);
        }
    }

    function spawnAsteroids() {
        var asteroids = [];

        function createAsteroid() {
            var el = document.createElement('div');
            var size = 1.5 + Math.random() * 4;
            var opacity = 0.15 + Math.random() * 0.35;
            el.style.cssText = 'position:fixed;pointer-events:none;z-index:3;border-radius:30%;' +
                'width:' + size + 'px;height:' + (size * (0.5 + Math.random() * 0.5)) + 'px;' +
                'background:rgba(160,140,180,' + opacity + ');';

            var W = window.innerWidth;
            var H = window.innerHeight;
            var edge = Math.floor(Math.random() * 4);
            var x, y, vx, vy;

            if (edge === 0) {
                x = Math.random() * W; y = -10;
                vx = (Math.random() - 0.5) * 1.5; vy = 0.5 + Math.random() * 1.5;
            } else if (edge === 1) {
                x = W + 10; y = Math.random() * H;
                vx = -(0.5 + Math.random() * 1.5); vy = (Math.random() - 0.5) * 1.5;
            } else if (edge === 2) {
                x = Math.random() * W; y = H + 10;
                vx = (Math.random() - 0.5) * 1.5; vy = -(0.5 + Math.random() * 1.5);
            } else {
                x = -10; y = Math.random() * H;
                vx = 0.5 + Math.random() * 1.5; vy = (Math.random() - 0.5) * 1.5;
            }

            document.body.appendChild(el);
            asteroids.push({ el: el, x: x, y: y, vx: vx, vy: vy, rot: Math.random() * 360, rotSpeed: (Math.random() - 0.5) * 3 });
        }

        function scheduleSpawn() {
            setTimeout(function () {
                if (asteroids.length < 15) createAsteroid();
                scheduleSpawn();
            }, 1500 + Math.random() * 3000);
        }
        scheduleSpawn();
        for (var i = 0; i < 4; i++) createAsteroid();

        function animate() {
            var W = window.innerWidth;
            var H = window.innerHeight;
            for (var i = asteroids.length - 1; i >= 0; i--) {
                var a = asteroids[i];
                a.x += a.vx;
                a.y += a.vy;
                a.rot += a.rotSpeed;
                a.el.style.left = a.x + 'px';
                a.el.style.top = a.y + 'px';
                a.el.style.transform = 'rotate(' + a.rot.toFixed(0) + 'deg)';
                if (a.x < -60 || a.x > W + 60 || a.y < -60 || a.y > H + 60) {
                    a.el.parentNode.removeChild(a.el);
                    asteroids.splice(i, 1);
                }
            }
            requestAnimationFrame(animate);
        }
        animate();
    }

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
                vx: (Math.random() - 0.5) * 0.3,
                vy: (Math.random() - 0.5) * 0.3,
                baseOpacity: 0.12 + Math.random() * 0.15
            });
        });

        function animateWords() {
            var W = window.innerWidth;
            var H = window.innerHeight;
            wordEls.forEach(function (w) {
                w.x += w.vx;
                w.y += w.vy;
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

    function drawOrbitRings(cx, cy, orbits) {
        var svgNS = 'http://www.w3.org/2000/svg';
        var svg = document.createElementNS(svgNS, 'svg');
        svg.style.cssText = 'position:fixed;top:0;left:0;width:100vw;height:100vh;z-index:1;pointer-events:none;';
        svg.setAttribute('width', '100%');
        svg.setAttribute('height', '100%');

        orbits.forEach(function (orb) {
            var circle = document.createElementNS(svgNS, 'ellipse');
            circle.setAttribute('cx', cx);
            circle.setAttribute('cy', cy);
            circle.setAttribute('rx', orb.rx);
            circle.setAttribute('ry', orb.rx * orb.tilt);
            circle.setAttribute('transform', 'rotate(' + (orb.rotation * 180 / Math.PI).toFixed(1) + ' ' + cx + ' ' + cy + ')');
            circle.setAttribute('fill', 'none');
            circle.setAttribute('stroke', 'rgba(114, 0, 159, 0.07)');
            circle.setAttribute('stroke-width', '1');
            circle.setAttribute('stroke-dasharray', '4 8');
            svg.appendChild(circle);
        });

        document.body.appendChild(svg);
        return svg;
    }

    function rebuildOrbitRings() {
        if (orbitSvg && orbitSvg.parentNode) orbitSvg.parentNode.removeChild(orbitSvg);
        var cx = window.innerWidth / 2;
        var cy = window.innerHeight / 2;
        var data = activePlanets.filter(function (p) { return !p.leaving; })
            .map(function (p) { return { rx: p.rx, tilt: p.tilt, rotation: p.rotation }; });
        orbitSvg = drawOrbitRings(cx, cy, data);
    }

    function getVisiblePlanets() {
        return Array.from(document.querySelectorAll('[data-orbit="planet"]')).filter(function (el) {
            var view = el.getAttribute('data-view');
            return view === currentView || view === 'always';
        });
    }

    function buildOrbit() {
        activePlanets = [];
        var visible = getVisiblePlanets();
        var W = window.innerWidth;
        var H = window.innerHeight;
        var minDim = Math.min(W, H);
        var baseRadius = minDim * 0.16;
        var radiusStep = minDim * 0.08;

        visible.forEach(function (el, i) {
            el.style.position = 'fixed';
            el.style.display = 'flex';

            activePlanets.push({
                el: el,
                rx: baseRadius + radiusStep * i + (Math.random() * 15 - 7),
                tilt: 0.25 + Math.random() * 0.35,
                rotation: Math.random() * Math.PI,
                speed: (0.06 + Math.random() * 0.10) / (1 + i * 0.15),
                angle: Math.random() * Math.PI * 2,
                idx: i,
                scale: 1,
                targetScale: 1,
                leaving: false
            });
        });

        rebuildOrbitRings();
    }

    function animateOrbits() {
        var cx = window.innerWidth / 2;
        var cy = window.innerHeight / 2;

        for (var i = activePlanets.length - 1; i >= 0; i--) {
            var p = activePlanets[i];
            p.angle += p.speed * 0.016;

            if (p.scale < p.targetScale) {
                p.scale = Math.min(p.targetScale, p.scale + 0.025);
            } else if (p.scale > p.targetScale) {
                p.scale = Math.max(p.targetScale, p.scale - 0.035);
            }

            if (p.leaving && p.scale <= 0.01) {
                p.el.style.display = 'none';
                p.el.style.opacity = '0';
                activePlanets.splice(i, 1);
                continue;
            }

            var localX = Math.cos(p.angle) * p.rx;
            var localY = Math.sin(p.angle) * p.rx * p.tilt;
            var cosR = Math.cos(p.rotation);
            var sinR = Math.sin(p.rotation);
            var x = cx + localX * cosR - localY * sinR;
            var y = cy + localX * sinR + localY * cosR;

            var ew = p.el.offsetWidth;
            var eh = p.el.offsetHeight;
            p.el.style.left = (x - ew / 2) + 'px';
            p.el.style.top = (y - eh / 2) + 'px';

            var depth = Math.sin(p.angle) * 0.2 + 0.8;
            var finalScale = depth * p.scale;
            p.el.style.transform = 'scale(' + finalScale.toFixed(3) + ')';
            p.el.style.opacity = (p.scale * (0.6 + depth * 0.4)).toFixed(3);
            p.el.style.zIndex = Math.floor(depth * 100);
        }

        requestAnimationFrame(animateOrbits);
    }

    function switchView(newView) {
        if (newView === currentView) return;

        activePlanets.forEach(function (p) {
            var view = p.el.getAttribute('data-view');
            if (view !== 'always') {
                p.leaving = true;
                p.targetScale = 0;
            }
        });

        currentView = newView;
        if (newView !== 'home') {
            history.replaceState(null, '', location.pathname + '#' + newView);
        } else {
            history.replaceState(null, '', location.pathname);
        }

        setTimeout(function () {
            var W = window.innerWidth;
            var H = window.innerHeight;
            var minDim = Math.min(W, H);
            var baseRadius = minDim * 0.16;
            var radiusStep = minDim * 0.08;

            var alwaysCount = activePlanets.filter(function (p) {
                return p.el.getAttribute('data-view') === 'always' && !p.leaving;
            }).length;

            var newEls = Array.from(document.querySelectorAll('[data-orbit="planet"]')).filter(function (el) {
                return el.getAttribute('data-view') === currentView;
            });

            newEls.forEach(function (el, i) {
                el.style.display = 'flex';
                el.style.position = 'fixed';
                var idx = alwaysCount + i;
                activePlanets.push({
                    el: el,
                    rx: baseRadius + radiusStep * idx + (Math.random() * 15 - 7),
                    tilt: 0.25 + Math.random() * 0.35,
                    rotation: Math.random() * Math.PI,
                    speed: (0.06 + Math.random() * 0.10) / (1 + idx * 0.15),
                    angle: Math.random() * Math.PI * 2,
                    idx: idx,
                    scale: 0,
                    targetScale: 1,
                    leaving: false
                });
            });

            rebuildOrbitRings();
        }, 300);
    }

    function setupNavigation() {
        transitionOverlay = document.createElement('div');
        transitionOverlay.style.cssText = 'position:fixed;top:0;left:0;width:100vw;height:100vh;background:#030010;z-index:9999;pointer-events:none;opacity:0;transition:opacity 0.35s ease;';
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
            setTimeout(function () {
                window.location.href = href;
            }, 350);
        });
    }

    window.addEventListener('load', function () {
        isOrbitPage = document.querySelectorAll('[data-orbit="planet"]').length > 0;

        if (isOrbitPage) {
            var hash = location.hash.replace('#', '');
            currentView = (hash === 'synth' || hash === 'resources') ? hash : 'home';
            document.querySelectorAll('[data-orbit="planet"]').forEach(function (el) {
                var view = el.getAttribute('data-view');
                if (view !== currentView && view !== 'always') {
                    el.style.display = 'none';
                }
            });
        }

        spawnStars();
        spawnAsteroids();
        setupNavigation();

        if (isOrbitPage) {
            if (document.querySelector('.icon-planet')) {
                spawnFloatingWords();
            }
            buildOrbit();
            animateOrbits();
        }
    });

    window.addEventListener('hashchange', function () {
        if (!isOrbitPage) return;
        var h = location.hash.replace('#', '');
        var newView = (h === 'synth' || h === 'resources') ? h : 'home';
        switchView(newView);
    });

    window.addEventListener('resize', function () {
        if (!isOrbitPage || activePlanets.length === 0) return;
        var minDim = Math.min(window.innerWidth, window.innerHeight);
        var baseRadius = minDim * 0.16;
        var radiusStep = minDim * 0.08;

        activePlanets.forEach(function (p) {
            p.rx = baseRadius + radiusStep * p.idx + (Math.random() * 15 - 7);
        });

        rebuildOrbitRings();
    });
})();
