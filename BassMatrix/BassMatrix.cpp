#include "BassMatrix.h"
#include "IPlug_include_in_plug_src.h"
#include "BassMatrixControls.h"
#include "open303/Source/DSPCode/rosic_Open303.h"
#include <filesystem>
#include <fstream>


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

BassMatrix::BassMatrix(const InstanceInfo &info) :
  Plugin(info, MakeConfig(kNumParams, kNumPresets)),
  mLastSamplePos(0),
  mStartSyncWithHost(false),
  mPlugUIScale(1.0),
  mCurrentPattern(0)
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
  open303Core.sequencer.setMode(rosic::AcidSequencer::RUN);

  //
  // Setup parameters and their default values
  //
  GetParam(kParamCutOff)->InitDouble("Cut off", 500.0, 314.0, 2394.0, 1.0, "Hz");
  GetParam(kParamResonance)->InitDouble("Resonace", 50.0, 0.0, 100.0, 1.0, "%");
  GetParam(kParamTuning)->InitDouble("Tuning", 440.0, 400.0, 480.0, 1.0, "%");
  GetParam(kParamEnvMode)->InitDouble("Env mode", 25.0, 0.0, 100.0, 1.0, "%");
  GetParam(kParamDecay)->InitDouble("Decay", 400.0, 200.0, 2000.0, 1.0, "ms");
  GetParam(kParamAccent)->InitDouble("Accent", 50.0, 0.0, 100.0, 1.0, "%");
  GetParam(kParamVolume)->InitDouble("Volume", -17.0, -75.0, 0.0, 0.1, "dB");
  GetParam(kParamTempo)->InitDouble("Tempo", 120.0, 10.0, 300.0, 1.0, "bpm");
  GetParam(kParamDrive)->InitDouble("Drive", 36.9, 0.0, 50.0, 1.0, "bpm");

  GetParam(kParamWaveForm)->InitBool("Waveform", false);
  GetParam(kParamStop)->InitBool("Stop", false);
  GetParam(kParamHostSync)->InitBool("Host Sync", false);
  GetParam(kParamKeySync)->InitBool("Key Sync", false);
#ifdef VST3_API
  GetParam(kParamInternalSync)->InitBool("Internal Sync", false);
  GetParam(kParamMidiPlay)->InitBool("Midi Play", true);
#else
  GetParam(kParamInternalSync)->InitBool("Internal Sync", true);
  GetParam(kParamMidiPlay)->InitBool("Midi Play", false);
#endif


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

  for (int i = kBtnPtnC; i < kBtnPtnC + 12; ++i)
  {
    GetParam(i)->InitBool(("Pattern button" + std::to_string(i - kBtnPtnC)).c_str(),
                          false,
                          "On/Off",
                          IParam::kFlagCannotAutomate);
  }

  GetParam(kBtnPtnOct2)
      ->InitBool("Octav 2",
                 false,
                 "On/Off",
                 IParam::kFlagCannotAutomate);  // It's bad to set something to true here!!
  GetParam(kBtnPtnOct3)
      ->InitBool("Octav 3",
                 false,
                 "On/Off",
                 IParam::kFlagCannotAutomate);  // It's bad to set something to true here!!
  //  GetParam(kBtnPtnC)->InitBool("Pattern C", false, "On/Off", IParam::kFlagCannotAutomate);  // It's bad to set something to true here!!

  GetParam(kKnobLoopSize)->InitInt("Loop size", 1, 1, 24);

  GetParam(kParamCopy)->InitBool("Pattern copy", false);
  GetParam(kParamClear)->InitBool("Pattern clear", false);
  GetParam(kParamRandomize)->InitBool("Pattern randomize", false);

    
    
#if IPLUG_EDITOR // http://bit.ly/2S64BDd
  mMakeGraphicsFunc = [&]() {
    return MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS, GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT));
  };
  
  mLayoutFunc = [&](IGraphics* pGraphics) {
    pGraphics->AttachCornerResizer(EUIResizerMode::Scale, false);
//    pGraphics->AttachPanelBackground(COLOR_GRAY);
    // Background
    pGraphics->LoadBitmap(BACKGROUND_FN, 1, true);
    pGraphics->AttachBackground(BACKGROUND_FN);

    // Knobs
        const IBitmap knobRotateBitmap = pGraphics->LoadBitmap(PNG6062_FN, 127);
        const IBitmap knobLittleBitmap = pGraphics->LoadBitmap(PNGFX1LITTLE_FN, 127);
        const IBitmap knobBigBitmap = pGraphics->LoadBitmap(PNGFX1BIG_FN, 61);
        //    pGraphics->AttachControl(new IBKnobControl(210, 30, knobLittleBitmap, kParamWaveForm));
        const IBitmap btnWaveFormBitmap = pGraphics->LoadBitmap(PNGWAVEFORM_FN, 2, true);
    pGraphics->AttachControl(new IBSwitchControl(200, 50, btnWaveFormBitmap, kParamWaveForm),
                             kCtrlWaveForm);
        pGraphics->AttachControl(new IBKnobControl(310, 30, knobLittleBitmap, kParamTuning));
        pGraphics->AttachControl(new IBKnobControl(410, 30, knobLittleBitmap, kParamCutOff));
        pGraphics->AttachControl(new IBKnobControl(510, 30, knobLittleBitmap, kParamResonance));
        pGraphics->AttachControl(new IBKnobControl(610, 30, knobLittleBitmap, kParamEnvMode));
        pGraphics->AttachControl(new IBKnobControl(710, 30, knobLittleBitmap, kParamDecay));
        pGraphics->AttachControl(new IBKnobControl(810, 30, knobLittleBitmap, kParamAccent));
    
    pGraphics->AttachControl(new IBKnobControl(0 + 210 - 175, 130, knobBigBitmap, kParamTempo));
    //    pGraphics->AttachControl(new IBKnobControl(510, 130, knobBigBitmap, kParamDrive));
    pGraphics->AttachControl(new IBKnobControl(1130 - 210, 130, knobBigBitmap, kParamVolume));

    // Pattern buttons
    const IBitmap btnPatternOctav2Bitmap = pGraphics->LoadBitmap(PNGBTNPATOCTAV2_FN, 2, true);
    pGraphics->AttachControl(new PatternBtnControl(485,
                                                   138,
                                                   btnPatternOctav2Bitmap,
                                                   kBtnPtnOct2,
                                                   kCtrlTagBtnPtnOct2,
                                                   open303Core),
                             kCtrlTagBtnPtnOct2);
    const IBitmap btnPatternOctav3Bitmap = pGraphics->LoadBitmap(PNGBTNPATOCTAV3_FN, 2, true);
    pGraphics->AttachControl(new PatternBtnControl(485.f + btnPatternOctav2Bitmap.FW() - 3,
                                                   138,
                                                   btnPatternOctav3Bitmap,
                                                   kBtnPtnOct3,
                                                   kCtrlTagBtnPtnOct3,
                                                   open303Core),
                             kCtrlTagBtnPtnOct3);
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
      pGraphics->AttachControl(new PatternBtnControl(481.f +
                                                         (i % 4) * (btnPatternBitmap[0].W() / 2),
                                                     178.f + (i / 4) * (btnPatternBitmap[0].H()),
                                                     btnPatternBitmap[i],
                                                     kBtnPtnC + i,
                                                     kCtrlTagBtnPtnC + i,
                                                     open303Core),
                               kCtrlTagBtnPtnC + i);
    }

    // Pattern control buttons
    const IBitmap btnClearBitmap = pGraphics->LoadBitmap(PNGCLEAR_FN, 2, true);
    pGraphics->AttachControl(new PtnModBtnControl(369, 178, btnClearBitmap, kParamClear));
    const IBitmap btnRandomizeBitmap = pGraphics->LoadBitmap(PNGRANDOMIZE_FN, 2, true);
    pGraphics->AttachControl(new PtnModBtnControl(369, 218, btnRandomizeBitmap, kParamRandomize));
    //    const IBitmap btnCopyBitmap = pGraphics->LoadBitmap(PNGCOPY_FN, 2, true);
    //    pGraphics->AttachControl(new PtnModBtnControl(369, 258, btnCopyBitmap, kParamCopy));

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
                                 kCtrlTagBtnProp0 + 16 * j + i,
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
                                 kCtrlTagBtnSeq0 + 16 * j + i,
                                 "Sequencer");
      }
    }

    const int yVal = 790;
    const int xBase = 310;
    const IBitmap btnStopBitmap = pGraphics->LoadBitmap(PNGSTOP_FN, 2, true);
    pGraphics->AttachControl(new SyncBtnControl(xBase + btnStopBitmap.W() / 2 * 0,
                                                yVal,
                                                btnStopBitmap,
                                                kParamStop,
                                                kCtrlTagStop),
                             kCtrlTagStop);
    const IBitmap btnHostSyncBitmap = pGraphics->LoadBitmap(PNGHOSTSYNC_FN, 2, true);
    pGraphics->AttachControl(new SyncBtnControl(xBase + btnStopBitmap.W() / 2 * 1,
                                                yVal,
                                                btnHostSyncBitmap,
                                                kParamHostSync,
                                                kCtrlTagHostSync),
                             kCtrlTagHostSync);
    const IBitmap btnKeySyncBitmap = pGraphics->LoadBitmap(PNGKEYSYNC_FN, 2, true);
    pGraphics->AttachControl(new SyncBtnControl(xBase + btnStopBitmap.W() / 2 * 2,
                                                yVal,
                                                btnKeySyncBitmap,
                                                kParamKeySync,
                                                kCtrlTagKeySync),
                             kCtrlTagKeySync);
    const IBitmap btnInternalSyncBitmap = pGraphics->LoadBitmap(PNGINTERNALSYNC_FN, 2, true);
    pGraphics->AttachControl(new SyncBtnControl(xBase + btnStopBitmap.W() / 2 * 3,
                                                yVal,
                                                btnInternalSyncBitmap,
                                                kParamInternalSync,
                                                kCtrlTagInternalSync),
                             kCtrlTagInternalSync);
    const IBitmap btnMidiPlayBitmap = pGraphics->LoadBitmap(PNGMIDIPLAY_FN, 2, true);
    pGraphics->AttachControl(new SyncBtnControl(xBase + btnStopBitmap.W() / 2 * 4,
                                                yVal,
                                                btnMidiPlayBitmap,
                                                kParamMidiPlay,
                                                kCtrlTagMidiPlay),
                             kCtrlTagMidiPlay);
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
#ifdef VST3_API
bool
BassMatrix::SerializeState(IByteChunk &chunk) const
{
#ifdef _DEBUG
  OutputDebugString(L"SerializeState() called\n");
#endif

  TRACE

  bool savedOK = true;

  // Set version of the preset format.
  double version = 1.1;
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
#ifdef _DEBUG
      OutputDebugString(v == 1.0 ? L"*" : L"-");
#endif  // _DEBUG
      savedOK &= (chunk.Put(&v) > 0);
    }
#ifdef _DEBUG
    OutputDebugString(L"\n");
#endif  // _DEBUG
  }

  // Save current octav and current pattern.
  //  double oct2 = GetParam(kBtnPtnOct2)->Value();
  double oct3 = GetParam(kBtnPtnOct3)->Value();
  double ptn;
  for (int i = kBtnPtnC; i < kBtnPtnC + 12; ++i)
  {
    if (GetParam(i)->Value() == 1.0)
    {
      ptn = static_cast<double>(i - kBtnPtnC);
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
#ifdef _DEBUG
  OutputDebugString(L"UnserializeState() called\n");
#endif

  TRACE

  ENTER_PARAMS_MUTEX

  int n = NParams(), pos = startPos;

  // Check version for the preset format
  double version;
  pos = chunk.Get(&version, pos);
  assert(version == 1.1);

  for (int i = kParamCutOff; i < n && pos >= 0; ++i)
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

#ifdef _DEBUG
      OutputDebugString(v == 1.0 ? L"*" : L"-");
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

#ifdef _DEBUG
      OutputDebugString(v == 1.0 ? L"*" : L"-");
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
#ifdef _DEBUG
    OutputDebugString(L"\n");
#endif  // _DEBUG
  }

  // Restore octav and pattern buttons.
  double ptn;
  pos = chunk.Get(&ptn, pos);  // ptn is between 0.0 and 23.0
                               //  open303Core.sequencer.setPattern(static_cast<int>(ptn));
  if (ptn < 12.0)
  {
    GetParam(kBtnPtnOct2)->Set(1.0);
    GetParam(kBtnPtnOct3)->Set(0.0);
  }
  else
  {
    GetParam(kBtnPtnOct2)->Set(0.0);
    GetParam(kBtnPtnOct3)->Set(1.0);
    ptn -= 12.0;
  }
  for (int i = kBtnPtnC; i < kBtnPtnC + 12; ++i)
  {
    if (static_cast<int>(ptn) == i - kBtnPtnC)
    {
      GetParam(i)->Set(1.0);
    }
    else
    {
      GetParam(i)->Set(0.0);
    }
  }

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

#if IPLUG_DSP

void
BassMatrix::ProcessBlock(PLUG_SAMPLE_DST **inputs, PLUG_SAMPLE_DST **outputs, int nFrames)
{
  // Channel declaration.
  PLUG_SAMPLE_DST *out01 = outputs[0];
  PLUG_SAMPLE_DST *out02 = outputs[1];

  // No sample accurate leds, because they will not be accurate anyway.
  mLedSeqSender.PushData({ kCtrlTagLedSeq0, { open303Core.sequencer.getStep() } });

  if (open303Core.sequencer.getSequencerMode() == rosic::AcidSequencer::HOST_SYNC ||
      open303Core.sequencer.getSequencerMode() == rosic::AcidSequencer::KEY_SYNC)
  {
    open303Core.sequencer.setTempo(GetTempo());
  }

  if ((open303Core.sequencer.getSequencerMode() == rosic::AcidSequencer::RUN ||
       open303Core.sequencer.getSequencerMode() == rosic::AcidSequencer::HOST_SYNC) &&
      !open303Core.sequencer.isRunning())
  {
    open303Core.noteOn(36, 64, 0.0);  // 36 seems to make C on sequencer be a C.
  }

  if (open303Core.sequencer.getSequencerMode() != rosic::AcidSequencer::OFF)
  {
    if (open303Core.sequencer.getUpdateSequenserGUI())
    {
      open303Core.sequencer.setUpdateSequenserGUI(false);

      mSequencerSender.PushData({ kCtrlTagBtnSeq0, { CollectSequenceButtons(open303Core) } });

      // Push pattern buttons
      int pat;
      pat = open303Core.sequencer.getActivePattern();
      mPatternSender.PushData({ kCtrlTagBtnPtnC, { pat } });
    }
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
        // Start
        // This lights the led that corresponds the position where the transport bar is set on.
        //
        double maxSamplePos = GetSamplesPerBeat() * 4.0;
        int currentSampleInSequence =
            static_cast<int>(GetSamplePos()) % static_cast<int>(maxSamplePos);
        double samplesPerStep = maxSamplePos / 16.0;
        int currentStepInSequence = (int)((double)currentSampleInSequence / samplesPerStep);
        open303Core.sequencer.setStep(currentStepInSequence,
                                      -1);  // Let countdown be recalculated.

        // End
        // This lights the led that corresponds the position where the transport bar is set on.
        //


        *out01++ = *out02++ = 0.0;  // Silence
        continue;                   // Next frame
      }

      else if (  // GetTransportIsRunning() &&
          (mStartSyncWithHost ||
           mLastSamplePos != 0 && (mLastSamplePos + offset != GetSamplePos() + offset)))
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

    if (open303Core.sequencer.getSequencerMode() == rosic::AcidSequencer::RUN)
    {
      if (mKnobLoopSize > 1)
      {
        if (open303Core.sequencer.getStep() == 0 && !mHasChanged)
        {
          mHasChanged = true;
          mCurrentPattern = (mCurrentPattern + 1) % mKnobLoopSize;
          open303Core.sequencer.setPattern(mCurrentPattern);
          open303Core.sequencer.setUpdateSequenserGUI(true);
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
        }
        else if (msg.StatusMsg() == IMidiMsg::kNoteOff)
        {
          open303Core.noteOn(msg.NoteNumber(), 0, 0.0);
        }
      }
      else if (open303Core.sequencer.getSequencerMode() == rosic::AcidSequencer::HOST_SYNC ||
               open303Core.sequencer.getSequencerMode() == rosic::AcidSequencer::RUN)
      {
        if (msg.StatusMsg() == IMidiMsg::kNoteOn)
        {
          if (msg.NoteNumber() >= 48 && msg.NoteNumber() < 72)
          {
            open303Core.sequencer.setPattern(msg.NoteNumber() - 48);
            open303Core.sequencer.setUpdateSequenserGUI(true);
          }
        }
      }
      mMidiQueue.Remove();
    }

    rosic::AcidNote note;
    bool onNew16th = false;
    *out01++ = *out02++ = open303Core.getSample(note, onNew16th);

#ifdef VST3_API
    static int currentKey = 0;
    static double noteOffSamplePos = 0.0;
    // Send a midi message to midi output
    if (onNew16th)
    {
      int key = note.key + 12 * note.octave + 36;
      IMidiMsg midiMessage;
      midiMessage.MakeNoteOnMsg(key, note.accent ? 127 : 100, 0);
      IPlugVST3ProcessorBase::SendMidiMsg(midiMessage);
      // Calculate when the next note off should be sent.
      double secondsToNextStep = beatsToSeconds(0.25, GetTempo());
      double samplesToNextStep = secondsToNextStep * GetSampleRate();
      double samplesToNoteOff = samplesToNextStep * 0.90;
      noteOffSamplePos = GetSamplePos() + offset + samplesToNoteOff;
      currentKey = key;
    }
    else if (static_cast<int>(noteOffSamplePos) == static_cast<int>(GetSamplePos() + offset))
    {
      IMidiMsg midiMessage;
      midiMessage.MakeNoteOffMsg(currentKey, 0);
      IPlugVST3ProcessorBase::SendMidiMsg(midiMessage);
    }
#endif
  }

  mLastSamplePos = static_cast<unsigned int>(GetSamplePos()) + nFrames;

  mMidiQueue.Flush(nFrames);
#endif
}

#if IPLUG_DSP
void
BassMatrix::OnIdle()
{
  mLedSeqSender.TransmitData(*this);
  mSequencerSender.TransmitData(*this);
  mPatternSender.TransmitData(*this);

  // Update the plugin scale.
  if (GetUI())
  {
    mPlugUIScale = GetUI()->GetDrawScale();
  }
}
#endif

#if IPLUG_EDITOR
#ifdef VST3_API
IGraphics *
BassMatrix::CreateGraphics()
{
  IGraphics *p;
  p = IGEditorDelegate::CreateGraphics();

  // Refresh sequencer.
  // The OnParamReset(kPresetRecall) call in UnserializeState will not be able to update
  // the sequencer gui because the graphics have not been created and therefore the
  // message from mSequencerSender will be lost.
  mSequencerSender.PushData({ kCtrlTagBtnSeq0, { CollectSequenceButtons(open303Core) } });

  return p;
}
#endif  // API
#endif


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
}

void
BassMatrix::ProcessMidiMsg(const IMidiMsg &msg)
{
  TRACE;
  mMidiQueue.Add(msg);  // Take care of MIDI events in ProcessBlock()
}

#if IPLUG_DSP

#ifdef VST3_API
void
BassMatrix::OnParamChangeUI(int paramIdx, EParamSource source)
#else
void
BassMatrix::OnParamChange(int paramIdx)
#endif  // API
{

#ifdef VST3_API
  if (source != kUI && source != kReset && source != kPresetRecall && source != kHost)
  {
    return;
  }
#endif  // API

  double value = GetParam(paramIdx)->Value();

  // Note buttons
  if (paramIdx >= kBtnSeq0 && paramIdx < kBtnSeq0 + kNumberOfSeqButtons - kNumberOfTotalPropButtons)
  {
#ifdef VST3_API
    if (source == kPresetRecall)
    {
      return;
    }
    if (source == kReset)
    {
      return;
    }
#endif  // API

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
#endif                                 // _DEBUG
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
#ifdef VST3_API
    if (source == kPresetRecall)
    {
      return;
    }
    if (source == kReset)
    {
      return;
    }
#endif  // API

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
  if (paramIdx >= kBtnPtnC && paramIdx <= kBtnPtnC + 11)
  {
    //#ifdef VST3_API
    //    if (source == kUI && GetTransportIsRunning() &&
    //        open303Core.sequencer.getSequencerMode() == rosic::AcidSequencer::HOST_SYNC)
    //    {
    //      return;
    //    }
    //#endif
    if (value == 1.0)
    {
      open303Core.sequencer.setPattern(12 * open303Core.sequencer.getPatternMultiplier() +
                                       paramIdx - kBtnPtnC);
      mSequencerSender.PushData({ kCtrlTagBtnSeq0, { CollectSequenceButtons(open303Core) } });
    }
    return;
  }

  switch (paramIdx)
  {
    case kBtnPtnOct2:
      //#ifdef VST3_API
      //      if (source == kUI && GetTransportIsRunning() &&
      //          open303Core.sequencer.getSequencerMode() == rosic::AcidSequencer::HOST_SYNC)
      //      {
      //        return;
      //      }
      //#endif
      if (value == 1.0)
      {
        open303Core.sequencer.setPatternMultiplier(0);
        int patternNr = open303Core.sequencer.getActivePattern();
        if (patternNr >= 12)
        {
          open303Core.sequencer.setPattern(patternNr - 12);
        }
        mSequencerSender.PushData({ kCtrlTagBtnSeq0, { CollectSequenceButtons(open303Core) } });
      }
      break;
    case kBtnPtnOct3:
      //#ifdef VST3_API
      //      if (source == kUI && GetTransportIsRunning() &&
      //          open303Core.sequencer.getSequencerMode() == rosic::AcidSequencer::HOST_SYNC)
      //      {
      //        return;
      //      }
      //#endif
      if (value == 1.0)
      {
        open303Core.sequencer.setPatternMultiplier(1);
        int patternNr = open303Core.sequencer.getActivePattern();
        if (patternNr < 12)
        {
          open303Core.sequencer.setPattern(patternNr + 12);
        }
        mSequencerSender.PushData({ kCtrlTagBtnSeq0, { CollectSequenceButtons(open303Core) } });
      }
      break;
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
    case kParamDrive: open303Core.setTanhShaperDrive(value); break;
    case kParamStop:
      if (value == 1.0)
      {
        open303Core.sequencer.setMode(rosic::AcidSequencer::OFF);
      }
      break;
    case kParamHostSync:
      if (value == 1.0)
      {
        open303Core.sequencer.setMode(rosic::AcidSequencer::HOST_SYNC);
        mStartSyncWithHost = true;
      }
      break;
    case kParamInternalSync:
      if (value == 1.0)
      {
        open303Core.sequencer.setMode(rosic::AcidSequencer::RUN);
        mStartSyncWithHost = false;
      }
      break;
    case kParamKeySync:
      if (value == 1.0)
      {
        open303Core.sequencer.setMode(rosic::AcidSequencer::KEY_SYNC);
        mStartSyncWithHost = false;
      }
      break;
    case kParamMidiPlay:
      if (value == 1.0)
      {
        open303Core.sequencer.setMode(rosic::AcidSequencer::MIDI_PLAY);
        open303Core.sequencer.stop();
        mStartSyncWithHost = false;
      }
      break;
    case kParamCopy:
      if (value == 1.0)
      {
      }
      break;
    case kParamClear:
      if (value == 1.0)
      {
        open303Core.sequencer.clearPattern(open303Core.sequencer.getActivePattern());
        mSequencerSender.PushData({ kCtrlTagBtnSeq0, { CollectSequenceButtons(open303Core) } });
      }
      break;
    case kParamRandomize:
      if (value == 1.0)
      {
        open303Core.sequencer.randomizePattern(open303Core.sequencer.getActivePattern());
        mSequencerSender.PushData({ kCtrlTagBtnSeq0, { CollectSequenceButtons(open303Core) } });
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
