/* Entry point for the Sharp FontWriter 620 keyboard matrix decoder */

// Basics to get the Pico going...
#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/unique_id.h"
#include <string.h>
#include <ctype.h>

// tinyusb parts...
#include <bsp/board.h>
#include <tusb.h>

// local parts
#include "fw-kb-main.h"

/* Are we emitting serial debug? */
//#define SER_DBG_ON  1  // serial debug on
#undef SER_DBG_ON      // serial debug off

// Keyboard mapping and decode tables
#define FNK (10)  // Base of the "Function Key" range
#define SPC ' '   // 32 - ASCII space - used to delimit the "private" range

// Internal "private" codes for function keys, etc.
#define DEL  (1)  // DELETE
#define _UP  (2)  // Cursor UP
#define FWD  (3)  // Cursor Forward (RIGHT)
#define PUP  (4)  // Page UP
#define INS  (5)  // INSERT
//#define CTR  (6)  // CTRL modifier
#define KPE  (7)  // Keypad Enter key code
#define TAB  '\t' // TAB key (9)
#define RTN  '\n' // Return key (10)

#define F01  (FNK + 1) // 11
#define F02  (FNK + 2)
#define F03  (FNK + 3)
#define F04  (FNK + 4)
#define F05  (FNK + 5) // 15
#define F06  (FNK + 6)
#define F07  (FNK + 7)
#define F08  (FNK + 8)
#define F09  (FNK + 9)
#define F10  (FNK + 10) // 20
#define F11  (FNK + 11)
#define F12  (FNK + 12) // 22
#define B_P  (23)  // Special that re-maps to backslash / pipe on UK layouts
#define HOM  (24)  // HOME
#define BCK  (25)  // Cursor BACK (LEFT)
#define DND  (26)  // Document END
#define DWN  (27)  // Cursor DOWN
#define PDN  (28)  // Page DOWN
#define _EC  (29)  // ESC
#define BSP  (30)  // 30 - Backspace
#define ALT  (31)  // 31 - ALT modifier

// convert "internal" codes into USB HID keycodes
static uint8_t const int_codes_table [32] = {
    0,
    HID_KEY_DELETE,
    HID_KEY_ARROW_UP,
    HID_KEY_ARROW_RIGHT,
    HID_KEY_PAGE_UP,
    HID_KEY_INSERT,
    HID_KEY_CONTROL_LEFT, // Can be a modifier
    HID_KEY_KEYPAD_ENTER,
    0, // 8 - unused
    HID_KEY_TAB,
    HID_KEY_ENTER,
    HID_KEY_F1,
    HID_KEY_F2,
    HID_KEY_F3,
    HID_KEY_F4,
    HID_KEY_F5,
    HID_KEY_F6,
    HID_KEY_F7,
    HID_KEY_F8,
    HID_KEY_F9,
    HID_KEY_F10,
    HID_KEY_F11,
    HID_KEY_F12,
    HID_KEY_EUROPE_2, // 23 - Special that re-maps to backslash / pipe on UK layouts
    HID_KEY_HOME,
    HID_KEY_ARROW_LEFT,
    HID_KEY_END,
    HID_KEY_ARROW_DOWN,
    HID_KEY_PAGE_DOWN,
    HID_KEY_ESCAPE,
    HID_KEY_BACKSPACE,
    HID_KEY_ALT_LEFT // Can be a modifier
    };

// "Special" keys and "code-2" (or code-II) keys
#define CER (128) // Old 1252 code for Euro sign €
#define CAP (129) // Caps Lock
#define BSQ (130) // The FW has weird layout for square and curly brackets - this is for square
#define BCR (131) // The FW has weird layout for square and curly brackets - this is for curly
#define SSZ (132) // German hard-s
#define YEN (133) // Yen
#define CNT (134) // cent
#define BKT (135) // single-backtick c/w umlaut modifier
#define SPM (136) // section marker c/w plus/minus sign
#define DEG (137) // degree modifier
#define IEX (138) // Inverted exclamation mark, as in Spanish
#define IQM (139) // Inverted question mark
#define NSQ (140) // Spanish-style N/~ symbol Ñ
#define CED (141) // Spanish-style C-cedilla symbol
#define GBP (142) // Old 1252 code for £ sign

// Modifier key codes
#define BLK (200) // BLOCK modifier
#define HLP (201) // HELP key
#define CD2 (202) // Code-2 modifier
#define SHF (203) // Shift modifier
#define WIN (204) // WIN key (as a modifier)
#define CTR (205) // Left CTRL modifier
#define CRR (206) // Right CTRL modifier

#define ROW_SZ  8
#define COL_SZ 10

// The basic keymap
static __uint8_t key_table [ROW_SZ * COL_SZ] = {
    0,  BSQ,  '-',  'p',  ';', '\'', '0',   0,  '/',  BCK,
  BSP,    0,  '=',  'o',  'l',    0, '9',   0,  '.',  WIN,
  FWD,  _UP,  PUP,  'i',  'k',  _EC, '8',   0,  ',',  PDN,
  'n',  'y',  '6',  'u',  'j',  'h', '7', CRR,  'm',    0,
  'b',  't',  '5',  'r',  'f',  'g', '4', CTR,  'v',    0,
  SPC,  RTN,  DEL,  'e',  'd',  BLK, '3',   0,  'c',  HLP,
  DWN,  DND,  HOM,  'w',  's',  CD2, '2',   0,  'x',  ALT,
    0,  TAB,  '`',  'q',  'a',  CAP, '1', SHF,  'z',    0
};

// The basic keymap, but with the Function keys F1 - F10 mapped to the number keys
static __uint8_t key_FN_table [ROW_SZ * COL_SZ] = {
    0,  BSQ,  '-',  'p',  ';', '\'',  F10,    0,  '/',  BCK,
  BSP,    0,  '=',  'o',  'l',    0,  F09,    0,  '.',  WIN,
  FWD,  _UP,  PUP,  'i',  'k',  _EC,  F08,    0,  ',',  PDN,
  'n',  'y',  F06,  'u',  'j',  'h',  F07,  CRR,  'm',    0,
  'b',  't',  F05,  'r',  'f',  'g',  F04,  CTR,  'v',    0,
  SPC,  RTN,  DEL,  'e',  'd',  BLK,  F03,    0,  'c',  HLP,
  DWN,  DND,  HOM,  'w',  's',  CD2,  F02,    0,  'x',  ALT,
    0,  TAB,  '`',  'q',  'a',  CAP,  F01,  SHF,  'z',    0
};

// The keymap with many (not all) of the Code-II keys mapped
static __uint8_t key2_table [ROW_SZ * COL_SZ] = {
    0,  BCR,  BKT,  'p',  NSQ, '\\',  '0',    0,  CED,  BCK,
  BSP,    0,  SPM,  'o',  'l',    0,  DEG,    0,  IQM,  WIN,
  FWD,  _UP,  PUP,  'i',  'k',  _EC,  '8',    0,  IEX,  PDN,
  'n',  'y',  CNT,  'u',  'j',  'h',  '7',  CRR,  'm',    0,
  'b',  't',  YEN,  'r',  'f',  'g',  SSZ,  CTR,  'v',    0,
  SPC,  RTN,  DEL,  'e',  'd',  BLK,  CER,    0,  'c',  HLP,
  DWN,  DND,  HOM,  'w',  's',  CD2,  '2',    0,  'x',  ALT,
    0,  TAB,  B_P,  'q',  'a',  CAP,  '1',  SHF,  'z',    0
};

// Table to "quickly" spot the modifier keys
static __uint8_t is_mod_key [ROW_SZ * COL_SZ] = {
  0, 0, 0, 0, 0,   0, 0,   0, 0,   0,
  0, 0, 0, 0, 0,   0, 0,   0, 0, WIN,
  0, 0, 0, 0, 0,   0, 0,   0, 0,   0,
  0, 0, 0, 0, 0,   0, 0, CRR, 0,   0,
  0, 0, 0, 0, 0,   0, 0, CTR, 0,   0,
  0, 0, 0, 0, 0, BLK, 0,   0, 0, HLP,
  0, 0, 0, 0, 0, CD2, 0,   0, 0, ALT,
  0, 0, 0, 0, 0,   0, 0, SHF, 0,   0
};

// The tinyusb ASCII -> HID code table
static uint8_t const conv_table[128][2] =  { HID_ASCII_TO_KEYCODE };

static __uint8_t prv_scan [COL_SZ]; // keys down on previous scan
static __uint8_t cur_scan [COL_SZ]; // keys down on this scan

// circular buffer for key-codes, pending sending...
#define KC_SZ 16
#define KC_MSK (KC_SZ - 1)
static uint32_t kc_buf [KC_SZ];
static uint32_t kc_in  = 0;
static uint32_t kc_out = 0;

// Used by main() to queue up payloads for sending to the USB hid_task()
static void kc_put (uint32_t uv)
{
    uint32_t next = (kc_in + 1) & KC_MSK;
    if (next == kc_out)
    {
        // queue full, skip this character
        return;
    }
    kc_buf [kc_in] = uv;
    kc_in = next;
}

// Used by hid_task() in usb-stack.c to read payloads to send on the USB
uint32_t kc_get (void)
{
    if (kc_in == kc_out)
    {
        return 0;
    }
    uint32_t uv = kc_buf [kc_out];
    kc_out = (kc_out + 1) & KC_MSK;
    return uv;
}

/* Process the key matrix to determine which keys are pressed
 * and decide what to send to the USB stack. */
static void process_keys (int all_keys_up)
{
    uint8_t Mods  = 0; // Which modifier bits are set
    uint8_t Kcode = 0; // What is the current key code

    msg_blk code;     // Holds the USB code to be sent for this key
    msg_blk code_alt; // Holds an additional USB code, e.g. for sending composed keys
    int code_idx = 2; // Records how many "slots" have been used in msg_blk code

    code.u_msg = 0;     // Clear all the bytes in the union
    code_alt.u_msg = 0; // Clear all the bytes in the union

    __uint8_t *pTable = key_table;
    // Pick the active keys out of the keymap
    int col;
    int row;
    int idx;

    // Scan the matrix for (at most) 4 pressed keys. In practice we reject any more than 3.
#define MX_KEYS 4
    int keys[MX_KEYS];
    int i_keys = 0;
    for (col = 0; ((col < COL_SZ) && (i_keys < MX_KEYS)); ++col)
    {
        // Is any key down on this column?
        if (ROW_MASK == cur_scan[col])
        {
            // All keys are UP
        }
        else // There must be at least 1 bit down, find it now
        {
            unsigned u_tst = 1;
            for (row = 0; row < 8; ++row)
            {
                if ((u_tst & cur_scan[col]) == 0)
                {
                    idx = (row * COL_SZ) + col;
                    keys [i_keys] = idx;
                    ++i_keys;
                    if (i_keys >= MX_KEYS)
                    {
                        break;
                    }
                }
                u_tst = u_tst << 1;
            }
        }
    }

    // Were any valid keys found?
    if (i_keys < 1)
    {
        // Are all the keys UP now? Tell the USB HID stack if so.
        if (all_keys_up != 0)
        {
            if (multicore_fifo_wready ())
            {
                multicore_fifo_push_blocking (FLAG_ALL_UP);
                all_keys_up = 0;
            }
        }
        return; // No more keys to process
    }

    /* Is there a modifier set? Scan the set for any modifiers first,
     * before we try to interpret any "normal" keys. */
    int is_mod = 0;
    for (idx = 0; idx < i_keys; ++idx)
    {
        __uint8_t kc = is_mod_key [keys[idx]];
        if (kc)
        {
            uint8_t ucode = 0;
            ++is_mod;
            switch (kc)
            {
                case SHF:
                Mods |= KEYBOARD_MODIFIER_LEFTSHIFT; //Shift key
                ucode = HID_KEY_SHIFT_LEFT;
                break;

                case WIN:
                Mods |= KEYBOARD_MODIFIER_LEFTGUI; // Left WIN key
                ucode = HID_KEY_GUI_LEFT;
                break;

                case ALT:
                Mods |= KEYBOARD_MODIFIER_LEFTALT; // Left ALT key
                ucode = HID_KEY_ALT_LEFT;
                break;

                case CRR:
                Mods |= KEYBOARD_MODIFIER_RIGHTCTRL; // Right CTRL
                ucode = HID_KEY_CONTROL_RIGHT;
                break;

                case CTR:
                Mods |= KEYBOARD_MODIFIER_LEFTCTRL; // Left CTRL
                ucode = HID_KEY_CONTROL_LEFT;
                break;

                case HLP: // HELP is pressed, map the Function keymap instead
                pTable = key_FN_table;
                break;

                case CD2: // Code-II is pressed, map the code-II keymap
                pTable = key2_table;
                break;

                case BLK: // Not used at present
                // is_BLK
                default:
                break;
            }

            // queue up modifier for sending (if it is an active key)
            if ((ucode != 0) && (code_idx >= 0) )
            {
                code.p[code_idx] = Kcode = ucode;
                --code_idx;
            }

            keys[idx] = 0; // Clear the processed modifier key so it is not scanned again later
        }
    }

    if ((i_keys - is_mod) > 1) return; // Too many keys are down...

    // Scan for the remaining key and interpret it, then emit the processed key(s) to the USB queue
    for (idx = 0; idx < i_keys; ++idx)
    {
        __uint8_t kc = pTable [keys[idx]];
        if (kc)
        {
            if (kc < SPC)
            {
                // Some "internal" key - determine which...
                Kcode = int_codes_table [kc];
                if (code_idx >= 0)
                {
                    code.p[code_idx] = Kcode;
                    --code_idx;
                }
            }
            else if (kc < CER) // Any "normal" key
            {
                if (conv_table[kc][0])
                {
                    Mods |= KEYBOARD_MODIFIER_LEFTSHIFT;
                }
                Kcode = conv_table[kc][1];
                if (code_idx >= 0)
                {
                    code.p[code_idx] = Kcode;
                    --code_idx;
                }
            }
            else if ((kc >= CER) && (kc <= GBP)) // A few special cases
            {
                uint8_t ucode = 0;
                switch (kc)
                {
                    case CER: // Euro currency symbol €
                    {
                        if ((Mods & KEYBOARD_MODIFIER_LEFTSHIFT) != 0)
                        {
                            Mods = KEYBOARD_MODIFIER_RIGHTALT;
                            ucode = HID_KEY_4; // AlrGr + 4 works for UK layouts...
                        }
                        else
                        {
                            ucode = HID_KEY_3;
                        }
                    }
                    break;

                    case CAP: // Caps Lock key
                    ucode = HID_KEY_CAPS_LOCK;
                    break;

                    case BSQ: // Square brackets [ and ]
                    {
                        if ((Mods & KEYBOARD_MODIFIER_LEFTSHIFT) != 0)
                        {
                            /* Shift is set: on a std UK kbd this produces "{"
                             * but here we want to produce "]" instead. */
                            Mods &= ~(KEYBOARD_MODIFIER_LEFTSHIFT); // clear the shift modifier
                            ucode = HID_KEY_BRACKET_RIGHT;
                        }
                        else
                        {
                            ucode = HID_KEY_BRACKET_LEFT;
                        }
                    }
                    break;

                    case BCR: // Curly brackets { and }
                    {
                        if ((Mods & KEYBOARD_MODIFIER_LEFTSHIFT) != 0)
                        {
                            /* We want "}" here, which is SHIFT + HID_KEY_BRACKET_RIGHT
                             * on a std UK kbd. */
                            ucode = HID_KEY_BRACKET_RIGHT;
                        }
                        else
                        {
                            /* We want "{" here, which is SHIFT + HID_KEY_BRACKET_LEFT
                             * on a std UK kbd. */
                            Mods |= KEYBOARD_MODIFIER_LEFTSHIFT;
                            ucode = HID_KEY_BRACKET_LEFT;
                        }
                    }
                    break;

                    case SSZ: // German-style sharp-S
                    {
                        if ((Mods & KEYBOARD_MODIFIER_LEFTSHIFT) != 0)
                        {
                            Mods = KEYBOARD_MODIFIER_RIGHTALT;
                            code.p[2] = HID_KEY_ALT_RIGHT;
                            code_idx = 1;
                            ucode = HID_KEY_S; // AlrGr + s works for UK layouts...
                        }
                        else
                        {
                            ucode = HID_KEY_4;
                        }
                    }
                    break;

                    case YEN: // Japanese Yen symbol
                    {
                        if ((Mods & KEYBOARD_MODIFIER_LEFTSHIFT) != 0)
                        {
                            code_alt.p[3] = KEYBOARD_MODIFIER_RIGHTALT;
                            code_alt.p[2] = HID_KEY_ALT_RIGHT;
                            if (multicore_fifo_wready ())
                            {
                                multicore_fifo_push_blocking (code_alt.u_msg);
                            }

                            Mods |= KEYBOARD_MODIFIER_RIGHTALT;
                            code.p[2] = HID_KEY_Y;
                            code.p[1] = HID_KEY_ALT_RIGHT;
                            code_idx = 0;
                            ucode = HID_KEY_SHIFT_LEFT; // AlrGr + shift + y works for UK layouts...
                        }
                        else
                        {
                            ucode = HID_KEY_5;
                        }
                    }
                    break;

                    case CNT: // Cent currency symbol
                    {
                        if ((Mods & KEYBOARD_MODIFIER_LEFTSHIFT) != 0)
                        {
                            Mods = KEYBOARD_MODIFIER_RIGHTALT;
                            code.p[2] = HID_KEY_ALT_RIGHT;
                            code_idx = 1;
                            ucode = HID_KEY_C; // AlrGr + c works for UK layouts...
                        }
                        else
                        {
                            ucode = HID_KEY_6;
                        }
                    }
                    break;

                    case BKT: // (Single) Backtick c/w Umlaut modifier
                    {
                        if ((Mods & KEYBOARD_MODIFIER_LEFTSHIFT) != 0)
                        {
                            // Umlaut case (a dead-key option)
                            Mods = KEYBOARD_MODIFIER_RIGHTALT;
                            code.p[2] = HID_KEY_ALT_RIGHT;
                            code_idx = 1;
                            ucode = HID_KEY_BRACKET_LEFT; // AlrGr + [ works for UK layouts...
                        }
                        else
                        {
                            // single backtick
                            code_alt.p[3] = KEYBOARD_MODIFIER_RIGHTALT;
                            code_alt.p[2] = HID_KEY_ALT_RIGHT;
                            if (multicore_fifo_wready ())
                            {
                                multicore_fifo_push_blocking (code_alt.u_msg);
                            }

                            Mods |= KEYBOARD_MODIFIER_RIGHTALT | KEYBOARD_MODIFIER_LEFTSHIFT;
                            code.p[2] = HID_KEY_B;
                            code.p[1] = HID_KEY_ALT_RIGHT;
                            code_idx = 0;
                            ucode = HID_KEY_SHIFT_LEFT; // AlrGr + shift + b works for UK layouts...
                        }
                    }
                    break;

                    case IEX: // Spanish-style inverted exclamation mark
                    {
                        if ((Mods & KEYBOARD_MODIFIER_LEFTSHIFT) != 0)
                        {
                            code_alt.p[3] = KEYBOARD_MODIFIER_RIGHTALT;
                            code_alt.p[2] = HID_KEY_ALT_RIGHT;
                            if (multicore_fifo_wready ())
                            {
                                multicore_fifo_push_blocking (code_alt.u_msg);
                            }

                            Mods |= KEYBOARD_MODIFIER_RIGHTALT | KEYBOARD_MODIFIER_LEFTSHIFT;
                            code.p[2] = HID_KEY_1;
                            code.p[1] = HID_KEY_ALT_RIGHT;
                            code_idx = 0;
                            ucode = HID_KEY_SHIFT_LEFT; // AlrGr + shift + 1 works for UK layouts...
                        }
                        else
                        {
                            ucode = HID_KEY_COMMA;
                        }
                    }
                    break;

                    case IQM: // Spanish-style inverted question mark
                    {
                        if ((Mods & KEYBOARD_MODIFIER_LEFTSHIFT) != 0)
                        {
                            code_alt.p[3] = KEYBOARD_MODIFIER_RIGHTALT;
                            code_alt.p[2] = HID_KEY_ALT_RIGHT;
                            if (multicore_fifo_wready ())
                            {
                                multicore_fifo_push_blocking (code_alt.u_msg);
                            }

                            Mods |= KEYBOARD_MODIFIER_RIGHTALT | KEYBOARD_MODIFIER_LEFTSHIFT;
                            code.p[2] = HID_KEY_MINUS;
                            code.p[1] = HID_KEY_ALT_RIGHT;
                            code_idx = 0;
                            ucode = HID_KEY_SHIFT_LEFT; // AlrGr + shift + "-" works for UK layouts...
                        }
                        else
                        {
                            ucode = HID_KEY_PERIOD;
                        }
                    }
                    break;

                    case SPM: // Section-mark c/w plus/minus symbol
                    {
                        if ((Mods & KEYBOARD_MODIFIER_LEFTSHIFT) != 0)
                        {
                            code_alt.p[3] = KEYBOARD_MODIFIER_RIGHTALT;
                            code_alt.p[2] = HID_KEY_ALT_RIGHT;
                            if (multicore_fifo_wready ())
                            {
                                multicore_fifo_push_blocking (code_alt.u_msg);
                            }

                            Mods |= KEYBOARD_MODIFIER_RIGHTALT | KEYBOARD_MODIFIER_LEFTSHIFT;
                            code.p[2] = HID_KEY_9;
                            code.p[1] = HID_KEY_ALT_RIGHT;
                            code_idx = 0;
                            ucode = HID_KEY_SHIFT_LEFT; // AlrGr + shift + 9 works for UK layouts...
                        }
                        else
                        {
                            code_alt.p[3] = KEYBOARD_MODIFIER_RIGHTALT;
                            code_alt.p[2] = HID_KEY_ALT_RIGHT;
                            if (multicore_fifo_wready ())
                            {
                                multicore_fifo_push_blocking (code_alt.u_msg);
                            }

                            Mods = KEYBOARD_MODIFIER_RIGHTALT | KEYBOARD_MODIFIER_LEFTSHIFT;
                            code.p[2] = HID_KEY_S;
                            code.p[1] = HID_KEY_ALT_RIGHT;
                            code_idx = 0;
                            ucode = HID_KEY_SHIFT_LEFT; // AlrGr + shift + s works for UK layouts...
                        }
                    }
                    break;

                    case DEG: // Degree symbol
                    {
                        if ((Mods & KEYBOARD_MODIFIER_LEFTSHIFT) != 0)
                        {
                            code_alt.p[3] = KEYBOARD_MODIFIER_RIGHTALT;
                            code_alt.p[2] = HID_KEY_ALT_RIGHT;
                            if (multicore_fifo_wready ())
                            {
                                multicore_fifo_push_blocking (code_alt.u_msg);
                            }

                            Mods |= KEYBOARD_MODIFIER_RIGHTALT | KEYBOARD_MODIFIER_LEFTSHIFT;
                            code.p[2] = HID_KEY_0;
                            code.p[1] = HID_KEY_ALT_RIGHT;
                            code_idx = 0;
                            ucode = HID_KEY_SHIFT_LEFT; // AlrGr + shift + 0 works for UK layouts...
                        }
                        else
                        {
                            ucode = HID_KEY_9;
                        }
                    }
                    break;

                    case NSQ: // Spanish style combo char Ñ or ñ here.
                    {
                        // send the dead-key first (ALT + "]" here)
                        code_alt.p[3] = KEYBOARD_MODIFIER_RIGHTALT;
                        code_alt.p[2] = HID_KEY_BRACKET_RIGHT;
                        code_alt.p[1] = HID_KEY_ALT_RIGHT;
                        if (multicore_fifo_wready ())
                        {
                            multicore_fifo_push_blocking (code_alt.u_msg);
                        }

                        if ((Mods & KEYBOARD_MODIFIER_LEFTSHIFT) != 0)
                        {
                            code.p[2] = HID_KEY_N;
                            code_idx = 1;
                            ucode = HID_KEY_SHIFT_LEFT;
                        }
                        else
                        {
                            ucode = HID_KEY_N;
                        }
                    }
                    break;

                    case CED: // Spanish style C-cedilla combo char ç or Ç here.
                    {
                        // send the dead-key first (ALT + "=" here)
                        code_alt.p[3] = KEYBOARD_MODIFIER_RIGHTALT;
                        code_alt.p[2] = HID_KEY_EQUAL;
                        code_alt.p[1] = HID_KEY_ALT_RIGHT;
                        if (multicore_fifo_wready ())
                        {
                            multicore_fifo_push_blocking (code_alt.u_msg);
                        }

                        if ((Mods & KEYBOARD_MODIFIER_LEFTSHIFT) != 0)
                        {
                            code.p[2] = HID_KEY_C;
                            code_idx = 1;
                            ucode = HID_KEY_SHIFT_LEFT;
                        }
                        else
                        {
                            ucode = HID_KEY_C;
                        }
                    }
                    break;

                    case GBP: // This is not actually used, it is just a sentinel for the range, really!
                    Mods |= KEYBOARD_MODIFIER_LEFTSHIFT;
                    ucode = HID_KEY_3; // Shift-3 is correct for UK layouts
                    break;

                    default:
                    break;
                }
                if ((code_idx >= 0) && (ucode != 0))
                {
                    Kcode = ucode;
                    code.p[code_idx] = Kcode;
                    --code_idx;
                }
            }
        }
    }
    code.p[3] = Mods;
    // If there is a key press ready, pass it to the main thread for processing / sending
    if (Kcode)
    {
        if (multicore_fifo_wready ())
        {
            multicore_fifo_push_blocking (code.u_msg);
        }
    }
    // Are all the keys UP now? Tell the USB HID stack if so.
    if (all_keys_up != 0)
    {
        if (multicore_fifo_wready ())
        {
            multicore_fifo_push_blocking (FLAG_ALL_UP);
            all_keys_up = 0;
        }
    }
} // process_keys

/* The "main" task on the second core.
 * This manages the reading and initial decoding of the keyboard matrix. */
void scan_thread (void)
{
    // signal to the primary thread that this worker thread is ready
    multicore_fifo_push_blocking (99);

    int sel_line = 0; // For columns 0 to 9 (10 lines)
    int all_off_count = 0;
    while (true)
    {
        unsigned set_ln = sel_line + 2; // Our "column 0" is GPIO line 2

        gpio_set_dir(set_ln, GPIO_OUT);
        gpio_put (set_ln, 0); // Drive test line low
        sleep_us (200);

        unsigned u_row = gpio_get_all (); // Read the 8 rows (GPIO lines 12 to 19)
        u_row = (u_row >> 12) & ROW_MASK;

        cur_scan [sel_line] = (__uint8_t)u_row;
        if (ROW_MASK == u_row)
        {
            // No keys are down in this row
            ++all_off_count;
        }

        gpio_put (set_ln, 1); // Drive test line high again
        sleep_us (50);

        // Set line back to an input
        gpio_set_dir(set_ln, GPIO_IN);
        gpio_pull_up (set_ln);

        ++sel_line; // Next column
        if (sel_line >= COL_SZ) // we have scanned all the lines
        {
            // Did a key change?
            int diff = memcmp (cur_scan, prv_scan, COL_SZ);
            if (diff != 0) // Something changed in the key map
            {
                // Set non-zero to flag all keys are up
                int all_keys_up = 0;
                /* If the previous map had keys down, and the current map does not
                 * then we must have released all the keys - make sure a key up is
                 * sent to the USB HID stack later. */
                if (all_off_count >= COL_SZ)
                {
                    all_keys_up = COL_SZ;
                }
                else // some key must be pressed
                {
                    all_keys_up = 0;
                }

                // Something changed, scan the current set and process accordingly
                process_keys (all_keys_up);
                // Record the new state
                memcpy (prv_scan, cur_scan, COL_SZ);
            }

            // Restart scan sequence
            sel_line = 0;
            all_off_count = 0;
        }
    }
} // scan_thread

// main - initialize the board, start tinyusb, start the worker thread
int main()
{
    board_init();

    // Try to grab the Pico board ID info.
//    pico_unique_board_id_t id_out;
//    pico_get_unique_board_id (&id_out);

    // Query the Pico serial number and use that to populate the serial number of the USB status
    char id_string [(2 * PICO_UNIQUE_BOARD_ID_SIZE_BYTES) + 1]; // Should be 17 - PICO_UNIQUE_BOARD_ID_SIZE_BYTES == 8
    pico_get_unique_board_id_string (id_string, 17);

    set_serial_string (id_string);

    // enable the board LED - we flash that to show USB state etc.
    const uint LED_PIN = PICO_DEFAULT_LED_PIN;
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    int idx;
    // GPIO pins 2 to 11 (ten pins) drive the select lines
    // These are normally high but driven low to select a line
    // Set them as inputs until we are ready to drive the line
    for (idx = 2; idx <= 11; ++idx)
    {
        gpio_init (idx);
        gpio_set_dir (idx, GPIO_IN);
        gpio_pull_up (idx);
    }

    // GPIO pins 12 to 19 (8 pins) test the rows
    // These are pulled high
    for (idx = 12; idx <= 19; ++idx)
    {
        gpio_init (idx);
        gpio_set_dir (idx, GPIO_IN);
        gpio_pull_up (idx);
    }

    // Clear the previous and current keyboard scan states
    for (idx = 0; idx < COL_SZ; ++idx)
    {
        prv_scan [idx] = 0; // previous scan
    }

    for (idx = 0; idx < COL_SZ; ++idx)
    {
        cur_scan [idx] = 0; // current scan
    }

    tusb_init(); // start tinyusb

#ifdef SER_DBG_ON
    stdio_init_all(); // Enable the Pico serial port
    printf ("\n-- Keyboard test starting --\n");
#endif // SER_DBG_ON

    // Start the keyboard scanner thread on core-1
    multicore_launch_core1 (scan_thread);
    // Wait for scan_thread() to start up
    uint32_t g = multicore_fifo_pop_blocking();

    // cursory check that core-1 started OK
    if (g == 99)
    {
#ifdef SER_DBG_ON
        printf ("Core-1 started OK\n");
#endif // SER_DBG_ON
    }
    else
    {
#ifdef SER_DBG_ON
        printf ("Core-1 not started correctly\n");
#endif // SER_DBG_ON
    }

    // forever - read keycodes from core-1 and pass them to the hid_task() for sending
    while (true)
    {
        if (multicore_fifo_rvalid ()) // data pending in FIFO
        {
            uint32_t uv = multicore_fifo_pop_blocking();
            // queue the key-down
            kc_put (uv);

#ifdef SER_DBG_ON
            if (uv != FLAG_ALL_UP)
            {
                // diagnostic - echo the keycode to the serial i/o
                printf ("  %08X \b\b\b\b\b\b\b\b\b\b\b", (unsigned)uv);
            }
#endif // SER_DBG_ON
        }

        tud_task(); // tinyusb device task
        led_blinking_task(); // LED heartbeat (in usb-stack.c)
        hid_task(); // HID processing task (in usb-stack.c)
    }
    return 0;
} // main

// end of file
