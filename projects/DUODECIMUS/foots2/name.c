// Overrides the USB descriptor strings. Both symbols are weak aliases in the
// Teensy core, so defining them here wins. This is what renames the device as
// the operating system and every DAW sees it: the MIDI port stops being
// "Teensy MIDI" and becomes DUODECIMUS.
#include "usb_names.h"

#define MANUFACTURER_NAME     {'E','M','G','O','R'}
#define MANUFACTURER_NAME_LEN 5

#define PRODUCT_NAME          {'D','U','O','D','E','C','I','M','U','S'}
#define PRODUCT_NAME_LEN      10

struct usb_string_descriptor_struct usb_string_manufacturer_name = {
  2 + MANUFACTURER_NAME_LEN * 2, 3, MANUFACTURER_NAME
};
struct usb_string_descriptor_struct usb_string_product_name = {
  2 + PRODUCT_NAME_LEN * 2, 3, PRODUCT_NAME
};
