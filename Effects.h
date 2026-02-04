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
    // Initialize with values that complement acid sounds
    stage.bias = 0.2;
    stage.gain = 1.5;
    stage.shape = 0.8;
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

double processAcidTubeSaturatorBlock(double sampleRate, double input);
