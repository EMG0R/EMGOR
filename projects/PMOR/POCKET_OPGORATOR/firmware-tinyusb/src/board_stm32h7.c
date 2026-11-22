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

  __HAL_RCC_USB_OTG_FS_CLK_ENABLE();

  // Single combined IRQ line; TinyUSB's DWC2 driver demultiplexes all
  // endpoint/core events from GINTSTS internally (see
  // lib/tinyusb/src/portable/synopsys/dwc2/dcd_dwc2.c). No separate EP1
  // IN/OUT IRQn lines are used by the DWC2 IP core -- those exist in the
  // NVIC table as leftovers from an older non-OTG USB device core and
  // libDaisy's ST-usbd-stack code enables them defensively, but TinyUSB
  // (and every other DWC2-based stack) only needs OTG_FS_IRQn.
  NVIC_SetPriority(OTG_FS_IRQn, 6);
  NVIC_ClearPendingIRQ(OTG_FS_IRQn);
  NVIC_EnableIRQ(OTG_FS_IRQn);
}

// TinyUSB expects RHPort0's ISR named per-MCU-family convention; the STM32
// dcd_dwc2 glue (and every TinyUSB stm32h7 board file) wires this exact
// handler name to tusb_int_handler(0, true).
void OTG_FS_IRQHandler(void) {
  tusb_int_handler(0, true);
}

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
