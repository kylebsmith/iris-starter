/* ---------------------------------------------------------------------------
   iris_tilt — tilt the board, it learns what you meant.

   Three demonstrations, then it plays. Everything between the three gets filled
   in by the network, and that in-between is the instrument.

   HARDWARE
     ES3C28P (ESP32-S3) + Adafruit BNO055 orientation sensor (Adafruit 4646)
     on the board's I2C socket (I2C, inter-integrated circuit: the two-wire
     bus the sensor talks on). PARTS.md shows the cables and which wire goes
     where.

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

   USB MODE (USB, Universal Serial Bus): either works. In USB-OTG (On-The-Go,
   driven by the TinyUSB software) mode 'R' uses the core's
   usb_persist_restart. In Hardware CDC and JTAG mode (the chip's fixed
   serial-and-debug port: CDC, Communications Device Class, is the USB serial
   standard; JTAG, Joint Test Action Group, a debugging interface) it sets the
   chip's force-download flag and restarts, which the chip's built-in loader
   reads at boot; that path is not yet tested on the board.

   WHY THIS PRINTS INSTEAD OF SENDING MIDI
     Printing proves the learning works with nothing between the network and
     your eyes. iris_instrument is the next step: it sends MIDI (Musical
     Instrument Digital Interface, the standard message format synthesisers
     understand) over USB, which needs the TinyUSB mode. The number printed
     here runs 0 to 127, the range of one MIDI controller value.
   --------------------------------------------------------------------------- */

#include <Wire.h>
#include <Adafruit_BNO055.h>
/* BOARD SETTINGS. This sketch uses the ES3C28P's pins, so it needs the
   ESP32-S3 board entry; Serial reaches the computer through the chip's own
   USB port only with USB CDC On Boot enabled. */
#if defined(ARDUINO_ARCH_ESP32) && !defined(CONFIG_IDF_TARGET_ESP32S3)
#error "This sketch is for the ESP32-S3 display board. Set Tools -> Board -> esp32 -> ESP32S3 Dev Module, then set the board options in GET-STARTED.md."
#endif
#if defined(CONFIG_IDF_TARGET_ESP32S3) && !ARDUINO_USB_CDC_ON_BOOT
#error "Set Tools -> USB CDC On Boot -> Enabled. The board's USB socket is the chip's own USB port; with this setting off, Serial prints to pins 43 and 44 instead and Serial Monitor stays empty."
#endif
#if ARDUINO_USB_MODE
#include "soc/rtc_cntl_reg.h"   /* Hardware CDC and JTAG: the force-download flag */
#else
#include "esp32-hal-tinyusb.h"  /* USB-OTG (TinyUSB): usb_persist_restart */
#endif
#include "iris.h"
#if IRIS_VERSION_MAJOR != 0 || IRIS_VERSION_MINOR != 2
#error "This sketch is written for iris 0.2. Copy iris.h from iris 0.2 (https://github.com/kylebsmith/iris) into this sketch's folder, next to the .ino file, replacing the copy there."
#endif

/* 'R': restart into the chip's built-in loader, ready for an upload. */
static void restart_into_bootloader(void) {
  Serial.println("restarting into the bootloader -- upload now.");
  Serial.flush();
  delay(100);
#if ARDUINO_USB_MODE
  /* Hardware CDC and JTAG mode: the flag makes the next boot enter download
     mode over the same USB port. Needs a test on the board. */
  REG_WRITE(RTC_CNTL_OPTION1_REG, RTC_CNTL_FORCE_DOWNLOAD_BOOT);
  esp_restart();
#else
  usb_persist_restart(RESTART_BOOTLOADER);
#endif
}

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
    bno = Adafruit_BNO055(55, 0x29, &Wire);      /* address-select (ADR) pad bridged? */
  }
  if (!bno.begin()) {
    /* Say what is actually on the bus rather than just failing. 0x38 (touch)
       and 0x18 (audio codec) are the board's own chips and answer whatever
       the sensor does. */
    Serial.println("No BNO055. Devices answering on I2C:");
    for (uint8_t a = 8; a < 120; ++a) {
      Wire.beginTransmission(a);
      if (Wire.endTransmission() == 0) Serial.printf("  0x%02X\n", a);
    }
    Serial.println("Expected 0x28 or 0x29 (0x18 and 0x38 are the board's own chips).");
    Serial.println("Check the sensor's four wires against PARTS.md and press RESET.");
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
    if (ch == 'R')                   restart_into_bootloader();
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
      /* Count what the library ACCEPTED, not what we offered it. A counter
         bumped for a refused reading would say "recorded 3" while the
         instrument held 2, trained on 2 and played a smooth, plausible
         number without mentioning it. */
      if (!iris_record(k, in, &target))
        Serial.println("that reading was refused -- not counted. Try again.");
      else {
      Serial.printf("recorded %d  (tilt %.2f, %.2f)\n",
                    ++recorded, in[0], in[1]);
      if (recorded == 3) {
        uint32_t t0 = millis();
        iris_train(k);
        if (!iris_is_trained(k))
          Serial.printf("\nTRAINING FAILED -- status %d. The instrument is not fitted: press c and record again.\n",
                        (int)iris_get_status(k));
        else
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
           keep their +/-1e30 starting values, and the warning below would
           print a spread of -2e30. */
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
      int value = (int)(out * 127.0f + 0.5f);          /* 0..127, a MIDI controller value */
      Serial.printf("tilt %6.2f %6.2f  ->  %3d  ",
                    in[0], in[1], value);
      for (int i = 0; i < value / 4; ++i) Serial.print('#');
      Serial.println();
    }
  }
  delay(10);
}
