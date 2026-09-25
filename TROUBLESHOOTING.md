# When it doesn't work

Find the symptom, read the likely cause, do what the last column says. The
rows run roughly in the order you meet them.

Terms: **IDE** is the Arduino IDE (integrated development environment), the
program you upload sketches with. **USB** is Universal Serial Bus. **CDC**
(Communications Device Class) is the USB standard for a serial port; "USB
CDC On Boot" makes the board's own USB socket the serial port. **I2C**
(inter-integrated circuit) is the two-wire bus the sensor talks on: **SDA** is
its data line and **SCL** its clock line. **GPIO** (general-purpose
input/output) is a numbered pin of the chip. **MIDI** (Musical Instrument
Digital Interface) is the message format synthesisers understand, and
**USB-OTG** (On-The-Go) is the USB Mode setting that lets the board appear
as a MIDI device. **UART** (universal asynchronous receiver-transmitter) is
the chip's plain serial port.

| Symptom | Likely cause | What to do |
|---|---|---|
| No new entry in **Tools → Port** when you plug the board in | A charge-only USB-C cable; or on a Mac, the "Allow accessory to connect?" prompt was declined | Try another cable first. On a Mac, unplug and replug and answer **Allow**. On Windows the board's USB serial port needs Windows 10 or later (vendor specification, page 9). |
| The upload stops with `Failed to connect to ESP32-S3: No serial data received` | **Tools → Upload Mode** is UART0 / Hardware CDC while the board is running a TinyUSB sketch, so nothing restarts it into its loader | Set **Tools → Upload Mode → USB-OTG CDC (TinyUSB)** and upload again. If it still fails, use the BOOT and RESET sequence in the next row once; after that, uploads start by themselves. |
| The upload fails, or the port disappears during or after an upload | The board is running a sketch that took over the USB port, or it is stuck between the sketch and its loader | **Hold BOOT, tap RESET, release BOOT**, then upload again. That starts the chip's built-in loader. `iris_tilt` and `iris_instrument` also restart into it when you send a capital **R** in Serial Monitor. |
| The port is there, but **Serial Monitor is empty** | USB CDC On Boot is Disabled, so the sketch prints to pins 43 and 44 instead of the USB socket; or the baud rate is wrong; or you opened Serial Monitor after the sketch printed | Set **Tools → USB CDC On Boot → Enabled** and upload again (the sketches refuse to build without it). Set Serial Monitor to **115200**. Press RESET on the board with Serial Monitor open. |
| A red build error that starts `Set Tools -> ...` | A board setting is wrong, and the sketch says which | Change the setting it names, then upload again. The table in GET-STARTED.md, step 4, has every setting. |
| A red build error saying the sketch is for the ESP32-S3 display board | **Tools → Board** is not **ESP32S3 Dev Module** (for example "ESP32 Dev Module") | **Tools → Board → esp32 → ESP32S3 Dev Module**, then set step 4's options again: they reset when the board changes. |
| The build succeeds but the upload fails with an error about the chip | **Tools → Board** is another ESP32 entry; the sketches that build for any ESP32 (`i2c_find`, `iris_scope`, the three in `boilerplate/`, `determinism_check`, `device_torture`) do not stop at compile time | Choose **ESP32S3 Dev Module** and set step 4's options again. |
| `fatal error: Adafruit_BNO055.h: No such file or directory` (or `Adafruit_GFX.h`, `Adafruit_ILI9341.h`) | A library is not installed | **Tools → Manage Libraries**, install **Adafruit BNO055**, **Adafruit GFX Library** and **Adafruit ILI9341**, and say yes to their dependencies (GET-STARTED.md, step 3). |
| A red build error saying the sketch is written for iris 0.2 | The sketch folder holds a different version of `iris.h` | Copy `iris.h` from iris 0.2 (https://github.com/kylebsmith/iris) into that sketch's folder, replacing the copy there. |
| **"No BNO055"**, and the list of devices shows only **0x18** and **0x38** | Those two are the board's own audio codec and touch controller, always on the same bus; the sensor is not answering | Check the sensor's four wires against PARTS.md: 3.3 V, ground, SDA to GPIO 16, SCL to GPIO 15. Run `i2c_find`: it says when only the board's own chips answer. |
| **"No BNO055"** and nothing at all answers | No power to the bus, or the lead is not seated | Check 3.3 V and ground first, then that both ends of each join are pushed fully home. |
| The sensor answers at **0x29** instead of 0x28 | The sensor's address-select pad (ADR) is bridged | Nothing to do: the sketches try both addresses. |
| The screen stays black or white | The panel or its backlight is not starting, or the sketch stopped before drawing | Run `display_check`. If it draws and reports touches, the screen is fine and the problem is in the other sketch. |
| Touching the screen does nothing in `display_check` | The touch controller (FT6336, I2C address 0x38) is not answering, or you have the ES3N28P, which has no touch | Check the board's model on its label; run `i2c_find` and look for 0x38 on SDA 16 / SCL 15. |
| After a power cycle, `stemma_bno055` says it loaded the instrument but prints no numbers | Only one demonstration was saved; it plays from two | Tap SAVE at a second, different pose. It trains, then plays. |
| Holding SAVE prints `finishing training before saving...` and pauses | Training was still running; the sketch finishes it so the saved instrument is the trained one | Wait for `kept.`. A hold never records a demonstration. |
| A pose you did not mean is in the instrument | It was recorded | Send **d** in Serial Monitor (`boilerplate/any_sensor`, `iris_scope`) to delete the last demonstration and retrain, or **c** to clear everything. |
| The number or sound follows you, then jumps at one pose | The sketch was changed to read orientation in degrees, which wraps from +180 to −180 | Read the gravity vector, as every sketch here that reads the BNO055 does: it points down and never wraps. |
| The curve in `iris_scope` is flat | Two different targets at nearly the same pose; the network plays their average | Move the sensor a long way between demonstrations; `c` clears. |
| The `iris_scope` plot stays empty | Processing opened the wrong serial port, or the Arduino Serial Monitor still holds the port | Close Serial Monitor. In the plot window press **LEFT** or **RIGHT** to step through the ports; the top right names the one it is listening on. |
| `iris_instrument` runs but no MIDI device appears | USB Mode is not USB-OTG (TinyUSB) | Set **Tools → USB Mode → USB-OTG (TinyUSB)**. The sketch refuses to build otherwise, so this means an old build is on the board. |
| The MIDI device appears but the synthesiser does not respond | The synthesiser is not listening to the board, or nothing in it is assigned to controllers 1, 2 and 3 on channel 1 | Choose the board as the synthesiser's MIDI input and assign the controllers: [SOUND.md](SOUND.md). |
