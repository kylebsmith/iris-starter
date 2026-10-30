# Where the target comes from

`iris_record(k, in, out)` wants `out`: the sound you mean, as numbers. You have
a sound you like and no idea what number it is. That gap is the whole
difficulty of starting with this library, and closing it is one function:

```c
static void read_target(float *out);   /* fill out[0..N_OUTPUTS-1], each 0 to 1 */
```

iris never sees your interface. It takes `N_OUTPUTS` numbers between 0 and 1 and
does not care where they came from.

`boilerplate/any_sensor` and `boilerplate/add_iris` ship the one that needs no
hardware: values held in memory, nudged from the Serial Monitor. This page has
four more. Each is a drop-in replacement for that function and nothing else in
the sketch changes — every pin number, scale factor and compile-time check a
target source needs lives inside the function that uses it.

All four compile for the ESP32-S3 under `--warnings all` in place of the
shipped version; the two that need no board-specific port compile for an
Arduino Uno as well. None has been run on hardware yet.

> One rule if you write your own. `read_target` is called once a pass, before
> the sketch's own `d` and `c` handler, because a source that *holds* a value
> has to be polled to notice a change. If yours reads the serial port, take only
> the keys you own — `Serial.peek()` before `Serial.read()` — or `d` and `c`
> stop working.

## Potentiometers, one per output

The version the kit shipped until now, and still the right answer when you
have the knobs: three analogue pins, read directly. It needs no screen and no
keys, and the knobs are absolute — where they point is what you get.

```c
static void read_target(float *out) {
  /* One knob per output. On an ESP32 these are the ES3C28P's expansion-socket
     pins, the only ones on that board that reach a connector and read a
     voltage (GPIO 2 and 3 on the first analog-to-digital converter, GPIO 14
     on the second; CHECK ON THE BOARD, see boilerplate/stemma_bno055 and
     PARTS.md). Elsewhere, A0-A2. */
#if defined(ARDUINO_ARCH_ESP32)
  static const int POT_PIN[] = { 2, 3, 14 };
#else
  static const int POT_PIN[] = { A0, A1, A2 };
#endif
  /* There must be exactly N_OUTPUTS of them, and if there are not this fails
     to compile with the name of the problem in the error. Without it a
     missing entry is read from past the end of the array, and that knob reads
     whatever pin number happens to be there.
     It lives inside this function because it is this function's problem. Put
     it at the top of the file instead and a student who moves to a
     touchscreen -- and so has no POT_PIN to count -- gets a compile error
     about potentiometers for changing something else entirely. */
  struct compile_time_check {
    char POT_PIN_must_have_exactly_N_OUTPUTS_entries
         [(sizeof POT_PIN / sizeof POT_PIN[0]) == N_OUTPUTS ? 1 : -1];
  };
  /* An ESP32's analogue inputs are 12-bit and read up to 4095. Almost
     everything else in the Arduino world is 10-bit and reads up to 1023.
     Divide by the wrong one and the knobs reach only part of their travel --
     on an Uno, dividing by 4095 gives 0.0 to 0.25 where send_sound expects
     0.0 to 1.0 -- and nothing announces it, because iris fits its
     output range to what you demonstrate and trains anyway. So the compiler
     picks the number for the board. */
#if defined(ARDUINO_ARCH_ESP32)
  const float adc_max = 4095.0f;
#else
  const float adc_max = 1023.0f;
#endif
  for (int i = 0; i < N_OUTPUTS; ++i)
    out[i] = analogRead(POT_PIN[i]) / adc_max;   /* <<< YOUR INTERFACE HERE */
}
```

## Sliders on a touchscreen (LVGL)

LVGL, the Light and Versatile Graphics Library, is the widget toolkit the
studio's touchscreens are driven with. Build one slider per output
once, in setup, keep the pointers, and read them here. The range is yours
to pick: 0 to 1000 means dividing by 1000 gives the 0 to 1 iris wants.

In your `setup()`, after `lv_init()` and your display and touch driver:

    for (int i = 0; i < N_OUTPUTS; ++i) {
      slider[i] = lv_slider_create(lv_scr_act());
      lv_slider_set_range(slider[i], 0, 1000);
      lv_slider_set_value(slider[i], 500, LV_ANIM_OFF);
      lv_obj_set_width(slider[i], 180);
      lv_obj_align(slider[i], LV_ALIGN_TOP_MID, 0, 30 + 40 * i);
    }

and call lv_timer_handler() once a pass in loop(), or the screen never
redraws and a drag is never seen.

```c
#include <lvgl.h>
static lv_obj_t *slider[N_OUTPUTS];
static void read_target(float *out) {
  static bool complained = false;
  for (int i = 0; i < N_OUTPUTS; ++i) {
    if (slider[i]) {
      out[i] = (float)lv_slider_get_value(slider[i]) / 1000.0f;
      continue;
    }
    out[i] = 0.5f;                // setup never built this one
    if (!complained) {            // say so: a target stuck at 0.5 trains fine
      Serial.println(F("read_target: a slider was never created. "
                       "See your setup()."));
      Serial.println(F("Until it is, that output is stuck at 0.5 -- and "
                       "0.5 is what gets demonstrated."));
      complained = true;
    }
  }
}
```

## Mechanical key switches, two per output

A key switch is a button: one pin to the switch, the switch to ground,
read with INPUT_PULLUP so the pin reads LOW while the key is down. Two keys
per output, up and down, and holding one ramps. The step is per pass of
loop(), which runs about every 20 ms, so 0.01 moves an output end to end in
about two seconds.

In your `setup()`:

```c
for (int i = 0; i < N_OUTPUTS; ++i) {
  pinMode(UP_PIN[i],   INPUT_PULLUP);
  pinMode(DOWN_PIN[i], INPUT_PULLUP);
}
```

**Check these pin numbers on your own board** before you wire anything: a pin
the board already uses for something else is the usual first bug.

```c
static const int UP_PIN[]   = { 4, 5, 6 };    // one per output
static const int DOWN_PIN[] = { 7, 8, 9 };    // one per output
static void read_target(float *out) {
  struct compile_time_check {
    char UP_PIN_and_DOWN_PIN_must_have_exactly_N_OUTPUTS_entries
         [(sizeof UP_PIN   / sizeof UP_PIN[0])   == N_OUTPUTS &&
          (sizeof DOWN_PIN / sizeof DOWN_PIN[0]) == N_OUTPUTS ? 1 : -1];
  };
  static float held[N_OUTPUTS];
  static bool started = false;
  if (!started) {
    for (int i = 0; i < N_OUTPUTS; ++i) held[i] = 0.5f;
    started = true;
  }
  for (int i = 0; i < N_OUTPUTS; ++i) {
    if (digitalRead(UP_PIN[i])   == LOW) held[i] += 0.01f;
    if (digitalRead(DOWN_PIN[i]) == LOW) held[i] -= 0.01f;
    if (held[i] > 1.0f) held[i] = 1.0f;
    if (held[i] < 0.0f) held[i] = 0.0f;
    out[i] = held[i];
  }
}
```

## An incoming MIDI control change

MIDI, the Musical Instrument Digital Interface, is the message format
synthesisers speak. A control change is three bytes: 0xB0 plus the channel
number, then the controller number, then its value, 0 to 127. Turn
controller 1 on any MIDI controller and out[0] follows it, controller 2
drives out[1], and so on up to N_OUTPUTS.

This reads MIDI at 31250 baud (bits per second) on Serial1, a second
hardware serial port, so the Serial Monitor on Serial keeps working.
In your `setup()`: `Serial1.begin(31250);`

A board with no `Serial1` needs a software serial port instead, or USB MIDI
(USB, Universal Serial Bus) through Adafruit's TinyUSB library, which is
what iris_instrument does.

```c
static void read_target(float *out) {
  static float held[N_OUTPUTS];
  static bool started = false, seen = false;
  static int status = 0, controller = -1;
  if (!started) {
    for (int i = 0; i < N_OUTPUTS; ++i) held[i] = 0.5f;
    started = true;
  }
  while (Serial1.available()) {
    const int b = Serial1.read();
    if (b & 0x80) { status = b; controller = -1; }    // a status byte
    else if ((status & 0xF0) == 0xB0) {               // a control change
      if (controller < 0) controller = b;             // its number
      else {                                          // and its value
        if (controller >= 1 && controller <= N_OUTPUTS) {
          held[controller - 1] = (float)b / 127.0f;
          if (!seen) {        // once: proof the cable and the port are right
            Serial.println(F("a MIDI control change reached read_target."));
            seen = true;
          }
        }
        controller = -1;      // running status: the next pair, same status
      }
    }
  }
  for (int i = 0; i < N_OUTPUTS; ++i) out[i] = held[i];
}
```

## The one that ships, for reference

This is what is already in `boilerplate/any_sensor` and
`boilerplate/add_iris`, so you do not need to copy it — it is here so the five
can be read side by side.

```c
static void read_target(float *out) {
  static float held[N_OUTPUTS];
  static bool started = false;
  static int sel = 0;
  if (!started) {                 // the middle of the range, not zero
    for (int i = 0; i < N_OUTPUTS; ++i) held[i] = 0.5f;
    started = true;
  }
  while (Serial.available()) {
    const int c = Serial.peek();
    if (c >= '1' && c <= '9' && c - '1' < N_OUTPUTS) sel = c - '1';
    else if (c == '+' || c == '=')                   held[sel] += 0.05f;
    else if (c == '-')                               held[sel] -= 0.05f;
    else break;                   // not a key this function owns: leave it
    Serial.read();                // ours: take it off the queue
    if (held[sel] > 1.0f) held[sel] = 1.0f;
    if (held[sel] < 0.0f) held[sel] = 0.0f;
    Serial.print(F("target ")); Serial.print(sel + 1);
    Serial.print(F(" = "));      Serial.println(held[sel], 3);
  }
  for (int i = 0; i < N_OUTPUTS; ++i) out[i] = held[i];
}
```
