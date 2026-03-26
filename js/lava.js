const canvas = document.getElementById("lavaCanvas");
const gl = canvas.getContext("webgl");

canvas.width = window.innerWidth;
canvas.height = window.innerHeight;
gl.clearColor(0.005, 0.0, 0.02, 1.0);
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

float random(vec2 st) {
    return fract(sin(dot(st.xy, vec2(12.9898, 78.233))) * 43758.5453123);
}

float noise(vec2 st) {
    vec2 i = floor(st);
    vec2 f = fract(st);
    float a = random(i);
    float b = random(i + vec2(1.0, 0.0));
    float c = random(i + vec2(0.0, 1.0));
    float d = random(i + vec2(1.0, 1.0));
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(a, b, u.x) +
           (c - a) * u.y * (1.0 - u.x) +
           (d - b) * u.x * u.y;
}

void main() {
    vec2 st = gl_FragCoord.xy / u_resolution.xy;
    float aspect = u_resolution.x / u_resolution.y;

    vec2 bhPos = vec2(0.82, 0.22);
    vec2 toBH = st - bhPos;
    vec2 toBHc = toBH;
    toBHc.x *= aspect;
    float bhDist = length(toBHc);
    vec2 bhDir = normalize(toBH + vec2(0.0001));

    float lensing = 0.005 / (bhDist * bhDist + 0.002);
    lensing = min(lensing, 0.4);
    vec2 lensedST = st + bhDir * lensing;

    vec2 q = lensedST * 2.0;
    float n = noise(q + vec2(
        sin(q.y * 2.0 + u_time * 0.8),
        cos(q.x * 2.0 + u_time * 1.2)
    ));

    vec3 base = vec3(0.008, 0.002, 0.035);
    vec3 glow = vec3(0.04, 0.01, 0.12);
    vec3 color = mix(base, glow, pow(n, 0.8));

    vec2 diskUV = toBHc;
    diskUV.y *= 2.8;
    float diskDist = length(diskUV);
    float angle = atan(diskUV.y, diskUV.x);
    float diskNoise = noise(vec2(angle * 3.0 + u_time * 0.3, diskDist * 20.0));

    float outerR = 0.18;
    float outerW = 0.035;
    float outerDisk = exp(-(diskDist - outerR) * (diskDist - outerR) / (outerW * outerW));
    float doppler = 0.4 + 0.6 * cos(angle + u_time * 0.5);
    vec3 outerColor = mix(vec3(0.08, 0.02, 0.18), vec3(0.28, 0.12, 0.5), doppler);
    color += outerColor * outerDisk * doppler * (0.7 + diskNoise * 0.3);

    float innerR = 0.10;
    float innerW = 0.02;
    float innerDisk = exp(-(diskDist - innerR) * (diskDist - innerR) / (innerW * innerW));
    float innerDoppler = 0.3 + 0.7 * cos(angle + u_time * 0.8);
    vec3 innerColor = mix(vec3(0.15, 0.06, 0.3), vec3(0.45, 0.2, 0.65), innerDoppler);
    color += innerColor * innerDisk * innerDoppler * (0.8 + diskNoise * 0.2);

    float photon = exp(-(bhDist - 0.07) * (bhDist - 0.07) / 0.00003) * 0.4;
    color += vec3(0.22, 0.12, 0.38) * photon;

    float horizon = smoothstep(0.055, 0.035, bhDist);
    color *= (1.0 - horizon);

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

const positionBuffer = gl.createBuffer();
gl.bindBuffer(gl.ARRAY_BUFFER, positionBuffer);
gl.bufferData(gl.ARRAY_BUFFER, new Float32Array([
    -1, -1, 1, -1, -1, 1,
    -1, 1, 1, -1, 1, 1
]), gl.STATIC_DRAW);

gl.enableVertexAttribArray(positionAttributeLocation);
gl.vertexAttribPointer(positionAttributeLocation, 2, gl.FLOAT, false, 0, 0);

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
    gl.drawArrays(gl.TRIANGLES, 0, 6);
    requestAnimationFrame(render);
}
requestAnimationFrame(render);
