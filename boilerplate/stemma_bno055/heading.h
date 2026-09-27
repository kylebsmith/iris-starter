/* heading.h -- which way the board points, as six numbers that never wrap.
   =========================================================================
   Gravity says which way is down in the board's own frame, and so how it is
   tilted, but a turn about the vertical leaves gravity where it was: gravity
   cannot see heading. Heading as an angle has two faults of its own. It
   wraps, from 359 degrees to 0, so two poses a degree apart arrive at
   opposite ends of its range, and it has no value at all when the board
   points straight up. Two directions in the board's frame have neither
   fault:

     UP     the gravity vector as the BNO055 reports it, scaled to length 1
     FRONT  a horizontal direction the player chooses as "this way is
            front", seen from the board: turn the board and FRONT turns the
            other way in the board's frame

   Each is three numbers that move smoothly however the board turns, so the
   six together say the whole orientation, a turn about the vertical moves
   FRONT, and no pose sits at the end of a range. iris wants exactly that: a
   network cannot fit a jump from 359 to 0 that is not there in the music.

   FRONT comes from the BNO055's fusion quaternion, which says how the board
   is turned from the Earth's frame. heading_set_front takes the direction
   the board's own x axis points now, lays it flat, and keeps it as FRONT in
   the Earth's frame; heading_read turns it back into the board's frame on
   every reading. Until the player sets FRONT it is the Earth frame's x axis,
   which the BNO055's fusion ties to magnetic north once its magnetometer is
   calibrated (Adafruit_BNO055::getCalibration reports how far it is).

   Arithmetic: v' = q v q* turns a vector by the quaternion q, which the
   Adafruit library's rotateVector does; q* is q with its vector part
   negated (conjugate), and turns the other way. */
#pragma once
#include <Adafruit_BNO055.h>

struct Heading {
  imu::Vector<3> front = imu::Vector<3>(1.0, 0.0, 0.0);   /* in the Earth's frame */
};

/* Makes the direction the board's x axis points now, laid flat, the FRONT.
   Returns false, and keeps the FRONT it had, when the x axis points so near
   straight up or down that laid flat it has no direction left: turn the
   board and try again. */
static inline bool heading_set_front(Heading &h, Adafruit_BNO055 &bno) {
  imu::Quaternion q = bno.getQuat();
  imu::Vector<3> x = q.rotateVector(imu::Vector<3>(1.0, 0.0, 0.0));   /* in the Earth's frame */
  x.z() = 0.0;                                                        /* laid flat */
  const double n = x.magnitude();
  if (n < 0.3) return false;
  h.front = x / n;
  return true;
}

/* UP into in[0..2] and FRONT into in[3..5], both in the board's frame. */
static inline void heading_read(const Heading &h, Adafruit_BNO055 &bno, float *in) {
  imu::Vector<3> g = bno.getVector(Adafruit_BNO055::VECTOR_GRAVITY);
  const double n = g.magnitude();
  for (int i = 0; i < 3; ++i) in[i] = n > 0.1 ? (float)(g[i] / n) : 0.0f;
  imu::Vector<3> f = bno.getQuat().conjugate().rotateVector(h.front);
  for (int i = 0; i < 3; ++i) in[3 + i] = (float)f[i];
}
