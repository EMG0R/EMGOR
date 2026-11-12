#include <stdio.h>
#include "daisy_seed.h"
#include "dev/oled_ssd130x.h"
#include "VL53L1X_mini.h"
#include "fatfs.h"

using namespace daisy;
using MyOled = OledDisplay<SSD130x4WireSpi128x64Driver>;

DaisySeed    hw;
MyOled       display;
VL53L1X_Mini lidar;
SdmmcHandler sd;
FatFSInterface fsi;

// ============================================================
// Generic bit-bang I2C bus (open-drain, non-blocking, no HAL)
// ============================================================
struct BBus {
    dsy_gpio scl_g, sda_g;

    void init(dsy_gpio_pin sp, dsy_gpio_pin dp) {
        scl_g = {sp, DSY_GPIO_MODE_OUTPUT_OD, DSY_GPIO_NOPULL};
        sda_g = {dp, DSY_GPIO_MODE_OUTPUT_OD, DSY_GPIO_NOPULL};
        dsy_gpio_init(&scl_g); dsy_gpio_init(&sda_g);
        scl(1); sda(1); System::Delay(2);
    }
    void scl(int v) { dsy_gpio_write(&scl_g, v); }
    void sda(int v) { dsy_gpio_write(&sda_g, v); }
    void hd()       { System::DelayUs(5); }

    void start() { sda(1);hd();scl(1);hd();sda(0);hd();scl(0);hd(); }
    void stop()  { sda(0);hd();scl(1);hd();sda(1);hd(); }

    bool byteW(uint8_t b) {
        for(int i=7;i>=0;i--){sda((b>>i)&1);hd();scl(1);hd();scl(0);}
        sda(1);hd();scl(1);hd();
        bool ack=(dsy_gpio_read(&sda_g)==0);
        scl(0);hd();return ack;
    }
    uint8_t byteR(bool nack) {
        uint8_t v=0; sda(1);
        for(int i=7;i>=0;i--){hd();scl(1);hd();if(dsy_gpio_read(&sda_g))v|=(1<<i);scl(0);}
        sda(nack?1:0);hd();scl(1);hd();scl(0);hd();sda(1);
        return v;
    }

    bool probe(uint8_t a7) { start();bool r=byteW((a7<<1)|0);stop();hd();return r; }

    void send(uint8_t a7, const uint8_t* d, uint8_t n) {
        start();byteW((a7<<1)|0);
        for(uint8_t i=0;i<n;i++) byteW(d[i]);
        stop();hd();
    }

    // 8-bit register write (MPU9250/AK8963)
    void w8(uint8_t a7, uint8_t reg, uint8_t val) {
        uint8_t b[2]={reg,val}; send(a7,b,2);
    }
    // 8-bit register read with repeated start
    uint8_t r8(uint8_t a7, uint8_t reg) {
        start();byteW((a7<<1)|0);byteW(reg);
        start();byteW((a7<<1)|1);
        uint8_t v=byteR(true); stop();hd(); return v;
    }
    // 16-bit big-endian read from 8-bit register
    int16_t r16be(uint8_t a7, uint8_t reg) {
        start();byteW((a7<<1)|0);byteW(reg);
        start();byteW((a7<<1)|1);
        uint8_t hi=byteR(false), lo=byteR(true);
        stop();hd(); return (int16_t)((hi<<8)|lo);
    }
    // 16-bit address write (VL53L1X)
    void w16a(uint8_t a7, uint16_t reg, uint8_t* d, uint8_t n) {
        start();byteW((a7<<1)|0);byteW(reg>>8);byteW(reg&0xFF);
        for(uint8_t i=0;i<n;i++) byteW(d[i]);
        stop();hd();
    }
    // 16-bit address read with repeated start (VL53L1X)
    void r16a(uint8_t a7, uint16_t reg, uint8_t* d, uint8_t n) {
        start();byteW((a7<<1)|0);byteW(reg>>8);byteW(reg&0xFF);
        start();byteW((a7<<1)|1);
        for(uint8_t i=0;i<n;i++) d[i]=byteR(i==n-1);
        stop();hd();
    }
    // Seesaw register read: STOP + 500µs + new START (seesaw requires this)
    uint8_t ssR1(uint8_t a7, uint8_t base, uint8_t reg) {
        uint8_t cmd[2]={base,reg}; send(a7,cmd,2);
        System::DelayUs(500);
        start();byteW((a7<<1)|1);
        uint8_t v=byteR(true); stop();hd(); return v;
    }
    void ssRN(uint8_t a7, uint8_t base, uint8_t reg, uint8_t* out, uint8_t n) {
        uint8_t cmd[2]={base,reg}; send(a7,cmd,2);
        System::DelayUs(500);
        start();byteW((a7<<1)|1);
        for(uint8_t i=0;i<n;i++) out[i]=byteR(i==n-1);
        stop();hd();
    }
};

static BBus tbus; // D11=SCL, D12=SDA — NeoTrellis
static BBus sbus; // D13=SCL, D14=SDA — sensors

// VL53L1X callbacks (called from VL53L1X_mini.cpp)
void vl53_write_reg(uint16_t reg, uint8_t* d, uint8_t n) { sbus.w16a(0x29,reg,d,n); }
void vl53_read_reg(uint16_t reg, uint8_t* d, uint8_t n)  { sbus.r16a(0x29,reg,d,n); }

// ============================================================
// NeoTrellis helpers
// ============================================================
static const uint8_t NT_SKEY[16] = {0,1,2,3, 8,9,10,11, 16,17,18,19, 24,25,26,27};
static const uint8_t TREL = 0x2E;

static void wheel(uint8_t p, uint8_t& r, uint8_t& g, uint8_t& b) {
    if(p<85)       {r=p*3;    g=255-p*3;b=0;}
    else if(p<170) {p-=85; r=255-p*3;g=0;   b=p*3;}
    else           {p-=170;r=0;      g=p*3; b=255-p*3;}
}

// Send pixel buffer in 8-pixel chunks (28 bytes each, within seesaw 32-byte wire limit)
static void ntShow(uint8_t* grb48) {
    for(uint8_t chunk=0;chunk<2;chunk++) {
        uint16_t off = chunk*24;
        uint8_t pkt[28];
        pkt[0]=0x0E;pkt[1]=0x04;pkt[2]=off>>8;pkt[3]=off&0xFF;
        for(uint8_t i=0;i<24;i++) pkt[4+i]=grb48[off+i];
        tbus.send(TREL,pkt,28);
    }
    const uint8_t sh[]={0x0E,0x05,0x01}; tbus.send(TREL,sh,3);
}

// ============================================================
// Encoders (polled in AudioCallback)
// ============================================================
struct Enc { dsy_gpio a,b,sw; uint8_t lab,psw; volatile int8_t delta; volatile bool clicked; };
static Enc enc1, enc2;
static bool enc_ready = false;
static void ePoll(Enc& e) {
    static const int8_t lut[4][4]={{0,-1,+1,0},{+1,0,0,-1},{-1,0,0,+1},{0,+1,-1,0}};
    uint8_t ab=(dsy_gpio_read(&e.a)<<1)|dsy_gpio_read(&e.b);
    if(ab!=e.lab){e.delta+=lut[e.lab][ab];e.lab=ab;}
    uint8_t sw=dsy_gpio_read(&e.sw);
    if(e.psw&&!sw) e.clicked=true; e.psw=sw;
}
static void eInit(Enc& e, dsy_gpio_pin pa, dsy_gpio_pin pb, dsy_gpio_pin ps) {
    e.a={pa,DSY_GPIO_MODE_INPUT,DSY_GPIO_PULLUP};
    e.b={pb,DSY_GPIO_MODE_INPUT,DSY_GPIO_PULLUP};
    e.sw={ps,DSY_GPIO_MODE_INPUT,DSY_GPIO_PULLUP};
    dsy_gpio_init(&e.a);dsy_gpio_init(&e.b);dsy_gpio_init(&e.sw);
    e.lab=(dsy_gpio_read(&e.a)<<1)|dsy_gpio_read(&e.b);
    e.psw=1;e.delta=0;e.clicked=false;
}

static volatile float mic_amp = 0.f;
void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t sz) {
    if(enc_ready){ePoll(enc1);ePoll(enc2);}
    float pk=0.f;
    for(size_t i=0;i<sz;i++){float s=in[0][i];if(s<0)s=-s;if(s>pk)pk=s;out[0][i]=0;out[1][i]=0;}
    mic_amp=pk;
}

// ============================================================
// Formatting
// ============================================================
static int fmtf(char* o, float v) {
    bool neg=v<0; if(neg) v=-v;
    int w=(int)v, f=(int)((v-w)*10+.5f);
    if(f>=10){w++;f=0;}
    return sprintf(o,"%c%d.%d",neg?'-':'+',w,f);
}
static char row[24];
static void orow(int y, const char* s) {
    display.SetCursor(0,y); display.WriteString(s,Font_6x8,true);
}

int main(void)
{
    hw.Configure(); hw.Init();

    MyOled::Config oc;
    oc.driver_config.transport_config.pin_config.dc    = hw.GetPin(3);
    oc.driver_config.transport_config.pin_config.reset = hw.GetPin(4);
    display.Init(oc);
    display.Fill(false);
    display.SetCursor(0,0); display.WriteString("INIT...",Font_6x8,true);
    display.Update();

    // Sensor bus: D13=SCL(PB6), D14=SDA(PB7)
    sbus.init(hw.GetPin(13), hw.GetPin(14));
    // Trellis bus: D11=SCL(PB8), D12=SDA(PB9)
    tbus.init(hw.GetPin(11), hw.GetPin(12));

    // MPU9250 — addr 0x68
    sbus.w8(0x68, 0x6B, 0x00); System::Delay(10); // wake
    bool imu_ok = (sbus.r8(0x68, 0x75) == 0x71);

    // AK8963 via MPU bypass — addr 0x0C
    sbus.w8(0x68, 0x37, 0x02); System::Delay(10);
    sbus.w8(0x0C, 0x0A, 0x00); System::Delay(10);
    bool mag_ok = (sbus.r8(0x0C, 0x00) == 0x48);
    sbus.w8(0x0C, 0x0A, 0x16); System::Delay(10);

    // VL53L1X — addr 0x29
    bool lidar_ok = lidar.init();
    if(lidar_ok) lidar.startContinuous(100);

    // NeoTrellis — SW reset, verify HW ID, init neopixels + keypad
    bool trel_ok = false;
    { const uint8_t rst[]={0x00,0x7F,0xFF}; tbus.send(TREL,rst,3); }
    System::Delay(500);
    uint8_t hwid = tbus.ssR1(TREL, 0x00, 0x00);
    trel_ok = (hwid == 0x55);
    if(trel_ok) {
        const uint8_t p[]  ={0x0E,0x01,3};    tbus.send(TREL,p,3);
        const uint8_t sp[] ={0x0E,0x02,0x01};  tbus.send(TREL,sp,3);
        const uint8_t bl[] ={0x0E,0x03,0,48};  tbus.send(TREL,bl,4);
        System::Delay(10);
        // Enable keypad events for all 16 keys
        for(uint8_t k=0;k<16;k++) {
            uint8_t ev[]={0x10,0x01,NT_SKEY[k],0x1C}; tbus.send(TREL,ev,4);
        }
    }

    // SD Card — 1-bit SDMMC (avoids D2/PC11 conflict with enc1 SW)
    // Pins: PC8(D5)=D0, PC12(D7)=CK, PD2=CMD (solder pad)
    bool sd_ok = false;
    {
        SdmmcHandler::Config sd_cfg;
        sd_cfg.Defaults();
        sd_cfg.width = SdmmcHandler::BusWidth::BITS_1;
        sd.Init(sd_cfg);
        fsi.Init(FatFSInterface::Config::MEDIA_SD);
        sd_ok = (f_mount(&fsi.GetSDFileSystem(), "/", 1) == FR_OK);
    }

    // Encoders D0/D1/D2 and D24/D16/D17
    eInit(enc1, hw.GetPin(0),  hw.GetPin(1),  hw.GetPin(2));
    eInit(enc2, hw.GetPin(24), hw.GetPin(16), hw.GetPin(17));
    enc_ready = true;

    // FSR on D15
    AdcChannelConfig adc; adc.InitSingle(hw.GetPin(15));
    hw.adc.Init(&adc,1); hw.adc.Start();

    hw.SetAudioBlockSize(64);
    hw.StartAudio(AudioCallback);

    // Show init result
    display.Fill(false);
    sprintf(row,"IMU:%s MAG:%s LDR:%s",imu_ok?"OK":"--",mag_ok?"OK":"--",lidar_ok?"OK":"--"); orow(0,row);
    sprintf(row,"TRL:%s SD:%s",trel_ok?"OK":"--",sd_ok?"OK":"--"); orow(9,row);
    display.Update(); System::Delay(1500);

    // NeoTrellis lava lamp state
    // Each pixel drifts at a slightly different speed
    static const uint8_t DRIFT[16]={1,3,2,1, 2,1,3,2, 1,2,1,3, 3,1,2,1};
    uint8_t pphase[16]={0,16,32,48, 64,80,96,112, 128,144,160,176, 192,208,224,240};

    uint16_t dist=0;
    float mx=0,my=0,mz=0;
    char e1s[6]="--", e2s[6]="--";
    uint32_t e1until=0, e2until=0;

    while(1)
    {
        // Encoder events — time-based display (250ms)
        uint32_t now = System::GetNow();
        if(enc1.delta>0)     {sprintf(e1s,">");  enc1.delta=0;e1until=now+250;}
        else if(enc1.delta<0){sprintf(e1s,"<");  enc1.delta=0;e1until=now+250;}
        if(enc1.clicked)     {sprintf(e1s,"CLK");enc1.clicked=false;e1until=now+250;}
        if(now>=e1until) sprintf(e1s,"--");

        if(enc2.delta>0)     {sprintf(e2s,">");  enc2.delta=0;e2until=now+250;}
        else if(enc2.delta<0){sprintf(e2s,"<");  enc2.delta=0;e2until=now+250;}
        if(enc2.clicked)     {sprintf(e2s,"CLK");enc2.clicked=false;e2until=now+250;}
        if(now>=e2until) sprintf(e2s,"--");

        // IMU
        float ax=0,ay=0,az=0,gx=0,gy=0,gz=0;
        if(imu_ok) {
            ax=sbus.r16be(0x68,0x3B)/16384.f; ay=sbus.r16be(0x68,0x3D)/16384.f; az=sbus.r16be(0x68,0x3F)/16384.f;
            gx=sbus.r16be(0x68,0x43)/131.f;   gy=sbus.r16be(0x68,0x45)/131.f;   gz=sbus.r16be(0x68,0x47)/131.f;
        }

        // Mag
        if(mag_ok && (sbus.r8(0x0C,0x02)&0x01)) {
            mx=sbus.r16be(0x0C,0x03)*0.15f;
            my=sbus.r16be(0x0C,0x05)*0.15f;
            mz=sbus.r16be(0x0C,0x07)*0.15f;
            sbus.r8(0x0C,0x09); // clear ST2
        }

        // Lidar — non-blocking: only read if data is ready
        if(lidar_ok && lidar.dataReady()) dist=lidar.read();

        // FSR median-3
        uint16_t fsr;
        { static uint16_t b[3]={}; static uint8_t idx=0;
          b[idx]=(uint16_t)(hw.adc.GetFloat(0)*4095.f); idx=(idx+1)%3;
          uint16_t a=b[0],c=b[1],d=b[2];
          if(a>c){uint16_t t=a;a=c;c=t;}
          if(c>d){uint16_t t=c;c=d;d=t;}
          if(a>c){uint16_t t=a;a=c;c=t;}
          fsr=c; }

        float amp=mic_amp;

        // NeoTrellis: lava lamp only (no keypad reads — keeps seesaw bus state clean)
        if(trel_ok) {
            for(uint8_t i=0;i<16;i++) pphase[i]+=DRIFT[i];
            uint8_t grb[48];
            for(uint8_t i=0;i<16;i++){
                uint8_t r,g,b;
                wheel(pphase[i],r,g,b);
                r>>=1; g>>=1; b>>=1;
                grb[i*3]=g; grb[i*3+1]=r; grb[i*3+2]=b;
            }
            ntShow(grb);
        }

        // OLED — 7 rows
        display.Fill(false);
        int n=0;
        row[n++]='A'; n+=fmtf(row+n,ax); row[n++]=' '; n+=fmtf(row+n,ay); row[n++]=' '; n+=fmtf(row+n,az); row[n]=0; orow(0,row);
        n=0;
        row[n++]='G'; n+=fmtf(row+n,gx); row[n++]=' '; n+=fmtf(row+n,gy); row[n++]=' '; n+=fmtf(row+n,gz); row[n]=0; orow(9,row);
        n=0;
        row[n++]='M'; n+=fmtf(row+n,mx); row[n++]=' '; n+=fmtf(row+n,my); row[n++]=' '; n+=fmtf(row+n,mz); row[n]=0; orow(18,row);
        sprintf(row,"D:%4dmm S:%02X",dist,lidar.lastStatus()); orow(27,row);
        sprintf(row,"E1:%-3s E2:%-3s",e1s,e2s); orow(36,row);
        { int f=(int)(amp*19); if(f>19)f=19; row[0]='['; for(int i=0;i<19;i++)row[1+i]=(i<f)?'#':'.'; row[20]=']'; row[21]=0; } orow(45,row);
        sprintf(row,"F:%4d SD:%s",fsr,sd_ok?"OK":"--"); orow(54,row);
        display.Update();

        System::Delay(20);
    }
}
