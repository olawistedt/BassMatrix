#include "Effects.h"
#include <algorithm>
#include <cmath>

// Amazon Q
AcidStereoDelay::AcidStereoDelay(double sampleRate) :
  writeIndex(0),
  delayLengthL(12000)  // ~272ms at 44.1kHz
  ,
  delayLengthR(12500)  // slightly different for stereo width
  ,
  feedback(0.4f),
  dryMix(0.7f),
  wetMix(0.5f),
  sampleRate(sampleRate),
  filterStateL(0.0f),
  filterStateR(0.0f),
  filterCoeff(0.3f)  // Adjust for desired filtering
{
  delayBufferL = new double[MAX_DELAY_LENGTH]();
  delayBufferR = new double[MAX_DELAY_LENGTH]();
}

AcidStereoDelay::~AcidStereoDelay()
{
  delete[] delayBufferL;
  delete[] delayBufferR;
}

void
AcidStereoDelay::setDelayTime(double timeMs)
{
  delayLengthL = (int)(timeMs * 0.001f * sampleRate);
  delayLengthR = (int)(timeMs * 0.001f * sampleRate * 1.05f);  // Slightly longer for stereo

  // Ensure we don't exceed buffer size
  delayLengthL = std::min(delayLengthL, MAX_DELAY_LENGTH);
  delayLengthR = std::min(delayLengthR, MAX_DELAY_LENGTH);
}

void
AcidStereoDelay::setFeedback(double fb)
{
  feedback = my_clamp(fb, 0.0, 0.95);  // Limit feedback to prevent runaway
}

void
AcidStereoDelay::setWetDryMix(double wet, double dry)
{
  wetMix = my_clamp(wet, 0.0, 1.0);
  dryMix = my_clamp(dry, 0.0, 1.0);
}

// Process a single mono sample and return stereo output
std::pair<double, double>
AcidStereoDelay::process(double input)
{
  // Read from delay buffer
  int readIndexL = writeIndex - delayLengthL;
  if (readIndexL < 0)
    readIndexL += MAX_DELAY_LENGTH;

  int readIndexR = writeIndex - delayLengthR;
  if (readIndexR < 0)
    readIndexR += MAX_DELAY_LENGTH;

  double delayedL = delayBufferL[readIndexL];
  double delayedR = delayBufferR[readIndexR];

  // Apply feedback filter (simple one-pole lowpass)
  filterStateL = filterStateL + filterCoeff * (delayedL - filterStateL);
  filterStateR = filterStateR + filterCoeff * (delayedR - filterStateR);

  // Write to delay buffer with feedback
  delayBufferL[writeIndex] = input + (filterStateL * feedback);
  delayBufferR[writeIndex] = input + (filterStateR * feedback);

  // Increment write index
  writeIndex++;
  if (writeIndex >= MAX_DELAY_LENGTH)
  {
    writeIndex = 0;
  }

  // Create output mix
  double outL = (input * dryMix) + (delayedL * wetMix);
  double outR = (input * dryMix) + (delayedR * wetMix);

  return std::make_pair(outL, outR);
}

// Simple reverb to complement the delay
SimpleAcidReverb::SimpleAcidReverb() : feedback(0.6f), wetMix(0.3f)
{
  // Initialize delay lines with prime numbers for better diffusion
  int primeLengths[NUM_DELAYS] = { 1087, 1093, 1097, 1103, 1109, 1117, 1123, 1129 };

  for (int i = 0; i < NUM_DELAYS; ++i)
  {
    delays[i] = new double[MAX_REVERB_LENGTH]();
    delayLengths[i] = primeLengths[i];
    writeIndices[i] = 0;
  }
}

SimpleAcidReverb::~SimpleAcidReverb()
{
  for (int i = 0; i < NUM_DELAYS; ++i)
  {
    delete[] delays[i];
  }
}

std::pair<double, double>
SimpleAcidReverb::process(double input)
{
  double outL = 0.0f;
  double outR = 0.0f;

  for (int i = 0; i < NUM_DELAYS; ++i)
  {
    int readIndex = writeIndices[i] - delayLengths[i];
    if (readIndex < 0)
      readIndex += MAX_REVERB_LENGTH;

    double delayed = delays[i][readIndex];

    // Update delay line
    delays[i][writeIndices[i]] = input + (delayed * feedback);

    // Increment write index
    writeIndices[i] = (writeIndices[i] + 1) % MAX_REVERB_LENGTH;

    // Sum to output, alternating between left and right
    if (i % 2 == 0)
    {
      outL += delayed;
    }
    else
    {
      outR += delayed;
    }
  }

  // Normalize and mix
  outL = (outL * wetMix / (NUM_DELAYS / 2)) + input;
  outR = (outR * wetMix / (NUM_DELAYS / 2)) + input;

  return std::make_pair(outL, outR);
}

std::pair<double, double>
processDelayReverbAudioBlock(double sampleRate, double input)
{
  static AcidStereoDelay delay(sampleRate);
  static SimpleAcidReverb reverb;

  // Configure delay parameters
  delay.setDelayTime(272.0f);      // 272ms delay time
  delay.setFeedback(0.4f);         // 40% feedback
  delay.setWetDryMix(0.5f, 0.7f);  // 50% wet, 70% dry

  // Process delay
  auto delayOut = delay.process(input);

  // Process reverb
  auto reverbOut = reverb.process(input);

  // Mix delay and reverb
  double left = 0.7f * delayOut.first + 0.3f * reverbOut.first;
  double right = 0.7f * delayOut.second + 0.3f * reverbOut.second;

  return std::make_pair(left, right);
}

// Usage example:
double
processAcidTubeSaturatorBlock(double sampleRate, double input)
{
  static AcidTubeSaturator saturator(sampleRate);

  saturator.setDrive(0.7);  // 70% drive
  saturator.setBias(0.1);   // Slight positive bias
  saturator.setMix(1.0);    // 100% wet

  return saturator.process(input);
}

class SimpleCompressor
{
public:
  SimpleCompressor() :
    threshold(0.5), ratio(4.0), attack(0.010), release(0.100), makeup(1.5), env(0.0), sampleRate(44100.0)
  {
    calcCoeffs();
  }

  void setSampleRate(double sr)
  {
    if (sampleRate != sr)
    {
      sampleRate = sr;
      calcCoeffs();
    }
  }

  void calcCoeffs()
  {
    attackCoeff = exp(-1.0 / (sampleRate * attack));
    releaseCoeff = exp(-1.0 / (sampleRate * release));
  }

  std::pair<double, double> process(double inL, double inR)
  {
    double det = std::max(std::abs(inL), std::abs(inR));

    if (det > env)
      env = attackCoeff * env + (1.0 - attackCoeff) * det;
    else
      env = releaseCoeff * env + (1.0 - releaseCoeff) * det;

    double gain = 1.0;
    if (env > threshold)
    {
      gain = pow(env / threshold, 1.0 / ratio - 1.0);
    }

    return { inL * gain * makeup, inR * gain * makeup };
  }

private:
  double threshold, ratio, attack, release, makeup, env, sampleRate, attackCoeff, releaseCoeff;
};

std::pair<double, double>
processCompressorBlock(double sampleRate, std::pair<double, double> input)
{
  static SimpleCompressor comp;
  comp.setSampleRate(sampleRate);
  return comp.process(input.first, input.second);
}
