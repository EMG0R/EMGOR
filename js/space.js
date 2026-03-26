(function () {
    var WORDS = ['sound', 'signal', 'rhythm', 'noise', 'pulse', 'wave', 'flux', 'drift', 'echo', 'void'];

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

    function spawnBlackHole() {
        var W = window.innerWidth;
        var H = window.innerHeight;
        var hole = document.createElement('div');
        hole.className = 'black-hole';
        hole.style.right = (W * 0.08) + 'px';
        hole.style.bottom = (H * 0.15) + 'px';

        hole.innerHTML =
            '<div class="black-hole-core"></div>' +
            '<div class="black-hole-ring"></div>' +
            '<div class="black-hole-ring"></div>';

        document.body.appendChild(hole);

        window.addEventListener('resize', function () {
            hole.style.right = (window.innerWidth * 0.08) + 'px';
            hole.style.bottom = (window.innerHeight * 0.15) + 'px';
        });
    }

    function spawnFloatingWords() {
        var container = document.querySelector('[data-orbit="planet"]');
        if (!container) return;

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
                baseOpacity: 0.15 + Math.random() * 0.2
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
            circle.setAttribute('ry', orb.ry);
            circle.setAttribute('fill', 'none');
            circle.setAttribute('stroke', 'rgba(114, 0, 159, 0.10)');
            circle.setAttribute('stroke-width', '1');
            circle.setAttribute('stroke-dasharray', '4 8');
            svg.appendChild(circle);
        });

        document.body.appendChild(svg);
        return svg;
    }

    function initSolarSystem() {
        var orbiters = document.querySelectorAll('[data-orbit="planet"]');
        if (orbiters.length === 0) return;

        var W = window.innerWidth;
        var H = window.innerHeight;
        var cx = W / 2;
        var cy = H / 2;

        var minDim = Math.min(W, H);
        var baseRadius = minDim * 0.18;
        var radiusStep = minDim * 0.10;

        var planets = [];
        var orbitData = [];

        orbiters.forEach(function (el, i) {
            el.style.position = 'fixed';
            el.style.zIndex = '15';

            var rx = baseRadius + radiusStep * i + (Math.random() * 20 - 10);
            var ry = rx * (0.45 + Math.random() * 0.15);
            var speed = (0.12 + Math.random() * 0.12) / (1 + i * 0.25);
            var angle = (i / orbiters.length) * Math.PI * 2 + Math.random() * 0.5;
            var tilt = (Math.random() - 0.5) * 0.3;

            planets.push({
                el: el,
                rx: rx,
                ry: ry,
                speed: speed,
                angle: angle,
                tilt: tilt
            });

            orbitData.push({ rx: rx, ry: ry });
        });

        var orbitSvg = drawOrbitRings(cx, cy, orbitData);

        function animate() {
            planets.forEach(function (p) {
                p.angle += p.speed * 0.016;

                var x = cx + Math.cos(p.angle) * p.rx;
                var y = cy + Math.sin(p.angle) * p.ry;

                x += Math.sin(p.angle * 0.7 + p.tilt) * p.rx * 0.05;
                y += Math.cos(p.angle * 0.5 + p.tilt) * p.ry * 0.05;

                var ew = p.el.offsetWidth;
                var eh = p.el.offsetHeight;
                p.el.style.left = (x - ew / 2) + 'px';
                p.el.style.top = (y - eh / 2) + 'px';

                var depth = Math.sin(p.angle) * 0.15 + 0.85;
                p.el.style.transform = 'scale(' + depth.toFixed(3) + ')';
                p.el.style.opacity = (0.7 + depth * 0.3).toFixed(3);
                p.el.style.zIndex = depth > 1 ? '16' : '14';
            });

            requestAnimationFrame(animate);
        }

        animate();

        window.addEventListener('resize', function () {
            W = window.innerWidth;
            H = window.innerHeight;
            cx = W / 2;
            cy = H / 2;

            minDim = Math.min(W, H);
            baseRadius = minDim * 0.18;
            radiusStep = minDim * 0.10;

            planets.forEach(function (p, i) {
                p.rx = baseRadius + radiusStep * i + (Math.random() * 20 - 10);
                p.ry = p.rx * (0.45 + Math.random() * 0.15);
            });

            if (orbitSvg && orbitSvg.parentNode) {
                orbitSvg.parentNode.removeChild(orbitSvg);
            }
            var newOrbitData = planets.map(function (p) { return { rx: p.rx, ry: p.ry }; });
            orbitSvg = drawOrbitRings(cx, cy, newOrbitData);
        });
    }

    window.addEventListener('load', function () {
        spawnStars();
        if (document.querySelector('.black-hole') === null && document.querySelector('.floating-word') === null) {
            var isHome = document.querySelector('.icon-planet');
            if (isHome) {
                spawnBlackHole();
                spawnFloatingWords();
            }
        }
        initSolarSystem();
    });
})();
