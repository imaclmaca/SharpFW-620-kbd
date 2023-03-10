/*
 * Header file for the Sharp FontWriter 620 keyboard matrix decoder
 */

#ifndef _KB_MAIN_H_
#define _KB_MAIN_H_

#ifdef __cplusplus
 extern "C" {
#endif

// Define the polling rate for the USB HID service
#define PW_POLL  10  // default to 10ms polling rate

#define ROW_MASK    0x000000FF
#define FLAG_ALL_UP 0xFFFFFFFF

 // Code to signal Caps Lock on
#define CAPS_ON     0x55

/* Used to pass a key-combo from the keyboard thread to the USB thread.
 * Uses a Pico FIFO to pass a uint32_t. This word has 4 "codes" packed into
 * it as "modifiers", "k1", "k2", "k3".
 * At most this supports a 3-key combo, which gamers might find derisory
 * but is plenty here since the FontWriter matrix has no diodes and is
 * therefore prone to shadowing if more than 3 keys are pressed anyway. */
typedef union
{
    uint32_t u_msg;
    uint8_t  p [4];
} msg_blk;

// defined in fw-kb-main.c
extern uint32_t kc_get (void);
extern void set_caps_lock_led (int i_state);

// Defined in usb-stack.c
extern void led_blinking_task(void);
extern void hid_task(void);

// Defined in usb_descriptors.c
extern void set_serial_string (char const *ser);

#ifdef __cplusplus
 }
#endif

#endif /* _KB_MAIN_H_ */

/* End of File */
