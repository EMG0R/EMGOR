// board_stm32h7.c — see board_stm32h7.h for the "why" of this file.
//
// GPIO/clock sequence below is deliberately identical to what libDaisy's
// own (unused-by-us) src/usbd/usbd_conf.c does for USB_OTG_FS's
// HAL_PCD_MspInit(): same pins (PA11/PA12 AF10_OTG1_FS, PA9 VBUS sense
// input), same peripheral clock enable
// (__HAL_RCC_USB_OTG_FS_CLK_ENABLE()). We don't call HAL_PCD_* at all --
// TinyUSB's dcd_dwc2 driver talks to the OTG_FS register block directly --
// but the clock/pin muxing has to happen exactly the same way regardless of
// which USB device stack ends up reading those registers.
//
// libDaisy's System::Init() (called from hw.Init() before board_usb_init())
// already selects HSI48 as the USB kernel clock source
// (PeriphClkInitStruct.UsbClockSelection = RCC_USBCLKSOURCE_HSI48 in
// src/sys/system.cpp) and calls HAL_PWREx_EnableUSBVoltageDetector() -- both
// prerequisites for the OTG_FS PHY to work at all -- so this file only adds
// what's specific to actually bringing the peripheral+pins up.

#include "board_stm32h7.h"

#include "stm32h7xx_hal.h"
#include "tusb.h"

void board_usb_init(void) {
  __HAL_RCC_GPIOA_CLK_ENABLE();

  GPIO_InitTypeDef gpio = {0};

  // PA11 = USB_OTG_FS_DM, PA12 = USB_OTG_FS_DP
  gpio.Pin       = GPIO_PIN_11 | GPIO_PIN_12;
  gpio.Mode      = GPIO_MODE_AF_PP;
  gpio.Pull      = GPIO_NOPULL;
  gpio.Speed     = GPIO_SPEED_FREQ_LOW;
  gpio.Alternate = GPIO_AF10_OTG1_FS;
  HAL_GPIO_Init(GPIOA, &gpio);

  // PA9 = USB_OTG_FS_VBUS (sense input only; Daisy Seed is USB-powered
  // device-side, this pin just tells the OTG core "cable is present").
  gpio.Pin  = GPIO_PIN_9;
  gpio.Mode = GPIO_MODE_INPUT;
  gpio.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &gpio);

  // ── Force a full OTG peripheral reset BEFORE enabling it ──────────────
  // This app is APP_TYPE = BOOT_SRAM: it is loaded and started by the Daisy
  // bootloader, which uses this SAME OTG core to expose DFU. The bootloader
  // hands the core over still clocked, configured and (from the host's point
  // of view) already enumerated. Enabling the clock on top of that dirty
  // state silently fails — the device runs fine but presents NOTHING on USB.
  //
  // Measured 2026-08-14: byte-identical USB code enumerates every time when
  // flashed to internal flash (runs directly from reset, core is pristine)
  // and never enumerates when loaded by the bootloader. Ruled out first:
  // the OLED (deferring its init changed nothing) and 480MHz boost mode
  // (hw.Init(false) changed nothing). The peripheral handover is the cause.
  // NOTE THE PERIPHERAL: the Daisy Seed's USB is OTG1 == USB1_OTG_HS driven
  // in full-speed mode via its internal PHY (hence GPIO_AF10_OTG1_FS on
  // PA11/PA12 above, and the hpcd_USB_OTG_HS symbol in the map). Resetting
  // USB2_OTG_FS here is a no-op on this part — a mistake worth flagging,
  // since the clock-enable macro below is spelled ..._USB_OTG_FS_... and
  // reads as if FS were a separate peripheral. It is not.
  // WHICH PERIPHERAL — settled by reading TinyUSB's own port table rather
  // than by inference from the macro names, which are actively misleading:
  //   dwc2_stm32.h:69   #define USB_OTG_FS_PERIPH_BASE  USB1_OTG_HS_BASE
  //   dwc2_stm32.h:131  { .reg_base = USB_OTG_FS_PERIPH_BASE, .irqnum = OTG_FS_IRQn }
  // So rhport0's REGISTERS are USB1_OTG_HS even though everything is spelled
  // "FS" (FS here means full-speed signalling via the internal PHY, not the
  // USB2_OTG_FS peripheral). Reset the core we actually drive.
  __HAL_RCC_USB1_OTG_HS_FORCE_RESET();
  HAL_Delay(10);
  __HAL_RCC_USB1_OTG_HS_RELEASE_RESET();
  HAL_Delay(10);

  __HAL_RCC_USB_OTG_FS_CLK_ENABLE();

  // Single combined IRQ line; TinyUSB's DWC2 driver demultiplexes all
  // endpoint/core events from GINTSTS internally (see
  // lib/tinyusb/src/portable/synopsys/dwc2/dcd_dwc2.c). No separate EP1
  // IN/OUT IRQn lines are used by the DWC2 IP core -- those exist in the
  // NVIC table as leftovers from an older non-OTG USB device core and
  // libDaisy's ST-usbd-stack code enables them defensively, but TinyUSB
  // (and every other DWC2-based stack) only needs OTG_FS_IRQn.
  // Priority 1, not 6. libDaisy puts EVERY DMA stream, SPI, I2C, UART,
  // SDMMC and QSPI IRQ at preempt priority 0 (sys/dma.c, per/spi.cpp etc),
  // so at 6 the OTG ISR was outranked by half the chip and could be
  // preempted mid-TXFIFO-copy -- which wedges the DWC2 packet state machine
  // permanently. The SAI audio DMA must stay at 0 (dropouts otherwise), so
  // 1 makes audio the ONLY thing that can preempt USB.
  // Priority 0 == the SAI/DMA audio ISR's preempt level (libDaisy sys/dma.c
  // sets every DMA stream to 0,0). Equal preempt priority means NEITHER can
  // preempt the other mid-operation on Cortex-M — which closes the DWC2
  // TXFIFO-interruption wedge at the silicon level. (At priority 1 the audio
  // ISR could still cut into an in-flight TXFIFO copy; the copy must never
  // be interrupted, per ChibiOS/libopencm3-documented DWC2 behaviour.)
  // Cost: the audio callback can be delayed by one USB ISR (~µs) — harmless.
  NVIC_SetPriority(OTG_FS_IRQn, 0);
  NVIC_ClearPendingIRQ(OTG_FS_IRQn);
  NVIC_EnableIRQ(OTG_FS_IRQn);

  // Enable the OTG_HS line too. On this part the core in use is OTG1 ==
  // USB1_OTG_HS (FS mode via internal PHY), so OTG_HS_IRQn is very likely
  // the line that actually carries its interrupts. Enabling both costs
  // nothing — whichever is wrong simply never fires — and the two counters
  // above then tell us which one is real instead of us guessing again.
  // (OTG_HS_IRQn deliberately NOT enabled — libDaisy owns that vector)
  // NVIC_SetPriority(OTG_HS_IRQn, 6);
  // NVIC_ClearPendingIRQ(OTG_HS_IRQn);
  // NVIC_EnableIRQ(OTG_HS_IRQn);
}

// TinyUSB expects RHPort0's ISR named per-MCU-family convention; the STM32
// dcd_dwc2 glue (and every TinyUSB stm32h7 board file) wires this exact
// handler name to tusb_int_handler(0, true).
// ── DIAGNOSTIC COUNTERS (2026-08-14) ─────────────────────────────────────
// USB is dead as a debug channel here, so these get rendered on the OLED.
// The question they answer: is the USB interrupt firing AT ALL, and on which
// line? The Daisy's USB is OTG1 == USB1_OTG_HS-in-FS-mode, whose NVIC line
// is OTG_HS_IRQn — but this file (copied from a build that works when
// flashed to internal flash) wires OTG_FS_IRQn. If fs_irqs stays 0 while
// hs_irqs climbs, the IRQ line is simply wrong and nothing else matters.
volatile uint32_t g_usb_fs_irqs = 0;
volatile uint32_t g_usb_hs_irqs = 0;

void OTG_FS_IRQHandler(void) {
  g_usb_fs_irqs++;
  tusb_int_handler(0, true);
}

// NOTE — we CANNOT define OTG_HS_IRQHandler here: libDaisy's own system.cpp
// already defines it (the link fails with "multiple definition"). That is
// itself the most important thing found while debugging this: libDaisy owns
// the OTG_HS vector and routes it into ITS OWN ST USB device stack. If the
// core actually in use is OTG1/USB1_OTG_HS, then its interrupts are being
// delivered to libDaisy's handler, TinyUSB never sees them, and enumeration
// cannot happen no matter how correct the TinyUSB side is. g_usb_fs_irqs
// staying at 0 while the device is plugged in confirms exactly that.

//--------------------------------------------------------------------+
// USB serial string from the STM32 factory unique ID
//--------------------------------------------------------------------+
#define STM32_UID_BASE 0x1FF1E800UL

uint32_t board_get_now_ms(void) {
  return HAL_GetTick();
}

size_t board_get_serial_utf16(uint16_t *buf, size_t max_chars) {
  static const char hex_digits[] = "0123456789ABCDEF";
  uint32_t const *uid            = (uint32_t const *) STM32_UID_BASE;

  size_t n = 0;
  for (int word = 0; word < 3 && n + 8 <= max_chars; word++) {
    uint32_t v = uid[word];
    for (int nibble = 7; nibble >= 0; nibble--) {
      buf[n++] = (uint16_t) hex_digits[(v >> (nibble * 4)) & 0xF];
    }
  }
  return n;
}
