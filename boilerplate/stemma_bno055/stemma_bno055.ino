/* BOILERPLATE — an Adafruit STEMMA QT sensor over I2C
   ===================================================
   The same instrument as boilerplate/any_sensor, wired to a real sensor
   instead of analogue pins. Shown with the BNO055 orientation board because
   that is what is in the kit, but the shape is what to copy: every STEMMA QT
   board is the same four steps.

   WIRING: the sensor goes on the board's I2C socket (I2C, inter-integrated
   circuit: the two-wire bus that carries data and clock on two pins). That
   socket is 1.25 mm pitch and the sensor's STEMMA QT socket is 1.0 mm, so a
   STEMMA QT cable does not fit the board: PARTS.md lists the lead and
   adapter cable that join them, and which wire goes where. Three knobs go on
   the expansion socket (below).

   LIBRARIES: Tools -> Manage Libraries, install "Adafruit BNO055". Say yes
   when it offers its dependencies. A different sensor means a different
   library, and its own example will show you the two lines to change.

   Pose the board. Turn the knobs until it sounds right. Tap SAVE.
   Twice, and it plays.

   It survives being unplugged. The SAVE button does two things:

     tap  (let go within a second)  records one demonstration: the pose and
                                    the knobs as they were when you pressed,
                                    then starts training in the background.
     hold (a second or longer)      records nothing. It finishes any training
                                    still running, then writes the instrument
                                    you are playing -- every demonstration and
                                    the trained network -- to flash.

   On the next power-up the sketch loads it and plays exactly what you were
   playing when you held SAVE. An instrument saved untrained with two or more
   demonstrations (its training had failed) retrains from them as soon as it
   has loaded them; one saved with a single demonstration trains at the next
   tap, which records the second.

   The other sketches deliberately do not save, so that the first file you
   read is as short as it can be -- this is the one to copy the two calls
   (iris_save and iris_load) out of when you want an instrument to outlive the
   cable.

   This file needs an ESP32. Two things in it are Espressif-only: Preferences.h,
   which is how this chip writes to its own flash, and Wire.begin(SDA, SCL),
   which takes pin numbers here and takes none on an Uno or a Pico. Everything
   else, iris.h included, runs anywhere -- boilerplate/any_sensor is the version
   that builds on all three, and it is the one to start from on another board.
   ========================================================================= */
#include <Wire.h>
#include <Adafruit_BNO055.h>
#include <Preferences.h>        /* the ESP32's own key-value store in flash */
#include "iris.h"
#if IRIS_VERSION_MAJOR != 0 || IRIS_VERSION_MINOR != 2
#error "This sketch is written for iris 0.2. Copy iris.h from iris 0.2 (https://github.com/kylebsmith/iris) into this sketch's folder, next to the .ino file, replacing the copy there."
#endif

/* SERIAL MONITOR SETTING. On an ESP32-S3 whose USB (Universal Serial Bus)
   socket is the chip's own USB port, as on the ES3C28P, Serial reaches the
   computer only with USB CDC On Boot enabled (CDC, Communications Device
   Class, is the USB standard for a serial port). ARDUINO_USB_MODE exists only
   on chips with that port, so every other board builds untouched. Either USB
   Mode works: this sketch uses nothing from the USB-OTG (On-The-Go) mode's
   TinyUSB software. */
#if defined(ARDUINO_ARCH_ESP32) && defined(ARDUINO_USB_MODE) && !ARDUINO_USB_CDC_ON_BOOT
#error "Set Tools -> USB CDC On Boot -> Enabled. The board's USB socket is the chip's own USB port; with this setting off, Serial prints to pins 43 and 44 instead and Serial Monitor stays empty."
#endif

/* ---- 1. THE SHAPE ------------------------------------------------------- */
#define N_INPUTS   2        /* two axes of tilt. Use 3 to add the third. */
#define N_OUTPUTS  3
#define N_DEMOS   16

#define SAVE_BTN  0
/* Not conditional, unlike the two portable sketches: this file cannot build
   anywhere but an ESP32 (Preferences.h, above), and 4095 is that chip's. */
#define ADC_MAX   4095.0f
#define SDA_PIN   16        /* your board's I2C pins; on many boards Wire.begin() */
#define SCL_PIN   15        /* with no arguments is already correct */

/* THE KNOBS. On the ES3C28P they go on the expansion socket, a 1.25 mm
   four-pin socket carrying GPIO 2, 3, 14 and 21 (vendor specification,
   ES3C28P/ES3N28P Specification V1.0, pages 9 and 12). GPIO 2 and 3 are on
   the chip's first analog-to-digital converter and GPIO 14 on its second;
   GPIO 21 cannot read a voltage. The socket carries no power, so each knob's
   outer legs take 3.3 V and ground from elsewhere (see PARTS.md).
   CHECK ON THE BOARD: that the socket's pins are in this order and that
   GPIO 14 reads a knob's full travel (the second converter is not used by
   anything else in these sketches). GPIO 4, 5 and 6 are the board's audio
   lines and reach no connector. */
static const int POT_PIN[] = { 2, 3, 14 };
static Adafruit_BNO055 bno = Adafruit_BNO055(55, 0x28, &Wire);
static Preferences store;

/* ---- 2. READ THE SENSOR -------------------------------------------------
   Gravity, not orientation in degrees. Euler angles wrap from +180 to -180,
   so two poses a degree apart arrive at opposite ends of the range and the
   sound falls off a cliff there. Gravity points down and never wraps.
   No scaling: raw metres per second squared is exactly what iris wants. */
static void read_sensor(float *in) {
  imu::Vector<3> g = bno.getVector(Adafruit_BNO055::VECTOR_GRAVITY);
  in[0] = (float)g.x();
  in[1] = (float)g.y();
}

/* ---- 3. WHERE THE TARGET COMES FROM ------------------------------------- */
static void read_target(float *out) {
  for (int i = 0; i < N_OUTPUTS; ++i)
    out[i] = analogRead(POT_PIN[i]) / ADC_MAX;
}

/* ---- 4. SEND THE SOUND -------------------------------------------------- */
static void send_sound(const float *out) {
  for (int i = 0; i < N_OUTPUTS; ++i) { Serial.print(out[i], 3); Serial.print(' '); }
  Serial.println();
}

/* ========================================================================= */
typedef char POT_PIN_must_have_exactly_N_OUTPUTS_entries
             [(sizeof POT_PIN / sizeof POT_PIN[0]) == N_OUTPUTS ? 1 : -1];

static unsigned char memory[IRIS_ARENA(N_INPUTS, 12, N_OUTPUTS, N_DEMOS)];
/* Sized from the arena, not from a guess: a saved instrument is smaller than
   the memory it lives in, so this follows N_INPUTS and N_DEMOS if you change
   them. keep_instrument checks iris_save's answer all the same. */
static unsigned char saved[sizeof memory];
static iris *k;

/* Training in slices, so the instrument never goes deaf. iris_train() blocks
   for seconds at eight or more demonstrations on this board (device_torture
   test 5 measures it), and an instrument that stops responding for seconds
   after every take is not an instrument. iris_train_begin + iris_train_slice
   do the same fit in pieces and end bit-identical to iris_train.
   See boilerplate/any_sensor for the same pattern. */
static bool training = false;

static void start_training(void) {
  if (!iris_train_begin(k, 0)) { Serial.println(F("TRAINING REFUSED.")); return; }
  training = true;
  Serial.println(F("learning -- keep moving it, it stays alive."));
}

static void finish_report(void) {
  training = false;
  if (iris_is_trained(k)) Serial.println(F("trained."));
  else { Serial.print(F("TRAINING FAILED -- status "));
         Serial.println((int)iris_get_status(k)); }
}

static void keep_training(void) {
  if (!training) return;
  iris_train_slice(k, 64);
  if (iris_train_busy(k)) return;
  finish_report();
}

void setup() {
  Serial.begin(115200);
  delay(400);
  pinMode(SAVE_BTN, INPUT_PULLUP);

  Wire.begin(SDA_PIN, SCL_PIN);
  /* Try the other address before giving up. The ADR (address-select) pad
     on the back of the Adafruit board moves it from 0x28 to 0x29, boards ship
     both ways, and the failure message below names both -- so it has to try
     both, or it sends someone to reseat a cable that was never the problem. */
  if (!bno.begin()) {
    bno = Adafruit_BNO055(55, 0x29, &Wire);
  }
  if (!bno.begin()) {
    /* Say what is actually on the bus. 0x38 (touch) and 0x18 (audio codec)
       are the board's own chips and answer whatever the sensor does. */
    Serial.println(F("No BNO055. Devices answering on I2C:"));
    for (uint8_t a = 8; a < 120; ++a) {
      Wire.beginTransmission(a);
      if (Wire.endTransmission() == 0) { Serial.print(F("  0x")); Serial.println(a, HEX); }
    }
    Serial.println(F("Expected 0x28 or 0x29 (0x18 and 0x38 are the board's own chips)."));
    Serial.println(F("Check the sensor's four wires against PARTS.md and press RESET."));
    for (;;) delay(1000);
  }

  k = iris_init(memory, sizeof memory, N_INPUTS, 12, N_OUTPUTS, N_DEMOS, 1234);
  if (!k) { Serial.println(F("iris_init refused -- the arena is too small")); for(;;); }

  /* Bring back the instrument from last time, if there is one. iris_save_size
     tells you exactly how many bytes to reserve, and iris_load refuses
     anything corrupt rather than playing weights it does not trust. */
  store.begin("iris", false);
  { size_t n = store.getBytesLength("inst");
    if (n > 0 && n <= sizeof saved) {
      store.getBytes("inst", saved, n);
      if (iris_load(k, saved, n))
        Serial.println(F("loaded the instrument from last time."));
      else
        Serial.println(F("the saved instrument is damaged; starting fresh."));
    } }

  Serial.println(F("ready. tilt the board, set the knobs, tap SAVE to record."));
  Serial.println(F("hold SAVE for a second to keep the instrument through a power cycle."));

  /* A saved instrument that was not trained still holds its demonstrations:
     train it now, so it plays them rather than one constant. */
  if (!iris_is_trained(k) && iris_count(k) >= 2) start_training();
}

/* A tap: store the demonstration read when the button went down. */
static void record_take(const float *in, const float *out) {
  /* Ask why it refused. There are three reasons and they need three
     different fixes; printing "full" for all of them sends a student to
     delete demonstrations they may not even have. */
  if (!iris_record(k, in, out)) {
    if (iris_get_status(k) == IRIS_STORE_FULL)
      Serial.println(F("full -- no room for more demonstrations"));
    else
      Serial.println(F("refused: a reading or a target is not a number. "
                       "Check the sensor cable and check read_target."));
  }
  else if (iris_count(k) >= 2) start_training();
  Serial.print(F("demonstrations: ")); Serial.println(iris_count(k));
}

/* A hold: save the instrument being played. A run still in progress is
   finished first, so the file holds the trained network, never a half-trained
   one. Nothing is recorded, so nothing is retrained. */
static void keep_instrument(void) {
  if (iris_count(k) < 1) { Serial.println(F("nothing to keep yet -- tap SAVE to record first.")); return; }
  if (training) {
    Serial.println(F("finishing training before saving..."));
    while (iris_train_slice(k, 256)) { }
    finish_report();
  }
  size_t n = iris_save(k, saved, sizeof saved);
  if (!n) { Serial.println(F("save refused -- iris_save returned 0.")); return; }
  store.putBytes("inst", saved, n);
  Serial.print(F("kept. ")); Serial.print((unsigned)n);
  Serial.print(F(" bytes in flash, "));
  Serial.print(iris_count(k));
  Serial.print(iris_count(k) == 1 ? F(" demonstration, ") : F(" demonstrations, "));
  if (iris_is_trained(k))      Serial.println(F("trained."));
  else if (iris_count(k) >= 2) Serial.println(F("not trained -- it retrains after power-up."));
  else                         Serial.println(F("not trained -- tap SAVE at a second pose to train it."));
}

#define HOLD_MS 1000

void loop() {
  float in[N_INPUTS], out[N_OUTPUTS];
  read_sensor(in);

  if (digitalRead(SAVE_BTN) == LOW) {
    /* Read the demonstration now, while the pose and knobs are where the
       student set them, then wait to see whether this is a tap or a hold. */
    float take_in[N_INPUTS], take_out[N_OUTPUTS];
    for (int i = 0; i < N_INPUTS; ++i) take_in[i] = in[i];
    read_target(take_out);
    uint32_t pressed = millis();
    while (digitalRead(SAVE_BTN) == LOW && millis() - pressed < HOLD_MS) delay(10);
    if (digitalRead(SAVE_BTN) == LOW) {
      keep_instrument();
      while (digitalRead(SAVE_BTN) == LOW) delay(10);   /* wait for the release */
    } else {
      record_take(take_in, take_out);
    }
    delay(50);                                          /* debounce the release */
  }

  keep_training();   /* a slice per pass, free when idle */

  if (iris_count(k) >= 2) { iris_predict(k, in, out); send_sound(out); }
  delay(20);
}
