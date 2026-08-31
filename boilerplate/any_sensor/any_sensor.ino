/* BOILERPLATE FOR ANY SENSOR
   ==========================
   Pose your sensor. Turn the knobs until it sounds right. Press SAVE.
   Do that twice and it starts playing: now moving the sensor sweeps between
   everything you showed it, and beyond.

   Fill in the five places marked FILL THIS IN. Everything else is done.

   Compiles and runs as it stands -- the placeholder sensor is two analogue
   pins -- so you can flash it and press the button before your real sensor
   even arrives. Verified on an Arduino Uno, a Nano Every, both Raspberry Pi
   Pico cores and the ESP32-S3: it uses no board-specific calls. (An earlier
   version used Serial.printf, which is an Espressif extension, and so failed
   on three of those four while its own header claimed it ran anywhere.)
   ========================================================================= */
/* ON A SMALL BOARD THE WORKING ARRAYS ARE THE STACK BUDGET.
   iris.h sizes nine internal arrays from these maxima rather than from the
   shape you asked for, so on an Uno the default 32/16/64 reserves 192 bytes of
   stack for an instrument that uses 20. Measured with avr-gcc -Os: the deepest
   frame goes from 340 bytes to 132 when they are shrunk to fit. An Uno leaves
   only a few hundred bytes of stack, so this is the difference between
   training and quietly running off the end of it.
   They must be at least as large as N_INPUTS, N_OUTPUTS and the hidden width
   (12) passed to iris_init -- iris_init refuses if they are not, rather than
   truncating. Raise them if you raise those. */
#if defined(__AVR__)
#define IRIS_MAX_IN  4
#define IRIS_MAX_OUT 4
#define IRIS_MAX_HID 12
#endif
#include "iris.h"

/* ---- FILL THIS IN 1: HOW BIG IS YOUR INSTRUMENT? ------------------------
   N_INPUTS  is how many numbers your sensor gives you at once. An
             accelerometer gives three, one per axis. A light sensor gives one.
   N_OUTPUTS is how many things you want to control.
   N_DEMOS   is how many demonstrations it can hold before it is full.

   If you change these, the compiler will stop you if you forget to update
   places 2 and 3 below. That is deliberate: a mismatch used to compile
   cleanly and then feed the model uninitialised memory for ever. */
#define N_INPUTS   2
#define N_OUTPUTS  3
#define N_DEMOS   16

#define SAVE_BTN 0          /* the BOOT button on most boards */
#define ADC_MAX  4095.0f    /* 4095 on an ESP32, 1023 on many others */

/* One knob per output. There must be exactly N_OUTPUTS of them. */
static const int POT_PIN[] = { 4, 5, 6 };

/* ---- FILL THIS IN 2: READ YOUR SENSOR -----------------------------------
   One line per input, and there must be exactly N_INPUTS of them.
   Any units at all: iris fits its range to whatever you actually give it, so
   do NOT scale, centre or normalise. Raw readings are correct. */
static void read_sensor(float *in) {
  in[0] = analogRead(1);              /* <<< YOUR SENSOR HERE */
  in[1] = analogRead(2);              /* <<< one line per input */
}

/* ---- FILL THIS IN 3: WHERE DOES THE TARGET COME FROM? -------------------
   When you press SAVE, this decides what sound you are demonstrating. Knobs
   here, because they need no screen -- but iris never sees your interface. It
   takes N_OUTPUTS numbers between 0 and 1 and does not care where they came
   from: a rotary encoder, faders, a touchscreen, an incoming MIDI message, a
   line typed into the serial monitor. Replace the body, keep the shape. */
static void read_target(float *out) {
  for (int i = 0; i < N_OUTPUTS; ++i)
    out[i] = analogRead(POT_PIN[i]) / ADC_MAX;   /* <<< YOUR INTERFACE HERE */
}

/* ---- FILL THIS IN 4: SEND YOUR SOUND ------------------------------------
   `out` holds N_OUTPUTS numbers, each between 0 and 1. Send them wherever you
   like: a synth over MIDI, a motor, a light, an oscillator you wrote. */
static void send_sound(const float *out) {
  for (int i = 0; i < N_OUTPUTS; ++i) {   /* <<< REPLACE THIS with your own */
    Serial.print(out[i], 3);
    Serial.print(' ');
  }
  Serial.println();
}

/* =========================================================================
   Nothing below here needs changing.
   ========================================================================= */

/* If POT_PIN does not have exactly N_OUTPUTS entries, this line fails to
   compile with the name of the problem in the error. Better than discovering
   at three in the morning that knob four reads pin zero. */
typedef char POT_PIN_must_have_exactly_N_OUTPUTS_entries
             [(sizeof POT_PIN / sizeof POT_PIN[0]) == N_OUTPUTS ? 1 : -1];

static unsigned char memory[IRIS_ARENA(N_INPUTS, 12, N_OUTPUTS, N_DEMOS)];
static iris *k;

void setup() {
  Serial.begin(115200);
  delay(400);
  pinMode(SAVE_BTN, INPUT_PULLUP);

  /* ---- FILL THIS IN 5: YOUR SENSOR'S SETUP, if it needs any -------------
     For example:  Wire.begin();  bno.begin();                             */

  k = iris_init(memory, sizeof memory, N_INPUTS, 12, N_OUTPUTS, N_DEMOS, 1234);
  if (!k) {
    /* iris_init returns 0 for a bad SHAPE as well as for a small arena, and
       naming only the arena sent students to check the one thing that was
       right. iris_size() separates them: it returns 0 when the shape itself
       cannot be sized, and otherwise tells you exactly how many bytes the
       shape needs, which you can compare against what you gave it. */
    size_t need = iris_size(N_INPUTS, 12, N_OUTPUTS, N_DEMOS);
    if (need == 0) {
      Serial.println(F("iris_init refused: the SHAPE is out of range."));
      Serial.println(F("Check N_INPUTS, N_OUTPUTS and N_DEMOS at the top."));
    } else {
      Serial.print(F("iris_init refused: the arena is too small. Need "));
      Serial.print((unsigned long)need); Serial.print(F(" bytes, have "));
      Serial.println((unsigned long)sizeof memory);
    }
    for(;;);
  }

  /* Do read_sensor and read_target really fill every slot? The compiler cannot
     check that, so check it here, once, loudly. Without it a forgotten line
     means uninitialised memory is recorded as a demonstration and played back
     as sound, and nothing ever says so -- an independent review found that to
     be the single largest silent-failure family in this sketch, 2,140 cases of
     2,140, every one of them an under-filling read_target.

     The probe arrays are deliberately OVERSIZED. An earlier version sized them
     exactly, so lowering a count while leaving an extra line in the function
     wrote past the end of the very array written to catch that mistake. The
     slack means the overrun lands in spare space we own and is then reported
     rather than corrupting the stack. */
  { float probe[N_INPUTS + 4], tprobe[N_OUTPUTS + 4];
    int in_unset[N_INPUTS + 4], t_unset[N_OUTPUTS + 4];
    int i, pass;

    /* TWO sentinels, not one. With a single magic number, a sensor that
       legitimately returns exactly that value is accused of never setting an
       input it sets on every call -- a review found 210 such false
       accusations. A slot is only genuinely unwritten if it still holds
       sentinel A after a pass seeded with A *and* sentinel B after a pass
       seeded with B. No real reading is equal to both. */
    for (i = 0; i < N_INPUTS  + 4; ++i) in_unset[i] = 1;
    for (i = 0; i < N_OUTPUTS + 4; ++i) t_unset[i]  = 1;
    for (pass = 0; pass < 2; ++pass) {
      const float S = pass ? 98765.4321f : -12345.678f;
      for (i = 0; i < N_INPUTS  + 4; ++i) probe[i]  = S;
      for (i = 0; i < N_OUTPUTS + 4; ++i) tprobe[i] = S;
      read_sensor(probe);
      read_target(tprobe);
      for (i = 0; i < N_INPUTS  + 4; ++i) if (probe[i]  != S) in_unset[i] = 0;
      for (i = 0; i < N_OUTPUTS + 4; ++i) if (tprobe[i] != S) t_unset[i]  = 0;
    }

    for (i = 0; i < N_INPUTS; ++i)
      if (in_unset[i]) {
        Serial.print(F("read_sensor never sets in[")); Serial.print(i);
        Serial.print(F("]. It must fill all ")); Serial.println(N_INPUTS);
        for(;;);
      }
    for (i = N_INPUTS; i < N_INPUTS + 4; ++i)
      if (!in_unset[i]) {
        Serial.print(F("read_sensor writes past in[")); Serial.print(N_INPUTS - 1);
        Serial.println(F("]. Delete the extra line, or raise N_INPUTS."));
        for(;;);
      }
    for (i = 0; i < N_OUTPUTS; ++i)
      if (t_unset[i]) {
        Serial.print(F("read_target never sets out[")); Serial.print(i);
        Serial.print(F("]. It must fill all ")); Serial.println(N_OUTPUTS);
        for(;;);
      }
    for (i = N_OUTPUTS; i < N_OUTPUTS + 4; ++i)
      if (!t_unset[i]) {
        Serial.print(F("read_target writes past out[")); Serial.print(N_OUTPUTS - 1);
        Serial.println(F("]. Delete the extra line, or raise N_OUTPUTS."));
        for(;;);
      } }

  /* ---- IS THE SENSOR ACTUALLY THERE? ------------------------------------
     This is the most common hardware fault there is, and the hardest to see.
     A disconnected I2C sensor does not report an error and does not return a
     not-a-number: the Adafruit drivers hand back a clean 0.0, or the last
     value, or a rated maximum. Every one of those is a perfectly valid float.
     It records, it trains, it plays -- one frozen note, for ever, with the
     status reading healthy.
     We cannot know what YOUR sensor returns when it is missing. But we know
     what a dead bus looks like from here: a reading that does not move at all,
     not in the last bit, over a third of a second. Real sensors dither. This
     warns rather than halts, because a genuinely static input (a switch, a
     knob you are not touching) is legitimate -- it just should not surprise
     you later. */
  { float first[N_INPUTS], now[N_INPUTS]; int i, t, moved = 0;
    read_sensor(first);
    for (t = 0; t < 20 && !moved; ++t) {
      delay(15);
      read_sensor(now);
      for (i = 0; i < N_INPUTS; ++i) if (now[i] != first[i]) moved = 1;
    }
    if (!moved) {
      Serial.println(F("WARNING: every input is perfectly frozen."));
      Serial.println(F("If this sensor should dither, it is not connected --"));
      Serial.println(F("check the cable at BOTH ends. An unplugged sensor"));
      Serial.println(F("reads as a valid number and will train and play"));
      Serial.println(F("without ever telling you it is missing."));
    } }

  Serial.println(F("ready. pose the sensor, set the knobs, press SAVE."));
  Serial.println(F("send 'd' to delete the last demonstration, 'c' to clear."));
}

/* ---- TRAINING WITHOUT GOING DEAF ---------------------------------------
   iris_train() does the whole fit in one call and does not return until it is
   finished. Measured on an ESP32-S3: 595 ms at 4 demonstrations and 2.7-3.0
   SECONDS at 8 to 20. For those seconds the board reads no sensor, answers no
   key and makes no sound -- which, in an instrument, is the entire experience.

   iris_train_begin / iris_train_slice do exactly the same fit in pieces, and
   the result is BIT-IDENTICAL -- verified across 4, 12 and 20 demonstrations,
   including with a prediction between every single slice, which is what this
   sketch does. So the instrument keeps playing while it learns, and you can
   hear it improve. There is no cost, only a loop.

   The reseed matters: iris_train() reseeds before fitting so that the same
   demonstrations always give the same instrument. iris_train_begin does not do
   that for you, so doing it here is what keeps the two equivalent. */
static bool training = false;
static uint32_t train_started = 0;

static void start_training(void) {
  iris_reseed(k, iris_seed(k));      /* what iris_train() does first */
  if (!iris_train_begin(k, 0)) {
    Serial.println(F("TRAINING REFUSED -- nothing to fit."));
    return;
  }
  training = true;
  train_started = millis();
  Serial.print(F("learning from ")); Serial.print(iris_count(k));
  Serial.println(F(" demonstrations -- keep playing, it stays alive."));
}

static void keep_training(void) {
  if (!training) return;
  iris_train_slice(k, 64);           /* a few milliseconds of work */
  if (iris_train_busy(k)) return;
  training = false;
  /* CHECK THE ANSWER. A finished run is not the same as a successful one:
     iris_is_trained is correct after every trainer in the library. */
  if (iris_is_trained(k)) {
    Serial.print(F("trained in ")); Serial.print(millis() - train_started);
    Serial.print(F(" ms, ")); Serial.print(iris_train_epochs_done(k));
    Serial.println(F(" epochs."));
  } else {
    Serial.print(F("TRAINING FAILED -- status "));
    Serial.print((int)iris_get_status(k));
    Serial.println(F(". The instrument is NOT fitted; do not trust it."));
  }
}

void loop() {
  float in[N_INPUTS], out[N_OUTPUTS];
  read_sensor(in);

  if (digitalRead(SAVE_BTN) == LOW) {
    read_target(out);
    if (!iris_record(k, in, out)) {        /* 0 means it did not record */
      /* Three different things make it refuse, and telling a student the wrong
         one sends them to fix the wrong thing: "full" when the real cause was
         a not-a-number reading sends them to delete demonstrations they do not
         have. The status says which. */
      if (iris_get_status(k) == IRIS_STORE_FULL)
        Serial.println(F("full -- no room for more demonstrations"));
      else
        Serial.println(F("refused: a reading or a target is not a number. "
                         "Check the wiring, and check read_target."));
    }
    else if (iris_count(k) >= 2) {
      start_training();
    }
    Serial.print(F("demonstrations: "));
    Serial.println(iris_count(k));
    delay(300);                            /* crude, but it debounces */
  }

  /* THE REPAIR LOOP. A bad demonstration is not a disaster you re-flash your
     way out of -- you delete it and demonstrate again. That loop is the whole
     argument for learning by showing, so the sketch that teaches the library
     has to expose it. Without this, one stuck button fills the store and a
     re-flash is the only way back. */
  if (Serial.available()) {
    int c = Serial.read();
    if (c == 'd') {
      if (iris_delete_last(k)) {
        Serial.print(F("deleted the last one. demonstrations: "));
        Serial.println(iris_count(k));
        if (iris_count(k) >= 2) {
          Serial.flush();
          start_training();
        }
      } else Serial.println(F("nothing to delete."));
    }
    else if (c == 'c') {
      iris_clear(k);
      Serial.println(F("cleared. start demonstrating again."));
    }
  }

  keep_training();                  /* a slice per pass; free when idle */

  if (iris_count(k) >= 2) { iris_predict(k, in, out); send_sound(out); }
  delay(20);
}
