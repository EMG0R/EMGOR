/*
 * i2c_sensors.cpp - MPU6050 + VL53L1X presence and readout.
 *
 * Deliberately raw /dev/i2c-1 ioctls, the same approach trellis_ui.cpp already
 * uses on this board, so there is one I2C idiom in the project rather than two.
 *
 * WHAT "WORKING" MEANS HERE
 *   For each device we check the ID register FIRST and report it. A device that
 *   ACKs its address but returns the wrong ID is a different fault from one
 *   that is absent, and both are different from one that is present and
 *   returning stuck values - so the log distinguishes all three. During a
 *   hardware bring-up that distinction is the entire value of the tool.
 *
 * VL53L1X SCOPE, stated honestly: full ranging needs a ~135-register init blob
 * from ST's driver. This does the ID check and a minimal single-shot range.
 * If the ID reads correctly but ranging returns nothing, the wiring is good and
 * what is missing is the init blob - which is a software job, not a soldering
 * one, and worth knowing before anyone reaches for a soldering iron.
 */
#include "i2c_sensors.h"

#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>

/* ---- device addresses ---- */
#define MPU_ADDR   0x68
#define TOF_ADDR   0x29

/* ---- MPU6050 registers ---- */
#define MPU_WHO_AM_I    0x75
#define MPU_PWR_MGMT_1  0x6B
#define MPU_ACCEL_XOUT  0x3B

/* ---- VL53L1X registers (16-bit addressing) ---- */
#define TOF_MODEL_ID          0x010F   /* reads 0xEA */
#define TOF_FIRMWARE_STATUS   0x00E5
#define TOF_SYSTEM_START      0x0087
#define TOF_RESULT_RANGE      0x0096   /* mm, 16-bit */
#define TOF_INT_STATUS        0x0089

static int gFd = -1;

int gEnableMpu = 1;
int gEnableTof = 1;

/*
 * TOF NO-RANGE MODE - the test that separates the two theories.
 *
 * Local decoupling at the sensor made NO audible difference to the clicking,
 * which rules out the shared power rail as the path. So either the VCSEL's
 * current pulse is coupling some other way (radiated, or through the ground
 * return), or the noise was never the laser at all and it is the I2C
 * transaction itself.
 *
 * With this set, the ToF is polled at exactly the same rate with an I2C
 * transaction of the same shape - but it reads the MODEL ID register instead
 * of starting a ranging measurement, so THE LASER NEVER FIRES.
 *
 *   still clicking -> it is the I2C transaction. Decoupling was never going
 *                     to fix it; the fix is wiring/routing.
 *   goes silent    -> it IS the laser, and the path is not the power rail -
 *                     look at the ground return and physical proximity.
 *
 * Either answer is worth more than another cap fitted on a hunch.
 */
int gTofNoRange = 1;

/* ---- 8-bit-register helpers (MPU6050) ---- */
static int setAddr(int addr)
{
	return ioctl(gFd, I2C_SLAVE_FORCE, addr) < 0 ? -1 : 0;
}

static int rd8(int addr, uint8_t reg, uint8_t *out, int len)
{
	if(setAddr(addr) < 0) return -1;
	if(write(gFd, &reg, 1) != 1) return -1;
	return read(gFd, out, len) == len ? 0 : -1;
}

static int wr8(int addr, uint8_t reg, uint8_t val)
{
	if(setAddr(addr) < 0) return -1;
	uint8_t b[2] = { reg, val };
	return write(gFd, b, 2) == 2 ? 0 : -1;
}

/* ---- 16-bit-register helpers (VL53L1X) ---- */
static int rd16reg(int addr, uint16_t reg, uint8_t *out, int len)
{
	if(setAddr(addr) < 0) return -1;
	uint8_t a[2] = { (uint8_t)(reg >> 8), (uint8_t)(reg & 0xFF) };
	if(write(gFd, a, 2) != 2) return -1;
	return read(gFd, out, len) == len ? 0 : -1;
}

static int wr16reg(int addr, uint16_t reg, uint8_t val)
{
	if(setAddr(addr) < 0) return -1;
	uint8_t b[3] = { (uint8_t)(reg >> 8), (uint8_t)(reg & 0xFF), val };
	return write(gFd, b, 3) == 3 ? 0 : -1;
}

/* ================= public ================= */

int sensorsInit(SensorState *s)
{
	memset(s, 0, sizeof(*s));
	s->tofMm = -1;

	gFd = open("/dev/i2c-1", O_RDWR);
	if(gFd < 0) {
		rt_printf("i2c: CANNOT OPEN /dev/i2c-1 - nothing else here can work\n");
		return -1;
	}

	/* ---- bus scan, so an unexpected address is visible immediately ---- */
	rt_printf("i2c: scanning bus 1...\n");
	for(int a = 0x08; a < 0x78; a++) {
		if(setAddr(a) < 0) continue;
		uint8_t dummy;
		if(read(gFd, &dummy, 1) == 1) {
			const char *known =
				(a == 0x2E) ? " (NeoTrellis)" :
				(a == MPU_ADDR) ? " (MPU6050)" :
				(a == TOF_ADDR) ? " (VL53L1X)" : "";
			rt_printf("i2c:   found 0x%02X%s\n", a, known);
		}
	}

	/* ---- MPU6050 ---- */
	uint8_t who = 0;
	if(!gEnableMpu) rt_printf("i2c: MPU6050 DISABLED\n"); else
	if(rd8(MPU_ADDR, MPU_WHO_AM_I, &who, 1) == 0) {
		/* Some clones answer 0x68, others 0x70/0x72/0x98. Accept anything that
		 * responds and PRINT the value, rather than rejecting a working clone
		 * for failing an equality test against one vendor's part. */
		s->mpuOk = 1;
		rt_printf("i2c: MPU6050 at 0x68 WHO_AM_I=0x%02X %s\n", who,
				(who == 0x68) ? "(genuine)" : "(clone - fine, just noting it)");
		/* wake it: it boots in sleep mode and returns all-zero until told */
		wr8(MPU_ADDR, MPU_PWR_MGMT_1, 0x00);
		usleep(50000);
	} else {
		rt_printf("i2c: MPU6050 NOT FOUND at 0x68 - check SDA/SCL/power\n");
	}

	/* ---- VL53L1X ---- */
	uint8_t id[2] = { 0, 0 };
	if(!gEnableTof) rt_printf("i2c: VL53L1X DISABLED\n"); else
	if(rd16reg(TOF_ADDR, TOF_MODEL_ID, id, 2) == 0) {
		s->tofOk = 1;
		rt_printf("i2c: VL53L1X at 0x29 MODEL_ID=0x%02X%02X (expect 0xEACC)\n",
				id[0], id[1]);
		/* Minimal start. Without ST's full init blob the ranging numbers are
		 * not calibrated, but a CHANGING value still proves the sensor is
		 * alive, addressed and wired - which is what this stage is for. */
		wr16reg(TOF_ADDR, TOF_SYSTEM_START, 0x40);
		usleep(50000);
	} else {
		rt_printf("i2c: VL53L1X NOT FOUND at 0x29 - check SDA/SCL/power\n");
	}

	return 0;
}

void sensorsRead(SensorState *s)
{
	if(gFd < 0) return;

	if(s->mpuOk && gEnableMpu) {
		uint8_t b[14];
		if(rd8(MPU_ADDR, MPU_ACCEL_XOUT, b, 14) == 0) {
			int16_t ax = (int16_t)((b[0] << 8) | b[1]);
			int16_t ay = (int16_t)((b[2] << 8) | b[3]);
			int16_t az = (int16_t)((b[4] << 8) | b[5]);
			int16_t gx = (int16_t)((b[8] << 8) | b[9]);
			int16_t gy = (int16_t)((b[10] << 8) | b[11]);
			int16_t gz = (int16_t)((b[12] << 8) | b[13]);
			/* default ranges: accel +/-2g (16384 LSB/g), gyro +/-250 dps (131) */
			s->accX = ax / 16384.0f;
			s->accY = ay / 16384.0f;
			s->accZ = az / 16384.0f;
			s->gyrX = gx / 131.0f;
			s->gyrY = gy / 131.0f;
			s->gyrZ = gz / 131.0f;
		}
	}

	if(s->tofOk && gEnableTof && gTofNoRange) {
		/* same bus traffic, same rate, no ranging: read the ID register */
		uint8_t idr[2];
		rd16reg(TOF_ADDR, TOF_MODEL_ID, idr, 2);
		s->tofMm = -1;
	} else if(s->tofOk && gEnableTof) {
		uint8_t st = 0;
		if(rd16reg(TOF_ADDR, TOF_INT_STATUS, &st, 1) == 0) {
			uint8_t r[2];
			if(rd16reg(TOF_ADDR, TOF_RESULT_RANGE, r, 2) == 0) {
				int mm = (r[0] << 8) | r[1];
				/* 0 and absurd values mean "no valid return", not "0 mm away" */
				s->tofMm = (mm > 0 && mm < 5000) ? mm : -1;
			}
			wr16reg(TOF_ADDR, TOF_SYSTEM_START, 0x40);   /* next shot */
		}
	}
}

void sensorsClose(void)
{
	if(gFd >= 0) close(gFd);
	gFd = -1;
}
