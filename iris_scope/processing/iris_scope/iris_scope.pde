/* iris_scope — seeing the mapping
   ==============================
   Run iris_scope.ino on the board, then run this. Pick your serial port with
   the LEFT and RIGHT arrow keys if it does not connect on its own.

   TWO VIEWS, press TAB to swap:

     TRANSFER   The horizontal axis is your sensor. The vertical axis is what
                the instrument plays. Your demonstrations are the big dots.
                The line through them is what the network invented. The dots
                are the only part you gave it -- everything else on that line
                is the network's guess, and that guess is the instrument.

     SCOPE      The two outputs plotted against each other, exactly the way an
                oscilloscope in X-Y mode plots two voltages. One input sweeping
                its range traces a shape. Straight demonstrations can produce a
                curved shape, because the network is not a straight line.

   KEYS
     CLICK  choose what this pose should mean (the crosshair)
     TAB    swap view          SPACE  teach it this pose (sent to the board)
     d      delete the last    c      clear everything
     s      save a PNG         LEFT/RIGHT  choose the serial port
   ========================================================================= */

import processing.serial.*;

Serial port;
String[] portNames;
int portIndex = 0;
boolean connected = false;

// ---- what the board has told us -------------------------------------------
float lo = 0, hi = 1;                    // the input range the sensor has seen
int    nCurve = 0;
float[] cx = new float[256];             // the swept input value
float[] c0 = new float[256], c1 = new float[256];   // and the two outputs there
int    nDemo = 0;
float[] dx = new float[64], d0 = new float[64], d1 = new float[64];
float  liveX = 0, live0 = 0, live1 = 0;
boolean haveLive = false;
String  message = "connecting...";

// A short trail of recent live points, so movement reads as movement.
int TRAIL = 90;
float[] tx = new float[TRAIL], ty = new float[TRAIL];
int tn = 0, thead = 0;

boolean scopeView = false;

// The point you last clicked: what the next SPACE will teach.
float aim0 = 0.5, aim1 = 0.5;

// ---- colours, defined once ------------------------------------------------
color BG     = #14161A;
color INK    = #E8E8E8;
color FAINT  = #3A3F47;
color OUT0   = #4FC3F7;                  // first output: blue
color OUT1   = #FFB74D;                  // second output: orange
color DEMO   = #66BB6A;                  // your demonstrations: green
color LIVE   = #EF5350;                  // where you are now: red

void setup() {
  size(1000, 620);
  surface.setTitle("iris_scope");
  textFont(createFont("Menlo", 13));
  openPort(bestGuessPort());
}

// Prefer something that looks like a board rather than a Bluetooth device.
int bestGuessPort() {
  portNames = Serial.list();
  for (int i = 0; i < portNames.length; i++) {
    String n = portNames[i].toLowerCase();
    if ((n.contains("usbmodem") || n.contains("usbserial") || n.contains("acm"))
        && !n.contains("bluetooth")) return i;
  }
  return 0;
}

void openPort(int idx) {
  portNames = Serial.list();
  if (portNames.length == 0) { message = "no serial ports found"; connected = false; return; }
  portIndex = constrain(idx, 0, portNames.length - 1);
  if (port != null) { port.stop(); port = null; }
  try {
    port = new Serial(this, portNames[portIndex], 115200);
    port.bufferUntil('\n');
    connected = true;
    message = "listening on " + portNames[portIndex];
  } catch (Exception e) {
    connected = false;
    message = "could not open " + portNames[portIndex] + " (is Serial Monitor still open?)";
  }
}

// ---- reading the board ----------------------------------------------------
void serialEvent(Serial p) {
  String line = p.readStringUntil('\n');
  if (line == null) return;
  line = trim(line);
  if (line.length() < 2) return;
  String[] f = splitTokens(line, " ");

  if (f[0].equals("R") && f.length >= 3) {
    lo = float(f[1]); hi = float(f[2]);
  }
  else if (f[0].equals("D") && f.length >= 5) {
    int i = int(f[1]);
    if (i >= 0 && i < dx.length) {
      dx[i] = float(f[2]); d0[i] = float(f[3]); d1[i] = float(f[4]);
      nDemo = max(nDemo, i + 1);
    }
  }
  else if (f[0].equals("C") && f.length >= 2) {
    int n = int(f[1]);
    n = min(n, cx.length);
    // Each point is three numbers: input, out0, out1.
    if (f.length >= 2 + n * 3) {
      for (int i = 0; i < n; i++) {
        cx[i] = float(f[2 + i*3]);
        c0[i] = float(f[3 + i*3]);
        c1[i] = float(f[4 + i*3]);
      }
      nCurve = n;
    }
  }
  else if (f[0].equals("L") && f.length >= 4) {
    liveX = float(f[1]); live0 = float(f[2]); live1 = float(f[3]);
    haveLive = true;
    tx[thead] = live0; ty[thead] = live1;
    thead = (thead + 1) % TRAIL;
    if (tn < TRAIL) tn++;
  }
  else if (f[0].equals("M")) {
    message = line.substring(2);
    // A clear on the board must clear the picture too, or you are looking at
    // an instrument that no longer exists.
    if (message.startsWith("cleared")) { nDemo = 0; nCurve = 0; tn = 0; haveLive = false; }
  }
}

// ---- drawing --------------------------------------------------------------
void draw() {
  background(BG);
  if (scopeView) drawScope(); else drawTransfer();
  drawChrome();
}

float PADL = 70, PADR = 40, PADT = 70, PADB = 70;

float px(float v, float a, float b) { return map(v, a, b, PADL, width - PADR); }
float py(float v, float a, float b) { return map(v, a, b, height - PADB, PADT); }

void drawTransfer() {
  // axes
  stroke(FAINT); strokeWeight(1);
  line(PADL, PADT, PADL, height - PADB);
  line(PADL, height - PADB, width - PADR, height - PADB);
  fill(FAINT); textAlign(CENTER, TOP);
  text("your sensor  →", (PADL + width - PADR) / 2, height - PADB + 26);
  pushMatrix();
  translate(PADL - 44, (PADT + height - PADB) / 2);
  rotate(-HALF_PI); textAlign(CENTER, BOTTOM);
  text("what it plays", 0, 0);
  popMatrix();
  textAlign(RIGHT, CENTER);
  text("1.0", PADL - 10, py(1, 0, 1));
  text("0.0", PADL - 10, py(0, 0, 1));

  if (nCurve < 2) { hintEmpty(); return; }

  // the curves the network invented
  noFill(); strokeWeight(2.5);
  stroke(OUT0);
  beginShape();
  for (int i = 0; i < nCurve; i++) vertex(px(cx[i], lo, hi), py(c0[i], 0, 1));
  endShape();
  stroke(OUT1);
  beginShape();
  for (int i = 0; i < nCurve; i++) vertex(px(cx[i], lo, hi), py(c1[i], 0, 1));
  endShape();

  // your demonstrations
  for (int i = 0; i < nDemo; i++) {
    float X = px(dx[i], lo, hi);
    stroke(DEMO); strokeWeight(1); 
    line(X, PADT, X, height - PADB);
    noStroke(); fill(DEMO);
    ellipse(X, py(d0[i], 0, 1), 11, 11);
    ellipse(X, py(d1[i], 0, 1), 11, 11);
  }

  // where you are right now
  if (haveLive) {
    float X = px(liveX, lo, hi);
    stroke(LIVE); strokeWeight(1.5);
    line(X, PADT, X, height - PADB);
    noStroke(); fill(LIVE);
    ellipse(X, py(live0, 0, 1), 9, 9);
    ellipse(X, py(live1, 0, 1), 9, 9);
  }

  // the crosshair: what the next SPACE will teach
  drawAim(px(haveLive ? liveX : (lo+hi)/2, lo, hi), py(aim0, 0, 1), py(aim1, 0, 1));

  noStroke(); textAlign(LEFT, TOP);
  fill(DEMO); text("●  the " + nDemo + " poses you taught it", PADL, PADT - 46);
  fill(OUT0); text("—  everything between them is invented", PADL, PADT - 28);
}

// Two small rings showing where the next demonstration will land.
void drawAim(float X, float Y0, float Y1) {
  noFill(); stroke(INK, 150); strokeWeight(1.5);
  ellipse(X, Y0, 16, 16); ellipse(X, Y1, 16, 16);
  fill(INK, 150); noStroke(); textAlign(LEFT, CENTER);
  text("SPACE teaches here", X + 14, Y0 - 14);
}

void drawScope() {
  float side = min(width - PADL - PADR, height - PADT - PADB);
  float cxo = (width - side) / 2, cyo = PADT;

  stroke(FAINT); strokeWeight(1); noFill();
  rect(cxo, cyo, side, side);
  fill(FAINT); textAlign(CENTER, TOP);
  text("output 1  →", cxo + side / 2, cyo + side + 26);
  pushMatrix();
  translate(cxo - 24, cyo + side / 2); rotate(-HALF_PI);
  textAlign(CENTER, BOTTOM); text("output 2", 0, 0);
  popMatrix();

  if (nCurve < 2) { hintEmpty(); return; }

  // the whole mapping as one shape
  noFill(); strokeWeight(2.5); stroke(OUT0);
  beginShape();
  for (int i = 0; i < nCurve; i++)
    vertex(cxo + c0[i] * side, cyo + (1 - c1[i]) * side);
  endShape();

  // your demonstrations sit ON that shape
  noStroke(); fill(DEMO);
  for (int i = 0; i < nDemo; i++)
    ellipse(cxo + d0[i] * side, cyo + (1 - d1[i]) * side, 11, 11);

  // the recent path, fading
  noFill(); strokeWeight(2);
  for (int j = 1; j < tn; j++) {
    int a = (thead - tn + j - 1 + TRAIL * 2) % TRAIL;
    int b = (thead - tn + j     + TRAIL * 2) % TRAIL;
    stroke(LIVE, map(j, 0, tn, 20, 180));
    line(cxo + tx[a] * side, cyo + (1 - ty[a]) * side,
         cxo + tx[b] * side, cyo + (1 - ty[b]) * side);
  }
  if (haveLive) {
    noStroke(); fill(LIVE);
    ellipse(cxo + live0 * side, cyo + (1 - live1) * side, 13, 13);
  }

  // the crosshair, in output space where clicking makes the most sense
  float aX = cxo + aim0 * side, aY = cyo + (1 - aim1) * side;
  noFill(); stroke(INK, 150); strokeWeight(1.5);
  ellipse(aX, aY, 18, 18);
  line(aX - 13, aY, aX + 13, aY); line(aX, aY - 13, aX, aY + 13);
  fill(INK, 150); noStroke(); textAlign(LEFT, CENTER);
  text("click to move · SPACE teaches here", aX + 16, aY - 16);

  noStroke(); textAlign(LEFT, TOP);
  fill(INK); text("one input sweeping its range draws this shape", cxo, cyo - 46);
  fill(FAINT); text("straight demonstrations, curved shape — that curve is the network", cxo, cyo - 28);
}

void hintEmpty() {
  fill(FAINT); textAlign(CENTER, CENTER);
  text("Click where you want this pose to land, then press SPACE.", width/2, height/2 - 12);
  text("Move the sensor, click somewhere else, press SPACE again.", width/2, height/2 + 12);
  textAlign(LEFT, BASELINE);
}

/* Clicking sets what the NEXT demonstration means. The board owns the
   instrument, so we only ever send it the number -- we never keep a second
   copy of the mapping over here that could drift out of step with it. */
void mousePressed() {
  if (scopeView) {
    float side = min(width - PADL - PADR, height - PADT - PADB);
    float cxo = (width - side) / 2, cyo = PADT;
    aim0 = constrain((mouseX - cxo) / side, 0, 1);
    aim1 = constrain(1 - (mouseY - cyo) / side, 0, 1);
  } else {
    float v = constrain(map(mouseY, height - PADB, PADT, 0, 1), 0, 1);
    // Upper half of the plot sets output 1, lower half sets output 2.
    if (v > (aim0 + aim1) / 2) aim0 = v; else aim1 = v;
  }
  if (port != null) port.write("T " + nf(aim0, 1, 4) + " " + nf(aim1, 1, 4) + "\n");
}

void drawChrome() {
  noStroke(); fill(INK); textAlign(LEFT, TOP);
  text(scopeView ? "SCOPE" : "TRANSFER", 18, 16);
  fill(FAINT);
  text("TAB view    SPACE teach    d delete    c clear    s save    ←→ port", 110, 16);
  fill(connected ? INK : LIVE); textAlign(RIGHT, TOP);
  text(message, width - 18, 16);
  fill(FAINT); textAlign(LEFT, BOTTOM);
  text("input range seen: " + nf(lo, 0, 2) + " to " + nf(hi, 0, 2), 18, height - 14);
}

// ---- keys -----------------------------------------------------------------
void keyPressed() {
  if (key == TAB) { scopeView = !scopeView; return; }
  if (key == 's') { saveFrame("iris_scope-####.png"); message = "saved a PNG"; return; }
  if (keyCode == LEFT)  { openPort(portIndex - 1); return; }
  if (keyCode == RIGHT) { openPort(portIndex + 1); return; }
  // Everything else goes straight to the board, which owns the instrument.
  if (port != null && (key == ' ' || key == 'd' || key == 'c')) port.write(key);
}
