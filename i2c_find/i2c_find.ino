/* i2c_find — where is my sensor?
   ===============================
   Every ESP32 board puts I2C (inter-integrated circuit, the two-wire bus
   sensors talk on) on different pins, and a STEMMA QT cable plugged
   into a board whose pins you guessed wrong looks exactly like a broken sensor:
   silence, no error, and a reading of zero that trains and plays perfectly
   happily. This sketch finds the sensor instead of guessing.

   It tries the board's own default pins first, then every pin pair that the
   common ESP32-S3 boards use, and prints every I2C address that answers on
   each, naming the pins as SDA (the data line) and SCL (the clock line) by
   their GPIO numbers (GPIO: general-purpose input/output, a numbered pin of
   the chip). Then it tells you which line to paste into your sketch.

   Known addresses it will name for you:
     0x28 / 0x29   BNO055 orientation      0x1C / 0x1E  LIS3MDL magnetometer
     0x6A / 0x6B   LSM6DS motion           0x38 / 0x18  the ES3C28P's own touch
                                                        controller and audio codec
     0x68 / 0x69   MPU6050, ICM20948       0x77 / 0x76  BMP/BME pressure
     0x29          VL53L4CD distance       0x5A         MPR121 touch
     0x10          VEML7700 light          0x39         APDS9960 gesture
     0x44          SHT4x humidity          0x36         seesaw / STEMMA soil
   ========================================================================= */
#include <Wire.h>

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

struct Pair { int sda, scl; const char *note; };

/* The default first: on many boards Wire.begin() with no arguments is already
   correct, and if it is you should not be hardcoding pins at all. */
static const Pair PAIRS[] = {
  { -1, -1, "Wire.begin() default for this board" },
  {  3,  4, "Adafruit Feather ESP32-S3 (STEMMA QT)" },
  { 16, 15, "ES3C28P I2C socket (these sketches' board)" },
  {  8,  9, "ESP32-S3-DevKitC common default" },
  {  5,  6, "ESP32-S3 alternate" },
  {  1,  2, "ESP32-S3 alternate" },
  { 41, 40, "Adafruit QT Py ESP32-S3" },
  { 42, 41, "ESP32-S3 alternate" },
  { 17, 18, "ESP32-S3 alternate" },
  { 21, 22, "ESP32 classic default" },
  {  7,  6, "ESP32-S3 alternate" },
  { 11, 12, "ESP32-S3 alternate" },
  { 13, 14, "ESP32-S3 alternate" },
};

static const char *name_of(uint8_t a) {
  switch (a) {
    case 0x28: case 0x29: return "BNO055 orientation (or VL53 distance at 0x29)";
    case 0x68: case 0x69: return "MPU6050 / ICM20948 motion";
    case 0x5A: return "MPR121 capacitive touch";
    case 0x10: return "VEML7700 light";
    case 0x39: return "APDS9960 gesture";
    case 0x44: return "SHT4x temperature/humidity";
    case 0x36: return "seesaw (STEMMA soil, rotary, etc)";
    case 0x76: case 0x77: return "BMP/BME pressure";
    case 0x1C: case 0x1E: return "LIS3MDL magnetometer";
    case 0x6A: case 0x6B: return "LSM6DS accelerometer/gyro";
    case 0x3C: case 0x3D: return "SSD1306 / SH1107 display";
    case 0x38: return "FT6336 touch controller (on the ES3C28P board itself)";
    case 0x18: return "audio codec (on the ES3C28P board itself)";
    default: return 0;
  }
}

static int found_total = 0, found_board = 0;

static int scan_one(const Pair &p) {
  Wire.end();
  delay(20);
  bool ok = (p.sda < 0) ? Wire.begin() : Wire.begin(p.sda, p.scl);
  if (!ok) return 0;
  Wire.setClock(100000);
  delay(30);

  int n = 0;
  for (uint8_t a = 0x08; a < 0x78; ++a) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      if (!n) {
        Serial.print(F("\n  FOUND on "));
        if (p.sda < 0) Serial.print(F("the default pins"));
        else { Serial.print(F("SDA ")); Serial.print(p.sda);
               Serial.print(F(" / SCL ")); Serial.print(p.scl); }
        Serial.print(F("   -- ")); Serial.println(p.note);
      }
      n++;
      if (p.sda == 16 && (a == 0x38 || a == 0x18)) found_board++;
      Serial.print(F("      0x"));
      if (a < 16) Serial.print('0');
      Serial.print(a, HEX);
      const char *nm = name_of(a);
      if (nm) { Serial.print(F("  = ")); Serial.println(nm); }
      else Serial.println(F("  = something, but not one I know"));

      if (a == 0x28 || a == 0x29) {
        Serial.print(F("      -> in your sketch use:  Wire.begin("));
        if (p.sda < 0) Serial.println(F(");   and Adafruit_BNO055(55, 0x28 or 0x29, &Wire)"));
        else { Serial.print(p.sda); Serial.print(F(", ")); Serial.print(p.scl);
               Serial.println(F(");")); }
      }
    }
  }
  found_total += n;
  return n;
}

void setup(void) {
  Serial.begin(115200);
  while (!Serial && millis() < 20000) delay(10);
  delay(200);
}

static int done = 0;

void loop(void) {
  if (done) {
    delay(10000);
    Serial.println(F("  [still here] press RESET to scan again."));
    return;
  }
  done = 1;

  Serial.println();
  Serial.println(F("=================================================================="));
  Serial.println(F("  I2C SCAN -- looking for your sensor on every plausible pin pair"));
  Serial.println(F("=================================================================="));

  for (unsigned i = 0; i < sizeof PAIRS / sizeof *PAIRS; ++i) scan_one(PAIRS[i]);

  Serial.println();
  if (found_total && found_total == found_board) {
    Serial.println(F("  ONLY THE BOARD'S OWN CHIPS ANSWERED (touch 0x38, codec 0x18)."));
    Serial.println(F("  Your sensor is not answering. Check its four wires against"));
    Serial.println(F("  PARTS.md: 3.3 V, ground, SDA to GPIO 16, SCL to GPIO 15."));
  } else if (!found_total) {
    Serial.println(F("  NOTHING ANSWERED ON ANY PIN PAIR."));
    Serial.println(F("  That means the sensor is not powered or not connected."));
    Serial.println(F("  Check the cable at BOTH ends, and check 3.3 V and ground"));
    Serial.println(F("  before you check anything else."));
  } else {
    Serial.print(F("  "));
    Serial.print(found_total);
    Serial.println(F(" device(s) answered. Use the pins printed above."));
  }
  Serial.println(F("  DONE."));
}
