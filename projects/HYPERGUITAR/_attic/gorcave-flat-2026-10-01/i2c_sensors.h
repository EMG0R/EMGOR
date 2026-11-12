/* i2c_sensors.h - MPU6050 + VL53L1X on /dev/i2c-1. See the .cpp for scope. */
#ifndef I2C_SENSORS_H
#define I2C_SENSORS_H

#include <Bela.h>
#include <cstdint>

typedef struct SensorState {
    int   mpuOk;      /* device answered its ID register */
    int   tofOk;
    float accX, accY, accZ;   /* g */
    float gyrX, gyrY, gyrZ;   /* deg/s */
    int   tofMm;              /* -1 = no valid reading */
} SensorState;

/* Per-device enables. The bus is audible on this rig - I2C traffic couples
 * into the audio - so each device has to be switchable independently to find
 * out which one, and at what poll rate, is tolerable. */
extern int gEnableMpu;
extern int gEnableTof;

int  sensorsInit(SensorState *s);   /* scans the bus and reports what it finds */
void sensorsRead(SensorState *s);   /* call from an aux task, NOT the audio thread */
void sensorsClose(void);
void tofSetRanging(int on);   /* laser on/off, poll rate untouched */

#endif
