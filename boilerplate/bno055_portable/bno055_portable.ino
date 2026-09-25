/* BOILERPLATE — a real motion sensor, on almost any board.
   =========================================================
   Pose the board, set the three knobs to the sound you want, press SAVE.
   Two saves and it starts playing.

   THIS IS THE PORTABLE ONE, and that is the whole reason it exists.
   boilerplate/stemma_bno055 is the same instrument with more in it -- it saves
   what you taught it to flash, so the instrument survives a reboot -- but
   Preferences.h and Wire.begin(SDA, SCL) are Espressif calls, so that file
   builds on an ESP32 and nowhere else. This one uses neither. Measured
   compiling on a Raspberry Pi Pico (68,896 bytes) and an ESP32-S3, and
   refusing an Uno below with an explanation instead of a compiler error.
   ========================================================================= */
#include <Wire.h>
#include <Adafruit_BNO055.h>
/* NOT FOR AN 8-BIT AVR. The Adafruit BNO055 driver plus this sketch's globals
   leave about 80 bytes of stack on an Uno, and training needs roughly 200. It
   would compile, flash, and then corrupt memory the first time you pressed
   record -- silently, which is the worst way for it to fail. If you are on an
   Uno use boilerplate/any_sensor: the same instrument, and it shrinks to fit.
   Delete these four lines if you know what you are doing and have measured it. */
#if defined(__AVR__)
#error "bno055_portable.ino needs more RAM than an AVR has. Use boilerplate/any_sensor on an Uno."
#endif
#include "iris.h"
#if IRIS_VERSION_MAJOR != 0 || IRIS_VERSION_MINOR != 2
#error "This sketch is written for iris 0.2. Copy iris.h from iris 0.2 (https://github.com/kylebsmith/iris) into this sketch's folder, next to the .ino file, replacing the copy there."
#endif

#define SAVE_BTN 0                 /* BOOT button */
#define POT_A 4
#define POT_B 5
#define POT_C 6

/* An ESP32's analogue inputs are 12-bit and read up to 4095. Almost everything
   else in the Arduino world is 10-bit and reads up to 1023. Dividing by the
   wrong one does not crash: iris fits its output range to whatever you
   actually demonstrate, so it still trains. It just means your knobs would
   only ever reach a quarter of their travel, and send the sound 0.0 to 0.25
   where every comment in this file promises 0.0 to 1.0. This file runs on both
   kinds of board, so it has to ask which one it is on. */
#if defined(ARDUINO_ARCH_ESP32)
#define ADC_MAX 4095.0f
#else
#define ADC_MAX 1023.0f
#endif

Adafruit_BNO055 bno = Adafruit_BNO055(55, 0x28, &Wire);

/* The shape of the instrument: 2 inputs, 12 hidden units, 3 outputs,
   room for 16 demonstrations. Written here... */
static unsigned char memory[IRIS_ARENA(2, 12, 3, 16)];
static iris *k;
static int demos = 0;

void setup() {
  Serial.begin(115200);
  Wire.begin();
  /* bno.begin() returns false when the sensor is not answering. Ignoring it
     was the worst bug in this file: with the cable out, every reading is a
     clean 0.0, every demonstration records the same input, training succeeds,
     the status stays 0, and the instrument plays one frozen number for ever.
     Nothing anywhere says the sensor is missing. Stop instead. */
  /* Try the other address before giving up. The ADR pad on the back of the
     Adafruit board moves it from 0x28 to 0x29, boards ship both ways, and the
     failure message below names both -- so it has to actually try both, or it
     sends someone to reseat a cable that was never the problem. */
  if (!bno.begin()) {
    bno = Adafruit_BNO055(55, 0x29, &Wire);
  }
  if (!bno.begin()) {
    Serial.println(F("No BNO055 found at 0x28 or 0x29. Check the cable at both ends."));
    Serial.println(F("Halted -- fix the wiring and press RESET."));
    for (;;) delay(1000);
  }
  pinMode(SAVE_BTN, INPUT_PULLUP);

  /* ...and written again here. The two must agree or iris_init refuses. */
  k = iris_init(memory, sizeof memory, 2, 12, 3, 16, /*seed=*/1234);
  if (!k) { Serial.println("iris_init refused: the two shapes disagree"); for(;;); }
}

/* Training in slices, so the instrument never goes deaf. iris_train() blocks
   for seconds at eight or more demonstrations on an ESP32-S3 (device_torture
   test 5 measures it on your board). iris_train_begin + iris_train_slice do
   the same fit in pieces and end bit-identical to iris_train, whatever the
   slice size, with a prediction between slices or not.
   Same pattern as boilerplate/any_sensor. */
static bool training = false;

static void start_training(void) {
  if (!iris_train_begin(k, 0)) { Serial.println(F("TRAINING REFUSED.")); return; }
  training = true;
  Serial.println(F("learning -- keep moving, it stays alive."));
}

static void keep_training(void) {
  if (!training) return;
  iris_train_slice(k, 64);
  if (iris_train_busy(k)) return;
  training = false;
  if (iris_is_trained(k)) Serial.println(F("trained."));
  else { Serial.print(F("TRAINING FAILED -- status "));
         Serial.println((int)iris_get_status(k)); }
}

void loop() {
  imu::Vector<3> g = bno.getVector(Adafruit_BNO055::VECTOR_GRAVITY);
  float in[2]  = { (float)g.x(), (float)g.y() };
  float out[3];

  if (digitalRead(SAVE_BTN) == LOW) {
    out[0] = analogRead(POT_A) / ADC_MAX;
    out[1] = analogRead(POT_B) / ADC_MAX;
    out[2] = analogRead(POT_C) / ADC_MAX;
    /* Ask WHY it refused -- see stemma_bno055. "full" is only one of three
       reasons, and it is the least likely one on a fresh board. */
    if (!iris_record(k, in, out)) {
      if (iris_get_status(k) == IRIS_STORE_FULL)
        Serial.println(F("full -- no room for more demonstrations"));
      else
        Serial.println(F("refused: a reading or a target is not a number. "
                         "Check the wiring."));
    }
    else {
      /* SAY SO ON EVERY SUCCESSFUL RECORD, not only on failure. This used to
         print nothing at all when the first save worked -- ++demos made it 1,
         the >= 2 test was false, and the board went silent. So a FAILED save
         was loud and a SUCCESSFUL one was invisible, which is backwards, and it
         taught a student on their very first press that the button does
         nothing. Every other sketch here prints a count; this one did not. */
      ++demos;
      Serial.print(F("saved. demonstrations: ")); Serial.println(demos);
      if (demos >= 2) start_training();
      else Serial.println(F("one more, somewhere different, and it will play."));
    }
    delay(300);
  }

  keep_training();   /* a slice per pass, free when idle */

  if (demos >= 2) {
    iris_predict(k, in, out);
    /* Serial.print, not Serial.printf -- printf on Serial is an Espressif
       extension and this file is meant to be copied onto any board. */
    Serial.print(out[0], 3); Serial.print(' ');
    Serial.print(out[1], 3); Serial.print(' ');
    Serial.println(out[2], 3);
  }
  delay(20);
}
