/* ===========================================================================
   iris_instrument — demonstrate a gesture, set the sound, play it.

   Tilt the board into a pose, drag the three bars until it sounds right, tap
   RECORD. Do that a few times. It learns the mapping, and then one tilt moves
   all three parameters together, in the relationship you showed it.

   The parameters go out as MIDI (Musical Instrument Digital Interface, the
   message format synthesisers understand) over the USB (Universal Serial
   Bus) cable: three CC messages (control change, a numbered controller set
   to a value from 0 to 127), so any synthesiser that accepts USB MIDI can
   play it.

   ---------------------------------------------------------------------------
   LIBRARIES   Install from Tools -> Manage Libraries:
                 Adafruit BNO055 · Adafruit GFX Library · Adafruit ILI9341
               Say yes when it offers their dependencies (GET-STARTED.md,
               step 3). SPI, Wire, USB and USBMIDI already come with the ESP32
               board package — do not install those separately.

               iris.h is in this folder. It is the one thing here with no
               dependencies at all, and that is the point: display and sensor
               drivers are replaceable plumbing, but the code that decides how
               your gesture becomes sound should never change under you.

   BOARD SETTINGS   Tools -> ESP32S3 Dev Module, then (GET-STARTED.md, step 4,
   explains each one):
     USB Mode            USB-OTG (TinyUSB)      <- MIDI does not exist without this
     USB CDC On Boot     Enabled                <- or Serial never appears
     Flash Size          16MB (128Mb)
     PSRAM               OPI PSRAM
     Partition Scheme    16M Flash (3MB APP/9.9MB FATFS)

   IF THE BOARD STOPS ACCEPTING UPLOADS
     Send 'R' over Serial, or CC 123 value 127 on MIDI channel 16. Either one
     reboots it into the bootloader. Failing both: hold BOOT, tap RESET,
     release BOOT, then upload.
   =========================================================================== */

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <Adafruit_BNO055.h>
/* The board settings that fail SILENTLY if you get them wrong: the wrong
   USB Mode compiles fine and then the board never appears as a MIDI device,
   and USB CDC On Boot off (CDC, Communications Device Class, is the USB
   serial-port standard) gives you a board that runs but cannot talk to you.
   Neither produces an error on its own, so here is the error. These come
   before the USB includes, which do not exist for other boards. */
#if defined(ARDUINO_ARCH_ESP32) && !defined(CONFIG_IDF_TARGET_ESP32S3)
#error "This sketch is for the ESP32-S3 display board. Set Tools -> Board -> esp32 -> ESP32S3 Dev Module, then set the board options in GET-STARTED.md."
#endif
#if ARDUINO_USB_MODE
#error "Set Tools -> USB Mode -> USB-OTG (TinyUSB). This sketch appears to the computer as a USB MIDI instrument, and only the TinyUSB mode can do that: in Hardware CDC and JTAG mode the board runs but no MIDI device appears."
#endif
#if defined(CONFIG_IDF_TARGET_ESP32S3) && !ARDUINO_USB_CDC_ON_BOOT
#error "Set Tools -> USB CDC On Boot -> Enabled. The board's USB socket is the chip's own USB port; with this setting off, Serial prints to pins 43 and 44 instead and Serial Monitor stays empty."
#endif
#include <USB.h>
#include <USBMIDI.h>
#include "esp32-hal-tinyusb.h"
#include "iris.h"
#if IRIS_VERSION_MAJOR != 0 || IRIS_VERSION_MINOR != 2
#error "This sketch is written for iris 0.2. Copy iris.h from iris 0.2 (https://github.com/kylebsmith/iris) into this sketch's folder, next to the .ino file, replacing the copy there."
#endif


/* ===========================================================================
   YOUR BUILD — everything below is true of MY hardware, not of hardware in
   general. If you change the board, the panel, or how the sensor is mounted,
   this is the only block you should need to touch.
   =========================================================================== */

/* ES3C28P board: ESP32-S3, 240x320 ILI9341 panel driven over SPI (serial
   peripheral interface, the fast four-wire bus screens use), FT6336
   capacitive touch. */
#define LCD_CS   10
#define LCD_DC   46
#define LCD_BL   45
#define SPI_SCK  12
#define SPI_MISO 13
#define SPI_MOSI 11

/* One I2C bus, three devices: touch 0x38, audio codec 0x18, BNO055 0x28. */
#define SDA_PIN   16
#define SCL_PIN   15
#define TOUCH_ADDR 0x38

/* The BNO055 answers at 0x28, or 0x29 if the ADR (address-select) pad on the breakout is
   bridged. We try both rather than telling you the cable is loose. */
#define BNO_ADDR_A 0x28
#define BNO_ADDR_B 0x29

/* This particular panel ships with its colours inverted. If yours comes up
   looking like a photographic negative, flip this. */
#define PANEL_INVERTED true

/* Screen is 240 wide, 320 tall, in rotation 0. The FT6336 reports touch in
   the same frame, so touch coordinates are used as screen coordinates with no
   transform. Change the rotation and you must revisit that. */
#define W 240
#define H 320

/* WHICH WAY IS TILT? Gravity is a 3-vector pointing down through the board.
   Which two components mean "roll" and "pitch" depends entirely on how your
   sensor is glued down. Mine reads x and y. If your instrument responds to the
   wrong movement, swap these before you change anything else.

   You do NOT need to correct for a sensor that sits slightly off level. iris
   fits its input range to the demonstrations you actually give it, so a tilted
   mount is absorbed the moment you record your first pose. */
#define AXIS_1 g.x()
#define AXIS_2 g.y()

/* The three MIDI CCs this instrument sends, on channel 1. */
static const uint8_t CC[] = { 1, 2, 3 };
#define NOUT  (sizeof CC / sizeof CC[0])
#define MAXEX 16                        /* how many demonstrations it can hold */

/* ===========================================================================
   The instrument.
   =========================================================================== */

/* Layout. The bars are drawn and hit-tested from these, so the drawn button
   and the region that responds to a touch cannot drift apart. */
#define BAR_X    50                     /* left edge of every bar */
#define BAR_W    (W - BAR_X - 20)       /* 170 px of travel, 0..127 */
#define BAR_TOP  70
#define BAR_H    34                     /* drawn height of one bar */
#define BAR_GAP  44                     /* bar pitch: 34 of bar, 10 of space */

#define REC_X 14
#define REC_Y 245
#define REC_W (W - 28)
#define REC_H 58

#define CLR_X (W - 46)
#define CLR_Y 6
#define CLR_W 40
#define CLR_H 30

Adafruit_ILI9341 tft = Adafruit_ILI9341(LCD_CS, LCD_DC, -1);
Adafruit_BNO055  bno = Adafruit_BNO055(55, BNO_ADDR_A, &Wire);
USBMIDI MIDI;

/* iris needs one block of memory and never asks for more. IRIS_ARENA works out
   how big at compile time from the same four numbers passed to iris_init, so
   if you change the shape, change it in BOTH places or the arena will be too
   small and iris_init will refuse. */
#define N_IN  2
#define N_HID 12
static unsigned char arena[IRIS_ARENA(N_IN, N_HID, NOUT, MAXEX)];
static iris *k;

static int  value[NOUT] = { 64, 64, 64 };  /* the CCs in force RIGHT NOW */
static int  drawn[NOUT] = { -1, -1, -1 };  /* what is currently drawn on the screen */
static int  demos   = 0;
static bool editing = true;                /* setting a sound, not playing */

/* --- sensor -------------------------------------------------------------
   Gravity, not Euler angles. Euler angles wrap: somewhere in the rotation they
   step from +180 to -180, so two poses one degree apart read as opposite ends
   of the range. Nothing smooth can fit that, and the sound falls off a cliff
   right there. Measured on this mapping, crossing the wrap makes the worst
   one-degree step 5x steeper. Gravity just points down and never wraps.

   No scaling either. iris normalises each input from your demonstrations, so
   raw metres per second squared works exactly as well as anything you divide
   it by. Feed it whatever your sensor gives you. */
static void tilt(float *in) {
  imu::Vector<3> g = bno.getVector(Adafruit_BNO055::VECTOR_GRAVITY);
  in[0] = (float)AXIS_1;
  in[1] = (float)AXIS_2;
}

/* --- touch --------------------------------------------------------------- */
static bool touched(int *x, int *y) {
  Wire.beginTransmission(TOUCH_ADDR);
  Wire.write(0x02);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(TOUCH_ADDR, 5) != 5) return false;
  uint8_t n  = Wire.read();
  uint8_t xh = Wire.read(), xl = Wire.read(), yh = Wire.read(), yl = Wire.read();
  if (!(n & 0x0F)) return false;
  *x = ((xh & 0x0F) << 8) | xl;
  *y = ((yh & 0x0F) << 8) | yl;
  return true;
}

static bool inside(int x, int y, int rx, int ry, int rw, int rh) {
  return x >= rx && x < rx + rw && y >= ry && y < ry + rh;
}

/* Which bar is under this touch? -1 for none, including the gaps between
   bars, so a near-miss does nothing rather than moving the wrong parameter. */
static int bar_at(int x, int y) {
  if (x < BAR_X - 20 || x >= BAR_X + BAR_W + 20) return -1;
  int i = (y - BAR_TOP) / BAR_GAP;
  if (y < BAR_TOP || i >= (int)NOUT) return -1;
  if ((y - BAR_TOP) % BAR_GAP >= BAR_H) return -1;   /* landed in a gap */
  return i;
}

/* --- screen -------------------------------------------------------------- */
static void bar(int i, int v, uint16_t colour) {
  int y = BAR_TOP + i * BAR_GAP;
  int w = (v * BAR_W) / 127;
  tft.fillRect(BAR_X, y, BAR_W, BAR_H, ILI9341_BLACK);
  tft.drawRect(BAR_X, y, BAR_W, BAR_H, ILI9341_WHITE);
  if (w > 2) tft.fillRect(BAR_X + 1, y + 1, w - 2, BAR_H - 2, colour);
  tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  tft.setTextSize(2);
  tft.setCursor(6, y + 9);
  tft.printf("CC%d", CC[i]);
  drawn[i] = v;
}

static void button(const char *label, uint16_t colour) {
  tft.fillRect(REC_X, REC_Y, REC_W, REC_H, colour);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(3);
  tft.setCursor(REC_X + 12, REC_Y + 18);
  tft.print(label);
}

static void screen() {
  char rec[24];
  tft.fillScreen(ILI9341_BLACK);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  tft.setCursor(14, 30);
  tft.print(editing ? "SET THE SOUND" : "PLAYING");

  for (unsigned i = 0; i < NOUT; ++i)
    bar(i, value[i], editing ? ILI9341_CYAN : ILI9341_GREEN);

  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  tft.setCursor(14, 212);
  tft.print(editing ? "then hold a pose:" : "drag a bar to edit");

  if (demos >= MAXEX) button("MEMORY FULL", ILI9341_DARKGREY);
  else { snprintf(rec, sizeof rec, "RECORD (%d)", demos); button(rec, ILI9341_BLUE); }

  tft.fillRect(CLR_X, CLR_Y, CLR_W, CLR_H, ILI9341_RED);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  tft.setCursor(CLR_X + 6, CLR_Y + 7);
  tft.print("CLR");
}

static void send(int i, int v) {
  value[i] = v;
  MIDI.controlChange(CC[i], v, 1);
}

static void reboot_to_bootloader() {
  usb_persist_restart(RESTART_BOOTLOADER);
}

/* A failure goes to the screen AND to Serial Monitor, repeated every two
   seconds so a monitor opened late still sees it: a dead screen must not
   leave the board silent. Each line fits the panel at text size 2 (19
   characters from x = 10). 'R' still restarts into the bootloader. */
static void fail(const char *line1, const char *line2) {
  tft.fillScreen(ILI9341_BLACK);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  tft.setCursor(10, 140); tft.print(line1);
  tft.setCursor(10, 168); tft.print(line2);
  for (;;) {
    Serial.print("STOPPED: "); Serial.print(line1); Serial.print(" "); Serial.println(line2);
    for (int i = 0; i < 200; ++i) {
      while (Serial.available()) if (Serial.read() == 'R') reboot_to_bootloader();
      delay(10);
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(LCD_BL, OUTPUT);
  digitalWrite(LCD_BL, HIGH);

  /* SPI.begin's argument order is sck, miso, mosi, ss — not the order the
     pins are usually listed in. Getting it wrong gives you a white screen. */
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, LCD_CS);
  tft.begin();
  tft.invertDisplay(PANEL_INVERTED);
  tft.setRotation(0);

  Wire.begin(SDA_PIN, SCL_PIN);
  MIDI.begin();            /* every USB interface must be up ... */
  USB.begin();             /* ... before USB starts, and USB starts once */

  if (!bno.begin()) {
    bno = Adafruit_BNO055(55, BNO_ADDR_B, &Wire);
    if (!bno.begin())
      fail("No BNO055 found.", "See PARTS.md wiring");
  }

  k = iris_init(arena, sizeof arena, N_IN, N_HID, NOUT, MAXEX, /*seed=*/1234);
  if (!k) fail("iris_init refused:", "arena too small");

  screen();
}

void loop() {
  while (Serial.available()) if (Serial.read() == 'R') reboot_to_bootloader();

  midiEventPacket_t rx;
  while (MIDI.readPacket(&rx))
    if ((rx.byte1 & 0xF0) == 0xB0 && (rx.byte1 & 0x0F) == 15 &&
        rx.byte2 == 123 && rx.byte3 == 127) reboot_to_bootloader();

  float in[N_IN];
  tilt(in);

  int tx, ty;
  static bool was = false;
  bool now = touched(&tx, &ty);
  int  b   = now ? bar_at(tx, ty) : -1;

  if (now && !was && inside(tx, ty, CLR_X, CLR_Y, CLR_W, CLR_H)) {
    iris_clear(k);
    demos = 0;
    editing = true;
    screen();
    delay(200);
  }
  /* Dragging a bar stops playback, so what you hear while you set a sound is
     the sound you are setting — not the instrument reacting to you moving the
     board into position. */
  else if (b >= 0) {
    if (!editing) { editing = true; screen(); }
    int v = ((tx - BAR_X) * 127) / (BAR_W - 1);
    v = v < 0 ? 0 : (v > 127 ? 127 : v);
    if (v != drawn[b]) { send(b, v); bar(b, v, ILI9341_CYAN); }
  }
  else if (now && !was && inside(tx, ty, REC_X, REC_Y, REC_W, REC_H)) {
    if (demos < MAXEX) {
      /* Record what is sounding right now, which in either mode is value[]. */
      float t[NOUT];
      for (unsigned i = 0; i < NOUT; ++i) t[i] = value[i] / 127.0f;
      /* See iris_tilt: count what was accepted, not what was offered. */
      if (!iris_record(k, in, t))
        Serial.println("that reading was refused -- not counted. Try again.");
      else {
      demos++;
      if (demos >= 2) {
        button("TRAINING", ILI9341_DARKGREY);   /* this blocks; say so */
        if (!iris_train(k))
          Serial.println("TRAINING FAILED -- the instrument is not fitted.");
        editing = false;
      }
      screen();
      }
    }
    delay(200);
  }
  was = now;

  if (!editing) {
    float out[NOUT];
    iris_predict(k, in, out);
    for (unsigned i = 0; i < NOUT; ++i) {
      int cc = (int)(out[i] * 127.0f + 0.5f);
      if (cc != drawn[i]) { send(i, cc); bar(i, cc, ILI9341_GREEN); }
    }
  }
  delay(12);
}
