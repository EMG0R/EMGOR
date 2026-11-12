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
#define MPU_SMPLRT_DIV  0x19
#define MPU_CONFIG      0x1A   /* DLPF */
#define MPU_GYRO_CONFIG 0x1B
#define MPU_ACCEL_CFG   0x1C

/* ---- VL53L1X registers (16-bit addressing) ---- */
#define TOF_MODEL_ID          0x010F   /* reads 0xEACC */
#define TOF_FIRMWARE_STATUS   0x00E5
#define TOF_SYSTEM_START      0x0087
#define TOF_RESULT_RANGE      0x0096   /* mm, 16-bit */
#define TOF_INT_STATUS        0x0089
#define TOF_GPIO_HV_STATUS    0x0031   /* data-ready lives here */
#define TOF_GPIO_HV_MUX_CTRL  0x0030   /* bit 4 sets the interrupt POLARITY */
#define TOF_CLEAR_INT         0x0086
#define TOF_CFG_START         0x002D   /* first byte of the init blob */
#define TOF_OSC_CAL_VAL       0x0022
#define TOF_INTERMEAS_PERIOD  0x006C   /* 32-bit */
#define TOF_TIMEOUT_MACROP_A  0x005E   /* 16-bit */
#define TOF_TIMEOUT_MACROP_B  0x0061   /* 16-bit */
#define TOF_VHV_LOOP_BOUND    0x0008
#define TOF_RANGE_STATUS      0x0089

/*
 * ST's default configuration blob: registers 0x2D..0x87 inclusive, 91 bytes.
 * This is the thing whose absence made the sensor answer its ID correctly and
 * then return nothing forever - it powers up UNCONFIGURED, and no amount of
 * poking SYSTEM_START gets a distance out of it until this is written.
 *
 * Taken from ST's VL53L1X ULD (VL53L1X_api.c, VL51L1X_DEFAULT_CONFIGURATION).
 * Most entries are marked "not user-modifiable" by ST; the ones we do change
 * afterwards (timing budget, inter-measurement period) are written by name
 * below rather than by patching bytes in here.
 */
static const uint8_t TOF_DEFAULT_CFG[] = {
	0x00, /* 0x2d */ 0x00, /* 0x2e */ 0x00, /* 0x2f */ 0x01, /* 0x30 */
	0x02, /* 0x31 */ 0x00, /* 0x32 */ 0x02, /* 0x33 */ 0x08, /* 0x34 */
	0x00, /* 0x35 */ 0x08, /* 0x36 */ 0x10, /* 0x37 */ 0x01, /* 0x38 */
	0x01, /* 0x39 */ 0x00, /* 0x3a */ 0x00, /* 0x3b */ 0x00, /* 0x3c */
	0x00, /* 0x3d */ 0xff, /* 0x3e */ 0x00, /* 0x3f */ 0x0f, /* 0x40 */
	0x00, /* 0x41 */ 0x00, /* 0x42 */ 0x00, /* 0x43 */ 0x00, /* 0x44 */
	0x00, /* 0x45 */ 0x20, /* 0x46 : int on new sample ready */
	0x0b, /* 0x47 */ 0x00, /* 0x48 */ 0x00, /* 0x49 */ 0x02, /* 0x4a */
	0x0a, /* 0x4b */ 0x21, /* 0x4c */ 0x00, /* 0x4d */ 0x00, /* 0x4e */
	0x05, /* 0x4f */ 0x00, /* 0x50 */ 0x00, /* 0x51 */ 0x00, /* 0x52 */
	0x00, /* 0x53 */ 0xc8, /* 0x54 */ 0x00, /* 0x55 */ 0x00, /* 0x56 */
	0x38, /* 0x57 */ 0xff, /* 0x58 */ 0x01, /* 0x59 */ 0x00, /* 0x5a */
	0x08, /* 0x5b */ 0x00, /* 0x5c */ 0x00, /* 0x5d */ 0x01, /* 0x5e */
	0xdb, /* 0x5f */ 0x0f, /* 0x60 */ 0x01, /* 0x61 */ 0xf1, /* 0x62 */
	0x0d, /* 0x63 */ 0x01, /* 0x64 */ 0x68, /* 0x65 */ 0x00, /* 0x66 */
	0x80, /* 0x67 */ 0x08, /* 0x68 */ 0xb8, /* 0x69 */ 0x00, /* 0x6a */
	0x00, /* 0x6b */ 0x00, /* 0x6c */ 0x00, /* 0x6d */ 0x0f, /* 0x6e */
	0x89, /* 0x6f */ 0x00, /* 0x70 */ 0x00, /* 0x71 */ 0x00, /* 0x72 */
	0x00, /* 0x73 */ 0x00, /* 0x74 */ 0x00, /* 0x75 */ 0x00, /* 0x76 */
	0x01, /* 0x77 */ 0x0f, /* 0x78 */ 0x0d, /* 0x79 */ 0x0e, /* 0x7a */
	0x0e, /* 0x7b */ 0x00, /* 0x7c */ 0x00, /* 0x7d */ 0x02, /* 0x7e */
	0xc7, /* 0x7f */ 0xff, /* 0x80 */ 0x9B, /* 0x81 */ 0x00, /* 0x82 */
	0x00, /* 0x83 */ 0x00, /* 0x84 */ 0x01, /* 0x85 */ 0x01, /* 0x86 */
	0x40  /* 0x87 : start ranging */
};

static_assert(sizeof(TOF_DEFAULT_CFG) == 0x87 - 0x2D + 1,
	"VL53L1X config blob must cover 0x2D..0x87 exactly");

static int gFd = -1;
static int gTofIntPol = 1;   /* active level of the data-ready bit */

int gEnableMpu = 1;
int gEnableTof = 1;

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

/* ST's blob is a single contiguous burst write starting at 0x2D. Writing it
 * byte-by-byte also works but is 91 transactions instead of 1 - and on this
 * rig bus traffic is audible, so it goes out in one. */
static int wr16blk(int addr, uint16_t reg, const uint8_t *data, int len)
{
	if(setAddr(addr) < 0) return -1;
	uint8_t b[128];
	if(len + 2 > (int)sizeof(b)) return -1;
	b[0] = (uint8_t)(reg >> 8);
	b[1] = (uint8_t)(reg & 0xFF);
	memcpy(b + 2, data, len);
	return write(gFd, b, len + 2) == len + 2 ? 0 : -1;
}

static int wr16word(int addr, uint16_t reg, uint16_t val)
{
	uint8_t d[2] = { (uint8_t)(val >> 8), (uint8_t)(val & 0xFF) };
	return wr16blk(addr, reg, d, 2);
}

static int wr16dword(int addr, uint16_t reg, uint32_t val)
{
	uint8_t d[4] = { (uint8_t)(val >> 24), (uint8_t)(val >> 16),
	                 (uint8_t)(val >> 8),  (uint8_t)(val & 0xFF) };
	return wr16blk(addr, reg, d, 4);
}

static int rd16word(int addr, uint16_t reg, uint16_t *out)
{
	uint8_t d[2];
	if(rd16reg(addr, reg, d, 2) < 0) return -1;
	*out = (uint16_t)((d[0] << 8) | d[1]);
	return 0;
}

/*
 * ===================== THE POINT OF ALL THIS =====================
 * The old code wrote SYSTEM_START on EVERY poll, which aborts and restarts a
 * ranging measurement 15 times a second. That is three round trips per tick
 * plus a laser restart locked to our poll rate - and it never returned a
 * distance, because the sensor was never configured in the first place.
 *
 * Continuous mode starts ranging ONCE. After that a poll is a single one-byte
 * read of the data-ready register. For comparison, the MPU6050 reads 14 bytes
 * every tick on the same bus at the same rate and is measurably silent, so
 * the configured lidar ends up with a SMALLER bus footprint than a device we
 * have already proven inaudible - at full speed, with no rate reduction.
 */
static int tofStartContinuous(int periodMs)
{
	/* 1. the configuration the sensor never had */
	if(wr16blk(TOF_ADDR, TOF_CFG_START, TOF_DEFAULT_CFG,
			(int)sizeof(TOF_DEFAULT_CFG)) < 0) {
		rt_printf("i2c: VL53L1X config write FAILED\n");
		return -1;
	}

	/* 2. ST's boot dance: one throwaway range, then clear and stop. Skipping
	 *    this leaves the first real measurement uncalibrated. */
	wr16reg(TOF_ADDR, TOF_SYSTEM_START, 0x40);
	for(int i = 0; i < 100; i++) {          /* <=1 s, then give up */
		uint8_t st = 0;
		if(rd16reg(TOF_ADDR, TOF_GPIO_HV_STATUS, &st, 1) == 0
				&& (st & 1) == gTofIntPol)
			break;
		usleep(10000);
	}
	wr16reg(TOF_ADDR, TOF_CLEAR_INT, 0x01);
	wr16reg(TOF_ADDR, TOF_SYSTEM_START, 0x00);
	wr16reg(TOF_ADDR, TOF_VHV_LOOP_BOUND, 0x09);
	wr16reg(TOF_ADDR, 0x000B, 0x00);

	/*
	 * 3. Timing budget 50 ms, long distance mode (up to ~4 m).
	 *
	 * THESE TWO VALUES ARE MEASURED-GOOD ON THIS UNIT (397-413 mm observed).
	 * Do not change them to chase laser duty cycle. Both attempts failed and
	 * each returned mm=0/1 with a VALID status, i.e. silently broken rather
	 * than obviously broken:
	 *   - short mode + 20 ms budget (0x001E/0x0022)  -> garbage
	 *   - long mode  + 33 ms budget (0x0060/0x006E)  -> garbage
	 * The timeout registers are not independent of the VCSEL period and
	 * SD_CONFIG registers; changing one set without the other de-tunes the
	 * sensor. Any future change needs ST's full SetTimingBudget +
	 * SetDistanceMode sequence, not a two-register edit.
	 */
	wr16word(TOF_ADDR, TOF_TIMEOUT_MACROP_A, 0x00AD);
	wr16word(TOF_ADDR, TOF_TIMEOUT_MACROP_B, 0x00C6);

	/* 4. inter-measurement period, in units of the sensor's own oscillator */
	uint16_t osc = 0;
	if(rd16word(TOF_ADDR, TOF_OSC_CAL_VAL, &osc) == 0 && (osc & 0x3FF)) {
		uint32_t pll = (uint32_t)(osc & 0x3FF);
		wr16dword(TOF_ADDR, TOF_INTERMEAS_PERIOD,
				(uint32_t)(pll * periodMs * 1.075f));
	}

	/* 4b. INTERRUPT POLARITY. Data-ready is "bit 0 of 0x31 equals the active
	 *     polarity", and that polarity is configurable - it is NOT always 0.
	 *     Assuming a fixed value is why the first attempt read -1 forever:
	 *     the ready test never once came true, so the range was never read. */
	uint8_t mux = 0;
	if(rd16reg(TOF_ADDR, TOF_GPIO_HV_MUX_CTRL, &mux, 1) == 0)
		gTofIntPol = ((mux >> 4) & 1) ? 0 : 1;
	rt_printf("i2c: VL53L1X interrupt polarity = %d\n", gTofIntPol);

	/* 5. and now start ranging - ONCE, for the lifetime of the program */
	if(wr16reg(TOF_ADDR, TOF_SYSTEM_START, 0x40) < 0) return -1;
	rt_printf("i2c: VL53L1X CONTINUOUS ranging started "
			"(50 ms budget, %d ms period, long mode)\n", periodMs);
	return 0;
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
	/* 2026-11-01 tier-A: bound a hung transfer to 20 ms (units of 10 ms), 1 retry */
	ioctl(gFd, I2C_TIMEOUT, 2);
	ioctl(gFd, I2C_RETRIES, 1);

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
		/*
		 * FULL CONFIGURATION, not just a wake. The previous version wrote
		 * PWR_MGMT_1 = 0 and inherited every other default, which worked but
		 * left two things worth fixing:
		 *
		 *  - CLOCK. 0x00 selects the internal 8 MHz RC oscillator, which
		 *    drifts with temperature. InvenSense's own guidance is to run off
		 *    the gyro X PLL (0x01) instead - same power state, steadier rate.
		 *  - DLPF. The default is 0: 260 Hz bandwidth, i.e. maximum noise.
		 *    For a gesture/modulation source that is all jitter and no signal.
		 *    DLPF 3 gives ~44 Hz, far above any gesture and far below the
		 *    hash, so the values stop twitching at rest.
		 *
		 * The ranges are written EXPLICITLY even though the values chosen are
		 * the power-on defaults, because the scale factors below (16384 LSB/g,
		 * 131 LSB/dps) silently depend on them - an inherited default that
		 * some future init changes would corrupt every reading with no error.
		 */
		wr8(MPU_ADDR, MPU_PWR_MGMT_1, 0x01);    /* wake, clock = gyro X PLL */
		usleep(50000);
		wr8(MPU_ADDR, MPU_CONFIG,      0x03);   /* DLPF ~44 Hz */
		wr8(MPU_ADDR, MPU_SMPLRT_DIV,  0x09);   /* 1 kHz / 10 = 100 Hz */
		wr8(MPU_ADDR, MPU_GYRO_CONFIG, 0x00);   /* +/-250 dps  -> 131 LSB/dps */
		wr8(MPU_ADDR, MPU_ACCEL_CFG,   0x00);   /* +/-2 g      -> 16384 LSB/g */
		usleep(10000);

		/* Prove the wake took and the part is returning live data, rather
		 * than trusting a write that may have been NAKed. A sleeping MPU
		 * answers its WHO_AM_I perfectly well and then reads all-zero
		 * forever, which looks identical to "mounted but not moving". */
		uint8_t pm = 0xFF;
		rd8(MPU_ADDR, MPU_PWR_MGMT_1, &pm, 1);
		uint8_t chk[14];
		if(rd8(MPU_ADDR, MPU_ACCEL_XOUT, chk, 14) == 0) {
			int16_t az0 = (int16_t)((chk[4] << 8) | chk[5]);
			rt_printf("i2c: MPU6050 PWR_MGMT_1=0x%02X accZ=%.2fg %s\n",
					pm, az0 / 16384.0f,
					(az0 == 0) ? "*** READS ZERO - still asleep? ***"
					           : "(live)");
		}
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
		/* Wait for firmware boot before configuring - writing the blob into a
		 * still-booting sensor is how you get a device that ACKs everything
		 * and ranges nothing. */
		for(int i = 0; i < 100; i++) {
			uint8_t fw = 0;
			if(rd16reg(TOF_ADDR, TOF_FIRMWARE_STATUS, &fw, 1) == 0 && (fw & 1))
				break;
			usleep(10000);
		}
		if(tofStartContinuous(100) < 0) {
			rt_printf("i2c: VL53L1X init FAILED - no ranging\n");
			s->tofOk = 0;
		}
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

	if(s->tofOk && gEnableTof) {
		/*
		 * THE CHEAP PATH. Most ticks this is the only thing that happens: one
		 * byte read, less traffic than the MPU's 14. The range is only read on
		 * the ticks where the sensor says a NEW measurement is ready, and
		 * SYSTEM_START is never touched again - ranging is already running.
		 */
		uint8_t rdy = 0;
		if(rd16reg(TOF_ADDR, TOF_GPIO_HV_STATUS, &rdy, 1) == 0
				&& (rdy & 1) == gTofIntPol) {
			uint8_t r[2];
			uint8_t status = 0;
			if(rd16reg(TOF_ADDR, TOF_RESULT_RANGE, r, 2) == 0) {
				int mm = (r[0] << 8) | r[1];
				rd16reg(TOF_ADDR, TOF_RANGE_STATUS, &status, 1);
				/* ST packs the real status into bits 4:1; 4 and 9 are the two
				 * that mean "this number is trustworthy". Anything else is a
				 * signal/sigma failure and must not be treated as a distance
				 * or the modulation it drives will jump to garbage. */
				int rs = (status & 0x1F) >> 1;
				int good = (rs == 4 || rs == 9) || status == 0;
				s->tofMm = (good && mm > 0 && mm < 4000) ? mm : -1;
				static int dbg = 0;
				if(dbg < 5) {
					rt_printf("i2c: VL53L1X raw mm=%d status=0x%02X rs=%d -> %d\n",
							mm, status, rs, s->tofMm);
					dbg++;
				}
			}
			wr16reg(TOF_ADDR, TOF_CLEAR_INT, 0x01);   /* arm the next one */
		}
	}
}

/* Stop or restart the laser without touching the poll - see HGSET tofnorange */
void tofSetRanging(int on)
{
	if(gFd < 0) return;
	wr16reg(TOF_ADDR, TOF_SYSTEM_START, on ? 0x40 : 0x00);
	if(on) wr16reg(TOF_ADDR, TOF_CLEAR_INT, 0x01);
	rt_printf("i2c: VL53L1X ranging %s (poll unchanged)\n", on ? "ON" : "OFF");
}

void sensorsClose(void)
{
	if(gFd >= 0) close(gFd);
	gFd = -1;
}
