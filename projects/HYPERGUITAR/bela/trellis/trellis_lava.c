/*
 * NeoTrellis 4x4 purple lava lamp for Bela Gem (PocketBeagle 2).
 * Talks raw Adafruit seesaw protocol over /dev/i2c-N - no libraries.
 *
 * Wiring: Gem "I2C"/qwiic connector (or P1 strip SDA/SCL rows):
 *   GND, 3.3V, SDA, SCL -> NeoTrellis GND, 3V, SDA, SCL. Addr 0x2E.
 *
 * Build (on the Bela):  gcc -O2 -o trellis_lava trellis_lava.c -lm
 * Run:                  ./trellis_lava [i2c-bus] [addr]   (default 1 0x2E)
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>

#define SS_STATUS       0x00
#define SS_STATUS_SWRST 0x7F
#define SS_STATUS_HW_ID 0x01
#define SS_NEOPIX       0x0E
#define SS_NEOPIX_PIN   0x01
#define SS_NEOPIX_SPEED 0x02
#define SS_NEOPIX_LEN   0x03
#define SS_NEOPIX_BUF   0x04
#define SS_NEOPIX_SHOW  0x05

#define NPIX 16
#define TRELLIS_PIXEL_PIN 3
#define FPS 30

static int gFd;

static int ssWrite(uint8_t mod, uint8_t fn, const uint8_t *data, int len)
{
	uint8_t buf[64] = { mod, fn };
	if(len > 60) return -1;
	if(len) memcpy(buf + 2, data, len);
	return write(gFd, buf, 2 + len) == 2 + len ? 0 : -1;
}

static int ssRead(uint8_t mod, uint8_t fn, uint8_t *out, int len)
{
	uint8_t reg[2] = { mod, fn };
	if(write(gFd, reg, 2) != 2) return -1;
	usleep(3000);
	return read(gFd, out, len) == len ? 0 : -1;
}

// gamma for LED-linear brightness
static uint8_t gammaLUT[256];

static void buildGamma(void)
{
	for(int i = 0; i < 256; i++)
		gammaLUT[i] = (uint8_t)(powf(i / 255.0f, 2.5f) * 255.0f + 0.5f);
}

/*
 * Purple lava palette, t in 0..1:
 * deep violet -> purple -> magenta -> warm pink glow at the hottest spots
 */
static void palette(float t, uint8_t *r, uint8_t *g, uint8_t *b)
{
	if(t < 0.0f) t = 0.0f;
	if(t > 1.0f) t = 1.0f;
	float R, G, B;
	if(t < 0.5f) { // near-black violet -> saturated purple
		float u = t / 0.5f;
		R = 0.10f + 0.45f * u;
		G = 0.0f;
		B = 0.18f + 0.62f * u;
	} else {       // purple -> magenta/pink bloom
		float u = (t - 0.5f) / 0.5f;
		R = 0.55f + 0.45f * u;
		G = 0.10f * u * u;
		B = 0.80f + 0.10f * u;
	}
	float bright = 0.15f + 0.85f * t; // keep lows dim, lava peaks hot
	*r = gammaLUT[(int)(R * bright * 255.0f)];
	*g = gammaLUT[(int)(G * bright * 255.0f)];
	*b = gammaLUT[(int)(B * bright * 255.0f)];
}

typedef struct { float px, py, r, fx, fy, fr, phx, phy, phr; } Blob;

int main(int argc, char **argv)
{
	int bus = argc > 1 ? atoi(argv[1]) : 1;
	int addr = argc > 2 ? (int)strtol(argv[2], NULL, 0) : 0x2E;

	char dev[32];
	snprintf(dev, sizeof(dev), "/dev/i2c-%d", bus);
	gFd = open(dev, O_RDWR);
	if(gFd < 0) { perror(dev); return 1; }
	if(ioctl(gFd, I2C_SLAVE, addr) < 0) { perror("I2C_SLAVE"); return 1; }

	// reset seesaw, confirm it's alive
	uint8_t one = 0xFF;
	ssWrite(SS_STATUS, SS_STATUS_SWRST, &one, 1);
	usleep(100000);
	uint8_t id = 0;
	if(ssRead(SS_STATUS, SS_STATUS_HW_ID, &id, 1) < 0) {
		fprintf(stderr, "no seesaw at %s addr 0x%02X\n", dev, addr);
		return 1;
	}
	printf("seesaw hw id 0x%02X on %s addr 0x%02X\n", id, dev, addr);

	// neopixel config: pin 3, 800 kHz, 16 px * 3 bytes GRB
	uint8_t pin = TRELLIS_PIXEL_PIN, speed = 1;
	uint8_t len[2] = { 0, NPIX * 3 };
	ssWrite(SS_NEOPIX, SS_NEOPIX_PIN, &pin, 1);
	ssWrite(SS_NEOPIX, SS_NEOPIX_SPEED, &speed, 1);
	ssWrite(SS_NEOPIX, SS_NEOPIX_LEN, len, 2);

	buildGamma();

	// three slow blobs drifting on lissajous paths, radii breathing
	Blob blobs[3] = {
		{ 0,0, 1.4f, 0.11f, 0.07f, 0.045f, 0.0f, 1.1f, 2.0f },
		{ 0,0, 1.1f, 0.05f, 0.13f, 0.06f,  2.1f, 3.9f, 0.7f },
		{ 0,0, 1.7f, 0.08f, 0.045f,0.03f,  4.4f, 0.6f, 4.2f },
	};

	float t = 0.0f;
	const float dt = 1.0f / FPS;
	while(1) {
		float field[NPIX] = { 0 };
		for(int bi = 0; bi < 3; bi++) {
			Blob *bl = &blobs[bi];
			float cx = 1.5f + 1.6f * sinf(2.0f * (float)M_PI * bl->fx * t + bl->phx);
			float cy = 1.5f + 1.6f * sinf(2.0f * (float)M_PI * bl->fy * t + bl->phy);
			float rad = bl->r * (0.8f + 0.25f * sinf(2.0f * (float)M_PI * bl->fr * t + bl->phr));
			for(int y = 0; y < 4; y++)
				for(int x = 0; x < 4; x++) {
					float dx = x - cx, dy = y - cy;
					float d2 = (dx * dx + dy * dy) / (rad * rad);
					field[y * 4 + x] += expf(-d2);
				}
		}

		uint8_t frame[NPIX * 3];
		for(int i = 0; i < NPIX; i++) {
			float v = field[i] * 0.75f;
			v = v / (1.0f + v * 0.6f); // soft knee
			uint8_t r, g, b;
			palette(v, &r, &g, &b);
			frame[i * 3 + 0] = g; // GRB order
			frame[i * 3 + 1] = r;
			frame[i * 3 + 2] = b;
		}

		// buffer writes: 2-byte BE offset + max 24 data bytes per xfer
		for(int off = 0; off < NPIX * 3; off += 24) {
			int chunk = NPIX * 3 - off;
			if(chunk > 24) chunk = 24;
			uint8_t pkt[26] = { (uint8_t)(off >> 8), (uint8_t)off };
			memcpy(pkt + 2, frame + off, chunk);
			ssWrite(SS_NEOPIX, SS_NEOPIX_BUF, pkt, 2 + chunk);
		}
		ssWrite(SS_NEOPIX, SS_NEOPIX_SHOW, NULL, 0);

		t += dt;
		usleep(1000000 / FPS);
	}
	return 0;
}
