/* iris_scope — SEE THE MAPPING
   ===========================
   This is the first thing to run. Not because it makes sound, but because it
   makes the network VISIBLE.

   You tilt the board and press SPACE, at two or more different poses. Each
   press says "when the board is like THIS, the numbers should be THAT." Then the network fills in
   everything in between — and the whole point of this sketch is that you can
   SEE what it filled in, as a curve, instead of taking it on faith.

   Run the Processing sketch in processing/iris_scope/ and you get:

     TRANSFER VIEW   your demonstrations as dots, and the curve the network
                     drew through them. The dots are yours. The curve is the
                     network's. Everything between the dots is invented, and
                     that invention IS the instrument.

     SCOPE VIEW      the two outputs plotted against each other, the way an
                     oscilloscope in X-Y mode plots two voltages. One input
                     sweeping through its range traces a shape. That shape is
                     the mapping, drawn as geometry.

   You can also just read the serial output. It is plain text on purpose.

   WHAT IT SENDS (one thing per line, so you can read it yourself)
     X lo hi unit         the sensor's full scale, sent once at power-on
     R lo hi              the input range it has seen so far
     N n                  how many demonstrations the board holds now; the
                          n D lines that follow are all of them
     D index x y0 y1      demonstration: input x taught to mean (y0, y1)
     C n x0 a0 b0 x1 ...  the curve: n points of (input, out0, out1);
                          C 0 means there is no curve (fewer than two)
     L x [y0 y1]          live: where you are now, and what it plays once
                          there are two demonstrations
     A y0 y1              the board received the target you clicked
     M text               a message for the human

   HARDWARE: a board and a BNO055 on I2C. Nothing else -- no knobs, no
   buttons, no screen. If you have no sensor at all, set USE_ANALOG to 1 and
   it reads a potentiometer on A0 instead. Everything below that is identical.

   WHAT IT LISTENS FOR
     SPACE     teach it: this pose means the point you last clicked
     T y0 y1   set that point (Processing sends this when you click)
     d         delete the last demonstration and retrain
     c         clear everything
   ========================================================================= */

#define USE_ANALOG 0        /* 1 = potentiometer on A0, no sensor needed */

#include <Wire.h>
#if !USE_ANALOG
#include <Adafruit_BNO055.h>
#endif
#include "iris.h"
#if IRIS_VERSION_MAJOR != 0 || IRIS_VERSION_MINOR != 2
#error "This sketch is written for iris 0.2. Copy iris.h from iris 0.2 (https://github.com/kylebsmith/iris) into this sketch's folder, next to the .ino file, replacing the copy there."
#endif

/* SERIAL MONITOR SETTING. On an ESP32-S3 whose USB socket is the chip's own
   USB port, as on the ES3C28P, Serial reaches the computer only with USB
   CDC On Boot enabled. ARDUINO_USB_MODE exists only on chips with that port,
   so every other board builds untouched. Either USB Mode works: this sketch
   uses no feature of the TinyUSB mode. */
#if defined(ARDUINO_ARCH_ESP32) && defined(ARDUINO_USB_MODE) && !ARDUINO_USB_CDC_ON_BOOT
#error "Set Tools -> USB CDC On Boot -> Enabled. The board's USB socket is the chip's own USB port; with this setting off, Serial prints to pins 43 and 44 instead and Serial Monitor stays empty."
#endif

/* ---- the shape of the instrument ---------------------------------------
   ONE input and TWO outputs, deliberately. One input is the smallest thing
   that still has an inside — you can draw its whole behaviour as a curve on
   a screen, which you cannot do once there are three or four. Two outputs is
   the smallest number that makes the scope view interesting: one output is a
   line, two is a shape. Change these when the picture stops surprising you. */
#define N_IN    1
#define N_OUT   2
#define N_HID  12
#define N_DEMOS 12
#define CURVE_POINTS 96      /* how finely we sample the curve for drawing */

#if defined(__AVR__)         /* small board: shrink the working arrays. See iris.h. */
#define IRIS_MAX_IN  4
#define IRIS_MAX_OUT 4
#define IRIS_MAX_HID 12
#endif

static unsigned char memory[IRIS_ARENA(N_IN, N_HID, N_OUT, N_DEMOS)];
static iris *k;

#if !USE_ANALOG
static Adafruit_BNO055 bno = Adafruit_BNO055(55, 0x28, &Wire);
#endif

/* Every demonstration we have taken, kept so the plot can draw the dots.
   The library also holds them -- iris_get(k, i, ...) would return them -- but
   keeping our own copy means the drawing code stays obvious. */
static float demo_x[N_DEMOS], demo_y0[N_DEMOS], demo_y1[N_DEMOS];
static int   demos = 0;

static float seen_lo =  1e30f, seen_hi = -1e30f;

/* ---- FILL THIS IN 1: WHERE THE INPUT COMES FROM -------------------------
   One number. That is all this sketch wants. Tilt, a knob, light, distance,
   how hard you are squeezing something -- anything that moves. */
static float read_input(void) {
#if USE_ANALOG
  return (float)analogRead(A0);
#else
  imu::Vector<3> g = bno.getVector(Adafruit_BNO055::VECTOR_GRAVITY);
  return (float)g.x();                 /* one axis of tilt, roughly -9.8..+9.8 */
#endif
}

/* ---- WHAT YOU ARE TEACHING IT TO SAY ------------------------------------
   The target comes from the Processing window: you click where you want this
   pose to land, and it sends "T y0 y1". No knobs, no extra wiring.

   An earlier version read two potentiometers on A1 and A2. With nothing wired
   those pins float, so every demonstration taught a random pair of numbers and
   the picture was noise -- a demo whose first impression depends on hardware
   the student was not told to buy is not a first demo. */
static float target0 = 0.5f, target1 = 0.5f;

/* Read one line like "T 0.31 0.88" that has already had its 'T' consumed. */
static void read_target_from_serial(void) {
  target0 = Serial.parseFloat();
  target1 = Serial.parseFloat();
  if (target0 < 0.0f) target0 = 0.0f;  if (target0 > 1.0f) target0 = 1.0f;
  if (target1 < 0.0f) target1 = 0.0f;  if (target1 > 1.0f) target1 = 1.0f;
  /* Echo it. The plot draws the click immediately from its own numbers -- it
     must, a serial round trip is far past the tenth of a second that makes an
     action feel instantaneous -- and then confirms against this that the BOARD
     has the same value. Two different facts, both worth showing. */
  Serial.print(F("A ")); Serial.print(target0, 4);
  Serial.print(' ');     Serial.println(target1, 4);
}

static void say(const char *m) { Serial.print(F("M ")); Serial.println(m); }

static void send_range(void) {
  float lo = seen_lo, hi = seen_hi;
  if (!(hi > lo) || (hi - lo) < 0.05f) {       /* same widening as send_curve,
                                                  so the axes match the curve */
    float mid = (hi > lo) ? (lo + hi) * 0.5f : lo;
    lo = mid - 0.5f; hi = mid + 0.5f;
  }
  Serial.print(F("R ")); Serial.print(lo, 4);
  Serial.print(' ');     Serial.println(hi, 4);
}

/* N first, so the plot knows exactly how many dots to draw: a deleted
   demonstration disappears from the picture as soon as it leaves the board. */
static void send_demos(void) {
  Serial.print(F("N ")); Serial.println(demos);
  for (int i = 0; i < demos; ++i) {
    Serial.print(F("D ")); Serial.print(i);
    Serial.print(' ');     Serial.print(demo_x[i], 4);
    Serial.print(' ');     Serial.print(demo_y0[i], 4);
    Serial.print(' ');     Serial.println(demo_y1[i], 4);
  }
}

/* THE INTERESTING ONE. We sweep a pretend input across the whole range the
   sensor has visited and ask the network what it would say at every step --
   including at values you never demonstrated. That sweep IS the mapping. It
   costs CURVE_POINTS predictions, about 1.4 ms on an ESP32-S3. */
static void send_curve(void) {
  if (demos < 2) return;

  /* If the sensor has barely moved there is no range to sweep, and an earlier
     version simply sent nothing -- leaving a blank window and no reason for
     it. Widen a degenerate range so there is always something to draw, and say
     plainly that the poses were too close together, which is the real problem
     and the same one iris_tilt warns about. */
  float lo = seen_lo, hi = seen_hi;
  if (!(hi > lo) || (hi - lo) < 0.05f) {
    float mid = (hi > lo) ? (lo + hi) * 0.5f : lo;
    lo = mid - 0.5f; hi = mid + 0.5f;
    say("your poses are nearly identical - move the sensor further between them");
  }

  Serial.print(F("C ")); Serial.print(CURVE_POINTS);
  for (int i = 0; i < CURVE_POINTS; ++i) {
    float x = lo + (hi - lo) * ((float)i / (float)(CURVE_POINTS - 1));
    float out[N_OUT];
    iris_predict(k, &x, out);
    Serial.print(' '); Serial.print(x, 4);
    Serial.print(' '); Serial.print(out[0], 4);
    Serial.print(' '); Serial.print(out[1], 4);
  }
  Serial.println();
}

/* Are the demonstrations actually far apart? This asks about the POSES, not
   about how far the sensor has wandered while sitting still. Those are
   different numbers and confusing them makes the warning useless: a board
   drifting 0.34 through noise looks like a healthy range while every pose was
   taken at the same spot. Teaching two different answers at one input is not
   a mistake the network can resolve -- it averages them, correctly, and the
   curve comes out flat. Better to say so than to let someone conclude the
   library does not work. */
static void check_spread(void) {
  if (demos < 2) return;
  float lo = demo_x[0], hi = demo_x[0];
  for (int i = 1; i < demos; ++i) {
    if (demo_x[i] < lo) lo = demo_x[i];
    if (demo_x[i] > hi) hi = demo_x[i];
  }
  if (hi - lo < 0.5f) {
    say("your poses are almost in the same place, so the curve will be flat");
    say("- move the sensor a LOT between demonstrations, then press 'c' and retry");
  }
}

/* TRAINING IN SLICES, so the plot keeps updating while it learns.
   iris_train() would block for up to three seconds on this board and the live
   dot would freeze -- in a sketch whose whole purpose is watching the mapping
   form, that is the worst possible moment to stop drawing. iris_train_begin +
   iris_train_slice do the identical fit in pieces: verified bit-identical at
   4, 12 and 20 demonstrations, including with a prediction between every
   slice, which is exactly what happens below. */
static bool training = false;

static void retrain_and_redraw(void) {
  if (demos < 2) {
    /* NOT ENOUGH TO FIT -- BUT SAY SO, because the student just did something.
       This was a bare `return`. The first demonstration was read, stored and
       counted, and then announced to nobody: no D marker, so the plot could not
       draw the dot, and no message, so the serial monitor stayed silent. You
       pressed SPACE and the machine gave you nothing back, which is the exact
       thing this sketch exists to stop happening. Measured before the fix: one
       press produced 192 live-value lines and zero acknowledgements.
       The delete branch below already handled this same case correctly, three
       lines apart, which is how the asymmetry was found. */
    send_range();
    send_demos();          /* the D that puts the student's dot on the plot */
    say(demos == 1 ? "got it. Now move the sensor somewhere different and"
                     " press SPACE again -- two poses makes a curve."
                   : "no demonstrations yet. Move the sensor and press SPACE.");
    return;
  }
  if (!iris_train_begin(k, 0)) { say("TRAINING REFUSED - nothing to fit"); return; }
  training = true;
  say("learning - the curve will settle as it goes");
}

/* Called every pass. Does a slice if one is owed, then redraws so you can
   watch the curve move rather than waiting for a finished one. */
static void keep_training(void) {
  if (!training) return;
  iris_train_slice(k, 48);
  if (iris_train_busy(k)) {
    send_curve();                        /* the curve, mid-flight */
    return;
  }
  training = false;
  if (!iris_is_trained(k)) { say("TRAINING FAILED - the instrument is not fitted"); return; }
  send_range();
  send_demos();
  send_curve();
  check_spread();
  say("trained");
}

void setup(void) {
  Serial.begin(115200);
  while (!Serial && millis() < 20000) delay(10);
  delay(200);

#if !USE_ANALOG
  /* FIND THE SENSOR, DO NOT ASSUME IT. Every board puts I2C somewhere else,
     and a plain Wire.begin() picks that board's default -- which on the Adafruit
     Feather boards is NOT where the STEMMA QT connector is. Written the naive
     way first, this sketch reported "no sensor" on a board with a perfectly
     good sensor sitting on pins 16 and 15. So: try the default, then the pin
     pairs the common ESP32-S3 boards actually use, and try both addresses on
     each. Whatever answers first wins, and we say which so you can hardcode it
     later if you want to. */
  { static const int PINS[][2] = { {-1,-1}, {16,15}, {3,4}, {8,9}, {5,6},
                                   {1,2}, {41,40}, {17,18}, {21,22} };
    bool found = false;
    for (unsigned i = 0; i < sizeof PINS / sizeof PINS[0] && !found; ++i) {
      /* Only the ESP32 core lets you choose I2C pins or tells you whether the
         bus came up: on AVR and RP2040, Wire.begin() returns void and takes no
         arguments. Guarding this is what keeps this sketch -- the FIRST thing a
         student runs -- buildable on a Pico and an Uno. Written the ESP32 way
         first, it compiled only there, which is the wrong sketch to lose. */
#if defined(ARDUINO_ARCH_ESP32)
      Wire.end(); delay(10);
      if (PINS[i][0] < 0) { if (!Wire.begin()) continue; }
      else                { if (!Wire.begin(PINS[i][0], PINS[i][1])) continue; }
#else
      if (PINS[i][0] >= 0) continue;   /* elsewhere there is one bus, the default */
      Wire.begin();
#endif
      delay(20);
      for (int a = 0x28; a <= 0x29 && !found; ++a) {
        bno = Adafruit_BNO055(55, (uint8_t)a, &Wire);
        if (bno.begin()) {
          found = true;
          Serial.print(F("M found the BNO055 at 0x")); Serial.print(a, HEX);
          if (PINS[i][0] < 0) Serial.println(F(" on the default pins"));
          else { Serial.print(F(" on SDA ")); Serial.print(PINS[i][0]);
                 Serial.print(F(" / SCL ")); Serial.println(PINS[i][1]); }
        }
      }
    }
    if (!found) {
      say("No BNO055 anywhere. Run i2c_find to see what is on the bus,");
      say("or set USE_ANALOG to 1 at the top and use a knob on A0 instead.");
      for (;;) delay(1000);
    } }
#endif

  k = iris_init(memory, sizeof memory, N_IN, N_HID, N_OUT, N_DEMOS, 1234u);
  if (!k) { say("iris_init refused - check the shape at the top"); for (;;) delay(1000); }

  /* THE SENSOR'S PHYSICAL FULL SCALE, sent once. The plot draws its horizontal
     axis against THIS and never rescales, because a silently rescaling axis is
     the thing that made the picture unreadable -- and rescaling is known to
     degrade value judgements even when it is animated (Heer & Robertson 2007).
     The range actually visited is drawn as a band inside it instead, which
     turns an invisible transformation into a visible, meaningful object: it is
     the part of the range the network has any evidence about. */
#if USE_ANALOG
  Serial.println(F("X 0 1023 counts"));
#else
  Serial.println(F("X -9.81 9.81 m/s2"));   /* gravity, one axis */
#endif

  say("ready. Move the sensor, then press SPACE to teach it this pose.");
  say("Two poses is enough to see a curve. 'c' clears, 'd' deletes the last.");
}

void loop(void) {
  float x = read_input();
  /* Tell the plot when the visited range grows, rate-limited. It used to be
     sent only after training, so between demonstrations the plot's idea of the
     range was stale and it drew the live marker outside its own axes. */
  { static uint32_t last_range = 0;
    bool grew = false;
    if (x < seen_lo) { seen_lo = x; grew = true; }
    if (x > seen_hi) { seen_hi = x; grew = true; }
    if (grew && millis() - last_range > 250) { send_range(); last_range = millis(); } }

  if (Serial.available()) {
    int c = Serial.read();

    if (c == 'T') { read_target_from_serial(); }     /* where the click was */

    else if (c == ' ') {                             /* teach it this pose */
      float t[N_OUT];
      t[0] = target0; t[1] = target1;
      if (demos < N_DEMOS && iris_record(k, &x, t)) {
        demo_x[demos] = x; demo_y0[demos] = t[0]; demo_y1[demos] = t[1];
        demos++;
        retrain_and_redraw();
      } else {
        /* Say WHICH refusal it was. "Full" when the truth was a bad reading
           sends you to delete demonstrations you do not have. */
        if (iris_get_status(k) == IRIS_STORE_FULL || demos >= N_DEMOS)
          say("full - press 'd' to delete one first");
        else
          say("refused - that reading is not a number. Check the sensor.");
      }
    }
    else if (c == 'd') {                             /* the repair loop */
      if (demos > 0 && iris_delete_last(k)) {
        demos--;
        say("deleted the last demonstration");
        send_demos();                                /* the dot goes now */
        if (demos >= 2) retrain_and_redraw();
        else { send_range(); Serial.println(F("C 0")); say("need two to draw a curve"); }
      } else say("nothing to delete");
    }
    else if (c == 'c') {
      iris_clear(k); demos = 0;
      seen_lo = 1e30f; seen_hi = -1e30f;
      say("cleared");
    }
  }

  keep_training();                       /* a slice per pass, free when idle */

  /* The live dot, about 30 times a second. Only the dot -- the curve is only
     resent when the mapping actually changes, so the link stays quiet. */
  /* THE LIVE LINE GOES OUT FROM POWER-ON, TRAINED OR NOT.
     This used to sit inside `if (demos >= 2)`, so the board said nothing at all
     until the second demonstration existed. A student waving the sensor in
     their first thirty seconds saw a dead window, clicked and saw nothing,
     pressed SPACE and saw nothing -- six actions before any evidence the system
     was alive. By then they have learned that their input does not matter, and
     no amount of curve afterwards repairs that. Three tokens when untrained,
     five when trained; the plot reads both. */
  { float out[N_OUT];
    Serial.print(F("L ")); Serial.print(x, 4);
    if (demos >= 2) {
      iris_predict(k, &x, out);
      Serial.print(' '); Serial.print(out[0], 4);
      Serial.print(' '); Serial.print(out[1], 4);
    }
    Serial.println(); }
  delay(30);
}
