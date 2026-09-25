/* BOARD PROBE -- how fast iris runs on this board, measured properly
   ==================================================================
   One sketch that measures what the documents quote about speed, prints
   every figure with its unit, and ends with one block you can paste into a
   lab notebook or a research record whole. It needs NO SENSOR and NO WIRING.

   What it measures, in order:

     0  the board: chip, clock, compiler, iris version, and whether the
        floating-point unit flushes very small numbers (subnormals) to zero
     1  three pinned recipes, compared bit for bit with the laptop's values:
        the library's golden recipe (tests/audit.c), device_torture test 1
        and determinism_check. PASS or FAIL each.
     2  the time of ONE prediction, iris_predict, at three shapes (2 inputs,
        12 hidden units, 3 outputs; 6/16/8; 12/32/8):
          - with the processor's cycle counter, which counts every clock tick
            (4.2 ns at 240 MHz), not micros(), which counts whole
            microseconds;
          - an identical loop without the call is timed too and subtracted,
            so the loop's own cost is not in the figure;
          - 101 batches of 200 calls, reported as minimum, median and
            maximum per call;
          - one batch with interrupts switched off on this core, which
            shows the code's own cost without the operating system's;
          - 20,000 single calls, reported as percentiles: an audio deadline
            is missed by the slowest call, not by the average one.
     3  training time for each trainer on representative data (the
        library's reference task: scattered points, three smooth outputs),
        not on device_torture's recipe, whose inputs move together.

   Runtime: one to three minutes. Every figure is also printed as a line
   starting "R," -- R,section,name,value,unit -- so a log can be searched.
   Run it twice on each board and keep the whole log.

   BOARD SETTINGS: as in GET-STARTED.md. It builds in either USB (Universal
   Serial Bus) Mode.
   ========================================================================= */
#include "iris.h"
#if IRIS_VERSION_MAJOR != 0 || IRIS_VERSION_MINOR != 2
#error "This sketch is written for iris 0.2. Copy iris.h from iris 0.2 (https://github.com/kylebsmith/iris) into this sketch's folder, next to the .ino file, replacing the copy there."
#endif

/* NO FUSED MULTIPLY-ADD IN THIS FILE. Section 1 computes the pinned recipes'
   demonstrations here, and GCC (the compiler the ESP32 board package uses)
   fuses a multiply and an add into one instruction by default, which rounds
   once instead of twice and can move the last bit. iris.h switches fusing
   off for its own code only; this switches it off for the rest of this
   file, as the laptop's tests do. */
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("fp-contract=off")
#elif defined(__clang__)
#pragma STDC FP_CONTRACT OFF
#endif

/* BOARD SETTINGS. board_probe times the ESP32-S3 with its cycle counter, so
   it needs the ESP32-S3 board entry; Serial reaches the computer through the
   chip's own USB port only with USB CDC On Boot enabled (CDC: Communications
   Device Class, the USB serial-port standard). Either USB Mode works: this
   sketch uses nothing from the USB-OTG mode's TinyUSB software. */
#if !defined(ARDUINO_ARCH_ESP32) || !defined(CONFIG_IDF_TARGET_ESP32S3)
#error "board_probe times an ESP32-S3 with its cycle counter. Set Tools -> Board -> esp32 -> ESP32S3 Dev Module, then set the board options in GET-STARTED.md."
#endif
#if defined(CONFIG_IDF_TARGET_ESP32S3) && !ARDUINO_USB_CDC_ON_BOOT
#error "Set Tools -> USB CDC On Boot -> Enabled. The board's USB socket is the chip's own USB port; with this setting off, Serial prints to pins 43 and 44 instead and Serial Monitor stays empty."
#endif

#include <esp_cpu.h>
#include <esp_idf_version.h>
#include <esp_timer.h>

static volatile float sink = 0.0f;
static inline uint32_t cyc(void) { return (uint32_t)esp_cpu_get_cycle_count(); }
static int hash_pass = 0, hash_fail = 0;

/* ---- the record: every figure goes to Serial as it is measured, and into
   this buffer, which is printed again as one block at the end ------------ */
static char record[12288];
static size_t rec_len = 0;
static void rec(const char *s) {
  Serial.print(s);
  size_t n = strlen(s);
  if (rec_len + n < sizeof record) { memcpy(record + rec_len, s, n); rec_len += n; record[rec_len] = 0; }
}
static void R(const char *sec, const char *name, double v, const char *unit, int digits) {
  char b[160]; snprintf(b, sizeof b, "R,%s,%s,%.*f,%s\n", sec, name, digits, v, unit); rec(b);
}
static void RU(const char *sec, const char *name, uint32_t v, const char *unit) {
  char b[160]; snprintf(b, sizeof b, "R,%s,%s,%lu,%s\n", sec, name, (unsigned long)v, unit); rec(b);
}
static void RS(const char *sec, const char *name, const char *v) {
  char b[200]; snprintf(b, sizeof b, "R,%s,%s,%s,text\n", sec, name, v); rec(b);
}
static void RH(const char *name, uint32_t h, uint32_t want) {
  char b[160];
  snprintf(b, sizeof b, "R,hash,%s,0x%08lX,%s laptop 0x%08lX\n", name, (unsigned long)h,
           h == want ? "PASS ==" : "FAIL !=", (unsigned long)want);
  rec(b);
  if (h == want) hash_pass++; else hash_fail++;
}
static int cmpu(const void *a, const void *b) {
  uint32_t x = *(const uint32_t *)a, y = *(const uint32_t *)b; return (x > y) - (x < y);
}
static uint32_t fnv(const unsigned char *p, size_t n, uint32_t h) {
  for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 16777619u; }
  return h;
}
static void drain(void) { Serial.flush(); delay(100); }   /* let USB go quiet */

/* ======================= 0. the board ==================================== */
static void environment(void) {
  Serial.println(F("\n== 0. the board =="));
  RS("env", "chip", ESP.getChipModel());
  RU("env", "chip_revision", (uint32_t)ESP.getChipRevision(), "revision");
  RU("env", "cpu_clock", getCpuFrequencyMhz(), "MHz");
  RU("env", "core", (uint32_t)xPortGetCoreID(), "core number");
  RU("env", "flash_speed", ESP.getFlashChipSpeed(), "Hz");
  RS("env", "esp_idf", esp_get_idf_version());
  RS("env", "arduino_esp32", ESP_ARDUINO_VERSION_STR);
  RS("env", "compiler", __VERSION__);
#if defined(__OPTIMIZE_SIZE__)
  RS("env", "optimisation", "-Os");
#elif defined(__OPTIMIZE__)
  RS("env", "optimisation", "-O1 or higher, not -Os");
#else
  RS("env", "optimisation", "-O0");
#endif
  RS("env", "usb_mode", ARDUINO_USB_MODE ? "Hardware CDC and JTAG" : "USB-OTG (TinyUSB)");
  RS("env", "iris", IRIS_VERSION_STRING);
  { char b[40]; snprintf(b, sizeof b, "%s %s", __DATE__, __TIME__); RS("env", "built", b); }
  { char b[24]; snprintf(b, sizeof b, "%012llX", (unsigned long long)ESP.getEfuseMac()); RS("env", "board_id_mac", b); }
  /* Subnormals: numbers too small for the float format's normal range. The
     bits of each result are printed; 0 means the hardware flushed it. */
  volatile float tiny = 1.0e-38f, scale = 1.0e-3f, sub = 1.0e-40f, one = 1.0f;
  float p = tiny * scale, q = sub * one;
  uint32_t pb, qb; memcpy(&pb, &p, 4); memcpy(&qb, &q, 4);
  RU("env", "subnormal_product_bits", pb, "bits as a number (0 = flushed to zero)");
  RU("env", "subnormal_operand_bits", qb, "bits as a number (0 = flushed to zero)");
}

/* ======================= 1. the pinned recipes =========================== */
/* The library's golden recipe (tests/audit.c, check 12): its reference task,
   20 demonstrations, seed 1234, iris_reseed and iris_continue(k, 800), then
   the saved file reduced to the bytes that describe the instrument
   (instrument_bytes, copied from tests/audit.c) and hashed. */
static void truth(float x, float y, float *o) {
  o[0] = 0.5f + 0.45f * iris_internal_tanh(3.0f * (x - 0.5f));
  o[1] = 0.5f + 0.40f * iris_internal_tanh(2.5f * (y - 0.5f) * (x + 0.3f));
  o[2] = 0.2f + 0.6f  * (x * y);
}
static void load_reference(iris *k, int n) {
  iris_clear(k);
  for (int i = 0; i < n; ++i) {
    float u = (float)((i * 7919) % 97) / 97.0f;
    float v = (float)((i * 6131) % 89) / 89.0f;
    float in[2] = { u, v }, out[3];
    truth(u, v, out);
    iris_record(k, in, out);
  }
}
static size_t instrument_bytes(unsigned char *buf, size_t n) {
  static const unsigned char prefix[8] = { 'E', 'W', 'E', 'K', 1, 0, 0, 0 };
  if (n < 52) return 0;
  memmove(buf + 8, buf + 16, 24);
  memmove(buf + 32, buf + 48, n - 52);
  memcpy(buf, prefix, sizeof prefix);
  return 32 + (n - 52);
}
static unsigned char arena256[IRIS_ARENA(2, 12, 3, 256)];
static unsigned char file[16 * 1024];

/* device_torture test 1 */
static unsigned char arenaA[IRIS_ARENA(2, 12, 3, 20)];
static iris *build_torture(void) {
  iris *k = iris_init(arenaA, sizeof arenaA, 2, 12, 3, 20, 1234u);
  if (!k) return 0;
  for (int i = 0; i < 20; ++i) {
    float in[2], out[3];
    in[0] = (float)i * 0.05f; in[1] = 1.0f - (float)i * 0.03f;
    out[0] = (float)((i * 7) % 11) / 11.0f;
    out[1] = (float)((i * 3) % 5) / 5.0f;
    out[2] = (float)((i * 5) % 7) / 7.0f;
    iris_record(k, in, out);
  }
  iris_continue(k, 800);
  return k;
}
static uint32_t hash_torture(iris *k) {
  uint32_t h = 2166136261u;
  for (int i = 0; i <= 20; ++i) {
    float in[2], out[3];
    in[0] = (float)i * 0.05f; in[1] = 1.0f - (float)i * 0.03f;
    iris_predict(k, in, out);
    h = fnv((const unsigned char *)out, sizeof out, h);
  }
  return h;
}

/* determinism_check */
static unsigned char arena64[IRIS_ARENA(2, 12, 3, 64)];
static uint32_t determinism_check_hash(void) {
  iris *k = iris_init(arena64, sizeof arena64, 2, 12, 3, 64, 1234);
  if (!k) return 0;
  for (int i = 0; i < 20; ++i) {
    float u = (float)((i * 7919) % 97) / 97.0f, v = (float)((i * 6131) % 89) / 89.0f;
    float in[2] = { u, v }, out[3] = { 0.25f + 0.5f * u, 0.25f + 0.5f * v, 0.25f + 0.5f * (u * v) };
    iris_record(k, in, out);
  }
  iris_reseed(k, 1234);
  iris_continue(k, 800);
  uint32_t h = 2166136261u;
  for (int a = 0; a <= 20; ++a) {
    float in[2] = { a / 20.0f, 0.5f }, o[3];
    iris_predict(k, in, o);
    for (int q = 0; q < 3; ++q) {
      uint32_t b; memcpy(&b, &o[q], 4);
      for (int y = 0; y < 4; ++y) { h ^= (b >> (8 * y)) & 0xFFu; h *= 16777619u; }
    }
  }
  return h;
}

static void golden(void) {
  Serial.println(F("\n== 1. the same bits as the laptop? =="));
  iris *kb = iris_init(arena256, sizeof arena256, 2, 12, 3, 256, 1234);
  if (kb) {
    load_reference(kb, 20);
    iris_reseed(kb, 1234);
    iris_continue(kb, 800);
    size_t n = iris_save(kb, file, sizeof file);
    size_t n1 = instrument_bytes(file, n);
    RH("golden_recipe_tests_audit_c", fnv(file, n1, 2166136261u), 0x6805FB0Du);
  } else RH("golden_recipe_tests_audit_c", 0u, 0x6805FB0Du);
  iris *k = build_torture();
  RH("device_torture_test_1", k ? hash_torture(k) : 0u, 0xB7FC47A0u);
  RH("determinism_check_predictions", determinism_check_hash(), 0x203834EDu);
}

/* ======================= 2. one prediction =============================== */
static unsigned char arenaP[IRIS_ARENA(12, 32, 8, 20)];
static float tab[256][12];
#define NB 101           /* batches */
#define M 200            /* calls per batch */
#define NCALL 20000      /* single-call samples */
static uint32_t batch[NB], empty[NB], single[NCALL];
static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

static float frand(uint32_t *s) { *s = *s * 1664525u + 1013904223u; return (float)(*s >> 8) / 16777216.0f; }

static void time_shape(int ni, int nh, int no) {
  char tag[24]; snprintf(tag, sizeof tag, "predict_%d_%d_%d", ni, nh, no);
  Serial.print(F("\n-- ")); Serial.println(tag);
  /* 20 random demonstrations, trained with iris_train, so the timed path is
     the fitted one a student plays. */
  iris *k = iris_init(arenaP, sizeof arenaP, ni, nh, no, 20, 1234u);
  if (!k) { Serial.println(F("  iris_init refused")); return; }
  uint32_t s = 7u;
  for (int i = 0; i < 20; ++i) {
    float in[12], out[8];
    for (int j = 0; j < ni; ++j) in[j] = frand(&s);
    for (int j = 0; j < no; ++j) out[j] = frand(&s);
    iris_record(k, in, out);
  }
  iris_train(k);
  RU(tag, "trained", (uint32_t)iris_is_trained(k), "1 = fitted (0 would time a shortcut)");
  RU(tag, "status", (uint32_t)iris_get_status(k), "code (0 = healthy)");
  { uint32_t r = 99u;                                 /* inputs inside the demonstrated range */
    for (int i = 0; i < 256; ++i) for (int j = 0; j < ni; ++j) tab[i][j] = frand(&r); }
  float out[8];
  const uint32_t mhz = getCpuFrequencyMhz();
  for (int w = 0; w < 1000; ++w) iris_predict(k, tab[w & 255], out);   /* warm the cache */
  drain();

  /* two counter reads back to back: the floor under every single-call sample */
  uint32_t floor_c = 0xFFFFFFFFu;
  for (int r = 0; r < 1000; ++r) { uint32_t a = cyc(), b = cyc(); if (b - a < floor_c) floor_c = b - a; }

  for (int b = 0; b < NB; ++b) {
    uint32_t t0 = cyc();
    for (int j = 0; j < M; ++j) { iris_predict(k, tab[j & 255], out); sink = sink + out[0]; }
    batch[b] = cyc() - t0;
    t0 = cyc();
    for (int j = 0; j < M; ++j) { sink = sink + tab[j & 255][0]; }
    empty[b] = cyc() - t0;
  }
  qsort(batch, NB, 4, cmpu); qsort(empty, NB, 4, cmpu);
  const double e = (double)empty[NB / 2] / M;
  R(tag, "batch_min_per_call", (double)batch[0] / M - e, "cycles", 1);
  R(tag, "batch_median_per_call", (double)batch[NB / 2] / M - e, "cycles", 1);
  R(tag, "batch_max_per_call", (double)batch[NB - 1] / M - e, "cycles", 1);
  R(tag, "empty_loop_per_call_subtracted", e, "cycles", 1);
  R(tag, "batch_median_per_call_us", ((double)batch[NB / 2] / M - e) / mhz, "us", 4);

  /* interrupts off on this core for one batch of about a millisecond, far
     under the interrupt watchdog: the code's own cost, no tick, no USB */
  { uint32_t t0, t1;
    taskENTER_CRITICAL(&mux);
    t0 = cyc();
    for (int j = 0; j < M; ++j) { iris_predict(k, tab[j & 255], out); sink = sink + out[0]; }
    t1 = cyc();
    taskEXIT_CRITICAL(&mux);
    R(tag, "interrupts_off_per_call", (double)(t1 - t0) / M - e, "cycles", 1); }

  /* single calls: the distribution an audio callback would see */
  for (int i = 0; i < NCALL; ++i) {
    uint32_t a = cyc();
    iris_predict(k, tab[i & 255], out);
    uint32_t b2 = cyc();
    sink = sink + out[0];
    single[i] = b2 - a - floor_c;
  }
  qsort(single, NCALL, 4, cmpu);
  RU(tag, "call_min", single[0], "cycles");
  RU(tag, "call_p50", single[NCALL / 2], "cycles");
  RU(tag, "call_p99", single[(NCALL * 99) / 100], "cycles");
  RU(tag, "call_p99_9", single[(NCALL * 999) / 1000], "cycles");
  RU(tag, "call_max", single[NCALL - 1], "cycles");
  R(tag, "call_p99_9_us", (double)single[(NCALL * 999) / 1000] / mhz, "us", 3);
  R(tag, "call_max_us", (double)single[NCALL - 1] / mhz, "us", 3);
}

/* ======================= 3. training ===================================== */
static unsigned char elm_scratch[IRIS_ELM_SCRATCH(12, 3)];

static void training(void) {
  Serial.println(F("\n== 3. training time, representative data =="));
  char n[48];
  const int cn[] = { 10, 20, 50 };
  /* iris_train: the call students use, run to its plateau */
  for (int e = 0; e < 3; ++e) {
    iris *k = iris_init(arena256, sizeof arena256, 2, 12, 3, 256, 4242u);
    load_reference(k, cn[e]);
    drain();
    int64_t t0 = esp_timer_get_time(); iris_train(k); int64_t dt = esp_timer_get_time() - t0;
    snprintf(n, sizeof n, "iris_train_%d_demos", cn[e]);
    R("train", n, dt / 1000.0, "ms", 1);
    snprintf(n, sizeof n, "iris_train_%d_demos_epochs", cn[e]);
    RU("train", n, (uint32_t)iris_train_epochs_done(k), "epochs");
    snprintf(n, sizeof n, "iris_train_%d_demos_trained", cn[e]);
    RU("train", n, (uint32_t)iris_is_trained(k), "1 = fitted");
  }
  /* iris_train_slice(k, 64): one slice, the pause a sliced sketch makes */
  { iris *k = iris_init(arena256, sizeof arena256, 2, 12, 3, 256, 4242u);
    load_reference(k, 20);
    static uint32_t sl[2048]; int ns = 0;
    drain();
    iris_train_begin(k, 0);
    for (;;) {
      int64_t t0 = esp_timer_get_time();
      int more = iris_train_slice(k, 64);
      int64_t dt = esp_timer_get_time() - t0;
      if (ns < 2048) sl[ns++] = (uint32_t)dt;
      if (!more) break;
    }
    qsort(sl, ns, 4, cmpu);
    RU("train", "slice_64_epochs_20_demos_median", sl[ns / 2], "us");
    RU("train", "slice_64_epochs_20_demos_max", sl[ns - 1], "us");
    RU("train", "slices_timed", (uint32_t)ns, "slices"); }
  /* iris_continue(k, 600): a fixed number of epochs from the seed's weights */
  for (int e = 0; e < 3; ++e) {
    iris *k = iris_init(arena256, sizeof arena256, 2, 12, 3, 256, 4242u);
    load_reference(k, cn[e]);
    iris_reseed(k, 4242u);
    drain();
    int64_t t0 = esp_timer_get_time(); iris_continue(k, 600); int64_t dt = esp_timer_get_time() - t0;
    snprintf(n, sizeof n, "iris_continue_600_%d_demos", cn[e]);
    R("train", n, dt / 1000.0, "ms", 1);
  }
  /* iris_train_elm: the closed-form solve, mean of 20 */
  for (int e = 1; e < 3; ++e) {
    iris *k = iris_init(arena256, sizeof arena256, 2, 12, 3, 256, 4242u);
    load_reference(k, cn[e]);
    drain();
    int64_t t0 = esp_timer_get_time();
    for (int r = 0; r < 20; ++r) iris_train_elm(k, 1e-4f, elm_scratch, sizeof elm_scratch);
    int64_t dt = esp_timer_get_time() - t0;
    snprintf(n, sizeof n, "iris_train_elm_%d_demos", cn[e]);
    R("train", n, dt / 20000.0, "ms (mean of 20)", 3);
    snprintf(n, sizeof n, "iris_train_elm_%d_demos_trained", cn[e]);
    RU("train", n, (uint32_t)iris_is_trained(k), "1 = fitted");
  }
}

void setup(void) {
  Serial.begin(115200);
  while (!Serial && millis() < 20000) delay(10);
  delay(200);
}

static int done = 0;
void loop(void) {
  if (done) {
    delay(15000);
    Serial.println(F("\n----- BEGIN BOARD PROBE RECORD -----"));
    Serial.print(record);
    Serial.println(F("----- END BOARD PROBE RECORD -----"));
    return;
  }
  done = 1;
  environment();
  golden();
  Serial.println(F("\n== 2. one prediction =="));
  time_shape(2, 12, 3);
  time_shape(6, 16, 8);
  time_shape(12, 32, 8);
  training();
  RU("env", "stack_never_used", (uint32_t)uxTaskGetStackHighWaterMark(NULL), "bytes");
  { char b[64]; snprintf(b, sizeof b, "%d PASS %d FAIL", hash_pass, hash_fail); RS("hash", "summary", b); }
  Serial.println(F("\n----- BEGIN BOARD PROBE RECORD -----"));
  Serial.print(record);
  Serial.println(F("----- END BOARD PROBE RECORD -----"));
  Serial.println(F("DONE. Copy everything between BEGIN and END. It prints again every 15 s."));
}
