/* ---------------------------------------------------------------------------
   iris_tilt — tilt the board, it learns what you meant.

   Three demonstrations, then it plays. Everything between the three gets filled
   in by the network, and that in-between is the instrument.

   HARDWARE
     ES3C28P (ESP32-S3) + Adafruit BNO055 absolute orientation on STEMMA QT.

   LIBRARIES  (Tools -> Manage Libraries, search and install)
     Adafruit BNO055
     Adafruit Unified Sensor      <- the BNO055 library pulls this in
   iris.h is already sitting next to this file. Nothing to install for that.

   HOW TO USE IT
     1. Flash, open Serial Monitor at 115200.
     2. Tilt the board. Press BOOT. That pose is "0".
     3. Tilt somewhere else. Press BOOT. That's "64".
     4. Once more. Press BOOT. That's "127".
     5. It trains, then prints a number that follows your hand — including
        through poses you never showed it. That is the whole idea.
     Hold BOOT for two seconds to wipe it and start again.

     If you cannot reach BOOT — it is often buried once the board is in an
     enclosure — send any character in Serial Monitor to record, and 'c' to
     clear. Same thing, no button.

   IF THE BOARD STOPS ACCEPTING UPLOADS
     Send 'R' over Serial. It reboots into the bootloader, ready to flash.
     Failing that, the physical recovery is: hold BOOT, tap RESET, release
     BOOT. Build an escape hatch into anything you write before you flash it,
     not after.

   WHY THIS PRINTS INSTEAD OF SENDING MIDI
     A USB-MIDI build on this board needs different USB settings, and getting
     them wrong bricks enumeration until you unplug it. I lost a day to that.
     So we prove the learning works first, on the safe USB config. MIDI is the
     next sketch and it carries an escape hatch. Keep that order.
   --------------------------------------------------------------------------- */

#include <Wire.h>
#include <Adafruit_BNO055.h>
#include "esp32-hal-tinyusb.h"
#include "iris.h"

/* Both of these settings fail silently when wrong: CDC off gives a board that
   runs and cannot talk to you, and the wrong USB Mode leaves the 'R' escape
   hatch without a bootloader to restart into. So here is the error instead. */
#if ARDUINO_USB_MODE
#error "Wrong USB Mode. Set Tools -> USB Mode -> 'USB-OTG (TinyUSB)'. Every sketch in this repo uses that one setting."
#endif
#if !ARDUINO_USB_CDC_ON_BOOT
#error "Set Tools -> USB CDC On Boot -> 'Enabled', or Serial Monitor will stay empty forever and nothing will tell you why."
#endif

/* The board's touch controller and audio codec already live on this bus.
   Your sensor joins them. */
#define SDA_PIN   16
#define SCL_PIN   15
#define BOOT_BTN   0

Adafruit_BNO055 bno = Adafruit_BNO055(55, 0x28, &Wire);

/* Two inputs, twelve hidden units, one output, room for eight demonstrations.
   IRIS_ARENA computes the size at compile time, so this is a plain fixed array
   and you know exactly how much memory the instrument uses before it runs. */
static unsigned char memory[IRIS_ARENA(2, 12, 1, 8)];
static iris *k;

static const float TARGETS[3] = { 0.0f, 64.0f, 127.0f };
static int recorded = 0;

static void prompt() {
  if (recorded < 3)
    Serial.printf("tilt, then press BOOT -- or send any key here except R"
                  "  ->  demo %d of 3 (target %.0f)\n",
                  recorded + 1, TARGETS[recorded]);
}

void setup() {
  Serial.begin(115200);
  delay(400);
  pinMode(BOOT_BTN, INPUT_PULLUP);
  Wire.begin(SDA_PIN, SCL_PIN);

  Serial.println("\niris_tilt\n");

  if (!bno.begin()) {
    bno = Adafruit_BNO055(55, 0x29, &Wire);      /* ADR pad bridged? */
  }
  if (!bno.begin()) {
    /* Say what is actually on the bus rather than just failing. Nine times out
       of ten the STEMMA cable is not fully clicked in at one end. */
    Serial.println("No BNO055. Devices answering on I2C:");
    for (uint8_t a = 8; a < 120; ++a) {
      Wire.beginTransmission(a);
      if (Wire.endTransmission() == 0) Serial.printf("  0x%02X\n", a);
    }
    Serial.println("Expected 0x28 (or 0x29). Reseat the cable and press RESET.");
    while (1) delay(1000);
  }

  k = iris_init(memory, sizeof memory, 2, 12, 1, 8, /*seed=*/1234);
  if (!k) {
    /* The four numbers here must match the four in IRIS_ARENA above. If you
       change the shape in one place and not the other, the arena is too small
       and iris refuses rather than scribbling past the end of it. */
    Serial.println("iris_init refused: arena too small for that shape.");
    while (1) delay(1000);
  }
  Serial.printf("ready. instrument uses %u bytes\n\n", (unsigned)sizeof memory);
  prompt();
}

void loop() {
  /* Gravity, not Euler angles. Euler angles wrap: somewhere in the rotation
     they jump from +180 to -180, so two poses one degree apart arrive as
     opposite ends of the range and the sound falls off a cliff there. Gravity
     just points down and never wraps.

     And no scaling. iris fits its input range to the demonstrations you give
     it, so raw sensor units work exactly as well as anything you divide them
     by — and a sensor mounted slightly off level needs no correction. */
  imu::Vector<3> g = bno.getVector(Adafruit_BNO055::VECTOR_GRAVITY);
  float in[2] = { (float)g.x(), (float)g.y() };

  /* Record either by pressing BOOT, or by sending any character over Serial.
     The serial route exists because some enclosures bury the button, and
     because it lets you drive this from a laptop while you work. Send 'c' to
     clear instead of holding the button. */
  static uint32_t held = 0;
  bool serial_rec = false, serial_clear = false;
  while (Serial.available()) {
    int ch = Serial.read();
    /* 'R' is reserved: it is the way back into the bootloader when the board
       is in an enclosure and BOOT is unreachable. */
    if (ch == 'R')                   usb_persist_restart(RESTART_BOOTLOADER);
    else if (ch == 'c' || ch == 'C') serial_clear = true;
    else if (ch > ' ')               serial_rec = true;
  }

  bool down = (digitalRead(BOOT_BTN) == LOW);
  if (down && !held) held = millis();

  if (serial_clear) {
    iris_clear(k); recorded = 0;
    Serial.println("\ncleared.\n"); prompt();
    return;
  }

  if (down && held && millis() - held > 2000) {          /* long press: wipe */
    iris_clear(k);
    recorded = 0;
    Serial.println("\ncleared.\n");
    prompt();
    while (digitalRead(BOOT_BTN) == LOW) delay(10);
    held = 0;
    return;
  }

  if ((!down && held) || serial_rec) {                   /* short press, or a keystroke */
    held = 0;
    if (recorded < 3) {
      float target = TARGETS[recorded] / 127.0f;         /* keep MIDI in 0..1 */
      /* Count what the library ACCEPTED, not what we offered it. These used
         to drift apart: a refused reading still bumped our own counter, so
         the sketch said "recorded 3" while the instrument held 2, trained on
         2, played a smooth plausible number, and never mentioned it. */
      if (!iris_record(k, in, &target))
        Serial.println("that reading was refused -- not counted. Try again.");
      else {
      Serial.printf("recorded %d  (tilt %.2f, %.2f)\n",
                    ++recorded, in[0], in[1]);
      if (recorded == 3) {
        uint32_t t0 = millis();
        int fitted = iris_train(k);
        if (!fitted) Serial.println("TRAINING FAILED -- the instrument is not fitted.");
        Serial.printf("\ntrained in %lu ms, %d epochs. Now move it.\n",
                      (unsigned long)(millis() - t0), iris_train_epochs_done(k));

        /* The most likely first mistake is three poses that are nearly the
           same. Training reports perfect success either way -- it fitted what
           it was given -- and the instrument is then a switch rather than a
           surface. Nothing downstream can tell you why, so check here: how far
           apart were the poses actually? */
        float lo[2] = {  1e30f,  1e30f }, hi[2] = { -1e30f, -1e30f };
        int seen = 0;
        for (int i = 0; i < recorded; ++i) {
          float ein[2], eout;
          if (!iris_get(k, i, ein, &eout)) continue;
          seen++;
          for (int d = 0; d < 2; ++d) {
            if (ein[d] < lo[d]) lo[d] = ein[d];
            if (ein[d] > hi[d]) hi[d] = ein[d];
          }
        }
        float spread = (hi[0] - lo[0]) > (hi[1] - lo[1]) ? hi[0] - lo[0] : hi[1] - lo[1];
        /* seen guards the sentinels: with no readable demonstrations lo and hi
           keep their +/-1e30 starting values and this printed "your three poses
           were only -2000000000000000000000000000000.0 apart". */
        if (seen > 0 && spread < 2.0f)   /* metres per second squared, of ~9.8 */
          Serial.printf("heads up: your three poses were only %.1f apart. That is "
                        "very close,\nso expect an abrupt mapping. Send 'c' and "
                        "try again with bigger tilts.\n", spread);
        Serial.println();
      } else prompt();
      }
    }
  }

  if (recorded == 3) {                                   /* play */
    static uint32_t last = 0;
    if (millis() - last > 100) {
      last = millis();
      float out;
      iris_predict(k, in, &out);
      int cc = (int)(out * 127.0f + 0.5f);
      Serial.printf("tilt %6.2f %6.2f  ->  CC %3d  ",
                    in[0], in[1], cc);
      for (int i = 0; i < cc / 4; ++i) Serial.print('#');
      Serial.println();
    }
  }
  delay(10);
}
