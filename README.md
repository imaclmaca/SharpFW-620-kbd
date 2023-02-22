# SharpFW-620
Use a Raspberry Pico to decode the keyboard matrix of an old Sharp Fontwriter

I have an old Sharp Fontwriter-620 I picked up on ebay for £15 (which mostly still works, even the 3.5" floppy disk, but it is
virtually impossible to get printer ribbons for them any more.)

It has quite a nice keyboard (albeit early 90's rubber dome, not actual switches) so I created this basic pico tool to decode the
matrix and allow me to use it as a basic USB keyboard.
The 90's Sharp keyboard layout is somewhat "idiosyncratic", so I have made some accomodations to allow for that here.

Note, also, that the matrix has no diodes between the lines, so if you press too many keys you can get weird shadow key effects.
I've tried to trap for that, but offer no guarantees.
Just, do not use this as a gaming keybaord, bascially. It's fine for actual typing!
