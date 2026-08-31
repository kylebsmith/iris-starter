/* iris_scope — watch a network learn
   ==================================
   Run iris_scope.ino on the board, then run this. LEFT and RIGHT arrows pick
   the serial port if it does not connect by itself.

   WHAT YOU ARE LOOKING AT

     THE BIG PLOT is the whole instrument. Left to right is your sensor. Bottom
     to top is what it plays. The green dots are the poses YOU taught it. The
     lines through them are what the network made up in between — and that
     made-up part is the instrument.

     THE SQUARE, top right, plots the two outputs against each other, the way
     an oscilloscope in X-Y mode plots two voltages. Both panels are on screen
     at once on purpose: move your mouse over either and the matching point
     lights up in the other.

     THE NUMBERS down the left are every value in the system, live. Nothing is
     hidden behind a keypress.

   HOW TO USE IT

     Drag the two round handles to say what this pose should mean. Press SPACE.
     Move the sensor somewhere clearly different and do it again. Two poses is
     enough to see a curve.

     You cannot break it. Press 'c' and start over whenever you like.

   WHY IT LOOKS LIKE THIS
     Every action has a visible effect, immediately (Victor, "Learnable
     Programming", 2012). The horizontal axis never rescales, because a
     silently rescaling axis makes values unreadable even when animated (Heer &
     Robertson, IEEE InfoVis 2007); the part of the range you have visited is
     drawn as a band instead. The two panels are shown together rather than
     swapped, because correspondence has to be seen, not remembered (Becker &
     Cleveland, Technometrics 1987).
   ========================================================================= */

import processing.serial.*;

// ---- regions. Every coordinate in this file comes from one of these. -------
class Rect {
  float x0, y0, x1, y1;
  Rect(float a, float b, float c, float d) { x0=a; y0=b; x1=c; y1=d; }
  float w() { return x1 - x0; }
  float h() { return y1 - y0; }
  float midX() { return (x0 + x1) * 0.5; }
  float midY() { return (y0 + y1) * 0.5; }
}
Rect HEAD, READ, XFER, PLOT, SQUARE, FOOT;

// ---- colours, defined once ------------------------------------------------
color BG    = #14161A;
color INK   = #E8E8E8;
color DIM   = #8A929E;
color FAINT = #2C313A;
color OUT0  = #4FC3F7;   // output one   — blue
color OUT1  = #FFB74D;   // output two   — orange
color DEMO  = #66BB6A;   // your poses   — green
color LIVE  = #EF5350;   // where you are — red
color BAND  = #1E232B;   // the visited part of the range

// ---- what the board has told us -------------------------------------------
float fullLo = -10, fullHi = 10;     // the sensor's PHYSICAL range. Never changes.
String unit = "";
float seenLo = 0, seenHi = 0;        // the part visited. Drawn, never scaled to.
boolean haveSeen = false;

int nCurve = 0;
float[] cx = new float[128], c0 = new float[128], c1 = new float[128];
int nDemo = 0;
float[] dx = new float[64], d0 = new float[64], d1 = new float[64];

float liveX = 0, live0 = 0, live1 = 0;
boolean haveInput = false, haveOut = false;

float aim0 = 0.75, aim1 = 0.25;      // what the next SPACE will teach
float ack0 = -1,   ack1 = -1;        // what the BOARD says it has
int   ackFlash = 0;

String message = "connecting…";
String state   = "";

// click feedback
float clickX = -1, clickY = -1;
int   clickAge = 999;
int   dragging = -1;                 // 0 or 1 while a handle is held

Serial port;
String[] portNames;
int portIndex = 0;
boolean connected = false;

PFont fBig, fMid, fSmall;

void setup() {
  size(1000, 620);
  surface.setTitle("iris_scope — watch a network learn");
  fBig   = createFont("Menlo", 21);
  fMid   = createFont("Menlo", 14);
  fSmall = createFont("Menlo", 11);

  HEAD   = new Rect(  0,   0, 1000,  40);
  READ   = new Rect(  0,  40,  248, 578);
  XFER   = new Rect(248,  40,  688, 578);
  PLOT   = new Rect(300,  92,  664, 520);      // the drawable rectangle inside XFER
  SQUARE = new Rect(716,  92,  976, 352);
  FOOT   = new Rect(  0, 578, 1000, 620);

  openPort(bestGuessPort());
}

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
    message = "could not open " + portNames[portIndex] + " — is the Serial Monitor still open?";
  }
}

// ---- reading the board ----------------------------------------------------
void serialEvent(Serial p) {
  String line = p.readStringUntil('\n');
  if (line == null) return;
  line = trim(line);
  if (line.length() < 2) return;
  String[] f = splitTokens(line, " ");

  if (f[0].equals("X") && f.length >= 3) {          // the physical full scale
    fullLo = float(f[1]); fullHi = float(f[2]);
    if (f.length >= 4) unit = f[3];
  }
  else if (f[0].equals("R") && f.length >= 3) {     // the part visited
    seenLo = float(f[1]); seenHi = float(f[2]); haveSeen = true;
  }
  else if (f[0].equals("D") && f.length >= 5) {
    int i = int(f[1]);
    if (i >= 0 && i < dx.length) {
      dx[i] = float(f[2]); d0[i] = float(f[3]); d1[i] = float(f[4]);
      nDemo = max(nDemo, i + 1);
    }
  }
  else if (f[0].equals("C") && f.length >= 2) {
    int n = min(int(f[1]), cx.length);
    if (f.length >= 2 + n * 3) {
      for (int i = 0; i < n; i++) {
        cx[i] = float(f[2 + i*3]); c0[i] = float(f[3 + i*3]); c1[i] = float(f[4 + i*3]);
      }
      nCurve = n;
    }
  }
  else if (f[0].equals("L")) {                      // live — 2 tokens or 4
    if (f.length >= 2) { liveX = float(f[1]); haveInput = true; }
    if (f.length >= 4) { live0 = float(f[2]); live1 = float(f[3]); haveOut = true; }
  }
  else if (f[0].equals("A") && f.length >= 3) {     // the board has our target
    ack0 = float(f[1]); ack1 = float(f[2]); ackFlash = 12;
  }
  else if (f[0].equals("M")) {
    message = line.substring(2);
    if (message.startsWith("cleared")) {
      nDemo = 0; nCurve = 0; haveOut = false; haveSeen = false;
    }
  }
}

// ---- mapping. px() CLAMPS: nothing is ever drawn outside its own plot. -----
float px(float v)      { return constrain(map(v, fullLo, fullHi, PLOT.x0, PLOT.x1), PLOT.x0, PLOT.x1); }
float py(float v)      { return constrain(map(v, 0, 1, PLOT.y1, PLOT.y0), PLOT.y0, PLOT.y1); }
float sqx(float v)     { return SQUARE.x0 + constrain(v, 0, 1) * SQUARE.w(); }
float sqy(float v)     { return SQUARE.y1 - constrain(v, 0, 1) * SQUARE.h(); }

void draw() {
  background(BG);
  if (clickAge < 999) clickAge++;
  if (ackFlash > 0) ackFlash--;
  drawHead();
  drawReadout();
  drawTransfer();
  drawSquare();
  drawFoot();
  drawClickMark();
}

// ---------------------------------------------------------------------------
void drawHead() {
  textFont(fMid);
  state = nDemo == 0 ? "move the sensor — nothing is being learned yet"
        : nDemo == 1 ? "one pose taught. Move somewhere different and teach another."
        : "playing — move the sensor and watch the red marker";
  noStroke(); fill(INK); textAlign(LEFT, CENTER);
  text(state, 18, HEAD.midY());
  fill(connected ? DIM : LIVE); textAlign(RIGHT, CENTER);
  text(message, width - 18, HEAD.midY());
}

// One numeral block. Fixed field widths so digits never move the layout.
void valueBlock(float y, String label, String value, color c) {
  noStroke(); textAlign(LEFT, TOP);
  textFont(fSmall); fill(DIM);  text(label, 18, y);
  textFont(fBig);   fill(c);    text(value, 18, y + 15);
}
String sf(float v) { return (v < 0 ? "-" : "+") + nf(abs(v), 1, 2); }

void drawReadout() {
  stroke(FAINT); strokeWeight(1);
  line(READ.x1, READ.y0 + 8, READ.x1, READ.y1 - 8);

  valueBlock( 56, "SENSOR" + (unit.equals("") ? "" : "  (" + unit + ")"),
              haveInput ? sf(liveX) : "  —  ", LIVE);
  valueBlock(114, "OUTPUT ONE", haveOut ? nf(live0, 1, 3) : "  —  ", OUT0);
  valueBlock(172, "OUTPUT TWO", haveOut ? nf(live1, 1, 3) : "  —  ", OUT1);

  stroke(FAINT); line(18, 236, READ.x1 - 18, 236);

  valueBlock(250, "SPACE WILL TEACH", nf(aim0, 1, 2) + " / " + nf(aim1, 1, 2), INK);
  valueBlock(308, "POSES TAUGHT", nf(nDemo, 1, 0), DEMO);
  valueBlock(366, "RANGE YOU HAVE USED",
             haveSeen ? sf(seenLo) + " " + sf(seenHi) : "  —  ", DIM);

  // what the board says it has, so a click is confirmed end to end
  textFont(fSmall); textAlign(LEFT, TOP); noStroke();
  fill(ackFlash > 0 ? DEMO : DIM);
  text(ack0 < 0 ? "board: waiting" : "board has " + nf(ack0,1,2) + " / " + nf(ack1,1,2),
       18, 430);
}

void drawTransfer() {
  // the visited band, drawn BEFORE the axes so it reads as ground
  if (haveSeen && seenHi > seenLo) {
    noStroke(); fill(BAND);
    rect(px(seenLo), PLOT.y0, px(seenHi) - px(seenLo), PLOT.h());
  }

  // gridlines at fixed, labelled positions. These never move.
  textFont(fSmall); textAlign(CENTER, TOP);
  for (int i = 0; i <= 4; i++) {
    float v = lerp(fullLo, fullHi, i / 4.0), X = px(v);
    stroke(FAINT); strokeWeight(1); line(X, PLOT.y0, X, PLOT.y1);
    noStroke(); fill(DIM); text(nf(v, 1, 1), X, PLOT.y1 + 8);
  }
  for (int i = 0; i <= 2; i++) {
    float v = i / 2.0, Y = py(v);
    stroke(FAINT); line(PLOT.x0, Y, PLOT.x1, Y);
    noStroke(); fill(DIM); textAlign(RIGHT, CENTER); text(nf(v, 1, 1), PLOT.x0 - 10, Y);
  }
  noFill(); stroke(FAINT); strokeWeight(1); rect(PLOT.x0, PLOT.y0, PLOT.w(), PLOT.h());

  // axis names, outside the plot, never over anything that moves
  noStroke(); fill(DIM); textFont(fSmall); textAlign(CENTER, TOP);
  text("your sensor" + (unit.equals("") ? "" : ", " + unit) + "  — the whole range it can read",
       PLOT.midX(), PLOT.y1 + 26);
  pushMatrix(); translate(PLOT.x0 - 42, (PLOT.y0 + PLOT.y1) / 2); rotate(-HALF_PI);
  textAlign(CENTER, BOTTOM); text("what it plays", 0, 0); popMatrix();

  if (haveSeen && seenHi > seenLo) {
    noStroke(); fill(DIM); textAlign(CENTER, BOTTOM); textFont(fSmall);
    text("the part you have actually used", (px(seenLo) + px(seenHi)) / 2, PLOT.y0 - 6);
  }

  // the curves the network invented
  if (nCurve >= 2) {
    noFill(); strokeWeight(2.5);
    stroke(OUT0); beginShape(); for (int i=0;i<nCurve;i++) vertex(px(cx[i]), py(c0[i])); endShape();
    stroke(OUT1); beginShape(); for (int i=0;i<nCurve;i++) vertex(px(cx[i]), py(c1[i])); endShape();
    // direct labels on the lines themselves, not a legend
    noStroke(); textFont(fSmall); textAlign(LEFT, CENTER);
    fill(OUT0); text("output one", px(cx[nCurve-1]) + 6, py(c0[nCurve-1]));
    fill(OUT1); text("output two", px(cx[nCurve-1]) + 6, py(c1[nCurve-1]));
  }

  // your poses
  for (int i = 0; i < nDemo; i++) {
    float X = px(dx[i]);
    stroke(DEMO, 90); strokeWeight(1); line(X, PLOT.y0, X, PLOT.y1);
    noStroke(); fill(BG); ellipse(X, py(d0[i]), 15, 15); ellipse(X, py(d1[i]), 15, 15);
    fill(DEMO);          ellipse(X, py(d0[i]), 11, 11); ellipse(X, py(d1[i]), 11, 11);
  }

  // where you are now
  if (haveInput) {
    float X = px(liveX);
    stroke(LIVE); strokeWeight(1.5); line(X, PLOT.y0, X, PLOT.y1);
    if (haveOut) {
      noStroke(); fill(LIVE);
      ellipse(X, py(live0), 10, 10); ellipse(X, py(live1), 10, 10);
    }
    // the label rides above the plot in its own rail, never over the curves
    noStroke(); fill(LIVE); textFont(fSmall);
    float lx = constrain(X, PLOT.x0 + 34, PLOT.x1 - 34);
    textAlign(CENTER, BOTTOM); text("you are here", lx, PLOT.y0 - 22);
  }

  drawHandles();
}

/* The two target handles. Each is named, each keeps its own colour, and each
   is dragged directly — no guessing which one you meant. */
void drawHandles() {
  float hx = PLOT.x1 + 18;
  for (int i = 0; i < 2; i++) {
    float v = (i == 0) ? aim0 : aim1;
    color c = (i == 0) ? OUT0 : OUT1;
    float Y = py(v);
    stroke(c, 70); strokeWeight(1);
    for (float x = PLOT.x0; x < PLOT.x1; x += 8) line(x, Y, x + 4, Y);   // dashed
    noFill(); stroke(c); strokeWeight(dragging == i ? 3 : 2);
    ellipse(hx, Y, dragging == i ? 24 : 18, dragging == i ? 24 : 18);
  }
  noStroke(); fill(DIM); textFont(fSmall); textAlign(CENTER, TOP);
  text("drag", hx, PLOT.y1 + 8);
}

void drawSquare() {
  noFill(); stroke(FAINT); strokeWeight(1);
  rect(SQUARE.x0, SQUARE.y0, SQUARE.w(), SQUARE.h());
  noStroke(); fill(DIM); textFont(fSmall);
  textAlign(CENTER, TOP);   text("output one →", SQUARE.midX(), SQUARE.y1 + 8);
  textAlign(CENTER, BOTTOM);
  pushMatrix(); translate(SQUARE.x0 - 12, (SQUARE.y0 + SQUARE.y1)/2); rotate(-HALF_PI);
  text("output two", 0, 0); popMatrix();
  textAlign(LEFT, BOTTOM); fill(DIM);
  text("the two outputs against each other", SQUARE.x0, SQUARE.y0 - 8);

  if (nCurve >= 2) {
    noFill(); stroke(OUT0, 170); strokeWeight(2);
    beginShape(); for (int i=0;i<nCurve;i++) vertex(sqx(c0[i]), sqy(c1[i])); endShape();
  }
  noStroke(); fill(DEMO);
  for (int i = 0; i < nDemo; i++) ellipse(sqx(d0[i]), sqy(d1[i]), 10, 10);
  if (haveOut) { fill(LIVE); ellipse(sqx(live0), sqy(live1), 12, 12); }

  // the same numbers again, beside the mark they describe
  if (haveOut) {
    noStroke(); fill(DIM); textFont(fSmall); textAlign(LEFT, TOP);
    text(nf(live0,1,3) + " , " + nf(live1,1,3), SQUARE.x0, SQUARE.y1 + 28);
  }
}

void drawFoot() {
  noStroke(); fill(DIM); textFont(fSmall); textAlign(LEFT, CENTER);
  text("drag a handle, then SPACE to teach this pose      d delete the last      c clear everything"
     + "      s save a picture      ← → serial port", 18, FOOT.midY());
}

/* Every click leaves a mark exactly where it landed, in the same frame.
   A click that lands somewhere meaningless is still not a no-op. */
void drawClickMark() {
  if (clickAge > 40 || clickX < 0) return;
  float a = map(clickAge, 0, 40, 200, 0);
  stroke(INK, a); strokeWeight(1.5); noFill();
  ellipse(clickX, clickY, 10 + clickAge * 1.5, 10 + clickAge * 1.5);
  line(clickX - 7, clickY, clickX + 7, clickY);
  line(clickX, clickY - 7, clickX, clickY + 7);
}

// ---- input ----------------------------------------------------------------
void mousePressed() {
  clickX = mouseX; clickY = mouseY; clickAge = 0;
  float hx = PLOT.x1 + 18;
  float d0y = py(aim0), d1y = py(aim1);
  if (abs(mouseX - hx) < 40 || (mouseX > PLOT.x0 && mouseX < PLOT.x1)) {
    dragging = (abs(mouseY - d0y) <= abs(mouseY - d1y)) ? 0 : 1;
    setAim(dragging, map(mouseY, PLOT.y1, PLOT.y0, 0, 1));
  }
}
void mouseDragged() { if (dragging >= 0) setAim(dragging, map(mouseY, PLOT.y1, PLOT.y0, 0, 1)); }
void mouseReleased() { dragging = -1; }

void setAim(int which, float v) {
  v = constrain(v, 0, 1);
  if (which == 0) aim0 = v; else aim1 = v;
  if (port != null) port.write("T " + nf(aim0, 1, 4) + " " + nf(aim1, 1, 4) + "\n");
}

void keyPressed() {
  if (key == 's') { saveFrame("iris_scope-####.png"); message = "saved a picture"; return; }
  if (keyCode == LEFT)  { openPort(portIndex - 1); return; }
  if (keyCode == RIGHT) { openPort(portIndex + 1); return; }
  if (port != null && (key == ' ' || key == 'd' || key == 'c')) port.write(key);
}
