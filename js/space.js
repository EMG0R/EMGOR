(function () {
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

        for (var i = 0; i < 80; i++) {
            var s = document.createElement('div');
            var size = 0.8 + Math.random() * 2;
            var dur = 3 + Math.random() * 6;
            var delay = Math.random() * 10;
            s.style.cssText = 'position:absolute;border-radius:50%;background:rgba(255,255,255,0.7);' +
                'width:' + size + 'px;height:' + size + 'px;' +
                'left:' + (Math.random() * 100) + '%;top:' + (Math.random() * 100) + '%;' +
                'animation:starPulse ' + dur + 's ease-in-out ' + delay + 's infinite;opacity:0;';
            container.appendChild(s);
        }
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
            circle.setAttribute('stroke', 'rgba(114, 0, 159, 0.12)');
            circle.setAttribute('stroke-width', '1');
            circle.setAttribute('stroke-dasharray', '4 8');
            svg.appendChild(circle);
        });

        document.body.appendChild(svg);
        return svg;
    }

    function initSolarSystem() {
        var sun = document.querySelector('[data-orbit="sun"]');
        var orbiters = document.querySelectorAll('[data-orbit="planet"]');
        if (!sun || orbiters.length === 0) return;

        var W = window.innerWidth;
        var H = window.innerHeight;
        var cx = W / 2;
        var cy = H / 2;

        sun.style.position = 'fixed';
        sun.style.zIndex = '20';
        sun.style.left = (cx - sun.offsetWidth / 2) + 'px';
        sun.style.top = (cy - sun.offsetHeight / 2) + 'px';

        var minDim = Math.min(W, H);
        var baseRadius = minDim * 0.14;
        var radiusStep = minDim * 0.09;

        var planets = [];
        var orbitData = [];

        orbiters.forEach(function (el, i) {
            el.style.position = 'fixed';
            el.style.zIndex = '15';

            var rx = baseRadius + radiusStep * i + (Math.random() * 20 - 10);
            var ry = rx * (0.45 + Math.random() * 0.15);
            var speed = (0.15 + Math.random() * 0.15) / (1 + i * 0.3);
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

            sun.style.left = (cx - sun.offsetWidth / 2) + 'px';
            sun.style.top = (cy - sun.offsetHeight / 2) + 'px';

            minDim = Math.min(W, H);
            baseRadius = minDim * 0.14;
            radiusStep = minDim * 0.09;

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
        initSolarSystem();
    });
})();
