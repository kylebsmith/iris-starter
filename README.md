# iris on the ESP32-S3 — starter

Tilt the board. It learns what you meant. Then it plays.

Eleven sketches, the iris library (version 0.2.0, from
https://github.com/kylebsmith/iris) copied into each sketch that uses it, and
a walkthrough. It exists so you can get a trained model running on real
hardware in about twenty minutes and see the thing work before anyone asks you
to understand it. The board is LCDWIKI's ES3C28P: an ESP32-S3 (Espressif's
microcontroller) with a touch screen.

**Start at [GET-STARTED.md](GET-STARTED.md).** Parts are in
[PARTS.md](PARTS.md); when something goes wrong, see
[TROUBLESHOOTING.md](TROUBLESHOOTING.md).

In the order GET-STARTED.md runs them:

```
i2c_find/                1. is the sensor wired right? lists every I2C address
iris_tilt/               2. the first instrument: 3 poses, one number; learn it
iris_scope/              3. see the curve the network invents
  processing/               the plot it talks to, over the USB cable
iris_instrument/         4. screen, touch and USB MIDI sound
boilerplate/
  stemma_bno055/         5. sensor and knobs, and it survives a power cycle
  any_sensor/            any sensor, any board: start here on another board
  bno055_portable/       a real motion sensor on boards other than the ES3C28P
display_check/           check: does the screen draw and the touch report?
determinism_check/       check: the same instrument as a laptop, bit for bit?
device_torture/          check: nine questions of the library on this board
board_probe/             check: prediction and training time, measured
PARTS.md                 every part, its number, where to buy it, what goes where
GET-STARTED.md           install, board settings, first upload, the walk
TROUBLESHOOTING.md       symptom, likely cause, what to do
TASKS.md                 what to pick up
```

I2C (inter-integrated circuit) is the two-wire bus the sensor talks on; USB is
Universal Serial Bus; MIDI (Musical Instrument Digital Interface) is the
message format synthesisers understand.

Compiler output for each, on the settings in GET-STARTED.md (esp32 board
package 3.3.3):

| sketch | flash (program storage) | RAM (working memory) |
|---|---|---|
| `iris_tilt` | 404,147 B (12%) | 46,368 B (14%) |
| `display_check` | 409,371 B (13%) | 45,520 B (13%) |
| `iris_instrument` | 423,487 B (13%) | 47,264 B (14%) |

Almost all of that is the Arduino and USB runtime. iris itself is a few
kilobytes: `iris_tilt`'s whole instrument — weights, demonstrations and all —
is its 944-byte `memory` array, `IRIS_ARENA(2, 12, 1, 8)`. That figure is a
compile-time assertion with the ESP32-S3's own compiler
(xtensa-esp32s3-elf-gcc 14.2.0); the same macro gives 1,024 bytes on a 64-bit
laptop (Apple clang and gcc 15 on 64-bit ARM), because pointers there are
twice as wide.

---

## What the first sketch does

Read it top to bottom. It's short on purpose and I want you to follow every
line.

**It reads the gravity vector off the sensor** over I2C, the two-wire bus
that carries data and clock on two pins, using the Adafruit BNO055 library. Gravity, not orientation in degrees — Euler angles wrap from
+180 to −180, and two poses a degree apart then arrive as opposite ends of the
range. Nothing smooth can fit that. Gravity points down and never wraps.

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

**Doesn't prove:** that the mapping is any good musically. That isn't a software
question and I can't answer it for you. You have to play it.

## Two libraries, two different risks

The sketches use Adafruit's BNO055, GFX and ILI9341 libraries, and that is
fine. They're drivers. If one breaks you swap it for another and your instrument
plays exactly as it did before.

`iris.h` is the part that can't be treated that way. It decides how your gesture
becomes sound, so if *it* moves, your instrument silently becomes a different
instrument — same demonstrations, different result. That is why it has no
dependencies, why its behaviour is pinned by hashes in its own test suite, and
why it's a single file sitting next to your sketch rather than something you
install. Each sketch checks at compile time that its copy is iris 0.2 and
names the file to copy when it is not.

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
- **More inputs.** The BNO055 gives you heading and raw acceleration too, and
  there are free pins for another sensor entirely.
- **A different sensor.** Distance, light, flex, pressure — anything that gives
  you a number.
- **The screen.** Showing the learned space rather than a number changes how it
  feels to train.
- **More demonstrations, and deleting bad ones.** `iris_delete_id` deletes a
  take. `iris_worst_example_id` names the take that fought the others hardest,
  once there are at least 12 (`IRIS_STRESS_MIN_EX`); below that it returns -1.
  `iris_loo_error` answers a different question: one leave-one-out error for
  the whole instrument, how well it predicts takes it did not see.

## What this repo is holding itself to

These were agreed before any of it was built, and they are the standard to
judge a change against:

- One clone. No submodules, no package manager, no build script to read first.
- Opens in the Arduino IDE.
- A wrong board setting produces a **compiler error with a human message**,
  never a bricked board. Every sketch stops at compile time when USB CDC On
  Boot is off on the ESP32-S3; the sketches written for this board stop when
  the board entry is not the ESP32-S3; `iris_instrument` stops when USB Mode is
  not USB-OTG (TinyUSB). Each message names the menu item and says why.
- First sound in under fifteen minutes, from zero prior experience.
- Everything you add is a module behind the existing interface. The core is
  frozen and is not yours to edit.

## A note on how I want you to work

Use whatever tools you want, including AI ones. I'd rather you were fluent with
them than pretend otherwise. But the thing that makes that work isn't the
prompting, it's the verifying.

Mistakes in this repo, whoever wrote them, get caught the same way: by running
the thing and looking.

So when something you generate compiles, that's the beginning of knowing whether
it's right, not the end. Run it. Tilt the board. Does it do what you said it
would? Same standard whether a person or a model wrote the line.

## Licence

BSD 3-Clause — see [LICENSE](LICENSE). The copies of `iris.h` in each sketch
folder are the same file under the same terms, kept in step by `sync-iris.sh`
(`sh sync-iris.sh` checks them against the library, `--fix` re-copies).
Use, change and redistribute the sketches freely; that is what they are for.
