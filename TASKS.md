# Tasks

Pick what you want. Every one of these is real work that ends up in the repo,
and every one has a definite finish line so you know when you're done without
asking me.

I've marked what each one actually needs. **"None"** means none — you do not
need to have done any of this before.

---

## Week 1 — everyone, same thing

**Get the tilt sketch running and record a video of you playing it.**
*Needs: none.* Follow GET-STARTED.md. When the number follows your hand, film it.
That's the deliverable.

**Read one paper and write 200 words.** What does it claim, what did it measure,
what would you try? One paper. Not a literature review — I'll give you the list.

---

## Week 2 — the one that makes the point

**Try to install a dead tool.** Pick one from the list I'll hand out — MnM
(the Mapping is not Music toolkit for Max), GRT (the Gesture Recognition
Toolkit), an old Wekinator example patch. Try to build and run it. Write down exactly
what broke and how far you got.
*Needs: none, and stubbornness helps.*

Most of you will fail. That's the finding. These are tools that worked fine when
they were published, and the reason our library has no dependencies is sitting
in whatever error message you end up staring at.

---

## Weeks 3–4 — make it yours

Pick one. These start from `iris_tilt`, which is deliberately at its floor.

Three of these — more outputs, real MIDI (Musical Instrument Digital
Interface, the message format synthesisers understand), using the screen —
are already done in `iris_instrument`. That does not remove them from the
list. **Read that sketch, then do it yourself to `iris_tilt` without
copying it**, and you will find out how much of it you actually understood.
Doing it from scratch is the task; the working version is the answer key, and
looking at the answer key first is how you learn nothing.

**More outputs.** Three sound parameters from one gesture instead of one.
*Needs: comfort editing the sketch.* Watch out: the output count appears in
both `IRIS_ARENA(...)` and `iris_init(...)` and they must agree, and `target`
and `out` have to stop being single floats. Change one of those and not the
others and it will fail in a way that is not obvious. Done when three numbers
move independently and sensibly as you tilt.

**A different sensor.** Distance, light, flex, capacitive — anything that gives
you a number. *Needs: willingness to read a datasheet.* Done when it trains off
your sensor. Remember iris fits its range to your demonstrations, so you do not
need to scale or centre anything first.

**Show the learned space.** `iris_instrument` draws three bars. Draw the actual
surface instead — the whole two-dimensional input space coloured by what it
would output.
*Needs: some graphics interest.* Done when you can see the mapping while you
train it, and it changes how training feels.

**Break it on purpose.** Record contradictory demonstrations, feed it garbage,
unplug the sensor mid-training, record all sixteen and then try a seventeenth.
Write down everything that fails badly or fails *silently*. *Needs: none. This
is genuinely valuable and nobody wants to do it.* A button that records a
value you can neither see nor hear is the class of bug I want you hunting.

---

## Weeks 5–10 — the body and the practice

**Build the instrument.** Enclosure, mounting, how it's held, where the button
goes, what it looks like on a stand. *Needs: making things.* Done when someone
else can pick it up and play it without instructions.

**Record real gesture data.** Twenty demonstrations of a gesture, then twenty
more of the *same intended gesture* on a different day. *Needs: none.*
This one matters more than it sounds: every number in the library was measured
on clean synthetic data, and nobody has ever checked what happens with a real
hand being inconsistent the way hands are. It's the single most useful thing
anyone could hand me.

**Fix the getting-started guide.** Follow GET-STARTED.md on a fresh machine,
note every point where reality disagrees with it, and fix it. *Needs: none, and
being new is an advantage.* Honestly the highest-value job on this list — I
cannot see my own instructions with fresh eyes and you can only do it once.

**Port it somewhere else.** Raspberry Pi, a browser, a phone, Max or Pd.
*Needs: more software experience.* Done when it passes the library's own test
suite on the new target.

---

## Weeks 11–14 — play it

**Make a piece.** Whatever length. Perform it.

**Write up what happened.** What you built, what you tried, what didn't work,
what you'd do differently. Failures are more interesting than successes and I'd
rather read about the thing that didn't work.

---

## What I'm actually grading

Did you build a thing, did you play it, and can someone else follow what you
did. That's it. Nothing here is graded on how technical it is.

And I'd rather see one instrument you actually played than four you started.
