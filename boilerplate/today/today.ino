/* BOILERPLATE — accelerometer in, three parameters out.
   Pose the board, set the three knobs to the sound you want, press SAVE.
   Two saves and it starts playing.                     TODAY'S INTERFACE. */
#include <Wire.h>
#include <Adafruit_BNO055.h>
/* NOT FOR AN 8-BIT AVR. The Adafruit BNO055 driver plus this sketch's globals
   leave about 80 bytes of stack on an Uno, and training needs roughly 200. It
   would compile, flash, and then corrupt memory the first time you pressed
   record -- silently, which is the worst way for it to fail. If you are on an
   Uno use boilerplate/any_sensor, which is the same instrument sized to fit.
   Delete these four lines if you know what you are doing and have measured it. */
#if defined(__AVR__)
#error "today.ino needs more RAM than an AVR has. Use boilerplate/any_sensor on an Uno."
#endif
#include "iris.h"

#define SAVE_BTN 0                 /* BOOT button */
#define POT_A 4
#define POT_B 5
#define POT_C 6

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
   for 2.7-3.0 seconds at eight or more demonstrations on an ESP32-S3, measured.
   iris_train_begin + iris_train_slice do the identical fit in pieces --
   verified bit-identical at 4, 12 and 20 demonstrations, including with a
   prediction between every slice. The reseed keeps them identical: iris_train()
   does it first and iris_train_begin does not.
   Same pattern as boilerplate/any_sensor. */
static bool training = false;

static void start_training(void) {
  iris_reseed(k, iris_seed(k));
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
    out[0] = analogRead(POT_A) / 4095.0f;
    out[1] = analogRead(POT_B) / 4095.0f;
    out[2] = analogRead(POT_C) / 4095.0f;
    /* Ask WHY it refused -- see stemma_bno055. "full" is only one of three
       reasons, and it is the least likely one on a fresh board. */
    if (!iris_record(k, in, out)) {
      if (iris_get_status(k) == IRIS_STORE_FULL)
        Serial.println(F("full -- no room for more demonstrations"));
      else
        Serial.println(F("refused: a reading or a target is not a number. "
                         "Check the wiring."));
    }
    else if (++demos >= 2) start_training();
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
