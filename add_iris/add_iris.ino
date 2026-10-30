/* ADD IRIS TO A PROJECT YOU ALREADY HAVE
   ======================================
   You have a sketch that reads something and makes a sound. Putting a learned
   mapping between the two is six lines. They are marked [1] to [6] below.

   Everything else here is one Arduino sketch's worth of scaffolding, so that
   this file compiles and plays on a bare board with nothing wired to it.
   Replace read_sensor with your sensor and send_sound with your synthesis,
   set the sound however you like (TARGETS.md has knobs, a touchscreen, key
   switches and MIDI), and move the six lines into the setup and loop you
   already have.

   It plays from the first flash: your target source has the controls until
   you have shown it two demonstrations, and iris takes them after that.
   Press SAVE -- the BOOT button on most boards -- to record one.

   boilerplate/any_sensor is this same instrument with every guard, every
   start-up check and every explanation a first instrument deserves, and it
   fits in slices so the sound never stops. Start there to learn it. Start
   here if you have a project and just want the mapping in it.
   ========================================================================= */
#include "iris.h"                                                  /* [1] */
#if IRIS_VERSION_MAJOR != 0 || IRIS_VERSION_MINOR != 3
#error "This sketch is written for iris 0.3. Copy iris.h from iris 0.3 (https://github.com/kylebsmith/iris) into this sketch's folder, next to the .ino file, replacing the copy there."
#endif
#if defined(ARDUINO_ARCH_ESP32) && defined(ARDUINO_USB_MODE) && !ARDUINO_USB_CDC_ON_BOOT
#error "Set Tools -> USB CDC On Boot -> Enabled. With this setting off, Serial prints to pins 43 and 44 instead and Serial Monitor stays empty."
#endif

#define N_INPUTS   2        /* numbers your sensor gives you at once */
#define N_OUTPUTS  3        /* things you want to control */
#define N_DEMOS   16        /* demonstrations it can hold */
#define SAVE_BTN   0        /* the BOOT button on most boards */

static unsigned char memory[IRIS_ARENA(N_INPUTS, 12, N_OUTPUTS, N_DEMOS)]; /* [2] */
static iris *k;

/* YOUR SENSOR. Any units at all -- iris fits its range to what you give it,
   so do NOT scale or centre. Two slow waves stand in until you do. */
static void read_sensor(float *in) {
  in[0] = (float)(millis() % 7000u);
  in[1] = (float)(millis() % 11000u);
}

/* YOUR TARGET: the sound you are demonstrating, each number 0 to 1. This one
   needs no hardware -- 1 2 3 pick an output, + and - move it. It is called
   once a pass, not only at the SAVE press, because it holds its values and
   has to be polled to notice a key. TARGETS.md has four more. */
static void read_target(float *out) {
  static float held[N_OUTPUTS];
  static bool started = false;
  static int sel = 0;
  if (!started) {
    for (int i = 0; i < N_OUTPUTS; ++i) held[i] = 0.5f;
    started = true;
  }
  while (Serial.available()) {
    const int c = Serial.read();
    if (c >= '1' && c <= '9' && c - '1' < N_OUTPUTS) sel = c - '1';
    else if (c == '+' || c == '=')                   held[sel] += 0.05f;
    else if (c == '-')                               held[sel] -= 0.05f;
    else continue;
    if (held[sel] > 1.0f) held[sel] = 1.0f;
    if (held[sel] < 0.0f) held[sel] = 0.0f;
    Serial.print(F("target ")); Serial.print(sel + 1);
    Serial.print(F(" = "));      Serial.println(held[sel], 3);
  }
  for (int i = 0; i < N_OUTPUTS; ++i) out[i] = held[i];
}

/* YOUR SOUND. */
static void send_sound(const float *out) {
  for (int i = 0; i < N_OUTPUTS; ++i) { Serial.print(out[i], 3); Serial.print(' '); }
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 20000) delay(10);   /* native USB: wait for it */
  pinMode(SAVE_BTN, INPUT_PULLUP);

  k = iris_init(memory, sizeof memory, N_INPUTS, 12, N_OUTPUTS, N_DEMOS, 1234); /* [3] */
  if (!k) {                      /* the shape is out of range, or memory is too small */
    const size_t need = iris_size(N_INPUTS, 12, N_OUTPUTS, N_DEMOS);
    if (!need) Serial.println(F("iris_init refused: the SHAPE is out of range."));
    else {
      Serial.print(F("iris_init refused: memory is too small. need "));
      Serial.print((unsigned long)need); Serial.print(F(", have "));
      Serial.println((unsigned long)sizeof memory);
    }
    for (;;);                    /* never play an instrument that did not start */
  }
  Serial.println(F("playing. 1 2 3 pick an output, + and - move it."));
  Serial.println(F("press SAVE to demonstrate. twice, and iris takes over."));
}

void loop() {
  float in[N_INPUTS], out[N_OUTPUTS];
  read_sensor(in);
  read_target(out);

  if (digitalRead(SAVE_BTN) == LOW) {
    if (!iris_record(k, in, out))                                  /* [4] */
      Serial.println(F("refused: full, or a reading or target is not a number."));
    else if (iris_count(k) >= 2 && !iris_train(k))                 /* [5] */
      Serial.println(F("TRAINING FAILED. it is NOT fitted; do not trust it."));
    /* iris_train does the whole fit in one call and does not return until it
       is done -- seconds, silent, at eight or more demonstrations. See
       any_sensor for iris_train_begin/iris_train_slice, which fit in pieces
       so the instrument keeps playing while it learns. */
    Serial.print(F("demonstrations: ")); Serial.println(iris_count(k));
    delay(300);                                    /* crude, but it debounces */
  }

  if (iris_count(k) >= 2) iris_predict(k, in, out);                /* [6] */
  send_sound(out);                 /* below two, you hear read_target itself */
  delay(20);
}
