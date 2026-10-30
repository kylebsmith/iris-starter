# About this kit

The reasoning behind the starter kit: what the sketches prove, what they do not,
which parts you may replace and which you may not, and how I would like you to
work. None of it is needed to build an instrument — [README.md](README.md) is
that — but all of it is why the kit is shaped the way it is.

## How much room it takes

What the compiler reports for three of them, in thousands of bytes, from
`arduino-cli compile` with the settings in GET-STARTED.md, step 4 (the
`FQBN_DOCUMENTED` line of `.github/workflows/sketches.yml`; esp32 board
package 3.3.3):

| sketch | flash (program storage) | RAM (random-access memory, the working memory) |
|---|---|---|
| `iris_tilt` | about 404 (12%) | about 46 (14%) |
| `display_check` | about 409 (13%) | about 46 (13%) |
| `iris_instrument` | about 424 (13%) | about 47 (14%) |

The percentages are the ones arduino-cli prints, rounded down. The workflow
prints the exact byte counts on every run. They move a little with the
Adafruit library versions, with how the board options are spelled and with
the operating system that builds them, because the board package compiles
the option text and that system's name into the program (`-DARDUINO_FQBN`
and `-DARDUINO_HOST_OS` in its `platform.txt`).

Almost all of each figure in the table is the Arduino and USB runtime. iris
itself is a few kilobytes: `iris_tilt`'s whole instrument — weights,
demonstrations and all — is its 944-byte `memory` array,
`IRIS_ARENA(2, 12, 1, 8)`. That figure is a compile-time assertion with the
ESP32-S3's own compiler (xtensa-esp32s3-elf-gcc 14.2.0); the same macro gives
1,024 bytes on a 64-bit laptop (Apple clang and gcc 15 on a 64-bit Arm
processor), because pointers there are twice as wide.

I2C (inter-integrated circuit) is the two-wire bus the sensor talks on.

## What the first sketch does

`iris_tilt`, step 7 of [GET-STARTED.md](GET-STARTED.md), is the first sketch
the walk runs. Read it top to bottom. It is short so that you can follow every
line.

**It reads the gravity vector off the sensor** over I2C, using the Adafruit
BNO055 library. Gravity, not orientation in degrees — Euler angles wrap from
+180 to −180, and two poses a degree apart then arrive as opposite ends of the
range. Nothing smooth can fit that. Gravity points down and never wraps.

Gravity cannot see a turn about the vertical, though, and heading as an angle
wraps at 360 degrees and has no value at all when the board points straight
up. For an instrument that should hear which way the player faces,
`boilerplate/stemma_bno055/heading.h` gives two directions in the board's own
frame: UP, from gravity, and a FRONT the player sets by pointing the board and
sending `f`, from the sensor's fusion quaternion. That is six numbers that
move smoothly however the board turns and never wrap; set `USE_HEADING 1` in
that sketch to play from them.

**It does not scale anything.** iris fits its input range to the demonstrations
you actually give it, so raw sensor units work exactly as well as anything you
divide them by — and a sensor mounted slightly off level needs no correction at
all. It's absorbed the moment you record your first pose.

**It records three demonstrations.** You tilt, you press BOOT, it stores that
pose against a target value. That is the entire training interface.

**It trains.** One function call. No epoch count to guess, no learning rate, no
optimiser to choose — it stops when it stops improving, and tells you how long
it took.

**Then it plays.** Every reading goes through the network and out comes a
number. It follows you smoothly through poses you never demonstrated, and *that
is the whole point*. Three points went in. A continuous surface came out. That
surface is your instrument.

## What this proves, and what it doesn't

**Proves:** the library trains and runs on the actual chip, from a real sensor,
with a human doing the demonstrating. Every accuracy number in the library was
measured on clean synthetic data at a desk, and real gestures are noisier and
less consistent than anything it was tuned against.

**Doesn't prove:** that the mapping is any good musically. That isn't a
software question. You answer it by playing.

What the sketches did on an ES3C28P, including the release header's hashes on
the board: [board-logs/2026-09-25-es3c28p.md](board-logs/2026-09-25-es3c28p.md)
for iris 0.2.0, and
[board-logs/2026-09-28-es3c28p.md](board-logs/2026-09-28-es3c28p.md) for
0.3.0 and the sketches that go with it.

## Two libraries, two different risks

The sketches use Adafruit's BNO055, GFX and ILI9341 libraries, and that is
fine. They're drivers. If one breaks you swap it for another and your instrument
plays exactly as it did before.

`iris.h` is the part that can't be treated that way. It decides how your gesture
becomes sound, so if *it* moves, your instrument silently becomes a different
instrument — same demonstrations, different result. That is why it has no
dependencies, why its behaviour is pinned by hashes in its own test suite, and
why it's a single file sitting next to your sketch rather than something you
install. Each sketch that uses it checks at compile time that its copy is
iris 0.3 and names the file to copy when it is not.

You should own your instrument. A stranger's commit shouldn't be able to
restring it.

## Where you take it

The first sketch is deliberately at its floor: three demonstrations, one output,
a number on a screen. What's obviously missing is missing on purpose.

- **More outputs.** Three sound parameters from one gesture is where it starts
  getting musical. `iris_instrument` already does this if you want to read how.
  Doing it yourself to `iris_tilt` means changing the output count in **both**
  `IRIS_ARENA(...)` and `iris_init(...)` — they must agree or the arena is too
  small and `iris_init` refuses — and turning `target` and `out` from single
  floats into arrays. Change only one of those and it will not work.
- **More inputs.** The BNO055 gives you heading and raw acceleration too
  (heading as UP and FRONT, which never wrap: `boilerplate/stemma_bno055/heading.h`),
  and there are free pins for another sensor entirely.
- **A different sensor.** Distance, light, flex, pressure — anything that gives
  you a number.
- **The screen.** Showing the learned space rather than a number changes how it
  feels to train.
- **More demonstrations, and deleting bad ones.** `iris_delete_id` deletes a
  take. `iris_worst_example_id` names the take that fought the others hardest
  in the last training run, once there are at least 12
  (`IRIS_STRESS_MIN_EX`); below that, or before any training, it returns -1.
  `iris_loo_error` answers a different question: one leave-one-out error for
  the whole instrument, how well it predicts takes it did not see. It
  retrains the instrument while it works, so call `iris_train` afterwards.

## What this repo holds itself to

The standard to judge a change against:

- One clone. No submodules, no package manager, no build script to read first.
- Opens in the Arduino IDE (integrated development environment).
- A wrong board setting produces a **compiler error with a human message**,
  never a bricked board. Every sketch stops at compile time when USB CDC On
  Boot (CDC: Communications Device Class; the setting makes the USB socket the
  serial port) is off on the ESP32-S3; `iris_tilt`, `iris_instrument`,
  `display_check` and `board_probe`, which use this board's own pins or
  timer, stop when the board entry is another ESP32; `iris_instrument` stops
  when USB Mode is not USB-OTG (On-The-Go, driven by TinyUSB). Each message
  names the menu item and says why.
- A first sound from zero prior experience, by following GET-STARTED.md from
  the top.
- Everything you add is a module behind the existing interface. The core is
  frozen and is not yours to edit.

## A note on how I want you to work

Use whatever tools you want, including AI (artificial intelligence) ones. I'd rather you were fluent with
them than pretend otherwise. But the thing that makes that work isn't the
prompting, it's the verifying.

Mistakes in this repo, whoever wrote them, get caught the same way: by running
the thing and looking.

So when something you generate compiles, that's the beginning of knowing whether
it's right, not the end. Run it. Tilt the board. Does it do what you said it
would? Same standard whether a person or a model wrote the line.
