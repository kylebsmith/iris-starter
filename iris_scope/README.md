# iris_scope — see the mapping

**Run this after `iris_tilt` and before anything that makes sound** (step 8
of GET-STARTED.md).

Sound is a bad first demo. When a mapping sounds wrong you cannot tell whether
the network is wrong, your synth is wrong, or your ears are. A picture has no
such ambiguity: you can see the exact numbers the network invented, at every
input value, including the ones you never taught it.

## What you need

A board and a BNO055 sensor on I²C — the two-wire bus (inter-integrated
circuit) that carries data and clock on two pins; PARTS.md shows how to
connect it. Nothing else — no knobs, no buttons, no screen.
The sketch searches for the sensor across the pin pairs the common ESP32-S3
boards use, so you do not have to know which pins yours is on.

No sensor at all? Set `USE_ANALOG 1` at the top and turn a potentiometer on
`ANALOG_PIN`: GPIO 2 (GPIO: general-purpose input/output, a numbered pin of
the chip) on the ES3C28P's expansion socket (wiring in PARTS.md; check it on
the board), A0 on other boards.

## Run it

1. Flash `iris_scope.ino`. **Close the Serial Monitor** — Processing cannot open
   the port while the Arduino IDE (integrated development environment) is
   holding it.
2. Open `processing/iris_scope/iris_scope.pde` in
   [Processing](https://processing.org) and press Run.
3. Click where you want this pose to mean. Press **SPACE**.
4. Tilt the board somewhere clearly different. Click somewhere else. **SPACE**.

Two poses is enough. You now have a curve.

## The two views — both on screen at once

**TRANSFER.** Horizontal is your sensor, vertical is what the instrument plays.
The big green dots are what you said: one pair for each pose you taught, one
dot per output. The two lines are what the network made up.

Those lines are the whole idea. You supplied two poses. Each line is
ninety-six points, every one of them the network's answer at an input you
did not necessarily teach, and playing the instrument means moving through
them.

**SCOPE.** The two outputs plotted against each other, the way an oscilloscope
in X-Y mode plots two voltages against each other instead of against time. One
input sweeping its range draws a shape.

Teach it three poses in a straight line and the shape still bends. The bend is
the network's nonlinearity — the thing that makes this an instrument rather
than a fader.

## What to try, in order

1. **Two poses, far apart.** Look at TRANSFER. Find a value on the curve you
   never demonstrated. That is the invention.
2. **Add a third pose in the middle**, pulling it somewhere unexpected. Watch
   the curve bend to reach it. You did not tell it how to bend.
3. **Press `d`** to delete that pose. Its dot leaves the picture at once and
   the curve relaxes as the network retrains. This is the repair loop: a bad
   take is deleted, not started over.
4. **Press `c`, then teach two different answers at the same pose.** The curve
   goes flat and the board tells you why. The network is not broken — you asked for two things at
   once and it gave you the average, which is the only honest answer.
5. **Look at SCOPE** while you move slowly. You are tracing the mapping.
   Both panels are drawn at once, so there is no view to switch to.

## The keys

| Key | What it does |
|---|---|
| SPACE | teach the board this pose means the point you last clicked |
| `d` | delete the last demonstration |
| `c` | clear all of them and start over |
| LEFT / RIGHT | step through the serial ports |
| `s` | save a screenshot next to the sketch |

**If the plot stays empty, press LEFT or RIGHT.** Opening the wrong serial port
is the most likely first failure of this sketch, and it looks exactly like a
dead board.

## Reading it without Processing

Everything the board sends is plain text, one item per line, so the Serial
Monitor is a valid second view:

```
X lo hi unit         the sensor's full physical scale, and its unit
R lo hi              the input range you have actually visited
N n                  how many demonstrations the board holds; the n D lines
                     that follow are all of them
D index x y0 y1      a demonstration: input x taught to mean (y0, y1)
C n x a b x a b ...  the curve: n points of (input, out0, out1); C 0 is no curve
L x [y0 y1]          live: where you are now, and what it plays once there are two
A y0 y1              the board echoing back the target you just clicked
M text               a message for you
```

`C` is the interesting one. It is produced by sweeping a pretend input across
the range and asking `iris_predict` what it would say at each step — about
1.4 ms of work for all ninety-six. Nothing is cached and nothing is smoothed;
that line is the mapping itself.

## Why one input and two outputs

One input is the smallest thing that still has an inside: its entire behaviour
fits on a screen as a curve. You lose that the moment there are three inputs.
Two outputs is the smallest number that makes SCOPE worth looking at — one
output is a line, two is a shape.

`N_IN` and `N_OUT` at the top are fixed at 1 and 2 for this sketch: the
reading, the targets and the plot are all written for one input and two
outputs, and changing only the numbers reads past the end of the arrays.
To go further, start from `boilerplate/any_sensor`, which takes any shape.

Adding outputs that **move together** is free — measured: eight coordinated
outputs are slightly *more* accurate than one, because every demonstration
teaches all eight at once. Outputs you want to move **independently** are
expensive: eight of them need roughly eight times the demonstrations, because
each one asks a separate question. The numbers, and the program that
produces them, are in the library's `docs/DEGREES-OF-FREEDOM.md`.
