#include "rosic_AcidPattern.h"
#include <assert.h>
#include <ctime>

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

void
AcidPattern::randomize(int patternNr)
{
  // Use the time as a seed for the random number generator
  std::srand(static_cast<unsigned int>(std::time(nullptr)));

  assert(patternNr >= 0 && patternNr <= 24);

  // Create rhythm
  bool rhythm[16];
  int nrUpInRow = 0;
  int nrLowInRow = 0;
  int maxUpInRow = 3;
  int maxLowInRow = 2;

  for (int i = 0; i < 16; i++)
  {
    rhythm[i] = std::rand() % 100 < 50;
    if (rhythm[i])  // up
    {
      nrUpInRow += 1;
      nrLowInRow = 0;
      if (nrUpInRow > maxUpInRow)
      {
        rhythm[i] = false;
        nrUpInRow = 0;
        nrLowInRow = 1;
      }
    }
    else
    // down
    {
      nrLowInRow += 1;
      nrUpInRow = 0;
      if (nrLowInRow > maxLowInRow)
      {
        rhythm[i] = true;
        nrLowInRow = 0;
        nrUpInRow = 1;
      }
    }
  }

  //  // Print the rhythm (optional)
  //  std::string pattern;
  //  for (bool beat : rhythm)
  //  {
  //    pattern += (beat ? "# " : ". ");
  //  }
  //#ifdef _DEBUG
  //  OutputDebugString(pattern.c_str());
  //  OutputDebugString("\n");
  //#endif


  const int kNoteNrForC0 = 24;  // The midi note number for C0
  const int kNoteNrForC1 = 36;  // The midi note number for C1
  const int kNoteNrForC2 = 48;  // The midi note number for C2
  const int kNoteNrForC3 = 60;  // The midi note number for C3

  int baseNote =
      (patternNr % 24) +
      kNoteNrForC0;  // Let the current pattern be the base note for the randomized pattern
  int i = 0;
  int octave = -1;
  const int kNrOf16ths = 16;  // * 8; Maybe we want to create longer loops in the future.
  const int kKvint = 7;       // Kvint is the swedish word for "fifth".

  while (i < kNrOf16ths)
  {
    int playedNote = baseNote;
    octave = -1;
    if (rhythm[i % 16])  // If we want play the base note.
    {
      playedNote += 12;
      if (playedNote + kKvint <= kNoteNrForC3)
      {
        playedNote += (std::rand() % 2) == 1 ? kKvint : 0;
      }
    }

    if (i < 16)
    {
      if (playedNote < kNoteNrForC1)
      {
        octave = -1;
      }
      else if (playedNote < kNoteNrForC2)
      {
        octave = 0;
      }
      else if (playedNote <= kNoteNrForC3)
      {
        octave = 1;
      }
      else
      {
        assert(false);
      }
    }
    //else if (i < 32)
    //{
    //  mPattern[i][playedNote + baseNote + 7] = true;
    //}

    notes[i].key = playedNote % 12;
    notes[i].octave = octave;
#ifdef _WIN32
    notes[i].accent = roundToInt(randomUniform(0, 1, rand())) == 1;
    notes[i].slide = roundToInt(randomUniform(0, 5, rand())) == 4;
    notes[i].gate = roundToInt(randomUniform(0, 11, rand())) < 9;
#else
    notes[i].accent = (std::rand() % 100) < 30;  // 30%
    notes[i].slide = (std::rand() % 100) < 15;   // 15%
    notes[i].gate = (std::rand() % 100) < 75;    // 75%
#endif

    // Increase step
    i++;
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
