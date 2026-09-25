/* display_check — does the screen light up, and does touch report sane numbers?
   Nothing to do with iris. This exists so that when the real sketch misbehaves
   you already know whether the panel and the touch controller are good. */

#include <SPI.h>
/* THIS SKETCH IS FOR ONE BOARD: the ES3C28P (LCDWIKI's 2.8-inch ESP32-S3
   display board). It drives that board's TFT (thin-film transistor) screen,
   an ILI9341 at 240 x 320, over SPI (serial peripheral interface, the fast
   four-wire bus screens use) on its fixed pins, and talks to its FT6336
   touch controller on I2C (inter-integrated circuit, a two-wire bus) at
   0x38, so it will not build for an Uno, a Pico or a plain ESP32 -- that is
   the hardware, not a bug. */
#include <Wire.h>

/* BOARD SETTINGS. This sketch drives the ES3C28P's own pins, so it needs
   the ESP32-S3 board entry, and Serial reaches the computer through the
   chip's own USB (Universal Serial Bus) port only with USB CDC On Boot
   enabled (CDC: Communications Device Class, the USB serial-port standard).
   Either USB Mode works: this sketch uses nothing from the USB-OTG mode's
   TinyUSB software. */
#if defined(ARDUINO_ARCH_ESP32) && !defined(CONFIG_IDF_TARGET_ESP32S3)
#error "This sketch is for the ESP32-S3 display board. Set Tools -> Board -> esp32 -> ESP32S3 Dev Module, then set the board options in GET-STARTED.md."
#endif
#if defined(CONFIG_IDF_TARGET_ESP32S3) && !ARDUINO_USB_CDC_ON_BOOT
#error "Set Tools -> USB CDC On Boot -> Enabled. The board's USB socket is the chip's own USB port; with this setting off, Serial prints to pins 43 and 44 instead and Serial Monitor stays empty."
#endif

#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

#define LCD_CS 10
#define LCD_DC 46
#define LCD_SCK 12
#define LCD_MOSI 11
#define LCD_MISO 13
#define LCD_BL 45
#define SDA_PIN 16
#define SCL_PIN 15
#define TOUCH_ADDR 0x38

Adafruit_ILI9341 tft = Adafruit_ILI9341(LCD_CS, LCD_DC, -1);

/* FT6336: touch count in 0x02, then X and Y as 12-bit values in 0x03..0x06. */
static bool touch_read(int *x, int *y) {
  Wire.beginTransmission(TOUCH_ADDR);
  Wire.write(0x02);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(TOUCH_ADDR, 5) != 5) return false;
  uint8_t n  = Wire.read() & 0x0F;
  uint8_t xh = Wire.read(), xl = Wire.read();
  uint8_t yh = Wire.read(), yl = Wire.read();
  if (n == 0) return false;
  *x = ((xh & 0x0F) << 8) | xl;
  *y = ((yh & 0x0F) << 8) | yl;
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(400);
  Serial.println("\ndisplay_check");

  pinMode(LCD_BL, OUTPUT);
  digitalWrite(LCD_BL, HIGH);                 /* backlight on, full brightness */

  SPI.begin(LCD_SCK, LCD_MISO, LCD_MOSI, LCD_CS);
  tft.begin();
  tft.invertDisplay(true);                    /* this panel ships inverted */
  tft.setRotation(0);
  tft.fillScreen(ILI9341_BLACK);

  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(3);
  tft.setCursor(20, 40);  tft.print("IRIS");
  tft.setTextSize(2);
  tft.setCursor(20, 90);  tft.print("touch me");

  tft.drawRect(10, 10, 220, 300, ILI9341_WHITE);
  tft.fillRect(20, 140, 60, 60, ILI9341_RED);
  tft.fillRect(90, 140, 60, 60, ILI9341_GREEN);
  tft.fillRect(160, 140, 60, 60, ILI9341_BLUE);

  Wire.begin(SDA_PIN, SCL_PIN);
  Serial.println("screen drawn. If you see IRIS, a white border and three");
  Serial.println("colour squares (red, green, blue) the panel is good.");
  /* Say so if the touch controller does not answer, rather than printing
     nothing for every touch. */
  Wire.beginTransmission(TOUCH_ADDR);
  if (Wire.endTransmission() != 0) {
    Serial.println("The touch controller (FT6336, I2C 0x38) is not answering.");
    Serial.println("An ES3N28P has no touch; on an ES3C28P, run i2c_find.");
  } else {
    Serial.println("Now touch it - coordinates print here.\n");
  }
}

void loop() {
  int x, y;
  if (touch_read(&x, &y)) {
    Serial.print(F("touch  x=")); Serial.print(x);
    Serial.print(F("  y=")); Serial.println(y);
    tft.fillCircle(x, y, 6, ILI9341_YELLOW);
    delay(40);
  }
  delay(10);
}
