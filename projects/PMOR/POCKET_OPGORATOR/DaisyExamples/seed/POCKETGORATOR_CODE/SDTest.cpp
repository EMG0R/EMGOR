// SD card bit-bang SPI test
// Pins: D25=MISO, D26=CLK, D27=MOSI, D28=CS
// Shows init result on OLED. Expects FAT32-formatted card.

#include <stdio.h>
#include "daisy_seed.h"
#include "dev/oled_ssd130x.h"

using namespace daisy;
using MyOled = OledDisplay<SSD130x4WireSpi128x64Driver>;

DaisySeed hw;
MyOled    display;

static dsy_gpio miso_g, clk_g, mosi_g, cs_g;

void showText(const char* l1, const char* l2 = nullptr)
{
    display.Fill(false);
    display.SetCursor(0, 8);
    display.WriteString(l1, Font_11x18, true);
    if(l2) { display.SetCursor(0, 34); display.WriteString(l2, Font_11x18, true); }
    display.Update();
}

static uint8_t spi_byte(uint8_t out)
{
    uint8_t in = 0;
    for(int i = 7; i >= 0; i--)
    {
        dsy_gpio_write(&clk_g, 0);
        dsy_gpio_write(&mosi_g, (out >> i) & 1);
        System::DelayUs(2);
        dsy_gpio_write(&clk_g, 1);
        System::DelayUs(2);
        in = (in << 1) | dsy_gpio_read(&miso_g);
    }
    return in;
}

static uint8_t sd_cmd(uint8_t cmd, uint32_t arg)
{
    spi_byte(0x40 | cmd);
    spi_byte(arg >> 24);
    spi_byte(arg >> 16);
    spi_byte(arg >> 8);
    spi_byte(arg);
    uint8_t crc = 0xFF;
    if(cmd == 0) crc = 0x95;
    if(cmd == 8) crc = 0x87;
    spi_byte(crc);
    uint8_t r = 0xFF;
    for(int i = 0; i < 8; i++)
    {
        r = spi_byte(0xFF);
        if(!(r & 0x80)) break;
    }
    return r;
}

int main(void)
{
    hw.Configure();
    hw.Init();

    MyOled::Config oled_cfg;
    oled_cfg.driver_config.transport_config.pin_config.dc    = hw.GetPin(3);
    oled_cfg.driver_config.transport_config.pin_config.reset = hw.GetPin(4);
    display.Init(oled_cfg);
    showText("SD TEST");

    // Setup GPIO using old C API (compatible with hw.GetPin())
    miso_g.pin  = hw.GetPin(25); miso_g.mode = DSY_GPIO_MODE_INPUT;     miso_g.pull = DSY_GPIO_NOPULL;
    clk_g.pin   = hw.GetPin(26); clk_g.mode  = DSY_GPIO_MODE_OUTPUT_PP; clk_g.pull  = DSY_GPIO_NOPULL;
    mosi_g.pin  = hw.GetPin(27); mosi_g.mode = DSY_GPIO_MODE_OUTPUT_PP; mosi_g.pull = DSY_GPIO_NOPULL;
    cs_g.pin    = hw.GetPin(28); cs_g.mode   = DSY_GPIO_MODE_OUTPUT_PP; cs_g.pull   = DSY_GPIO_NOPULL;
    dsy_gpio_init(&miso_g);
    dsy_gpio_init(&clk_g);
    dsy_gpio_init(&mosi_g);
    dsy_gpio_init(&cs_g);

    // SD power-up: CS high + 80 clocks with MOSI high
    dsy_gpio_write(&cs_g,   1);
    dsy_gpio_write(&mosi_g, 1);
    for(int i = 0; i < 10; i++) spi_byte(0xFF);

    // CMD0: GO_IDLE_STATE — expect 0x01
    dsy_gpio_write(&cs_g, 0);
    uint8_t r1 = sd_cmd(0, 0);
    dsy_gpio_write(&cs_g, 1);
    spi_byte(0xFF);

    if(r1 != 0x01)
    {
        char buf[16];
        sprintf(buf, "CMD0:%02X", r1);
        showText("SD FAIL", buf);
        while(1) {}
    }

    // CMD8: SEND_IF_COND — check for SDv2/SDHC
    dsy_gpio_write(&cs_g, 0);
    r1 = sd_cmd(8, 0x000001AA);
    bool is_v2 = (r1 == 0x01);
    if(is_v2) { spi_byte(0xFF); spi_byte(0xFF); spi_byte(0xFF); spi_byte(0xFF); }
    dsy_gpio_write(&cs_g, 1);
    spi_byte(0xFF);

    // ACMD41: send init loop until card ready (CMD55 + CMD41)
    uint32_t deadline = System::GetNow() + 2000;
    do {
        dsy_gpio_write(&cs_g, 0); sd_cmd(55, 0); dsy_gpio_write(&cs_g, 1); spi_byte(0xFF);
        uint32_t arg = is_v2 ? 0x40000000 : 0;
        dsy_gpio_write(&cs_g, 0); r1 = sd_cmd(41, arg); dsy_gpio_write(&cs_g, 1); spi_byte(0xFF);
    } while(r1 == 0x01 && System::GetNow() < deadline);

    if(r1 != 0x00)
    {
        showText("SD FAIL", "ACMD41 TMO");
        while(1) {}
    }

    // CMD58: read OCR to detect SDHC bit
    bool sdhc = false;
    if(is_v2)
    {
        dsy_gpio_write(&cs_g, 0);
        sd_cmd(58, 0);
        uint8_t ocr0 = spi_byte(0xFF);
        spi_byte(0xFF); spi_byte(0xFF); spi_byte(0xFF);
        dsy_gpio_write(&cs_g, 1);
        spi_byte(0xFF);
        sdhc = (ocr0 & 0x40) != 0;
    }

    showText("SD OK", sdhc ? "SDHC/SDXC" : "SDv1/SDv2");
    while(1) {}
}
