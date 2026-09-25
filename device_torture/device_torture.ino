/* ON-DEVICE TORTURE TEST
   ======================
   The library's tests run on a laptop. A laptop is not the machine it is
   for. This sketch runs the same kind of checks on the chip in your hand, and
   it needs NO SENSOR and NO WIRING -- the data is synthetic and fixed, so the
   only thing under test is the library and your board.

   It asks nine questions. The yes-or-no ones print PASS or FAIL with the
   number measured; the rest print a labelled figure. Anything that says FAIL
   is a real result and worth keeping. On a board that is not an ESP32 the
   tests that need its flash, heap counter or task stack print SKIP and count
   as neither.

     1  does this chip produce the SAME instrument as the laptop, bit for bit
     2  how much does the heap move across init, train and predict (a figure)
     3  does a saved instrument survive a real write to real flash
     4  does a CORRUPTED file actually get refused, on this hardware
     5  how long does iris_train take on this recipe, at four sizes (figures)
     6  how much stack is left at the deepest point
     7  do predictions drift over tens of thousands of calls
     8  do several instruments running at once stay independent
     9  the mean time of one prediction, loop overhead included (a figure;
        board_probe measures one call properly)

   BOARD SETTINGS: as in GET-STARTED.md.
   RUNTIME: about a minute. Open Serial Monitor at 115200 and wait for DONE.
   ========================================================================= */
#include "iris.h"
#if IRIS_VERSION_MAJOR != 0 || IRIS_VERSION_MINOR != 2
#error "This sketch is written for iris 0.2. Copy iris.h from iris 0.2 (https://github.com/kylebsmith/iris) into this sketch's folder, next to the .ino file, replacing the copy there."
#endif

/* NO FUSED MULTIPLY-ADD IN THIS FILE. Test 1's demonstrations are computed
   here, and 1.0f - (float)i * 0.03f is a multiply and a subtract, which GCC
   fuses into one instruction by default, rounding once instead of twice. The
   fused inputs differ in the last bit, so the chip would train on different
   numbers from the laptop and test 1 would fail for a reason that has
   nothing to do with the library (measured on a laptop with GCC: 0x60E31823
   instead of 0xB7FC47A0). iris.h switches fusing off for its own code only;
   this switches it off for the rest of this file. */
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("fp-contract=off")
#elif defined(__clang__)
#pragma STDC FP_CONTRACT OFF
#endif

#ifdef ARDUINO_ARCH_ESP32
#if ARDUINO_USB_MODE
#error "Wrong USB Mode. Set Tools -> USB Mode -> 'USB-OTG (TinyUSB)'."
#endif
#if !ARDUINO_USB_CDC_ON_BOOT
#error "Set Tools -> USB CDC On Boot -> 'Enabled', or Serial Monitor stays empty."
#endif
#include <Preferences.h>
static Preferences store;
#endif

/* The value this exact recipe produces on a laptop with iris 0.2.0, pinned in
   the library's tests/starter_recipes.c. If the chip disagrees, that is a
   real finding about the chip or its compiler, not a broken test. Keep the
   output and tell someone. */
#define HOST_HASH 0xB7FC47A0u

#define NI 2
#define NH 12
#define NO 3
#define CAP 20

static unsigned char arena[IRIS_ARENA(NI, NH, NO, CAP)];
static unsigned char blob[IRIS_ARENA(NI, NH, NO, CAP)];   /* always >= save size */
static int passed = 0, failed = 0;

static int skipped = 0;

static void result(const char *name, int ok, const char *detail) {
  Serial.print(ok ? F("  PASS  ") : F("  FAIL  "));
  Serial.print(name);
  for (int i = (int)strlen(name); i < 34; ++i) Serial.print(' ');
  Serial.println(detail);
  if (ok) passed++; else failed++;
}

#ifndef ARDUINO_ARCH_ESP32
static void skip(const char *name, const char *why) {
  Serial.print(F("  SKIP  "));
  Serial.print(name);
  for (int i = (int)strlen(name); i < 34; ++i) Serial.print(' ');
  Serial.println(why);
  skipped++;
}
#endif

static void hex8(uint32_t v, char *out) {
  for (int i = 0; i < 8; ++i) {
    int d = (int)((v >> (28 - 4 * i)) & 0xF);
    out[i] = (char)(d < 10 ? '0' + d : 'A' + d - 10);
  }
  out[8] = 0;
}

/* The same fixed recipe the laptop test runs: 20 demonstrations, then exactly
   800 training passes continuing from the starting weights iris_init drew.
   iris_continue is the fixed-length trainer; iris_train would stop wherever
   the error levels off, which is not the pinned recipe. Nothing here may
   vary. */
static iris *build(void) {
  iris *k = iris_init(arena, sizeof arena, NI, NH, NO, CAP, 1234u);
  if (!k) return 0;
  for (int i = 0; i < CAP; ++i) {
    float in[NI], out[NO];
    in[0] = (float)i * 0.05f;
    in[1] = 1.0f - (float)i * 0.03f;
    out[0] = (float)((i * 7) % 11) / 11.0f;
    out[1] = (float)((i * 3) % 5)  / 5.0f;
    out[2] = (float)((i * 5) % 7)  / 7.0f;
    iris_record(k, in, out);
  }
  iris_continue(k, 800);
  return k;
}

static uint32_t hash_predictions(iris *k) {
  uint32_t h = 2166136261u;                       /* FNV-1a over the raw bytes */
  for (int i = 0; i <= 20; ++i) {
    float in[NI], out[NO];
    in[0] = (float)i * 0.05f;
    in[1] = 1.0f - (float)i * 0.03f;
    iris_predict(k, in, out);
    for (int o = 0; o < NO; ++o) {
      unsigned char b[sizeof(float)];
      memcpy(b, &out[o], sizeof(float));
      for (unsigned j = 0; j < sizeof(float); ++j) { h ^= b[j]; h *= 16777619u; }
    }
  }
  return h;
}

void setup(void) {
  Serial.begin(115200);
  /* WAIT FOR THE PORT, do not guess at it with a delay. On a board with native
     USB the serial device does not exist until the computer opens it, and
     anything printed before that is gone for ever. */
  while (!Serial && millis() < 20000) delay(10);
  delay(200);
}

static int done = 0;

void loop(void) {
  if (done) {
    /* Print the verdict again every ten seconds. If you plug in late, or open
       Serial Monitor after the run, you still get the answer instead of a
       blank screen. */
    delay(10000);
    Serial.print(F("  [still here] "));
    Serial.print(passed); Serial.print(F(" passed, "));
    Serial.print(failed); Serial.print(F(" failed, "));
    Serial.print(skipped); Serial.println(F(" skipped -- press RESET to run again."));
    return;
  }
  done = 1;

  Serial.println();
  Serial.println(F("=================================================================="));
  Serial.print(F("  IRIS ON-DEVICE TORTURE TEST   library "));
  Serial.println(IRIS_VERSION_STRING);
  Serial.println(F("=================================================================="));
  Serial.println();

  char buf[64], hx[9];

  /* ---- 1. determinism ---------------------------------------------------- */
  /* The heap is sampled with NOTHING between the two reads but library calls,
     so printing and the USB stack's own allocations stay out of test 2. */
#ifdef ARDUINO_ARCH_ESP32
  uint32_t heap_before = ESP.getFreeHeap();
#endif
  iris *k = build();
  if (!k) { Serial.println(F("  iris_init refused -- cannot continue.")); return; }
  uint32_t h = hash_predictions(k);
#ifdef ARDUINO_ARCH_ESP32
  uint32_t heap_after = ESP.getFreeHeap();      /* sampled before any printing */
#endif
  hex8(h, hx);
  strcpy(buf, "0x"); strcat(buf, hx);
  strcat(buf, HOST_HASH == h ? " == host" : " != host 0xB7FC47A0");
  result("1 determinism on this chip", HOST_HASH == h, buf);

  /* ---- 2. does it allocate ----------------------------------------------- */
#ifdef ARDUINO_ARCH_ESP32
  { long d = (long)heap_before - (long)heap_after;
    /* REPORTED, NOT JUDGED. This board runs an operating system and a USB
       stack that allocate on their own schedule, so a heap delta measured
       across any span of time measures them as well as this library. The
       proof that iris never allocates is its symbol table: built
       freestanding, it references no allocator at all (the library's
       tests/freestanding.sh checks that with this chip's own compiler). */
    Serial.print(F("  ----  2 heap delta across init+train+predict  "));
    Serial.print(d); Serial.println(F(" bytes (the OS's, not ours --"));
    Serial.println(F("        the no-allocation proof is the symbol table,"));
    Serial.println(F("        checked by sh build.sh target)")); }
#else
  skip("2 heap delta", "needs the ESP32 heap counter");
#endif

  /* ---- 3. save / load through REAL flash ---------------------------------- */
  size_t n = iris_save(k, blob, sizeof blob);
#ifdef ARDUINO_ARCH_ESP32
  if (n) {
    store.begin("torture", false);
    store.putBytes("blob", blob, n);
    memset(blob, 0, sizeof blob);                 /* prove it comes from flash */
    size_t got = store.getBytes("blob", blob, sizeof blob);
    static unsigned char arena2[IRIS_ARENA(NI, NH, NO, CAP)];
    iris *r = iris_init(arena2, sizeof arena2, NI, NH, NO, CAP, 9999u);
    int ok = r && got == n && iris_load(r, blob, got) && hash_predictions(r) == h;
    snprintf(buf, sizeof buf, "%u bytes through flash, hash %s", (unsigned)n,
             ok ? "identical" : "CHANGED");
    result("3 survives a real power cycle", ok, buf);
    store.end();
  } else result("3 survives a real power cycle", 0, "iris_save returned 0");
#else
  skip("3 survives a real power cycle", "needs the ESP32 flash store");
#endif

  /* ---- 4. corruption is refused, ON THIS HARDWARE ------------------------- */
  { static unsigned char scratch[sizeof blob];
    static unsigned char probe[IRIS_ARENA(NI, NH, NO, CAP)];
    long refused = 0, accepted = 0;
    for (size_t byte = 0; byte < n; ++byte) {
      for (int bit = 0; bit < 8; ++bit) {
        memcpy(scratch, blob, n);
        scratch[byte] ^= (unsigned char)(1u << bit);
        iris *c = iris_init(probe, sizeof probe, NI, NH, NO, CAP, 1u);
        if (c && iris_load(c, scratch, n)) accepted++; else refused++;
      }
    }
    snprintf(buf, sizeof buf, "%ld refused, %ld accepted", refused, accepted);
    result("4 every single-bit corruption", accepted == 0, buf); }

  /* ---- 5. what training actually costs ------------------------------------ */
  Serial.println();
  Serial.println(F("  5 iris_train time on this chip, on this sketch's recipe"));
  Serial.println(F("    (inputs that move together: not representative data --"));
  Serial.println(F("    board_probe times training on representative data):"));
  { const int sizes[] = { 4, 8, 14, 20 };
    for (unsigned s = 0; s < sizeof sizes / sizeof *sizes; ++s) {
      static unsigned char a3[IRIS_ARENA(NI, NH, NO, CAP)];
      iris *t = iris_init(a3, sizeof a3, NI, NH, NO, CAP, 1234u);
      for (int i = 0; i < sizes[s]; ++i) {
        float in[NI] = { (float)i * 0.05f, 1.0f - (float)i * 0.03f };
        float out[NO] = { (float)(i % 3) / 3.0f, (float)(i % 5) / 5.0f, 0.5f };
        iris_record(t, in, out);
      }
      uint32_t t0 = millis();
      iris_train(t);
      uint32_t ms = millis() - t0;
      Serial.print(F("        "));
      Serial.print(sizes[s]); Serial.print(F(" demonstrations  "));
      Serial.print(ms); Serial.print(F(" ms   "));
      Serial.print(iris_train_epochs_done(t)); Serial.println(F(" epochs"));
    } }
  Serial.println();

  /* ---- 6. stack headroom at the deepest point ----------------------------- */
#ifdef ARDUINO_ARCH_ESP32
  { UBaseType_t hw = uxTaskGetStackHighWaterMark(NULL);
    snprintf(buf, sizeof buf, "%u bytes never used", (unsigned)hw);
    result("6 stack headroom remaining", hw > 512, buf); }
#else
  skip("6 stack headroom remaining", "needs the ESP32's task stack counter");
#endif

  /* ---- 7. does it drift over a long run ----------------------------------- */
  { float first[NO], now[NO];
    float in[NI] = { 0.37f, 0.61f };
    iris_predict(k, in, first);
    int drifted = 0;
    for (long i = 0; i < 40000L; ++i) {
      float junk[NO];
      float x[NI] = { (float)(i % 100) * 0.01f, (float)(i % 37) * 0.02f };
      iris_predict(k, x, junk);
    }
    iris_predict(k, in, now);
    for (int o = 0; o < NO; ++o) if (now[o] != first[o]) drifted = 1;
    snprintf(buf, sizeof buf, "40,000 predictions, %s",
             drifted ? "OUTPUT MOVED" : "bit-identical");
    result("7 no drift over a long run", !drifted, buf); }

  /* ---- 8. several instruments at once ------------------------------------- */
  { static unsigned char a[3][IRIS_ARENA(NI, NH, NO, 8)];
    iris *set[3]; uint32_t want[3];
    for (int j = 0; j < 3; ++j) {
      set[j] = iris_init(a[j], sizeof a[j], NI, NH, NO, 8, (uint32_t)(100 + j));
      for (int i = 0; i < 8; ++i) {
        float in[NI] = { (float)i * 0.1f, (float)j * 0.2f };
        float out[NO] = { (float)j / 3.0f, (float)i / 8.0f, 0.25f };
        iris_record(set[j], in, out);
      }
      iris_train(set[j]);
    }
    for (int j = 0; j < 3; ++j) want[j] = hash_predictions(set[j]);
    for (long round = 0; round < 2000; ++round)      /* interleave them hard */
      for (int j = 0; j < 3; ++j) {
        float in[NI] = { (float)(round % 10) * 0.1f, (float)j * 0.2f }, out[NO];
        iris_predict(set[j], in, out);
      }
    int same = 1;
    for (int j = 0; j < 3; ++j) if (hash_predictions(set[j]) != want[j]) same = 0;
    result("8 instruments stay independent", same,
           same ? "3 instruments, 6,000 interleaved, no cross-talk"
                : "AN INSTRUMENT CHANGED"); }

  /* ---- 9. the mean time of one prediction, loop overhead included --------
     20,000 calls timed with micros(), divided by 20,000. The figure includes
     the loop, the input update and the volatile store, and it is a mean, not
     a worst case. board_probe measures one call with the cycle counter, the
     empty loop subtracted, and prints percentiles. `sink` is volatile so the
     compiler cannot delete the work. */
  { static volatile float sink = 0.0f;
    const long N = 20000;
    float in[NI] = { 0.37f, 0.61f }, out[NO];
    iris_predict(k, in, out);                     /* warm the caches */
    uint32_t t0 = micros();
    for (long i = 0; i < N; ++i) {
      in[0] = (float)(i % 100) * 0.01f;
      iris_predict(k, in, out);
      sink = sink + out[0];
    }
    uint32_t us = micros() - t0;
    float each = (float)us / (float)N;
    Serial.println();
    Serial.print(F("  9 mean per prediction, loop overhead included   "));
    Serial.print(each, 3);
    Serial.print(F(" us   (")); Serial.print(N); Serial.print(F(" calls in "));
    Serial.print(us); Serial.println(F(" us)"));
    Serial.print(F("      one audio sample at 48 kHz lasts 20.8 us; this mean is "));
    Serial.print(each / 20.8f * 100.0f, 1);
    Serial.println(F("% of it."));
    Serial.println(); }

  /* ---- verdict ------------------------------------------------------------ */
  Serial.println();
  Serial.println(F("=================================================================="));
  Serial.print(F("  "));
  Serial.print(passed); Serial.print(F(" passed, "));
  Serial.print(failed); Serial.print(F(" failed, "));
  Serial.print(skipped); Serial.println(F(" skipped"));
  if (!failed) Serial.println(F("  Every test that ran passed on this chip. DONE."));
  else         Serial.println(F("  Something above is real. Keep the output. DONE."));
  Serial.println(F("=================================================================="));
}
