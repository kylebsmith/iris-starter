/* ON-DEVICE DETERMINISM CHECK
   ===========================
   The library's central promise is that the same seed and the same
   demonstrations produce the same instrument, bit for bit, so a mapping you
   save plays the same on every board that passes this check.

   This sketch runs one fixed recipe, hashes what the trained instrument
   plays and the file it saves, and compares both numbers with the values a
   laptop computes for the same recipe. It prints PASS when the chip matches
   the laptop and FAIL when it does not. A FAIL is a real result about the
   chip or its compiler: keep the whole printout.

   The recipe: iris_init with seed 1234, 20 fixed demonstrations, then
   iris_reseed(k, 1234) and iris_continue(k, 800) -- exactly 800 training
   passes from the seed's starting weights. (iris_train would stop wherever
   the error levels off; a fixed count keeps the recipe identical to the one
   the library's own test, tests/starter_recipes.c, pins.)

   BOARD SETTINGS: as in GET-STARTED.md. No sensor needed -- the data is fixed.
   ========================================================================= */
#include "iris.h"
#if IRIS_VERSION_MAJOR != 0 || IRIS_VERSION_MINOR != 2
#error "This sketch is written for iris 0.2. Copy iris.h from iris 0.2 (https://github.com/kylebsmith/iris) into this sketch's folder, next to the .ino file, replacing the copy there."
#endif

/* No fused multiply-add in this file. The demonstrations below are computed
   here, and 0.25f + 0.5f * u is a multiply and an add, which GCC (the
   compiler the ESP32 board package uses) fuses into one instruction by
   default, rounding once instead of twice. A fused result can differ in the
   last bit, and then the chip trains on different numbers from the laptop.
   iris.h switches fusing off for its own code only; this switches it off for
   the rest of this file, so the recipe stays comparable whatever you change
   in it. */
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("fp-contract=off")
#elif defined(__clang__)
#pragma STDC FP_CONTRACT OFF
#endif

/* The laptop's values for this recipe with iris 0.2.0. PREDICTION_HASH is
   pinned in the library's tests/starter_recipes.c. FILE_HASH is the same
   recipe's saved file (format 7, 872 bytes), computed by a laptop build of
   this sketch. */
#define HOST_PREDICTION_HASH 0x203834EDu
#define HOST_FILE_HASH       0xD8666A69u
#define HOST_FILE_BYTES      872u

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


#define NI 2
#define NH 12
#define NO 3
#define CAP 64

static unsigned char arena[IRIS_ARENA(NI, NH, NO, CAP)];

/* The same target the desktop check uses. */
static void truth(float u, float v, float *o) {
  o[0] = 0.25f + 0.5f * u;
  o[1] = 0.25f + 0.5f * v;
  o[2] = 0.25f + 0.5f * (u * v);
}

static uint32_t fnv1a(const unsigned char *p, size_t n) {
  uint32_t h = 2166136261u;
  for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 16777619u; }
  return h;
}

static uint32_t g_hash, g_file_hash, g_ms; static size_t g_n; static int g_ok;

/* Eight hex digits with the leading zeros kept. Serial.print(x, HEX)
   drops them, and a hash printed as 0x203834ED on one board and 0x3834ED
   on another looks like a determinism failure when it is a printing one. */
static void print_hex8(uint32_t v) {
  Serial.print(F("0x"));
  for (int i = 28; i >= 0; i -= 4) {
    int d = (int)((v >> i) & 0xF);
    Serial.print((char)(d < 10 ? '0' + d : 'A' + d - 10));
  }
}

/* One labelled line: PASS or FAIL, what it measured, what the laptop got. */
static int check(const __FlashStringHelper *name, uint32_t got, uint32_t want) {
  Serial.print(got == want ? F("  PASS  ") : F("  FAIL  "));
  Serial.print(name); print_hex8(got);
  Serial.print(F("   laptop ")); print_hex8(want);
  Serial.println();
  return got == want;
}

void setup() {
  Serial.begin(115200);
  delay(1500);

  iris *k = iris_init(arena, sizeof arena, NI, NH, NO, CAP, 1234);
  if (!k) { Serial.println("iris_init refused"); return; }

  for (int i = 0; i < 20; ++i) {
    float u = (float)((i * 7919) % 97) / 97.0f;
    float v = (float)((i * 6131) % 89) / 89.0f;
    float in[NI] = { u, v }, out[NO];
    truth(u, v, out);
    iris_record(k, in, out);
  }

  uint32_t t0 = millis();
  iris_reseed(k, 1234);                    /* the seed's starting weights */
  iris_continue(k, 800);                   /* exactly 800 passes from them */
  uint32_t ms = millis() - t0;

  /* Hash every prediction over a fixed grid: this is the instrument's
     behaviour, not its file layout, so it cannot move for bookkeeping. */
  uint32_t h = 2166136261u;
  for (int a = 0; a <= 20; ++a) {
    float in[NI] = { a / 20.0f, 0.5f }, o[NO];
    iris_predict(k, in, o);
    for (int q = 0; q < NO; ++q) {
      uint32_t b; memcpy(&b, &o[q], 4);
      for (int y = 0; y < 4; ++y) { h ^= (b >> (8 * y)) & 0xFF; h *= 16777619u; }
    }
  }

  static unsigned char file[8192];
  size_t n = iris_save(k, file, sizeof file);

  g_hash = h; g_file_hash = fnv1a(file, n); g_ms = ms; g_n = n; g_ok = 1;
}

/* Print from loop(), not setup(), and repeat every few seconds: a result
   printed before anyone opens the serial port is a result nobody sees. */
void loop() {
  if (!g_ok) { Serial.println(F("iris_init refused")); delay(2000); return; }
  Serial.println();
  Serial.println(F("=== iris on-device determinism check ==="));
  /* Serial.print, not Serial.printf: printf on Serial exists only on
     Espressif boards, and this sketch is meant to build on any board. */
  Serial.print(F("  library version   ")); Serial.println(IRIS_VERSION_STRING);
  Serial.println(F("  recipe            seed 1234, 20 demonstrations, iris_reseed + iris_continue 800"));
  Serial.print(F("  training took     ")); Serial.print(g_ms); Serial.println(F(" ms"));
  Serial.print(F("  saved file        ")); Serial.print((unsigned)g_n);
  Serial.print(F(" bytes   laptop ")); Serial.println(HOST_FILE_BYTES);
  int ok = check(F("prediction hash   "), g_hash, HOST_PREDICTION_HASH);
  ok &= check(F("saved-bytes hash  "), g_file_hash, HOST_FILE_HASH);
  ok &= (g_n == HOST_FILE_BYTES);
  Serial.println(ok ? F("  RESULT: PASS -- this chip builds the same instrument as the laptop, bit for bit.")
                    : F("  RESULT: FAIL -- this chip differs from the laptop. Keep this printout."));
  delay(3000);
}
