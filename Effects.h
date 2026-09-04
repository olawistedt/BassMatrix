#pragma once

#include <algorithm>  // For std::min, std::clamp
#include <utility>    // For std::pair, std::make_pair
#include <cstring>    // For memory operations
#include <cmath>      // For mathematical operations

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

template <typename T>
T
my_clamp(T value, T min, T max)
{
  return value < min ? min : (value > max ? max : value);
}

// Rest of the code remains the same...

static const int MAX_DELAY_LENGTH = 192000;  // 4 seconds at 48kHz

// From Amazon Q
class AcidStereoDelay
{
private:
  double *delayBufferL;
  double *delayBufferR;
  int writeIndex;
  int delayLengthL;
  int delayLengthR;
  double feedback;
  double dryMix;
  double wetMix;
  double sampleRate;

  // Simple low-pass filter for delay feedback
  double filterStateL;
  double filterStateR;
  double filterCoeff;

public:
  AcidStereoDelay(double sampleRate = 44100.0);
  ~AcidStereoDelay();
  void setDelayTime(double timeMs);
  void setFeedback(double fb);
  void setWetDryMix(double wet, double dry);

  // Process a single mono sample and return stereo output
  std::pair<double, double> process(double input);
};

// Simple reverb to complement the delay
class SimpleAcidReverb
{
private:
  static const int NUM_DELAYS = 8;
  double *delays[NUM_DELAYS];
  int delayLengths[NUM_DELAYS];
  int writeIndices[NUM_DELAYS];
  double feedback;
  double wetMix;
  static const int MAX_REVERB_LENGTH = 8192;

public:
  SimpleAcidReverb();
  ~SimpleAcidReverb();
  std::pair<double, double> process(double input);
};

std::pair<double, double> processDelayReverbAudioBlock(double sampleRate, double input);

//
//#include <cmath>
//#include <algorithm>
//
//class AcidTubeSaturator
//{
//private:
//  double sampleRate;
//  double drive;
//  double bias;
//  double mix;
//
//  // DC blocking filter states
//  double lastInputDC;
//  double lastOutputDC;
//
//  // Tube stage parameters
//  struct TubeStage
//  {
//    double bias;
//    double gain;
//    double shape;
//    double dcLevel;
//
//    TubeStage() : bias(0.0), gain(1.0), shape(1.0), dcLevel(0.0) {}
//  };
//
//  TubeStage stage1;
//  TubeStage stage2;
//
//public:
//  AcidTubeSaturator(double sampleRate = 44100.0) :
//    sampleRate(sampleRate),
//    drive(1.0),
//    bias(0.0),
//    mix(1.0),
//    lastInputDC(0.0),
//    lastOutputDC(0.0)
//  {
//    // Initialize first tube stage (preamp)
//    stage1.bias = 0.2;
//    stage1.gain = 1.5;
//    stage1.shape = 0.8;
//
//    // Initialize second tube stage (power amp)
//    stage2.bias = 0.1;
//    stage2.gain = 2.0;
//    stage2.shape = 0.6;
//  }
//
//  // Set the drive amount (0.0 to 1.0, will be scaled internally)
//  void setDrive(double newDrive) { drive = my_clamp(newDrive, 0.0, 1.0); }
//
//  // Set the bias (DC offset before saturation)
//  void setBias(double newBias) { bias = my_clamp(newBias, -1.0, 1.0); }
//
//  // Set wet/dry mix
//  void setMix(double newMix) { mix = my_clamp(newMix, 0.0, 1.0); }
//
//private:
//  // Asymmetric soft clipping function
//  double tubeSaturate(double input, const TubeStage &stage)
//  {
//    double x = input * stage.gain + stage.bias;
//
//    // Asymmetric wave shaping
//    if (x > 0.0)
//    {
//      x = 1.0 - expf(-x * stage.shape);
//    }
//    else
//    {
//      x = -1.0 + expf(x * stage.shape);
//    }
//
//    return x;
//  }
//
//  // DC blocking filter
//  double removeDC(double input)
//  {
//    const double dcFilterCutoff = 20.0;  // Hz
//    const double r = 1.0 - (2.0 * M_PI * dcFilterCutoff / sampleRate);
//
//    double output = input - lastInputDC + r * lastOutputDC;
//    lastInputDC = input;
//    lastOutputDC = output;
//
//    return output;
//  }
//
//public:
//  double process(double input)
//  {
//    // Input gain with drive control (0.0 to 24dB of gain)
//    double driveGain = 1.0 + (drive * drive * 15.0);
//    double inputGain = input * driveGain;
//
//    // Add bias
//    inputGain += bias;
//
//    // First tube stage (preamp)
//    double preamp = tubeSaturate(inputGain, stage1);
//
//    // Second tube stage (power amp)
//    double poweramp = tubeSaturate(preamp, stage2);
//
//    // Remove DC offset
//    double processed = removeDC(poweramp);
//
//    // Output level compensation based on drive
//    double compensation = 1.0 / (1.0 + (drive * 0.5));
//    processed *= compensation;
//
//    // Wet/dry mix
//    return (processed * mix) + (input * (1.0 - mix));
//  }
//};
//
//// Extended version with additional tone control
//class AcidTubeSaturatorEx : public AcidTubeSaturator
//{
//private:
//  // Simple one-pole filters for tone control
//  double lowpassL, highpassL;
//  double lowpassR, highpassR;
//  double tone;  // -1 to 1 (dark to bright)
//
//public:
//  AcidTubeSaturatorEx(double sampleRate = 44100.0) :
//    AcidTubeSaturator(sampleRate),
//    lowpassL(0.0),
//    highpassL(0.0),
//    lowpassR(0.0),
//    highpassR(0.0),
//    tone(0.0)
//  {
//  }
//
//  void setTone(double newTone) { tone = my_clamp(newTone, -1.0, 1.0); }
//
//  // Process stereo signal
//  std::pair<double, double> processStereo(double inputL, double inputR)
//  {
//    // Basic saturation
//    double outL = process(inputL);
//    double outR = process(inputR);
//
//    // Tone control
//    const double alpha = 0.2;
//
//    // Lowpass
//    lowpassL = lowpassL + alpha * (outL - lowpassL);
//    lowpassR = lowpassR + alpha * (outR - lowpassR);
//
//    // Highpass
//    highpassL = outL - lowpassL;
//    highpassR = outR - lowpassR;
//
//    // Mix based on tone control
//    if (tone > 0.0)
//    {
//      // Brighter
//      outL = (1.0 - tone) * outL + tone * highpassL;
//      outR = (1.0 - tone) * outR + tone * highpassR;
//    }
//    else
//    {
//      // Darker
//      outL = (1.0 + tone) * outL - tone * lowpassL;
//      outR = (1.0 + tone) * outR - tone * lowpassR;
//    }
//
//    return std::make_pair(outL, outR);
//  }
//};
//
//// Usage example:
//void
//processAudioBlock(double *inputBuffer, double *outputBufferL, double *outputBufferR, int numSamples)
//{
//  static AcidTubeSaturatorEx saturator(44100.0);
//
//  // Configure parameters
//  saturator.setDrive(0.7);  // 70% drive
//  saturator.setBias(0.1);   // Slight positive bias for asymmetric clipping
//  saturator.setMix(1.0);    // 100% wet
//  saturator.setTone(0.2);   // Slightly bright
//
//  for (int i = 0; i < numSamples; ++i)
//  {
//    // Process mono input to stereo output
//    auto output = saturator.processStereo(inputBuffer[i], inputBuffer[i]);
//    outputBufferL[i] = output.first;
//    outputBufferR[i] = output.second;
//  }
//}
//


//
// Drive
//
// A TB-303-clone style overdrive stage: a pre-gain sweep into an asymmetric
// soft clipper, followed by a tone tilt and only partial gain make-up.
//
// Three things make this feel like the drive knob on a hardware clone rather
// than a "distortion on/off" switch:
//
//  * The pre-gain sweep is exponential (0 dB .. +32 dB). An even
//    dB-per-degree taper is what makes a gain knob feel linear to the ear
//    over its whole travel instead of doing everything in the first 20%.
//
//  * The clipping is *asymmetric*, and gets more so as the knob comes up: a
//    bias pushes the signal off-centre and the negative half is squashed
//    harder than the positive one. That asymmetry is what generates the
//    even-order harmonics behind the vocal, growling quality of an
//    overdriven 303. A plain symmetric tanh only ever produces odd harmonics
//    and lands somewhere between "fuzz box" and "nothing".
//
//  * Make-up gain holds the output level constant, so the knob changes the
//    character of the sound and nothing else. You can sweep it without the
//    level moving underneath you, and A/B settings without the louder one
//    simply sounding better. The compensation is a measured curve rather than
//    a formula — see kMakeupDb below.
//
class AcidDrive
{
public:
  AcidDrive(double sampleRate = 44100.0)
  {
    setAmount(0.0);
    setSampleRate(sampleRate);
  }

  void setSampleRate(double sr)
  {
    if (sr <= 0.0)
      return;

    mSampleRate = sr;

    // Post-clip tone control. Clipping a 303 line generates a lot of energy
    // well above the musical range; rolling it off takes the edge off the
    // aliasing that energy folds back down as. Kept gentle and high — measured
    // against a real open303 note it costs almost none of the added harmonic
    // character, so it is close to free insurance.
    const double fc = std::min(9000.0, 0.45 * mSampleRate);
    mLpCoeff = std::exp(-2.0 * M_PI * fc / mSampleRate);

    // 20 Hz DC blocker, needed because the bias below deliberately shifts the
    // waveform off-centre.
    mDcCoeff = 1.0 - (2.0 * M_PI * 20.0 / mSampleRate);

    // ~15 ms parameter smoothing, so automating or dragging the knob glides
    // instead of stepping the gain and clicking.
    mSmoothCoeff = 1.0 - std::exp(-1.0 / (0.015 * mSampleRate));

    reset();
  }

  void reset()
  {
    mLp1 = 0.0;
    mLp2 = 0.0;
    mDcLastIn = 0.0;
    mDcLastOut = 0.0;
    snapToTarget();
  }

  // amount: 0.0 = clean, 1.0 = fully driven.
  void setAmount(double amount)
  {
    mAmountTarget = my_clamp(amount, 0.0, 1.0);

    mPreGainTarget = std::pow(10.0, (kMaxDriveDb * mAmountTarget) / 20.0);

    mBiasTarget = 0.35 * mAmountTarget;        // asymmetric operating point
    mAsymTarget = 1.0 + 0.35 * mAmountTarget;  // negative half squashed harder

    mLevelMatchTarget = std::pow(10.0, makeupDb(mAmountTarget) / 20.0);
  }

  // The smoothed value, so downstream stages track the knob the same way.
  double getAmount() const { return mAmount; }

  /** Gain that holds the level of the whole drive path constant as the knob
    moves. Apply it AFTER the tube stage, not inside this one.

    Folding it into process() does not work: it would lower the level going
    into the tube saturator, moving that stage into a more linear part of its
    curve, so the tube would stop compressing the rise it is supposed to be
    compensating for — the correction partly cancels itself. Applied at the end
    of the path it is a plain output trim and the measured curve holds. */
  double getLevelMatchGain() const { return mLevelMatch; }

  double process(double input)
  {
    mAmount += (mAmountTarget - mAmount) * mSmoothCoeff;
    mPreGain += (mPreGainTarget - mPreGain) * mSmoothCoeff;
    mBias += (mBiasTarget - mBias) * mSmoothCoeff;
    mAsym += (mAsymTarget - mAsym) * mSmoothCoeff;
    mLevelMatch += (mLevelMatchTarget - mLevelMatch) * mSmoothCoeff;

    // kInputTrim keeps the clean end of the knob inside the linear region of
    // the tanh curve, so "drive off" really is transparent instead of
    // arriving pre-cooked — which is what leaves room for the rest of the
    // sweep to be audible.
    const double x = input * kInputTrim * mPreGain + mBias;

    const double shaped = (x >= 0.0) ? std::tanh(x) : std::tanh(x * mAsym) / mAsym;

    // Strip the DC that the bias and the asymmetry introduce.
    double out = shaped - mDcLastIn + mDcCoeff * mDcLastOut;
    mDcLastIn = shaped;
    mDcLastOut = out;

    // Two cascaded one-poles, blended in proportionally to the drive so a
    // clean setting stays fully open.
    mLp1 += (out - mLp1) * (1.0 - mLpCoeff);
    mLp2 += (mLp1 - mLp2) * (1.0 - mLpCoeff);
    out += (mLp2 - out) * (0.30 * mAmount);

    // Undo the headroom trim only. The level matching is a separate output
    // trim applied after the tube stage — see getLevelMatchGain().
    return out * (1.0 / kInputTrim);
  }

private:
  // Level-matching make-up gain, in dB, at 11 evenly spaced knob positions.
  //
  // Measured rather than derived: with make-up held at the trim-only baseline,
  // the level at the END of the drive path (this shaper plus the tube stage,
  // whose own gain also tracks the knob) rises by up to ~8 dB, so these are the
  // negated figures that hold it flat. Calibrated as the mean across saw,
  // square and accented open303 notes; the worst residual across those is
  // 0.83 dB over the whole knob travel.
  //
  // A static curve is used deliberately in preference to an RMS-tracking
  // auto-gain: the latter is a compressor by another name and would flatten
  // the note-to-note dynamics that accents depend on.
  static double makeupDb(double amount)
  {
    static const double kMakeupDb[11] = { 0.00,  -1.93, -3.54, -4.83, -5.80, -6.50,
                                          -6.95, -7.23, -7.36, -7.38, -7.33 };
    const double pos = my_clamp(amount, 0.0, 1.0) * 10.0;
    int i = static_cast<int>(pos);
    if (i > 9)
      i = 9;
    return kMakeupDb[i] + (kMakeupDb[i + 1] - kMakeupDb[i]) * (pos - i);
  }

  // Tuned by sweeping these against a real open303 note rendered through the
  // same peaking EQ that feeds this stage, selecting for a waveform that keeps
  // changing across the whole knob travel and a genuinely unity-gain clean
  // setting.
  //
  // kInputTrim looks small because the signal arriving here is hot: open303 at
  // unity volume peaks around 2.5, and the +13 dB EQ takes that past 5.
  static constexpr double kMaxDriveDb = 32.0;  // pre-gain at the top of the knob
  static constexpr double kInputTrim = 0.115;  // headroom trim, undone by the make-up gain

  void snapToTarget()
  {
    mAmount = mAmountTarget;
    mPreGain = mPreGainTarget;
    mBias = mBiasTarget;
    mAsym = mAsymTarget;
    mLevelMatch = mLevelMatchTarget;
  }

  double mSampleRate = 44100.0;

  double mAmount = 0.0, mAmountTarget = 0.0;
  double mPreGain = 1.0, mPreGainTarget = 1.0;
  double mBias = 0.0, mBiasTarget = 0.0;
  double mAsym = 1.0, mAsymTarget = 1.0;
  double mLevelMatch = 1.0, mLevelMatchTarget = 1.0;

  double mSmoothCoeff = 1.0;
  double mLpCoeff = 0.0;
  double mDcCoeff = 0.0;
  double mLp1 = 0.0;
  double mLp2 = 0.0;
  double mDcLastIn = 0.0;
  double mDcLastOut = 0.0;
};

class AcidTubeSaturator
{
private:
  double sampleRate;
  double drive;
  double bias;
  double mix;

  // DC blocking filter state
  double lastInputDC;
  double lastOutputDC;

  // Tube stage parameters
  struct TubeStage
  {
    double bias;
    double gain;
    double shape;

    TubeStage() : bias(0.2), gain(1.5), shape(0.8) {}
  };

  TubeStage stage;

public:
  AcidTubeSaturator(double sampleRate = 44100.0) :
    sampleRate(sampleRate),
    drive(1.0),
    bias(0.0),
    mix(1.0),
    lastInputDC(0.0),
    lastOutputDC(0.0)
  {
    // Initialize with values that complement acid sounds.
    // NOTE: stage.gain is deliberately modest. Combined with the drive-
    // dependent input gain below it is easy to push this stage so far that
    // 1-exp(-x) pins at 1.0 for every input, at which point it flattens
    // whatever the drive knob did upstream into one identical crushed
    // waveform. This stage is here to add colour, not to be the distortion.
    stage.bias = 0.2;
    stage.gain = 1.2;
    stage.shape = 0.8;
  }

  void setSampleRate(double sr)
  {
    if (sr > 0.0)
      sampleRate = sr;
  }

  void setDrive(double newDrive) { drive = my_clamp(newDrive, 0.0, 1.0); }

  void setBias(double newBias) { bias = my_clamp(newBias, -1.0, 1.0); }

  void setMix(double newMix) { mix = my_clamp(newMix, 0.0, 1.0); }

private:
  double tubeSaturate(double input)
  {
    double x = input * stage.gain + stage.bias;

    // Asymmetric wave shaping
    if (x > 0.0)
    {
      x = 1.0 - exp(-x * stage.shape);
    }
    else
    {
      x = -1.0 + exp(x * stage.shape);
    }

    return x;
  }

  double removeDC(double input)
  {
    const double dcFilterCutoff = 20.0;
    const double r = 1.0 - (2.0 * M_PI * dcFilterCutoff / sampleRate);

    double output = input - lastInputDC + r * lastOutputDC;
    lastInputDC = input;
    lastOutputDC = output;

    return output;
  }

public:
  double process(double input)
  {
    // Input gain with drive control
    double driveGain = 1.0 + (drive * drive * 15.0);
    double inputGain = input * driveGain + bias;

    // Tube saturation
    double saturated = tubeSaturate(inputGain);

    // Remove DC offset
    double processed = removeDC(saturated);

    // Output level compensation
    double compensation = 1.0 / (1.0 + (drive * 0.5));
    processed *= compensation;

    // Wet/dry mix
    return (processed * mix) + (input * (1.0 - mix));
  }
};

// driveAmount (0..1) comes from the Drive knob: the tube stage leans in as the
// drive comes up, so the two saturators cascade the way stacked gain stages do
// in hardware, instead of the tube stage sitting at a fixed heavy setting that
// masks the knob entirely.
double processAcidTubeSaturatorBlock(double sampleRate, double input, double driveAmount);

// Brickwall soft limiter — applied at the end of both signal paths to ensure
// consistent output levels with and without effects.
std::pair<double, double> processLimiter(std::pair<double, double> input);

//
// EQ
//
class PeakingEQ
{
public:
  void calculateCoefficients(double sampleRate, double frequency, double dbGain, double Q)
  {
    double A = std::pow(10.0, dbGain / 40.0);
    double omega = 2.0 * M_PI * frequency / sampleRate;
    double sn = std::sin(omega);
    double cs = std::cos(omega);
    double alpha = sn / (2.0 * Q);

    double b0 = 1.0 + alpha * A;
    double b1 = -2.0 * cs;
    double b2 = 1.0 - alpha * A;
    double a0 = 1.0 + alpha / A;
    double a1 = -2.0 * cs;
    double a2 = 1.0 - alpha / A;

    // Normalisera koefficienterna
    mB0 = b0 / a0;
    mB1 = b1 / a0;
    mB2 = b2 / a0;
    mA1 = a1 / a0;
    mA2 = a2 / a0;
  }

  double process(double input)
  {
    // Direkt Form I implementation
    double output = mB0 * input + mB1 * mZ1 + mB2 * mZ2 - mA1 * mY1 - mA2 * mY2;

    // Uppdatera delay-lines
    mZ2 = mZ1;
    mZ1 = input;
    mY2 = mY1;
    mY1 = output;

    return output;
  }

private:
  // Default to a pass-through, so the filter is harmless if it is ever used
  // before calculateCoefficients() has run.
  double mB0 = 1.0, mB1 = 0.0, mB2 = 0.0, mA1 = 0.0, mA2 = 0.0;
  double mZ1 = 0, mZ2 = 0, mY1 = 0, mY2 = 0;  // Delay lines
};