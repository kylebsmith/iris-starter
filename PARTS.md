# Parts

Everything the sketches here need, what each part connects to, and where the
facts come from. Prices change; the part numbers do not.

Terms used on this page: **USB** (Universal Serial Bus) is the cable
between the board and your computer; USB-C is its reversible plug. **I2C**
(inter-integrated circuit) is the two-wire bus that carries a data line
(SDA) and a clock line (SCL) between the board and the sensor. **STEMMA QT**
is Adafruit's name for its 4-pin I2C connector, a JST SH socket (JST is the
connector's maker, SH its series) at 1.0 mm pitch, compatible with
SparkFun's Qwiic. **Pitch** is the distance between
neighbouring pins. **IMU** (inertial measurement unit) is a motion sensor;
the BNO055 is one, and its "9-DOF" (nine degrees of freedom) means it
measures motion, rotation and magnetic field on three axes each. **MIDI**
(Musical Instrument Digital Interface) is the message format synthesisers
understand. **GPIO** (general-purpose input/output) is a numbered pin of the
chip. **GND** is ground.

| Part | Manufacturer part number | Where to get it | Qty | Connects to |
|---|---|---|---|---|
| 2.8-inch ESP32-S3 display board with capacitive touch | LCDWIKI **ES3C28P** (the ES3N28P is the same board without touch; `display_check` and `iris_instrument` need touch) | Manufacturer page: http://www.lcdwiki.com/2.8inch_ESP32-S3_Display (it names no shop). Sold on online marketplaces under the model number: check that the listing says ES3C28P, not ES3N28P | 1 | Your computer, by USB-C |
| Orientation sensor | Adafruit **4646**, "Adafruit 9-DOF Absolute Orientation IMU Fusion Breakout - BNO055 - STEMMA QT / Qwiic" | https://www.adafruit.com/product/4646 | 1 | The board's I2C socket, through the two cables below |
| Sensor cable | Adafruit **4209**, "STEMMA QT / Qwiic JST SH 4-pin to Premium Male Headers Cable - 150mm Long" | https://www.adafruit.com/product/4209 | 1 | Sensor's STEMMA QT socket to the board's lead (below) |
| Board lead | the "4P 1.25mm to 2.54mm terminal wire" that ships with the ES3C28P (vendor specification page 4) | in the box | 1 | Board's I2C socket to the sensor cable |
| USB-C cable **that carries data** | any | any | 1 | Board to computer. A charge-only cable looks like a dead board. |
| 10 kΩ linear potentiometers (knobs) | Adafruit **562**, "Panel Mount 10K potentiometer (Breadboard Friendly) - 10K Linear" (any 10 kΩ linear potentiometer works) | https://www.adafruit.com/product/562 | 3 | Only for the three `boilerplate/` sketches and `iris_scope` with `USE_ANALOG 1`: the board's expansion socket |
| A synthesiser that accepts USB MIDI | any: a software synth, a digital audio workstation, or a hardware synth with USB MIDI in | — | 1 | Only for `iris_instrument`, over the same USB cable |

## Connecting the sensor

The board's I2C socket is a **1.25 mm** 4-pin socket (vendor specification,
ES3C28P/ES3N28P Specification V1.0, page 9, "I2C peripheral interface: 1.25mm
4P socket"; https://www.lcdwiki.com/2.8inch_ESP32-S3_Display, "IIC
interface"; IIC is another name for I2C). The sensor's STEMMA QT socket is **1.0 mm** (JST SH;
https://www.adafruit.com/product/4209, "1mm pitch"). A STEMMA QT cable
therefore does not fit the board. Join them without soldering like this:

1. Plug the board's own lead (1.25 mm end) into the board's I2C socket.
2. Plug the STEMMA QT end of Adafruit 4209 into either STEMMA QT socket on the
   sensor.
3. Push each of 4209's four male pins into the lead's matching 2.54 mm end,
   by signal:

   | Signal | Adafruit 4209 wire (https://www.adafruit.com/product/4209) | Board |
   |---|---|---|
   | 3.3 V | red | 3.3 V pin of the I2C socket |
   | ground | black | GND pin of the I2C socket |
   | SDA, data | blue | GPIO 16 |
   | SCL, clock | yellow | GPIO 15 |

   GPIO 16 and 15 are the I2C socket's data and clock lines (vendor
   specification, page 10; http://www.lcdwiki.com/2.8inch_ESP32-S3_Display).
   The same two lines also carry the board's touch controller (address 0x38)
   and audio codec (0x18), so `i2c_find` always sees those two.

**CHECK ON THE BOARD before first power-up** (none of this is in the vendor
documents): that the lead's 2.54 mm end has female sockets that take 4209's
male pins; which of the lead's wires is 3.3 V, ground, SDA and SCL, read from
the silkscreen beside the board's I2C socket, not from the wire colours; and
that the joins hold when the sensor moves. A 3.3 V wire on the wrong pin can
damage the sensor.

The alternative is a cable with a 1.25 mm plug on one end and a JST SH 1.0 mm
plug on the other, wired to the same signal order; buy one only after
checking which 1.25 mm connector family the socket is, because several
incompatible ones share that pitch.

## Connecting the knobs

Only `boilerplate/any_sensor`, `bno055_portable`, `stemma_bno055` and
`iris_scope` with `USE_ANALOG 1` read knobs. On the ES3C28P they go on the
**expansion socket**, a 1.25 mm 4-pin socket carrying GPIO 2, 3, 14 and 21
(vendor specification, pages 9 and 12). Each knob's middle leg (the wiper)
goes to one pin:

| Knob | Pin |
|---|---|
| 1 (output one) | GPIO 2 |
| 2 (output two) | GPIO 3 |
| 3 (output three) | GPIO 14 |

The vendor documents list no power pin on that socket, so each knob's outer
legs take 3.3 V and ground from elsewhere: the 3.3 V and GND pins of the
sensor's breakout board are the nearest.

**CHECK ON THE BOARD:** the expansion socket's pin order; that its lead is the
same kind as the I2C lead; and that each knob, turned end to end, reads close
to 0 and close to 4095 in `analogRead` (GPIO 14 is on the chip's second
analog-to-digital converter, GPIO 2 and 3 on the first).

Do not use GPIO 4, 5 and 6 or `A0` (GPIO 1) for knobs on this board: they
are its audio lines and amplifier enable (vendor specification, page 11) and
reach no connector.

## Where the board facts come from

The vendor specification is "ES3C28P&ES3N28P Specification V1.0" by LCDWIKI,
linked from http://www.lcdwiki.com/2.8inch_ESP32-S3_Display. Page numbers
above are that document's.
