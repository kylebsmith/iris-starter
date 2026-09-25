# iris_scope — see the mapping

**Run this after `iris_tilt` and before anything that makes sound** (step 8
of GET-STARTED.md).

Sound is a hard first demo. When a mapping sounds wrong you cannot tell
whether the network is wrong, your synth is wrong, or your ears are. A picture
has no such ambiguity: you can see the exact numbers the network invented, at
every input value, including the ones you never taught it.

## What you need

The board and a BNO055 sensor on I2C (inter-integrated circuit, the two-wire
bus that carries data and clock on two pins); PARTS.md shows how to connect
it. No knobs, no buttons, no screen, and
[Processing](https://processing.org) on the computer. The sketch searches for
the sensor across the pin pairs the common ESP32-S3 boards use, so you do not
have to know which pins yours is on.

No sensor at all? Set `USE_ANALOG 1` at the top and turn a potentiometer on
`ANALOG_PIN`: GPIO 2 (GPIO: general-purpose input/output, a numbered pin of
the chip) on the ES3C28P's expansion socket (wiring in PARTS.md; check it on
the board), A0 on other boards.

## The demo

1. Upload `iris_scope.ino`, then **close the Serial Monitor**: Processing
   cannot open the port while the Arduino IDE (integrated development
   environment) is holding it.
2. Open `processing/iris_scope/iris_scope.pde` in Processing and press Run.
   Move the board: a red line marked "you are here" moves across the big
   plot. If it does not, press the **left or right arrow key** to step to the
   next serial port; the top right of the window names the port it is
   listening on.
3. **Set the handles.** The two round handles to the right of the big plot
   are the two outputs, blue for output one and orange for output two. Drag
   each to the height you want this pose to mean (clicking inside the plot
   moves the nearer handle to that height). SPACE WILL TEACH, on the left,
   shows the pair.
4. Hold a pose and press **SPACE**. Two green dots appear, one per output,
   and the left panel shows `board has` with the pair the board received.
5. Tilt the board somewhere clearly different, set the handles to something
   else, and press **SPACE** again. The board starts training, and the two
   curves appear and settle while you watch.
6. **Tilt.** The red line follows the board, and the red dots on the curves
   are what it plays. It reads one axis of the sensor (the x component of
   gravity), so tilting the other way changes nothing.
7. **A bad pose bends it.** Drag the handles somewhere unexpected, hold a pose
   between the two you taught, and press SPACE. The curves bend to reach the
   new dots.
8. **`d` repairs it.** Press `d`: the last pose's dots leave the picture at
   once and the board retrains on the poses that remain. A bad take is
   deleted, not started over.
9. **`c` clears** every pose, and you start again from step 3.

Poses too close together give a flat curve, and the message at the top right
of the window says so: press `c` and move the board further between poses.

## The two views

**The big plot.** Horizontal is your sensor, over the whole range it can read
(−9.81 to +9.81 metres per second squared of gravity on one axis); vertical is
what the instrument plays, 0 to 1. The shaded band is the part of that range
you have actually visited. The green dots are what you taught: one pair for
each pose, one dot per output. The two lines are what the network made up,
each 96 points of the network's answer at inputs you did not necessarily
teach. Playing the instrument means moving along them.

**The square**, top right, plots the two outputs against each other, the way
an oscilloscope in X-Y mode plots two voltages against each other instead of
against time. One input sweeping its range draws a shape. Teach three poses
whose outputs lie on a straight line and the shape can still bend: the bend
is the network's nonlinearity, the thing that makes this an instrument rather
than a fader.

## The keys

| Key | What it does |
|---|---|
| SPACE | teach the board that this pose means the values on the handles |
| `d` | delete the last demonstration and retrain |
| `c` | clear all of them and start over |
| left and right arrows | step through the serial ports |
| `s` | save a picture of the window, `iris_scope-` and a frame number, in the Processing sketch's folder |

## Reading it without Processing

Everything the board sends is plain text, one item per line, so the Serial
Monitor (at 115200 baud, bits per second) is a second view:

```
X lo hi unit         the sensor's full physical scale, and its unit
R lo hi              the input range you have actually visited
N n                  how many demonstrations the board holds; the n D lines
                     that follow are all of them
D index x y0 y1      a demonstration: input x taught to mean (y0, y1)
C n x a b x a b ...  the curve: n points of (input, out0, out1); C 0 is no curve
L x [y0 y1]          live: where you are now, and what it plays once there are two
A y0 y1              the board echoing back the target it received
M text               a message for you
```

It also listens: `T 0.2 0.8` sets the target, a space records the pose, `d`
deletes the last demonstration and `c` clears.

`C` is produced by sweeping a pretend input across the visited range and
asking `iris_predict` what it would say at each step. Nothing is cached and
nothing is smoothed; that line is the mapping itself.

## Why one input and two outputs

One input is the smallest thing that still has an inside: its entire behaviour
fits on a screen as a curve. You lose that the moment there are three inputs.
Two outputs is the smallest number that makes the square worth looking at: one
output is a line, two is a shape.

`N_IN` and `N_OUT` at the top are fixed at 1 and 2 for this sketch: the
reading, the targets and the plot are all written for one input and two
outputs, and changing only the numbers reads past the end of the arrays.
To go further, start from `boilerplate/any_sensor`, which takes any shape.

Adding outputs that **move together** costs little: eight coordinated outputs
come out slightly more accurate than one, because every demonstration teaches
all eight at once. Outputs you want to move **independently** are expensive:
eight of them need roughly eight times the demonstrations, because each one
asks a separate question. The numbers, and the program that produces them,
are in iris-studies S05 (https://github.com/kylebsmith/iris-studies/tree/v1).
