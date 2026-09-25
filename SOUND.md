# Getting sound out

The board makes no sound itself in any sketch here. It sends numbers to a
computer over the USB (Universal Serial Bus) cable, and something on the
computer turns them into sound. There are two ways to send them.

## MIDI: `iris_instrument`

`iris_instrument` appears to the computer as a USB MIDI device (MIDI: Musical
Instrument Digital Interface, the message format synthesisers understand). It
sends three CC messages (control change: a numbered controller set to a value
from 0 to 127): controllers 1, 2 and 3 on channel 1, one per bar on the
screen, each time a value changes. Build and upload it with the settings in
GET-STARTED.md, step 4; USB Mode must be USB-OTG (TinyUSB).

To see it on the computer:

- **Mac:** open Audio MIDI Setup (Applications → Utilities), then
  **Window → Show MIDI Studio**. The board is listed there while it is
  plugged in.
- **Windows:** the board appears as a MIDI input in the settings of any music
  program.
- **Linux:** `amidi -l` in a terminal lists it.

The sketch leaves the names at the board package's defaults (esp32 3.3.3):
the USB device is called `ESP32S3_DEV` and its MIDI port `TinyUSB MIDI`.
Programs differ in which of the two they show.

In a synthesiser, choose the board as the MIDI input, then assign controllers
1, 2 and 3 to the sounds you want to move. Most synthesisers call this MIDI
learn: click the control on screen, then drag one bar on the board, and the
synthesiser takes that bar's controller. Controller 1 is the modulation wheel
on most synthesisers, so it often does something before you assign anything.

The board restarts into its loader if it receives controller 123 at value 127
on channel 16. That is the escape hatch for a board that stops accepting
uploads; nothing else in the sketch listens for incoming MIDI.

## Plain text over the serial port

The other sketches print their outputs as lines of text on the USB serial
port at 115200 baud (bits per second), which Max, Pure Data and Processing
can all read. Only one program can hold the port at a time, so close the
Arduino IDE's (integrated development environment's) Serial Monitor first.

**`iris_tilt`**, once it has its three poses, prints a line about every
100 milliseconds (`iris_tilt.ino`, the `Serial.printf` at the end of
`loop`):

```
tilt  -3.21   4.05  ->   87  #####################
```

The first word is `tilt`, then the two gravity readings, `->`, and the value
the network plays: a whole number from 0 to 127, the range of one MIDI
controller. The row of `#` is the same value drawn as a bar (one `#` per 4)
and is absent below 4. No other line the sketch prints starts with the word
`tilt` on its own; the prompt starts with `tilt,`.

**`boilerplate/stemma_bno055`, `boilerplate/any_sensor` and
`boilerplate/bno055_portable`**, once they hold two demonstrations, print a
line on every pass of `loop`, which waits 20 milliseconds, so up to fifty
lines a second (the `send_sound` function in the first two, the end of
`loop` in the third):

```
0.412 0.873 0.100
```

Three numbers from 0 to 1, three decimal places, separated by spaces, one per
output. Status lines such as `trained.` and `demonstrations: 2` are mixed
in, so keep only the lines made of exactly three numbers.

`iris_scope` prints its live outputs on `L` lines; its README lists the
format.

Most lines end in a carriage return and a newline (bytes 13 and 10); some of
`iris_tilt`'s status lines end in a newline alone. Split on the newline and
throw away any carriage return.

### Reading the lines

The objects named below are the usual ones for this job; the recipes have not
been run against these sketches.

- **Processing:** the Serial library. Open the port at 115200, call
  `bufferUntil('\n')`, and in `serialEvent` read the line, `trim` it and
  `splitTokens` it at spaces. `iris_scope/processing/iris_scope/iris_scope.pde`
  reads the board this way (its `serialEvent`).
- **Max:** `[serial]` at 115200, banged by a `[metro]`, gives one byte per
  number. Collect bytes with `[zl group]` and send the collected list on when
  a 10 arrives (`[sel 10 13]` catches the newline and drops the carriage
  return). `[itoa]` turns the bytes into text and `[fromsymbol]` into a list
  of words and numbers. `[route tilt]` keeps `iris_tilt`'s lines; `[unpack]`
  takes a line apart. From there, `[ctlout]` sends a value on as MIDI, or it
  can drive anything in the patch directly.
- **Pure Data:** `[comport]`, installed through **Help → Find externals**,
  opened at 115200, gives one byte per number. Collect them into a list until
  a 10 arrives, drop the 13s, and turn the list into a message with
  `[fudiparse]` (in Pure Data's own objects). Then `[route tilt]` and `[unpack]` as in Max.
