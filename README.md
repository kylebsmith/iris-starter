# iris on the ESP32-S3 — starter

Show it a few examples of "when the sensor is like *this*, make *that* sound."
It learns the mapping. Then it plays, including everywhere between the examples
you gave it.

## The whole library is six lines

```c
#include "iris.h"                                           // 1. the library
static unsigned char memory[IRIS_ARENA(2, 12, 3, 16)];       // 2. its memory
iris *k = iris_init(memory, sizeof memory, 2, 12, 3, 16, 1234);   // 3. set up

iris_record(k, sensor, sound);    // 4. "this sensor reading means this sound"
iris_train(k);                    // 5. learn it, once you have two
iris_predict(k, sensor, sound);   // 6. play: sensor in, sound out
```

That is the shape of it, not a program you can paste —
[`add_iris/add_iris.ino`](add_iris/add_iris.ino) below is the working version.
`2, 12, 3, 16` is: 2 numbers in from your sensor, 12 hidden units (leave it),
3 numbers out to your sound, room for 16 examples. There is no installing, no
scaling your readings, and no allocation — `iris.h` sits next to your sketch
and uses the memory you gave it.

Every sketch here carries its own copy of **iris 0.3.0**, from
https://github.com/kylebsmith/iris, and refuses to build against a different
0.x minor version with a message saying which file to replace. The board the
walk is written for is LCDWIKI's ES3C28P, an ESP32-S3 (Espressif's
microcontroller) with a touch screen, but `add_iris` and
`boilerplate/any_sensor` build for an Uno, a Pico or a Teensy too.

## Start here

**Open [`add_iris/add_iris.ino`](add_iris/add_iris.ino) and flash it.** It is
those six lines in the smallest working sketch, and they are marked `[1]` to
`[6]` so you can find them.

It plays straight away, with **nothing wired to the board**. In Serial Monitor
at 115200: press `1` `2` `3` to pick an output and `+` `-` to move it — that is
you playing it by hand. Then:

1. Hold your sensor in a pose. Set the sound you want for that pose.
2. Press the **BOOT** button. That is one example.
3. Do it again somewhere else.

After the second one, iris takes the controls: move the sensor and it plays
what you showed it, and everything in between.

Three functions to replace with your own, and nothing else:

| function | what you put in it |
|---|---|
| `read_sensor` | your sensor's readings, raw — do not scale them |
| `read_target` | how you set the sound ([TARGETS.md](TARGETS.md)) |
| `send_sound`  | your synthesis, your MIDI, your oscillator |

Then move the six lines into the `setup` and `loop` you already have.

## Setting the sound

`iris_record` wants numbers. You have a sound you like and no idea what number
it is — that gap is the only real difficulty in starting, and it is one
function. [TARGETS.md](TARGETS.md) has five ways to fill it: the Serial Monitor
one that ships (no hardware), potentiometers, a touchscreen, key switches, and
an incoming MIDI control change.

## If you want it explained line by line

[`boilerplate/any_sensor`](boilerplate/any_sensor) is the same instrument with
every guard, every start-up check and every explanation written out, and it
fits in slices so the sound never stops while it learns. Longer on purpose.
Read it when you want to know *why*, not to get started.

## Installing, and this board

[GET-STARTED.md](GET-STARTED.md) — the Arduino IDE, the board package, the
board settings, and a walk through the demo sketches on the ES3C28P (an
ESP32-S3 with a touch screen). [PARTS.md](PARTS.md) is every part and what
connects to what. [TROUBLESHOOTING.md](TROUBLESHOOTING.md) is symptom, cause,
fix. [SOUND.md](SOUND.md) gets the numbers out to a synthesiser, Max or Pure
Data.

## Everything else here

```
add_iris/                START HERE: the six lines, in the smallest sketch
TARGETS.md               five ways to set the sound. One function each
boilerplate/
  any_sensor/            the same thing, fully explained. Any sensor, any board
  stemma_bno055/         a real sensor, and it survives a power cycle
  bno055_portable/       that sensor on boards other than the ES3C28P
```

Demonstrations of the library on this board, run them to see what it does —
`iris_tilt` (tilt, teach three poses, play), `iris_scope` (watch the mapping
get drawn), `iris_instrument` (screen, touch and USB MIDI).

Checks, for when something seems wrong — `i2c_find` (is the sensor wired?),
`display_check`, `determinism_check`, `device_torture`, `board_probe`.
`board-logs/` is what they printed on a real board. `TASKS.md` is what to pick
up. [ABOUT.md](ABOUT.md) is why the kit is shaped this way, what the sketches
prove and what they do not, and how I would like you to work.

## Licence

BSD 3-Clause (BSD: Berkeley Software Distribution, the licence's origin) —
see [LICENSE](LICENSE). The copies of `iris.h` in the sketch
folders are the same file under the same terms, kept in step by `sync-iris.sh`
(`sh sync-iris.sh` checks them against the library, `--fix` re-copies).
Use, change and redistribute the sketches freely; that is what they are for.
