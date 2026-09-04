#include "rosic_AcidPattern.h"
#include <assert.h>
#include <chrono>
#include <cstdint>

using namespace rosic;

AcidPattern::AcidPattern()
{
  numSteps = 16;
  stepLength = 0.5;
}

//-------------------------------------------------------------------------------------------------
// setup:

void
AcidPattern::clear(int patternNr)
{
  for (int i = 0; i < maxNumSteps; i++)
  {
    notes[i].key = (24 + patternNr) % 12;
    notes[i].octave = patternNr < 12 ? -1 : 0;
    notes[i].accent = false;
    notes[i].slide = false;
    notes[i].gate = true;
  }
}

namespace
{

/** A small self-contained generator (xorshift32).

  The previous code called std::srand(std::time(nullptr)) on entry, which had
  two problems: time() only advances once a second, so pressing Randomize twice
  in quick succession produced the *same* pattern (and randomizeAllPatterns()
  gave 24 nearly identical ones), and reseeding the global std::rand() state on
  every call is rude to everything else in the process. */
class AcidRng
{
public:
  unsigned int next()
  {
    mState ^= mState << 13;
    mState ^= mState >> 17;
    mState ^= mState << 5;
    return mState;
  }

  /** 0 .. n-1 */
  int below(int n)
  {
    return (n > 0) ? static_cast<int>(next() % static_cast<unsigned int>(n)) : 0;
  }

  /** lo .. hi, inclusive. */
  int range(int lo, int hi) { return (hi > lo) ? lo + below(hi - lo + 1) : lo; }

  bool chance(int percent) { return below(100) < percent; }

  void seedOnce()
  {
    if (mSeeded)
      return;
    mSeeded = true;
    const unsigned long long t = static_cast<unsigned long long>(
        std::chrono::high_resolution_clock::now().time_since_epoch().count());
    unsigned int s = static_cast<unsigned int>(t) ^ static_cast<unsigned int>(t >> 32)
                     ^ static_cast<unsigned int>(reinterpret_cast<uintptr_t>(this));
    mState = s ? s : 0x9E3779B9u;
    for (int i = 0; i < 8; ++i)  // let the state diffuse before first use
      next();
  }

private:
  unsigned int mState = 0x9E3779B9u;
  bool mSeeded = false;
};

AcidRng &
rng()
{
  static AcidRng g;
  g.seedOnce();
  return g;
}

}  // namespace

void
AcidPattern::randomize(int patternNr)
{
  assert(patternNr >= 0 && patternNr < 24);

  AcidRng &r = rng();

  // The pattern index names the pattern (see BassMatrix::GetPatternName): the
  // pitch class is the root and the upper twelve patterns sit an octave up.
  // clear() uses the same convention.
  const int root = patternNr % 12;
  const int baseOctave = (patternNr < 12) ? -1 : 0;

  //
  // Scale
  //
  // The old generator could only ever emit two pitch classes — the root and
  // the fifth — which is the main reason every random pattern sounded like the
  // last one. Picking from a scale instead is what buys the variety.
  // Acid lives in minor territory; pentatonic is weighted highest because with
  // only five notes almost any ordering of them sounds deliberate.
  static const int kMinorPent[] = { 0, 3, 5, 7, 10 };
  static const int kNatMinor[] = { 0, 2, 3, 5, 7, 8, 10 };
  static const int kPhrygian[] = { 0, 1, 3, 5, 7, 8, 10 };
  static const int kDorian[] = { 0, 2, 3, 5, 7, 9, 10 };
  static const int kBlues[] = { 0, 3, 5, 6, 7, 10 };

  const int *scale = kMinorPent;
  int scaleLen = 5;
  switch (r.below(10))
  {
    case 0:
    case 1:
    case 2:
    case 3: scale = kMinorPent; scaleLen = 5; break;
    case 4:
    case 5: scale = kNatMinor; scaleLen = 7; break;
    case 6:
    case 7: scale = kPhrygian; scaleLen = 7; break;
    case 8: scale = kDorian; scaleLen = 7; break;
    default: scale = kBlues; scaleLen = 6; break;
  }

  //
  // Style
  //
  // Pick a character for this press, so hitting Randomize repeatedly gives
  // genuinely different kinds of line rather than one texture reshuffled.
  int gateChance, accentChance, slideChance, octaveChance, restRunMax;
  switch (r.below(4))
  {
    case 0:  // rolling — near-continuous 16ths, lots of slides
      gateChance = 88; accentChance = 22; slideChance = 40; octaveChance = 12; restRunMax = 2;
      break;
    case 1:  // sparse — room to breathe, punchy accents
      gateChance = 52; accentChance = 38; slideChance = 16; octaveChance = 22; restRunMax = 4;
      break;
    case 2:  // syncopated — driven off the beat
      gateChance = 68; accentChance = 30; slideChance = 26; octaveChance = 18; restRunMax = 3;
      break;
    default:  // stabby — mid density, big octave leaps
      gateChance = 62; accentChance = 28; slideChance = 20; octaveChance = 34; restRunMax = 3;
      break;
  }

  //
  // Motif
  //
  // The single biggest difference between "a riff" and "noise" is repetition.
  // Build a short cell and tile it across the 16 steps, mutating the odd step
  // in the later repeats so the bar develops instead of looping mechanically.
  const int cellLen = r.chance(55) ? 8 : 4;

  int degree[16];
  int octShift[16];
  bool gate[16];

  int prevDegree = 0;
  int restRun = 0;

  for (int i = 0; i < 16; ++i)
  {
    const int src = i % cellLen;
    const bool fresh = (i < cellLen) || r.chance(25);

    if (!fresh)
    {
      degree[i] = degree[src];
      octShift[i] = octShift[src];
      gate[i] = gate[src];
    }
    else
    {
      // Gate. Step 0 is the anchor of the bar and always sounds.
      bool g = (i == 0) ? true : r.chance(gateChance);
      if (!g && restRun >= restRunMax)
        g = true;  // never leave a hole big enough to lose the pulse
      gate[i] = g;

      // Pitch. Favour the root, otherwise move by a small step from the
      // previous note — a contour, rather than independent random leaps.
      int d;
      if (i == 0 || r.chance(22))
      {
        d = 0;  // root
      }
      else if (r.chance(72))
      {
        // Stepwise motion, and never standing still: repeating the previous
        // degree here is what used to collapse whole bars onto the root.
        static const int kSteps[] = { -2, -1, 1, 2 };
        d = prevDegree + kSteps[r.below(4)];
      }
      else
      {
        d = r.below(scaleLen);  // the occasional leap
      }
      if (d < 0)
        d += scaleLen;
      if (d >= scaleLen)
        d -= scaleLen;
      degree[i] = d;

      // Octave. The octave-up stab is the signature 303 move, so bias it onto
      // strong steps where it reads as a hook rather than an accident.
      int o = 0;
      if (r.chance(octaveChance))
        o = ((i % 4) == 0 || r.chance(40)) ? 1 : -1;
      octShift[i] = o;
    }

    restRun = gate[i] ? 0 : restRun + 1;
    if (gate[i])
      prevDegree = degree[i];
  }

  // If the dice produced almost nothing, put notes back on the strong beats —
  // an empty bar is never the interesting answer.
  int noteCount = 0;
  for (int i = 0; i < 16; ++i)
    noteCount += gate[i] ? 1 : 0;
  if (noteCount < 4)
  {
    for (int i = 0; i < 16; i += 4)
      gate[i] = true;
  }

  //
  // Pitches
  //
  int pitch[16];
  for (int i = 0; i < 16; ++i)
  {
    const int semi = root + scale[degree[i]];
    int oct = baseOctave + (semi / 12) + octShift[i];
    if (oct < -1)  // the editor can only show octave -1, 0 and +1
      oct = -1;
    if (oct > 1)
      oct = 1;

    notes[i].key = semi % 12;
    notes[i].octave = oct;
    notes[i].gate = gate[i];
    pitch[i] = oct * 12 + notes[i].key;
  }

  //
  // Accents
  //
  // Weighted onto the beat, with enough offbeat hits to stop it marching.
  bool anyAccent = false;
  for (int i = 0; i < 16; ++i)
  {
    if (!gate[i])
    {
      notes[i].accent = false;
      continue;
    }
    int c = ((i % 4) == 0) ? accentChance + 25 : accentChance - 8;
    if (c < 0)
      c = 0;
    notes[i].accent = r.chance(c);
    anyAccent = anyAccent || notes[i].accent;
  }
  if (!anyAccent)
    notes[0].accent = true;  // an acid line with no accent has no shape at all

  //
  // Slides
  //
  // A slide is only audible if the next step actually sounds and is a
  // different pitch, so these are decided last, once the pitches and gates are
  // known. Runs are capped because a long chain of slides just turns to mush.
  int slideRun = 0;
  for (int i = 0; i < 16; ++i)
  {
    const int nxt = (i + 1) % 16;
    const bool canSlide = gate[i] && gate[nxt] && pitch[i] != pitch[nxt];
    notes[i].slide = canSlide && slideRun < 2 && r.chance(slideChance);
    slideRun = notes[i].slide ? slideRun + 1 : 0;
  }
}

void
AcidPattern::circularShift(int numStepsToShift)
{
  rosic::circularShift(notes, maxNumSteps, numStepsToShift);
}

//-------------------------------------------------------------------------------------------------
// inquiry:

bool
AcidPattern::isEmpty() const
{
  for (int i = 0; i < maxNumSteps; i++)
  {
    if (notes[i].gate == true)
      return false;
  }
  return true;
}
