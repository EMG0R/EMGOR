// physics.js — starfield: slow drifting stars, strong obstacle avoidance

(function () {
    const { Engine, Runner, Bodies, Composite, Events, Body, Mouse, MouseConstraint } = Matter;

    const STAR_COUNT  = 45;
    const HIT_SIZE    = 40;
    const STAR_VIS    = 8;

    // Target every actual visible element on the page
    const OBSTACLE_SELECTORS = [
        'h1',
        '.navigation-button',
        '.synth-box',
        '.bio-overlay',
        '.hero-image',
        '.social-links',
        '.home-button',
    ];

    function initStars() {
        let W = window.innerWidth;
        let H = window.innerHeight;
        const WALL = 150;

        const engine = Engine.create({ gravity: { x: 0, y: 0 } });
        const world  = engine.world;

        let walls = [];
        function buildWalls() {
            if (walls.length) Composite.remove(world, walls);
            W = window.innerWidth;
            H = window.innerHeight;
            walls = [
                Bodies.rectangle(W / 2, -WALL / 2,     W + WALL * 2, WALL, { isStatic: true, restitution: 1, friction: 0 }),
                Bodies.rectangle(W / 2,  H + WALL / 2, W + WALL * 2, WALL, { isStatic: true, restitution: 1, friction: 0 }),
                Bodies.rectangle(-WALL / 2, H / 2,     WALL, H + WALL * 2, { isStatic: true, restitution: 1, friction: 0 }),
                Bodies.rectangle(W + WALL / 2, H / 2,  WALL, H + WALL * 2, { isStatic: true, restitution: 1, friction: 0 }),
            ];
            Composite.add(world, walls);
        }
        buildWalls();

        function getObstacleRects() {
            const rects = [];
            OBSTACLE_SELECTORS.forEach(sel => {
                document.querySelectorAll(sel).forEach(el => {
                    const r = el.getBoundingClientRect();
                    if (r.width < 1 || r.height < 1) return;
                    // Use the TIGHT visual bounding box only
                    rects.push({ l: r.left, t: r.top, r: r.right, b: r.bottom });
                });
            });
            return rects;
        }

        let obstacleRects = getObstacleRects();

        function overlapsObstacle(x, y, pad) {
            for (let k = 0; k < obstacleRects.length; k++) {
                const o = obstacleRects[k];
                if (x > o.l - pad && x < o.r + pad &&
                    y > o.t - pad && y < o.b + pad) return true;
            }
            return false;
        }

        if (!document.getElementById('star-style')) {
            const s = document.createElement('style');
            s.id = 'star-style';
            s.textContent = `
                .star-wrap {
                    position: fixed;
                    width: ${HIT_SIZE}px; height: ${HIT_SIZE}px;
                    display: flex; align-items: center; justify-content: center;
                    cursor: grab; z-index: 50;
                    pointer-events: all; user-select: none;
                }
                .star-wrap:active { cursor: grabbing; }
                .star-glyph {
                    font-size: ${STAR_VIS}px; line-height: 1;
                    color: rgba(255,255,255,0.85);
                    pointer-events: none;
                    text-shadow: 0 0 6px rgba(180,120,255,0.7), 0 0 2px #fff;
                }
            `;
            document.head.appendChild(s);
        }

        const items = [];
        const HS = HIT_SIZE / 2;

        for (let i = 0; i < STAR_COUNT; i++) {
            let x, y, attempts = 0;
            do {
                x = HS + Math.random() * (W - HIT_SIZE);
                y = HS + Math.random() * (H - HIT_SIZE);
                attempts++;
            } while (overlapsObstacle(x, y, 30) && attempts < 80);

            const wrap = document.createElement('div');
            wrap.className = 'star-wrap';
            wrap.style.left = (x - HS) + 'px';
            wrap.style.top  = (y - HS) + 'px';
            const glyph = document.createElement('span');
            glyph.className = 'star-glyph';
            glyph.textContent = '★';
            wrap.appendChild(glyph);
            document.body.appendChild(wrap);

            const body = Bodies.rectangle(x, y, HIT_SIZE, HIT_SIZE, {
                restitution: 0.9,
                friction:    0.0,
                frictionAir: 0.02,
                inertia:     Infinity,
                label:       'star',
            });

            body._el = wrap;
            items.push({ body, el: wrap });
            Composite.add(world, body);
        }

        // Mouse
        const mouse = Mouse.create(document.body);
        Mouse.setOffset(mouse, { x: 0, y: 0 });
        Mouse.setScale(mouse,  { x: 1, y: 1 });

        ['mousemove','mousedown','mouseup','mousewheel','DOMMouseScroll'].forEach(ev => {
            mouse.element.removeEventListener(ev, mouse[ev.replace('DOM','').toLowerCase()] || (() => {}));
        });

        // Remove Matter.js default touch handlers — they call preventDefault()
        // which blocks <a> links and buttons from working on mobile
        ['touchstart', 'touchmove', 'touchend'].forEach(ev => {
            mouse.element.removeEventListener(ev, mouse[ev]);
        });

        function syncPos(e) {
            mouse.position.x = mouse.absolute.x = e.clientX;
            mouse.position.y = mouse.absolute.y = e.clientY;
        }
        document.addEventListener('mousemove', syncPos);
        document.addEventListener('mousedown', e => {
            if (!e.target.closest('.star-wrap')) return;
            mouse.button = 0; syncPos(e);
            mouse.mousedownPosition.x = e.clientX;
            mouse.mousedownPosition.y = e.clientY;
        });
        document.addEventListener('mouseup', e => {
            mouse.button = -1; syncPos(e);
            mouse.mouseupPosition.x = e.clientX;
            mouse.mouseupPosition.y = e.clientY;
        });

        // Touch support for mobile
        function syncTouch(e) {
            const t = e.touches[0] || e.changedTouches[0];
            if (!t) return;
            mouse.position.x = mouse.absolute.x = t.clientX;
            mouse.position.y = mouse.absolute.y = t.clientY;
        }
        document.addEventListener('touchstart', e => {
            const t = e.touches[0];
            if (!t || !e.target.closest('.star-wrap')) return;
            mouse.button = 0; syncTouch(e);
            mouse.mousedownPosition.x = t.clientX;
            mouse.mousedownPosition.y = t.clientY;
        }, { passive: true });
        document.addEventListener('touchmove', e => {
            if (mouse.button !== 0) return;
            syncTouch(e);
        }, { passive: true });
        document.addEventListener('touchend', e => {
            const t = e.changedTouches[0];
            if (!t) return;
            mouse.button = -1; syncTouch(e);
            mouse.mouseupPosition.x = t.clientX;
            mouse.mouseupPosition.y = t.clientY;
        }, { passive: true });

        const mc = MouseConstraint.create(engine, {
            mouse,
            constraint: { stiffness: 0.18, damping: 0.08, render: { visible: false } },
        });
        Composite.add(world, mc);
        Events.on(mc, 'startdrag', e => { if (e.body._el) e.body._el.style.cursor = 'grabbing'; });
        Events.on(mc, 'enddrag',   e => { if (e.body._el) e.body._el.style.cursor = 'grab'; });

        const runner = Runner.create();
        Runner.run(runner, engine);

        const MAX_SPEED    = 4;          // slow drift cap
        const JITTER_FORCE = 0.00004;    // very gentle nudge
        const SPREAD_FORCE = 0.00012;    // strong repulsion to fill gaps
        const SPREAD_DIST  = 220;
        const OBS_FORCE    = 0.001;      // strong obstacle avoidance
        const OBS_RANGE    = 80;         // wide avoidance zone

        Events.on(engine, 'beforeUpdate', () => {
            const nW = window.innerWidth;
            const nH = window.innerHeight;

            for (let i = 0; i < items.length; i++) {
                const a = items[i].body;

                // Gentle jitter
                Body.applyForce(a, a.position, {
                    x: (Math.random() - 0.5) * JITTER_FORCE,
                    y: (Math.random() - 0.5) * JITTER_FORCE,
                });

                // Star-to-star repulsion — actively spread out
                for (let j = i + 1; j < items.length; j++) {
                    const b = items[j].body;
                    const dx = a.position.x - b.position.x;
                    const dy = a.position.y - b.position.y;
                    const dist = Math.sqrt(dx * dx + dy * dy);
                    if (dist < SPREAD_DIST && dist > 1) {
                        const f = SPREAD_FORCE * Math.pow(1 - dist / SPREAD_DIST, 1.5);
                        const nx = (dx / dist) * f;
                        const ny = (dy / dist) * f;
                        Body.applyForce(a, a.position, { x:  nx, y:  ny });
                        Body.applyForce(b, b.position, { x: -nx, y: -ny });
                    }
                }

                // Strong obstacle avoidance
                for (let k = 0; k < obstacleRects.length; k++) {
                    const ob = obstacleRects[k];
                    const clampX = Math.max(ob.l, Math.min(ob.r, a.position.x));
                    const clampY = Math.max(ob.t, Math.min(ob.b, a.position.y));
                    const dx = a.position.x - clampX;
                    const dy = a.position.y - clampY;
                    const dist = Math.sqrt(dx * dx + dy * dy);
                    if (dist < OBS_RANGE && dist > 0.5) {
                        const f = OBS_FORCE * Math.pow(1 - dist / OBS_RANGE, 2);
                        Body.applyForce(a, a.position, { x: (dx / dist) * f, y: (dy / dist) * f });
                    } else if (dist < 0.5) {
                        // Inside obstacle — strong push out
                        const pushX = a.position.x - (ob.l + ob.r) / 2;
                        const pushY = a.position.y - (ob.t + ob.b) / 2;
                        const pushD = Math.sqrt(pushX * pushX + pushY * pushY) || 1;
                        Body.applyForce(a, a.position, { x: (pushX / pushD) * 0.003, y: (pushY / pushD) * 0.003 });
                    }
                }

                // Soft edge push
                const margin = 25, edgeF = 0.0002;
                if (a.position.x < margin)       Body.applyForce(a, a.position, { x:  edgeF, y: 0 });
                if (a.position.x > nW - margin)  Body.applyForce(a, a.position, { x: -edgeF, y: 0 });
                if (a.position.y < margin)        Body.applyForce(a, a.position, { x: 0, y:  edgeF });
                if (a.position.y > nH - margin)   Body.applyForce(a, a.position, { x: 0, y: -edgeF });
            }
        });

        const VISUAL_JITTER = 0.8;
        Events.on(engine, 'afterUpdate', () => {
            const nW = window.innerWidth;
            const nH = window.innerHeight;
            items.forEach(({ body, el }) => {
                // Clamp speed
                const spd = Math.hypot(body.velocity.x, body.velocity.y);
                if (spd > MAX_SPEED) {
                    Body.setVelocity(body, {
                        x: (body.velocity.x / spd) * MAX_SPEED,
                        y: (body.velocity.y / spd) * MAX_SPEED,
                    });
                }
                // Clamp position
                const clx = Math.max(HS, Math.min(nW - HS, body.position.x));
                const cly = Math.max(HS, Math.min(nH - HS, body.position.y));
                if (clx !== body.position.x || cly !== body.position.y) {
                    Body.setPosition(body, { x: clx, y: cly });
                    Body.setVelocity(body, { x: -body.velocity.x * 0.5, y: -body.velocity.y * 0.5 });
                }
                // Visual jitter
                const jx = (Math.random() - 0.5) * VISUAL_JITTER;
                const jy = (Math.random() - 0.5) * VISUAL_JITTER;
                el.style.left = (body.position.x - HS + jx) + 'px';
                el.style.top  = (body.position.y - HS + jy) + 'px';
            });
        });

        // Resize: rebuild walls, refresh obstacles, explode then settle
        window.addEventListener('resize', () => {
            buildWalls();
            obstacleRects = getObstacleRects();
            const nW = window.innerWidth;
            const nH = window.innerHeight;
            items.forEach(({ body }) => {
                Body.setPosition(body, {
                    x: Math.max(HS, Math.min(nW - HS, body.position.x)),
                    y: Math.max(HS, Math.min(nH - HS, body.position.y)),
                });
                Body.setVelocity(body, {
                    x: (Math.random() - 0.5) * 8,
                    y: (Math.random() - 0.5) * 8,
                });
            });
        });
    }

    window.addEventListener('load', initStars);
})();
