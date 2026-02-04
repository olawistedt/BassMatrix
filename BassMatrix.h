#pragma once

#include "IPlug_include_in_plug_hdr.h"
#include "IControls.h"
#include "open303/Source/DSPCode/rosic_Open303.h"
#include "IPlugProcessor.h"
#include "IPlugEditorDelegate.h"
#include "IPlugAPIBase.h"

const double kVersion = 2.00;
const int kNumPresets = 1;
const int kNumberOfNoteBtns = 13;
const int kNumberOfPropertyBtns = 5;
const int kNumberOfTotalPropButtons = kNumberOfPropertyBtns * 16;
const int kNumberOfSeqButtons = kNumberOfNoteBtns * 16 + kNumberOfPropertyBtns * 16;
const int kNumberOfPatterns = 24;

enum EParams
{
  // First the parameters that is not saved.
  kLedBtn0 = 0,
  kBtnSeq0 = kLedBtn0 + 16,
  kBtnProp0 = kBtnSeq0 + 16 * kNumberOfNoteBtns,

  // Parameters that are saved
  kParamPattern0 = kBtnProp0 + 16 * kNumberOfPropertyBtns,
  kParamOct0 = kParamPattern0 + 12,
  kParamCutOff = kParamOct0 + 2,
  kParamResonance,
  kParamWaveForm,
  kParamTuning,
  kParamEnvMode,
  kParamDecay,
  kParamAccent,
  kParamVolume,
  kParamTempo,
  kParamDrive,
  kParamPlayMode0,
  kKnobLoopSize = kParamPlayMode0 + 5,
  kParamCopy,
  kParamClear,
  kParamRandomize,
  kParamEffects,

  kNumParams
};

enum ECtrlTags
{
  kCtrlTagVersionNumber = 0,
  kCtrlTagSeq0,
  kCtrlTagProp0 = kCtrlTagSeq0 + 16 * kNumberOfNoteBtns,
  kCtrlTagLedSeq0 = kCtrlTagProp0 + 16 * kNumberOfPropertyBtns,
  kCtrlTagPattern0 = kCtrlTagLedSeq0 + 16,
  kCtrlTagOctav0 = kCtrlTagPattern0 + 12,
  kCtrlWaveForm = kCtrlTagOctav0 + 2,
  kCtrlTagPlayMode0,
  kCtrlEffects = kCtrlTagPlayMode0 + 5,
  kNumCtrlTags
};

using namespace iplug;
using namespace igraphics;

class BassMatrix final : public Plugin
{
public:
  BassMatrix(const InstanceInfo &info);
  virtual ~BassMatrix();

#if IPLUG_EDITOR
  //  void OnParentWindowResize(int width, int height) override;
  bool OnHostRequestingSupportedViewConfiguration(int width, int height) override { return true; }
#if defined(VST3_API) || defined(AU_API)
  bool SerializeState(IByteChunk &chunk) const override;
  int UnserializeState(const IByteChunk &chunk, int startPos) override;
  IGraphics *CreateGraphics() override;
#endif  // API
#endif  // IPLUG_EDITOR

#if IPLUG_DSP
  void ProcessMidiMsg(const IMidiMsg &msg) override;
  void OnReset() override;
#if defined(VST3_API) || defined(AU_API)
  void OnParamChangeUI(int paramIdx, EParamSource source = kUnknown) override;
#else
  void OnParamChange(int paramIdx) override;
#endif  // VST3_API
  void OnIdle() override;
  bool OnMessage(int msgTag, int ctrlTag, int dataSize, const void *pData) override;
#endif  // IPLUG_DSP
  static std::array<bool, kNumberOfSeqButtons>
  CollectSequenceButtons(rosic::Open303 &open303Core, int patternNr = -1);
#if IPLUG_DSP  // http://bit.ly/2S64BDd
  void ProcessBlock(PLUG_SAMPLE_DST **inputs, PLUG_SAMPLE_DST **outputs, int nFrames) override;
#endif

protected:
  IMidiQueue mMidiQueue;

private:
  // the embedded core dsp object:
  rosic::Open303 open303Core;
  ISender<1, 1, int> mLedSeqSender;
  ISender<1, 1, std::array<bool, kNumberOfSeqButtons>> mSequencerSender;
  //  ISender<1, 1, int> mPatternSender;
  ISender<1, 1, int> mSelectedOctavSender;
  ISender<1, 1, int> mSelectedPatternSender;
  ISender<1, 1, int> mSelectedModeSender;
  unsigned int mLastSamplePos;
  bool mStartSyncWithHost;
  int mKnobLoopSize;
  int mCurrentPattern;
  int mSelectedOctav;
  int mSelectedPattern;
  bool mHasChanged;
  double mPlugUIScale;
  bool mUseEffects;
  bool mHasLoadingPresets;
  int mSelectedPlayMode;
};
