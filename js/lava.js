const canvas = document.getElementById("lavaCanvas");
const gl = canvas.getContext("webgl");

canvas.width = window.innerWidth;
canvas.height = window.innerHeight;
gl.clearColor(0.027, 0.098, 0.290, 1.0);
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
    for (int i = 0; i < 4; i++) {
        val += amp * noise(p * freq);
        freq *= 2.0;
        amp *= 0.5;
        p += vec2(1.7, 9.2);
    }
    return val;
}

void main() {
    vec2 uv = gl_FragCoord.xy / u_resolution.xy;
    float t = u_time * 0.15;
    float scroll = u_scroll * 0.0003;

    vec2 q = uv * 2.5;
    q.y += scroll;

    float n1 = fbm(q + vec2(
        sin(q.y * 1.5 + t * 2.0) * 0.8,
        cos(q.x * 1.8 + t * 1.6) * 0.7
    ));

    float n2 = fbm(q * 1.3 + vec2(
        cos(t * 1.2 + n1 * 2.0),
        sin(t * 0.9 + n1 * 1.5)
    ));

    float n3 = fbm(q * 0.7 + vec2(n2 * 1.5, n1 * 1.2) + t * 0.5);

    vec3 deepBlue = vec3(0.012, 0.043, 0.125);
    vec3 deepPurple = vec3(0.06, 0.0, 0.12);
    vec3 deepTeal = vec3(0.0, 0.05, 0.1);
    vec3 midBlue = vec3(0.02, 0.07, 0.2);

    float blend1 = smoothstep(0.2, 0.7, n2);
    float blend2 = smoothstep(0.3, 0.8, n3);
    float blend3 = smoothstep(0.1, 0.6, n1);

    vec3 color = deepBlue;
    color = mix(color, deepPurple, blend1 * 0.6);
    color = mix(color, deepTeal, blend2 * 0.5);
    color = mix(color, midBlue, blend3 * 0.4);

    float glow = pow(n2, 2.0) * 0.15;
    color += vec3(0.03, 0.01, 0.06) * glow;

    color *= 0.85 + 0.15 * n3;

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
