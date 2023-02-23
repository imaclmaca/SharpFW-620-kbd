# SharpFW-620
Use a Raspberry Pico to decode the keyboard matrix of an old Sharp Fontwriter

I have an old Sharp Fontwriter-620 I picked up on eBay for £15 (which mostly still works, even the 3.5" floppy disk,
but it is virtually impossible to get printer ribbons for them any more.)

So I'm "refurbishing" it with a Raspberry-Pi internally, and losing the old printer mechanism and the FDD.

It has quite a nice keyboard (albeit early 90's rubber dome, not actual switches) so I created this basic Pico tool to
decode the matrix and allow me to use it as a basic USB keyboard.
The 90's Sharp keyboard layout is somewhat "idiosyncratic", so I have made some accommodations to allow for that here.

The code here attempts to "interpret" the more unusual of the FontWriter keys and then emit (to the USB HID interface)
various key combinations that (at least for a debian-based Linux with a UK key map loaded) produces the "expected" symbol
on the screen.

FWIW, I doubt this will work quite right with anything other than a UK-key map under Linux, though the basic key mapping
(i.e. for the non-weird keys) is likely to be just fine.
In particular, I doubt this will would produce the expected combined keys under Windows, which tends to use different
mappings (though again the "basic" keys should all be fine.)

Note, also, that the matrix has no diodes between the lines, so if you press too many keys you can get weird shadow key
effects. I've tried to trap for that, but offer no guarantees.

Just, do not use this as a gaming keyboard, basically. It's fine for actual typing!

# Hardware configuration
The Sharp FontWriter keyboard matrix is basically an 8 x 10 mapping, though not all 80 possible key positions are used.

I initially planned to use the Pico PIO to scan the matrix, but given that an 8 x 10 matrix only needs 18 GPIO lines,
and the Pico has plenty more GPIO than that, I just scanned in software as an initial test - which worked well, so the PIO
never happened.

The Pico drives each of the 10 columns in turn, then checks the 8 rows for the pressed keys. It then determines the key(s)
that are required and sends them to the USB as a pretty basic HID keyboard device.

(This only does the HID version, it does not do the BOOTP version, so probably not a good idea to use this to access the BIOS
of your old PC...)

It is fine for my needs!
