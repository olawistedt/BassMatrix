#include "BassMatrixControls.h"

SeqLedBtnControl::SeqLedBtnControl(float x,
                                   float y,
                                   const IBitmap &bitmap,
                                   int paramIdx,
                                   rosic::Open303 &in303) :
  IBSwitchControl(x, y, bitmap, paramIdx),
  open303Core(in303)
{
}

void
SeqLedBtnControl::OnMsgFromDelegate(int msgTag, int dataSize, const void *pData)
{
  if (!IsDisabled() && msgTag == ISender<>::kUpdateMessage)
  {
    IByteStream stream(pData, dataSize);

    int pos = 0;
    ISenderData<1, int> d;
    pos = stream.Get(&d, pos);

    // Turn off all leds
    for (int i = 0; i < 16; i++)
    {
      IControl *pControlOff = GetUI()->GetControlWithTag(kCtrlTagLedSeq0 + i);
      double before = pControlOff->GetValue();
      pControlOff->SetValue(0.0);
      if (before != pControlOff->GetValue())
      {
        pControlOff->SetDirty(true);
      }
    }

    int step = d.vals[0];
    //if (step == 0) { step = 15; }
    //else { step = (step - 1); }

    assert(step >= 0 && step <= 15);

    IControl *pControlOn = GetUI()->GetControlWithTag(kCtrlTagLedSeq0 + step);
    double before = pControlOn->GetValue();
    pControlOn->SetValue(1.0);
    if (before != pControlOn->GetValue())
    {
      pControlOn->SetDirty(true);
    }

    SetDirty(false);
  }
}
void
SeqLedBtnControl::OnMouseDown(float x, float y, const IMouseMod &mod)
{
  return;
}


// A button control that can take a message from the DSP
SeqNoteBtnControl::SeqNoteBtnControl(float x, float y, const IBitmap &bitmap, int paramIdx) :
  IBSwitchControl(x, y, bitmap, paramIdx),
  mParamIdx(paramIdx)
{
}

//
// This function updates one the GUI sequencer buttons. It is just in GUI visible things. The actual values
// is stored in Sequencer. So the sender of these notes should have read the values from the Sequencer.
//
void
SeqNoteBtnControl::SetSequencerButtons(std::array<bool, kNumberOfSeqButtons> sequencer,
                                       IGraphics *ui)
{
  Trace(TRACELOC, "");
  for (int i = 0; i < kNumberOfSeqButtons; i++)
  {
    IControl *pControlBtn = ui->GetControlWithTag(kCtrlTagSeq0 + i);
    double before = pControlBtn->GetValue();
#ifdef _WIN32
#ifdef _DEBUG
    OutputDebugStringW(sequencer[i] ? L"*" : L"-");
#endif  // _DEBUG
#endif
    pControlBtn->SetValue(sequencer[i] ? 1.0 : 0.0);
    if (before != pControlBtn->GetValue())
    {
      pControlBtn->SetDirty(true);
    }
  }
#ifdef _WIN32
#ifdef _DEBUG
  OutputDebugStringW(L"\n");
#endif  // _DEBUG
#endif
}

void
SeqNoteBtnControl::OnMsgFromDelegate(int msgTag, int dataSize, const void *pData)
{
  if (!IsDisabled() && msgTag == ISender<>::kUpdateMessage)
  {
    IByteStream stream(pData, dataSize);
    int pos = 0;
    ISenderData<1, std::array<bool, kNumberOfSeqButtons>> d;
    pos = stream.Get(&d, pos);
    std::array<bool, kNumberOfSeqButtons> sequencer = d.vals[0];

    SeqNoteBtnControl::SetSequencerButtons(sequencer, GetUI());

    SetDirty(false);
  }
}

void
SeqNoteBtnControl::OnMouseDown(float x, float y, const IMouseMod &mod)
{
  //  IBSwitchControl::OnMouseDown(x, y, mod);
  if (mParamIdx - kBtnSeq0 < kNumberOfSeqButtons - kNumberOfTotalPropButtons)
  {
    // For the notes. Turn off all note buttons on the same column and then turn on the button just pressed.
    for (int row = 0; row < kNumberOfNoteBtns; ++row)
    {
      int col = (mParamIdx - kBtnSeq0) % 16;
      IControl *pControlBtn = GetUI()->GetControlWithTag(kCtrlTagSeq0 + col + 16 * row);
      double before = pControlBtn->GetValue();
      pControlBtn->SetValue(0.0);
      if (before != 0.0)
      {
        pControlBtn->SetDirty(true);
      }
      if (kBtnSeq0 + col + row * 16 == mParamIdx)
      {
        pControlBtn->SetValue(1.0);
        if (before != 1.0)
        {
          pControlBtn->SetDirty(true);
        }
      }
    }
  }
  else
  {
    int col = (mParamIdx - kBtnSeq0) % 16;
    int row = (mParamIdx - kBtnSeq0) / 16;
    IControl *pControlBtn =
        GetUI()->GetControlWithTag(kCtrlTagProp0 + (row - kNumberOfNoteBtns) * 16 + col);
    if (row == kNumberOfNoteBtns || row == kNumberOfNoteBtns + 1)  // Up or down
    {                                                              // Up is pressed.
      IControl *pControlBtnUp;
      IControl *pControlBtnDown;

      if (row == kNumberOfNoteBtns)
      {
        pControlBtnUp = pControlBtn;
        pControlBtnDown = GetUI()->GetControlWithTag(kCtrlTagProp0 + col + 16);
      }
      else
      {
        pControlBtnUp = GetUI()->GetControlWithTag(kCtrlTagProp0 + col);
        pControlBtnDown = pControlBtn;
      }

      if (row == kNumberOfNoteBtns)  // Up
      {
        if (1.0 == pControlBtnUp->GetValue())
        {  // We wants neither up or down
          pControlBtnUp->SetValue(0.0);
          pControlBtnDown->SetValue(0.0);
        }
        else if (0.0 == pControlBtnUp->GetValue())
        {
          pControlBtnDown->SetValue(0.0);
          pControlBtnUp->SetValue(1.0);
        }
      }
      else  // Down
      {
        if (1.0 == pControlBtnDown->GetValue())
        {  // We wants neither up or down
          pControlBtnUp->SetValue(0.0);
          pControlBtnDown->SetValue(0.0);
        }
        else if (0.0 == pControlBtnDown->GetValue())
        {
          pControlBtnDown->SetValue(1.0);
          pControlBtnUp->SetValue(0.0);
        }
      }

      // The order of the calls to SetDirty() matters. In case one button is lit
      // and the other goes off. We want one who is lit to come last, so the
      // SetDirty() for that control should come last.
      if (pControlBtnDown->GetValue() == 1.0)
      {
        pControlBtnUp->SetDirty(true);
        pControlBtnDown->SetDirty(true);
      }
      else
      {
        pControlBtnDown->SetDirty(true);
        pControlBtnUp->SetDirty(true);
      }
    }
    else  // Accent, glide or gate
    {
      pControlBtn->SetValue(pControlBtn->GetValue() == 1.0 ? 0.0 : 1.0);
      pControlBtn->SetDirty(true);
    }
  }
}

PtnModBtnControl::PtnModBtnControl(float x, float y, const IBitmap &bitmap, int paramIdx) :
  IBSwitchControl(x, y, bitmap, paramIdx)
{
}

void
PtnModBtnControl::OnMouseDown(float x, float y, const IMouseMod &mod)
{
  SetValue(1.0);
  SetDirty();
}

void
PtnModBtnControl::OnMouseUp(float x, float y, const IMouseMod &mod)
{
  SetValue(0.0);
  SetDirty();
}


////////////////////////////////////////////////////////////////////////////////////////
// GroupBtnControl
////////////////////////////////////////////////////////////////////////////////////////
GroupBtnControl::GroupBtnControl(float x,
                                 float y,
                                 const IBitmap &bitmap,
                                 int paramIdx,
                                 int &selectedGroupTag,
                                 int thisTag,
                                 int groupStart,
                                 int nrOfGroupMembers) :
  IBSwitchControl(x, y, bitmap, paramIdx),
  mSelectedGroupTag(selectedGroupTag),
  mThisTag(thisTag),
  mGroupStart(groupStart),
  mNrOfGroupMembers(nrOfGroupMembers)
{
}

void
GroupBtnControl::OnMsgFromDelegate(int msgTag, int dataSize, const void *pData)
{
  if (GetUI())
  {
    if (!IsDisabled() && msgTag == ISender<>::kUpdateMessage)
    {
      IByteStream stream(pData, dataSize);
      int pos = 0;
      ISenderData<1, int> d;
      pos = stream.Get(&d, pos);
      mSelectedGroupTag = d.vals[0];

      for (int i = 0; i < mNrOfGroupMembers; i++)
      {
        IControl *pControlBtn = GetUI()->GetControlWithTag(mGroupStart + i);

        double before = pControlBtn->GetValue();

        if (i == mSelectedGroupTag)
        {
          pControlBtn->SetValue(1.0);
        }
        else
        {
          pControlBtn->SetValue(0.0);
        }
        if (before != pControlBtn->GetValue())
        {
          pControlBtn->SetDirty(true);
        }
      }
    }
  }
}

void
GroupBtnControl::OnMouseDown(float x, float y, const IMouseMod &mod)
{
  if (GetUI())
  {
    for (int i = 0; i < mNrOfGroupMembers; i++)
    {
      IControl *pControlBtn = GetUI()->GetControlWithTag(mGroupStart + i);
      if (i == mThisTag)
      {
        pControlBtn->SetValue(1.0);
      }
      else
      {
        pControlBtn->SetValue(0.0);
      }
      pControlBtn->SetDirty(true);
    }
  }
  return;
}
