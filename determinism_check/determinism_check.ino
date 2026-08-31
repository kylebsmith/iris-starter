/* ON-DEVICE DETERMINISM CHECK
   ===========================
   The library's central promise is that the same seed and the same
   demonstrations produce the same instrument -- so a mapping you save today
   plays the same in ten years. That promise has been verified on a laptop
   across nine compiler and optimisation settings.

   It has NEVER been verified on the chip it is actually for. The README says
   so. This sketch is how that stops being true.

   It runs the same fixed recipe the desktop test runs, hashes every weight,
   and prints the result. If the number below matches the host's, the promise
   holds on this hardware. If it does not, it does not, and we would rather
   know.

   BOARD SETTINGS: as in GET-STARTED.md. No sensor needed -- the data is fixed.
   ========================================================================= */
#include "iris.h"

/* The two settings that make Serial Monitor stay empty for ever. This is the
   sketch you were sent to when nothing else worked, so it is the last one that
   should reproduce the fault it is meant to diagnose.
   Wrapped in ARDUINO_ARCH_ESP32 on purpose: on AVR and RP2040 these macros do
   not exist, and an unwrapped `#if !ARDUINO_USB_CDC_ON_BOOT` reads undefined
   as 0 and fires the error on every board that never had the problem. */
#ifdef ARDUINO_ARCH_ESP32
#if ARDUINO_USB_MODE
#error "Wrong USB Mode. Set Tools -> USB Mode -> 'USB-OTG (TinyUSB)'. Every sketch in this repo uses that one setting."
#endif
#if !ARDUINO_USB_CDC_ON_BOOT
#error "Set Tools -> USB CDC On Boot -> 'Enabled', or Serial Monitor will stay empty forever and nothing will tell you why."
#endif
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
  for (int i = 28; i >= 0; i -= 4) {
    int d = (int)((v >> i) & 0xF);
    Serial.print((char)(d < 10 ? '0' + d : 'A' + d - 10));
  }
  Serial.println();
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
  iris_retrain_new(k, 1234, 800);          /* fixed seed, fixed epoch count */
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

/* Print from loop(), not setup(). A result printed before anyone opens the
   serial port is a result nobody sees -- which is how the first run of this
   sketch reported nothing at all. */
void loop() {
  if (!g_ok) { Serial.println("iris_init refused"); delay(2000); return; }
  Serial.println();
  Serial.println("=== iris on-device determinism check ===");
  /* Serial.print, not Serial.printf: printf on Serial is an Espressif
     extension and does not exist on AVR, SAMD or RP2040. This sketch exists
     to compare a hash on YOUR board against the one on a laptop, so it is
     the last sketch in the repository that should refuse to build. */
  Serial.print(F("  library version   ")); Serial.println(IRIS_VERSION_STRING);
  Serial.println(F("  training          800 epochs, 20 demonstrations, seed 1234"));
  Serial.print(F("  took              ")); Serial.print(g_ms); Serial.println(F(" ms"));
  Serial.print(F("  saved file        ")); Serial.print((unsigned)g_n); Serial.println(F(" bytes"));
  Serial.print(F("  PREDICTION HASH   0x")); print_hex8(g_hash);
  Serial.print(F("  saved-bytes hash  0x")); print_hex8(g_file_hash);
  Serial.println();
  Serial.println("  Compare PREDICTION HASH against the same recipe on a host.");
  Serial.println("  Equal means the promise holds on this chip.");
  delay(3000);
}
