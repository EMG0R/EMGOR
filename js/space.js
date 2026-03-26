(function () {
    const { Engine, Runner, Bodies, Composite, Body, Mouse, MouseConstraint, Events } = Matter;

    function generateShape(pointCount) {
        var points = [];
        for (var i = 0; i < pointCount; i++) {
            var angle = (i / pointCount) * Math.PI * 2;
            var r = 0.38 + Math.random() * 0.14;
            points.push({
                x: 50 + Math.cos(angle) * r * 100,
                y: 50 + Math.sin(angle) * r * 100
            });
        }
        return points.map(function(p) { return p.x.toFixed(1) + '% ' + p.y.toFixed(1) + '%'; }).join(', ');
    }

    function applyShape(el) {
        var pts = 7 + Math.floor(Math.random() * 5);
        el.style.clipPath = 'polygon(' + generateShape(pts) + ')';
        el.style.webkitClipPath = el.style.clipPath;
    }

    function spawnStars() {
        var container = document.createElement('div');
        container.id = 'bg-stars';
        container.style.cssText = 'position:fixed;top:0;left:0;width:100vw;height:100vh;z-index:-1;pointer-events:none;overflow:hidden;';
        document.body.appendChild(container);

        var COUNT = 60;

        function makeStar() {
            var s = document.createElement('div');
            var size = 1 + Math.random() * 2.5;
            var x = Math.random() * 100;
            var y = Math.random() * 100;
            var dur = 3 + Math.random() * 5;
            var delay = Math.random() * 8;
            s.style.cssText = 'position:absolute;border-radius:50%;background:rgba(255,255,255,0.8);' +
                'width:' + size + 'px;height:' + size + 'px;' +
                'left:' + x + '%;top:' + y + '%;' +
                'animation:starPulse ' + dur + 's ease-in-out ' + delay + 's infinite;' +
                'opacity:0;';
            container.appendChild(s);
            return s;
        }

        if (!document.getElementById('star-pulse-style')) {
            var style = document.createElement('style');
            style.id = 'star-pulse-style';
            style.textContent =
                '@keyframes starPulse{0%,100%{opacity:0;transform:scale(0.5)}' +
                '50%{opacity:0.9;transform:scale(1.2)}}';
            document.head.appendChild(style);
        }

        for (var i = 0; i < COUNT; i++) makeStar();

        setInterval(function() {
            var existing = container.children;
            if (existing.length > 0) {
                var idx = Math.floor(Math.random() * existing.length);
                existing[idx].style.left = Math.random() * 100 + '%';
                existing[idx].style.top = Math.random() * 100 + '%';
            }
        }, 4000);
    }

    function initPhysics() {
        var W = window.innerWidth;
        var H = window.innerHeight;

        var engine = Engine.create({ gravity: { x: 0, y: 0 } });
        var world = engine.world;

        var WALL = 200;
        var walls = [
            Bodies.rectangle(W / 2, -WALL / 2, W + WALL * 4, WALL, { isStatic: true, restitution: 1, friction: 0, frictionStatic: 0 }),
            Bodies.rectangle(W / 2, H + WALL / 2, W + WALL * 4, WALL, { isStatic: true, restitution: 1, friction: 0, frictionStatic: 0 }),
            Bodies.rectangle(-WALL / 2, H / 2, WALL, H + WALL * 4, { isStatic: true, restitution: 1, friction: 0, frictionStatic: 0 }),
            Bodies.rectangle(W + WALL / 2, H / 2, WALL, H + WALL * 4, { isStatic: true, restitution: 1, friction: 0, frictionStatic: 0 })
        ];
        Composite.add(world, walls);

        var floatables = document.querySelectorAll('[data-float]');
        var items = [];
        var PAD = 80;

        floatables.forEach(function(el, i) {
            el.style.position = 'fixed';
            el.style.zIndex = '10';
            el.style.margin = '0';

            if (el.dataset.float === 'shape') {
                applyShape(el);
            }

            var rect = el.getBoundingClientRect();
            var w = rect.width + 10;
            var h = rect.height + 10;

            var x = PAD + Math.random() * (W - w - PAD * 2);
            var y = PAD + Math.random() * (H - h - PAD * 2);

            var body = Bodies.rectangle(x + w / 2, y + h / 2, w, h, {
                restitution: 1,
                friction: 0,
                frictionAir: 0,
                frictionStatic: 0,
                inertia: Infinity,
                inverseInertia: 0,
                label: 'float-' + i
            });

            var speed = 0.3 + Math.random() * 0.5;
            var angle = Math.random() * Math.PI * 2;
            Body.setVelocity(body, {
                x: Math.cos(angle) * speed,
                y: Math.sin(angle) * speed
            });

            body._el = el;
            body._hw = w / 2;
            body._hh = h / 2;
            items.push({ body: body, el: el });
            Composite.add(world, body);
        });

        var mouse = Mouse.create(document.body);
        mouse.element.removeEventListener('mousewheel', mouse.mousewheel);
        mouse.element.removeEventListener('DOMMouseScroll', mouse.mousewheel);

        var mc = MouseConstraint.create(engine, {
            mouse: mouse,
            constraint: {
                stiffness: 0.1,
                damping: 0.05,
                render: { visible: false }
            }
        });

        mc.collisionFilter = { category: 0x0001, mask: 0x0001 };

        Composite.add(world, mc);

        Events.on(mc, 'startdrag', function(e) {
            if (e.body._el) e.body._el.style.cursor = 'grabbing';
        });
        Events.on(mc, 'enddrag', function(e) {
            if (e.body._el) e.body._el.style.cursor = 'grab';
        });

        var runner = Runner.create();
        Runner.run(runner, engine);

        var MAX_SPEED = 2;
        var MIN_SPEED = 0.2;

        Events.on(engine, 'afterUpdate', function() {
            var nW = window.innerWidth;
            var nH = window.innerHeight;

            items.forEach(function(item) {
                var b = item.body;
                var el = item.el;

                var spd = Math.sqrt(b.velocity.x * b.velocity.x + b.velocity.y * b.velocity.y);
                if (spd > MAX_SPEED) {
                    Body.setVelocity(b, {
                        x: (b.velocity.x / spd) * MAX_SPEED,
                        y: (b.velocity.y / spd) * MAX_SPEED
                    });
                } else if (spd < MIN_SPEED && spd > 0) {
                    Body.setVelocity(b, {
                        x: (b.velocity.x / spd) * MIN_SPEED,
                        y: (b.velocity.y / spd) * MIN_SPEED
                    });
                }

                var px = b.position.x - b._hw;
                var py = b.position.y - b._hh;
                px = Math.max(0, Math.min(nW - b._hw * 2, px));
                py = Math.max(0, Math.min(nH - b._hh * 2, py));

                el.style.left = px + 'px';
                el.style.top = py + 'px';
            });
        });

        window.addEventListener('resize', function() {
            var nW = window.innerWidth;
            var nH = window.innerHeight;

            Composite.remove(world, walls);
            walls = [
                Bodies.rectangle(nW / 2, -WALL / 2, nW + WALL * 4, WALL, { isStatic: true, restitution: 1, friction: 0, frictionStatic: 0 }),
                Bodies.rectangle(nW / 2, nH + WALL / 2, nW + WALL * 4, WALL, { isStatic: true, restitution: 1, friction: 0, frictionStatic: 0 }),
                Bodies.rectangle(-WALL / 2, nH / 2, WALL, nH + WALL * 4, { isStatic: true, restitution: 1, friction: 0, frictionStatic: 0 }),
                Bodies.rectangle(nW + WALL / 2, nH / 2, WALL, nH + WALL * 4, { isStatic: true, restitution: 1, friction: 0, frictionStatic: 0 })
            ];
            Composite.add(world, walls);

            items.forEach(function(item) {
                var b = item.body;
                Body.setPosition(b, {
                    x: Math.max(b._hw, Math.min(nW - b._hw, b.position.x)),
                    y: Math.max(b._hh, Math.min(nH - b._hh, b.position.y))
                });
            });
        });
    }

    window.addEventListener('load', function() {
        spawnStars();
        if (document.querySelector('[data-float]')) {
            initPhysics();
        }
    });
})();
