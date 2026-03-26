const canvas = document.getElementById("lavaCanvas");
const gl = canvas.getContext("webgl");

canvas.width = window.innerWidth;
canvas.height = window.innerHeight;
gl.clearColor(0.01, 0.0, 0.04, 1.0);
gl.clear(gl.COLOR_BUFFER_BIT);

const vertexShaderSource = `
attribute vec2 a_position;
void main() {
    gl_Position = vec4(a_position, 0, 1);
}
`;

const fragmentShaderSource = `
precision mediump float;
uniform float u_time;
uniform vec2 u_resolution;
uniform float u_scroll;

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}

float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(
        mix(hash(i), hash(i + vec2(1.0, 0.0)), u.x),
        mix(hash(i + vec2(0.0, 1.0)), hash(i + vec2(1.0, 1.0)), u.x),
        u.y
    );
}

float fbm(vec2 p) {
    float val = 0.0;
    float amp = 0.5;
    float freq = 1.0;
    for (int i = 0; i < 5; i++) {
        val += amp * noise(p * freq);
        freq *= 2.1;
        amp *= 0.48;
        p += vec2(1.7, 9.2);
    }
    return val;
}

float starField(vec2 uv, float t) {
    float stars = 0.0;
    for (float i = 0.0; i < 3.0; i++) {
        vec2 grid = uv * (80.0 + i * 60.0);
        vec2 id = floor(grid);
        vec2 gv = fract(grid) - 0.5;
        float h = hash(id + i * 100.0);
        float size = 0.015 + h * 0.01;
        float brightness = smoothstep(size, 0.0, length(gv));
        float twinkle = sin(t * (2.0 + h * 4.0) + h * 6.28) * 0.5 + 0.5;
        twinkle = mix(0.3, 1.0, twinkle);
        stars += brightness * twinkle * step(0.92, h);
    }
    return stars;
}

void main() {
    vec2 uv = gl_FragCoord.xy / u_resolution.xy;
    float t = u_time * 0.12;
    float scroll = u_scroll * 0.0003;

    vec2 q = uv * 2.0;
    q.y += scroll;

    float n1 = fbm(q + vec2(
        sin(q.y * 1.2 + t * 1.8) * 0.9,
        cos(q.x * 1.5 + t * 1.4) * 0.8
    ));

    float n2 = fbm(q * 1.4 + vec2(
        cos(t * 1.0 + n1 * 2.5),
        sin(t * 0.7 + n1 * 2.0)
    ));

    float n3 = fbm(q * 0.6 + vec2(n2 * 1.8, n1 * 1.4) + t * 0.4);

    vec3 voidBlack = vec3(0.005, 0.0, 0.015);
    vec3 deepPurple = vec3(0.08, 0.0, 0.14);
    vec3 spacePurple = vec3(0.04, 0.005, 0.10);
    vec3 darkViolet = vec3(0.06, 0.0, 0.08);
    vec3 nebulaTeal = vec3(0.0, 0.03, 0.07);
    vec3 nebulaBlue = vec3(0.01, 0.02, 0.09);

    float blend1 = smoothstep(0.15, 0.75, n2);
    float blend2 = smoothstep(0.25, 0.85, n3);
    float blend3 = smoothstep(0.1, 0.65, n1);

    vec3 color = voidBlack;
    color = mix(color, spacePurple, blend1 * 0.7);
    color = mix(color, deepPurple, blend3 * 0.5);
    color = mix(color, nebulaTeal, blend2 * 0.35);
    color = mix(color, nebulaBlue, (1.0 - blend1) * blend2 * 0.3);
    color = mix(color, darkViolet, smoothstep(0.4, 0.9, n1 * n2) * 0.4);

    float nebulaGlow = pow(n2 * n3, 1.5) * 0.25;
    color += vec3(0.06, 0.01, 0.10) * nebulaGlow;

    color *= 0.8 + 0.2 * n3;

    float stars = starField(uv, u_time * 0.5);
    float starMask = 1.0 - smoothstep(0.0, 0.3, length(color) * 2.0);
    color += vec3(0.9, 0.85, 1.0) * stars * starMask * 0.7;

    gl_FragColor = vec4(color, 1.0);
}
`;

function createShader(gl, type, source) {
    const shader = gl.createShader(type);
    gl.shaderSource(shader, source);
    gl.compileShader(shader);
    if (!gl.getShaderParameter(shader, gl.COMPILE_STATUS)) {
        console.error("Shader Error:", gl.getShaderInfoLog(shader));
    }
    return shader;
}

const vertexShader = createShader(gl, gl.VERTEX_SHADER, vertexShaderSource);
const fragmentShader = createShader(gl, gl.FRAGMENT_SHADER, fragmentShaderSource);

const program = gl.createProgram();
gl.attachShader(program, vertexShader);
gl.attachShader(program, fragmentShader);
gl.linkProgram(program);
if (!gl.getProgramParameter(program, gl.LINK_STATUS)) {
    console.error("Program Link Error:", gl.getProgramInfoLog(program));
}
gl.useProgram(program);

const positionAttributeLocation = gl.getAttribLocation(program, "a_position");
const timeUniformLocation = gl.getUniformLocation(program, "u_time");
const resolutionUniformLocation = gl.getUniformLocation(program, "u_resolution");
const scrollUniformLocation = gl.getUniformLocation(program, "u_scroll");

const positionBuffer = gl.createBuffer();
gl.bindBuffer(gl.ARRAY_BUFFER, positionBuffer);
gl.bufferData(gl.ARRAY_BUFFER, new Float32Array([
    -1, -1, 1, -1, -1, 1,
    -1, 1, 1, -1, 1, 1
]), gl.STATIC_DRAW);

gl.enableVertexAttribArray(positionAttributeLocation);
gl.vertexAttribPointer(positionAttributeLocation, 2, gl.FLOAT, false, 0, 0);

let scrollY = 0;
window.addEventListener('scroll', () => { scrollY = window.scrollY; }, { passive: true });

function handleResize() {
    canvas.width = window.innerWidth;
    canvas.height = window.innerHeight;
}
window.addEventListener('resize', handleResize);

function render(time) {
    time *= 0.001;
    gl.viewport(0, 0, canvas.width, canvas.height);
    gl.clear(gl.COLOR_BUFFER_BIT);
    gl.uniform1f(timeUniformLocation, time);
    gl.uniform2f(resolutionUniformLocation, canvas.width, canvas.height);
    gl.uniform1f(scrollUniformLocation, scrollY);
    gl.drawArrays(gl.TRIANGLES, 0, 6);
    requestAnimationFrame(render);
}
requestAnimationFrame(render);
