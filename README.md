# iris on the ESP32-S3 — starter

Tilt the board. It learns what you meant. Then it plays.

Ten sketches, one header, and a walkthrough. It exists so you can get a
trained model running on real hardware in about twenty minutes and see the thing
work before anyone asks you to understand it.

**Start at [GET-STARTED.md](GET-STARTED.md).**

```
iris_scope/
  iris_scope.ino         run this first — watch the curve the network invents
  processing/            the plot it talks to, over the USB cable
boilerplate/
  any_sensor/            any sensor, any board — start here
  bno055_portable/       a real motion sensor, on almost any board
  stemma_bno055/         the same, ESP32 only, and it remembers after a reboot
iris_tilt/
  iris_tilt.ino          225 lines — learn it, print it
  iris.h                 the library
iris_instrument/
  iris_instrument.ino    345 lines — screen, touch, real USB-MIDI
  iris.h                 the library (same file)
display_check/
  display_check.ino      98 lines — hardware triage, run this if the screen is dead
i2c_find/                finds which pins your sensor is wired to, and its address
determinism_check/       proves the same demonstrations give the same instrument
device_torture/          long-run stability, on the board rather than on a laptop
GET-STARTED.md           install, board settings, first flash, and the traps
TASKS.md                 what to pick up
```

Compiler output for each, on the settings in GET-STARTED.md:

| sketch | flash | RAM |
|---|---|---|
| `iris_tilt` | 403,867 B (12%) | 46,368 B (14%) |
| `display_check` | 409,319 B (13%) | 45,520 B (13%) |
| `iris_instrument` | 423,275 B (13%) | 47,264 B (14%) |

Almost all of that is the Arduino and USB runtime. iris itself is a few
kilobytes: `iris_tilt`'s whole instrument — weights, demonstrations and all —
is the 944-byte `memory` array on line 67. (Measured on the ESP32-S3's own
compiler as a compile-time assertion, and confirmed by a second 32-bit target;
the same macro gives 1,032 on a 64-bit laptop, because the struct pads
differently. Both figures are 8 bytes larger than this paragraph said before
0.1.0 -- `struct iris` grew by that much and the prose did not follow.)

---

## What the first sketch does

Read it top to bottom. It's short on purpose and I want you to follow every
line.

**It reads the gravity vector off the sensor** over I2C — the two-wire bus
(inter-integrated circuit) that carries data and clock on two pins, which is
what the four-wire STEMMA QT cable is — using the Adafruit
BNO055 library. Gravity, not orientation in degrees — Euler angles wrap from
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
with a human doing the demonstrating. That had never been tested until this
year. Every accuracy number in the library was measured on clean synthetic data
at a desk, and real gestures are noisier and less consistent than anything it
was tuned against.

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
install.

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
- **More demonstrations, and deleting bad ones.** `iris_delete_id` exists, and
  `iris_loo_error` will tell you which of your takes is fighting the others.

## What this repo is holding itself to

These were agreed before any of it was built, and they are the standard to
judge a change against:

- One clone. No submodules, no package manager, no build script to read first.
- Opens in the Arduino IDE.
- A wrong board setting produces a **compiler error with a human message**,
  never a bricked board. *(Met, as of this pass: seven of the ten sketches check
  `ARDUINO_USB_MODE` and `ARDUINO_USB_CDC_ON_BOOT` at compile time and stop
  with an error naming the exact menu item. Before that, the wrong USB Mode
  built cleanly and silently removed MIDI.)*
- First sound in under fifteen minutes, from zero prior experience.
- Everything you add is a module behind the existing interface. The core is
  frozen and is not yours to edit.

## A note on how I want you to work

Use whatever tools you want, including AI ones. I'd rather you were fluent with
them than pretend otherwise. But the thing that makes that work isn't the
prompting, it's the verifying.

Everything in this repo has been wrong at some point this month — a board
setting that silently disabled MIDI, a documented first step that crashed the
sketch, a comment stating a measurement that was 28× off. Some of that I wrote,
some a model wrote, and the ones that got caught were caught the same way: by
running it and looking.

So when something you generate compiles, that's the beginning of knowing whether
it's right, not the end. Run it. Tilt the board. Does it do what you said it
would? Same standard whether a person or a model wrote the line.

## Licence

BSD 3-Clause — see [LICENSE](LICENSE). The copies of `iris.h` vendored into each
sketch folder are the same file under the same terms, kept in step by `sync-iris.sh`.
Use, change and redistribute the sketches freely; that is what they are for.
