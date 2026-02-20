#include "BassMatrix.h"
#include "IPlug_include_in_plug_src.h"
#include "BassMatrixControls.h"
#include "open303/Source/DSPCode/rosic_Open303.h"
#include <filesystem>
#include <sstream>
#include <fstream>
#include "Effects.h"

std::pair<double, double>
processCompressorBlock(double sampleRate, std::pair<double, double> input);

////////////////////////////////////////////////////////////////////////////////////////
// Windows specific
////////////////////////////////////////////////////////////////////////////////////////
#ifdef _WIN32
#include <shlobj.h>
std::filesystem::path
getDllPath()
{
  wchar_t dllPath[MAX_PATH];
  HMODULE hModule = GetModuleHandleA("BassMatrix.vst3");
  if (hModule == NULL)
  {
    throw std::runtime_error("Failed to get module handle.");
  }
  GetModuleFileNameW(hModule, dllPath, MAX_PATH);
  return std::filesystem::path(dllPath).parent_path();
}

std::wstring
getSettingsFilePath()
{
  wchar_t programDataPath[MAX_PATH];
  SHGetSpecialFolderPathW(0, programDataPath, CSIDL_COMMON_APPDATA, false);

  std::wstring directoryPath = std::wstring(programDataPath) + L"\\Witech\\BassMatrix";
  std::filesystem::create_directories(directoryPath);

  return directoryPath + L"\\settings.json";
}

void
WriteSettingsToProgramDataPath(double plugUIScale)
{
  std::wofstream file(getSettingsFilePath());
  if (file.is_open())
  {
    file << L"{\n\t\"guiSize\": \"" + std::to_wstring(plugUIScale) + L"\"\n}";
    file.close();
  }
}

void
ReadSettingsFromProgramDataPath(double &plugUIScale)
{
  std::wifstream file(getSettingsFilePath());
  if (file.is_open())
  {
    std::wstring line;
    while (std::getline(file, line))
    {
      if (line.find(L"guiSize") != std::wstring::npos)
      {
        std::wstring guiSize = line.substr(line.find(L"\"") + 1);
        guiSize =
            guiSize.substr(guiSize.find(':') + 3, guiSize.rfind(L"\"") - guiSize.find(':') - 3);
        plugUIScale = std::stod(guiSize);
      }
    }
    file.close();
  }
}

#endif

class TempoLabelControl : public IControl
{
public:
  TempoLabelControl(IRECT bounds, int paramIdx) : IControl(bounds, paramIdx) { Hide(true); }

  void Draw(IGraphics &g) override
  {
    g.FillRect(COLOR_BLACK, mRECT);

    WDL_String str;
    GetParam()->GetDisplay(str);

    IText text(14.f, COLOR_WHITE);
    g.DrawText(text, str.Get(), mRECT);
  }
};

class TempoKnobControl : public IBKnobControl
{
public:
  TempoKnobControl(float x, float y, const IBitmap &bitmap, int paramIdx, int labelTag) :
    IBKnobControl(x, y, bitmap, paramIdx),
    mLabelTag(labelTag)
  {
  }

  void OnMouseDown(float x, float y, const IMouseMod &mod) override
  {
    IBKnobControl::OnMouseDown(x, y, mod);
    if (auto *pLabel = GetUI()->GetControlWithTag(mLabelTag))
    {
      pLabel->Hide(false);
    }
  }

  void OnMouseUp(float x, float y, const IMouseMod &mod) override
  {
    IBKnobControl::OnMouseUp(x, y, mod);
    if (auto *pLabel = GetUI()->GetControlWithTag(mLabelTag))
    {
      pLabel->Hide(true);
    }
  }

private:
  int mLabelTag;
};

BassMatrix::BassMatrix(const InstanceInfo &info) :
  Plugin(info, MakeConfig(kNumParams, kNumPresets)),
  mLastSamplePos(0),
  mStartSyncWithHost(false),
  mPlugUIScale(1.0),
  mHasChanged(false),
  mKnobLoopSize(0),
  mCurrentPattern(0),
  mUseEffects(true),
  mHasLoadingPresets(false),
  mSelectedOctav(0),
  mSelectedPattern(0),
#if IPLUG_EDITOR
  mSelectedPlayMode(rosic::AcidSequencer::HOST_SYNC)
#else
  mSelectedPlayMode(rosic::AcidSequencer::RUN)
#endif
{
#ifdef _WIN32
#ifdef _DEBUG
  OutputDebugStringW(L"### BassMatrix started ###\n");
#endif
#endif

  // Setup the open303 sequencer.
  //srand(static_cast<unsigned int>(time(0)));
  //open303Core.sequencer.randomizeAllPatterns();
  // Clear all patterns.
  for (int i = 0; i < 24; i++)
  {
    open303Core.sequencer.clearPattern(i);
  }
  open303Core.sequencer.setPattern(0);
  open303Core.sequencer.setMode(mSelectedPlayMode);

  //
  // Setup parameters and their default values
  //
  GetParam(kParamCutOff)->InitDouble("Cut off", 640.0, 314.0, 2394.0, 1.0, "Hz");
  GetParam(kParamResonance)->InitDouble("Resonace", 50.0, 0.0, 100.0, 1.0, "%");
  GetParam(kParamTuning)->InitDouble("Tuning", 440.0, 400.0, 480.0, 1.0, "%");
  GetParam(kParamEnvMode)->InitDouble("Env mode", 25.0, 0.0, 100.0, 1.0, "%");
  GetParam(kParamDecay)->InitDouble("Decay", 400.0, 200.0, 2000.0, 1.0, "ms");
  GetParam(kParamAccent)->InitDouble("Accent", 50.0, 0.0, 100.0, 1.0, "%");
  GetParam(kParamVolume)->InitDouble("Volume", -30.0, -75.0, 0.0, 0.1, "dB");
  GetParam(kParamTempo)->InitDouble("Tempo", 120.0, 10.0, 300.0, 1.0, "bpm");
  GetParam(kParamDrive)->InitDouble("Drive", 13.0, 1.0, 50.0, 0.1, "dB");

  GetParam(kParamWaveForm)->InitBool("Waveform", false);
  GetParam(kParamEffects)->InitBool("Effects", true);

  GetParam(kParamPlayMode0)->InitBool("Stop", mSelectedPlayMode == rosic::AcidSequencer::OFF);
  GetParam(kParamPlayMode0 + 1)
      ->InitBool("Host Sync", mSelectedPlayMode == rosic::AcidSequencer::HOST_SYNC);
  GetParam(kParamPlayMode0 + 2)
      ->InitBool("Key Sync", mSelectedPlayMode == rosic::AcidSequencer::KEY_SYNC);
  GetParam(kParamPlayMode0 + 3)
      ->InitBool("Internal Sync", mSelectedPlayMode == rosic::AcidSequencer::RUN);
  GetParam(kParamPlayMode0 + 4)
      ->InitBool("Midi Play", mSelectedPlayMode == rosic::AcidSequencer::KEY_SYNC);

  // Led buttons. We don't want them to be able to automate.
  for (int i = kLedBtn0; i < kLedBtn0 + 16; ++i)
  {
    GetParam(i)->InitBool(("Led button " + std::to_string(i - kLedBtn0)).c_str(),
                          false,
                          "On/Off",
                          IParam::kFlagCannotAutomate);
  }

  // Sequencer buttons (note buttons and property buttons). We don't want them to be able to automate.
  for (int i = kBtnSeq0; i < kBtnSeq0 + kNumberOfSeqButtons; ++i)
  {
    if (i > kBtnProp0 + 63 && i < kBtnProp0 + 80)  // The gate buttons
    {
      GetParam(i)->InitBool(("Sequencer button " + std::to_string(i - kBtnSeq0)).c_str(),
                            true,
                            "On/Off",
                            IParam::kFlagCannotAutomate);
    }
    else
    {
      GetParam(i)->InitBool(("Sequencer button " + std::to_string(i - kBtnSeq0)).c_str(),
                            false,
                            "On/Off",
                            IParam::kFlagCannotAutomate);
    }
  }

  for (int i = kParamPattern0; i < kParamPattern0 + 12; ++i)
  {
    GetParam(i)->InitBool(("Pattern button" + std::to_string(i - kParamPattern0)).c_str(),
                          false,
                          "On/Off",
                          IParam::kFlagCannotAutomate);
  }

  GetParam(kParamOct0)
      ->InitBool("Octav 2",
                 false,
                 "On/Off",
                 IParam::kFlagCannotAutomate);  // It's bad to set something to true here!!
  GetParam(kParamOct0 + 1)
      ->InitBool("Octav 3",
                 false,
                 "On/Off",
                 IParam::kFlagCannotAutomate);  // It's bad to set something to true here!!
  //  GetParam(kBtnPtnC)->InitBool("Pattern C", false, "On/Off", IParam::kFlagCannotAutomate);  // It's bad to set something to true here!!

  GetParam(kKnobLoopSize)->InitInt("Loop size", 1, 1, 24);

  GetParam(kParamCopy)->InitBool("Pattern copy", false);
  GetParam(kParamClear)->InitBool("Pattern clear", false);
  GetParam(kParamRandomize)->InitBool("Pattern randomize", false);


#if IPLUG_EDITOR  // http://bit.ly/2S64BDd
  mMakeGraphicsFunc = [&]()
  {
    return MakeGraphics(*this,
                        PLUG_WIDTH,
                        PLUG_HEIGHT,
                        PLUG_FPS,
                        GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT));
  };

  mLayoutFunc = [&](IGraphics *pGraphics)
  {
    pGraphics->AttachCornerResizer(EUIResizerMode::Scale, false);
    pGraphics->LoadFont("Roboto-Regular", ROBOTO_FN);
    //    pGraphics->AttachPanelBackground(COLOR_GRAY);
    // Background
    pGraphics->LoadBitmap(BACKGROUND_FN, 1, true);
    pGraphics->AttachBackground(BACKGROUND_FN);

    // Knobs
    const IBitmap knobRotateBitmap = pGraphics->LoadBitmap(PNG6062_FN, 127);
    const IBitmap knobLittleBitmap = pGraphics->LoadBitmap(PNGFX1LITTLE_FN, 127);
    const IBitmap knobBigBitmap = pGraphics->LoadBitmap(PNGFX1BIG_FN, 61);
    //    pGraphics->AttachControl(new IBKnobControl(210, 30, knobLittleBitmap, kParamWaveForm));
    const IBitmap btnWaveForm = pGraphics->LoadBitmap(PNGWAVEFORM_FN, 2, true);
    pGraphics->AttachControl(new IBSwitchControl(200, 50, btnWaveForm, kParamWaveForm),
                             kCtrlWaveForm);
    const IBitmap btnUseEffects = pGraphics->LoadBitmap(PNGEFFECTS_FN, 2, true);
    pGraphics->AttachControl(new IBSwitchControl(200, 75, btnUseEffects, kParamEffects),
                             kCtrlEffects);
    pGraphics->AttachControl(new IBKnobControl(310, 30, knobLittleBitmap, kParamTuning));
    pGraphics->AttachControl(new IBKnobControl(410, 30, knobLittleBitmap, kParamCutOff));
    pGraphics->AttachControl(new IBKnobControl(510, 30, knobLittleBitmap, kParamResonance));
    pGraphics->AttachControl(new IBKnobControl(610, 30, knobLittleBitmap, kParamEnvMode));
    pGraphics->AttachControl(new IBKnobControl(710, 30, knobLittleBitmap, kParamDecay));
    pGraphics->AttachControl(new IBKnobControl(810, 30, knobLittleBitmap, kParamAccent));
    pGraphics->AttachControl(new IBKnobControl(108, 30, knobLittleBitmap, kParamDrive));
    pGraphics->AttachControl(new TempoLabelControl(IRECT(100, 135, 140, 155), kParamTempo),
                             kCtrlTagTempoLabel);
    pGraphics->AttachControl(
        new TempoKnobControl(0 + 210 - 175, 130, knobBigBitmap, kParamTempo, kCtrlTagTempoLabel));
    //    pGraphics->AttachControl(new IBKnobControl(510, 130, knobBigBitmap, kParamDrive));
    pGraphics->AttachControl(new IBKnobControl(1130 - 210, 130, knobBigBitmap, kParamVolume));

    // Octav buttons
    IBitmap btnPatternOctavBitmap[2];
    btnPatternOctavBitmap[0] = pGraphics->LoadBitmap(PNGBTNPATOCTAV2_FN, 2, true);
    btnPatternOctavBitmap[1] = pGraphics->LoadBitmap(PNGBTNPATOCTAV3_FN, 2, true);

    for (int i = 0; i < 2; i++)
    {
      pGraphics->AttachControl(new GroupBtnControl(485 + (btnPatternOctavBitmap[i].FW() - 3) * i,
                                                   138,
                                                   btnPatternOctavBitmap[i],
                                                   kParamOct0 + i,
                                                   mSelectedOctav,
                                                   i,
                                                   kCtrlTagOctav0,
                                                   2),
                               kCtrlTagOctav0 + i,
                               "Octav button");
    }

    IBitmap btnPatternBitmap[12];
    btnPatternBitmap[0] = pGraphics->LoadBitmap(PNGBTNPATC_FN, 2, true);
    btnPatternBitmap[1] = pGraphics->LoadBitmap(PNGBTNPATCc_FN, 2, true);
    btnPatternBitmap[2] = pGraphics->LoadBitmap(PNGBTNPATD_FN, 2, true);
    btnPatternBitmap[3] = pGraphics->LoadBitmap(PNGBTNPATDd_FN, 2, true);
    btnPatternBitmap[4] = pGraphics->LoadBitmap(PNGBTNPATE_FN, 2, true);
    btnPatternBitmap[5] = pGraphics->LoadBitmap(PNGBTNPATF_FN, 2, true);
    btnPatternBitmap[6] = pGraphics->LoadBitmap(PNGBTNPATFf_FN, 2, true);
    btnPatternBitmap[7] = pGraphics->LoadBitmap(PNGBTNPATG_FN, 2, true);
    btnPatternBitmap[8] = pGraphics->LoadBitmap(PNGBTNPATGg_FN, 2, true);
    btnPatternBitmap[9] = pGraphics->LoadBitmap(PNGBTNPATA_FN, 2, true);
    btnPatternBitmap[10] = pGraphics->LoadBitmap(PNGBTNPATAa_FN, 2, true);
    btnPatternBitmap[11] = pGraphics->LoadBitmap(PNGBTNPATB_FN, 2, true);
    for (int i = 0; i < 12; ++i)
    {
      pGraphics->AttachControl(new GroupBtnControl(481.f + (i % 4) * btnPatternBitmap[0].W() / 2,
                                                   178.f + (i / 4) * btnPatternBitmap[0].H(),
                                                   btnPatternBitmap[i],
                                                   kParamPattern0 + i,
                                                   mSelectedPattern,
                                                   i,
                                                   kCtrlTagPattern0,
                                                   12),
                               kCtrlTagPattern0 + i);
    }

    // Pattern control buttons
    const IBitmap btnClearBitmap = pGraphics->LoadBitmap(PNGCLEAR_FN, 2, true);
    pGraphics->AttachControl(new PtnModBtnControl(369, 178, btnClearBitmap, kParamClear));
    const IBitmap btnRandomizeBitmap = pGraphics->LoadBitmap(PNGRANDOMIZE_FN, 2, true);
    pGraphics->AttachControl(new PtnModBtnControl(369, 218, btnRandomizeBitmap, kParamRandomize));
    const IBitmap btnCopyBitmap = pGraphics->LoadBitmap(PNGCOPY_FN, 1, true);
    pGraphics->AttachControl(new PtnModBtnControl(369, 258, btnCopyBitmap, kParamCopy),
                             kCtrlTagBtnCopy);

    // Loop size knob
    const IBitmap btnPatternLoopSizeBitmap =
        pGraphics->LoadBitmap(PNGKNOBPATLOOPSIZE_FN, 24, false);
    pGraphics->AttachControl(
        new IBKnobControl(666.f, 195.f, btnPatternLoopSizeBitmap, kKnobLoopSize));

    // Led buttons
    const IBitmap ledBtnBitmap = pGraphics->LoadBitmap(PNGBTNLED_FN, 2, true);
    for (int i = 0; i < 16; i++)
    {
      pGraphics->AttachControl(new SeqLedBtnControl(124.f + i * (ledBtnBitmap.W() / 2 + 3) +
                                                        ((i > 3) ? 12 : 0) + ((i > 7) ? 12 : 0) +
                                                        ((i > 11) ? 12 : 0),
                                                    294.f,
                                                    ledBtnBitmap,
                                                    kLedBtn0 + i,
                                                    open303Core),
                               kCtrlTagLedSeq0 + i,
                               "Sequencer");
    }

    // Properties buttons
    const IBitmap btnPropBitmap = pGraphics->LoadBitmap(PNGBTNPROP_FN, 2, true);
    for (int i = 0; i < 16; i++)
    {
      for (int j = 0; j < 5; j++)
      {
        pGraphics->AttachControl(new SeqNoteBtnControl(134.f + i * (btnPropBitmap.W() / 2 + 21) +
                                                           ((i > 3) ? 12 : 0) + ((i > 7) ? 12 : 0) +
                                                           ((i > 11) ? 12 : 0),
                                                       665.f + j * (btnPropBitmap.H() + 1),
                                                       btnPropBitmap,
                                                       kBtnProp0 + 16 * j + i),
                                 kCtrlTagProp0 + 16 * j + i,
                                 "Sequencer");
      }
    }

    // Sequencer tones buttons
    const IBitmap btnSeqBitmap = pGraphics->LoadBitmap(PNGBTNSEQ_FN, 2, true);
    for (int i = 0; i < 16; i++)
    {
      for (int j = 0; j < kNumberOfNoteBtns; j++)
      {
        int heigth = btnSeqBitmap.H();
        pGraphics->AttachControl(new SeqNoteBtnControl(134.f + i * (btnSeqBitmap.W() / 2 + 21) +
                                                           ((i > 3) ? 12 : 0) + ((i > 7) ? 12 : 0) +
                                                           ((i > 11) ? 12 : 0),
                                                       361.f + j * (heigth + 1),
                                                       btnSeqBitmap,
                                                       kBtnSeq0 + 16 * j + i),
                                 kCtrlTagSeq0 + 16 * j + i,
                                 "Sequencer");
      }
    }

    // Play mode buttons
    //
    IBitmap btnPlayModes[5];
    btnPlayModes[0] = pGraphics->LoadBitmap(PNGSTOP_FN, 2, true);
    btnPlayModes[1] = pGraphics->LoadBitmap(PNGHOSTSYNC_FN, 2, true);
    btnPlayModes[2] = pGraphics->LoadBitmap(PNGKEYSYNC_FN, 2, true);
    btnPlayModes[3] = pGraphics->LoadBitmap(PNGINTERNALSYNC_FN, 2, true);
    btnPlayModes[4] = pGraphics->LoadBitmap(PNGMIDIPLAY_FN, 2, true);

    for (int i = 0; i < 5; ++i)
    {
      pGraphics->AttachControl(new GroupBtnControl(310.f + (btnPlayModes[i].W() / 2 * i),
                                                   790.f,
                                                   btnPlayModes[i],
                                                   kParamPlayMode0 + i,
                                                   mSelectedPlayMode,
                                                   i,
                                                   kCtrlTagPlayMode0,
                                                   5),
                               kCtrlTagPlayMode0 + i,
                               "Mode button");
    }

#ifdef _WIN32
    ReadSettingsFromProgramDataPath(mPlugUIScale);
    pGraphics->Resize(PLUG_WIDTH, PLUG_HEIGHT, static_cast<float>(mPlugUIScale), true);
#endif
  };
#endif
}

BassMatrix::~BassMatrix()
{
#ifdef _WIN32
  WriteSettingsToProgramDataPath(mPlugUIScale);
#endif
}

//#if IPLUG_EDITOR
//void BassMatrix::OnParentWindowResize(int width, int height)
//{
//  if(GetUI())
//    GetUI()->Resize(width, height, 1.f, false);
//}
//#endif


//
// Save plugin settings to hard drive. First save is junk.
//
#if IPLUG_EDITOR
#if defined(VST3_API) || defined(AU_API)
bool
BassMatrix::SerializeState(IByteChunk &chunk) const
{
#if defined(_DEBUG) && defined(_WIN32)
  OutputDebugStringW(L"SerializeState() called\n");
#endif

  TRACE

  bool savedOK = true;

  // Set version of the preset format.
  double version = kVersion;
  savedOK &= (chunk.Put(&version) > 0);

  // Save parameters except the leds and the parameters that are stored in sequencer.
  int n = NParams();
  for (int i = kParamCutOff; i < n && savedOK; ++i)
  {
    const IParam *pParam = GetParam(i);
    Trace(TRACELOC, " %s %d %f", pParam->GetName(), i, pParam->Value());
    double v = pParam->Value();
    savedOK &= (chunk.Put(&v) > 0);
  }

  // Save all patterns
  for (int patternNr = 0; patternNr < kNumberOfPatterns && savedOK; ++patternNr)
  {
    Trace(TRACELOC, " %s %d ", "Pattern nr", patternNr);
    std::array<bool, kNumberOfSeqButtons> a =
        CollectSequenceButtons((rosic::Open303 &)open303Core, patternNr);
    for (auto elem : a)
    {
      //      Trace(TRACELOC, " %s %d %f", "Sequencer button nr", patternNr, elem ? 1.0 : 0.0);
      double v = elem ? 1.0 : 0.0;
#if defined(_DEBUG) && defined(_WIN32)
      OutputDebugStringW(v == 1.0 ? L"*" : L"-");
#endif  // _DEBUG
      savedOK &= (chunk.Put(&v) > 0);
    }
#if defined(_DEBUG) && defined(_WIN32)
    OutputDebugStringW(L"\n");
#endif  // _DEBUG
  }

  // Save current octav and current pattern.
  //  double oct2 = GetParam(kBtnPtnOct2)->Value();
  double oct3 = GetParam(kParamOct0 + 1)->Value();
  double ptn = 0.0;
  for (int i = kParamPattern0; i < kParamPattern0 + 12; ++i)
  {
    if (GetParam(i)->Value() == 1.0)
    {
      ptn = static_cast<double>(i - kParamPattern0);
    }
  }
  if (oct3 == 1.0)
  {
    ptn += 12.0;
  }
  savedOK &= (chunk.Put(&ptn) > 0);

  assert(savedOK == true);

  return savedOK;
}

//
// From hard disk to BassMatrix.
//
int
BassMatrix::UnserializeState(const IByteChunk &chunk, int startPos)
{
#if defined(_DEBUG) && defined(_WIN32)
  OutputDebugStringW(L"UnserializeState() called\n");
#endif

  TRACE

  ENTER_PARAMS_MUTEX

  int nrOfParameters = NParams(), pos = startPos;

  // Check version for the preset format
  double version;
  pos = chunk.Get(&version, pos);
  //  assert(version == 1.1);

  if (version == 1.0 || version == 1.10)
  {
    nrOfParameters = NParams() - 1;  // The use effects button has been added since 1.00 and 1.10
    GetParam(kParamEffects)->Set(0.0);
    mUseEffects = false;
  }

  for (int i = kParamCutOff; i < nrOfParameters && pos >= 0; ++i)
  {
    IParam *pParam = GetParam(i);
    double v = 0.0;
    pos = chunk.Get(&v, pos);
    pParam->Set(v);
    Trace(TRACELOC, "%d %s %f", i, pParam->GetName(), pParam->Value());
  }

  // Unserialize patterns
  for (int patternNr = 0; patternNr < kNumberOfPatterns && pos >= 0; ++patternNr)
  {
    Trace(TRACELOC, " %s %d ", "Pattern nr", patternNr);

    rosic::AcidPattern *pattern = open303Core.sequencer.getPattern(patternNr);

    for (int i = 0; i < kNumberOfSeqButtons - kNumberOfTotalPropButtons; ++i)
    {
      double v = 0.0;
      pos = chunk.Get(&v, pos);
      //      Trace(TRACELOC, "%d %s %d %f", patternNr, "Sequencer button", i, v);
#if defined(_DEBUG) && defined(_WIN32)
      OutputDebugStringW(v == 1.0 ? L"*" : L"-");
#endif  // _DEBUG

      if (v == 1.0)
      {
        pattern->getNote(i % 16)->key = kNumberOfNoteBtns - i / 16 - 1;
      }
    }

    for (int i = 0; i < kNumberOfTotalPropButtons; ++i)  // The note properties
    {
      double v = 0.0;
      pos = chunk.Get(&v, pos);
      //      Trace(TRACELOC, "%d %s %d %f", patternNr, "Property button", i, v);

#if defined(_DEBUG) && defined(_WIN32)
      OutputDebugStringW(v == 1.0 ? L"*" : L"-");
#endif  // _DEBUG

      if (i < 16)
      {
        pattern->getNote(i % 16)->octave = (v == 1.0 ? 1 : 0);
      }
      else if (i < 32)
      {
        if (v == 1.0)
        {
          pattern->getNote(i % 16)->octave = -1;
        }
      }
      else if (i < 48)
      {
        pattern->getNote(i % 16)->accent = (v == 1.0);
      }
      else if (i < 64)
      {
        pattern->getNote(i % 16)->slide = (v == 1.0);
      }
      else if (i < 80)
      {
        pattern->getNote(i % 16)->gate = (v == 1.0);
      }
    }
#if defined(_DEBUG) && defined(_WIN32)
    OutputDebugStringW(L"\n");
#endif  // _DEBUG
  }

  // Restore octav and pattern buttons.
  double ptn;
  pos = chunk.Get(&ptn, pos);  // ptn is between 0.0 and 23.0
                               //  open303Core.sequencer.setPattern(static_cast<int>(ptn));


  if (ptn < 12.0)
  {
    GetParam(kParamOct0)->Set(1.0);
    GetParam(kParamOct0 + 1)->Set(0.0);
    mSelectedPattern = ptn;
    mSelectedOctav = 0;
  }
  else
  {
    GetParam(kParamOct0)->Set(0.0);
    GetParam(kParamOct0 + 1)->Set(1.0);
    ptn -= 12.0;
    mSelectedPattern = ptn;
    mSelectedOctav = 1;
  }
  for (int i = kParamPattern0; i < kParamPattern0 + 12; ++i)
  {
    if (static_cast<int>(ptn) == i - kParamPattern0)
    {
      GetParam(i)->Set(1.0);
    }
    else
    {
      GetParam(i)->Set(0.0);
    }
  }

  mHasLoadingPresets = true;

  OnParamReset(kPresetRecall);

  LEAVE_PARAMS_MUTEX

  return pos;
}
#endif  // API
#endif  // IPLUG_EDITOR


std::array<bool, kNumberOfSeqButtons>
BassMatrix::CollectSequenceButtons(rosic::Open303 &open303Core, int patternNr)
{
  std::array<bool, kNumberOfSeqButtons> seq;

  if (patternNr == -1)
  {
    patternNr = open303Core.sequencer.getActivePattern();
  }

#ifdef _WIN32
#ifdef _DEBUG
  OutputDebugStringW(L"CollectSequenceButtons()");
  OutputDebugStringW(std::wstring(L"Pattern: " + std::to_wstring(patternNr) + L"\n").c_str());
#endif  // _DEBUG
#endif

  rosic::AcidPattern *pattern = open303Core.sequencer.getPattern(patternNr);

  for (int i = 0; i < kNumberOfSeqButtons - kNumberOfTotalPropButtons; ++i)
  {
    seq[i] = pattern->getNote(i % 16)->key == kNumberOfNoteBtns - i / 16 - 1;
#ifdef _WIN32
#ifdef _DEBUG
    OutputDebugStringW(seq[i] ? L"*" : L"-");
#endif  // _DEBUG
#endif
  }

  for (int i = 0; i < kNumberOfTotalPropButtons; ++i)  // The note properties
  {
    int j = i + kNumberOfSeqButtons - kNumberOfTotalPropButtons;
    if (i < 16)  // Octave up button
    {
      seq[j] = pattern->getNote(i % 16)->octave == 1;
    }
    else if (i < 32)  // Octave down button
    {
      seq[j] = pattern->getNote(i % 16)->octave == -1;
    }
    else if (i < 48)
    {
      seq[j] = pattern->getNote(i % 16)->accent;
    }
    else if (i < 64)
    {
      seq[j] = pattern->getNote(i % 16)->slide;
    }
    else if (i < 80)
    {
      seq[j] = pattern->getNote(i % 16)->gate;
    }
#ifdef _WIN32
#ifdef _DEBUG
    OutputDebugStringW(seq[j] ? L"*" : L"-");
#endif  // _DEBUG
#endif
  }
#ifdef _WIN32
#ifdef _DEBUG
  OutputDebugStringW(L"\n");
#endif  // _DEBUG
#endif

  return seq;
}

std::string
BassMatrix::GetPatternName(int patternIdx)
{
  const char *noteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

  int octav = patternIdx / 12;
  int pattern = patternIdx % 12;
  std::string octavStr = (octav == 0) ? "Octav 2" : "Octav 3";

  return octavStr + " " + noteNames[pattern];
}

void
BassMatrix::CopyPattern(int fromPatternIdx, int toPatternIdx)
{
  if (fromPatternIdx < 0 || fromPatternIdx >= 24 || toPatternIdx < 0 || toPatternIdx >= 24)
  {
    return;
  }

  open303Core.sequencer.copyPattern(fromPatternIdx, toPatternIdx);
}


#if IPLUG_DSP

void
BassMatrix::ProcessBlock(PLUG_SAMPLE_DST **inputs, PLUG_SAMPLE_DST **outputs, int nFrames)
{
  assert(GetSampleRate() == open303Core.getSampleRate());

  PLUG_SAMPLE_DST *out01 = outputs[0];
  PLUG_SAMPLE_DST *out02 = outputs[1];

  if (open303Core.sequencer.getSequencerMode() != rosic::AcidSequencer::RUN)
  {
    open303Core.sequencer.setTempo(GetTempo());
  }

  if (open303Core.sequencer.getSequencerMode() == rosic::AcidSequencer::RUN &&
      !open303Core.sequencer.isRunning())
  {
    open303Core.noteOn(36, 64, 0.0);  // 36 seems to make C on sequencer be a C.
  }

  for (int offset = 0; offset < nFrames; ++offset)
  {
    if (open303Core.sequencer.getSequencerMode() == rosic::AcidSequencer::HOST_SYNC)
    {
      if (GetSamplePos() < 0.0)  // At least Cubase can return a negative value
      {
        *out01++ = *out02++ = 0.0;  // Silence
        continue;                   // Next frame
      }

      if (!GetTransportIsRunning())
      {
        // This lights the led that corresponds the position where the transport bar is set on.
        double maxSamplePos = GetSamplesPerBeat() * 4.0;
        int currentSampleInSequence =
            static_cast<int>(GetSamplePos()) % static_cast<int>(maxSamplePos);
        double samplesPerStep = maxSamplePos / 16.0;
        int currentStepInSequence = (int)((double)currentSampleInSequence / samplesPerStep);
        open303Core.sequencer.setStep(currentStepInSequence,
                                      -1);  // Let countdown be recalculated.
        *out01++ = *out02++ = 0.0;          // Silence
        continue;                           // Next frame
      }
      else if ((mStartSyncWithHost ||
                (mLastSamplePos != 0 && (mLastSamplePos + offset != GetSamplePos() + offset))))
      {  // Transport has changed
        mStartSyncWithHost = false;
        double maxSamplePos = GetSamplesPerBeat() * 4.0;
        int currentSampleInSequence =
            static_cast<int>(GetSamplePos()) % static_cast<int>(maxSamplePos);
        double samplesPerStep = maxSamplePos / 16.0;
        int currentStepInSequence = (int)((double)currentSampleInSequence / samplesPerStep);
        open303Core.sequencer.setStep(currentStepInSequence,
                                      -1);  // Let countdown be recalculated.
        mLastSamplePos = 0;  // We hope that a change doesn't occurs twice in a ProcessBlock() call.
      }
    }

    // Handle the loop knob. Only relevant in run mode.
    if (open303Core.sequencer.getSequencerMode() == rosic::AcidSequencer::RUN)
    {
      if (mKnobLoopSize > 1)
      {
        if (open303Core.sequencer.getStep() == 0 && !mHasChanged)
        {
          mHasChanged = true;
          mCurrentPattern = (mCurrentPattern + 1) % mKnobLoopSize;
          open303Core.sequencer.setPattern(mCurrentPattern);
          mSelectedOctav = mCurrentPattern / 12;
          mSelectedPattern = mCurrentPattern % 12;
          mSequencerSender.PushData({ kCtrlTagSeq0, { CollectSequenceButtons(open303Core) } });
          mSelectedOctavSender.PushData({ kCtrlTagOctav0, { mSelectedOctav } });
          mSelectedPatternSender.PushData({ kCtrlTagPattern0, { mSelectedPattern } });
        }
        if (open303Core.sequencer.getStep() != 0)
        {
          mHasChanged = false;
        }
      }
    }

    while (!mMidiQueue.Empty())
    {
      IMidiMsg msg = mMidiQueue.Peek();
      if (msg.mOffset > offset)
        break;

      if (open303Core.sequencer.getSequencerMode() == rosic::AcidSequencer::KEY_SYNC ||
          open303Core.sequencer.getSequencerMode() == rosic::AcidSequencer::MIDI_PLAY)
      {
        if (msg.StatusMsg() == IMidiMsg::kNoteOn)
        {
          open303Core.noteOn(msg.NoteNumber(), msg.Velocity(), 0.0);
          open303Core.sequencer.start();
        }
        else if (msg.StatusMsg() == IMidiMsg::kNoteOff)
        {
          open303Core.noteOn(msg.NoteNumber(), 0, 0.0);
          open303Core.sequencer.stop();
        }
      }
      else if (open303Core.sequencer.getSequencerMode() == rosic::AcidSequencer::HOST_SYNC)
      {
        // Select the pattern according to msg.NoteNumber().
        if (msg.StatusMsg() == IMidiMsg::kNoteOn && msg.NoteNumber() >= 48 && msg.NoteNumber() < 72)
        {
          int noteOffset = msg.NoteNumber() - 48;
          open303Core.sequencer.setPattern(noteOffset);
          mSelectedOctav = (msg.NoteNumber() <= 59) ? 0 : 1;
          mSelectedPattern = noteOffset - mSelectedOctav * 12;
          mSequencerSender.PushData({ kCtrlTagSeq0, { CollectSequenceButtons(open303Core) } });
          mSelectedOctavSender.PushData({ kCtrlTagOctav0, { mSelectedOctav } });
          mSelectedPatternSender.PushData({ kCtrlTagPattern0, { mSelectedPattern } });
        }

        assert(mSelectedPattern >= 0 && mSelectedPattern < 12 && mSelectedOctav >= 0 &&
               mSelectedOctav <= 1);
        open303Core.noteOn(36, 64, 0.0);  // 36 seems to make C on sequencer be a C.
        open303Core.sequencer.start();
      }
      if (msg.StatusMsg() == IMidiMsg::kNoteOff && msg.NoteNumber() >= 48 && msg.NoteNumber() < 72)
      {
        open303Core.allNotesOff();
        open303Core.sequencer.stop();
      }
      mMidiQueue.Remove();
    }

    rosic::AcidNote note;
    bool onNew16th = false;

    double tb303Oout = open303Core.getSample(note, onNew16th);
    if (onNew16th)
    {
      mLedSeqSender.PushData({ kCtrlTagLedSeq0, { open303Core.sequencer.getStep() } });
    }

    if (mUseEffects)
    {
      double drive = std::tanh(tb303Oout * GetParam(kParamDrive)->Value());
      double tubeOut = processAcidTubeSaturatorBlock(GetSampleRate(), drive);
      std::pair<double, double> delayOut = processDelayReverbAudioBlock(GetSampleRate(), tubeOut);
      std::pair<double, double> compOut = processCompressorBlock(GetSampleRate(), delayOut);
      *out01++ = compOut.first;
      *out02++ = compOut.second;
    }
    else
    {
      *out01++ = tb303Oout;
      *out02++ = tb303Oout;
    }
  }
  mLastSamplePos = static_cast<unsigned int>(GetSamplePos()) + nFrames;

  mMidiQueue.Flush(nFrames);
}

void
BassMatrix::OnIdle()
{
  mLedSeqSender.TransmitData(*this);
  mSequencerSender.TransmitData(*this);
  //  mPatternSender.TransmitData(*this);
  mSelectedOctavSender.TransmitData(*this);
  mSelectedPatternSender.TransmitData(*this);
  mSelectedModeSender.TransmitData(*this);

#ifndef WAM_API
  // Update the plugin scale.
  if (GetUI())
  {
    mPlugUIScale = GetUI()->GetDrawScale();
  }
#endif  // WAM_API
}

#endif  // IPLUG_DSP

#if IPLUG_EDITOR
#if defined(VST3_API) || defined(AU_API)
IGraphics *
BassMatrix::CreateGraphics()
{
  IGraphics *p;
  p = IGEditorDelegate::CreateGraphics();

  // Refresh sequencer.
  // The OnParamReset(kPresetRecall) call in UnserializeState will not be able to update
  // the sequencer gui because the graphics have not been created and therefore the
  // message from mSequencerSender will be lost.
  mSequencerSender.PushData({ kCtrlTagSeq0, { CollectSequenceButtons(open303Core) } });
  mSelectedModeSender.PushData({ kCtrlTagPlayMode0, { mSelectedPlayMode } });
  mSelectedOctavSender.PushData({ kCtrlTagOctav0, { mSelectedOctav } });
  mSelectedPatternSender.PushData({ kCtrlTagPattern0, { mSelectedPattern } });

  return p;
}
#endif  // VST3_API or AU_API
#endif  // IPLUG_EDITOR

#if IPLUG_DSP
void
BassMatrix::ProcessMidiMsg(const IMidiMsg &msg)
{
  TRACE;
  mMidiQueue.Add(msg);  // Take care of MIDI events in ProcessBlock()
}

void
BassMatrix::OnReset()
{
  // NOTE: OnReset() is called after a preset have been
  // loaded, so be sure you really want to reset the parameter.

  open303Core.setSampleRate(GetSampleRate());

  // Some internal stuff. Maybe we need to change this to sound more as a real TB-303?
  // Don't move this code, because Square wave stops to work then.
  open303Core.filter.setMode(rosic::TeeBeeFilter::TB_303);  // Should be LP_12
  open303Core.setAmpSustain(-60.0);
  open303Core.setTanhShaperDrive(36.9);
  open303Core.setTanhShaperOffset(4.37);
  open303Core.setPreFilterHighpass(44.5);
  open303Core.setFeedbackHighpass(150.0);
  open303Core.setPostFilterHighpass(24.0);
  open303Core.setSquarePhaseShift(189.0);

  if (!mHasLoadingPresets)
  {
#ifdef APP_API
    open303Core.sequencer.setMode(rosic::AcidSequencer::RUN);
    open303Core.sequencer.stop();
#else
    open303Core.sequencer.setMode(rosic::AcidSequencer::HOST_SYNC);
#endif
    mSelectedPattern = 9;
    open303Core.sequencer.randomizePattern(mSelectedPattern);
    open303Core.sequencer.setPattern(mSelectedPattern);
    mSequencerSender.PushData({ kCtrlTagSeq0, { CollectSequenceButtons(open303Core) } });
    mSelectedPatternSender.PushData({ kCtrlTagPattern0, { mSelectedPattern } });
    mSelectedOctavSender.PushData({ kCtrlTagOctav0, { mSelectedOctav } });
  }
}
#endif

#if IPLUG_DSP

#if defined(VST3_API) || defined(AU_API)
void
BassMatrix::OnParamChangeUI(int paramIdx, EParamSource source)
#else
void
BassMatrix::OnParamChange(int paramIdx)
#endif  // VST3_API or AU_API
{

#if defined(VST3_API) || defined(AU_API)
  if (source != kUI && source != kReset && source != kPresetRecall && source != kHost)
  {
    return;
  }
#endif  // VST3_API or AU_API

  double value = GetParam(paramIdx)->Value();

  // Note buttons
  if (paramIdx >= kBtnSeq0 && paramIdx < kBtnSeq0 + kNumberOfSeqButtons - kNumberOfTotalPropButtons)
  {
#if defined(VST3_API) || defined(AU_API)
    if (source == kPresetRecall)
    {
      return;
    }
    if (source == kReset)
    {
      return;
    }
#endif  // VST3_API or AU_API

    int seqNr = (paramIdx - kBtnSeq0) % 16;
    int noteNr = kNumberOfNoteBtns - (paramIdx - kBtnSeq0) / 16 - 1;  // noteNr between 0 and 12
    rosic::AcidPattern *pattern =
        open303Core.sequencer.getPattern(open303Core.sequencer.getActivePattern());

    if (value == 1.0)
    {
#ifdef _WIN32
#ifdef _DEBUG
      OutputDebugStringW(std::wstring(L"Setting step " + to_wstring(seqNr) + L" Note nr " +
                                      to_wstring(noteNr) + L"\n")
                             .c_str());
#endif  // _DEBUG
#endif
      pattern->setKey(seqNr, noteNr);  // Take care of the key notes
    }
    else
    {
      return;
    }
    return;
  }

  // Note properties buttons
  if (paramIdx >= kBtnProp0 && paramIdx < kBtnProp0 + kNumberOfTotalPropButtons)
  {
#if defined(VST3_API) || defined(AU_API)
    if (source == kPresetRecall)
    {
      return;
    }
    if (source == kReset)
    {
      return;
    }
#endif  // VST3_API or AU_API

    int seqNr = (paramIdx - kBtnProp0) % 16;
    int rowNr = (paramIdx - kBtnProp0) / 16;
    rosic::AcidPattern *pattern =
        open303Core.sequencer.getPattern(open303Core.sequencer.getActivePattern());
    if (rowNr == 0)
    {
      if (value == 1.0)
      {
        pattern->setOctave(seqNr, 1);
      }
      else
      {
        pattern->setOctave(seqNr, 0);
      }
    }
    if (rowNr == 1)
    {
      if (value == 1.0)
      {
        pattern->setOctave(seqNr, -1);
      }
      else
      {
        pattern->setOctave(seqNr, 0);
      }
    }
    if (rowNr == 2)
    {
      pattern->setAccent(seqNr, value == 1.0 ? true : false);
    }
    if (rowNr == 3)
    {
      pattern->setSlide(seqNr, value == 1.0 ? true : false);
    }
    if (rowNr == 4)
    {
      pattern->setGate(seqNr, value == 1.0 ? true : false);
    }
    return;
  }

  // Pattern selection buttons
  if (paramIdx >= kParamPattern0 && paramIdx <= kParamPattern0 + 11)
  {
    if (value == 1.0)
    {
      open303Core.sequencer.setPattern(12 * open303Core.sequencer.getPatternMultiplier() +
                                       paramIdx - kParamPattern0);
      mSequencerSender.PushData({ kCtrlTagSeq0, { CollectSequenceButtons(open303Core) } });
    }
    return;
  }

  if (paramIdx >= kParamPlayMode0 && paramIdx <= kParamPlayMode0 + 4)
  {
    if (value == 1.0)
    {
      mSelectedPlayMode = paramIdx - kParamPlayMode0;
      open303Core.sequencer.setMode(mSelectedPlayMode);
      mStartSyncWithHost = (mSelectedPlayMode == rosic::AcidSequencer::HOST_SYNC);
      if (mSelectedPlayMode == rosic::AcidSequencer::MIDI_PLAY ||
          mSelectedPlayMode == rosic::AcidSequencer::OFF ||
          mSelectedPlayMode == rosic::AcidSequencer::KEY_SYNC)
      {
        open303Core.sequencer.stop();
      }
      else  // rosic::AcidSequencer::HOST_SYNC or rosic::AcidSequencer::RUN
      {
        open303Core.sequencer.stop();
      }
    }
  }

  if (paramIdx == kParamOct0)
  {
    if (value == 1.0)
    {
      mSelectedOctav = 0;
      open303Core.sequencer.setPatternMultiplier(0);
      int patternNr = open303Core.sequencer.getActivePattern();
      if (patternNr >= 12)
      {
        open303Core.sequencer.setPattern(patternNr - 12);
      }
      mSequencerSender.PushData({ kCtrlTagSeq0, { CollectSequenceButtons(open303Core) } });
      mSelectedOctavSender.PushData({ kCtrlTagOctav0, { mSelectedOctav } });
    }
  }

  if (paramIdx == kParamOct0 + 1)
  {
    if (value == 1.0)
    {
      mSelectedOctav = 1;
      open303Core.sequencer.setPatternMultiplier(1);
      int patternNr = open303Core.sequencer.getActivePattern();
      if (patternNr < 12)
      {
        open303Core.sequencer.setPattern(patternNr + 12);
      }
      mSequencerSender.PushData({ kCtrlTagSeq0, { CollectSequenceButtons(open303Core) } });
      mSelectedOctavSender.PushData({ kCtrlTagOctav0, { mSelectedOctav } });
    }
  }

  switch (paramIdx)
  {
    case kKnobLoopSize: mKnobLoopSize = static_cast<int>(value); break;
    case kParamResonance: open303Core.setResonance(value); break;
    case kParamCutOff: open303Core.setCutoff(value); break;
    case kParamWaveForm:
      if (value == 1.0)
      {
        open303Core.setWaveform(1.0);  // Square
      }
      else
      {
        open303Core.setWaveform(0.0);  // Saw
      }
      break;
    case kParamEffects: mUseEffects = (value == 1.0) ? true : false; break;
    case kParamTuning: open303Core.setTuning(value); break;
    case kParamEnvMode: open303Core.setEnvMod(value); break;
    case kParamDecay: open303Core.setDecay(value); break;
    case kParamAccent: open303Core.setAccent(value); break;
    case kParamVolume: open303Core.setVolume(value); break;
    case kParamTempo:
      assert(value >= 10);
      assert(value <= 300);
      open303Core.sequencer.setTempo(value);
      break;
    case kParamCopy:
      if (value == 1.0)
      {

        // Copy button
        {
          if (GetUI())
          {
            // Step 1: Show menu to select SOURCE pattern
            IPopupMenu sourceMenu;
            for (int octav = 0; octav < 2; octav++)
            {
              for (int pattern = 0; pattern < 12; pattern++)
              {
                int idx = octav * 12 + pattern;
                std::string name = "Copy from: " + GetPatternName(idx);
                sourceMenu.AddItem(name.c_str());
              }
            }

            sourceMenu.SetFunction(
                [this](IPopupMenu *pSelectedMenu)
                {
                  int sourceIdx = pSelectedMenu->GetChosenItemIdx();

#ifdef _WIN32
                  OutputDebugStringA(
                      ("Source chosen: " + std::to_string(sourceIdx) + "\n").c_str());
#endif

                  if (sourceIdx >= 0 && sourceIdx < 24)
                  {
                    // Step 2: Show menu to select DESTINATION pattern
                    IPopupMenu destMenu;
                    for (int octav = 0; octav < 2; octav++)
                    {
                      for (int pattern = 0; pattern < 12; pattern++)
                      {
                        int idx = octav * 12 + pattern;
                        if (idx != sourceIdx)  // Don't show source as destination
                        {
                          std::string name = "Copy to: " + GetPatternName(idx);
                          destMenu.AddItem(name.c_str());
                        }
                      }
                    }

                    destMenu.SetFunction(
                        [this, sourceIdx](IPopupMenu *pDestMenu)
                        {
                          int destChosenIdx = pDestMenu->GetChosenItemIdx();

#ifdef _WIN32
                          OutputDebugStringA(
                              ("Dest chosen idx: " + std::to_string(destChosenIdx) + "\n").c_str());
#endif

                          if (destChosenIdx >= 0 &&
                              destChosenIdx < 23)  // 23 because we excluded one
                          {
                            // Map the chosen index back to actual pattern index
                            // (accounting for skipped source pattern)
                            int destIdx = destChosenIdx;
                            if (destChosenIdx >= sourceIdx)
                            {
                              destIdx++;  // Adjust for the skipped source pattern
                            }

#ifdef _WIN32
                            OutputDebugStringA(("Copying from " + std::to_string(sourceIdx) +
                                                " to " + std::to_string(destIdx) + "\n")
                                                   .c_str());
#endif

                            CopyPattern(sourceIdx, destIdx);

                            // Update the sequencer display if the current pattern was modified
                            if (destIdx == (mSelectedOctav * 12 + mSelectedPattern))
                            {
                              mSequencerSender.PushData(
                                  { kCtrlTagSeq0,
                                    { CollectSequenceButtons(open303Core, destIdx) } });
                            }
                          }

                          // Always reset the copy button when the destination menu is closed
                          GetParam(kParamCopy)->Set(0.0);
                        });

                    IControl *pControl = GetUI()->GetControlWithTag(kCtrlTagBtnCopy);
                    if (pControl)
                      GetUI()->CreatePopupMenu(*pControl, destMenu, pControl->GetRECT());
                  }
                  else
                  {
                    // User cancelled the source menu, reset button
                    GetParam(kParamCopy)->Set(0.0);
                  }
                });
            IControl *pControl = GetUI()->GetControlWithTag(kCtrlTagBtnCopy);
            if (pControl)
              GetUI()->CreatePopupMenu(*pControl, sourceMenu, pControl->GetRECT());
          }
          return;
        }
      }
      break;
    case kParamClear:
      if (value == 1.0)
      {
        open303Core.sequencer.clearPattern(open303Core.sequencer.getActivePattern());
        mSequencerSender.PushData({ kCtrlTagSeq0, { CollectSequenceButtons(open303Core) } });
      }
      break;
    case kParamRandomize:
      if (value == 1.0)
      {
        open303Core.sequencer.randomizePattern(open303Core.sequencer.getActivePattern());
        mSequencerSender.PushData({ kCtrlTagSeq0, { CollectSequenceButtons(open303Core) } });
      }
      break;
    default: break;
  }
}

bool
BassMatrix::OnMessage(int msgTag, int ctrlTag, int dataSize, const void *pData)
{
  return false;
}
#endif
