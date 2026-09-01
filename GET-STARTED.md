# Get started

Goal: a sensor on your desk controlling a number, learned from three
demonstrations you gave it, in about twenty minutes. Most of that is
downloading.

Work top to bottom. If something doesn't match what you see on screen, that's
worth telling me — these instructions are only as good as the last person who
followed them.

---

## What you need

- The ES3C28P board (ESP32-S3 with a 240×320 screen)
- An Adafruit BNO055 orientation sensor and a STEMMA QT cable
- A USB-C cable **that carries data**. A charge-only cable will look like a
  dead board and cost you an hour. If no port shows up later, suspect the cable
  before anything else.
- **Processing**, free from processing.org, but only for `iris_scope/` — the
  sketch that draws the mapping. Every other sketch here needs nothing but the
  Arduino IDE, and the board prints the same numbers as plain text if you would
  rather not install it.

## 1. Get these files, and the Arduino IDE

Download this repository: the green **Code** button on its GitHub page →
**Download ZIP**, then unzip it somewhere you will find again. It unpacks into a
folder with `iris_tilt/`, `iris_instrument/` and this page inside it. If you use
git, `git clone` the same URL instead — it makes no difference to anything
below.

Then install Arduino IDE 2.x from arduino.cc.

## 2. Add the ESP32 boards

Open the preferences window — **Arduino IDE → Settings…** on a Mac (or press
**⌘,**), **File → Preferences** on Windows and Linux. It is not under File on a
Mac, which stops people at this exact step.

In **Additional boards manager URLs**, paste in:

```
https://espressif.github.io/arduino-esp32/package_esp32_index.json
```

Then **Tools → Board → Boards Manager**, search `esp32`, install
**esp32 by Espressif Systems**. It's a large download.

## 3. Install three libraries

**Tools → Manage Libraries**, then search for and install each of:

- **Adafruit BNO055**
- **Adafruit GFX Library**
- **Adafruit ILI9341**

When it offers to install dependencies too, say yes — that's Adafruit BusIO and
Adafruit Unified Sensor. Five libraries in total; you only ask for three.

> **You do not install iris.** `iris.h` is sitting in the sketch folder and
> Arduino picks up headers next to a sketch automatically.
>
> This is worth a second of your attention, because it's the point of the whole
> project. The Adafruit libraries are drivers — they talk to a specific screen
> and a specific sensor, and if one of them breaks you swap it for another and
> your instrument plays exactly as it did before. The code that decides *how
> your gesture becomes sound* is the part that must never move under you, and
> that part has no dependencies at all. One file, one compiler, no install step
> that can fail in three years.

## 4. Board settings

**Tools → Board → esp32 → ESP32S3 Dev Module**, then set every one of these:

| Setting | Value |
|---|---|
| USB Mode | **USB-OTG (TinyUSB)** |
| USB CDC On Boot | **Enabled** |
| Flash Size | **16MB (128Mb)** |
| PSRAM | **OPI PSRAM** |
| Partition Scheme | **16M Flash (3MB APP/9.9MB FATFS)** |

These are not defaults and they are not optional. I verified this exact
combination on this exact board.

**The two that bite.** *USB CDC On Boot = Disabled* gives you a board that runs
fine and cannot talk to you — Serial Monitor stays empty forever. *USB Mode =
Hardware CDC and JTAG* used to build without complaint and then the board would
never appear as a MIDI device, so the second sketch looked broken when it
wasn't.

The sketches refuse to build with either of those wrong -- seven of the ten
carry the guard, including both on this page -- and the error
names the menu item to fix. If you see a red message mentioning USB Mode, that
is this check doing its job — read it, change the setting, upload again. It is
the only mistake here the compiler can catch for you, which is why it does.

## 5. Plug the sensor in

Connect the BNO055 to the board with the STEMMA QT cable — the little
four-pin connector, one end into the sensor, the other into the matching socket
on the board. It clicks when it is properly seated, and a cable that looks
seated but isn't is the single most common reason for "No BNO055."

There is nothing to solder and no wires to get the right way round; the
connector only fits one way. If you are wiring by hand instead, the data line
is pin 16 and the clock is pin 15.

## Before the sketches: see the mathematics

If you want to understand what this library is doing rather than only use it,
run **`iris_scope/`** first. It needs the same board and the same sensor, and
instead of making a sound it draws the mapping: your demonstrations as dots, and
the curve the network invented between them. There is a Processing sketch in
`iris_scope/processing/` that draws it, and the board also prints the numbers as
plain text if you would rather read them.

Both views are on screen at once. TRANSFER shows what it plays against what you did.
SCOPE plots the two outputs against each other like an oscilloscope, which is
where the nonlinearity becomes a shape you can see.

The rest of this page is the sound route. Come back here when the picture stops
surprising you.

## 6. Open the first sketch

**File → Open**, navigate to `iris_tilt/` and open **`iris_tilt.ino`**.

Open the `.ino` file itself, or the folder that has the same name as it —
Arduino requires a sketch folder and its main file to share a name, so opening
this repo's top-level folder will not work.

## 7. Flash it

Plug the board in. **Tools → Port** and pick the one that appeared. Press
**Upload** (the arrow). Then **Tools → Serial Monitor**, and set the baud rate
to **115200**.

## 8. Play it

```
tilt, then press BOOT -- or send any key here except R  ->  demo 1 of 3 (target 0)
```

1. Tilt the board somewhere. Press **BOOT**. That pose is now `0`.
2. Tilt somewhere else. Press **BOOT**. That's `64`.
3. A third pose. Press **BOOT**. That's `127`.

Can't reach BOOT? Type a character into Serial Monitor and hit send — that
records too, and `c` clears.

**One exception: a capital `R` reboots the board into flashing mode** rather
than recording, because that is the escape hatch described at the bottom of this
page. If your board goes quiet and the port disappears after you typed
something, that is what happened — nothing is broken, just upload again. The
sketch prints the same reminder every time it asks you for a pose.

It trains, tells you how long that took, and starts printing a number that
follows the board as you move it.

**Now move it somewhere you never demonstrated.** The number still does
something sensible. You gave it three points and got back a continuous surface,
and that surface is the instrument. That is the entire idea, and everything else
in this project is detail.

---

## Where the rest of it lives

This tree is the hardware half. The library itself, its decision records and
its measurements are in the `iris/` repository next to this one:

- `iris/iris.h` — the library, and the place the mathematics is written down
- `iris/examples/` — four programs that run on a laptop with no board at all,
  including one that draws the learned surface as ASCII art
- `iris/docs/adr/` — 21 numbered decision records, each with the measurement
  behind it. If you ever wonder "why is it like that", the answer is there.
- `iris/docs/DEGREES-OF-FREEDOM.md` — why adding outputs is nearly free and
  adding inputs is not

## Don't have the board? Start here instead

Everything above assumes the ES3C28P and a BNO055. If you have something else —
an Uno, a Pico, a Teensy, a bare ESP32 — open `boilerplate/any_sensor/` instead.
It is the same instrument with the hardware taken out: five marked places to
fill in, and it compiles and runs before you change anything, using two
analogue pins as a stand-in sensor so you can press the button and watch it
learn while your real parts are still in the post.

Verified on an Arduino Uno, both Raspberry Pi Pico cores and the ESP32-S3 —
compiling *and* training, which are different claims. On the Uno the whole
thing fits in 2 kilobytes of RAM with about 176 bytes of stack to spare, and it
only fits because the file shrinks the library's working arrays on AVR (see the
`IRIS_MAX_IN` block at the top). Measured with `avr-gcc -Os -fstack-usage`:
the deepest frame is 128 bytes with that block and 288 without it, against 376
bytes of free stack.

**Only `boilerplate/any_sensor/` builds for an Uno — 1 of the 10 sketches here.**
Measured 2026-08-30 with `arduino-cli compile --fqbn arduino:avr:uno` on all ten.
That is not a defect: the other nine need a screen, an ESP32's USB stack, or
more RAM than an Uno has, and `boilerplate/bno055_portable/` says so with a build error.
The rest fail with compiler messages rather than an explanation, which is worth
knowing before you try one.

`boilerplate/stemma_bno055/` is the same file wired to a real sensor over I2C
(inter-integrated circuit, the two-wire bus the STEMMA QT cable carries),
if you want to see what filling in those five places actually looks like.

## Then: the one that makes sound

`iris_instrument/iris_instrument.ino` is the same learning core with a screen
and real USB-MIDI. You drag three bars to set a sound, hold a pose, tap RECORD,
and after two demonstrations it plays — one tilt moving three parameters
together in the relationship you showed it. It shows up as a MIDI device in
Ableton or any synth. Same board settings, no new libraries.

## Keeping an instrument

Everything above dies when you unplug the board. The demonstrations live in
memory, and memory goes away with power.

`boilerplate/stemma_bno055/` is the one that keeps them. **Hold SAVE for a
second** and the whole instrument — the demonstrations and the trained network —
is written to the board's flash. It comes back by itself the next time you power
up, and the sketch tells you which happened:

```
loaded the instrument from last time.
```

Two calls do it, and they are the same two on any board with somewhere to put
bytes:

```c
size_t n = iris_save(k, buffer, sizeof buffer);   /* returns bytes written, 0 if it refused */
iris_load(k, buffer, n);                          /* returns 0 if the bytes are damaged */
```

`iris_save` writes a checksum and `iris_load` checks it, so a half-written or
corrupted file is refused rather than played as an instrument you did not make.
You do not have to know the format. If you want the size before you write
anything, `iris_save_size(k)` tells you.

The other sketches deliberately do not save, so that the first thing you flash
is as short as it can be. Copy the two calls out of `stemma_bno055` when you
want an instrument to outlive the cable.

## Fixing a bad demonstration

If you record a pose you did not mean, you do not start over.
`boilerplate/any_sensor/` takes **`d`** in the Serial Monitor to delete the last
demonstration and retrain, and **`c`** to clear everything. That loop —
demonstrate, listen, delete the bad one, demonstrate again — is the point of
teaching by showing rather than by typing numbers.

`display_check/display_check.ino` is a hardware triage sketch. It draws to the
screen and reports touches, nothing else. If the display or touch seems dead,
run this before you debug anything more interesting.

---

## When it doesn't work

**No port in Tools → Port.** Try a different USB-C cable first. Many are
charge-only.

**Serial Monitor is empty.** Baud rate 115200, and check *USB CDC On Boot* is
Enabled.

**"No BNO055."** The sketch prints every address answering on the two-wire
I2C bus, so
you can see whether the sensor is there at all. It tries both 0x28 and 0x29.
Reseat the STEMMA cable — it clicks when it's seated.

**The board stops accepting uploads.** This is the one that costs people an
afternoon, so read it before you need it.

The ESP32-S3 can latch into a state where the IDE stops seeing its port. The
physical recovery is: **hold BOOT, tap RESET, release BOOT.** That puts it in
download mode ready to flash.

Both sketches also carry a software escape hatch, because a board in an
enclosure has no reachable buttons. Send **`R`** over Serial and it reboots into
the bootloader, ready to flash. `iris_instrument` additionally answers to **CC
123 value 127 on MIDI channel 16**, for when it is enumerated as a MIDI device
and you have no serial terminal open.

If you write your own sketch, build the escape hatch in **before** you flash it,
not after. That is the whole lesson of this section.

**The sound jumps instead of sweeping.** If you've modified the sketch to read
orientation in degrees, that's why. Euler angles wrap from +180 to −180, so two
poses a degree apart arrive as opposite ends of the range and no smooth mapping
can survive it. Both sketches read the gravity vector instead, which points down
and never wraps.
