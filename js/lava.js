const canvas = document.getElementById("lavaCanvas");
const gl = canvas.getContext("webgl");

canvas.width = window.innerWidth;
canvas.height = window.innerHeight;
gl.clearColor(0.051, 0.008, 0.129, 1.0);
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

    vec2 bhPos = vec2(0.5, 0.5);
    vec2 toBH = st - bhPos;
    vec2 toBHc = toBH;
    toBHc.x *= aspect;
    float bhDist = length(toBHc);
    vec2 bhDir = normalize(toBH + vec2(0.0001));
    float bhAngle = atan(toBHc.y, toBHc.x);

    float lensing = 0.008 / (bhDist * bhDist + 0.003);
    lensing = min(lensing, 0.6);
    vec2 lensedST = st + bhDir * lensing;

    vec2 q = lensedST * 2.0;
    float n = noise(q + vec2(
        sin(q.y * 2.0 + u_time * 0.8),
        cos(q.x * 2.0 + u_time * 1.2)
    ));

    vec3 base = vec3(0.051, 0.008, 0.129);
    vec3 glow = vec3(0.082, 0.039, 0.337);
    vec3 color = mix(base, glow, pow(n, 0.8));

    vec2 diskUV = toBHc;
    diskUV.y *= 2.8;
    float diskDist = length(diskUV);
    float diskAngle = atan(diskUV.y, diskUV.x);

    float spiralNoise = noise(vec2(diskAngle * 4.0 - u_time * 0.4 + diskDist * 15.0, diskDist * 30.0));
    float fineNoise = noise(vec2(diskAngle * 8.0 + u_time * 0.2, diskDist * 50.0 - u_time * 0.5));

    float outerR = 0.22;
    float outerW = 0.05;
    float outerDisk = exp(-(diskDist - outerR) * (diskDist - outerR) / (outerW * outerW));
    float doppler = 0.3 + 0.7 * cos(diskAngle + u_time * 0.4);
    vec3 outerColor = mix(vec3(0.06, 0.02, 0.18), vec3(0.24, 0.11, 0.56), doppler);
    color += outerColor * outerDisk * doppler * (0.5 + spiralNoise * 0.4 + fineNoise * 0.1);

    float midR = 0.15;
    float midW = 0.032;
    float midDisk = exp(-(diskDist - midR) * (diskDist - midR) / (midW * midW));
    vec3 midColor = mix(vec3(0.12, 0.04, 0.34), vec3(0.48, 0.18, 0.74), doppler);
    color += midColor * midDisk * doppler * (0.6 + spiralNoise * 0.3 + fineNoise * 0.1);

    float innerR = 0.09;
    float innerW = 0.02;
    float innerDisk = exp(-(diskDist - innerR) * (diskDist - innerR) / (innerW * innerW));
    float innerDoppler = 0.2 + 0.8 * cos(diskAngle + u_time * 0.7);
    vec3 innerColor = mix(vec3(0.24, 0.1, 0.56), vec3(0.66, 0.4, 0.92), innerDoppler);
    color += innerColor * innerDisk * innerDoppler * (0.7 + fineNoise * 0.3);

    float photonR = 0.06;
    float photon = exp(-(bhDist - photonR) * (bhDist - photonR) / 0.000018) * 0.5;
    float photonDoppler = 0.4 + 0.6 * cos(bhAngle + u_time * 1.2);
    color += vec3(0.4, 0.25, 0.7) * photon * photonDoppler;

    float einsteinR = 0.08;
    float einstein = exp(-(bhDist - einsteinR) * (bhDist - einsteinR) / 0.00006) * 0.15;
    color += vec3(0.0, 0.12, 0.25) * einstein;

    float jetWidth = 0.02;
    float jetX = exp(-toBHc.x * toBHc.x / (jetWidth * jetWidth));
    float jetY = smoothstep(0.0, 0.07, abs(toBHc.y)) * exp(-abs(toBHc.y) * 5.0);
    float jetMask = smoothstep(0.055, 0.08, bhDist);
    float jet = jetX * jetY * jetMask * 0.06;
    color += vec3(0.0, 0.15, 0.35) * jet;

    float horizon = smoothstep(0.05, 0.03, bhDist);
    color *= (1.0 - horizon);

    float redshift = smoothstep(0.03, 0.065, bhDist);
    color *= (0.3 + 0.7 * redshift);

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
