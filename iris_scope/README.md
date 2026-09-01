# iris_scope — see the mapping

**Run this before anything that makes sound.**

Sound is a bad first demo. When a mapping sounds wrong you cannot tell whether
the network is wrong, your synth is wrong, or your ears are. A picture has no
such ambiguity: you can see the exact numbers the network invented, at every
input value, including the ones you never taught it.

## What you need

A board and a BNO055 on I²C. Nothing else — no knobs, no buttons, no screen.
The sketch searches for the sensor across the pin pairs the common ESP32-S3
boards use, so you do not have to know which pins yours is on.

No sensor at all? Set `USE_ANALOG 1` at the top and turn a potentiometer on A0.

## Run it

1. Flash `iris_scope.ino`. **Close the Serial Monitor** — Processing cannot open
   the port while the Arduino IDE is holding it.
2. Open `processing/iris_scope/iris_scope.pde` in
   [Processing](https://processing.org) and press Run.
3. Click where you want this pose to mean. Press **SPACE**.
4. Tilt the board somewhere clearly different. Click somewhere else. **SPACE**.

Two poses is enough. You now have a curve.

## The two views — both on screen at once

**TRANSFER.** Horizontal is your sensor, vertical is what the instrument plays.
The big green dots are the two things you said. The line through them is what
the network made up.

That line is the whole idea. You supplied two points. The line has ninety-six.
The other ninety-four were invented, and playing the instrument means moving
through them.

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
3. **Press `d`** to delete that pose. Watch the curve relax. This is the repair
   loop: a bad take is deleted, not started over.
4. **Put two different answers at the same pose.** The curve goes flat and the
   board tells you why. The network is not broken — you asked for two things at
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
D index x y0 y1      a demonstration: input x taught to mean (y0, y1)
C n x a b x a b ...  the curve: n points of (input, out0, out1)
L x y0 y1            live: where you are right now
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

Change `N_IN` and `N_OUT` at the top when the picture stops surprising you.

Adding outputs that **move together** is free — measured: eight coordinated
outputs are slightly *more* accurate than one, because every demonstration
teaches all eight at once. Adding outputs you want to move **independently**
costs you about one demonstration's worth of evidence each. The numbers, and
the program that produces them, are in the library's
`docs/DEGREES-OF-FREEDOM.md`.
