# Get started

Goal: a sensor on your desk controlling a number, learned from three
demonstrations you gave it, in about twenty minutes. Most of that is
downloading.

Work top to bottom. If something doesn't match what you see on screen, say
so: these instructions are only as good as the last person who followed them.
When something goes wrong, [TROUBLESHOOTING.md](TROUBLESHOOTING.md) lists each
symptom, its likely cause and what to do.

Steps 1 to 5 install and set up. After that, the order of this page is the
order to run the sketches in:

- step 6, `i2c_find` — is the sensor wired right?
- step 7, `iris_tilt` — the first instrument: tilt, teach three poses, play
- step 8, `iris_scope` — see the mapping the network invents, drawn as a curve
- step 9, `iris_instrument` — the same learning with a screen and sound: MIDI
  (Musical Instrument Digital Interface, the message format synthesisers
  understand) over the USB (Universal Serial Bus) cable
- step 10, `boilerplate/stemma_bno055` — an instrument that survives a power
  cycle

`boilerplate/any_sensor` is the starting point on any other board.
`display_check`, `determinism_check`, `device_torture` and `board_probe` are
checks you run when you need them (the end of this page says when).

---

## What you need

[PARTS.md](PARTS.md) lists every part with its part number, where to buy it,
and what connects to what. In short:

- The ES3C28P board: an ESP32-S3 (Espressif's microcontroller, the chip that
  runs your sketch) with a 240×320 touch screen
- An Adafruit BNO055 orientation sensor (part 4646), the lead that ships with
  the board, and an Adafruit 4209 cable to join them
- A USB-C cable **that carries data**. A charge-only cable will look like a
  dead board and cost you an hour. If no port shows up later, suspect the cable
  before anything else.
- **Processing**, free from processing.org, but only for `iris_scope/` — the
  sketch that draws the mapping. Every other sketch here needs nothing but the
  Arduino IDE, and the board prints the same numbers as plain text if you would
  rather not install it.

## 1. Get these files, and the Arduino IDE

Download this repository: the green **Code** button on its GitHub page
(https://github.com/kylebsmith/iris-starter) → **Download ZIP**, then unzip it
somewhere you will find again. It unpacks into a folder with `iris_tilt/`,
`iris_instrument/` and this page inside it. If you use git, `git clone` the
same address instead — it makes no difference to anything below.

Then install Arduino IDE 2.x (IDE: integrated development environment, the
program you write and upload sketches with) from arduino.cc.

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

When it offers to install dependencies too, say yes. You ask for three and
nine arrive: those three, plus Adafruit BusIO, Adafruit Unified Sensor,
Adafruit SH110X, Adafruit STMPE610, Adafruit TouchScreen and Adafruit TSC2007,
which the ILI9341 library lists as its dependencies. The sketches use only the
first five.

> **You do not install iris.** `iris.h`, the library itself, is copied into
> every sketch folder that uses it, and Arduino picks up headers next to a
> sketch automatically. Every copy is iris **0.2.0**, from
> https://github.com/kylebsmith/iris, and every sketch that uses it checks at
> compile time that its copy is iris 0.2: a different version stops the
> build with a message saying which file to copy.
>
> The Adafruit libraries are drivers — they talk to a specific screen and a
> specific sensor, and if one of them breaks you swap it for another and your
> instrument plays exactly as it did before. The code that decides *how your
> gesture becomes sound* is the part that must never move under you, and that
> part has no dependencies at all.

## 4. Board settings

**Tools → Board → esp32 → ESP32S3 Dev Module**, then set these:

| Setting | Value | What it is |
|---|---|---|
| USB Mode | **USB-OTG (TinyUSB)** | USB is Universal Serial Bus. USB-OTG (On-The-Go) is the chip's full USB controller, driven by TinyUSB, an open-source USB software stack; it is what lets the board appear as a MIDI instrument. The other choice, **Hardware CDC and JTAG**, is the chip's fixed serial-and-debug port (CDC: Communications Device Class, the USB standard for a serial port; JTAG: Joint Test Action Group, a debugging interface). |
| USB CDC On Boot | **Enabled** | Makes the board's USB socket the serial port Serial Monitor reads. |
| Flash Size | **16MB (128Mb)** | The board's flash memory chip: 16 megabytes. |
| PSRAM | **OPI PSRAM** | PSRAM (pseudo-static random-access memory) is the board's extra memory chip; OPI (octal peripheral interface) is the 8-wire link to it. |
| Partition Scheme | **16M Flash (3MB APP/9.9MB FATFS)** | How the flash is divided: 3 MB for your sketch (the app), 9.9 MB for files (FATFS: a FAT file system; FAT, file allocation table, is the format memory cards use). |
| Upload Mode | **USB-OTG CDC (TinyUSB)** | How the computer puts the board into its loader before an upload. With the USB mode above, a sketch owns the USB port, and this choice makes the upload tap the port at 1200 baud (bits per second) so the sketch restarts into the loader by itself. The other choice, UART0 / Hardware CDC (UART: universal asynchronous receiver-transmitter, the chip's plain serial port), cannot reach a board running a TinyUSB sketch: the upload stops with `Failed to connect to ESP32-S3: No serial data received`. |

This combination is the one tested on this board. **The first two change
whether a sketch works, and Upload Mode decides whether the second and later
uploads start by themselves;** the other three match the board's memory and
leave room to grow.

*USB CDC On Boot = Disabled* gives you a board that runs fine and cannot talk
to you — Serial Monitor stays empty. Every sketch refuses to build with it
off, and the error names the menu item to fix.

*USB Mode* matters only to `iris_instrument`, which appears to the computer as
a USB MIDI instrument (MIDI: Musical Instrument Digital Interface, the message
format synthesisers understand) and needs USB-OTG (TinyUSB); it refuses to
build in the other mode. Every other sketch also builds in Hardware CDC and
JTAG mode, the board's default, but only `iris_tilt` has been run in that
mode on this board ([board log](board-logs/2026-09-25-es3c28p.md)), so stay
with the table.

`iris_tilt`, `iris_instrument`, `display_check` and `board_probe` also refuse
to build if **Tools → Board** is another ESP32 entry. The rest are written to
build for any ESP32, so with the wrong entry they build and the upload fails
instead, because the chip is not the one the build was for. If you see a red
message starting `Set Tools ->`, read it, change the setting, upload again.

## 5. Connect the sensor

The board's I2C socket (I2C, inter-integrated circuit: the two-wire bus that
carries data and clock on two pins) is **1.25 mm** pitch. The sensor's STEMMA
QT socket (Adafruit's 4-pin I2C connector) is **1.0 mm**, so a STEMMA QT
cable does not fit the board.
[PARTS.md](PARTS.md) shows the join without soldering: the board's own lead
into the board, Adafruit's 4209 cable into the sensor, and 4209's four pins
into the lead by signal — 3.3 V, ground, data (SDA) to GPIO 16, clock (SCL) to
GPIO 15 (GPIO: general-purpose input/output, a numbered pin of the chip).
Check which of the lead's wires is which against the board's silkscreen before
you power up.

## 6. Check the wiring: `i2c_find`

**File → Open**, open `i2c_find/i2c_find.ino`. Plug the board in, pick the port
that appeared in **Tools → Port**, press **Upload** (the arrow), then open
**Tools → Serial Monitor** at **115200** baud (the serial speed).

It lists every device answering on every likely pin pair. On SDA 16 / SCL 15
you should see **0x28** (or 0x29), the BNO055, next to **0x18** and **0x38**,
the board's own audio codec and touch controller. If only those two answer,
the sensor is not connected: it says so. Fix that before going on.

## 7. The first sketch: `iris_tilt`

**File → Open**, navigate to `iris_tilt/` and open **`iris_tilt.ino`**.

Open the `.ino` file itself, or the folder that has the same name as it —
Arduino requires a sketch folder and its main file to share a name, so opening
this repo's top-level folder will not work.

Upload it and open Serial Monitor at 115200, as in step 6.

```
tilt, then press BOOT -- or send any key here except R  ->  demo 1 of 3 (target 0)
```

1. Tilt the board somewhere. Press **BOOT**. That pose is now `0`.
2. Tilt somewhere else. Press **BOOT**. That's `64`.
3. A third pose. Press **BOOT**. That's `127`.

Can't reach BOOT? Type a character into Serial Monitor and hit send — that
records too. To start over, send `c`, or hold BOOT for two seconds.

**One exception: a capital `R` restarts the board into its loader** rather
than recording: it is the escape hatch in TROUBLESHOOTING.md for a board that
stops accepting uploads. If the board goes quiet and the port disappears after
you typed something, that is what happened — nothing is broken, upload again.
The sketch prints the same reminder every time it asks you for a pose.

It trains, tells you how long that took, and starts printing a number that
follows the board as you move it.

**Now move it somewhere you never demonstrated.** The number still does
something sensible. You gave it three points and got back a continuous surface,
and that surface is the instrument. That is the entire idea, and everything else
in this project is detail.

## 8. See the mapping: `iris_scope`

`iris_scope/` needs the same board and sensor, and instead of printing one
number it draws the mapping: your demonstrations as dots, and the curve the
network invented between them. Upload `iris_scope.ino`, **close Serial
Monitor**, then run `iris_scope/processing/iris_scope/iris_scope.pde` in
Processing. [iris_scope/README.md](iris_scope/README.md) walks through it.

Both views are on screen at once. The big plot shows what it plays against
where the sensor is. The square at the top right plots the two outputs against
each other like an oscilloscope, which is where the nonlinearity becomes a
shape you can see. Press `d` and the last demonstration leaves the picture and
the curve relaxes.

## 9. Sound: `iris_instrument`

`iris_instrument/iris_instrument.ino` is the same learning core with a screen
and USB MIDI. You drag three bars to set a sound, hold a pose, tap RECORD, and
after two demonstrations it plays — one tilt moving three parameters together
in the relationship you showed it. Tap CLR to start over. Same board settings,
no new libraries.

The bars go out as MIDI controllers 1, 2 and 3 on channel 1, to any
synthesiser or music program that accepts USB MIDI.
[SOUND.md](SOUND.md) says how to find the board on your computer, and how to
get sound from the other sketches, which print their outputs as plain text
that Max, Pure Data or Processing can read.

## 10. Keeping an instrument: `boilerplate/stemma_bno055`

Everything above dies when you unplug the board. The demonstrations live in
memory, and memory goes away with power.

`boilerplate/stemma_bno055/` keeps them. It reads the sensor and three knobs
(wiring in PARTS.md), and its SAVE button (the board's BOOT button) does two
things:

- **Tap SAVE** (let go within a second): records one demonstration — the pose
  and the knobs as they were when you pressed — and starts training in the
  background. The sketch prints `demonstrations: N`, then `trained.` when the
  training finishes. It plays as soon as there are two.
- **Hold SAVE** for a second or longer: records nothing. If training is still
  running it prints `finishing training before saving...` and finishes it.
  Then it writes the instrument you are playing — every demonstration and the
  trained network — to the board's flash and prints
  `kept. 464 bytes in flash, 3 demonstrations, trained.` (the numbers are
  yours).

On the next power-up it prints `loaded the instrument from last time.` and
plays exactly what you were playing when you held SAVE. An instrument saved
untrained with two or more demonstrations (its training had failed) retrains
from them straight after loading. One saved with a single demonstration
plays nothing until the next tap records a second.

Two calls do the saving, and they are the same two on any board with somewhere
to put bytes:

```c
size_t n = iris_save(k, buffer, sizeof buffer);   /* returns bytes written, 0 if it refused */
iris_load(k, buffer, n);                          /* returns 0 if the bytes are damaged */
```

`iris_save` writes a checksum and `iris_load` checks it, so a half-written or
corrupted file is refused rather than played as an instrument you did not make.
You do not have to know the format. If you want the size before you write
anything, `iris_save_size(k)` tells you.

The other instruments deliberately do not save, so that the first thing you
flash is as short as it can be. Copy the two calls out of `stemma_bno055`
when you want an instrument to outlive the cable.

## Fixing a bad demonstration

If you record a pose you did not mean, you do not start over.
`boilerplate/any_sensor/` and `iris_scope` take **`d`** to delete the last
demonstration and retrain, and **`c`** to clear everything (in the Serial
Monitor for `any_sensor`, in the plot window for `iris_scope`). `iris_tilt`
clears with `c`, and `iris_instrument` with its CLR button.
That loop — demonstrate, listen, delete the bad one, demonstrate again — is the
point of teaching by showing rather than by typing numbers.

## Don't have the board? Start here instead

Everything above assumes the ES3C28P and a BNO055. If you have something else —
an Uno, a Pico, a Teensy, a bare ESP32 — open `boilerplate/any_sensor/` instead.
It is the same instrument with the hardware taken out: five marked places to
fill in, and it compiles and runs before you change anything, using two slow
waves it makes itself as a stand-in sensor so you can press the button and
watch it learn while your real parts are still in the post.

It builds for an Arduino Uno, both Raspberry Pi Pico cores and the ESP32-S3.
On an Uno it uses 81% of the 2 kilobytes of memory and fits only because the
file shrinks the library's working arrays on that chip (the `IRIS_MAX_IN`
block at its top). It is the only sketch here that builds for an Uno: the
others need a screen, an ESP32's USB, or more memory than an Uno has.

`boilerplate/bno055_portable/` is the same instrument wired to a real BNO055,
for boards other than the ES3C28P that have more memory than an Uno.

## The checks

- `display_check` draws to the screen and reports touches, nothing else. If
  the display or touch seems dead, run this before you debug anything more
  interesting.
- `determinism_check` trains a fixed recipe and prints PASS when the board
  builds exactly the instrument a laptop builds, bit for bit.
- `device_torture` asks the library questions on this board — same bits as
  the laptop, saving through real flash, corrupted files refused, drift,
  several instruments at once — and prints PASS, FAIL or a labelled figure
  for each.
- `board_probe` measures how long one prediction and each kind of training
  take on this board, with the processor's cycle counter, and prints one
  block to keep as a record.

## Where the rest of it lives

This tree is the hardware half. The library itself, its decision records and
its measurements are at https://github.com/kylebsmith/iris (these copies are
version 0.2.0):

- `iris.h` — the library, and the place the mathematics is written down
- `examples/` — programs that run on a laptop with no board at all
- the library's decision records, each with the measurement behind it: where
  to look when you wonder "why is it like that".
